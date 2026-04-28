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
project(Arrow)

if(VELOX_ENABLE_ARROW)
  if(Thrift_FOUND)
    set(THRIFT_SOURCE "SYSTEM")
  else()
    set(THRIFT_SOURCE "BUNDLED")
  endif()

  # Avoid issues in finding the boost headers and libraries
  # by setting the BOOST_ROOT to the install prefix.
  # The same logic is used in the setup script-common.sh to install boost.
  if(NOT DEFINED ENV{BOOST_ROOT} OR "$ENV{BOOST_ROOT}" STREQUAL "")
    if(DEFINED ENV{INSTALL_PREFIX} AND NOT "$ENV{INSTALL_PREFIX}" STREQUAL "")
      set(BOOST_ROOT "$ENV{INSTALL_PREFIX}")
    elseif(WIN32)
      # On Windows use the vcpkg install directory which has Boost headers.
      if(NOT DEFINED ENV{VCPKG_TRIPLET_INSTALL_DIR}
         OR "$ENV{VCPKG_TRIPLET_INSTALL_DIR}" STREQUAL "")
        message(
          WARNING
          "VCPKG_TRIPLET_INSTALL_DIR is not set. Arrow's Boost detection may "
          "fail. Set it to your vcpkg installed/<triplet> directory, e.g.:\n"
          "  set VCPKG_TRIPLET_INSTALL_DIR=C:/vcpkg/installed/x64-windows-static"
        )
      endif()
      set(BOOST_ROOT "$ENV{VCPKG_TRIPLET_INSTALL_DIR}")
    else()
      set(BOOST_ROOT "/usr/local")
    endif()
  else()
    set(BOOST_ROOT "$ENV{BOOST_ROOT}")
  endif()
  message(STATUS "Using BOOST_ROOT: ${BOOST_ROOT}")
  # Normalize path separators to forward slashes for cross-platform safety.
  # On Windows, backslashes in cmake string literals (e.g. in ExternalProject
  # configure scripts) are interpreted as escape sequences, causing parse errors.
  file(TO_CMAKE_PATH "${BOOST_ROOT}" BOOST_ROOT)
  # On Windows, use a short base path for the Arrow ExternalProject to avoid
  # exceeding cmake's CMAKE_OBJECT_PATH_MAX (250 chars) when Thrift builds its
  # nested try-compile tests. The default CMake_CURRENT_BINARY_DIR path is too
  # deep when combined with the "arrow_ep/src/arrow_ep-build/thrift_ep-..." nesting.
  if(WIN32)
    # Use the build drive root with a short name. Fall back to the system temp
    # directory when the drive root is not writable.
    string(SUBSTRING "${CMAKE_BINARY_DIR}" 0 3 _arrow_drive_root)
    set(_arrow_short_path "${_arrow_drive_root}ae")
    file(MAKE_DIRECTORY "${_arrow_short_path}" RESULT_VARIABLE _arrow_mkdir_result)
    if(_arrow_mkdir_result EQUAL 0 OR IS_DIRECTORY "${_arrow_short_path}")
      set(ARROW_PREFIX "${_arrow_short_path}")
    else()
      set(ARROW_PREFIX "$ENV{TEMP}/ae")
    endif()
  else()
    set(ARROW_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/arrow_ep")
  endif()
  set(
    ARROW_CMAKE_ARGS
    -DARROW_PARQUET=OFF
    -DARROW_DEPENDENCY_SOURCE=AUTO
    # Force Arrow to use static variants; vcpkg x64-windows-static only provides
    # static libraries, so shared targets (zstd::libzstd_shared etc.) don't exist.
    -DARROW_DEPENDENCY_USE_SHARED=OFF
    -DARROW_ZSTD_USE_SHARED=OFF
    -DARROW_LZ4_USE_SHARED=OFF
    -DARROW_WITH_THRIFT=ON
    -DARROW_WITH_LZ4=ON
    -DARROW_WITH_SNAPPY=ON
    -DARROW_WITH_ZLIB=ON
    -DARROW_WITH_ZSTD=ON
    -DARROW_JEMALLOC=OFF
    -DARROW_SIMD_LEVEL=NONE
    -DARROW_RUNTIME_SIMD_LEVEL=NONE
    -DARROW_WITH_UTF8PROC=OFF
    # ARROW_TESTING=OFF: we don't need Arrow's test library when VELOX_BUILD_TESTING=OFF,
    # and it avoids conflicts between Arrow's bundled googletest and vcpkg's gtest targets.
    -DARROW_TESTING=OFF
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_INSTALL_PREFIX=${ARROW_PREFIX}/install
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DARROW_BUILD_STATIC=ON
    -DARROW_BUILD_SHARED=OFF
    -DThrift_SOURCE=${THRIFT_SOURCE}
    -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
    -DBOOST_ROOT=${BOOST_ROOT}
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    # Do NOT pass the vcpkg toolchain to Arrow's ExternalProject:
    # vcpkg injects static-only zstd/lz4 targets (zstd::libzstd_static) but Arrow
    # 18.x requires zstd::libzstd_shared. Instead we pass CMAKE_PREFIX_PATH so Arrow
    # can still find these libraries through its own cmake modules, which handle both
    # static and shared transparently.
  )
  # On MSVC, force static CRT (/MT or /MTd) to match the x64-windows-static
  # vcpkg triplet used for all other Velox dependencies.  ARROW_USE_STATIC_CRT
  # makes Arrow's ThirdpartyToolchain propagate the static CRT to bundled
  # ExternalProjects (Thrift, etc.) via EP_CXX_FLAGS.
  if(MSVC)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      list(APPEND ARROW_CMAKE_ARGS
           "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug")
    else()
      list(APPEND ARROW_CMAKE_ARGS
           "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded")
    endif()
    list(APPEND ARROW_CMAKE_ARGS "-DARROW_USE_STATIC_CRT=ON")
  endif()
  # On Windows/MSVC static libs use .lib suffix with no prefix;
  # Arrow's cmake adds "_static" suffix on MSVC: arrow_static.lib / arrow_testing_static.lib.
  # On Linux/macOS they use lib<name>.a (no "_static" suffix).
  if(WIN32)
    set(ARROW_LIBDIR ${ARROW_PREFIX}/install/lib)
    set(ARROW_STATIC_LIB_NAME "arrow_static${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(ARROW_TESTING_STATIC_LIB_NAME "arrow_testing_static${CMAKE_STATIC_LIBRARY_SUFFIX}")
  else()
    set(ARROW_LIBDIR ${ARROW_PREFIX}/install/lib)
    set(ARROW_STATIC_LIB_NAME "libarrow${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(ARROW_TESTING_STATIC_LIB_NAME "libarrow_testing${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endif()

  add_library(thrift STATIC IMPORTED GLOBAL)
  if(NOT Thrift_FOUND)
    set(THRIFT_ROOT ${ARROW_PREFIX}/src/arrow_ep-build/thrift_ep-install)
    if(WIN32)
      # Arrow's bundled Thrift on Windows/MSVC Release builds with ARROW_USE_STATIC_CRT=ON
      # produces thriftmt.lib (static CRT /MT). Without it, it produces thriftmd.lib (/MD).
      # We always use ARROW_USE_STATIC_CRT=ON on Windows to match the x64-windows-static vcpkg triplet.
      # Arrow installs the bundled thrift lib to bin/ on Windows (MSVC puts .lib files there by default).
      set(THRIFT_LIB ${THRIFT_ROOT}/bin/thriftmt${CMAKE_STATIC_LIBRARY_SUFFIX})
    else()
      set(THRIFT_LIB ${THRIFT_ROOT}/lib/libthrift${CMAKE_STATIC_LIBRARY_SUFFIX})
    endif()

    file(MAKE_DIRECTORY ${THRIFT_ROOT}/include)
    set(THRIFT_INCLUDE_DIR ${THRIFT_ROOT}/include)
  endif()

  set_property(TARGET thrift PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${THRIFT_INCLUDE_DIR})
  set_property(TARGET thrift PROPERTY IMPORTED_LOCATION ${THRIFT_LIB})

  set(VELOX_ARROW_BUILD_VERSION 18.0.0)
  set(
    VELOX_ARROW_BUILD_SHA256_CHECKSUM
    9c473f2c9914c59ab571761c9497cf0e5cfd3ea335f7782ccc6121f5cb99ae9b
  )
  set(
    VELOX_ARROW_SOURCE_URL
    "https://github.com/apache/arrow/archive/refs/tags/apache-arrow-${VELOX_ARROW_BUILD_VERSION}.tar.gz"
  )

  velox_resolve_dependency_url(ARROW)

  ExternalProject_Add(
    arrow_ep
    PREFIX ${ARROW_PREFIX}
    URL ${VELOX_ARROW_SOURCE_URL}
    URL_HASH ${VELOX_ARROW_BUILD_SHA256_CHECKSUM}
    SOURCE_SUBDIR cpp
    CMAKE_ARGS ${ARROW_CMAKE_ARGS}
    BUILD_BYPRODUCTS
      ${ARROW_LIBDIR}/${ARROW_STATIC_LIB_NAME}
      ${THRIFT_LIB}
    PATCH_COMMAND
      ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/apply-patches.cmake
  )

  add_library(arrow STATIC IMPORTED GLOBAL)
  # arrow_testing is an INTERFACE (empty) target because ARROW_TESTING=OFF when
  # VELOX_BUILD_TESTING=OFF; we still define the target so references to it don't fail.
  add_library(arrow_testing INTERFACE IMPORTED GLOBAL)
  add_dependencies(arrow arrow_ep)
  file(MAKE_DIRECTORY ${ARROW_PREFIX}/install/include)
  set_target_properties(
    arrow
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${ARROW_PREFIX}/install/include
  )
  set_target_properties(
    arrow PROPERTIES IMPORTED_LOCATION ${ARROW_LIBDIR}/${ARROW_STATIC_LIB_NAME}
  )
  set_property(TARGET arrow PROPERTY INTERFACE_LINK_LIBRARIES ${RE2} thrift)
  set_property(TARGET arrow_testing PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ARROW_PREFIX}/install/include)
  # On Windows, Arrow and Parquet are built as static libraries.  Without defining
  # ARROW_STATIC / PARQUET_STATIC the Arrow headers default to __declspec(dllimport)
  # which causes LNK2019 "unresolved external symbol __imp_..." errors when linking
  # code that includes those headers against the static arrow_static.lib.
  if(WIN32)
    set_property(
      TARGET arrow
      APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS ARROW_STATIC PARQUET_STATIC
    )
    set_property(
      TARGET arrow_testing
      APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS ARROW_STATIC PARQUET_STATIC
    )
  endif()
endif()
