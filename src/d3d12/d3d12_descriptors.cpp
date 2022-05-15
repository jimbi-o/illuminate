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
bool DescriptorCpu::Init(D3d12Device* const device, const uint32_t buffer_allocation_num, const uint32_t* descriptor_handle_num_per_type, MemoryAllocationJanitor* allocator) {
  buffer_allocation_num_ = buffer_allocation_num;
  sampler_num_ = descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSampler)];
  for (uint32_t i = 0; i < kDescriptorTypeNum; i++) {
    const auto num = (i == static_cast<uint32_t>(DescriptorType::kSampler)) ? sampler_num_ : buffer_allocation_num_;
    handles_[i] = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(allocator, num);
    for (uint32_t j = 0; j < num; j++) {
      handles_[i][j].ptr = 0;
    }
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
    SetD3d12Name(descriptor_heap_[i], "descriptor_heap" + std::to_string(i));
    heap_start_[i] = descriptor_heap_[i]->GetCPUDescriptorHandleForHeapStart().ptr;
    handle_num_[i] = 0;
  }
  return true;
}
void DescriptorCpu::Term() {
  for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
    if (descriptor_heap_[i]) {
      const auto refval = descriptor_heap_[i]->Release();
      logtrace("DescriptorCpu refval:{}", refval);
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
  logtrace("handle created. alloc:{} type:{} ptr:{:x}", index, type, handles_[descriptor_type_index][index].ptr);
  return handles_[descriptor_type_index][index];
}
void DescriptorCpu::RegisterExternalHandle(const uint32_t index, const DescriptorType type, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
  auto descriptor_type_index = static_cast<uint32_t>(type);
  handles_[descriptor_type_index][index].ptr = handle.ptr;
  logtrace("handle registered. alloc:{} desc:{} ptr:{}", index, type, handles_[descriptor_type_index][index].ptr);
}
ID3D12DescriptorHeap* DescriptorCpu::RetainDescriptorHeap(const DescriptorType type) {
  auto heap = descriptor_heap_[GetDescriptorTypeIndex(type)];
  heap->AddRef();
  return heap;
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
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto handle_num = AllocateArray<uint32_t>(&tmp_allocator, num);
  auto handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, num);
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
}
