#include "illuminate/memory/memory_allocation.h"
#include "illuminate/util/hash_map.h"
#include "doctest/doctest.h"
TEST_CASE("hash map") {
  using namespace illuminate;
  const uint32_t buffer_size = 128;
  std::byte buffer[buffer_size]{};
  LinearAllocator allocator(buffer, buffer_size);
  const uint32_t table_size = 10;
  struct TestStruct {
    uint32_t a{0}, b{0};
  };
  HashMap<TestStruct, LinearAllocator> map(&allocator, table_size);
  auto key = CalcStrHash("key0");
  auto key2 = CalcStrHash("key2");
  CHECK_UNARY(map.Insert(key, TestStruct{3, 5}));
  CHECK_UNARY(map.Insert(key2, TestStruct{8, 1}));
  CHECK_EQ(map.Get(key)->a, 3);
  CHECK_EQ(map.Get(key)->b, 5);
  CHECK_EQ(map.Get(key2)->a, 8);
  CHECK_EQ(map.Get(key2)->b, 1);
  allocator.Reset();
  HashMap<void*, LinearAllocator> map2(&allocator, table_size);
  auto ptr = Allocate<uint32_t>(&allocator);
  *ptr = 10;
  auto ptr2 = Allocate<uint32_t>(&allocator);
  *ptr2 = 101;
  CHECK_EQ(*ptr, 10);
  CHECK_EQ(*ptr2, 101);
  CHECK_UNARY(map2.Insert(key, ptr));
  CHECK_UNARY(map2.Insert(key2, ptr2));
  CHECK_EQ(*static_cast<uint32_t*>(*map2.Get(key)), 10);
  CHECK_EQ(*static_cast<uint32_t*>(*map2.Get(key2)), 101);
}
