#include "d3d12_memory_allocators.h"
namespace illuminate {
namespace {
static const uint32_t system_memory_buffer_size_in_bytes = 128;
static std::byte system_memory_buffer[system_memory_buffer_size_in_bytes];
static const uint32_t temporal_memory_buffer_size_in_bytes = 512;
static std::byte temporal_memory_buffer[temporal_memory_buffer_size_in_bytes];
} // anonymous namespace
LinearAllocator gSystemMemoryAllocator(system_memory_buffer, system_memory_buffer_size_in_bytes);
LinearAllocator GetTemporalMemoryAllocator() {
  return LinearAllocator(temporal_memory_buffer, temporal_memory_buffer_size_in_bytes);
}
}
#include "doctest/doctest.h"
TEST_CASE("d3d12 memory allocation") { // NOLINT
  using namespace illuminate;
  auto sys_val = Allocate<uint32_t>(gSystemMemoryAllocator);
  *sys_val = 100;
  CHECK_EQ(*sys_val, 100);
  {
    auto allocator = GetTemporalMemoryAllocator();
    const uint32_t len = 10;
    auto tmparr = AllocateArray<uint32_t>(allocator, len);
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
    auto tmpval = Allocate<uint32_t>(allocator);
    *tmpval = 0xFFFF;
    CHECK_EQ(*tmpval, 0xFFFF);
    auto tmpval2 = Allocate<uint32_t>(allocator);
    *tmpval2 = 101;
    CHECK_EQ(*tmpval, 0xFFFF);
    CHECK_EQ(*tmpval2, 101);
  }
  CHECK_EQ(*sys_val, 100);
}
