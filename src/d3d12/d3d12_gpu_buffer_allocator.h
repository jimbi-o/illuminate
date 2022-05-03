#ifndef ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATOR_H
#define ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATOR_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_render_graph.h"
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
struct BufferAllocation {
  D3D12MA::Allocation* allocation{nullptr};
  ID3D12Resource* resource{nullptr};
};
D3D12MA::Allocator* GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device);
D3D12_RESOURCE_DESC1 ConvertToD3d12ResourceDesc1(const BufferConfig& config, const MainBufferSize& main_buffer_size);
void ReleaseBufferAllocation(BufferAllocation* b);
void* MapResource(ID3D12Resource* resource, const uint32_t size, const uint32_t read_begin = 0, const uint32_t read_end = 0);
void UnmapResource(ID3D12Resource* resource);
static const uint32_t kBufferSubIndexDefault = 0;
static const uint32_t kBufferSubIndexUpload = 1;
struct BufferList {
  uint32_t** buffer_allocation_index{nullptr};
  uint32_t buffer_allocation_num{0};
  uint32_t buffer_allocation_num_wo_additional_buffers{0};
  D3D12MA::Allocation** buffer_allocation_list{nullptr};
  ID3D12Resource** resource_list{nullptr};
  uint32_t* buffer_config_index{nullptr};
  uint32_t additional_buffer_index_upload{};
  uint32_t additional_buffer_index_default{};
};
constexpr inline auto GetBufferAllocationNum(const BufferConfig& config, const uint32_t frame_buffer_num) {
  if (config.pingpong) {
    return 2U;
  }
  if (config.frame_buffered) {
    return frame_buffer_num;
  }
  if (config.need_upload) {
    return 2U;
  }
  return 1U;
}
BufferList CreateBuffers(const uint32_t buffer_config_num, const BufferConfig* buffer_config_list, const uint32_t additional_buffer_num, const uint32_t additional_buffer_size_in_bytes, const MainBufferSize& main_buffer_size, const uint32_t frame_buffer_num, D3D12MA::Allocator* buffer_allocator, MemoryAllocationJanitor* allocator);
void ReleaseBuffers(BufferList* buffer_list);
constexpr inline auto GetBufferAllocationIndex(const BufferList& buffer_list, const uint32_t buffer_index, const uint32_t index) {
  return buffer_list.buffer_allocation_index[buffer_index][index];
}
constexpr inline auto GetResource(const BufferList& buffer_list, const uint32_t buffer_config_index, const uint32_t sub_index) {
  return buffer_list.resource_list[GetBufferAllocationIndex(buffer_list, buffer_config_index, sub_index)];
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
constexpr inline void RegisterBufferAllocation(const uint32_t buffer_allocation_index, const BufferAllocation& buffer_allocation, BufferList* buffer_list) {
  buffer_list->buffer_allocation_list[buffer_allocation_index] = buffer_allocation.allocation;
  buffer_list->resource_list[buffer_allocation_index] = buffer_allocation.resource;
}
void ConfigurePingPongBufferWriteToSubList(const uint32_t render_pass_num, const RenderPass* render_pass_list, const bool* render_pass_enable_flag, const uint32_t buffer_num, bool** pingpong_buffer_write);
constexpr inline auto IsPingPongMainBuffer(const BufferList& buffer_list, const uint32_t buffer_config_index, const uint32_t buffer_allocation_index) {
  return buffer_allocation_index == buffer_list.buffer_allocation_index[buffer_config_index][0];
}
constexpr inline auto GetBufferLocalIndex(const BufferList& buffer_list, const uint32_t buffer_config_index, const uint32_t buffer_allocation_index) {
  auto initial_index = buffer_allocation_index;
  for (; initial_index > 0 && buffer_list.buffer_config_index[initial_index - 1] == buffer_config_index; initial_index--);
  for (uint32_t i = 0; buffer_list.buffer_config_index[initial_index + i] == buffer_config_index; i++) {
    if (buffer_list.buffer_allocation_index[buffer_config_index][i] == buffer_allocation_index) { return i; }
  }
  return 0U;
}
}
#endif
