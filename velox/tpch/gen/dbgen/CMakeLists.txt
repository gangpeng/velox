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
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0063 NEW)

project(dbgen CXX C)

add_library(
  dbgen
  STATIC
  dbgen.cpp
  dbgen_gunk.cpp
  bm_utils.cpp
  build.cpp
  permute.cpp
  rnd.cpp
  rng64.cpp
  speed_seed.cpp
  text.cpp
)

target_compile_definitions(dbgen PRIVATE DBNAME=dss ORACLE TPCH)
if(NOT MSVC)
  target_compile_definitions(dbgen PRIVATE MAC)
endif()
# On Windows, define GLOG_NO_ABBREVIATED_SEVERITIES to avoid conflict between
# Windows ERROR macro and glog's severity levels.
if(MSVC)
  target_compile_definitions(dbgen PRIVATE GLOG_NO_ABBREVIATED_SEVERITIES WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
target_include_directories(dbgen PRIVATE include)
target_link_libraries(dbgen PRIVATE fmt::fmt Folly::folly)

# Suppress warnings when compiling dbgen.
if(MSVC)
  # Create an interface target whose options are appended AFTER Folly's transitive
  # options. This ensures our overrides (/std:c++20, /Zc:strictStrings-) win
  # regardless of what Folly's INTERFACE_COMPILE_OPTIONS inject.
  add_library(dbgen_msvc_overrides INTERFACE)
  target_compile_options(dbgen_msvc_overrides INTERFACE
    /w
    # Disable strict string literal rules: dbgen (legacy TPC-H code) assigns
    # string literals to char* (not const char*), which MSVC rejects under
    # /Zc:strictStrings (inherited from Folly's transitive compile options).
    /Zc:strictStrings-
    # Remove /permissive- flag that Folly adds, to allow C-style casts and
    # other legacy patterns in TPC-H code.
    /permissive
    # Force C++20 last so it overrides /std:c++17 from Folly's transitive
    # interface compile options. This must come after all other flags.
    /std:c++20
  )
  # Link AFTER Folly so our INTERFACE options appear last in the command line.
  target_link_libraries(dbgen PRIVATE dbgen_msvc_overrides)
else()
  target_compile_options(dbgen PRIVATE -w)
endif()
