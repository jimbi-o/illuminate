#ifndef ILLUMINATE_D3D12_BARRIES_H
#define ILLUMINATE_D3D12_BARRIES_H
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
struct BarrierTransitions {
  uint32_t** barrier_num;
  BarrierConfig*** barrier_config_list;
};
/*
  ExecuteBarrier(command_list, barrier_num[k][0], barrier_config_list[k][0], barrier_resource_list[k][0]);
  ExecuteBarrier(command_list, barrier_num[k][1], barrier_config_list[k][1], barrier_resource_list[k][1]);
*/
BarrierTransitions ConfigureBarrierTransitions(const uint32_t buffer_num, const uint32_t render_pass_num, const uint32_t* render_pass_buffer_num,
                                               const uint32_t* const * render_pass_buffer_allocation_index_list, const ResourceStateType* const * render_pass_resource_state_list,
                                               const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                               const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                               const uint32_t buffer_num_with_initial_state, const uint32_t* const buffer_allocation_index_with_initial_state, const ResourceStateType* initial_state,
                                               const uint32_t buffer_num_with_final_state, const uint32_t* const buffer_allocation_index_with_final_state, const ResourceStateType* final_state,
                                               const MemoryType& memory_type);
static const uint32_t kBarrierExecutionTimingNum = 2;
}
#endif
