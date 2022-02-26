#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
void* D3d12BufferAllocatorAllocCallback(size_t Size, size_t Alignment, [[maybe_unused]] void* pUserData) {
  return gSystemMemoryAllocator->Allocate(Size, Alignment);
}
void D3d12BufferAllocatorFreeCallback([[maybe_unused]] void* pMemory, [[maybe_unused]] void* pUserData) {}
} // anonymous namespace
D3D12MA::Allocator* GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device) {
  using namespace D3D12MA;
  ALLOCATION_CALLBACKS allocation_callbacks{
    .pAllocate = D3d12BufferAllocatorAllocCallback,
    .pFree = D3d12BufferAllocatorFreeCallback,
    .pUserData = nullptr,
  };
  ALLOCATOR_DESC desc{
    .Flags = ALLOCATOR_FLAG_NONE,
    .pDevice = device,
    .PreferredBlockSize = 0, // will use default (64MiB)
    .pAllocationCallbacks = &allocation_callbacks,
    .pAdapter = adapter,
  };
  Allocator* allocator{nullptr};
  auto hr = CreateAllocator(&desc, &allocator);
  if (FAILED(hr)) {
    logerror("CreateAllocator failed. {}", hr);
  }
  return allocator;
}
D3D12_RESOURCE_DESC1 ConvertToD3d12ResourceDesc1(const BufferConfig& config, const MainBufferSize& main_buffer_size) {
  return {
    .Dimension = config.dimension,
    .Alignment = config.alignment,
    .Width = GetPhysicalWidth(main_buffer_size, config.size_type, config.width),
    .Height = GetPhysicalHeight(main_buffer_size, config.size_type, config.height),
    .DepthOrArraySize = config.depth_or_array_size,
    .MipLevels = config.miplevels,
    .Format = config.format,
    .SampleDesc = {
      .Count = config.sample_count,
      .Quality = config.sample_quality,
    },
    .Layout = config.layout,
    .Flags = config.flags,
    .SamplerFeedbackMipRegion = {
      .Width = config.mip_width,
      .Height = config.mip_height,
      .Depth = config.mip_depth,
    },
  };
}
namespace {
void CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator, D3D12MA::Allocation** allocation, ID3D12Resource** resource) {
  using namespace D3D12MA;
  ALLOCATION_DESC allocation_desc{
    .Flags = ALLOCATION_FLAG_NONE,
    .HeapType = heap_type,
    .ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
    .CustomPool = nullptr,
  };
  auto hr = allocator->CreateResource2(&allocation_desc, &resource_desc, initial_state, clear_value, nullptr, allocation, IID_PPV_ARGS(resource));
  if (FAILED(hr)) {
    logerror("failed to create resource. {}", hr);
    assert(false && "failed to create resource");
    if (*allocation) {
      (*allocation)->Release();
      (*allocation) = nullptr;
    }
    if (*resource) {
      (*resource)->Release();
      (*resource) = nullptr;
    }
  }
}
} // namespace anonymous
BufferAllocation CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator) {
  BufferAllocation buffer_allocation{};
  CreateBuffer(heap_type, initial_state, resource_desc, clear_value, allocator, &buffer_allocation.allocation, &buffer_allocation.resource);
  return buffer_allocation;
}
void CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator, D3D12MA::Allocation** allocation, ID3D12Resource** resource) {
  auto resource_desc = ConvertToD3d12ResourceDesc1(config, main_buffer_size);
  auto clear_value = (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0) ? &config.clear_value : nullptr;
  CreateBuffer(config.heap_type, ConvertToD3d12ResourceState(config.initial_state), resource_desc, clear_value, allocator, allocation, resource);
}
BufferAllocation CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator) {
  BufferAllocation buffer_allocation{};
  CreateBuffer(config, main_buffer_size, allocator, &buffer_allocation.allocation, &buffer_allocation.resource);
  return buffer_allocation;
}
void ReleaseBufferAllocation(BufferAllocation* b) {
  if (b->allocation) {
    b->allocation->Release();
  }
  if (b->resource) {
    b->resource->Release();
  }
}
void* MapResource(ID3D12Resource* resource, const uint32_t size, const uint32_t read_begin, const uint32_t read_end) {
  D3D12_RANGE read_range{
    .Begin = read_begin,
    .End = read_end,
  };
  void* ptr{nullptr};
  auto hr = resource->Map(0, &read_range, &ptr);
  if (FAILED(hr)) {
    logerror("MapResource failed. {} {} {} {}", hr, size, read_begin, read_end);
    return nullptr;
  }
  return ptr;
}
void UnmapResource(ID3D12Resource* resource) {
  resource->Unmap(0, nullptr);
}
void ReleaseBuffers(BufferList* buffer_list) {
  for (uint32_t i = 0; i < buffer_list->buffer_allocation_num; i++) {
    if (buffer_list->buffer_allocation_list[i]) {
      buffer_list->buffer_allocation_list[i]->Release();
    }
    if (buffer_list->resource_list[i]) {
      buffer_list->resource_list[i]->Release();
    }
  }
}
void ConfigurePingPongBufferWriteToSubList(const uint32_t render_pass_num, const RenderPass* render_pass_list, const bool* render_pass_enable_flag, const uint32_t buffer_num, bool** pingpong_buffer_write_to_sub_list) {
  for (uint32_t buffer_index = 0; buffer_index < buffer_num; buffer_index++) {
    for (uint32_t i = 0; i < render_pass_num; i++) {
      pingpong_buffer_write_to_sub_list[buffer_index][i] = false;
    }
  }
  for (uint32_t i = 0; i < render_pass_num - 1; i++) {
    if (!render_pass_enable_flag[i]) { continue; }
    for (uint32_t j = 0; j < render_pass_list[i].flip_pingpong_num; j++) {
      const auto buffer_index = render_pass_list[i].flip_pingpong_index_list[j];
      const bool write_to_sub = !pingpong_buffer_write_to_sub_list[buffer_index][i];
      pingpong_buffer_write_to_sub_list[buffer_index][i + 1] = write_to_sub;
      for (uint32_t k = i + 2; k < render_pass_num; k++) {
        pingpong_buffer_write_to_sub_list[buffer_index][k] = write_to_sub;
      }
    }
  }
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("barrier configuration") { // NOLINT
  using namespace illuminate;
  const uint32_t buffer_num = 5;
  uint32_t flip_pingpong_index_list0[] = {0,1,};
  uint32_t flip_pingpong_index_list1[] = {0,1,2,3,};
  uint32_t flip_pingpong_index_list2[] = {0,2,};
  uint32_t flip_pingpong_index_list3[] = {0,};
  RenderPass render_pass_list[] = {{},{},{},{},};
  bool render_pass_enable_flag[] = {true,true,true,true,};
  static_assert(countof(render_pass_list) == countof(render_pass_enable_flag));
  const auto render_pass_num = countof(render_pass_list);
  render_pass_list[0].flip_pingpong_num = countof(flip_pingpong_index_list0);
  render_pass_list[0].flip_pingpong_index_list = flip_pingpong_index_list0;
  render_pass_list[1].flip_pingpong_num = countof(flip_pingpong_index_list1);
  render_pass_list[1].flip_pingpong_index_list = flip_pingpong_index_list1;
  render_pass_list[2].flip_pingpong_num = countof(flip_pingpong_index_list2);
  render_pass_list[2].flip_pingpong_index_list = flip_pingpong_index_list2;
  render_pass_list[3].flip_pingpong_num = countof(flip_pingpong_index_list3); // last flip will be ignored.
  render_pass_list[3].flip_pingpong_index_list = flip_pingpong_index_list3; // last flip will be ignored.
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto pingpong_buffer_write_to_sub_list = AllocateArray<bool*>(&tmp_allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    pingpong_buffer_write_to_sub_list[i] = AllocateArray<bool>(&tmp_allocator, render_pass_num);
  }
  ConfigurePingPongBufferWriteToSubList(render_pass_num, render_pass_list, render_pass_enable_flag, buffer_num, pingpong_buffer_write_to_sub_list);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][1], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][3], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][1], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][3], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][2], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][3], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][2], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][3], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][3], false);
  render_pass_enable_flag[1] = false;
  ConfigurePingPongBufferWriteToSubList(render_pass_num, render_pass_list, render_pass_enable_flag, buffer_num, pingpong_buffer_write_to_sub_list);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][1], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][2], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[0][3], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][1], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][2], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[1][3], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[2][3], true);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[3][3], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][0], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][1], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][2], false);
  CHECK_EQ(pingpong_buffer_write_to_sub_list[4][3], false);
}
