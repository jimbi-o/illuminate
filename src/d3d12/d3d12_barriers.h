#ifndef ILLUMINATE_D3D12_BARRIES_H
#define ILLUMINATE_D3D12_BARRIES_H
#include "illuminate/util/util_defines.h"
#include "d3d12_header_common.h"
namespace illuminate {
enum class MemoryType : uint8_t;
struct BarrierConfig {
  uint32_t buffer_allocation_index{};
  D3D12_RESOURCE_BARRIER_TYPE type{};
  D3D12_RESOURCE_BARRIER_FLAGS flag{}; // split begin/end/none
  D3D12_RESOURCE_STATES state_before{};
  D3D12_RESOURCE_STATES state_after{};
};
static const uint32_t kBarrierExecutionTimingNum = 2;
// retval: barrier_config_list[pass_index][timing(0/1)]
using BarrierConfigList = ArrayOf<BarrierConfig>*;
struct BarrierTransitionInfo {
  const BarrierConfigList * barrier_config_list;
  const ResourceStateTypeFlags::FlagType* state_at_frame_end;
};
BarrierTransitionInfo ConfigureBarrierTransitions(const uint32_t buffer_num, const uint32_t render_pass_num, const uint32_t* render_pass_buffer_num,
                                                  const uint32_t* const * render_pass_buffer_allocation_index_list,
                                                  const ResourceStateTypeFlags::FlagType* const * render_pass_resource_state_list,
                                                  const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                                  const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                                  const ResourceStateTypeFlags::FlagType* initial_state, const ResourceStateTypeFlags::FlagType* final_state,
                                                  const MemoryType& memory_type);
}
#endif
