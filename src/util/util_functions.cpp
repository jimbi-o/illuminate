#include "illuminate/util/util_functions.h"
namespace illuminate {
bool SucceedMultipleIndex(const uint32_t array_size, const uint32_t* array_size_array, uint32_t* array) {
  for (uint32_t i = 0; i < array_size; i++) {
    auto index = array_size - i - 1;
    if (array[index] + 1 < array_size_array[index]) {
      array[index]++;
      return true;
    }
    array[index] = 0;
  }
  return false;
}
}
#include "doctest/doctest.h"
#include "illuminate/memory/memory_allocation.h"
TEST_CASE("succeed multiple index") {
  using namespace illuminate;
  const uint32_t buffer_size = 128;
  std::byte buffer[buffer_size]{};
  LinearAllocator allocator(buffer, buffer_size);
  const uint32_t array_size = 4;
  auto array_size_array = AllocateArray<uint32_t>(&allocator, array_size);
  array_size_array[0] = 2;
  array_size_array[1] = 4;
  array_size_array[2] = 1;
  array_size_array[3] = 3;
  auto array = AllocateArray<uint32_t>(&allocator, array_size);
  for (uint32_t i = 0; i < array_size; i++) {
    array[i] = 0;
  }
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY_FALSE(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
}
