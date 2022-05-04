#ifndef ILLUMINATE_D3D12_DESCRIPTORS_H
#define ILLUMINATE_D3D12_DESCRIPTORS_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_render_graph.h"
#include "illuminate/util/hash_map.h"
namespace illuminate {
ID3D12DescriptorHeap* CreateDescriptorHeap(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags);
class MemoryAllocationJanitor;
class DescriptorCpu {
 public:
  bool Init(D3d12Device* const device, const uint32_t buffer_allocation_num, const uint32_t sampler_num, const uint32_t* descriptor_handle_num_per_type, MemoryAllocationJanitor* allocator);
  void Term();
  const D3D12_CPU_DESCRIPTOR_HANDLE& CreateHandle(const uint32_t index, const DescriptorType type);
  const D3D12_CPU_DESCRIPTOR_HANDLE& GetHandle(const uint32_t index, const DescriptorType type) const {
    auto descriptor_type_index = static_cast<uint32_t>(type);
    return handles_[descriptor_type_index][index];
  }
  void RegisterExternalHandle(const uint32_t index, const DescriptorType type, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
 private:
  static constexpr D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorTypeIndex(const DescriptorType& type) {
    switch (type) {
      case DescriptorType::kCbv: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case DescriptorType::kSrv: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case DescriptorType::kUav: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case DescriptorType::kSampler: { return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; }
      case DescriptorType::kRtv: { return D3D12_DESCRIPTOR_HEAP_TYPE_RTV; }
      case DescriptorType::kDsv: { return D3D12_DESCRIPTOR_HEAP_TYPE_DSV; }
    }
    assert(false && "invalid DescriptorType");
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE* handles_[kDescriptorTypeNum]{};
  ID3D12DescriptorHeap* descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t total_handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_increment_size_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint64_t heap_start_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t buffer_allocation_num_{0};
  uint32_t sampler_num_{0};
};
class DescriptorGpu {
 public:
  bool Init(D3d12Device* device, const uint32_t handle_num_view, const uint32_t handle_num_sampler);
  void Term();
  D3D12_GPU_DESCRIPTOR_HANDLE CopyViewDescriptors(D3d12Device* device, const uint32_t buffer_num, const uint32_t* buffer_id_list, const DescriptorType* descriptor_type_list, const DescriptorCpu& descriptor_cpu);
  D3D12_GPU_DESCRIPTOR_HANDLE CopySamplerDescriptors(D3d12Device* device, const uint32_t sampler_num, const uint32_t* sampler_index_list, const DescriptorCpu& descriptor_cpu);
  auto GetViewHandleCount() const { return descriptor_cbv_srv_uav_.current_handle_num; }
  auto GetViewHandleTotal() const { return descriptor_cbv_srv_uav_.total_handle_num; }
  auto GetSamplerHandleCount() const { return descriptor_sampler_.current_handle_num; }
  auto GetSamplerHandleTotal() const { return descriptor_sampler_.total_handle_num; }
  void SetDescriptorHeapsToCommandList(const uint32_t command_list_num, D3d12CommandList** command_list);
 private:
  struct DescriptorHeapSetGpu {
    uint32_t handle_increment_size{};
    uint32_t total_handle_num{0};
    uint32_t current_handle_num{0};
    ID3D12DescriptorHeap* descriptor_heap{nullptr};
    uint64_t heap_start_cpu{};
    uint64_t heap_start_gpu{};
  };
  static DescriptorHeapSetGpu InitDescriptorHeapSetGpu(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t handle_num);
  static D3D12_GPU_DESCRIPTOR_HANDLE CopyToGpuDescriptor(const uint32_t src_descriptor_num, const uint32_t handle_list_len, const uint32_t* handle_num_list, const D3D12_CPU_DESCRIPTOR_HANDLE* handle_list, const D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3d12Device* device, DescriptorHeapSetGpu* descriptor);
  DescriptorHeapSetGpu descriptor_cbv_srv_uav_;
  DescriptorHeapSetGpu descriptor_sampler_;
};
}
#endif
