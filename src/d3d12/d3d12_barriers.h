#ifndef ILLUMINATE_D3D12_BARRIES_H
#define ILLUMINATE_D3D12_BARRIES_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_render_graph.h"
namespace illuminate {
struct RenderPassBufferState {
  uint32_t pass_index{};
  uint32_t buffer_config_index;
  ResourceStateType state_type{};
};
using ResourceStateTypePerPass = ResourceStateType* const* const; // https://isocpp.org/wiki/faq/const-correctness#constptrptr-conversion
struct RenderPassResourceState {
  const ResourceStateTypePerPass* resource_state_list;
  const uint32_t* const* last_user_pass;
};
struct BarrierTransitions {
  const uint32_t* const* barrier_num;
  const Barrier* const* const* barrier_config_list;
};
RenderPassResourceState ConfigureRenderPassResourceStates(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const bool*const* pingpong_buffer_write_to_sub_list, const bool* render_pass_enable_flag, const MemoryType retval_memory_type);
BarrierTransitions ConfigureBarrierTransitions(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const ResourceStateTypePerPass* resource_state_list, const uint32_t* const * const last_user_pass, const MemoryType retval_memory_type);
}
#endif
