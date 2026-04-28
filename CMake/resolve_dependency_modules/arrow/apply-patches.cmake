# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script applies Arrow patches idempotently via a PowerShell helper.
# It is invoked as: cmake -P apply-patches.cmake
# The working directory must be the Arrow source root.

get_filename_component(PATCH_DIR "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
cmake_path(NATIVE_PATH PATCH_DIR NORMALIZE PATCH_DIR_NATIVE)

# The working directory when running cmake -P is set by ExternalProject to the source dir.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT SOURCE_DIR OR SOURCE_DIR STREQUAL "")
  # Fallback: use current binary dir as a hint
  set(SOURCE_DIR ".")
endif()

execute_process(
  COMMAND powershell.exe -ExecutionPolicy Bypass -NonInteractive
          -File "${PATCH_DIR}/apply-patches.ps1"
          -SourceDir "."
          -PatchDir "${PATCH_DIR_NATIVE}"
  RESULT_VARIABLE RESULT
  ERROR_VARIABLE ERR_OUT
)

if(NOT RESULT EQUAL 0)
  message(FATAL_ERROR "Patch application failed: ${ERR_OUT}")
endif()
