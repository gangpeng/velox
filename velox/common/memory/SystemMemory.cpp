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

#include "velox/common/memory/SystemMemory.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>

#include <fmt/format.h>

#ifdef _WIN32
#include <malloc.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace facebook::velox::memory {

void* systemMmap(size_t bytes, SystemMmapMode mode) {
#ifdef _WIN32
  const auto allocationType = mode == SystemMmapMode::kCommit
      ? MEM_COMMIT | MEM_RESERVE
      : MEM_RESERVE;
  return ::VirtualAlloc(nullptr, bytes, allocationType, PAGE_READWRITE);
#else
  (void)mode;
  void* result = ::mmap(
      nullptr,
      bytes,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS,
      -1,
      0);
  return result == MAP_FAILED ? nullptr : result;
#endif
}

bool systemMmapCommit(void* address, size_t bytes) {
#ifdef _WIN32
  return ::VirtualAlloc(address, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
  (void)address;
  (void)bytes;
  return true;
#endif
}

bool systemMmapDecommit(void* address, size_t bytes) {
#ifdef _WIN32
  return ::VirtualFree(address, bytes, MEM_DECOMMIT) != 0;
#else
  return ::madvise(address, bytes, MADV_DONTNEED) == 0;
#endif
}

bool systemMunmap(void* address, size_t bytes) {
#ifdef _WIN32
  return ::VirtualFree(address, 0, MEM_RELEASE) != 0;
#else
  return ::munmap(address, bytes) == 0;
#endif
}

bool systemMadviseDontNeed(void* address, size_t bytes) {
#ifdef _WIN32
  // MEM_RESET tells Windows the contents are no longer needed without
  // decommitting the range. That preserves MmapAllocator's invariant that the
  // virtual address remains writable when the logical page is allocated again.
  return ::VirtualAlloc(address, bytes, MEM_RESET, PAGE_READWRITE) != nullptr;
#else
  return ::madvise(address, bytes, MADV_DONTNEED) == 0;
#endif
}

bool systemMadviseHugePage(void* address, size_t bytes, bool enable) {
#ifdef linux
  return ::madvise(address, bytes, enable ? MADV_HUGEPAGE : MADV_NOHUGEPAGE) ==
      0;
#else
  return true;
#endif
}

void* systemAlignedAlloc(size_t alignment, size_t size) {
#ifdef _WIN32
  alignment = std::max(alignment, sizeof(void*));
  return ::_aligned_malloc(size, alignment);
#else
  void* result = nullptr;
  if (::posix_memalign(&result, alignment, size) != 0) {
    return nullptr;
  }
  return result;
#endif
}

void systemAlignedFree(void* address) {
#ifdef _WIN32
  ::_aligned_free(address);
#else
  ::free(address);
#endif
}

std::optional<SystemMemoryUsage> systemProcessMemoryUsage() {
#ifdef _WIN32
  SYSTEM_INFO systemInfo;
  ::GetNativeSystemInfo(&systemInfo);

  auto address = reinterpret_cast<uintptr_t>(
      systemInfo.lpMinimumApplicationAddress);
  const auto maxAddress =
      reinterpret_cast<uintptr_t>(systemInfo.lpMaximumApplicationAddress);
  SystemMemoryUsage usage;
  while (address < maxAddress) {
    MEMORY_BASIC_INFORMATION info;
    if (::VirtualQuery(
            reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0) {
      break;
    }

    if (info.State == MEM_COMMIT || info.State == MEM_RESERVE) {
      usage.virtualBytes += info.RegionSize;
    }
    // Windows doesn't expose an mmap-style process RSS counter from
    // VirtualQuery. Committed pages are the allocator-relevant counterpart:
    // reserved-only pages increase virtual size but not this value.
    if (info.State == MEM_COMMIT) {
      usage.residentBytes += info.RegionSize;
    }

    const auto next =
        reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (next <= address) {
      break;
    }
    address = next;
  }
  return usage;
#elif defined(__linux__) || defined(linux)
  std::ifstream statm("/proc/self/statm");
  uint64_t virtualPages;
  uint64_t residentPages;
  if (!(statm >> virtualPages >> residentPages)) {
    return std::nullopt;
  }
  const auto pageSizeValue = ::sysconf(_SC_PAGESIZE);
  if (pageSizeValue <= 0) {
    return std::nullopt;
  }
  const auto pageSize = static_cast<uint64_t>(pageSizeValue);
  return SystemMemoryUsage{
      virtualPages * pageSize,
      residentPages * pageSize,
  };
#else
  return std::nullopt;
#endif
}

std::string systemMemoryError() {
#ifdef _WIN32
  const auto error = ::GetLastError();
  if (error == ERROR_SUCCESS) {
    return "GetLastError=0";
  }
  return fmt::format("GetLastError={}", error);
#else
  return ::strerror(errno);
#endif
}

} // namespace facebook::velox::memory
