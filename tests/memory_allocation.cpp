#include "illuminate/memory/memory_allocation.h"
#include "doctest/doctest.h"
TEST_CASE("LinearAllocator") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 1024;
  std::byte buffer[size_in_byte]{};
  LinearAllocator allocator(buffer, size_in_byte);
  auto a = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *a = 5;
  CHECK_EQ(a, reinterpret_cast<void*>(buffer));
  CHECK_EQ(*a, 5);
  auto b = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *b = 10;
  CHECK_GT(b, reinterpret_cast<void*>(buffer));
  CHECK_EQ(*a, 5);
  CHECK_EQ(*b, 10);
  allocator.Reset();
  auto c = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *c = 15;
  CHECK_NE(*a, 5);
  CHECK_EQ(c, reinterpret_cast<void*>(buffer));
  CHECK_EQ(*c, 15);
}
TEST_CASE("DoubleBufferedAllocator") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 1024;
  std::byte buffer[size_in_byte]{};
  DoubleBufferedAllocator allocator(buffer, &buffer[size_in_byte / 2], size_in_byte / 2);
  auto a = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *a = 5;
  CHECK_EQ(a, reinterpret_cast<void*>(buffer));
  CHECK_EQ(*a, 5);
  auto b = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *b = 10;
  CHECK_GT(b, reinterpret_cast<void*>(buffer));
  CHECK_EQ(*a, 5);
  CHECK_EQ(*b, 10);
  allocator.Reset();
  auto c = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *c = 15;
  CHECK_EQ(c, reinterpret_cast<void*>(&buffer[size_in_byte / 2]));
  CHECK_EQ(*a, 5);
  CHECK_EQ(*c, 15);
  allocator.Reset();
  auto d = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *d = 20;
  CHECK_EQ(d, reinterpret_cast<void*>(buffer));
  CHECK_NE(*a, 5);
  CHECK_EQ(*c, 15);
  CHECK_EQ(*d, 20);
  const uint32_t len = 5;
  auto e = new(allocator.Allocate(sizeof(uint32_t*) * len)) uint32_t[len];
  for (uint32_t i = 0; i < len; i++) {
    e[i] = i + 1;
  }
  auto f = new(allocator.Allocate(sizeof(uint32_t*))) uint32_t;
  *f = 99;
  for (uint32_t i = 0; i < len; i++) {
    CAPTURE(i);
    CHECK_EQ(e[i], i + 1);
  }
  CHECK_EQ(*f, 99);
}
TEST_CASE("Allocate") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 1024;
  std::byte buffer[size_in_byte]{};
  LinearAllocator allocator(buffer, size_in_byte);
  auto a = Allocate<uint32_t>(allocator);
  *a = 5;
  const uint32_t len = 10;
  auto arr = AllocateArray<uint32_t>(allocator, len);
  auto b = Allocate<uint32_t>(allocator);
  *b = 99;
  for (uint32_t i = 0; i < len; i++) {
    arr[i] = i + 1;
  }
  for (uint32_t i = 0; i < len; i++) {
    CAPTURE(i);
    CHECK_EQ(arr[i], i + 1);
  }
  CHECK_EQ(*a, 5);
  CHECK_EQ(*b, 99);
}
