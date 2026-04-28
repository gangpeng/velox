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

#include "velox/common/base/Fs.h"

#include <fmt/format.h>
#include <glog/logging.h>

#ifdef _MSC_VER
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <cerrno>

// Minimal Windows implementations of the POSIX mkstemp/mkdtemp helpers.
// mkstemp: replaces the trailing "XXXXXX" in the template with a unique
// suffix, creates the file exclusively, and returns an open file descriptor.
// Uses _O_EXCL to avoid TOCTOU races between name generation and creation.
static int mkstemp(char* tmpl) {
  if (!tmpl) {
    errno = EINVAL;
    return -1;
  }
  const size_t len = strlen(tmpl) + 1;
  // _mktemp_s generates at most 26 names per template. On collision with
  // _O_EXCL, we restore the template suffix and retry.
  std::string original(tmpl);
  for (int attempt = 0; attempt < 26; ++attempt) {
    // Restore the XXXXXX suffix for each attempt since _mktemp_s modifies
    // the template in place and won't retry on its own.
    if (attempt > 0) {
      memcpy(tmpl, original.c_str(), len);
    }
    if (_mktemp_s(tmpl, len) != 0) {
      errno = EEXIST;
      return -1;
    }
    int fd = -1;
    errno_t err = _sopen_s(
        &fd,
        tmpl,
        _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
        _SH_DENYRW,
        _S_IREAD | _S_IWRITE);
    if (err == 0) {
      return fd;
    }
    if (err != EEXIST) {
      return -1;
    }
  }
  errno = EEXIST;
  return -1;
}

// mkdtemp: replaces the trailing "XXXXXX" with a unique suffix, creates the
// directory, and returns a pointer to the modified template or nullptr on error.
// Uses a retry loop since _mktemp_s and _mkdir are not atomic.
static char* mkdtemp(char* tmpl) {
  if (!tmpl) {
    errno = EINVAL;
    return nullptr;
  }
  const size_t len = strlen(tmpl) + 1;
  std::string original(tmpl);
  for (int attempt = 0; attempt < 26; ++attempt) {
    if (attempt > 0) {
      memcpy(tmpl, original.c_str(), len);
    }
    if (_mktemp_s(tmpl, len) != 0) {
      errno = EEXIST;
      return nullptr;
    }
    if (_mkdir(tmpl) == 0) {
      return tmpl;
    }
    if (errno != EEXIST) {
      return nullptr;
    }
  }
  errno = EEXIST;
  return nullptr;
}
#else
#include <unistd.h>
#endif // _MSC_VER

namespace facebook::velox::common {

bool generateFileDirectory(const char* dirPath) {
  std::error_code errorCode;
  const auto success = fs::create_directories(dirPath, errorCode);
  fs::permissions(dirPath, fs::perms::all, fs::perm_options::replace);
  if (!success && errorCode.value() != 0) {
    LOG(ERROR) << "Failed to create file directory '" << dirPath
               << "'. Error: " << errorCode.message() << " errno "
               << errorCode.value();
    return false;
  }
  return true;
}

std::optional<std::string> generateTempFilePath(
    const char* basePath,
    const char* prefix) {
  auto path = fmt::format("{}/velox_{}_XXXXXX", basePath, prefix);
  auto fd = mkstemp(path.data());
  if (fd == -1) {
    return std::nullopt;
  }
  // This API returns only a path. Close the mkstemp descriptor immediately so
  // callers can reopen the file; on Windows the descriptor uses exclusive
  // sharing and otherwise blocks subsequent std::fstream opens.
#ifdef _MSC_VER
  _close(fd);
#else
  ::close(fd);
#endif
  return path;
}

std::optional<std::string> generateTempFolderPath(
    const char* basePath,
    const char* prefix) {
  auto path = fmt::format("{}/velox_{}_XXXXXX", basePath, prefix);
  auto createdPath = mkdtemp(path.data());
  if (createdPath == nullptr) {
    return std::nullopt;
  }
  return path;
}

} // namespace facebook::velox::common
