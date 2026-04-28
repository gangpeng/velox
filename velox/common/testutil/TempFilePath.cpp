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

#include "velox/common/testutil/TempFilePath.h"

#ifdef _WIN32
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>

#include <fmt/format.h>

namespace facebook::velox::common::testutil {

#ifdef _WIN32
namespace {
std::atomic<uint64_t> tempFileCounter{0};
}
#endif

TempFilePath::~TempFilePath() {
#ifdef _WIN32
  // fd_ may already be closed (we close it early in createTempFile on Windows
  // to avoid blocking file removal). Skip the close in that case.
  ::DeleteFileA(tempPath_.c_str());
#else
  ::unlink(tempPath_.c_str());
  ::close(fd_);
#endif
}

std::shared_ptr<TempFilePath> TempFilePath::create(bool enableFaultInjection) {
  auto* tempFilePath = new TempFilePath(enableFaultInjection);
  return std::shared_ptr<TempFilePath>(tempFilePath);
}

std::string TempFilePath::createTempFile(TempFilePath* tempFilePath) {
#ifdef _WIN32
  char tmpDir[MAX_PATH];
  DWORD ret = ::GetTempPathA(MAX_PATH, tmpDir);
  if (ret == 0 || ret > MAX_PATH) {
    VELOX_FAIL("Cannot get temp directory");
  }

  std::string path;
  constexpr int kMaxAttempts = 100;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const auto id = tempFileCounter.fetch_add(1, std::memory_order_relaxed);
    path = fmt::format(
        "{}velox_test_{}_{}_{}_{}.tmp",
        tmpDir,
        ::GetCurrentProcessId(),
        ::GetCurrentThreadId(),
        ::GetTickCount64(),
        id);

    // Use O_EXCL to keep creation atomic when tests create temp files in
    // parallel. GetTempFileNameA can exhaust its per-prefix namespace in a
    // busy Windows temp directory and fail with ERROR_FILE_EXISTS.
    tempFilePath->fd_ = ::_open(
        path.c_str(),
        _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY,
        _S_IREAD | _S_IWRITE);
    if (tempFilePath->fd_ != -1) {
      break;
    }
    if (errno != EEXIST) {
      VELOX_FAIL("Cannot open temp file: {}", folly::errnoStr(errno));
    }
  }

  if (tempFilePath->fd_ == -1) {
    VELOX_FAIL(
        "Cannot create unique temp file after {} attempts", kMaxAttempts);
  }
  // Close the fd immediately on Windows so that subsequent remove/open
  // operations on this path are not blocked by an open handle.
  ::_close(tempFilePath->fd_);
  std::string result = std::move(path);
  std::replace(result.begin(), result.end(), '\\', '/');
  return result;
#else
  char path[] = "/tmp/velox_test_XXXXXX";
  tempFilePath->fd_ = ::mkstemp(path);
  if (tempFilePath->fd_ == -1) {
    VELOX_FAIL("Cannot open temp file: {}", folly::errnoStr(errno));
  }
  return path;
#endif
}

std::vector<std::string> toFilePaths(
    const std::vector<std::shared_ptr<TempFilePath>>& tempFiles) {
  std::vector<std::string> filePaths;
  filePaths.reserve(tempFiles.size());
  for (const auto& tempFile : tempFiles) {
    filePaths.push_back(tempFile->getPath());
  }
  return filePaths;
}
} // namespace facebook::velox::common::testutil
