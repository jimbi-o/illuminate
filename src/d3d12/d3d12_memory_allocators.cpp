#include "d3d12_memory_allocators.h"
namespace illuminate {
namespace {
static const uint32_t system_memory_buffer_size_in_bytes = 1024 * 1024;
static std::byte system_memory_buffer[system_memory_buffer_size_in_bytes];
static const uint32_t temporal_memory_buffer_size_in_bytes = 1024 * 1024;
static std::byte temporal_memory_buffer[temporal_memory_buffer_size_in_bytes];
static LinearAllocator system_memory_allocator(system_memory_buffer, system_memory_buffer_size_in_bytes);
static StackAllocator temporal_memory_allocator(temporal_memory_buffer, temporal_memory_buffer_size_in_bytes);
} // anonymous namespace
LinearAllocator* gSystemMemoryAllocator = &system_memory_allocator;
MemoryAllocationJanitor GetTemporalMemoryAllocator() {
  return MemoryAllocationJanitor(&temporal_memory_allocator);
}
uint32_t GetTemporalMemoryOffset() {
  return static_cast<uint32_t>(temporal_memory_allocator.GetOffset());
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("d3d12 memory allocation") { // NOLINT
  using namespace illuminate;
  auto sys_val = Allocate<uint32_t>(gSystemMemoryAllocator);
  *sys_val = 100;
  CHECK_EQ(*sys_val, 100);
  {
    auto allocator = GetTemporalMemoryAllocator();
    const uint32_t len = 10;
    auto tmparr = AllocateArray<uint32_t>(&allocator, len);
    for (uint32_t i = 0; i < len; i++) {
      tmparr[i] = i * 100;
    }
    for (uint32_t i = 0; i < len; i++) {
      CAPTURE(i);
      CHECK_EQ(tmparr[i], i * 100);
    }
  }
  {
    auto allocator = GetTemporalMemoryAllocator();
    auto tmpval = Allocate<uint32_t>(&allocator);
    *tmpval = 0xFFFF;
    CHECK_EQ(*tmpval, 0xFFFF);
    auto tmpval2 = Allocate<uint32_t>(&allocator);
    *tmpval2 = 101;
    CHECK_EQ(*tmpval, 0xFFFF);
    CHECK_EQ(*tmpval2, 101);
  }
  CHECK_EQ(*sys_val, 100);
}
TEST_CASE("MemoryAllocationJanitor") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 1024;
  std::byte buffer[size_in_byte]{};
  StackAllocator allocator(buffer, size_in_byte);
  MemoryAllocationJanitor j0(&allocator);
  CHECK_EQ(j0.GetMarker(), allocator.GetOffset());
  auto v0 = Allocate<uint32_t>(&j0);
  CHECK_GE(allocator.GetOffset(), j0.GetMarker() + sizeof(uint32_t));
  *v0 = 1;
  CHECK_EQ(*v0, 1);
  uintptr_t prev_marker{};
  {
    MemoryAllocationJanitor j1(&allocator);
    CHECK_EQ(j1.GetMarker(), allocator.GetOffset());
    auto v1 = Allocate<uint32_t>(&j1);
    CHECK_GE(allocator.GetOffset(), j1.GetMarker() + sizeof(uint32_t));
    *v1 = 2;
    CHECK_EQ(*v0, 1);
    CHECK_EQ(*v1, 2);
    {
      MemoryAllocationJanitor j2(&allocator);
      CHECK_EQ(j2.GetMarker(), allocator.GetOffset());
      auto v2 = Allocate<uint32_t>(&j2);
      CHECK_GE(allocator.GetOffset(), j2.GetMarker() + sizeof(uint32_t));
      *v2 = 3;
      CHECK_EQ(*v0, 1);
      CHECK_EQ(*v1, 2);
      CHECK_EQ(*v2, 3);
      prev_marker = j2.GetMarker();
    }
    CHECK_EQ(allocator.GetOffset(), prev_marker);
    CHECK_EQ(*v0, 1);
    CHECK_EQ(*v1, 2);
    prev_marker = j1.GetMarker();
  }
  CHECK_EQ(allocator.GetOffset(), prev_marker);
  CHECK_EQ(*v0, 1);
  {
    MemoryAllocationJanitor j3(&allocator);
    CHECK_EQ(j3.GetMarker(), allocator.GetOffset());
    const uint32_t len = 3;
    auto a = AllocateArray<uint16_t>(&j3, len);
    CHECK_GE(allocator.GetOffset(), j3.GetMarker() + sizeof(uint16_t) * len);
    a[0] = 4;
    a[1] = 5;
    a[2] = 6;
    CHECK_EQ(*v0, 1);
    CHECK_EQ(a[0], 4);
    CHECK_EQ(a[1], 5);
    CHECK_EQ(a[2], 6);
    {
      MemoryAllocationJanitor j4(&allocator);
      CHECK_EQ(j4.GetMarker(), allocator.GetOffset());
      auto v4 = Allocate<uint32_t>(&j4);
      CHECK_GE(allocator.GetOffset(), j4.GetMarker() + sizeof(uint32_t));
      *v4 = 7;
      CHECK_EQ(*v0, 1);
      CHECK_EQ(a[0], 4);
      CHECK_EQ(a[1], 5);
      CHECK_EQ(a[2], 6);
      CHECK_EQ(*v4, 7);
      prev_marker = j4.GetMarker();
    }
    CHECK_EQ(allocator.GetOffset(), prev_marker);
    CHECK_EQ(*v0, 1);
    CHECK_EQ(a[0], 4);
    CHECK_EQ(a[1], 5);
    CHECK_EQ(a[2], 6);
    {
      MemoryAllocationJanitor j5(&allocator);
      CHECK_EQ(j5.GetMarker(), allocator.GetOffset());
      auto v5 = Allocate<uint32_t>(&j5);
      CHECK_GE(allocator.GetOffset(), j5.GetMarker() + sizeof(uint32_t));
      *v5 = 8;
      CHECK_EQ(*v0, 1);
      CHECK_EQ(a[0], 4);
      CHECK_EQ(a[1], 5);
      CHECK_EQ(a[2], 6);
      CHECK_EQ(*v5, 8);
      auto v6 = Allocate<uint32_t>(&j5);
      CHECK_GE(allocator.GetOffset(), j5.GetMarker() + sizeof(uint32_t) * 2);
      *v6 = 9;
      CHECK_EQ(*v0, 1);
      CHECK_EQ(a[0], 4);
      CHECK_EQ(a[1], 5);
      CHECK_EQ(a[2], 6);
      CHECK_EQ(*v5, 8);
      CHECK_EQ(*v6, 9);
      prev_marker = j5.GetMarker();
    }
    CHECK_EQ(allocator.GetOffset(), prev_marker);
    CHECK_EQ(*v0, 1);
    CHECK_EQ(a[0], 4);
    CHECK_EQ(a[1], 5);
    CHECK_EQ(a[2], 6);
    prev_marker = j3.GetMarker();
  }
  CHECK_EQ(allocator.GetOffset(), prev_marker);
  CHECK_EQ(*v0, 1);
}
TEST_CASE("array allocation") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t size_in_byte = 4096;
  std::byte buffer[size_in_byte]{};
  StackAllocator allocator(buffer, size_in_byte);
  MemoryAllocationJanitor janitor(&allocator);
  auto arr1 = AllocateArray<uint64_t>(&janitor, 100);
  auto arr2 = AllocateArray<uint64_t*>(&janitor, 100);
  for (uint32_t i = 0; i < 100; i++) {
    arr1[i] = 100 + i;
    arr2[i] = Allocate<uint64_t>(&janitor);
    (*arr2[i]) = 200 + i;
  }
  for (uint32_t i = 0; i < 100; i++) {
    CAPTURE(i);
    CHECK_EQ(arr1[i], 100 + i);
    CHECK_EQ((*arr2[i]), 200 + i);
  }
}
