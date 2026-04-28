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

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace facebook::velox::memory {

enum class SystemMmapMode {
  kCommit,
  kReserve,
};

// Cross-platform virtual-memory operations used by the memory allocators.
// POSIX builds map these to mmap/munmap/madvise. Windows maps them to
// VirtualAlloc/VirtualFree/MEM_RESET. Committed mappings are readable and
// writable immediately; reserved mappings must be committed with
// systemMmapCommit before use.
void* systemMmap(
    size_t bytes,
    SystemMmapMode mode = SystemMmapMode::kCommit);
bool systemMmapCommit(void* address, size_t bytes);
bool systemMmapDecommit(void* address, size_t bytes);
bool systemMunmap(void* address, size_t bytes);
bool systemMadviseDontNeed(void* address, size_t bytes);
bool systemMadviseHugePage(void* address, size_t bytes, bool enable);

// Allocations from systemAlignedAlloc must be released with
// systemAlignedFree. The implementation handles the CRT-specific pairing on
// Windows and uses posix_memalign/free on POSIX.
void* systemAlignedAlloc(size_t alignment, size_t size);
void systemAlignedFree(void* address);

struct SystemMemoryUsage {
  uint64_t virtualBytes{0};
  uint64_t residentBytes{0};
};

std::optional<SystemMemoryUsage> systemProcessMemoryUsage();

std::string systemMemoryError();

} // namespace facebook::velox::memory
