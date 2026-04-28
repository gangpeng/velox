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

#include "velox/common/testutil/TempDirectoryPath.h"

#include "boost/filesystem.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#endif

#include <algorithm>
#include <atomic>

#include <fmt/format.h>

namespace facebook::velox::common::testutil {

#ifdef _WIN32
namespace {
std::atomic<uint64_t> tempDirectoryCounter{0};
}
#endif

std::shared_ptr<TempDirectoryPath> TempDirectoryPath::create(bool injectFault) {
  auto* tempDirPath = new TempDirectoryPath(injectFault);
  return std::shared_ptr<TempDirectoryPath>(tempDirPath);
}

TempDirectoryPath::~TempDirectoryPath() {
  LOG(INFO) << "TempDirectoryPath:: removing all files from " << tempPath_;
  try {
    boost::filesystem::remove_all(tempPath_.c_str());
  } catch (...) {
    LOG(WARNING)
        << "TempDirectoryPath:: destructor failed while calling boost::filesystem::remove_all";
  }
}

std::string TempDirectoryPath::createTempDirectory() {
#ifdef _WIN32
  char tmpDir[MAX_PATH];
  DWORD ret = ::GetTempPathA(MAX_PATH, tmpDir);
  if (ret == 0 || ret > MAX_PATH) {
    VELOX_FAIL("Cannot get temp directory");
  }

  std::string path;
  constexpr int kMaxAttempts = 100;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const auto id =
        tempDirectoryCounter.fetch_add(1, std::memory_order_relaxed);
    path = fmt::format(
        "{}velox_test_dir_{}_{}_{}_{}",
        tmpDir,
        ::GetCurrentProcessId(),
        ::GetCurrentThreadId(),
        ::GetTickCount64(),
        id);

    // Create the directory directly instead of using GetTempFileNameA followed
    // by DeleteFile/CreateDirectory. The file-to-directory conversion has a
    // race window under parallel Windows tests and can fail with
    // ERROR_ALREADY_EXISTS.
    if (::CreateDirectoryA(path.c_str(), nullptr)) {
      std::replace(path.begin(), path.end(), '\\', '/');
      return path;
    }
    if (::GetLastError() != ERROR_ALREADY_EXISTS) {
      VELOX_FAIL("Cannot create temp directory: {}", ::GetLastError());
    }
  }

  VELOX_FAIL(
      "Cannot create unique temp directory after {} attempts", kMaxAttempts);
#else
  char tempPath[] = "/tmp/velox_test_XXXXXX";
  const char* tempDirectoryPath = ::mkdtemp(tempPath);
  VELOX_CHECK_NOT_NULL(tempDirectoryPath, "Cannot open temp directory");
  return tempDirectoryPath;
#endif
}

} // namespace facebook::velox::common::testutil
