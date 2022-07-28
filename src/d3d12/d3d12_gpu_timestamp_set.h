#ifndef ILLUMINATE_D3D12_GPU_TIMESTAMP_SET_H
#define ILLUMINATE_D3D12_GPU_TIMESTAMP_SET_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
namespace illuminate {
struct GpuTimestampSet {
  float* gpu_timestamp_frequency_inv{};
  ID3D12QueryHeap** timestamp_query_heaps{};
  ID3D12Resource*** timestamp_query_dst_resource{};
  D3D12MA::Allocation*** timestamp_query_dst_allocation{};
  uint32_t timestamp_query_dst_resource_index{0};
  uint64_t** timestamp_value{};
  uint64_t* timestamp_prev_end_value{};
};
GpuTimestampSet CreateGpuTimestampSet(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, D3d12Device* device, D3D12MA::Allocator* buffer_allocator);
void ReleaseGpuTimestampSet(const uint32_t command_queue_num, GpuTimestampSet* gpu_timestamp_set);
void StartGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
void EndGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
void OutputGpuTimestampToCpuVisibleBuffer(const uint32_t * const render_pass_num_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
struct GpuTimeDurations {
  uint32_t command_queue_num{};
  float total_time_msec{};
  uint32_t* duration_num{};
  float** duration_msec{};
};
GpuTimeDurations GetEmptyGpuTimeDurations(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, const MemoryType& memory_type);
GpuTimeDurations GetGpuTimeDurationsPerFrame(const uint32_t command_queue_num, const uint32_t * const render_pass_num_per_queue, const GpuTimestampSet& gpu_timestamp_set, const MemoryType& memory_type);
void AccumulateGpuTimeDuration(const GpuTimeDurations& per_frame, GpuTimeDurations* accumulated);
void CalcAvarageGpuTimeDuration(const GpuTimeDurations& accumulated, const uint32_t frame_num, GpuTimeDurations* average);
void ClearGpuTimeDuration(GpuTimeDurations* gpu_time_durations);
}
#endif
