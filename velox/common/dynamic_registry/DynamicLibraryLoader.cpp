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

#ifdef _WIN32
#include <windows.h>
namespace {
inline void* velox_dlopen(const char* path) {
  return static_cast<void*>(LoadLibraryA(path));
}
inline void* velox_dlsym(void* handle, const char* symbol) {
  return reinterpret_cast<void*>(
      GetProcAddress(static_cast<HMODULE>(handle), symbol));
}
inline const char* velox_dlerror() {
  static thread_local char buf[256];
  DWORD err = GetLastError();
  if (err == 0) {
    return nullptr;
  }
  FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      err,
      0,
      buf,
      sizeof(buf),
      nullptr);
  return buf;
}
} // namespace
#define dlopen(path, flags) velox_dlopen(path)
#define dlsym(handle, sym) velox_dlsym(handle, sym)
#define dlerror() velox_dlerror()
#define RTLD_NOW 0
#else
#include <dlfcn.h>
#endif
#include <iostream>
#include "velox/common/base/Exceptions.h"

namespace facebook::velox {

void loadDynamicLibrary(
    const std::string& fileName,
    const std::string& registrationFunctionName) {
  // Try to dynamically load the shared library.
  void* handler = dlopen(fileName.c_str(), RTLD_NOW);

  if (handler == nullptr) {
    VELOX_USER_FAIL("Error while loading shared library: {}", dlerror());
  }

  LOG(INFO) << "Loaded library " << fileName << ". Searching registry symbol "
            << registrationFunctionName;

  // Lookup the symbol.
  void* registrySymbol = dlsym(handler, registrationFunctionName.c_str());
  auto loadUserLibrary = reinterpret_cast<void (*)()>(registrySymbol);
  const char* error = dlerror();

  // Check for an error first as a null symbol pointer is not necessarily an
  // error.
  if (error != nullptr) {
    VELOX_USER_FAIL("Couldn't find Velox registry symbol: {}", error);
  }

  if (loadUserLibrary == nullptr) {
    VELOX_USER_FAIL(
        "Symbol '{}' resolved to a nullptr, unable to invoke it.",
        registrationFunctionName);
  }

  // Invoke the registry function.
  loadUserLibrary();
  LOG(INFO) << "Registered functions by " << registrationFunctionName;
}

} // namespace facebook::velox
