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

#include "velox/common/base/Portability.h"
#include "velox/common/base/BitUtil.h"
#include "velox/type/HugeInt.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace facebook::velox::test {

class PortabilityTest : public testing::Test {};

// --- __builtin_add_overflow tests ---

TEST_F(PortabilityTest, addOverflowInt32NoOverflow) {
  int32_t result;
  EXPECT_FALSE(__builtin_add_overflow(int32_t{1}, int32_t{2}, &result));
  EXPECT_EQ(result, 3);
}

TEST_F(PortabilityTest, addOverflowInt32PositiveOverflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int32_t>::max(), int32_t{1}, &result));
}

TEST_F(PortabilityTest, addOverflowInt32NegativeOverflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int32_t>::min(), int32_t{-1}, &result));
}

TEST_F(PortabilityTest, addOverflowInt32AtBoundary) {
  int32_t result;
  EXPECT_FALSE(__builtin_add_overflow(
      std::numeric_limits<int32_t>::max(), int32_t{0}, &result));
  EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
}

TEST_F(PortabilityTest, addOverflowUint32NoOverflow) {
  uint32_t result;
  EXPECT_FALSE(__builtin_add_overflow(uint32_t{1}, uint32_t{2}, &result));
  EXPECT_EQ(result, 3);
}

TEST_F(PortabilityTest, addOverflowUint32Overflow) {
  uint32_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<uint32_t>::max(), uint32_t{1}, &result));
}

TEST_F(PortabilityTest, addOverflowInt64PositiveOverflow) {
  int64_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int64_t>::max(), int64_t{1}, &result));
}

TEST_F(PortabilityTest, addOverflowInt64NegativeOverflow) {
  int64_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int64_t>::min(), int64_t{-1}, &result));
}

TEST_F(PortabilityTest, addOverflowInt64NoOverflow) {
  int64_t result;
  EXPECT_FALSE(__builtin_add_overflow(int64_t{100}, int64_t{200}, &result));
  EXPECT_EQ(result, 300);
}

// --- __builtin_sub_overflow tests ---

TEST_F(PortabilityTest, subOverflowInt32NoOverflow) {
  int32_t result;
  EXPECT_FALSE(__builtin_sub_overflow(int32_t{5}, int32_t{3}, &result));
  EXPECT_EQ(result, 2);
}

TEST_F(PortabilityTest, subOverflowInt32PositiveOverflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_sub_overflow(
      std::numeric_limits<int32_t>::max(), int32_t{-1}, &result));
}

TEST_F(PortabilityTest, subOverflowInt32NegativeOverflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_sub_overflow(
      std::numeric_limits<int32_t>::min(), int32_t{1}, &result));
}

TEST_F(PortabilityTest, subOverflowUint32Underflow) {
  uint32_t result;
  EXPECT_TRUE(__builtin_sub_overflow(uint32_t{0}, uint32_t{1}, &result));
}

TEST_F(PortabilityTest, subOverflowInt64PositiveOverflow) {
  int64_t result;
  EXPECT_TRUE(__builtin_sub_overflow(
      std::numeric_limits<int64_t>::max(), int64_t{-1}, &result));
}

// --- __builtin_mul_overflow tests ---

TEST_F(PortabilityTest, mulOverflowInt32NoOverflow) {
  int32_t result;
  EXPECT_FALSE(__builtin_mul_overflow(int32_t{6}, int32_t{7}, &result));
  EXPECT_EQ(result, 42);
}

TEST_F(PortabilityTest, mulOverflowInt32Overflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<int32_t>::max(), int32_t{2}, &result));
}

TEST_F(PortabilityTest, mulOverflowInt32NegativeOverflow) {
  int32_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<int32_t>::min(), int32_t{2}, &result));
}

TEST_F(PortabilityTest, mulOverflowInt32WithZero) {
  int32_t result;
  EXPECT_FALSE(__builtin_mul_overflow(int32_t{0}, int32_t{1'000'000}, &result));
  EXPECT_EQ(result, 0);
  EXPECT_FALSE(
      __builtin_mul_overflow(int32_t{1'000'000}, int32_t{0}, &result));
  EXPECT_EQ(result, 0);
}

TEST_F(PortabilityTest, mulOverflowInt64Overflow) {
  int64_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<int64_t>::max(), int64_t{2}, &result));
}

TEST_F(PortabilityTest, mulOverflowInt64NoOverflow) {
  int64_t result;
  EXPECT_FALSE(__builtin_mul_overflow(
      int64_t{1'000'000'000}, int64_t{1'000'000'000}, &result));
  EXPECT_EQ(result, 1'000'000'000'000'000'000LL);
}

TEST_F(PortabilityTest, mulOverflowUint64Overflow) {
  uint64_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<uint64_t>::max(), uint64_t{2}, &result));
}

TEST_F(PortabilityTest, mulOverflowUint64NoOverflow) {
  uint64_t result;
  EXPECT_FALSE(__builtin_mul_overflow(uint64_t{100}, uint64_t{200}, &result));
  EXPECT_EQ(result, 20'000);
}

// --- 128-bit overflow tests (absl::int128 / absl::uint128 on MSVC) ---

using int128_t = facebook::velox::int128_t;
using uint128_t = facebook::velox::uint128_t;

TEST_F(PortabilityTest, addOverflow128NoOverflow) {
  int128_t result;
  EXPECT_FALSE(__builtin_add_overflow(int128_t{100}, int128_t{200}, &result));
  EXPECT_EQ(result, 300);
}

TEST_F(PortabilityTest, addOverflow128PositiveOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int128_t>::max(), int128_t{1}, &result));
}

TEST_F(PortabilityTest, addOverflow128NegativeOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<int128_t>::min(), int128_t{-1}, &result));
}

TEST_F(PortabilityTest, subOverflow128NoOverflow) {
  int128_t result;
  EXPECT_FALSE(__builtin_sub_overflow(int128_t{300}, int128_t{100}, &result));
  EXPECT_EQ(result, 200);
}

TEST_F(PortabilityTest, subOverflow128PositiveOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_sub_overflow(
      std::numeric_limits<int128_t>::max(), int128_t{-1}, &result));
}

TEST_F(PortabilityTest, subOverflow128NegativeOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_sub_overflow(
      std::numeric_limits<int128_t>::min(), int128_t{1}, &result));
}

TEST_F(PortabilityTest, mulOverflow128NoOverflow) {
  int128_t result;
  EXPECT_FALSE(__builtin_mul_overflow(int128_t{1000}, int128_t{2000}, &result));
  EXPECT_EQ(result, 2'000'000);
}

TEST_F(PortabilityTest, mulOverflow128PositiveOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<int128_t>::max(), int128_t{2}, &result));
}

TEST_F(PortabilityTest, mulOverflow128NegativeOverflow) {
  int128_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<int128_t>::min(), int128_t{-1}, &result));
}

TEST_F(PortabilityTest, mulOverflow128WithZero) {
  int128_t result;
  EXPECT_FALSE(__builtin_mul_overflow(int128_t{0}, int128_t{12345}, &result));
  EXPECT_EQ(result, 0);
}

TEST_F(PortabilityTest, addOverflowUint128NoOverflow) {
  uint128_t result;
  EXPECT_FALSE(__builtin_add_overflow(uint128_t{50}, uint128_t{75}, &result));
  EXPECT_EQ(result, 125);
}

TEST_F(PortabilityTest, addOverflowUint128Overflow) {
  uint128_t result;
  EXPECT_TRUE(__builtin_add_overflow(
      std::numeric_limits<uint128_t>::max(), uint128_t{1}, &result));
}

TEST_F(PortabilityTest, subOverflowUint128Underflow) {
  uint128_t result;
  EXPECT_TRUE(__builtin_sub_overflow(uint128_t{0}, uint128_t{1}, &result));
}

TEST_F(PortabilityTest, mulOverflowUint128WithZero) {
  uint128_t result;
  EXPECT_FALSE(__builtin_mul_overflow(uint128_t{0}, uint128_t{12345}, &result));
  EXPECT_EQ(result, 0);
}

TEST_F(PortabilityTest, mulOverflowUint128Overflow) {
  uint128_t result;
  EXPECT_TRUE(__builtin_mul_overflow(
      std::numeric_limits<uint128_t>::max(), uint128_t{2}, &result));
}

// --- __builtin_bswap tests ---

TEST_F(PortabilityTest, bswap64) {
  EXPECT_EQ(
      __builtin_bswap64(0x0123456789ABCDEFULL), 0xEFCDAB8967452301ULL);
}

TEST_F(PortabilityTest, bswap64Zero) {
  EXPECT_EQ(__builtin_bswap64(0ULL), 0ULL);
}

TEST_F(PortabilityTest, bswap64RoundTrip) {
  uint64_t value = 0xDEADBEEFCAFEBABEULL;
  EXPECT_EQ(__builtin_bswap64(__builtin_bswap64(value)), value);
}

TEST_F(PortabilityTest, bswap32) {
  EXPECT_EQ(
      __builtin_bswap32(0x01020304UL), 0x04030201UL);
}

TEST_F(PortabilityTest, bswap32RoundTrip) {
  unsigned long value = 0xDEADBEEFUL;
  EXPECT_EQ(__builtin_bswap32(__builtin_bswap32(value)), value);
}

TEST_F(PortabilityTest, bswap16) {
  EXPECT_EQ(__builtin_bswap16(0x0102), 0x0201);
}

// --- BitUtil lowMask tests (verifies 64-bit shift correctness on Windows) ---

TEST_F(PortabilityTest, lowMask) {
  EXPECT_EQ(bits::lowMask(0), 0ULL);
  EXPECT_EQ(bits::lowMask(1), 1ULL);
  EXPECT_EQ(bits::lowMask(8), 0xFFULL);
  EXPECT_EQ(bits::lowMask(16), 0xFFFFULL);
  EXPECT_EQ(bits::lowMask(32), 0xFFFFFFFFULL);
  EXPECT_EQ(bits::lowMask(63), 0x7FFFFFFFFFFFFFFFULL);
}

// --- Counting zeros/trailing/leading tests ---

TEST_F(PortabilityTest, countTrailingZeros) {
  EXPECT_EQ(count_trailing_zeros(uint64_t{1}), 0);
  EXPECT_EQ(count_trailing_zeros(uint64_t{2}), 1);
  EXPECT_EQ(count_trailing_zeros(uint64_t{4}), 2);
  EXPECT_EQ(count_trailing_zeros(uint64_t{0x8000000000000000ULL}), 63);
  EXPECT_EQ(count_trailing_zeros(uint64_t{0}), 64);
}

TEST_F(PortabilityTest, countLeadingZeros) {
  EXPECT_EQ(count_leading_zeros(uint64_t{1}), 63);
  EXPECT_EQ(count_leading_zeros(uint64_t{0x8000000000000000ULL}), 0);
  EXPECT_EQ(count_leading_zeros(uint64_t{0}), 64);
}

// --- 128-bit byte-swap test ---

TEST_F(PortabilityTest, bswap128RoundTrip) {
  auto value = HugeInt::build(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);
  auto swapped = bits::builtin_bswap128(value);
  auto restored = bits::builtin_bswap128(swapped);
  EXPECT_EQ(restored, value);
}

} // namespace facebook::velox::test
