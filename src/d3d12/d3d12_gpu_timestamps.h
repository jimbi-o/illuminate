#ifndef ILLUMINATE_D3D12_GPU_TIMESTAMPS_H
#define ILLUMINATE_D3D12_GPU_TIMESTAMPS_H
#include "d3d12_header_common.h"
namespace illuminate {
struct GpuTimestampSet {
  uint64_t* gpu_timestamp_frequency{};
  ID3D12QueryHeap** timestamp_query_heaps{};
  ID3D12Resource*** timestamp_query_dst_resource{};
  D3D12MA::Allocation*** timestamp_query_dst_allocation{};
  uint32_t timestamp_query_dst_resource_index{0};
};
GpuTimestampSet CreateGpuTimestampSet(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, D3d12Device* device, D3D12MA::Allocator* buffer_allocator);
void ReleaseGpuTimestampSet(const uint32_t command_queue_num, GpuTimestampSet* gpu_timestamp_set);
void StartGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
void EndGpuTimestamp(const uint32_t * const render_pass_index_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
void OutputGpuTimestampToCpuVisibleBuffer(const uint32_t * const render_pass_num_per_queue, const uint32_t* const render_pass_queue_index, const uint32_t render_pass_index, GpuTimestampSet* gpu_timestamp_set, D3d12CommandList* command_list);
}
#endif
