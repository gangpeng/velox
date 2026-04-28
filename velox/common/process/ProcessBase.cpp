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

#include "velox/common/process/ProcessBase.h"

#include <limits.h>
#include <stdlib.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <folly/CpuId.h>
#include <folly/FileUtil.h>
#include <folly/String.h>
#include <gflags/gflags.h>

#ifndef _WIN32
constexpr const char* kProcSelfCmdline = "/proc/self/cmdline";
#endif

DECLARE_bool(avx2); // Enables use of AVX2 when available NOLINT

DECLARE_bool(bmi2); // Enables use of BMI2 when available NOLINT

namespace facebook {
namespace velox {
namespace process {

/**
 * Current executable's name.
 */
std::string getAppName() {
  const char* result = getenv("_");
  if (result) {
    return result;
  }

#ifndef _WIN32
  // if we're running under gtest, getenv will return null
  std::string appName;
  if (folly::readFile(kProcSelfCmdline, appName)) {
    auto pos = appName.find('\0');
    if (pos != std::string::npos) {
      appName = appName.substr(0, pos);
    }

    return appName;
  }
#endif

  return "";
}

/**
 * This machine's name.
 */
std::string getHostName() {
#ifdef _WIN32
  char hostbuf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = sizeof(hostbuf);
  if (GetComputerNameA(hostbuf, &size)) {
    return hostbuf;
  }
  return "";
#else
  char hostbuf[_POSIX_HOST_NAME_MAX + 1] = {0};
  if (gethostname(hostbuf, _POSIX_HOST_NAME_MAX + 1) < 0) {
    return "";
  } else {
    // When the host name is precisely HOST_NAME_MAX bytes long, gethostname
    // returns 0 even though the result is not NUL-terminated. Manually NUL-
    // terminate to handle that case.
    hostbuf[_POSIX_HOST_NAME_MAX] = '\0';
    return hostbuf;
  }
#endif
}

/**
 * Process identifier.
 */
pid_t getProcessId() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

/**
 * Current thread's identifier.
 */
pthread_t getThreadId() {
#ifdef _WIN32
  return pthread_self();
#else
  return pthread_self();
#endif
}

/**
 * Get current working directory.
 */
std::string getCurrentDirectory() {
#ifdef _WIN32
  char buf[MAX_PATH];
  DWORD len = GetCurrentDirectoryA(MAX_PATH, buf);
  return len > 0 ? std::string(buf, len) : "";
#else
  char buf[PATH_MAX];
  return getcwd(buf, PATH_MAX);
#endif
}

uint64_t threadCpuNanos() {
#ifdef _WIN32
  FILETIME creation, exit, kernel, user;
  if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user)) {
    ULARGE_INTEGER k;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    ULARGE_INTEGER u;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    // Match CLOCK_THREAD_CPUTIME_ID semantics by reporting user + system CPU
    // time. FILETIME is in 100-nanosecond intervals.
    return (k.QuadPart + u.QuadPart) * 100;
  }
  return 0;
#else
  timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ts.tv_sec * 1'000'000'000 + ts.tv_nsec;
#endif
}

namespace {
bool bmi2CpuFlag = folly::CpuId().bmi2();
bool avx2CpuFlag = folly::CpuId().avx2();
} // namespace

bool hasAvx2() {
#ifdef __AVX2__
  return avx2CpuFlag && FLAGS_avx2;
#else
  return false;
#endif
}

bool hasBmi2() {
#ifdef __BMI2__
  return bmi2CpuFlag && FLAGS_bmi2;
#else
  return false;
#endif
}

} // namespace process
} // namespace velox
} // namespace facebook
