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

if(MSVC)
  set(PROCESS_TEST_SOURCES
    ThreadLocalRegistryTest.cpp
    TraceContextTest.cpp
    TraceHistoryTest.cpp
  )
else()
  set(PROCESS_TEST_SOURCES
    ProfilerTest.cpp
    ThreadLocalRegistryTest.cpp
    TraceContextTest.cpp
    TraceHistoryTest.cpp
  )
endif()

add_executable(
  velox_process_test
  ${PROCESS_TEST_SOURCES}
)

add_test(velox_process_test velox_process_test)

target_link_libraries(
  velox_process_test
  PRIVATE velox_process velox_profiler fmt::fmt velox_time GTest::gtest GTest::gtest_main
)
