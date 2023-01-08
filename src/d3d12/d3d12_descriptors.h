#ifndef ILLUMINATE_D3D12_DESCRIPTORS_H
#define ILLUMINATE_D3D12_DESCRIPTORS_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_render_graph.h"
#include "illuminate/util/hash_map.h"
namespace illuminate {
class DescriptorGpu {
 public:
  bool Init(D3d12Device* device, const uint32_t handle_num_view, const uint32_t handle_num_sampler);
  void Term();
  void SetDescriptorHeapsToCommandList(const uint32_t command_list_num, D3d12CommandList** command_list);
  D3D12_GPU_DESCRIPTOR_HANDLE WriteToTransientViewHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, D3d12Device* device);
  D3D12_GPU_DESCRIPTOR_HANDLE WriteToTransientSamplerHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, D3d12Device* device);
  void SetPersistentViewHandleNum(const uint32_t handle_num);
  void SetPersistentSamplerHandleNum(const uint32_t handle_num);
  D3D12_GPU_DESCRIPTOR_HANDLE GetViewGpuHandle(const uint32_t index);
  D3D12_GPU_DESCRIPTOR_HANDLE WriteToPersistentViewHandleRange(const uint32_t start, const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE handle, D3d12Device* device);
  D3D12_GPU_DESCRIPTOR_HANDLE WriteToPersistentSamplerHandleRange(const uint32_t start, const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE handle, D3d12Device* device);
  bool IsNextHandle(const D3D12_GPU_DESCRIPTOR_HANDLE& current_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& next_handle, const DescriptorType& type) const;
 private:
  struct DescriptorHeapSetGpu {
    uint32_t handle_increment_size{};
    uint32_t total_handle_num{0};
    uint32_t current_handle_num{0};
    uint32_t reserved_num{0};
    ID3D12DescriptorHeap* descriptor_heap{nullptr};
    uint64_t heap_start_cpu{};
    uint64_t heap_start_gpu{};
  };
  static DescriptorHeapSetGpu InitDescriptorHeapSetGpu(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t handle_num);
  static D3D12_GPU_DESCRIPTOR_HANDLE CopyToGpuDescriptor(const uint32_t src_descriptor_num, const uint32_t handle_list_len, const uint32_t* handle_num_list, const D3D12_CPU_DESCRIPTOR_HANDLE* handle_list, const D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3d12Device* device, DescriptorHeapSetGpu* descriptor);
  static D3D12_GPU_DESCRIPTOR_HANDLE WriteToTransientHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, const D3D12_DESCRIPTOR_HEAP_TYPE heap_type, DescriptorHeapSetGpu* descriptor, D3d12Device* device);
  DescriptorHeapSetGpu descriptor_cbv_srv_uav_;
  DescriptorHeapSetGpu descriptor_sampler_;
};
struct DescriptorHeapSet {
  ID3D12DescriptorHeap* heap{};
  D3D12_CPU_DESCRIPTOR_HANDLE heap_head_addr{};
  uint32_t handle_increment_size{};
};
DescriptorHeapSet CreateDescriptorHeapSet(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE heap_head, const uint32_t increment_size, const uint32_t index);
bool IsGpuHandleAvailableType(const ResourceStateType& type);
}
#endif
