#ifndef ILLUMINATE_D3D12_DESCRIPTORS_H
#define ILLUMINATE_D3D12_DESCRIPTORS_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_render_graph.h"
#include "illuminate/util/hash_map.h"
namespace illuminate {
ID3D12DescriptorHeap* CreateDescriptorHeap(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags);
template <typename A>
class DescriptorCpu {
 public:
  bool Init(D3d12Device* const device, const uint32_t* descriptor_handle_num_per_type, A* allocator) {
    for (uint32_t i = 0; i < kDescriptorTypeNum; i++) {
      handles_[i].SetAllocator(allocator, descriptor_handle_num_per_type[i] * kHandleMapSizeCoefficient);
      total_handle_num_[GetDescriptorTypeIndex(static_cast<DescriptorType>(i))] += descriptor_handle_num_per_type[i];
    }
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
      if (total_handle_num_[i] == 0) { continue; }
      const auto type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
      handle_increment_size_[i] = device->GetDescriptorHandleIncrementSize(type);
      descriptor_heap_[i] = CreateDescriptorHeap(device, type, total_handle_num_[i], D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
      if (descriptor_heap_[i] == nullptr) {
        assert(false && "CreateDescriptorHeap failed");
        for (uint32_t j = 0; j < i; j++) {
          descriptor_heap_[j]->Release();
          descriptor_heap_[j] = nullptr;
        }
        return false;
      }
      heap_start_[i] = descriptor_heap_[i]->GetCPUDescriptorHandleForHeapStart().ptr;
      handle_num_[i] = 0;
    }
    return true;
  }
  void Term() {
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
      if (descriptor_heap_[i]) {
        auto refval = descriptor_heap_[i]->Release();
        if (refval != 0) {
          assert(false && "descriptor_heap_ refefence left");
        }
      }
    }
  }
  const D3D12_CPU_DESCRIPTOR_HANDLE* CreateHandle(const StrHash& name, const DescriptorType type) {
    auto descriptor_type_index = static_cast<uint32_t>(type);
    assert(handles_[descriptor_type_index].Get(name) == nullptr && "increase handles_[descriptor_type_index] map size");
    auto index = GetDescriptorTypeIndex(type);
    assert(handle_num_[index] <= total_handle_num_[index] && "handle num exceeded handle pool size");
    D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    handle.ptr = heap_start_[index] + handle_num_[index] * handle_increment_size_[index];
    if (!handles_[descriptor_type_index].Insert(name, std::move(handle))) {
      assert(false && "failed to insert handle to map, increase map table size");
      return nullptr;
    }
    handle_num_[index]++;
    return handles_[descriptor_type_index].Get(name);
  }
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetHandle(const StrHash& name, const DescriptorType type) const {
    auto descriptor_type_index = static_cast<uint32_t>(type);
    return handles_[descriptor_type_index].Get(name);
  }
 private:
  static constexpr auto GetDescriptorTypeIndex(const DescriptorType& type) {
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
  static const uint32_t kHandleMapSizeCoefficient = 8;
  HashMap<D3D12_CPU_DESCRIPTOR_HANDLE, A> handles_[kDescriptorTypeNum];
  ID3D12DescriptorHeap* descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t total_handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_increment_size_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint64_t heap_start_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
};
class DescriptorGpu {
 public:
  static uint32_t GetViewNum(const uint32_t buffer_num, const RenderPassBuffer* buffer_list);
  bool Init(D3d12Device* device, const uint32_t handle_num_view, const uint32_t handle_num_sampler);
  void Term();
  template <typename AllocatorCpu>
  auto CopyViewDescriptors(D3d12Device* device, const uint32_t buffer_num, const RenderPassBuffer* buffer_list, const DescriptorCpu<AllocatorCpu>& descriptor_cpu) {
    const auto view_num = GetViewNum(buffer_num, buffer_list);
    if (view_num == 0) {
      return D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
    assert(view_num <= descriptor_cbv_srv_uav_.total_handle_num);
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto handle_num = AllocateArray<uint32_t>(&tmp_allocator, view_num);
    auto handles = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, view_num);
    for (uint32_t i = 0; i < view_num; i++) {
      handle_num[i] = 0;
      handles[i].ptr = 0;
    }
    uint32_t view_index = 0;
    bool first_buffer = true;
    for (uint32_t i = 0; i < buffer_num; i++) {
      auto& buffer = buffer_list[i];
      auto descriptor_type = ConvertToDescriptorType(buffer.state);
      switch (descriptor_type) {
        case DescriptorType::kCbv:
        case DescriptorType::kSrv:
        case DescriptorType::kUav: {
          auto handle = descriptor_cpu.GetHandle(buffer.buffer_name, descriptor_type);
          if (handle == nullptr) { break; }
          if (!first_buffer && handles[view_index].ptr + descriptor_cbv_srv_uav_.handle_increment_size == handle->ptr) {
            handle_num[view_index]++;
            break;
          }
          if (!first_buffer) {
            view_index++;
          }
          handle_num[view_index] = 1;
          handles[view_index].ptr = handle->ptr;
          first_buffer = false;
          break;
        }
        default: {
          break;
        }
      }
    }
    assert(view_index < view_num && "buffer view count bug");
    return CopyToGpuDescriptor(view_num, view_index + 1, handle_num, handles, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, device, &descriptor_cbv_srv_uav_);
  }
  template <typename AllocatorCpu>
  auto CopySamplerDescriptors(D3d12Device* device, const uint32_t buffer_num, const RenderPassBuffer* buffer_list, const DescriptorCpu<AllocatorCpu>& descriptor_cpu) {
    uint32_t sampler_num = 0;
    for (uint32_t i = 0; i < buffer_num; i++) {
      if (buffer_list[i].sampler) {
        sampler_num++;
      }
    }
    if (sampler_num == 0) { return D3D12_GPU_DESCRIPTOR_HANDLE{}; }
    assert(sampler_num <= descriptor_sampler_.total_handle_num);
    auto tmp_allocator = GetTemporalMemoryAllocator();
    uint32_t sampler_index = 0;
    auto handle_num = AllocateArray<uint32_t>(&tmp_allocator, sampler_num);
    auto handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, sampler_num);
    bool first_sampler = true;
    for (uint32_t i = 0; i < buffer_num; i++) {
      if (!buffer_list[i].sampler) { continue; }
      auto handle = descriptor_cpu.GetHandle(buffer_list[i].sampler, DescriptorType::kSampler);
      if (handle == nullptr) { continue; }
      if (!first_sampler && handle_list[sampler_index].ptr + descriptor_sampler_.handle_increment_size == handle->ptr) {
        handle_num[sampler_index]++;
        continue;
      }
      if (!first_sampler) {
        sampler_index++;
      }
      handle_num[sampler_index] = 1;
      handle_list[sampler_index].ptr = handle->ptr;
      first_sampler = false;
    }
    return CopyToGpuDescriptor(sampler_num, sampler_index + 1, handle_num, handle_list, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, device, &descriptor_sampler_);
  }
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