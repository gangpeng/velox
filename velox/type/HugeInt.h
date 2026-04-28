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

#include <folly/dynamic.h>
#include <sstream>
#include <string>
#include "velox/common/base/BitUtil.h"
#include "velox/common/base/Exceptions.h"
#include "velox/type/StringView.h"

// On MSVC, __int128_t/__uint128_t and absl types are provided by Portability.h
// (included transitively via BitUtil.h).
#ifdef _MSC_VER
#include "absl/numeric/int128.h"
#endif

namespace facebook::velox {

using int128_t = __int128_t;
using uint128_t = __uint128_t;

class HugeInt {
 public:
  static FOLLY_ALWAYS_INLINE int128_t
  build(uint64_t hi, uint64_t lo) {
    // GCC does not allow left shift negative value.
#ifdef _MSC_VER
    return int128_t(absl::MakeUint128(hi, lo));
#else
    return (static_cast<__uint128_t>(hi) << 64) | lo;
#endif
  }

  static FOLLY_ALWAYS_INLINE uint64_t lower(int128_t value) {
    return static_cast<uint64_t>(value);
  }

  static FOLLY_ALWAYS_INLINE uint64_t upper(int128_t value) {
    return static_cast<uint64_t>(value >> 64);
  }

  static FOLLY_ALWAYS_INLINE int128_t deserialize(const char* serializedData) {
    int128_t value;
    memcpy(&value, serializedData, sizeof(int128_t));
    return value;
  }

  static FOLLY_ALWAYS_INLINE void serialize(
      const int128_t& value,
      char* serializedData) {
    memcpy(serializedData, &value, sizeof(int128_t));
  }

  static int128_t parse(const std::string& str);
};

} // namespace facebook::velox

namespace std {
string to_string(__int128_t x);
} // namespace std

// On MSVC, absl::int128 / absl::uint128 are not standard types, so {fmt}
// has no built-in formatter for them. Provide specializations here so that
// any header that includes HugeInt.h (directly or via velox/type/Type.h)
// can format int128_t values via VELOX_CHECK / fmt::format.
#ifdef _MSC_VER
#include <functional>
#include <fmt/ostream.h>
#include <folly/Conv.h>
#include <folly/hash/Hash.h>

// std::hash specializations so that std::unordered_set, folly::F14FastSet, etc.
// can use absl::int128 / absl::uint128 as keys.
namespace std {
template <>
struct hash<absl::int128> {
  size_t operator()(absl::int128 v) const noexcept {
    uint64_t hi = static_cast<uint64_t>(static_cast<absl::uint128>(v) >> 64);
    uint64_t lo = static_cast<uint64_t>(v);
    return static_cast<size_t>(folly::hash::hash_128_to_64(hi, lo));
  }
};
template <>
struct hash<absl::uint128> {
  size_t operator()(absl::uint128 v) const noexcept {
    uint64_t hi = static_cast<uint64_t>(v >> 64);
    uint64_t lo = static_cast<uint64_t>(v);
    return static_cast<size_t>(folly::hash::hash_128_to_64(hi, lo));
  }
};
} // namespace std

// folly::hasher specializations for folly::Hash / folly::F14 containers.
namespace folly {
template <>
struct hasher<absl::int128, void> {
  using folly_is_avalanching = std::true_type;
  size_t operator()(absl::int128 v) const noexcept {
    return std::hash<absl::int128>{}(v);
  }
};
template <>
struct hasher<absl::uint128, void> {
  using folly_is_avalanching = std::true_type;
  size_t operator()(absl::uint128 v) const noexcept {
    return std::hash<absl::uint128>{}(v);
  }
};

// folly::toAppend overloads so that folly::to<std::string>(absl::int128) works.
template <class Tgt>
std::enable_if_t<IsSomeString<Tgt>::value> toAppend(
    absl::int128 value,
    Tgt* result) {
  std::ostringstream oss;
  oss << value;
  result->append(oss.str());
}
template <class Tgt>
std::enable_if_t<IsSomeString<Tgt>::value> toAppend(
    absl::uint128 value,
    Tgt* result) {
  std::ostringstream oss;
  oss << value;
  result->append(oss.str());
}
} // namespace folly

template <typename Char>
struct fmt::formatter<absl::int128, Char>
    : fmt::formatter<std::string, Char> {
  template <typename FormatContext>
  auto format(absl::int128 v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    std::ostringstream oss;
    oss << v;
    return fmt::formatter<std::string, Char>::format(oss.str(), ctx);
  }
};
template <typename Char>
struct fmt::formatter<absl::uint128, Char>
    : fmt::formatter<std::string, Char> {
  template <typename FormatContext>
  auto format(absl::uint128 v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    std::ostringstream oss;
    oss << v;
    return fmt::formatter<std::string, Char>::format(oss.str(), ctx);
  }
};
#endif // _MSC_VER
