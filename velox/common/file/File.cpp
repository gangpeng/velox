/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/common/file/File.h"
#include "velox/common/base/Fs.h"

#include <fmt/format.h>
#include <glog/logging.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

#include <folly/ScopeGuard.h>
#include <folly/portability/SysUio.h>
#ifdef linux
#include <linux/fs.h>
#endif // linux
#ifndef _WIN32
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
// Windows CRT file I/O equivalents.
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
// Map POSIX names to Windows CRT equivalents. Note: we intentionally do NOT
// define macros for `close`, `write`, and `read` because they collide with
// member function names in other Velox classes (e.g. LocalWriteFile,
// ByteStream). Those are wrapped via posix_write(), posix_close(), and
// posix_read() inline functions defined later in this file.
#define open _open
#define lseek _lseek
#define ftruncate(fd, sz) _chsize_s(fd, sz)
#define fsync _commit
// S_IRUSR / S_IWUSR are not defined in MSVC CRT; use equivalent Windows mode.
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif

#ifdef _MSC_VER
namespace {
void ignoreInvalidParameter(
    const wchar_t*,
    const wchar_t*,
    const wchar_t*,
    unsigned int,
    uintptr_t) {}

template <typename Func>
auto crtCallNoAbort(Func&& func) {
  // MSVC's CRT calls the invalid-parameter handler instead of returning errno
  // for stale file descriptors. Keep POSIX-style error reporting so Velox can
  // raise a normal exception.
  const auto oldHandler =
      _set_thread_local_invalid_parameter_handler(ignoreInvalidParameter);
  auto guard = folly::makeGuard([&]() {
    _set_thread_local_invalid_parameter_handler(oldHandler);
  });
  return func();
}

intptr_t getOsFileHandleNoAbort(int fd) {
  return crtCallNoAbort([&]() { return ::_get_osfhandle(fd); });
}

int closeNoAbort(int fd) {
  return crtCallNoAbort([&]() { return ::_close(fd); });
}
} // namespace
#endif

// pread: positional read using OVERLAPPED I/O to be thread-safe.
// Loops to handle count > DWORD_MAX (4GB).
inline ssize_t pread(int fd, void* buf, size_t count, int64_t offset) {
  HANDLE h = reinterpret_cast<HANDLE>(getOsFileHandleNoAbort(fd));
  if (h == INVALID_HANDLE_VALUE) {
    return -1;
  }
  size_t totalRead = 0;
  auto* dest = static_cast<char*>(buf);
  while (totalRead < count) {
    DWORD chunkSize =
        static_cast<DWORD>(std::min<size_t>(count - totalRead, MAXDWORD));
    LARGE_INTEGER liOffset;
    liOffset.QuadPart = offset + totalRead;
    OVERLAPPED ov = {};
    ov.Offset = liOffset.LowPart;
    ov.OffsetHigh = liOffset.HighPart;
    ov.hEvent = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (ov.hEvent == nullptr) {
      return totalRead > 0 ? static_cast<ssize_t>(totalRead) : -1;
    }
    DWORD bytesRead = 0;
    if (!ReadFile(h, dest + totalRead, chunkSize, nullptr, &ov)) {
      const auto error = ::GetLastError();
      if (error == ERROR_IO_PENDING) {
        if (!::GetOverlappedResult(h, &ov, &bytesRead, TRUE)) {
          ::CloseHandle(ov.hEvent);
          return totalRead > 0 ? static_cast<ssize_t>(totalRead) : -1;
        }
      } else if (error == ERROR_HANDLE_EOF) {
        ::CloseHandle(ov.hEvent);
        break;
      } else {
        ::CloseHandle(ov.hEvent);
        return totalRead > 0 ? static_cast<ssize_t>(totalRead) : -1;
      }
    } else if (!::GetOverlappedResult(h, &ov, &bytesRead, FALSE)) {
      ::CloseHandle(ov.hEvent);
      return totalRead > 0 ? static_cast<ssize_t>(totalRead) : -1;
    }
    ::CloseHandle(ov.hEvent);
    if (bytesRead == 0) {
      break; // EOF.
    }
    totalRead += bytesRead;
  }
  return static_cast<ssize_t>(totalRead);
}
// pwrite: positional write using OVERLAPPED I/O.
// Loops to handle count > DWORD_MAX (4GB).
inline ssize_t pwrite(int fd, const void* buf, size_t count, int64_t offset) {
  HANDLE h = reinterpret_cast<HANDLE>(getOsFileHandleNoAbort(fd));
  if (h == INVALID_HANDLE_VALUE) {
    return -1;
  }
  size_t totalWritten = 0;
  const auto* src = static_cast<const char*>(buf);
  while (totalWritten < count) {
    DWORD chunkSize =
        static_cast<DWORD>(std::min<size_t>(count - totalWritten, MAXDWORD));
    LARGE_INTEGER liOffset;
    liOffset.QuadPart = offset + totalWritten;
    OVERLAPPED ov = {};
    ov.Offset = liOffset.LowPart;
    ov.OffsetHigh = liOffset.HighPart;
    DWORD bytesWritten = 0;
    if (!WriteFile(h, src + totalWritten, chunkSize, &bytesWritten, &ov)) {
      return totalWritten > 0 ? static_cast<ssize_t>(totalWritten) : -1;
    }
    if (bytesWritten == 0) {
      break;
    }
    totalWritten += bytesWritten;
  }
  return static_cast<ssize_t>(totalWritten);
}
#endif

namespace facebook::velox {

void IoStats::addCounter(const std::string& name, RuntimeCounter counter) {
  auto locked = stats_.wlock();
  auto it = locked->find(name);
  if (it == locked->end()) {
    auto [ptr, inserted] = locked->emplace(name, RuntimeMetric(counter.unit));
    VELOX_CHECK(inserted);
    ptr->second.addValue(counter.value);
  } else {
    VELOX_CHECK_EQ(it->second.unit, counter.unit);
    it->second.addValue(counter.value);
  }
}

void IoStats::merge(const IoStats& other) {
  auto otherStats = other.stats();
  auto locked = stats_.wlock();
  for (const auto& [name, metric] : otherStats) {
    auto it = locked->find(name);
    if (it == locked->end()) {
      locked->emplace(name, metric);
    } else {
      it->second.merge(metric);
    }
  }
}

folly::F14FastMap<std::string, RuntimeMetric> IoStats::stats() const {
  return stats_.copy();
}

#define RETURN_IF_ERROR(func, result) \
  result = func;                      \
  if (result < 0) {                   \
    return result;                    \
  }

namespace {
FOLLY_ALWAYS_INLINE void checkNotClosed(bool closed) {
  VELOX_CHECK(!closed, "file is closed");
}

template <typename T>
T getAttribute(
    const std::unordered_map<std::string, std::string>& attributes,
    const std::string_view& key,
    const T& defaultValue) {
  if (attributes.count(std::string(key)) > 0) {
    try {
      return folly::to<T>(attributes.at(std::string(key)));
    } catch (const std::exception& e) {
      VELOX_FAIL("Failed while parsing File attributes: {}", e.what());
    }
  }
  return defaultValue;
}

#ifdef _WIN32
void collapseDuplicateBackslashes(std::string& path, size_t start) {
  size_t write = start;
  bool previousWasBackslash = false;
  for (size_t read = start; read < path.size(); ++read) {
    if (path[read] == '\\') {
      if (previousWasBackslash) {
        continue;
      }
      previousWasBackslash = true;
    } else {
      previousWasBackslash = false;
    }
    path[write++] = path[read];
  }
  path.resize(write);
}

std::string toWindowsExtendedPath(std::string_view path) {
  std::string result(path);
  if (result.rfind(R"(\\?\)", 0) == 0 || result.rfind(R"(\\.\)", 0) == 0) {
    return result;
  }

  std::replace(result.begin(), result.end(), '/', '\\');
  if (result.rfind(R"(\\)", 0) == 0) {
    // The \\?\ prefix disables Win32 path normalization. Collapse duplicate
    // separators and resolve lexical '..' segments from POSIX-style joins
    // while preserving the UNC introducer.
    collapseDuplicateBackslashes(result, 2);
    result = fs::path(result).lexically_normal().string();
    return R"(\\?\UNC\)" + result.substr(2);
  }
  if (result.size() >= 2 && result[1] == ':') {
    // MSVC CRT and Win32 file APIs need the extended-path prefix to open local
    // files once deeply nested partition directories exceed MAX_PATH. Because
    // extended paths are not normalized by Win32, clean up duplicate
    // separators and lexical '..' segments before adding the prefix.
    collapseDuplicateBackslashes(result, 2);
    result = fs::path(result).lexically_normal().string();
    return R"(\\?\)" + result;
  }
  return result;
}
#endif
} // namespace

std::string ReadFile::pread(
    uint64_t offset,
    uint64_t length,
    const FileIoContext& context) const {
  std::string buf;
  buf.resize(length);
  auto res = pread(offset, length, buf.data(), context);
  buf.resize(res.size());
  return buf;
}

uint64_t ReadFile::preadv(
    uint64_t offset,
    const std::vector<folly::Range<char*>>& buffers,
    const FileIoContext& context) const {
  auto fileSize = size();
  uint64_t numRead = 0;
  if (offset >= fileSize) {
    return 0;
  }
  for (auto& range : buffers) {
    auto copySize = std::min<size_t>(range.size(), fileSize - offset);
    // NOTE: skip the gap in case of coalesce io.
    if (range.data() != nullptr) {
      pread(offset, copySize, range.data(), context);
    }
    offset += copySize;
    numRead += copySize;
  }
  return numRead;
}

uint64_t ReadFile::preadv(
    folly::Range<const common::Region*> regions,
    folly::Range<folly::IOBuf*> iobufs,
    const FileIoContext& context) const {
  VELOX_CHECK_EQ(regions.size(), iobufs.size());
  uint64_t length = 0;
  for (size_t i = 0; i < regions.size(); ++i) {
    const auto& region = regions[i];
    auto& output = iobufs[i];
    output = folly::IOBuf(folly::IOBuf::CREATE, region.length);
    pread(region.offset, region.length, output.writableData(), context);
    output.append(region.length);
    length += region.length;
  }
  return length;
}

std::string_view InMemoryReadFile::pread(
    uint64_t offset,
    uint64_t length,
    void* buf,
    const FileIoContext& context) const {
  bytesRead_ += length;
  memcpy(buf, file_.data() + offset, length);
  return {static_cast<char*>(buf), length};
}

std::string InMemoryReadFile::pread(
    uint64_t offset,
    uint64_t length,
    const FileIoContext& context) const {
  bytesRead_ += length;
  return std::string(file_.data() + offset, length);
}

void InMemoryWriteFile::append(std::string_view data) {
  file_->append(data);
}

void InMemoryWriteFile::append(std::unique_ptr<folly::IOBuf> data) {
  for (auto rangeIter = data->begin(); rangeIter != data->end(); ++rangeIter) {
    file_->append(
        reinterpret_cast<const char*>(rangeIter->data()), rangeIter->size());
  }
}

uint64_t InMemoryWriteFile::size() const {
  return file_->size();
}

LocalReadFile::LocalReadFile(
    std::string_view path,
    folly::Executor* executor,
    bool bufferIo)
    : executor_(executor), path_(path) {
  int32_t flags = O_RDONLY;
#ifdef _WIN32
  flags |= O_BINARY;
#endif
#ifdef linux
  if (!bufferIo) {
    flags |= O_DIRECT;
  }
#endif // linux
#ifdef _WIN32
  // Open Windows read handles for overlapped I/O so pread() can issue
  // concurrent offset-based reads without racing on the handle's file pointer.
  const auto nativePath = toWindowsExtendedPath(path_);
  HANDLE handle = ::CreateFileA(
      nativePath.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    const auto error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      VELOX_FILE_NOT_FOUND_ERROR("No such file or directory: {}", path);
    }
    VELOX_FAIL(
        "open failure in LocalReadFile constructor, {} {}.", path, error);
  }
  LARGE_INTEGER fileSize;
  if (!::GetFileSizeEx(handle, &fileSize)) {
    const auto error = ::GetLastError();
    ::CloseHandle(handle);
    VELOX_FAIL(
        "fseek failure in LocalReadFile constructor, {} {}.", path, error);
  }
  fd_ = _open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
  if (fd_ < 0) {
    ::CloseHandle(handle);
    VELOX_FAIL(
        "open failure in LocalReadFile constructor, {} {}.",
        path,
        folly::errnoStr(errno));
  }
  size_ = fileSize.QuadPart;
#else
  fd_ = open(path_.c_str(), flags);
  if (fd_ < 0) {
    if (errno == ENOENT) {
      VELOX_FILE_NOT_FOUND_ERROR("No such file or directory: {}", path);
    } else {
      VELOX_FAIL(
          "open failure in LocalReadFile constructor, {} {} {}.",
          fd_,
          path,
          folly::errnoStr(errno));
    }
  }
  const off_t ret = lseek(fd_, 0, SEEK_END);
  VELOX_CHECK_GE(
      ret,
      0,
      "fseek failure in LocalReadFile constructor, {} {} {}.",
      ret,
      path,
      folly::errnoStr(errno));
  size_ = ret;
#endif
}

LocalReadFile::LocalReadFile(int32_t fd, folly::Executor* executor)
    : executor_(executor), fd_(fd) {
#ifdef _WIN32
  // This constructor is used by tests to wrap both valid and intentionally
  // stale descriptors. Leave size_ as zero if the descriptor is already closed
  // and let the read path report the descriptor error.
  struct __stat64 fileStat;
  if (crtCallNoAbort([&]() { return ::_fstat64(fd_, &fileStat); }) == 0) {
    size_ = static_cast<uint64_t>(fileStat.st_size);
  }
#else
  struct stat fileStat;
  if (::fstat(fd_, &fileStat) == 0) {
    size_ = static_cast<uint64_t>(fileStat.st_size);
  }
#endif
}

LocalReadFile::~LocalReadFile() {
  const int ret =
#ifdef _WIN32
      closeNoAbort(fd_);
#else
      close(fd_);
#endif
  if (ret < 0) {
    LOG(WARNING) << "close failure in LocalReadFile destructor: " << ret << ", "
                 << folly::errnoStr(errno);
  }
}

void LocalReadFile::preadInternal(uint64_t offset, uint64_t length, char* pos)
    const {
  bytesRead_ += length;
  auto bytesRead = ::pread(fd_, pos, length, offset);
  VELOX_CHECK_EQ(
      bytesRead,
      length,
      "fread failure in LocalReadFile::PReadInternal, {} vs {}: {}",
      bytesRead,
      length,
      folly::errnoStr(errno));
}

std::string_view LocalReadFile::pread(
    uint64_t offset,
    uint64_t length,
    void* buf,
    const FileIoContext& context) const {
  preadInternal(offset, length, static_cast<char*>(buf));
  return {static_cast<char*>(buf), length};
}

uint64_t LocalReadFile::preadv(
    uint64_t offset,
    const std::vector<folly::Range<char*>>& buffers,
    const FileIoContext& context) const {
#ifdef _WIN32
  // folly::preadv uses CRT positional I/O on Windows and is not compatible
  // with the overlapped handles used above. Issue discrete overlapped reads
  // and skip gaps here. Do not delegate to ReadFile::preadv(), because it
  // bounds reads by size(); SSD cache files can grow after the read handle is
  // opened, making the cached size stale.
  uint64_t totalBytesRead = 0;
  for (const auto& range : buffers) {
    const auto bytes = range.size();
    if (range.data() != nullptr && bytes > 0) {
      preadInternal(offset, bytes, range.data());
    }
    offset += bytes;
    totalBytesRead += bytes;
  }
  return totalBytesRead;
#else
  // Dropped bytes sized so that a typical dropped range of 50K is not
  // too many iovecs.
  static thread_local std::vector<char> droppedBytes(16 * 1024);
  uint64_t totalBytesRead = 0;
  std::vector<struct iovec> iovecs;
  iovecs.reserve(buffers.size());

  auto readvFunc = [&]() -> ssize_t {
    const auto bytesRead =
        folly::preadv(fd_, iovecs.data(), iovecs.size(), offset);
    if (bytesRead < 0) {
      LOG(ERROR) << "preadv failed with error: " << folly::errnoStr(errno);
    } else {
      totalBytesRead += bytesRead;
      offset += bytesRead;
    }
    iovecs.clear();
    return bytesRead;
  };

  for (auto& range : buffers) {
    if (!range.data()) {
      auto skipSize = range.size();
      while (skipSize) {
        auto bytes = std::min<size_t>(droppedBytes.size(), skipSize);

        if (iovecs.size() >= IOV_MAX) {
          ssize_t bytesRead{0};
          RETURN_IF_ERROR(readvFunc(), bytesRead);
        }

        iovecs.push_back({droppedBytes.data(), bytes});
        skipSize -= bytes;
      }
    } else {
      if (iovecs.size() >= IOV_MAX) {
        ssize_t bytesRead{0};
        RETURN_IF_ERROR(readvFunc(), bytesRead);
      }

      iovecs.push_back({range.data(), range.size()});
    }
  }

  // Perform any remaining preadv calls
  if (!iovecs.empty()) {
    ssize_t bytesRead{0};
    RETURN_IF_ERROR(readvFunc(), bytesRead);
  }

  return totalBytesRead;
#endif
}

folly::SemiFuture<uint64_t> LocalReadFile::preadvAsync(
    uint64_t offset,
    const std::vector<folly::Range<char*>>& buffers,
    const FileIoContext& context) const {
  if (!executor_) {
    return ReadFile::preadvAsync(offset, buffers, context);
  }
  auto [promise, future] = folly::makePromiseContract<uint64_t>();
  executor_->add([this,
                  _promise = std::move(promise),
                  _offset = offset,
                  _buffers = buffers,
                  _context = context]() mutable {
    auto delegateFuture = ReadFile::preadvAsync(_offset, _buffers, _context);
    _promise.setTry(std::move(delegateFuture).getTry());
  });
  return std::move(future);
}

uint64_t LocalReadFile::size() const {
  return size_;
}

uint64_t LocalReadFile::memoryUsage() const {
  // TODO: does FILE really not use any more memory? From the stdio.h
  // source code it looks like it has only a single integer? Probably
  // we need to go deeper and see how much system memory is being taken
  // by the file descriptor the integer refers to?
  return sizeof(FILE);
}

bool LocalWriteFile::Attributes::cowDisabled(
    const std::unordered_map<std::string, std::string>& attrs) {
  return getAttribute<bool>(attrs, kNoCow, kDefaultNoCow);
}

LocalWriteFile::LocalWriteFile(
    std::string_view path,
    bool shouldCreateParentDirectories,
    bool shouldThrowOnFileAlreadyExists,
    bool bufferIo)
    : path_(path) {
  const auto dir = fs::path(path_).parent_path();
  if (shouldCreateParentDirectories && !fs::exists(dir)) {
    VELOX_CHECK(
        common::generateFileDirectory(dir.string().c_str()),
        "Failed to generate file directory");
  }

  // File open flags: write-only, create the file if it doesn't exist.
  int32_t flags = O_WRONLY | O_CREAT;
#ifdef _WIN32
  flags |= O_BINARY;
#endif
  if (shouldThrowOnFileAlreadyExists) {
    flags |= O_EXCL;
  }
#ifdef linux
  if (!bufferIo) {
    flags |= O_DIRECT;
  }
#endif // linux

  // The file mode bits to be applied when a new file is created. By default
  // user has read and write access to the file.
  // NOTE: The mode argument must be supplied if O_CREAT or O_TMPFILE is
  // specified in flags; if it is not supplied, some arbitrary bytes from the
  // stack will be applied as the file mode.
  const int32_t mode = S_IRUSR | S_IWUSR;

  const auto nativePath =
#ifdef _WIN32
      toWindowsExtendedPath(path_);
#else
      std::string(path_);
#endif
  std::unique_ptr<char[]> buf(new char[nativePath.size() + 1]);
  buf[nativePath.size()] = 0;
  ::memcpy(buf.get(), nativePath.data(), nativePath.size());
  fd_ = open(buf.get(), flags, mode);
  VELOX_CHECK_GE(
      fd_,
      0,
      "Cannot open or create {}. Error: {}",
      path_,
      folly::errnoStr(errno));

  const off_t ret = lseek(fd_, 0, SEEK_END);
  VELOX_CHECK_GE(
      ret,
      0,
      "fseek failure in LocalWriteFile constructor, {} {} {}.",
      ret,
      path_,
      folly::errnoStr(errno));
  size_ = ret;
}

LocalWriteFile::~LocalWriteFile() {
  try {
    close();
  } catch (const std::exception& ex) {
    // We cannot throw an exception from the destructor. Warn instead.
    LOG(WARNING) << "fclose failure in LocalWriteFile destructor: "
                 << ex.what();
  }
}

#ifdef _MSC_VER
namespace {
inline int posix_write(int fd, const void* buf, unsigned int count) {
  return crtCallNoAbort([&]() { return ::_write(fd, buf, count); });
}
inline int posix_close(int fd) {
  return closeNoAbort(fd);
}
} // namespace
#else
namespace {
inline int posix_write(int fd, const void* buf, size_t count) {
  return ::write(fd, buf, count);
}
inline int posix_close(int fd) {
  return ::close(fd);
}
} // namespace
#endif

void LocalWriteFile::append(std::string_view data) {
  checkNotClosed(closed_);
  const uint64_t bytesWritten = posix_write(fd_, data.data(), data.size());
  VELOX_CHECK_EQ(
      bytesWritten,
      data.size(),
      "fwrite failure in LocalWriteFile::append, {} vs {}: {}",
      bytesWritten,
      data.size(),
      folly::errnoStr(errno));
  size_ += bytesWritten;
}

void LocalWriteFile::append(std::unique_ptr<folly::IOBuf> data) {
  checkNotClosed(closed_);
  uint64_t totalBytesWritten{0};
  for (auto rangeIter = data->begin(); rangeIter != data->end(); ++rangeIter) {
    const auto bytesToWrite = rangeIter->size();
    const uint64_t bytesWritten =
        posix_write(fd_, rangeIter->data(), rangeIter->size());
    totalBytesWritten += bytesWritten;
    if (bytesWritten != bytesToWrite) {
      VELOX_FAIL(
          "fwrite failure in LocalWriteFile::append, {} vs {}: {}",
          bytesWritten,
          bytesToWrite,
          folly::errnoStr(errno));
    }
  }
  const auto totalBytesToWrite = data->computeChainDataLength();
  VELOX_CHECK_EQ(
      totalBytesWritten,
      totalBytesToWrite,
      "Failure in LocalWriteFile::append, {} vs {}",
      totalBytesWritten,
      totalBytesToWrite);
  size_ += totalBytesWritten;
}

void LocalWriteFile::write(
    const std::vector<iovec>& iovecs,
    int64_t offset,
    int64_t length) {
  checkNotClosed(closed_);
  VELOX_CHECK_GE(offset, 0, "Offset cannot be negative.");
#ifdef _WIN32
  // folly::pwritev is implemented on top of CRT file APIs on Windows. Use the
  // OVERLAPPED pwrite shim above so multi-range SSD cache writes remain
  // positional and do not depend on shared file-pointer state.
  int64_t bytesWritten{0};
  for (const auto& iovec : iovecs) {
    auto* data = static_cast<const char*>(iovec.iov_base);
    size_t bytesLeft = iovec.iov_len;
    while (bytesLeft > 0) {
      const auto written = ::pwrite(fd_, data, bytesLeft, offset + bytesWritten);
      VELOX_CHECK_GT(
          written,
          0,
          "Failure in LocalWriteFile::write at offset {}: {}",
          offset + bytesWritten,
          folly::errnoStr(errno));
      bytesWritten += written;
      data += written;
      bytesLeft -= written;
    }
  }
#else
  const auto bytesWritten = folly::pwritev(
      fd_, iovecs.data(), static_cast<ssize_t>(iovecs.size()), offset);
#endif
  VELOX_CHECK_EQ(
      bytesWritten,
      length,
      "Failure in LocalWriteFile::write, {} vs {}",
      bytesWritten,
      length);
  size_ = std::max<uint64_t>(size_, offset + bytesWritten);
}

void LocalWriteFile::truncate(int64_t newSize) {
  checkNotClosed(closed_);
  VELOX_CHECK_GE(newSize, 0, "New size cannot be negative.");
  const auto ret = ::ftruncate(fd_, newSize);
  VELOX_CHECK_EQ(
      ret,
      0,
      "ftruncate failed in LocalWriteFile::truncate: {}.",
      folly::errnoStr(errno));
  // Reposition the file offset to the end of the file for append().
  ::lseek(fd_, newSize, SEEK_SET);
  size_ = newSize;
}

void LocalWriteFile::flush() {
  checkNotClosed(closed_);
  const auto ret = ::fsync(fd_);
  VELOX_CHECK_EQ(
      ret,
      0,
      "fsync failed in LocalWriteFile::flush: {}.",
      folly::errnoStr(errno));
}

void LocalWriteFile::setAttributes(
    const std::unordered_map<std::string, std::string>& attributes) {
  checkNotClosed(closed_);
  attributes_ = attributes;
#ifdef linux
  if (Attributes::cowDisabled(attributes_)) {
    int attr{0};
    auto ret = ioctl(fd_, FS_IOC_GETFLAGS, &attr);
    VELOX_CHECK_EQ(
        0,
        ret,
        "ioctl(FS_IOC_GETFLAGS) failed: {}, {}",
        ret,
        folly::errnoStr(errno));
    attr |= FS_NOCOW_FL;
    ret = ioctl(fd_, FS_IOC_SETFLAGS, &attr);
    VELOX_CHECK_EQ(
        0,
        ret,
        "ioctl(FS_IOC_SETFLAGS, FS_NOCOW_FL) failed: {}, {}",
        ret,
        folly::errnoStr(errno));
  }
#endif // linux
}

std::unordered_map<std::string, std::string> LocalWriteFile::getAttributes()
    const {
  checkNotClosed(closed_);
  return attributes_;
}

void LocalWriteFile::close() {
  if (!closed_) {
    const auto ret = posix_close(fd_);
    VELOX_CHECK_EQ(
        ret,
        0,
        "close failed in LocalWriteFile::close: {}.",
        folly::errnoStr(errno));
    closed_ = true;
  }
}

} // namespace facebook::velox
