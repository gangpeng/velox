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

#include <charconv>
#include <sstream>
#include <string>
#include "velox/common/base/CheckedArithmetic.h"
#include "velox/common/base/CountBits.h"
#include "velox/common/base/Doubles.h"
#include "velox/common/base/Exceptions.h"
#include "velox/common/base/Nulls.h"
#include "velox/common/base/Status.h"
#include "velox/type/Type.h"

namespace facebook::velox {

// On MSVC, absl::int128 arithmetic is not constexpr (C2131), so we provide
// pre-computed values using absl::MakeInt128 for entries that exceed int64_t.

/// A static class that holds helper functions for DECIMAL type.
class DecimalUtil {
 public:
#ifdef _MSC_VER
  // MSVC: absl::int128::operator* is not constexpr. Use inline static const
  // with pre-computed literal values for the large entries.
  inline static const int128_t kPowersOfTen[LongDecimalType::kMaxPrecision + 1] = {
      absl::MakeInt128(0, 1ULL),
      absl::MakeInt128(0, 10ULL),
      absl::MakeInt128(0, 100ULL),
      absl::MakeInt128(0, 1'000ULL),
      absl::MakeInt128(0, 10'000ULL),
      absl::MakeInt128(0, 100'000ULL),
      absl::MakeInt128(0, 1'000'000ULL),
      absl::MakeInt128(0, 10'000'000ULL),
      absl::MakeInt128(0, 100'000'000ULL),
      absl::MakeInt128(0, 1'000'000'000ULL),
      absl::MakeInt128(0, 10'000'000'000ULL),
      absl::MakeInt128(0, 100'000'000'000ULL),
      absl::MakeInt128(0, 1'000'000'000'000ULL),
      absl::MakeInt128(0, 10'000'000'000'000ULL),
      absl::MakeInt128(0, 100'000'000'000'000ULL),
      absl::MakeInt128(0, 1'000'000'000'000'000ULL),
      absl::MakeInt128(0, 10'000'000'000'000'000ULL),
      absl::MakeInt128(0, 100'000'000'000'000'000ULL),
      absl::MakeInt128(0, 1'000'000'000'000'000'000ULL),
      // 10^19
      absl::MakeInt128(0x0, 0x8AC7230489E80000ULL),
      // 10^20
      absl::MakeInt128(0x5, 0x6BC75E2D63100000ULL),
      // 10^21
      absl::MakeInt128(0x36, 0x35C9ADC5DEA00000ULL),
      // 10^22
      absl::MakeInt128(0x21E, 0x19E0C9BAB2400000ULL),
      // 10^23
      absl::MakeInt128(0x152D, 0x02C7E14AF6800000ULL),
      // 10^24
      absl::MakeInt128(0xD3C2, 0x1BCECCEDA1000000ULL),
      // 10^25
      absl::MakeInt128(0x84595, 0x161401484A000000ULL),
      // 10^26
      absl::MakeInt128(0x52B7D2, 0xDCC80CD2E4000000ULL),
      // 10^27
      absl::MakeInt128(0x33B2E3C, 0x9FD0803CE8000000ULL),
      // 10^28
      absl::MakeInt128(0x204FCE5E, 0x3E25026110000000ULL),
      // 10^29
      absl::MakeInt128(0x1431E0FAE, 0x6D7217CAA0000000ULL),
      // 10^30
      absl::MakeInt128(0xC9F2C9CD0, 0x4674EDEA40000000ULL),
      // 10^31
      absl::MakeInt128(0x7E37BE2022, 0xC0914B2680000000ULL),
      // 10^32
      absl::MakeInt128(0x4EE2D6D415B, 0x85ACEF8100000000ULL),
      // 10^33
      absl::MakeInt128(0x314DC6448D93, 0x38C15B0A00000000ULL),
      // 10^34
      absl::MakeInt128(0x1ED09BEAD87C0, 0x378D8E6400000000ULL),
      // 10^35
      absl::MakeInt128(0x13426172C74D82, 0x2B878FE800000000ULL),
      // 10^36
      absl::MakeInt128(0xC097CE7BC90715, 0xB34B9F1000000000ULL),
      // 10^37
      absl::MakeInt128(0x785EE10D5DA46D9, 0x00F436A000000000ULL),
      // 10^38
      absl::MakeInt128(0x4B3B4CA85A86C47A, 0x098A224000000000ULL),
  };
#else
  static constexpr int128_t kPowersOfTen[LongDecimalType::kMaxPrecision + 1] = {
      1,
      10,
      100,
      1'000,
      10'000,
      100'000,
      1'000'000,
      10'000'000,
      100'000'000,
      1'000'000'000,
      10'000'000'000,
      100'000'000'000,
      1'000'000'000'000,
      10'000'000'000'000,
      100'000'000'000'000,
      1'000'000'000'000'000,
      10'000'000'000'000'000,
      100'000'000'000'000'000,
      1'000'000'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)10,
      1'000'000'000'000'000'000 * (int128_t)100,
      1'000'000'000'000'000'000 * (int128_t)1'000,
      1'000'000'000'000'000'000 * (int128_t)10'000,
      1'000'000'000'000'000'000 * (int128_t)100'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000,
      1'000'000'000'000'000'000 * (int128_t)10'000'000,
      1'000'000'000'000'000'000 * (int128_t)100'000'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)10'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)100'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)10'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)100'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)10'000'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)100'000'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000'000'000'000,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000'000'000'000 *
          (int128_t)10,
      1'000'000'000'000'000'000 * (int128_t)1'000'000'000'000'000'000 *
          (int128_t)100};
#endif

#ifdef _MSC_VER
  inline static const int128_t kLongDecimalMin =
      -kPowersOfTen[LongDecimalType::kMaxPrecision] + int128_t(1);
  inline static const int128_t kLongDecimalMax =
      kPowersOfTen[LongDecimalType::kMaxPrecision] - int128_t(1);
  inline static const int128_t kShortDecimalMin =
      -kPowersOfTen[ShortDecimalType::kMaxPrecision] + int128_t(1);
  inline static const int128_t kShortDecimalMax =
      kPowersOfTen[ShortDecimalType::kMaxPrecision] - int128_t(1);
#else
  static constexpr int128_t kLongDecimalMin =
      -kPowersOfTen[LongDecimalType::kMaxPrecision] + 1;
  static constexpr int128_t kLongDecimalMax =
      kPowersOfTen[LongDecimalType::kMaxPrecision] - 1;
  static constexpr int128_t kShortDecimalMin =
      -kPowersOfTen[ShortDecimalType::kMaxPrecision] + 1;
  static constexpr int128_t kShortDecimalMax =
      kPowersOfTen[ShortDecimalType::kMaxPrecision] - 1;
#endif

  /// Scale threshold for scientific notation.
  static constexpr int32_t kMinScientificNotationScale = 6;

  static constexpr uint64_t kInt64Mask = ~(static_cast<uint64_t>(1) << 63);
#ifdef _MSC_VER
  // absl::uint128 shift is not constexpr on MSVC.
  inline static const uint128_t kInt128Mask = (static_cast<uint128_t>(1) << 127);
#else
  static constexpr uint128_t kInt128Mask = (static_cast<uint128_t>(1) << 127);
#endif

  FOLLY_ALWAYS_INLINE static void valueInRange(int128_t value) {
    VELOX_USER_CHECK(
        (value >= kLongDecimalMin && value <= kLongDecimalMax),
        "Decimal overflow. Value '{}' is not in the range of Decimal Type",
        value);
  }

  // Returns true if the precision can represent the value.
  template <typename T>
  FOLLY_ALWAYS_INLINE static bool valueInPrecisionRange(
      T value,
      uint8_t precision) {
    return value < kPowersOfTen[precision] && value > -kPowersOfTen[precision];
  }

  /// Helper function to convert a decimal value to string.
  static std::string toString(int128_t value, const Type& type);

  // TODO Remove.
  static std::string toString(int128_t value, const TypePtr& type) {
    return toString(value, *type);
  }

  template <typename T>
  inline static void fillDecimals(
      T* decimals,
      const uint64_t* nullsPtr,
      const T* values,
      const int64_t* scales,
      int32_t numValues,
      int32_t targetScale) {
    for (int32_t i = 0; i < numValues; i++) {
      if (!nullsPtr || !bits::isBitNull(nullsPtr, i)) {
        int32_t currentScale = scales[i];
        T value = values[i];
        if constexpr (std::is_same_v<T, std::int64_t>) { // Short Decimal
          if (targetScale > currentScale &&
              targetScale - currentScale <= ShortDecimalType::kMaxPrecision) {
            value *= static_cast<T>(kPowersOfTen[targetScale - currentScale]);
          } else if (
              targetScale < currentScale &&
              currentScale - targetScale <= ShortDecimalType::kMaxPrecision) {
            value /= static_cast<T>(kPowersOfTen[currentScale - targetScale]);
          } else if (targetScale != currentScale) {
            VELOX_FAIL("Decimal scale out of range");
          }
        } else { // Long Decimal
          if (targetScale > currentScale) {
            while (targetScale > currentScale) {
              int32_t scaleAdjust = std::min<int32_t>(
                  ShortDecimalType::kMaxPrecision, targetScale - currentScale);
              value *= kPowersOfTen[scaleAdjust];
              currentScale += scaleAdjust;
            }
          } else if (targetScale < currentScale) {
            while (currentScale > targetScale) {
              int32_t scaleAdjust = std::min<int32_t>(
                  ShortDecimalType::kMaxPrecision, currentScale - targetScale);
              value /= kPowersOfTen[scaleAdjust];
              currentScale -= scaleAdjust;
            }
          }
        }
        decimals[i] = value;
      }
    }
  }

  template <typename TInput, typename TOutput>
  inline static Status rescaleWithRoundUp(
      TInput inputValue,
      int fromPrecision,
      int fromScale,
      int toPrecision,
      int toScale,
      TOutput& output) {
    int128_t rescaledValue = inputValue;
    auto scaleDifference = toScale - fromScale;
    bool isOverflow = false;
    if (scaleDifference >= 0) {
#ifdef _MSC_VER
      {
        const int128_t factor = DecimalUtil::kPowersOfTen[scaleDifference];
        if (factor != 0) {
          static const int128_t kInt128Max = absl::Int128Max();
          static const int128_t kInt128Min = absl::Int128Min();
          if (rescaledValue > kInt128Max / factor ||
              rescaledValue < kInt128Min / factor) {
            isOverflow = true;
          } else {
            rescaledValue *= factor;
          }
        }
      }
#else
      isOverflow = __builtin_mul_overflow(
          rescaledValue,
          DecimalUtil::kPowersOfTen[scaleDifference],
          &rescaledValue);
#endif
    } else {
      scaleDifference = -scaleDifference;
      const auto scalingFactor = DecimalUtil::kPowersOfTen[scaleDifference];
      rescaledValue /= scalingFactor;
      int128_t remainder = inputValue % scalingFactor;
      if (inputValue >= 0 && remainder >= scalingFactor / 2) {
        ++rescaledValue;
      } else if (remainder <= -scalingFactor / 2) {
        --rescaledValue;
      }
    }
    // Check overflow.
    if (!valueInPrecisionRange(rescaledValue, toPrecision) || isOverflow) {
      return Status::UserError(
          "Cannot cast DECIMAL '{}' to DECIMAL({}, {})",
          DecimalUtil::toString(inputValue, DECIMAL(fromPrecision, fromScale)),
          toPrecision,
          toScale);
    }
    output = static_cast<TOutput>(rescaledValue);
    return Status::OK();
  }

  template <typename TInput, typename TOutput>
  inline static std::optional<TOutput>
  rescaleInt(TInput inputValue, int toPrecision, int toScale) {
    int128_t rescaledValue = static_cast<int128_t>(inputValue);
#ifdef _MSC_VER
    bool isOverflow = false;
    {
      const int128_t factor = DecimalUtil::kPowersOfTen[toScale];
      if (factor != 0) {
        static const int128_t kInt128Max = absl::Int128Max();
        static const int128_t kInt128Min = absl::Int128Min();
        if (rescaledValue > kInt128Max / factor ||
            rescaledValue < kInt128Min / factor) {
          isOverflow = true;
        } else {
          rescaledValue *= factor;
        }
      }
    }
#else
    bool isOverflow = __builtin_mul_overflow(
        rescaledValue, DecimalUtil::kPowersOfTen[toScale], &rescaledValue);
#endif
    // Check overflow.
    if (!valueInPrecisionRange(rescaledValue, toPrecision) || isOverflow) {
      VELOX_USER_FAIL(
          "Cannot cast {} '{}' to DECIMAL({}, {})",
          SimpleTypeTrait<TInput>::name,
          inputValue,
          toPrecision,
          toScale);
    }
    return static_cast<TOutput>(rescaledValue);
  }

  /// Rescales a floating point value to decimal value of given precision and
  /// scale. Returns error status if fails.
  /// @tparam TInput Either float or double.
  /// @tparam TOutput Either int64_t or int128_t.
  template <typename TInput, typename TOutput>
  inline static Status rescaleFloatingPoint(
      TInput value,
      int precision,
      int scale,
      TOutput& output) {
    if (!std::isfinite(value)) {
      return Status::UserError("The input value should be finite.");
    }

    TInput maxValue;
    if constexpr (std::is_same_v<TOutput, int64_t>) {
      maxValue = kMaxDoubleBelowInt64Max;
    } else {
      maxValue = kMaxDoubleBelowInt128Max;
    }

#ifdef _MSC_VER
    // MSVC: std::numeric_limits<int128_t> has no specialization; use absl helpers.
    {
      TInput minVal;
      if constexpr (std::is_same_v<TOutput, int64_t>) {
        minVal = static_cast<TInput>(std::numeric_limits<int64_t>::min());
      } else {
        minVal = static_cast<TInput>(absl::Int128Min());
      }
      if (value <= minVal || value > maxValue) {
        return Status::UserError("Result overflows.");
      }
    }
#else
    if (value <= std::numeric_limits<TOutput>::min() || value > maxValue) {
      return Status::UserError("Result overflows.");
    }
#endif

    uint8_t digits;
    if constexpr (std::is_same_v<TInput, float>) {
      // A float provides nearly 7 precise digits.
      digits = 7;
    } else {
      // A double provides from 15 to 17 decimal digits, so at least 15 digits
      // are precise.
      digits = 15;
    }

    // Calculate the precise fractional digits.
    const auto integralValue = static_cast<uint128_t>(std::abs(value));
    const auto integralDigits =
        integralValue == 0 ? 0 : countDigits(integralValue);
    const auto fractionDigits = std::max(digits - integralDigits, 0);

    // Scales up the input value with all the precise fractional digits kept.
    // Convert value as long double type because 1) double * int128_t returns
    // int128_t and fractional digits are lost. 2) we could also convert the
    // int128_t value as double to avoid 'double * int128_t', but double
    // multiplication gives inaccurate result on large numbers. For example,
    // -3333030000000000000 * 1e3 = -3333030000000000065536. No need to
    // consider the result becoming infinite as DOUBLE_MAX * 10^38 <
    // LONG_DOUBLE_MAX.
    long double scaledValue = std::round(
        (long double)value * static_cast<long double>(DecimalUtil::kPowersOfTen[fractionDigits]));
#ifdef _MSC_VER
    // MSVC: folly::tryTo<int128_t>(long double) is not supported; do a manual
    // range check and direct cast instead.
    TOutput rescaledValue;
    {
      long double minCast, maxCast;
      if constexpr (std::is_same_v<TOutput, int64_t>) {
        minCast = static_cast<long double>(std::numeric_limits<int64_t>::min());
        maxCast = static_cast<long double>(std::numeric_limits<int64_t>::max());
      } else {
        minCast = static_cast<long double>(absl::Int128Min());
        maxCast = static_cast<long double>(absl::Int128Max());
      }
      if (scaledValue < minCast || scaledValue > maxCast) {
        return Status::UserError("Result overflows.");
      }
      rescaledValue = static_cast<TOutput>(scaledValue);
    }
#else
    const auto result = folly::tryTo<TOutput>(scaledValue);
    if (result.hasError()) {
      return Status::UserError("Result overflows.");
    }
    TOutput rescaledValue = result.value();
#endif
    if (scale > fractionDigits) {
#ifdef _MSC_VER
      // MSVC: __builtin_mul_overflow is not available; use a manual check.
      {
        const TOutput factor =
            static_cast<TOutput>(DecimalUtil::kPowersOfTen[scale - fractionDigits]);
        bool isOverflow = false;
        if constexpr (std::is_same_v<TOutput, int128_t>) {
          const int128_t kMax = absl::Int128Max();
          const int128_t kMin = absl::Int128Min();
          if (factor != 0 &&
              (rescaledValue > kMax / factor ||
               rescaledValue < kMin / factor)) {
            isOverflow = true;
          } else {
            rescaledValue *= factor;
          }
        } else {
          if (factor != 0 &&
              (rescaledValue >
                   std::numeric_limits<int64_t>::max() / factor ||
               rescaledValue <
                   std::numeric_limits<int64_t>::min() / factor)) {
            isOverflow = true;
          } else {
            rescaledValue *= factor;
          }
        }
        if (isOverflow) {
          return Status::UserError("Result overflows.");
        }
      }
#else
      bool isOverflow = __builtin_mul_overflow(
          rescaledValue,
          DecimalUtil::kPowersOfTen[scale - fractionDigits],
          &rescaledValue);
      if (isOverflow) {
        return Status::UserError("Result overflows.");
      }
#endif
    } else {
      const auto scalingFactor =
          DecimalUtil::kPowersOfTen[fractionDigits - scale];
      divideWithRoundUp<TOutput, TOutput, int128_t>(
          rescaledValue, rescaledValue, scalingFactor, false, 0, 0);
    }

    if (!valueInPrecisionRange<TOutput>(rescaledValue, precision)) {
      return Status::UserError(
          "Result cannot fit in the given precision {}.", precision);
    }
    output = rescaledValue;
    return Status::OK();
  }

  template <typename R, typename A, typename B>
  inline static R divideWithRoundUp(
      R& r,
      A a,
      B b,
      bool noRoundUp,
      uint8_t aRescale,
      uint8_t /*bRescale*/) {
    VELOX_USER_CHECK_NE(b, 0, "Division by zero");
    int resultSign = 1;
    R unsignedDividendRescaled(a);
    if (a < 0) {
      resultSign = -1;
      unsignedDividendRescaled *= -1;
    }
    B unsignedDivisor(b);
    if (b < 0) {
      resultSign *= -1;
      unsignedDivisor *= -1;
    }
    unsignedDividendRescaled = checkedMultiply<R>(
        unsignedDividendRescaled,
        R(DecimalUtil::kPowersOfTen[aRescale]),
        "Decimal");
    R quotient = static_cast<R>(unsignedDividendRescaled / unsignedDivisor);
    R remainder = static_cast<R>(unsignedDividendRescaled % unsignedDivisor);
    if (!noRoundUp && static_cast<const B>(remainder) * 2 >= unsignedDivisor) {
      ++quotient;
    }
    r = quotient * resultSign;
    return remainder * resultSign;
  }

  /// Returns the max required size to convert the decimal of this precision and
  /// scale to varchar. A varchar's size is estimated with unscaled value
  /// digits, dot, leading zero, and possible minus sign.
  static int32_t maxStringViewSize(int precision, int scale);

  /// Converts a 128-bit unsigned integer to decimal characters, writing into
  /// [first, last). Returns a to_chars_result-compatible struct with ptr and
  /// ec fields. On all platforms this handles absl::uint128 (MSVC) and
  /// __uint128_t (GCC/Clang), since std::to_chars lacks 128-bit overloads on
  /// MSVC.
  static std::to_chars_result uint128ToChars(
      char* first,
      char* last,
      uint128_t value) {
    if (first >= last) {
      return {last, std::errc::value_too_large};
    }
    if (value == 0) {
      *first = '0';
      return {first + 1, std::errc()};
    }
    // Write digits in reverse, then reverse them.
    char* start = first;
    char* cur = first;
    while (value > 0) {
      if (cur >= last) {
        return {last, std::errc::value_too_large};
      }
#ifdef _MSC_VER
      *cur++ = '0' + static_cast<char>(
                         static_cast<uint64_t>(value % uint128_t{10}));
      value /= uint128_t{10};
#else
      *cur++ = '0' + static_cast<char>(static_cast<uint64_t>(value % 10));
      value /= 10;
#endif
    }
    std::reverse(start, cur);
    return {cur, std::errc()};
  }

  /// Converts a 128-bit signed integer to decimal characters, writing into
  /// [first, last). Returns a to_chars_result-compatible struct.
  static std::to_chars_result int128ToChars(
      char* first,
      char* last,
      int128_t value) {
    if (value < 0) {
      if (first >= last) {
        return {last, std::errc::value_too_large};
      }
      *first++ = '-';
      // Negate safely as uint128_t to handle INT128_MIN.
      uint128_t uval = uint128_t{0} - static_cast<uint128_t>(value);
      auto res = uint128ToChars(first, last, uval);
      return res;
    }
    return uint128ToChars(first, last, static_cast<uint128_t>(value));
  }

  /// @brief Convert the unscaled value of a decimal to string and write to raw
  /// string buffer from start position.
  /// @tparam T The type of input value.
  /// @param unscaledValue The input unscaled value.
  /// @param scale The scale of decimal.
  /// @param maxSize The estimated max size of string.
  /// @param startPosition The start position to write from.
  /// @param isScientific Whether to format small magnitude decimals using
  /// scientific notation (Spark-compatible). When true, absolute values less
  /// than 1e-6 are formatted in scientific notation. For example:
  /// - With scale=20 and value=1: "1E-20" (scientific) vs
  /// "0.00000000000000000001" (normal)
  /// @return The number of characters written starting from startPosition.
  template <typename T>
  static size_t castToString(
      T unscaledValue,
      int32_t scale,
      int32_t maxSize,
      char* const startPosition,
      bool isScientific = false) {
    char* writePosition = startPosition;
    if (unscaledValue == 0) {
      *writePosition++ = '0';
      if (isScientific && scale > kMinScientificNotationScale) {
        *writePosition++ = 'E';
        auto exp =
            std::to_chars(writePosition, startPosition + maxSize, -scale);
        VELOX_DCHECK_EQ(
            exp.ec,
            std::errc(),
            "Failed to cast exponent value to varchar: {}",
            std::make_error_code(exp.ec).message());
        VELOX_DCHECK_LE(exp.ptr - startPosition, maxSize);
        writePosition = exp.ptr;
      } else if (scale > 0) {
        *writePosition++ = '.';
        // Append trailing zeros.
        std::memset(writePosition, '0', scale);
        writePosition += scale;
      }
    } else {
      if (unscaledValue < 0) {
        *writePosition++ = '-';
        unscaledValue = -unscaledValue;
      }
      if (isScientific) {
        if (scale >= kMinScientificNotationScale &&
            unscaledValue < DecimalUtil::kPowersOfTen
                                [scale - kMinScientificNotationScale]) {
          // Use scientific notation if the absolute value is less than 1e-6.
          // This is consistent with Spark's behavior.
          const auto digits = countDigits(unscaledValue);
          auto coefficientBuf = std::vector<char>(digits);
          const auto coefficient = int128ToChars(
              coefficientBuf.data(),
              coefficientBuf.data() + digits,
              unscaledValue);
          VELOX_DCHECK_EQ(
              coefficient.ec,
              std::errc(),
              "Failed to cast coefficient to varchar.");
          VELOX_DCHECK_EQ(coefficient.ptr, coefficientBuf.data() + digits);
          *writePosition++ = coefficientBuf[0];
          if (coefficient.ptr - coefficientBuf.data() > 1) {
            *writePosition++ = '.';
            size_t toCopy = digits - 1;
            std::memcpy(writePosition, coefficientBuf.data() + 1, toCopy);
            writePosition += toCopy;
          }
          *writePosition++ = 'E';
          const auto adjusted = digits - 1 - scale;
          auto exp = std::to_chars(
              writePosition, writePosition + maxSize - digits - 2, adjusted);
          VELOX_DCHECK_EQ(
              exp.ec,
              std::errc(),
              "Failed to cast exponent value to varchar: {}",
              std::make_error_code(exp.ec).message());
          writePosition = exp.ptr;
          return writePosition - startPosition;
        }
      }
      auto [position, errorCode] = int128ToChars(
          writePosition,
          writePosition + maxSize,
          unscaledValue / DecimalUtil::kPowersOfTen[scale]);
      VELOX_DCHECK_EQ(
          errorCode,
          std::errc(),
          "Failed to cast decimal to varchar: {}",
          std::make_error_code(errorCode).message());
      writePosition = position;

      if (scale > 0) {
        *writePosition++ = '.';
        uint128_t fraction = unscaledValue % DecimalUtil::kPowersOfTen[scale];
        // Append leading zeros.
        int numLeadingZeros = std::max(scale - countDigits(fraction), 0);
        std::memset(writePosition, '0', numLeadingZeros);
        writePosition += numLeadingZeros;
        // Append remaining fraction digits.
        auto result =
            uint128ToChars(writePosition, writePosition + maxSize, fraction);
        VELOX_DCHECK_EQ(
            result.ec,
            std::errc(),
            "Failed to cast decimal to varchar: {}",
            std::make_error_code(result.ec).message());
        writePosition = result.ptr;
      }
    }
    return writePosition - startPosition;
  }

  /*
   * sum up and return overflow/underflow.
   */
  inline static int64_t addUnsignedValues(
      int128_t& sum,
      int128_t lhs,
      int128_t rhs,
      bool isResultNegative) {
    __uint128_t unsignedSum = (__uint128_t)lhs + (__uint128_t)rhs;
    // Ignore overflow value.
#ifdef _MSC_VER
    // absl::uint128 → absl::int128 requires explicit construction on MSVC.
    // Clear bit 127 (the overflow indicator) by masking the high word.
    sum = absl::MakeInt128(
        static_cast<int64_t>(
            absl::Uint128High64(unsignedSum) & 0x7FFFFFFFFFFFFFFFULL),
        absl::Uint128Low64(unsignedSum));
    sum = isResultNegative ? -sum : sum;
    return static_cast<int64_t>(absl::Uint128Low64(unsignedSum >> 127));
#else
    sum = (int128_t)unsignedSum & ~kOverflowMultiplier;
    sum = isResultNegative ? -sum : sum;
    return (unsignedSum >> 127);
#endif
  }

  /// Adds two signed 128-bit numbers (int128_t), calculates the sum, and
  /// returns the overflow. It can be used to track the number of overflow when
  /// adding a batch of input numbers. It takes lhs and rhs as input, and stores
  /// their sum in result. overflow == 1 indicates upward overflow. overflow ==
  /// -1 indicates downward overflow. overflow == 0 indicates no overflow.
  /// Adding negative and non-negative numbers never overflows, so we can
  /// directly add them. Adding two negative or two positive numbers may
  /// overflow. To add numbers that may overflow, first convert both numbers to
  /// unsigned 128-bit number (uint128_t), and perform the addition. The highest
  /// bits in the result indicates overflow. Adjust the signs of sum and
  /// overflow based on the signs of the inputs. The caller must sum up overflow
  /// values and call adjustSumForOverflow after processing all inputs.
  inline static int64_t
  addWithOverflow(int128_t& result, int128_t lhs, int128_t rhs) {
    bool isLhsNegative = lhs < 0;
    bool isRhsNegative = rhs < 0;
    int64_t overflow = 0;
    if (isLhsNegative == isRhsNegative) {
      // Both inputs of same time.
      if (isLhsNegative) {
        // Both negative, ignore signs and add.
        VELOX_DCHECK_NE(lhs, std::numeric_limits<int128_t>::min());
        VELOX_DCHECK_NE(rhs, std::numeric_limits<int128_t>::min());
        overflow = addUnsignedValues(result, -lhs, -rhs, true);
        overflow = -overflow;
      } else {
        overflow = addUnsignedValues(result, lhs, rhs, false);
      }
    } else {
      // If one of them is negative, use addition.
      result = lhs + rhs;
    }
    return overflow;
  }

  /// Corrects the sum result calculated using addWithOverflow. Since the sum
  /// calculated by addWithOverflow only retains the lower 127 bits,
  /// it may miss one calculation of +(1 << 127) or -(1 << 127).
  /// Therefore, we need to make the following adjustments:
  /// 1. If overflow = 1 && sum < 0, the calculation missed +(1 << 127).
  /// Add 1 << 127 to the sum.
  /// 2. If overflow = -1 && sum > 0, the calculation missed -(1 << 127).
  /// Subtract 1 << 127 to the sum.
  /// If an overflow indeed occurs and the result cannot be adjusted,
  /// it will return std::nullopt.
  inline static std::optional<int128_t> adjustSumForOverflow(
      int128_t sum,
      int64_t overflow) {
    // Value is valid if the conditions below are true.
    if ((overflow == 1 && sum < 0) || (overflow == -1 && sum > 0)) {
#ifdef _MSC_VER
      // On MSVC, kOverflowMultiplier is absl::uint128. To avoid mixed-type
      // 128-bit arithmetic, do the computation in uint128_t then cast.
      // overflow is +1 or -1, kOverflowMultiplier = 2^127.
      // For overflow==1:  result = sum + 2^127 (sum is negative, result fits)
      // For overflow==-1: result = sum - 2^127 (sum is positive, result fits)
      uint128_t usum = static_cast<uint128_t>(sum);
      uint128_t uresult = (overflow >= 0)
          ? (usum + kOverflowMultiplier)
          : (usum - kOverflowMultiplier);
      return static_cast<int128_t>(uresult);
#else
      return static_cast<int128_t>(
          DecimalUtil::kOverflowMultiplier * overflow + sum);
#endif
    }
    if (overflow != 0) {
      // The actual overflow occurred.
      return std::nullopt;
    }

    return sum;
  }

  /// avg = (sum + overflow * kOverflowMultiplier) / count
  static void
  computeAverage(int128_t& avg, int128_t sum, int64_t count, int64_t overflow);

  /// Origins from java side BigInteger#bitLength.
  ///
  /// Returns the number of bits in the minimal two's-complement
  /// representation of this BigInteger, <em>excluding</em> a sign bit.
  /// For positive BigIntegers, this is equivalent to the number of bits in
  /// the ordinary binary representation.  For zero this method returns
  /// {@code 0}.  (Computes {@code (ceil(log2(this < 0 ? -this : this+1)))}.)
  ///
  /// @return number of bits in the minimal two's-complement
  ///         representation of this BigInteger, <em>excluding</em> a sign bit.
  static int32_t getByteArrayLength(int128_t value);

  /// This method return the same result with the BigInterger#toByteArray()
  /// method in Java side.
  ///
  /// Returns a byte array containing the two's-complement representation of
  /// this BigInteger. The byte array will be in big-endian byte-order: the most
  /// significant byte is in the zeroth element. The array will contain the
  /// minimum number of bytes required to represent this BigInteger, including
  /// at least one sign bit, which is (ceil((this.bitLength() + 1)/8)).
  ///
  /// @return The length of out.
  static int32_t toByteArray(int128_t value, char* out);

  /// Reverse byte order of an int128_t if native byte-order is little endian.
  /// If native byte-order is big endian, the value will be unchanged. This
  /// is similar to folly::Endian::big(), which does not support int128_t.
  ///
  /// \return A value with reversed byte-order for little endian platforms.
  inline static int128_t bigEndian(int128_t value) {
    if (folly::kIsLittleEndian) {
      auto upper = folly::Endian::big(HugeInt::upper(value));
      auto lower = folly::Endian::big(HugeInt::lower(value));
      return HugeInt::build(lower, upper);
    } else {
      return value;
    }
  }

  /// Converts string view to decimal value of given precision and scale.
  /// Derives from Arrow function DecimalFromString. Arrow implementation:
  /// https://github.com/apache/arrow/blob/56c0e2f508fdc5137d6734b406634386f9284a52/cpp/src/arrow/util/decimal.cc#L862.
  ///
  /// Firstly, it parses the varchar to DecimalComponents which contains the
  /// message that can represent a decimal value. Secondly, processes the
  /// exponent to get the scale. Thirdly, compute the rescaled value. Returns
  /// status for the outcome of computing.
  template <typename T>
  static Status castFromString(
      const StringView s,
      int32_t toPrecision,
      int32_t toScale,
      T& decimalValue) {
    int32_t parsedPrecision = 0;
    int32_t parsedScale = 0;
    int128_t out = 0;
    VELOX_RETURN_NOT_OK(parseStringToDecimalComponents(
        s, toScale, parsedPrecision, parsedScale, out));

    const auto status = rescaleWithRoundUp<int128_t, T>(
        out,
        std::min(
            static_cast<uint8_t>(parsedPrecision),
            LongDecimalType::kMaxPrecision),
        parsedScale,
        toPrecision,
        toScale,
        decimalValue);
    if (!status.ok()) {
      return Status::UserError("Value too large.");
    }
    return status;
  }

#ifdef _MSC_VER
  inline static const __uint128_t kOverflowMultiplier = ((__uint128_t)1 << 127);
#else
  static constexpr __uint128_t kOverflowMultiplier = ((__uint128_t)1 << 127);
#endif

 private:
  // Parses the string view to decimal components, which contains the
  // unscaled value, precision, and scale. The parsed precision and scale are
  // returned through the reference parameters. The unscaled value is returned
  // through the out parameter.
  static Status parseStringToDecimalComponents(
      const StringView& s,
      int32_t toScale,
      int32_t& parsedPrecision,
      int32_t& parsedScale,
      int128_t& out);
}; // DecimalUtil
} // namespace facebook::velox
