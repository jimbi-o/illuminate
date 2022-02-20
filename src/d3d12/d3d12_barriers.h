#ifndef ILLUMINATE_D3D12_BARRIES_H
#define ILLUMINATE_D3D12_BARRIES_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_render_graph.h"
namespace illuminate {
const ResourceStateType*** ConfigureRenderPassResourceStates(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const bool** pingpong_buffer_write_to_sub_list, const bool* render_pass_enable_flag, MemoryAllocationJanitor* allocator);
std::tuple<const uint32_t**, const Barrier***> ConfigureBarrierTransitions(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const ResourceStateType*** resource_state_list, MemoryAllocationJanitor* allocator);
}
#endif
