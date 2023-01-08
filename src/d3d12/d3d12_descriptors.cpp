#include "d3d12_descriptors.h"
#include "d3d12_scene.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
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
  SetD3d12Name(descriptor_cbv_srv_uav_.descriptor_heap, "descriptor_heap_gpu_view");
  descriptor_sampler_ = InitDescriptorHeapSetGpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, handle_num_sampler);
  if (descriptor_sampler_.descriptor_heap == nullptr) {
    return false;
  }
  SetD3d12Name(descriptor_sampler_.descriptor_heap, "descriptor_heap_gpu_sampler");
  return true;
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
  assert(descriptor->reserved_num > 0 && descriptor->current_handle_num >= descriptor->reserved_num);
  if (descriptor->current_handle_num + src_descriptor_num > descriptor->total_handle_num) {
    descriptor->current_handle_num = descriptor->reserved_num;
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
bool DescriptorGpu::IsNextHandle(const D3D12_GPU_DESCRIPTOR_HANDLE& current_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& next_handle, const DescriptorType& type) const {
  switch (type) {
    case DescriptorType::kCbv:
    case DescriptorType::kSrv:
    case DescriptorType::kUav: {
      return next_handle.ptr - current_handle.ptr == descriptor_cbv_srv_uav_.handle_increment_size;
    }
    case DescriptorType::kSampler: {
      return next_handle.ptr - current_handle.ptr == descriptor_sampler_.handle_increment_size;
    }
    default: {
      logwarn("invalid descriptor type {}", type);
      assert(false && "IsNextHandle not implmented type");
      return false;
    }
  }
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::WriteToTransientHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, const D3D12_DESCRIPTOR_HEAP_TYPE heap_type, DescriptorHeapSetGpu* descriptor, D3d12Device* device) {
  if (num == 0) { return {}; }
  auto handle_num = AllocateArrayFrame<uint32_t>(num);
  auto handle_list = AllocateArrayFrame<D3D12_CPU_DESCRIPTOR_HANDLE>(num);
  uint32_t index = 0;
  handle_num[index] = 1;
  handle_list[index].ptr = handles[0].ptr;
  for (uint32_t i = 1; i < num; i++) {
    if (handles[i].ptr - handles[i-1].ptr == descriptor->handle_increment_size) {
      handle_num[index]++;
      continue;
    }
    index++;
    handle_list[index].ptr = handles[i].ptr;
    handle_num[index] = 1;
  }
  return CopyToGpuDescriptor(num, index + 1, handle_num, handle_list, heap_type, device, descriptor);
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::WriteToTransientViewHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, D3d12Device* device) {
  return WriteToTransientHandleRange(num, handles, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, &descriptor_cbv_srv_uav_, device);
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::WriteToTransientSamplerHandleRange(const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, D3d12Device* device) {
  return WriteToTransientHandleRange(num, handles, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, &descriptor_sampler_, device);
}
void DescriptorGpu::SetPersistentViewHandleNum(const uint32_t handle_num) {
  assert(descriptor_cbv_srv_uav_.reserved_num == 0 || descriptor_cbv_srv_uav_.reserved_num == handle_num);
  assert(handle_num <= descriptor_cbv_srv_uav_.total_handle_num);
  descriptor_cbv_srv_uav_.reserved_num = handle_num;
  descriptor_cbv_srv_uav_.current_handle_num = handle_num;
}
void DescriptorGpu::SetPersistentSamplerHandleNum(const uint32_t handle_num) {
  assert(descriptor_sampler_.reserved_num == 0 || descriptor_sampler_.reserved_num == handle_num);
  assert(handle_num <= descriptor_sampler_.total_handle_num);
  descriptor_sampler_.reserved_num = handle_num;
  descriptor_sampler_.current_handle_num = handle_num;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::GetViewGpuHandle(const uint32_t index) {
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{descriptor_cbv_srv_uav_.heap_start_gpu + index * descriptor_cbv_srv_uav_.handle_increment_size};
  return gpu_handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::WriteToPersistentViewHandleRange(const uint32_t start, const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE handle, D3d12Device* device) {
  assert(start + num <= descriptor_cbv_srv_uav_.reserved_num);
  D3D12_CPU_DESCRIPTOR_HANDLE dst_handle{descriptor_cbv_srv_uav_.heap_start_cpu + start * descriptor_cbv_srv_uav_.handle_increment_size};
  device->CopyDescriptorsSimple(num, dst_handle, handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{descriptor_cbv_srv_uav_.heap_start_gpu + start * descriptor_cbv_srv_uav_.handle_increment_size};
  return gpu_handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorGpu::WriteToPersistentSamplerHandleRange(const uint32_t start, const uint32_t num, const D3D12_CPU_DESCRIPTOR_HANDLE handle, D3d12Device* device) {
  assert(start + num <= descriptor_sampler_.reserved_num);
  D3D12_CPU_DESCRIPTOR_HANDLE dst_handle{descriptor_sampler_.heap_start_cpu + start * descriptor_sampler_.handle_increment_size};
  device->CopyDescriptorsSimple(num, dst_handle, handle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{descriptor_sampler_.heap_start_gpu + start * descriptor_sampler_.handle_increment_size};
  return gpu_handle;
}
D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE heap_head, const uint32_t increment_size, const uint32_t index) {
  return {heap_head.ptr + increment_size * index};
}
bool IsGpuHandleAvailableType(const ResourceStateType& type) {
  switch (type) {
    case ResourceStateType::kCbv:      { return true; }
    case ResourceStateType::kSrvPs:    { return true; }
    case ResourceStateType::kSrvNonPs: { return true; }
    case ResourceStateType::kUav:      { return true; }
    case ResourceStateType::kCommon:   {
      assert(false && "set other type to use ResourceStateType::kCommon");
      return false;
    }
  }
  return false;
}
DescriptorHeapSet CreateDescriptorHeapSet(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
  auto heap = CreateDescriptorHeap(device, type, descriptor_num, flags);
  const auto head = heap->GetCPUDescriptorHandleForHeapStart();
  const auto increment_size = device->GetDescriptorHandleIncrementSize(type);
  return {heap, head, increment_size};
}
}
