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
uint32_t DescriptorGpu::GetViewNum(const uint32_t buffer_num, const RenderPassBuffer* buffer_list) {
  uint32_t view_num = 0;
  for (uint32_t i = 0; i < buffer_num; i++) {
    switch (buffer_list[i].state) {
      case ResourceStateType::kCbv:
      case ResourceStateType::kSrv:
      case ResourceStateType::kUav: {
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
}
