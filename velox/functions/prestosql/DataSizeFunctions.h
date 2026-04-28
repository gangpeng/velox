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

#include <map>
#include <string>
#include "velox/functions/Macros.h"
#include "velox/functions/lib/string/StringCore.h"
#include "velox/type/DecimalUtil.h"

namespace facebook::velox::functions {

// On MSVC, int128_t is absl::int128 (a class) and cannot be used as enum
// underlying type. Use constexpr int128_t constants instead.
namespace Unit {
static const int128_t BYTE = 1;
static const int128_t KILOBYTE = int128_t(1) << 10;
static const int128_t MEGABYTE = int128_t(1) << 20;
static const int128_t GIGABYTE = int128_t(1) << 30;
static const int128_t TERABYTE = int128_t(1) << 40;
static const int128_t PETABYTE = int128_t(1) << 50;
static const int128_t EXABYTE = int128_t(1) << 60;
static const int128_t ZETTABYTE = int128_t(1) << 70;
static const int128_t YOTTABYTE = int128_t(1) << 80;
} // namespace Unit

inline int128_t parseUnit(std::string_view dataSize, size_t valueLength) {
  static const std::map<std::string_view, int128_t> unitMap = {
      {"B", Unit::BYTE},
      {"kB", Unit::KILOBYTE},
      {"MB", Unit::MEGABYTE},
      {"GB", Unit::GIGABYTE},
      {"TB", Unit::TERABYTE},
      {"PB", Unit::PETABYTE},
      {"EB", Unit::EXABYTE},
      {"ZB", Unit::ZETTABYTE},
      {"YB", Unit::YOTTABYTE},
  };
  try {
    std::string_view unitString = dataSize.substr(valueLength);
    auto it = unitMap.find(unitString);
    VELOX_USER_CHECK(it != unitMap.end(), "Invalid data size: '{}'", dataSize);
    return it->second;
  } catch (const std::exception&) {
    VELOX_USER_FAIL("Invalid data size: '{}'", dataSize);
  }
}
inline double parseValue(std::string_view dataSize, size_t valueLength) {
  try {
    std::string_view value = dataSize.substr(0, valueLength);
    return folly::to<double>(value);
  } catch (const std::exception&) {
    VELOX_USER_FAIL("Invalid data size: '{}'", dataSize);
  }
}

inline int128_t getDecimal(std::string_view dataSize) {
  size_t valueLength = 0;
  while (valueLength < dataSize.length() &&
         (isdigit(dataSize[valueLength]) || dataSize[valueLength] == '.')) {
    valueLength++;
  }
  VELOX_USER_CHECK_GT(valueLength, 0, "Invalid data size: '{}'", dataSize);
  double value = parseValue(dataSize, valueLength);
  int128_t factor = parseUnit(dataSize, valueLength);

  int128_t scaledValue = static_cast<int128_t>(value * static_cast<double>(factor));

  // Ensure the result is within a valid range.
  try {
    DecimalUtil::valueInRange(scaledValue);
    std::vector<char> encodedValue(
        DecimalUtil::getByteArrayLength(scaledValue));
    DecimalUtil::toByteArray(scaledValue, encodedValue.data());
  } catch (const std::exception&) {
    VELOX_USER_FAIL("Value out of range: '{}' ('{}B')", dataSize, scaledValue);
  }
  return scaledValue;
}

template <typename TExec>
struct ParsePrestoDataSizeFunction {
  VELOX_DEFINE_FUNCTION_TYPES(TExec);
  static constexpr bool is_default_ascii_behavior = true;
  FOLLY_ALWAYS_INLINE void callAscii(
      out_type<LongDecimal<P1, S1>>& result,
      const arg_type<Varchar>& input) {
    // TODO: Remove explicit std::string_view cast.
    result = getDecimal(std::string_view(input));
  }
  FOLLY_ALWAYS_INLINE void call(
      out_type<LongDecimal<P1, S1>>& result,
      const arg_type<Varchar>& input) {
    // If ASCII input process else fail.
    if (stringCore::isAscii(input.data(), input.size())) {
      // TODO: Remove explicit std::string_view cast.
      result = getDecimal(std::string_view(input));
    } else {
      VELOX_USER_FAIL("Invalid data size: '{}'", input);
    }
  }
};

} // namespace facebook::velox::functions
