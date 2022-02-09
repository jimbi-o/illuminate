#include "d3d12_descriptors.h"
#include "d3d12_src_common.h"
namespace illuminate {
ID3D12DescriptorHeap* CreateDescriptorHeap(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {
    .Type = type,
    .NumDescriptors = descriptor_num,
    .Flags = flags,
    .NodeMask = 0,
  };
  ID3D12DescriptorHeap* descriptor_heap{nullptr};
  auto hr = device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr)) {
    logerror("CreateDescriptorHeap failed. {} {} {} {}", hr, type, descriptor_num, flags);
    assert(false && "CreateDescriptorHeap failed");
    return nullptr;
  }
  return descriptor_heap;
}
void DescriptorCpu::Term() {
  for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
    if (descriptor_heap_[i]) {
      auto refval = descriptor_heap_[i]->Release();
      if (refval != 0) {
        assert(false && "descriptor_heap_ refefence left");
      }
    }
  }
}
const D3D12_CPU_DESCRIPTOR_HANDLE& DescriptorCpu::CreateHandle(const uint32_t index, const DescriptorType type) {
  assert((type == DescriptorType::kSampler && index < sampler_num_) || (type != DescriptorType::kSampler && index < buffer_allocation_num_));
  auto heap_index = GetDescriptorTypeIndex(type);
  D3D12_CPU_DESCRIPTOR_HANDLE handle{};
  handle.ptr = heap_start_[heap_index] + handle_num_[heap_index] * handle_increment_size_[heap_index];
  auto descriptor_type_index = static_cast<uint32_t>(type);
  assert(handles_[descriptor_type_index][index].ptr == 0);
  handles_[descriptor_type_index][index].ptr = handle.ptr;
  handle_num_[heap_index]++;
  assert(handle_num_[heap_index] <= total_handle_num_[heap_index] && "handle num exceeded handle pool size");
  return handles_[descriptor_type_index][index];
}
void DescriptorCpu::RegisterExternalHandle(const uint32_t index, const DescriptorType type, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
  auto descriptor_type_index = static_cast<uint32_t>(type);
  handles_[descriptor_type_index][index].ptr = handle.ptr;
}
namespace {
uint32_t GetViewNum(const uint32_t buffer_num, const DescriptorType* descriptor_type_list) {
  uint32_t view_num = 0;
  for (uint32_t i = 0; i < buffer_num; i++) {
    switch (descriptor_type_list[i]) {
      case DescriptorType::kCbv:
      case DescriptorType::kSrv:
      case DescriptorType::kUav: {
        view_num++;
        break;
      }
      default: {
        break;
      }
    }
  }
  return view_num;
}
} // namespace anonymous
DescriptorGpu::DescriptorHeapSetGpu DescriptorGpu::InitDescriptorHeapSetGpu(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t handle_num) {
  DescriptorHeapSetGpu descriptor_heap;
  descriptor_heap.total_handle_num = handle_num;
  descriptor_heap.current_handle_num = 0;
  descriptor_heap.handle_increment_size = device->GetDescriptorHandleIncrementSize(type);
  descriptor_heap.descriptor_heap = CreateDescriptorHeap(device, type, handle_num, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
  descriptor_heap.heap_start_cpu = descriptor_heap.descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr;
  descriptor_heap.heap_start_gpu = descriptor_heap.descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr;
  return descriptor_heap;
}
bool DescriptorGpu::Init(D3d12Device* device, const uint32_t handle_num_view, const uint32_t handle_num_sampler) {
  descriptor_cbv_srv_uav_ = InitDescriptorHeapSetGpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, handle_num_view);
  if (descriptor_cbv_srv_uav_.descriptor_heap == nullptr) {
    return false;
  }
  descriptor_sampler_ = InitDescriptorHeapSetGpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, handle_num_sampler);
  return descriptor_sampler_.descriptor_heap != nullptr;
}
void DescriptorGpu::Term() {
  {
    auto refval = descriptor_cbv_srv_uav_.descriptor_heap->Release();
    if (refval > 0) {
      logerror("descriptor_cbv_srv_uav_ descriptor_heap reference left: {}", refval);
      assert(false && "descriptor_cbv_srv_uav_ descriptor_heap reference left");
    }
  }
  {
    auto refval = descriptor_sampler_.descriptor_heap->Release();
    if (refval > 0) {
      logerror("descriptor_sampler_ descriptor_heap reference left: {}", refval);
      assert(false && "descriptor_sampler_ descriptor_heap reference left");
    }
  }
}
void DescriptorGpu::SetDescriptorHeapsToCommandList(const uint32_t command_list_num, D3d12CommandList** command_list) {
  ID3D12DescriptorHeap* heaps[] = {descriptor_cbv_srv_uav_.descriptor_heap, descriptor_sampler_.descriptor_heap, };
  for (uint32_t i = 0; i < command_list_num; i++) {
    command_list[i]->SetDescriptorHeaps(2, heaps);
  }
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::CopyToGpuDescriptor(const uint32_t src_descriptor_num, const uint32_t handle_list_len, const uint32_t* handle_num_list, const D3D12_CPU_DESCRIPTOR_HANDLE* handle_list, const D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3d12Device* device, DescriptorHeapSetGpu* descriptor) {
  if (descriptor->current_handle_num + src_descriptor_num > descriptor->total_handle_num) {
    descriptor->current_handle_num = 0;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE dst_handle{descriptor->heap_start_cpu + descriptor->current_handle_num * descriptor->handle_increment_size};
  assert(handle_num_list[handle_list_len - 1] > 0);
  if (handle_list_len == 1) {
    assert(handle_num_list[0] == src_descriptor_num);
    device->CopyDescriptorsSimple(src_descriptor_num, dst_handle, handle_list[0], heap_type);
  } else {
#ifdef BUILD_WITH_TEST
    uint32_t handle_num_debug = 0;
    for (uint32_t i = 0; i < handle_list_len; i++) {
      handle_num_debug += handle_num_list[i];
    }
    assert(handle_num_debug == src_descriptor_num);
#endif
    device->CopyDescriptors(1, &dst_handle, &src_descriptor_num, handle_list_len, handle_list, handle_num_list, heap_type);
  }
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{descriptor->heap_start_gpu + descriptor->current_handle_num * descriptor->handle_increment_size};
  descriptor->current_handle_num += src_descriptor_num;
  return gpu_handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::CopyViewDescriptors(D3d12Device* device, const uint32_t buffer_num, const uint32_t* buffer_id_list, const DescriptorType* descriptor_type_list, const DescriptorCpu& descriptor_cpu) {
  const auto view_num = GetViewNum(buffer_num, descriptor_type_list);
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
    switch (descriptor_type_list[i]) {
      case DescriptorType::kCbv:
      case DescriptorType::kSrv:
      case DescriptorType::kUav: {
        auto handle = descriptor_cpu.GetHandle(buffer_id_list[i], descriptor_type_list[i]);
        if (handle.ptr == 0) {
          logerror("descriptor_cpu.GetHandle failed {} {}", buffer_id_list[i], descriptor_type_list[i]);
          assert(false && "descriptor_cpu.GetHandle failed.");
          break;
        }
        if (!first_buffer && handles[view_index].ptr + descriptor_cbv_srv_uav_.handle_increment_size == handle.ptr) {
          handle_num[view_index]++;
          break;
        }
        if (!first_buffer) {
          view_index++;
        }
        handle_num[view_index] = 1;
        handles[view_index].ptr = handle.ptr;
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
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::CopySamplerDescriptors(D3d12Device* device, const uint32_t sampler_num, const uint32_t* sampler_index_list, const DescriptorCpu& descriptor_cpu) {
  if (sampler_num == 0) { return D3D12_GPU_DESCRIPTOR_HANDLE{}; }
  assert(sampler_num <= descriptor_sampler_.total_handle_num);
  auto tmp_allocator = GetTemporalMemoryAllocator();
  uint32_t sampler_index = 0;
  auto handle_num = AllocateArray<uint32_t>(&tmp_allocator, sampler_num);
  auto handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, sampler_num);
  for (uint32_t i = 0; i < sampler_num; i++) {
    auto handle = descriptor_cpu.GetHandle(sampler_index_list[i], DescriptorType::kSampler);
    if (handle.ptr == 0) { continue; }
    if (i > 0 && handle_list[sampler_index].ptr + descriptor_sampler_.handle_increment_size == handle.ptr) {
      handle_num[sampler_index]++;
      continue;
    }
    if (i > 0) {
      sampler_index++;
    }
    handle_num[sampler_index] = 1;
    handle_list[sampler_index].ptr = handle.ptr;
  }
  return CopyToGpuDescriptor(sampler_num, sampler_index + 1, handle_num, handle_list, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, device, &descriptor_sampler_);
}
}
