#include "d3d12_gpu_buffer_allocation.h"
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
}
