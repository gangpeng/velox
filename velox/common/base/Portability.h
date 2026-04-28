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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// MSVC compiler built-in compatibility shims
// ---------------------------------------------------------------------------
#ifdef _MSC_VER
#include <cmath>
#include <immintrin.h>
#include <intrin.h>
#include <limits.h>
#include "absl/numeric/int128.h"
// Pull in folly's builtin shims for __builtin_clz, __builtin_ctz,
// __builtin_popcount, etc. which folly already implements for MSVC.
#include <folly/portability/Builtins.h>

// 128-bit integer types (GCC extension not available in MSVC).
using __int128_t = absl::int128;
using __uint128_t = absl::uint128;

// MSVC's <cmath> provides fpclassify / isnan / isinf / isfinite as overloaded
// functions for float, double, long double only.  When a template (e.g. in
// {fmt}) tries to SFINAE-test std::isfinite(T()) with T = absl::int128, MSVC
// emits a hard error (C2665) instead of a substitution failure.  Providing
// overloads for the 128-bit types makes overload resolution succeed and
// returns the correct (trivial) answer: integers are always finite.
inline int fpclassify(absl::int128) {
  return FP_NORMAL;
}
inline int fpclassify(absl::uint128) {
  return FP_NORMAL;
}
inline bool isnan(absl::int128) {
  return false;
}
inline bool isnan(absl::uint128) {
  return false;
}
inline bool isinf(absl::int128) {
  return false;
}
inline bool isinf(absl::uint128) {
  return false;
}
inline bool isfinite(absl::int128) {
  return true;
}
inline bool isfinite(absl::uint128) {
  return true;
}

// Also inject into namespace std so that std::fpclassify, std::isnan, etc.
// resolve for absl::int128 / absl::uint128.
namespace std {
inline int fpclassify(absl::int128) {
  return FP_NORMAL;
}
inline int fpclassify(absl::uint128) {
  return FP_NORMAL;
}
inline bool isnan(absl::int128) {
  return false;
}
inline bool isnan(absl::uint128) {
  return false;
}
inline bool isinf(absl::int128) {
  return false;
}
inline bool isinf(absl::uint128) {
  return false;
}
inline bool isfinite(absl::int128) {
  return true;
}
inline bool isfinite(absl::uint128) {
  return true;
}
} // namespace std

// __m128i_u — GCC's unaligned __m128i type alias (not needed on MSVC).
using __m128i_u = __m128i;
// __m256i_u — GCC's unaligned __m256i type alias.
using __m256i_u = __m256i;

// __builtin_bswap64 — byte-swap 64-bit (not in folly's Builtins.h)
inline unsigned long long __builtin_bswap64(unsigned long long x) {
  return _byteswap_uint64(x);
}

// __builtin_bswap32 — byte-swap 32-bit (not in folly's Builtins.h)
inline unsigned long __builtin_bswap32(unsigned long x) {
  return _byteswap_ulong(x);
}

// __builtin_bswap16 — byte-swap 16-bit (not in folly's Builtins.h)
inline unsigned short __builtin_bswap16(unsigned short x) {
  return _byteswap_ushort(x);
}

// __builtin_expect — branch prediction hint (no-op on MSVC)
#ifndef __builtin_expect
#define __builtin_expect(expr, expected) (expr)
#endif

// __builtin_add_overflow / __builtin_sub_overflow / __builtin_mul_overflow
// MSVC does not have these; implement via arithmetic + range check.
template <typename T>
inline bool __builtin_add_overflow(T a, T b, T* result) {
  using U = std::make_unsigned_t<T>;
  *result = static_cast<T>(static_cast<U>(a) + static_cast<U>(b));
  if constexpr (std::is_signed_v<T>) {
    return (b > 0 && a > std::numeric_limits<T>::max() - b) ||
        (b < 0 && a < std::numeric_limits<T>::min() - b);
  } else {
    return *result < a;
  }
}

template <typename T>
inline bool __builtin_sub_overflow(T a, T b, T* result) {
  using U = std::make_unsigned_t<T>;
  *result = static_cast<T>(static_cast<U>(a) - static_cast<U>(b));
  if constexpr (std::is_signed_v<T>) {
    return (b > 0 && a < std::numeric_limits<T>::min() + b) ||
        (b < 0 && a > std::numeric_limits<T>::max() + b);
  } else {
    return a < b;
  }
}

template <typename T>
inline bool __builtin_mul_overflow(T a, T b, T* result) {
  if constexpr (std::is_signed_v<T>) {
    if (a == 0 || b == 0) {
      *result = 0;
      return false;
    }
    // Use wider type if possible, otherwise check via division.
    if constexpr (sizeof(T) < 8) {
      using W = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
      W wide = static_cast<W>(a) * static_cast<W>(b);
      *result = static_cast<T>(wide);
      return wide > std::numeric_limits<T>::max() ||
          wide < std::numeric_limits<T>::min();
    } else {
      // 64-bit: use 128-bit intermediary
      __int128_t wide = static_cast<__int128_t>(a) * static_cast<__int128_t>(b);
      *result = static_cast<T>(wide);
      return wide > std::numeric_limits<T>::max() ||
          wide < std::numeric_limits<T>::min();
    }
  } else {
    if (a == 0 || b == 0) {
      *result = 0;
      return false;
    }
    if constexpr (sizeof(T) < 8) {
      uint64_t wide = static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
      *result = static_cast<T>(wide);
      return wide > std::numeric_limits<T>::max();
    } else {
      __uint128_t wide =
          static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
      *result = static_cast<T>(wide);
      return wide > std::numeric_limits<T>::max();
    }
  }
}

// Specializations for absl::int128 / absl::uint128:
// The generic templates above use std::make_unsigned_t<T> which fails for
// absl's 128-bit types because they are not standard integral types.

inline bool __builtin_add_overflow(
    absl::int128 a,
    absl::int128 b,
    absl::int128* result) {
  // Store the wrapped result using unsigned arithmetic, matching GCC/Clang
  // builtins while keeping the signed overflow check below well-defined.
  *result = absl::int128(
      static_cast<absl::uint128>(a) + static_cast<absl::uint128>(b));
  return ((b > 0 && a > absl::Int128Max() - b) ||
          (b < 0 && a < absl::Int128Min() - b));
}

inline bool __builtin_add_overflow(
    absl::uint128 a,
    absl::uint128 b,
    absl::uint128* result) {
  *result = a + b;
  return *result < a;
}

inline bool __builtin_sub_overflow(
    absl::int128 a,
    absl::int128 b,
    absl::int128* result) {
  // Store the wrapped result using unsigned arithmetic, matching GCC/Clang
  // builtins while keeping the signed overflow check below well-defined.
  *result = absl::int128(
      static_cast<absl::uint128>(a) - static_cast<absl::uint128>(b));
  return ((b > 0 && a < absl::Int128Min() + b) ||
          (b < 0 && a > absl::Int128Max() + b));
}

inline bool __builtin_sub_overflow(
    absl::uint128 a,
    absl::uint128 b,
    absl::uint128* result) {
  *result = a - b;
  return a < b;
}

inline bool __builtin_mul_overflow(
    absl::int128 a,
    absl::int128 b,
    absl::int128* result) {
  // Store the low 128 bits even on overflow. Some callers inspect the result
  // to match compiler builtin behavior, so avoid a saturating or zero result.
  *result = absl::int128(
      static_cast<absl::uint128>(a) * static_cast<absl::uint128>(b));

  if (a == 0 || b == 0) {
    return false;
  }

  if (a > 0) {
    return b > 0 ? a > absl::Int128Max() / b
                 : b < absl::Int128Min() / a;
  }
  return b > 0 ? a < absl::Int128Min() / b
               : b < absl::Int128Max() / a;
}

inline bool __builtin_mul_overflow(
    absl::uint128 a,
    absl::uint128 b,
    absl::uint128* result) {
  if (a == 0 || b == 0) {
    *result = 0;
    return false;
  }
  *result = a * b;
  return (b != 0 && *result / b != a);
}

#endif // _MSC_VER

inline size_t count_trailing_zeros(uint64_t x) {
  return x == 0 ? 64 : __builtin_ctzll(x);
}

inline size_t count_trailing_zeros_32bits(uint32_t x) {
  return x == 0 ? 32 : __builtin_ctz(x);
}

inline size_t count_leading_zeros(uint64_t x) {
  return x == 0 ? 64 : __builtin_clzll(x);
}

inline size_t count_leading_zeros_32bits(uint32_t x) {
  return x == 0 ? 32 : __builtin_clz(x);
}

namespace facebook::velox {

#if defined(__GNUC__) || defined(__clang__)
#define INLINE_LAMBDA __attribute__((__always_inline__))
#else
#define INLINE_LAMBDA
#endif

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN_BUILD 1
#endif
#endif

/// Define tsan_atomic<T> to be std::atomic<T?> for tsan builds and
/// T otherwise. This allows declaring variables like statistics
/// counters that do not have to be exact nor have synchronized
/// semantics. This deals with san errors while not incurring the
/// bus lock overhead at run time in regular builds.
#ifdef TSAN_BUILD
template <typename T>
using tsan_atomic = std::atomic<T>;

template <typename T>
inline T tsanAtomicValue(const std::atomic<T>& x) {
  return x;
}

/// Lock guard in tsan build and no-op otherwise.
template <typename T>
using tsan_lock_guard = std::lock_guard<T>;

#else

template <typename T>
using tsan_atomic = T;

template <typename T>
inline T tsanAtomicValue(T x) {
  return x;
}
template <typename T>
struct TsanEmptyLockGuard {
  TsanEmptyLockGuard(T& /*ignore*/) {}
};

template <typename T>
using tsan_lock_guard = TsanEmptyLockGuard<T>;

#endif

template <typename T>
inline void resizeTsanAtomic(
    std::vector<tsan_atomic<T>>& vector,
    int32_t newSize) {
  std::vector<tsan_atomic<T>> newVector(newSize);
  auto numCopy = std::min<int32_t>(newSize, vector.size());
  for (auto i = 0; i < numCopy; ++i) {
    newVector[i] = tsanAtomicValue(vector[i]);
  }
  vector = std::move(newVector);
}
} // namespace facebook::velox
