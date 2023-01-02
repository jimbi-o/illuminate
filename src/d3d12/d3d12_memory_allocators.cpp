#include "d3d12_memory_allocators.h"
namespace illuminate {
namespace {
static const uint32_t system_memory_buffer_size_in_bytes = 32 * 1024 * 1024;
static std::byte system_memory_buffer[system_memory_buffer_size_in_bytes];
static const uint32_t scene_frame_buffer_size_in_bytes = 32 * 1024 * 1024;
static std::byte scene_frame_memory_buffer[scene_frame_buffer_size_in_bytes];
static LinearAllocator system_memory_allocator(system_memory_buffer, system_memory_buffer_size_in_bytes);
static DoubleEndedLinearAllocator scene_frame_memory_allocator(scene_frame_memory_buffer, scene_frame_buffer_size_in_bytes);
static const uint32_t render_graph_buffer_size_in_bytes = 32 * 1024 * 1024;
static std::byte render_graph_buffer[render_graph_buffer_size_in_bytes];
static LinearAllocator render_graph_allocator(render_graph_buffer, render_graph_buffer_size_in_bytes);
}
void ResetAllocation(const MemoryType type) {
  switch (type) {
    case MemoryType::kSystem: {
      system_memory_allocator.Reset();
      break;
    }
    case MemoryType::kScene: {
      scene_frame_memory_allocator.ResetLower();
      break;
    }
    case MemoryType::kFrame: {
      scene_frame_memory_allocator.ResetHigher();
      break;
    }
  }
}
void ClearAllAllocations() {
  ResetAllocation(MemoryType::kSystem);
  ResetAllocation(MemoryType::kScene);
  ResetAllocation(MemoryType::kFrame);
}
void* AllocateSystem(const size_t bytes, const size_t alignment_in_bytes) {
  auto addr = system_memory_allocator.Allocate(bytes, alignment_in_bytes);
  assert(addr != nullptr);
  return addr;
}
void* AllocateScene(const size_t bytes, const size_t alignment_in_bytes) {
  auto addr = scene_frame_memory_allocator.AllocateLower(bytes, alignment_in_bytes);
  assert(addr != nullptr);
  return addr;
}
void* AllocateFrame(const size_t bytes, const size_t alignment_in_bytes) {
  auto addr = scene_frame_memory_allocator.AllocateHigher(bytes, alignment_in_bytes);
  assert(addr != nullptr);
  return addr;
}
void* AllocateRenderGraph([[maybe_unused]] void* context, const size_t bytes, const size_t alignment_in_bytes) {
  auto addr = system_memory_allocator.Allocate(bytes, alignment_in_bytes);
  assert(addr != nullptr);
  return addr;
}
void FreeRenderGraph([[maybe_unused]] void* context, [[maybe_unused]] void* ptr) {
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("d3d12 memory allocation") { // NOLINT
  using namespace illuminate;
  auto i = AllocateSystem<uint32_t>();
  *i = 32;
  CHECK_EQ(*i, 32);
  auto j = AllocateSystem<uint32_t>();
  *j = 64;
  CHECK_EQ(*i, 32);
  CHECK_EQ(*j, 64);
  auto k = AllocateScene<uint32_t>();
  *k = 3;
  CHECK_EQ(*i, 32);
  CHECK_EQ(*j, 64);
  CHECK_EQ(*k, 3);
  auto l = AllocateFrame<uint32_t>();
  *l = ~0U;
  CHECK_EQ(*i, 32);
  CHECK_EQ(*j, 64);
  CHECK_EQ(*k, 3);
  CHECK_EQ(*l, ~0U);
  ResetAllocation(MemoryType::kSystem);
  auto m = AllocateSystem<uint32_t>();
  *m = 24;
  CHECK_EQ(*i, 24);
  CHECK_EQ(*j, 64);
  CHECK_EQ(*k, 3);
  CHECK_EQ(*l, ~0U);
  CHECK_EQ(*m, 24);
  ResetAllocation(MemoryType::kScene);
  auto n = AllocateScene<uint32_t>();
  *n = 5;
  CHECK_EQ(*i, 24);
  CHECK_EQ(*j, 64);
  CHECK_EQ(*k, 5);
  CHECK_EQ(*l, ~0U);
  CHECK_EQ(*m, 24);
  CHECK_EQ(*n, 5);
  ResetAllocation(MemoryType::kFrame);
  auto o = AllocateFrame<uint32_t>();
  *o = 10;
  CHECK_EQ(*i, 24);
  CHECK_EQ(*j, 64);
  CHECK_EQ(*k, 5);
  CHECK_EQ(*l, 10);
  CHECK_EQ(*m, 24);
  CHECK_EQ(*n, 5);
  CHECK_EQ(*o, 10);
}
