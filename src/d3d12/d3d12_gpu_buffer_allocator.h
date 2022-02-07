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
  uint32_t* buffer_index_writable{nullptr};
  uint32_t* buffer_index_readable{nullptr};
  uint32_t buffer_allocation_num{0};
  D3D12MA::Allocation** buffer_allocation_list{nullptr};
  ID3D12Resource** resource_list{nullptr};
};
template <typename A>
BufferList CreateBuffers(const uint32_t buffer_num, const BufferConfig* buffer_config_list, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* buffer_allocator, A* allocator) {
  // TODO pingpong
  BufferList buffer_list{};
  buffer_list.buffer_allocation_num = buffer_num;
  buffer_list.buffer_index_writable = AllocateArray<uint32_t>(allocator, buffer_num);
  buffer_list.buffer_index_readable = AllocateArray<uint32_t>(allocator, buffer_num);
  buffer_list.buffer_allocation_list = AllocateArray<D3D12MA::Allocation*>(allocator, buffer_num);
  buffer_list.resource_list = AllocateArray<ID3D12Resource*>(allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
    buffer_list.buffer_index_writable[i] = i;
    buffer_list.buffer_index_readable[i] = i;
    auto& buffer_config = buffer_config_list[i];
    if (buffer_config.descriptor_only) {
      buffer_list.buffer_allocation_list[i] = nullptr;
      buffer_list.resource_list[i] = nullptr;
      continue;
    }
    CreateBuffer(buffer_config, main_buffer_size, buffer_allocator, &buffer_list.buffer_allocation_list[i], &buffer_list.resource_list[i]);
  }
  return buffer_list;
}
void ReleaseBuffers(BufferList* buffer_list);
ID3D12Resource* GetResource(const BufferList& buffer_list, const uint32_t buffer_index, const ResourceStateType state);
void RegisterResource(const uint32_t buffer_index, ID3D12Resource* resource, BufferList* buffer_list);
}
#endif
