#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_scene.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
void* D3d12BufferAllocatorAllocCallback(size_t Size, size_t Alignment, [[maybe_unused]] void* pUserData) {
  return AllocateSystem(Size, Alignment);
}
void D3d12BufferAllocatorFreeCallback([[maybe_unused]] void* pMemory, [[maybe_unused]] void* pUserData) {}
auto GetResourceWidth(const BufferConfig& config, const MainBufferSize& main_buffer_size) {
  if (config.dimension == D3D12_RESOURCE_DIMENSION_BUFFER && config.num_elements > 0 && config.stride_bytes > 0) {
    return config.num_elements * config.stride_bytes;
  }
  return GetPhysicalWidth(main_buffer_size, config.size_type, config.width);
}
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
    .Width = GetResourceWidth(config, main_buffer_size),
    .Height = GetPhysicalHeight(main_buffer_size, config.size_type, config.height),
    .DepthOrArraySize = config.depth_or_array_size,
    .MipLevels = config.miplevels,
    .Format = config.format,
    .SampleDesc = {
      .Count = config.sample_count,
      .Quality = config.sample_quality,
    },
    .Layout = config.layout,
    .Flags = GetD3d12ResourceFlags(config.descriptor_type_flags),
    .SamplerFeedbackMipRegion = {
      .Width = config.mip_width,
      .Height = config.mip_height,
      .Depth = config.mip_depth,
    },
  };
}
void CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, const D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator, D3D12MA::Allocation** allocation, ID3D12Resource** resource) {
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
BufferList CreateBuffers(const uint32_t buffer_config_num, const BufferConfig* buffer_config_list, const MainBufferSize& main_buffer_size, const uint32_t frame_buffer_num, D3D12MA::Allocator* buffer_allocator) {
  BufferList buffer_list{};
  buffer_list.buffer_allocation_index = AllocateArraySystem<uint32_t*>(buffer_config_num);
  for (uint32_t i = 0; i < buffer_config_num; i++) {
    buffer_list.buffer_allocation_num += GetBufferAllocationNum(buffer_config_list[i], frame_buffer_num);
  }
  buffer_list.buffer_allocation_list = AllocateArraySystem<D3D12MA::Allocation*>(buffer_list.buffer_allocation_num);
  buffer_list.resource_list = AllocateArraySystem<ID3D12Resource*>(buffer_list.buffer_allocation_num);
  buffer_list.buffer_config_index = AllocateArraySystem<uint32_t>(buffer_list.buffer_allocation_num);
  uint32_t buffer_allocation_index = 0;
  for (uint32_t i = 0; i < buffer_config_num; i++) {
    auto& buffer_config = buffer_config_list[i];
    const auto alloc_num = GetBufferAllocationNum(buffer_config, frame_buffer_num);
    buffer_list.buffer_allocation_index[i] = AllocateArraySystem<uint32_t>(alloc_num);
    for (uint32_t j = 0; j < alloc_num; j++) {
      buffer_list.buffer_allocation_index[i][j] = buffer_allocation_index;
      buffer_list.buffer_config_index[buffer_allocation_index] = i;
      if (buffer_config.descriptor_only) {
        buffer_list.buffer_allocation_list[buffer_allocation_index] = nullptr;
        buffer_list.resource_list[buffer_allocation_index] = nullptr;
      } else {
        auto resource_desc = ConvertToD3d12ResourceDesc1(buffer_config, main_buffer_size);
        auto clear_value = (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0) ? &buffer_config.clear_value : nullptr;
        CreateBuffer(buffer_config.heap_type, ConvertToD3d12ResourceState(buffer_config.initial_state), resource_desc, clear_value, buffer_allocator,
                     &buffer_list.buffer_allocation_list[buffer_allocation_index], &buffer_list.resource_list[buffer_allocation_index]);
      }
      logdebug("buffer created. config_index:{} local_index:{} alloc_index:{}", i, j, buffer_allocation_index);
      buffer_allocation_index++;
    }
  }
  assert(buffer_allocation_index == buffer_list.buffer_allocation_num);
  return buffer_list;
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
ID3D12Resource** GetResourceList(const uint32_t buffer_num, const uint32_t* const buffer_allocation_index_list, const BufferList& buffer_list, const MemoryType& type) {
  auto resource_list = AllocateArray<ID3D12Resource*>(type, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    if (IsSceneBuffer(buffer_allocation_index_list[i])) {
      resource_list[i] = nullptr;
      continue;
    }
    resource_list[i] = buffer_list.resource_list[buffer_allocation_index_list[i]];
  }
  return resource_list;
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
  auto pingpong_buffer_write_to_sub_list = AllocateArrayFrame<bool*>(buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    pingpong_buffer_write_to_sub_list[i] = AllocateArrayFrame<bool>(render_pass_num);
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
  ClearAllAllocations();
}
