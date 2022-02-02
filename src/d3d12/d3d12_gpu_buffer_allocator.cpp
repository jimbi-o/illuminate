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
D3D12_RESOURCE_DESC1 ConvertToD3d12ResouceDesc1(const BufferConfig& config, const MainBufferSize& main_buffer_size) {
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
BufferAllocation CreateBuffer(const D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_DESC1& resource_desc, const D3D12_CLEAR_VALUE* clear_value, D3D12MA::Allocator* allocator) {
  using namespace D3D12MA;
  ALLOCATION_DESC allocation_desc{
    .Flags = ALLOCATION_FLAG_NONE,
    .HeapType = heap_type,
    .ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
    .CustomPool = nullptr,
  };
  BufferAllocation buffer_allocation{};
  auto hr = allocator->CreateResource2(&allocation_desc, &resource_desc, initial_state, clear_value, nullptr, &buffer_allocation.allocation, IID_PPV_ARGS(&buffer_allocation.resource));
  if (FAILED(hr)) {
    logerror("failed to create resource. {}", hr);
    assert(false && "failed to create resource");
    if (buffer_allocation.allocation) {
      buffer_allocation.allocation->Release();
      buffer_allocation.allocation = nullptr;
    }
    if (buffer_allocation.resource) {
      buffer_allocation.resource->Release();
      buffer_allocation.resource = nullptr;
    }
  }
  return buffer_allocation;
}
BufferAllocation CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator) {
  auto resource_desc = ConvertToD3d12ResouceDesc1(config, main_buffer_size);
  auto clear_value = (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0) ? &config.clear_value : nullptr;
  return CreateBuffer(config.heap_type, config.initial_state, resource_desc, clear_value, allocator);
}
void ReleaseBufferAllocation(BufferAllocation* b) {
  b->allocation->Release();
  b->resource->Release();
}
} // namespace illuminate
