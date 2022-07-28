#include "d3d12_gpu_timestamp_set.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/UWP/D3D12xGPU/src/GPUTimer.cpp
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
      continue;
    }
    SetD3d12Name(timestamp_query_heaps[i], "timestamp" + std::to_string(i));
  }
  return timestamp_query_heaps;
}
auto CreateTimestampQuertyDstResource(const uint32_t command_queue_num, const uint32_t* render_pass_num_per_queue, D3D12MA::Allocator* buffer_allocator) {
  auto timestamp_query_dst_resource = AllocateArraySystem<ID3D12Resource**>(command_queue_num);
  auto timestamp_query_dst_allocation = AllocateArraySystem<D3D12MA::Allocation**>(command_queue_num);
  auto timestamp_value = AllocateArraySystem<uint64_t*>(command_queue_num);
  auto timestamp_prev_end_value = AllocateArraySystem<uint64_t>(command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (render_pass_num_per_queue[i] == 0) {
      timestamp_query_dst_resource[i]  = nullptr;
      timestamp_query_dst_allocation[i] = nullptr;
      timestamp_value[i] = nullptr;
      continue;
    }
    timestamp_query_dst_resource[i]  = AllocateArraySystem<ID3D12Resource*>(kGpuTimestampDstResourceRingBufferNum);
    timestamp_query_dst_allocation[i] = AllocateArraySystem<D3D12MA::Allocation*>(kGpuTimestampDstResourceRingBufferNum);
    const auto timestamp_num = render_pass_num_per_queue[i] * kGpuTimestampQueryNumPerPass;
    const auto buffer_size = GetUint32(sizeof(uint64_t)) * timestamp_num;
    const auto desc = GetBufferDesc(buffer_size);
    for (uint32_t j = 0; j < kGpuTimestampDstResourceRingBufferNum; j++) {
      CreateBuffer(D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, desc, nullptr, buffer_allocator, &timestamp_query_dst_allocation[i][j], &timestamp_query_dst_resource[i][j]);
      SetD3d12Name(timestamp_query_dst_resource[i][j], "timestamp_resource_" + std::to_string(i) + "_" + std::to_string(j));
    }
    timestamp_value[i] = AllocateArraySystem<uint64_t>(timestamp_num);
  }
  return std::make_tuple(timestamp_query_dst_resource, timestamp_query_dst_allocation, timestamp_value, timestamp_prev_end_value);
}
auto GetGpuTimestampFrequency(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list) {
  const float sec_to_msec = 1000.0f;
  auto gpu_timestamp_frequency_inv = AllocateArraySystem<float>(command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    uint64_t frequecy{};
    command_queue_list[i]->GetTimestampFrequency(&frequecy);
    gpu_timestamp_frequency_inv[i] = sec_to_msec / static_cast<float>(frequecy);
  }
  return gpu_timestamp_frequency_inv;
}
void CopyTimestampToCpuCachedMemory(const uint32_t * const render_pass_num_per_queue, GpuTimestampSet* gpu_timestamp_set, const uint32_t command_queue_index) {
  if (render_pass_num_per_queue[command_queue_index] == 0) { return; }
  const auto render_pass_num = render_pass_num_per_queue[command_queue_index] * kGpuTimestampQueryNumPerPass;
  const auto buffer_size = GetUint32(sizeof(uint64_t) * render_pass_num);
  gpu_timestamp_set->timestamp_prev_end_value[command_queue_index] = gpu_timestamp_set->timestamp_value[command_queue_index][render_pass_num - 1];
  auto resource = gpu_timestamp_set->timestamp_query_dst_resource[command_queue_index][gpu_timestamp_set->timestamp_query_dst_resource_index];
  auto ptr = MapResource(resource, buffer_size, 0, buffer_size);
  memcpy(gpu_timestamp_set->timestamp_value[command_queue_index], ptr, buffer_size);
  UnmapResource(resource);
}
} // namespace
GpuTimestampSet CreateGpuTimestampSet(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, D3d12Device* device, D3D12MA::Allocator* buffer_allocator) {
  auto gpu_timestamp_frequency_inv = GetGpuTimestampFrequency(command_queue_num, command_queue_list);
  auto timestamp_query_heaps = CreateTimestampQueryHeaps(command_queue_num, command_queue_type, render_pass_num_per_queue, device);
  auto [timestamp_query_dst_resource, timestamp_query_dst_allocation, timestamp_value, timestamp_prev_end_value] = CreateTimestampQuertyDstResource(command_queue_num, render_pass_num_per_queue, buffer_allocator);
  return {
    .gpu_timestamp_frequency_inv = gpu_timestamp_frequency_inv,
    .timestamp_query_heaps = timestamp_query_heaps,
    .timestamp_query_dst_resource = timestamp_query_dst_resource,
    .timestamp_query_dst_allocation = timestamp_query_dst_allocation,
    .timestamp_value = timestamp_value,
    .timestamp_prev_end_value = timestamp_prev_end_value,
  };
}
void ReleaseGpuTimestampSet(const uint32_t command_queue_num, GpuTimestampSet* gpu_timestamp_set) {
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (gpu_timestamp_set->timestamp_query_dst_resource[i] == nullptr) { continue; }
    for (uint32_t j = 0; j < kGpuTimestampDstResourceRingBufferNum; j++) {
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
void OutputGpuTimestampToCpuVisibleBuffer(const uint32_t * const render_pass_num_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto command_queue_index = render_pass_queue_index[render_pass_index];
  CopyTimestampToCpuCachedMemory(render_pass_num_per_queue, gpu_timestamp_set, command_queue_index);
  auto dst_resource = gpu_timestamp_set->timestamp_query_dst_resource[command_queue_index][gpu_timestamp_set->timestamp_query_dst_resource_index];
  gpu_timestamp_set->timestamp_query_dst_resource_index++;
  if (gpu_timestamp_set->timestamp_query_dst_resource_index >= kGpuTimestampDstResourceRingBufferNum) {
    gpu_timestamp_set->timestamp_query_dst_resource_index = 0;
  }
  if (!IsDebuggerPresent() || command_queue_index == 0) {
    // graphics debugger issues an error:
    // D3D12 ERROR: ID3D12GraphicsCommandList::SetComputeRootSignature: Invalid API called.  This method is not valid for D3D12_COMMAND_LIST_TYPE_COPY. [ EXECUTION ERROR #933: NO_COMPUTE_API_SUPPORT]
    const auto query_num = render_pass_num_per_queue[command_queue_index] * kGpuTimestampQueryNumPerPass;
    command_list->ResolveQueryData(gpu_timestamp_set->timestamp_query_heaps[command_queue_index], D3D12_QUERY_TYPE_TIMESTAMP, 0, query_num, dst_resource, 0);
  }
}
void StartGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto query_index = render_pass_index_per_queue[render_pass_index] * kGpuTimestampQueryNumPerPass;
  command_list->EndQuery(gpu_timestamp_set->timestamp_query_heaps[render_pass_queue_index[render_pass_index]], D3D12_QUERY_TYPE_TIMESTAMP, query_index);
}
void EndGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list) {
  const auto query_index = render_pass_index_per_queue[render_pass_index] * kGpuTimestampQueryNumPerPass;
  command_list->EndQuery(gpu_timestamp_set->timestamp_query_heaps[render_pass_queue_index[render_pass_index]], D3D12_QUERY_TYPE_TIMESTAMP, query_index + 1);
}
GpuTimeDurations GetGpuTimeDurationsPerFrame(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, const GpuTimestampSet& gpu_timestamp_set, const MemoryType& memory_type) {
  auto gpu_time_durations = GetEmptyGpuTimeDurations(command_queue_num, render_pass_num_per_queue, memory_type);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (gpu_time_durations.duration_num[i] == 0) { continue; }
    {
      const auto diff = gpu_timestamp_set.timestamp_value[i][0] - gpu_timestamp_set.timestamp_prev_end_value[i];
      gpu_time_durations.duration_msec[i][0] = diff * gpu_timestamp_set.gpu_timestamp_frequency_inv[i];
    }
    for (uint32_t j = 1; j < gpu_time_durations.duration_num[i]; j++) {
      const auto diff = gpu_timestamp_set.timestamp_value[i][j] - gpu_timestamp_set.timestamp_value[i][j - 1];
      gpu_time_durations.duration_msec[i][j] = diff * gpu_timestamp_set.gpu_timestamp_frequency_inv[i];
    }
  }
  return gpu_time_durations;
}
GpuTimeDurations GetEmptyGpuTimeDurations(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, const MemoryType& memory_type) {
  GpuTimeDurations gpu_time_durations{
    .command_queue_num = command_queue_num,
    .total_time_msec   = 0.0f,
    .duration_num      = AllocateArray<uint32_t>(memory_type, command_queue_num),
    .duration_msec     = AllocateArray<float*>(memory_type, command_queue_num),
  };
  for (uint32_t i = 0; i < command_queue_num; i++) {
    const auto duration_num = render_pass_num_per_queue[i] * kGpuTimestampQueryNumPerPass;
    gpu_time_durations.duration_num[i] = duration_num;
    if (duration_num == 0) {
      gpu_time_durations.duration_msec[i] = nullptr;
      continue;
    }
    gpu_time_durations.duration_msec[i] = AllocateArray<float>(memory_type, duration_num);
    for (uint32_t j = 0; j < duration_num; j++) {
      gpu_time_durations.duration_msec[i][j] = 0.0f;
    }
  }
  return gpu_time_durations;
}
void AccumulateGpuTimeDuration(const GpuTimeDurations& per_frame, GpuTimeDurations* accumulated) {
  accumulated->total_time_msec += per_frame.total_time_msec;
  for (uint32_t i = 0; i < accumulated->command_queue_num; i++) {
    for (uint32_t j = 0; j < accumulated->duration_num[i]; j++) {
      accumulated->duration_msec[i][j] += per_frame.duration_msec[i][j];
    }
  }
}
void CalcAvarageGpuTimeDuration(const GpuTimeDurations& accumulated, const uint32_t frame_num, GpuTimeDurations* average) {
  const auto inv_frame_num = 1.0f / static_cast<float>(frame_num);
  average->total_time_msec *= inv_frame_num;
  for (uint32_t i = 0; i < average->command_queue_num; i++) {
    for (uint32_t j = 0; j < average->duration_num[i]; j++) {
      average->duration_msec[i][j] = accumulated.duration_msec[i][j] * inv_frame_num;
    }
  }
}
void ClearGpuTimeDuration(GpuTimeDurations* gpu_time_durations) {
  gpu_time_durations->total_time_msec = 0.0f;
  for (uint32_t i = 0; i < gpu_time_durations->command_queue_num; i++) {
    for (uint32_t j = 0; j < gpu_time_durations->duration_num[i]; j++) {
      gpu_time_durations->duration_msec[i][j] = 0.0f;
    }
  }
}
} // namespace illuminate
