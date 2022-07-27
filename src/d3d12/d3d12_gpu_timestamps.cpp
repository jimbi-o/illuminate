#include "d3d12_gpu_timestamps.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
namespace illuminate {
namespace {
static const uint32_t kGpuTimestampQueryNumPerPass = 2;
static const uint32_t kGpuTimestampDstResourceRingBufferNum = 5;
auto CreateTimestampQueryHeaps(const uint32_t command_queue_num, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, D3d12Device* device) {
  auto timestamp_query_heaps = AllocateArraySystem<ID3D12QueryHeap*>(command_queue_num);
  D3D12_QUERY_HEAP_DESC desc{};
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (render_pass_num_per_queue[i] == 0) {
      timestamp_query_heaps[i] = nullptr;
      continue;
    }
    desc.Type = (command_queue_type[i] == D3D12_COMMAND_LIST_TYPE_COPY) ? D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    desc.Count = render_pass_num_per_queue[i] * kGpuTimestampQueryNumPerPass;
    auto hr = device->CreateQueryHeap(&desc, IID_PPV_ARGS(&timestamp_query_heaps[i]));
    if (FAILED(hr)) {
      assert(timestamp_query_heaps[i] == nullptr);
      logwarn("CreateQueryHeap failed. {} {} {} {}", desc.Type, desc.Count, i, hr);
    }
  }
  return timestamp_query_heaps;
}
auto CreateTimestampQuertyDstResource(const uint32_t command_queue_num, const uint32_t* render_pass_num_per_queue, D3D12MA::Allocator* buffer_allocator) {
  auto timestamp_query_dst_resource = AllocateArraySystem<ID3D12Resource**>(command_queue_num);
  auto timestamp_query_dst_allocation = AllocateArraySystem<D3D12MA::Allocation**>(command_queue_num);
  auto timestamp_query_dst_ptr = AllocateArraySystem<uint64_t**>(command_queue_num);
  auto timestamp_value = AllocateArraySystem<uint64_t*>(command_queue_num);
  auto timestamp_prev_end_value = AllocateArraySystem<uint64_t>(command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (render_pass_num_per_queue[i] == 0) {
      timestamp_query_dst_resource[i]  = nullptr;
      timestamp_query_dst_allocation[i] = nullptr;
      timestamp_query_dst_ptr[i] = nullptr;
      timestamp_value[i] = nullptr;
      continue;
    }
    timestamp_query_dst_resource[i]  = AllocateArraySystem<ID3D12Resource*>(kGpuTimestampDstResourceRingBufferNum);
    timestamp_query_dst_allocation[i] = AllocateArraySystem<D3D12MA::Allocation*>(kGpuTimestampDstResourceRingBufferNum);
    timestamp_query_dst_ptr[i] = AllocateArraySystem<uint64_t*>(kGpuTimestampDstResourceRingBufferNum);
    const auto timestamp_num = render_pass_num_per_queue[i] * kGpuTimestampQueryNumPerPass;
    const auto buffer_size = GetUint32(sizeof(uint64_t)) * timestamp_num;
    const auto desc = GetBufferDesc(buffer_size);
    for (uint32_t j = 0; j < kGpuTimestampDstResourceRingBufferNum; j++) {
      CreateBuffer(D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, desc, nullptr, buffer_allocator, &timestamp_query_dst_allocation[i][j], &timestamp_query_dst_resource[i][j]);
      timestamp_query_dst_ptr[i][j] = static_cast<uint64_t*>(MapResource(timestamp_query_dst_resource[i][j], buffer_size, 0, buffer_size));
    }
    timestamp_value[i] = AllocateArraySystem<uint64_t>(timestamp_num);
  }
  return std::make_tuple(timestamp_query_dst_resource, timestamp_query_dst_allocation, timestamp_query_dst_ptr, timestamp_value, timestamp_prev_end_value);
}
auto GetGpuTimestampFrequency(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list) {
  auto gpu_timestamp_frequency = AllocateArraySystem<uint64_t>(command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    command_queue_list[i]->GetTimestampFrequency(&gpu_timestamp_frequency[i]);
  }
  return gpu_timestamp_frequency;
}
void CopyTimestampToCpuCachedMemory(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, GpuTimestampSet* gpu_timestamp_set) {
  for (uint32_t i = 0; i < command_queue_num; i++) {
    const auto render_pass_num = render_pass_num_per_queue[i] * kGpuTimestampQueryNumPerPass;
    if (render_pass_num == 0) { continue; }
    gpu_timestamp_set->timestamp_prev_end_value[i] = gpu_timestamp_set->timestamp_value[i][render_pass_num - 1];
    memcpy(gpu_timestamp_set->timestamp_value[i],
           gpu_timestamp_set->timestamp_query_dst_ptr[i][gpu_timestamp_set->timestamp_query_dst_resource_index],
           sizeof(uint64_t) * render_pass_num);
  }
}
} // namespace
GpuTimestampSet CreateGpuTimestampSet(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, D3d12Device* device, D3D12MA::Allocator* buffer_allocator) {
  auto gpu_timestamp_frequency = GetGpuTimestampFrequency(command_queue_num, command_queue_list);
  auto timestamp_query_heaps = CreateTimestampQueryHeaps(command_queue_num, command_queue_type, render_pass_num_per_queue, device);
  auto [timestamp_query_dst_resource, timestamp_query_dst_allocation, timestamp_query_dst_ptr, timestamp_value, timestamp_prev_end_value] = CreateTimestampQuertyDstResource(command_queue_num, render_pass_num_per_queue, buffer_allocator);
  return {
    .gpu_timestamp_frequency = gpu_timestamp_frequency,
    .timestamp_query_heaps = timestamp_query_heaps,
    .timestamp_query_dst_resource = timestamp_query_dst_resource,
    .timestamp_query_dst_allocation = timestamp_query_dst_allocation,
    .timestamp_query_dst_ptr = timestamp_query_dst_ptr,
    .timestamp_value = timestamp_value,
    .timestamp_prev_end_value = timestamp_prev_end_value,
  };
}
void ReleaseGpuTimestampSet(const uint32_t command_queue_num, GpuTimestampSet* gpu_timestamp_set) {
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (gpu_timestamp_set->timestamp_query_dst_resource[i] == nullptr) { continue; }
    for (uint32_t j = 0; j < kGpuTimestampDstResourceRingBufferNum; j++) {
      UnmapResource(gpu_timestamp_set->timestamp_query_dst_resource[i][j]);
      gpu_timestamp_set->timestamp_query_dst_resource[i][j]->Release();
      gpu_timestamp_set->timestamp_query_dst_allocation[i][j]->Release();
    }
  }
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (gpu_timestamp_set->timestamp_query_heaps[i]) {
      gpu_timestamp_set->timestamp_query_heaps[i]->Release();
    }
  }
}
void OutputGpuTimestampToCpuVisibleBuffer(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto query_num = render_pass_num_per_queue[render_pass_queue_index[render_pass_index]] * kGpuTimestampQueryNumPerPass;
  CopyTimestampToCpuCachedMemory(command_queue_num, render_pass_num_per_queue, gpu_timestamp_set);
  auto dst_resource = gpu_timestamp_set->timestamp_query_dst_resource[render_pass_queue_index[render_pass_index]][gpu_timestamp_set->timestamp_query_dst_resource_index];
  gpu_timestamp_set->timestamp_query_dst_resource_index++;
  if (gpu_timestamp_set->timestamp_query_dst_resource_index >= kGpuTimestampDstResourceRingBufferNum) {
    gpu_timestamp_set->timestamp_query_dst_resource_index = 0;
  }
  if (!IsDebuggerPresent()) {
    // graphics debugger issues an error:
    // D3D12 ERROR: ID3D12GraphicsCommandList::SetComputeRootSignature: Invalid API called.  This method is not valid for D3D12_COMMAND_LIST_TYPE_COPY. [ EXECUTION ERROR #933: NO_COMPUTE_API_SUPPORT]
    // TODO read from buffer
    command_list->ResolveQueryData(gpu_timestamp_set->timestamp_query_heaps[render_pass_queue_index[render_pass_index]], D3D12_QUERY_TYPE_TIMESTAMP, 0, query_num, dst_resource, 0);
  }
}
void StartGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto query_index = render_pass_index_per_queue[render_pass_index] * kGpuTimestampQueryNumPerPass;
  command_list->EndQuery(gpu_timestamp_set->timestamp_query_heaps[render_pass_queue_index[render_pass_index]], D3D12_QUERY_TYPE_TIMESTAMP, query_index);
}
void EndGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto query_index = render_pass_index_per_queue[render_pass_index] * kGpuTimestampQueryNumPerPass;
  command_list->EndQuery(gpu_timestamp_set->timestamp_query_heaps[render_pass_queue_index[render_pass_index]], D3D12_QUERY_TYPE_TIMESTAMP, query_index + 1); // end
}
} // namespace illuminate
