#ifndef ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATOR_H
#define ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATOR_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_render_graph.h"
namespace illuminate {
struct BufferAllocation {
  D3D12MA::Allocation* allocation{nullptr};
  ID3D12Resource* resource{nullptr};
};
D3D12MA::Allocator* GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device);
D3D12_RESOURCE_DESC1 ConvertToD3d12ResourceDesc1(const BufferConfig& config, const MainBufferSize& main_buffer_size);
void CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator, D3D12MA::Allocation** allocation, ID3D12Resource** resource);
BufferAllocation CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator);
BufferAllocation CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator);
void ReleaseBufferAllocation(BufferAllocation* b);
void* MapResource(ID3D12Resource* resource, const uint32_t size, const uint32_t read_begin = 0, const uint32_t read_end = 0);
void UnmapResource(ID3D12Resource* resource);
struct BufferList {
  uint32_t** buffer_allocation_index{nullptr};
  uint32_t buffer_allocation_num{0};
  D3D12MA::Allocation** buffer_allocation_list{nullptr};
  ID3D12Resource** resource_list{nullptr};
  uint32_t* buffer_config_index{nullptr};
};
constexpr inline auto GetBufferAllocationNum(const BufferConfig& config, const uint32_t frame_buffer_num) {
  if (config.pingpong) {
    return 2U;
  }
  if (config.frame_buffered) {
    return frame_buffer_num;
  }
  return 1U;
}
template <typename A>
BufferList CreateBuffers(const uint32_t buffer_config_num, const BufferConfig* buffer_config_list, const MainBufferSize& main_buffer_size, const uint32_t frame_buffer_num, D3D12MA::Allocator* buffer_allocator, A* allocator) {
  BufferList buffer_list{};
  buffer_list.buffer_allocation_index = AllocateArray<uint32_t*>(allocator, buffer_config_num);
  buffer_list.buffer_allocation_num = 0;
  for (uint32_t i = 0; i < buffer_config_num; i++) {
    buffer_list.buffer_allocation_num += GetBufferAllocationNum(buffer_config_list[i], frame_buffer_num);
  }
  buffer_list.buffer_allocation_list = AllocateArray<D3D12MA::Allocation*>(allocator, buffer_list.buffer_allocation_num);
  buffer_list.resource_list = AllocateArray<ID3D12Resource*>(allocator, buffer_list.buffer_allocation_num);
  buffer_list.buffer_config_index = AllocateArray<uint32_t>(allocator, buffer_list.buffer_allocation_num);
  uint32_t buffer_allocation_index = 0;
  for (uint32_t i = 0; i < buffer_config_num; i++) {
    auto& buffer_config = buffer_config_list[i];
    const auto alloc_num = GetBufferAllocationNum(buffer_config, frame_buffer_num);
    buffer_list.buffer_allocation_index[i] = AllocateArray<uint32_t>(allocator, alloc_num);
    for (uint32_t j = 0; j < alloc_num; j++) {
      buffer_list.buffer_allocation_index[i][j] = buffer_allocation_index;
      buffer_list.buffer_config_index[buffer_allocation_index] = i;
      if (buffer_config.descriptor_only) {
        buffer_list.buffer_allocation_list[buffer_allocation_index] = nullptr;
        buffer_list.resource_list[buffer_allocation_index] = nullptr;
      } else {
        CreateBuffer(buffer_config, main_buffer_size, buffer_allocator, &buffer_list.buffer_allocation_list[buffer_allocation_index], &buffer_list.resource_list[buffer_allocation_index]);
      }
      buffer_allocation_index++;
    }
  }
  assert(buffer_allocation_index == buffer_list.buffer_allocation_num && "buffer allocation count bug");
  return buffer_list;
}
void ReleaseBuffers(BufferList* buffer_list);
constexpr inline auto GetBufferAllocationIndex(const BufferList& buffer_list, const uint32_t buffer_index, const uint32_t index) {
  return buffer_list.buffer_allocation_index[buffer_index][index];
}
constexpr inline auto GetBufferLocalIndex(const BufferConfig& buffer_config, const ResourceStateType state, const bool write_to_sub, const uint32_t frame_index) {
  if (buffer_config.pingpong) {
    const auto read_only = IsResourceStateReadOnly(state);
    if (read_only && !write_to_sub || !read_only && write_to_sub) {
      return 1U;
    }
    return 0U;
  }
  if (buffer_config.frame_buffered) {
    return frame_index;
  }
  return 0U;
}
constexpr inline void RegisterResource(const uint32_t buffer_allocation_index, ID3D12Resource* resource, BufferList* buffer_list) {
  buffer_list->resource_list[buffer_allocation_index] = resource;
}
void ConfigurePingPongBufferWriteToSubList(const uint32_t render_pass_num, const RenderPass* render_pass_list, const bool* render_pass_enable_flag, const uint32_t buffer_num, bool** pingpong_buffer_write);
constexpr inline auto IsPingPongMainBuffer(const BufferList& buffer_list, const uint32_t buffer_config_index, const uint32_t buffer_allocation_index) {
  return buffer_allocation_index == buffer_list.buffer_allocation_index[buffer_config_index][0];
}
constexpr inline auto GetBufferLocalIndex(const BufferList& buffer_list, const uint32_t buffer_config_index, const uint32_t buffer_allocation_index) {
  for (uint32_t i = 0; buffer_list.buffer_config_index[buffer_allocation_index + i] == buffer_config_index; i++) {
    if (buffer_list.buffer_allocation_index[buffer_config_index][i] == buffer_allocation_index) { return i; }
  }
  return 0U;
}
}
#endif
