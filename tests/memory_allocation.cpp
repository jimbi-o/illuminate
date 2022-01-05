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
  auto a = Allocate<uint32_t>(&allocator);
  *a = 5;
  const uint32_t len = 10;
  auto arr = AllocateArray<uint32_t>(&allocator, len);
  auto b = Allocate<uint32_t>(&allocator);
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
TEST_CASE("StackAllocator") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 1024;
  std::byte buffer[size_in_byte]{};
  StackAllocator allocator(buffer, size_in_byte);
  auto marker0 = allocator.GetOffset();
  CHECK_EQ(marker0, 0);
  auto v0 = Allocate<uint32_t>(&allocator);
  CHECK_GE(allocator.GetOffset(), marker0 + sizeof(uint32_t));
  *v0 = 0;
  CHECK_EQ(*v0, 0);
  allocator.ResetToMarker(marker0);
  auto marker1 = allocator.GetOffset();
  CHECK_EQ(marker0, marker1);
  auto v1 = Allocate<uint32_t>(&allocator);
  auto marker2 = allocator.GetOffset();
  CHECK_GE(marker2, marker1 + sizeof(uint32_t));
  *v1 = 1;
  CHECK_NE(*v0, 0);
  CHECK_EQ(*v1, 1);
  auto v2 = Allocate<uint32_t>(&allocator);
  auto marker3 = allocator.GetOffset();
  CHECK_GE(marker3, marker2 + sizeof(uint32_t));
  *v2 = 2;
  CHECK_EQ(*v1, 1);
  CHECK_EQ(*v2, 2);
  auto v3 = Allocate<uint32_t>(&allocator);
  auto marker4 = allocator.GetOffset();
  CHECK_GT(marker4, marker3 + sizeof(uint32_t));
  *v3 = 3;
  CHECK_EQ(*v1, 1);
  CHECK_EQ(*v2, 2);
  CHECK_EQ(*v3, 3);
  allocator.ResetToMarker(marker2);
  CHECK_EQ(allocator.GetOffset(), marker2);
  auto a = AllocateArray<uint32_t>(&allocator, 3);
  auto marker5 = allocator.GetOffset();
  CHECK_GE(marker5, marker2 + sizeof(uint32_t) * 3);
  a[0] = 4;
  a[1] = 5;
  a[2] = 6;
  CHECK_EQ(*v1, 1);
  CHECK_NE(*v2, 2);
  CHECK_NE(*v3, 3);
  CHECK_EQ(a[0], 4);
  CHECK_EQ(a[1], 5);
  CHECK_EQ(a[2], 6);
  auto v4 = Allocate<uint32_t>(&allocator);
  CHECK_GT(allocator.GetOffset(), marker5 + sizeof(uint32_t));
  *v4 = 7;
  CHECK_EQ(*v1, 1);
  CHECK_NE(*v2, 2);
  CHECK_NE(*v3, 3);
  CHECK_EQ(a[0], 4);
  CHECK_EQ(a[1], 5);
  CHECK_EQ(a[2], 6);
  CHECK_EQ(*v4, 7);
}
