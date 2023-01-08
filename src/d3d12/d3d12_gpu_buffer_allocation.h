#ifndef ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATION_H
#define ILLUMINATE_D3D12_GPU_BUFFER_ALLOCATION_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_render_graph.h"
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
enum class MemoryType : uint8_t;
D3D12MA::Allocator* GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device);
D3D12_RESOURCE_DESC1 ConvertToD3d12ResourceDesc1(const BufferConfig& config, const MainBufferSize& main_buffer_size);
void CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, const D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator, D3D12MA::Allocation** allocation, ID3D12Resource** resource);
void* MapResource(ID3D12Resource* resource, const uint32_t size, const uint32_t read_begin = 0, const uint32_t read_end = 0);
void UnmapResource(ID3D12Resource* resource);
}
#endif
