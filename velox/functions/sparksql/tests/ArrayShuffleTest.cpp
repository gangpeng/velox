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

#include "velox/expression/VectorReaders.h"
#include "velox/functions/sparksql/tests/SparkFunctionBaseTest.h"

namespace facebook::velox::functions::sparksql::test {
namespace {

using namespace facebook::velox::test;

class ArrayShuffleTest : public SparkFunctionBaseTest {
 protected:
  void testShuffle(
      const VectorPtr& input,
      const VectorPtr& expected,
      int64_t seed,
      int32_t partitionId = 0) {
    setSparkPartitionId(partitionId);
    assertEqualVectors(
        evaluate(fmt::format("shuffle(c0, {})", seed), makeRowVector({input})),
        expected);
  }

  template <typename T>
  void testShuffle(
      const VectorPtr& input,
      const std::vector<std::string>& expected,
      int64_t seed,
      int32_t partitionId = 0) {
    testShuffle(input, makeArrayVectorFromJson<T>(expected), seed, partitionId);
  }

  template <typename T>
  void testShuffle(
      const std::string& input,
      const std::string& expected,
      int64_t seed,
      int32_t partitionId = 0) {
    testShuffle<T>(
        makeArrayVectorFromJson<T>({input}), {expected}, seed, partitionId);
  }

  /// Evaluate shuffle and verify the result is a permutation of the input
  /// (same elements, same size) without checking exact order, since
  /// std::shuffle produces different results across standard library
  /// implementations.
  void testShuffleIsPermutation(
      const VectorPtr& input,
      int64_t seed,
      int32_t partitionId = 0) {
    setSparkPartitionId(partitionId);
    auto result = evaluate(
        fmt::format("shuffle(c0, {})", seed), makeRowVector({input}));
    ASSERT_EQ(result->size(), input->size());

    auto inputArray = input->wrappedVector()->as<ArrayVector>();
    auto resultArray = result->wrappedVector()->as<ArrayVector>();

    for (vector_size_t row = 0; row < input->size(); ++row) {
      if (input->isNullAt(row)) {
        ASSERT_TRUE(result->isNullAt(row)) << "at row " << row;
        continue;
      }
      // Unwrap to get the actual row index in the base vector.
      auto inputRow = input->wrappedIndex(row);
      auto resultRow = result->wrappedIndex(row);

      auto inputSize = inputArray->sizeAt(inputRow);
      auto resultSize = resultArray->sizeAt(resultRow);
      ASSERT_EQ(inputSize, resultSize) << "at row " << row;

      auto inputOffset = inputArray->offsetAt(inputRow);
      auto resultOffset = resultArray->offsetAt(resultRow);

      auto inputElements = inputArray->elements();
      auto resultElements = resultArray->elements();

      // Collect sorted string representations to compare as multisets.
      // Use DecodedVector to get flat values regardless of encoding.
      std::vector<std::string> inputVals;
      std::vector<std::string> resultVals;
      for (vector_size_t j = 0; j < inputSize; ++j) {
        auto ii = inputOffset + j;
        auto ri = resultOffset + j;
        inputVals.push_back(
            inputElements->isNullAt(ii)
                ? "NULL"
                : inputElements->wrappedVector()->toString(
                      inputElements->wrappedIndex(ii)));
        resultVals.push_back(
            resultElements->isNullAt(ri)
                ? "NULL"
                : resultElements->wrappedVector()->toString(
                      resultElements->wrappedIndex(ri)));
      }
      std::sort(inputVals.begin(), inputVals.end());
      std::sort(resultVals.begin(), resultVals.end());
      ASSERT_EQ(inputVals, resultVals) << "at row " << row;
    }
  }

  /// Verify that two shuffle calls with different seeds or partition IDs
  /// produce different results for the given input.
  void testShuffleDiffers(
      const VectorPtr& input,
      int64_t seed1,
      int32_t partitionId1,
      int64_t seed2,
      int32_t partitionId2) {
    setSparkPartitionId(partitionId1);
    auto result1 = evaluate(
        fmt::format("shuffle(c0, {})", seed1), makeRowVector({input}));
    setSparkPartitionId(partitionId2);
    auto result2 = evaluate(
        fmt::format("shuffle(c0, {})", seed2), makeRowVector({input}));

    // At least one row should differ between the two results.
    bool anyDifference = false;
    for (vector_size_t row = 0; row < result1->size(); ++row) {
      if (!result1->equalValueAt(result2.get(), row, row)) {
        anyDifference = true;
        break;
      }
    }
    EXPECT_TRUE(anyDifference)
        << "Expected different results with different seed/partitionId";
  }
};

TEST_F(ArrayShuffleTest, basic) {
  auto input = makeArrayVectorFromJson<int64_t>({"[1, 2, 3, 4, 5]"});
  testShuffleIsPermutation(input, 0);

  auto stringInput =
      makeArrayVectorFromJson<std::string>({R"(["a", "b", "c", "d"])"});
  testShuffleIsPermutation(stringInput, 0);

  // Assert results are different with different seeds / partition ids.
  testShuffleDiffers(input, 0, 0, 0, 1);
  testShuffleDiffers(input, 0, 0, 2, 0);
}

TEST_F(ArrayShuffleTest, nestedArrays) {
  auto input = makeNestedArrayVectorFromJson<int64_t>(
      {"[[1, 2, 3, 4], [5, 6]]",
       "[null, null, [1, 2, 3, 4], [5, 6], [6, 7, 8]]",
       "[[]]",
       "[[null]]"});
  testShuffleIsPermutation(input, 0);
}

TEST_F(ArrayShuffleTest, constantEncoding) {
  vector_size_t size = 3;
  // Test empty array, array with null element,
  // array with duplicate elements, and array with distinct values.
  auto valueVector = makeArrayVectorFromJson<int64_t>(
      {"[]", "[null, 0]", "[5, 5]", "[1, 2, 3]"});
  for (auto i = 0; i < valueVector->size(); i++) {
    auto input = BaseVector::wrapInConstant(size, i, valueVector);
    testShuffleIsPermutation(input, 0);
  }
}

TEST_F(ArrayShuffleTest, dictEncoding) {
  // Test dict with repeated elements: {1,2,3} x 3, {4,5} x 2.
  auto base = makeArrayVectorFromJson<int64_t>(
      {"[0]",
       "[1, 2 ,3]",
       "[4, 5, null]",
       "[1, 2, 3]",
       "[1, 2, 3]",
       "[4, 5, null]"});
  // Test repeated index elements and indices filtering (filter out element at
  // index 0).
  auto indices = makeIndices({3, 3, 4, 2, 2, 1, 1, 1});
  auto input = wrapInDictionary(indices, base);
  testShuffleIsPermutation(input, 0);
}

} // namespace
} // namespace facebook::velox::functions::sparksql::test
