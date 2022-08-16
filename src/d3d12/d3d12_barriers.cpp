#include "d3d12_barriers.h"
#include "d3d12_scene.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
auto FindBufferIndex(const uint32_t num, const uint32_t* buffer_id_list, const uint32_t buffer_id) {
  for (uint32_t i = 0; i < num; i++) {
    if (buffer_id_list[i] == buffer_id) { return i; }
  }
  return ~0U;
}
auto GetPrevState(const uint32_t num, const uint32_t* buffer_id_list, const ResourceStateType* states, const uint32_t buffer_id) {
  for (uint32_t i = 0; i < num; i++) {
    if (buffer_id_list[i] == buffer_id) {
      return states[i];
    }
  }
  logerror("no valid bufferid found {} {}", num, buffer_id);
  assert(false && "no valid bufferid found");
  return ResourceStateType::kCommon;
}
auto IsResourceStateValidQueue(const D3D12_COMMAND_LIST_TYPE command_queue_type, const ResourceStateType state) {
  switch (command_queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT: {
      return true;
    }
    case D3D12_COMMAND_LIST_TYPE_COMPUTE: {
      switch (state) {
        case ResourceStateType::kSrvPs:
        case ResourceStateType::kRtv:
        case ResourceStateType::kDsvWrite:
        case ResourceStateType::kDsvRead:  {
          return false;
        }
      }
      return true;
    }
    case D3D12_COMMAND_LIST_TYPE_COPY: {
      return false;
    }
  }
  return false;
}
auto GetValidRenderPassForResourceStates(const uint32_t pass_index, const ResourceStateType prev_state, const ResourceStateType next_state,
                                         const uint32_t* wait_pass_num, const uint32_t* const* signal_pass_index,
                                         const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type) {
  const auto command_queue_index = render_pass_command_queue_index[pass_index];
  if (command_queue_type[command_queue_index] == D3D12_COMMAND_LIST_TYPE_DIRECT) {
    return std::make_pair(pass_index, 0);
  }
  if (IsResourceStateValidQueue(command_queue_type[command_queue_index], prev_state) && IsResourceStateValidQueue(command_queue_type[command_queue_index], next_state)) {
    return std::make_pair(pass_index, 0);
  }
  for (uint32_t i = pass_index; i != ~0U; i--) {
    if (command_queue_index != render_pass_command_queue_index[i]) { continue; }
    for (uint32_t j = 0; j < wait_pass_num[i]; j++) {
      const auto signal_queue = command_queue_type[render_pass_command_queue_index[signal_pass_index[i][j]]];
      if (!IsResourceStateValidQueue(signal_queue, prev_state)) { continue; }
      if (!IsResourceStateValidQueue(signal_queue, next_state)) { continue; }
      return std::make_pair(signal_pass_index[i][j], 1);
    }
  }
  logerror("no valid pass for barrier found (wrong sync settings?). {} {} {} {}", pass_index, command_queue_index, prev_state, next_state);
  assert(false && "no valid pass for barrier found");
  return std::make_pair(pass_index, 0);
}
auto GetValidRenderPassForResourceStatesBeforeFrameEnd(const uint32_t pass_index, const D3D12_COMMAND_LIST_TYPE command_queue_type, const ResourceStateType prev_state, const ResourceStateType next_state, const uint32_t last_graphics_queue_render_pass_index) {
  if (command_queue_type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
    return std::make_pair(pass_index, 1);
  }
  if (IsResourceStateValidQueue(command_queue_type, prev_state) && IsResourceStateValidQueue(command_queue_type, next_state)) {
    return std::make_pair(pass_index, 1);
  }
  return std::make_pair(last_graphics_queue_render_pass_index, 1);
}
enum class BarrierFillMode :uint8_t { kNumOnly, kFillParams, };
template <BarrierFillMode fill_mode>
auto ConfigureBarrierTransitions(const uint32_t buffer_allocation_num, const uint32_t render_pass_num, const uint32_t* render_pass_buffer_num,
                                 const uint32_t* const * render_pass_buffer_allocation_index_list, const ResourceStateType* const * render_pass_resource_state_list,
                                 const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                 const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                 const uint32_t buffer_num_with_initial_state, const uint32_t* const buffer_allocation_index_with_initial_state, const ResourceStateType* initial_state,
                                 const uint32_t buffer_num_with_final_state, const uint32_t* const buffer_allocation_index_with_final_state, const ResourceStateType* final_state,
                                 const MemoryType& memory_type, uint32_t** barrier_num, BarrierConfig*** barrier_config_list) {
  auto buffer_id_list = AllocateArray<uint32_t>(memory_type, buffer_allocation_num);
  auto buffer_initial_state = AllocateArray<ResourceStateType>(memory_type, buffer_allocation_num);
  auto buffer_prev_state = AllocateArray<ResourceStateType>(memory_type, buffer_allocation_num);
  auto last_user_pass = AllocateArray<uint32_t>(memory_type, buffer_allocation_num);
  auto barrier_index = AllocateArrayFrame<uint32_t*>(render_pass_num);
  uint32_t next_vacant_buffer_index = 0;
  for (uint32_t i = 0; i < buffer_num_with_initial_state; i++) {
    buffer_id_list[next_vacant_buffer_index]       = buffer_allocation_index_with_initial_state[i];
    buffer_initial_state[next_vacant_buffer_index] = initial_state[i];
    buffer_prev_state[next_vacant_buffer_index]    = initial_state[i];
    last_user_pass[next_vacant_buffer_index]       = ~0U;
    next_vacant_buffer_index++;
  }
  auto last_graphics_queue_render_pass_index = render_pass_num - 1;
  for (uint32_t i = 0; i < render_pass_num; i++) {
    if (command_queue_type[render_pass_command_queue_index[i]] == D3D12_COMMAND_LIST_TYPE_DIRECT) {
      last_graphics_queue_render_pass_index = i;
    }
    barrier_index[i] = AllocateArrayFrame<uint32_t>(kBarrierExecutionTimingNum);
    barrier_index[i][0] = 0;
    barrier_index[i][1] = 0;
    for (uint32_t j = 0; j < render_pass_buffer_num[i]; j++) {
      const auto buffer_id = render_pass_buffer_allocation_index_list[i][j];
      if (IsSceneBuffer(buffer_id)) { continue; }
      auto buffer_index = FindBufferIndex(next_vacant_buffer_index, buffer_id_list, buffer_id);
      if (buffer_index == ~0U) {
        buffer_id_list[next_vacant_buffer_index]       = buffer_id;
        buffer_initial_state[next_vacant_buffer_index] = render_pass_resource_state_list[i][j];
        buffer_prev_state[next_vacant_buffer_index]    = render_pass_resource_state_list[i][j];
        last_user_pass[next_vacant_buffer_index]       = i;
        next_vacant_buffer_index++;
        continue;
      }
      last_user_pass[buffer_index] = i;
      auto prev_state = GetPrevState(next_vacant_buffer_index, buffer_id_list, buffer_prev_state, buffer_id);
      if (prev_state == render_pass_resource_state_list[i][j]) { continue; }
      auto next_state = render_pass_resource_state_list[i][j];
      auto [barrier_valid_pass_index, per_pass_barrier_position_index] = GetValidRenderPassForResourceStates(i, prev_state, next_state,
                                                                                                             wait_pass_num, signal_pass_index,
                                                                                                             render_pass_command_queue_index, command_queue_type);
      if constexpr (fill_mode == BarrierFillMode::kFillParams) {
        auto& barrier = barrier_config_list[barrier_valid_pass_index][per_pass_barrier_position_index][barrier_index[barrier_valid_pass_index][per_pass_barrier_position_index]];
        barrier.buffer_allocation_index = buffer_id;
        barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.state_before = ConvertToD3d12ResourceState(prev_state);
        barrier.state_after  = ConvertToD3d12ResourceState(next_state);
      }
      buffer_prev_state[buffer_index] = next_state;
      barrier_index[barrier_valid_pass_index][per_pass_barrier_position_index]++;
    }
  }
  for (uint32_t i = 0; i < next_vacant_buffer_index; i++) {
    const auto buffer_id = buffer_id_list[i];
    const auto prev_state = GetPrevState(next_vacant_buffer_index, buffer_id_list, buffer_prev_state, buffer_id);
    const auto pass_index = last_user_pass[i];
    auto next_state = buffer_initial_state[i];
    for (uint32_t j = 0; j < buffer_num_with_final_state; j++) {
      if (buffer_id != buffer_allocation_index_with_final_state[j]) { continue; }
      next_state = final_state[j];
      break;
    }
    if (prev_state == next_state) { continue; }
    auto [barrier_valid_pass_index, per_pass_barrier_position_index] = GetValidRenderPassForResourceStatesBeforeFrameEnd(pass_index, command_queue_type[render_pass_command_queue_index[pass_index]],
                                                                                                                         prev_state, next_state,
                                                                                                                         last_graphics_queue_render_pass_index);
    if constexpr (fill_mode == BarrierFillMode::kFillParams) {
      auto& barrier = barrier_config_list[barrier_valid_pass_index][per_pass_barrier_position_index][barrier_index[barrier_valid_pass_index][per_pass_barrier_position_index]];
      barrier.buffer_allocation_index = buffer_id;
      barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.state_before = ConvertToD3d12ResourceState(prev_state);
      barrier.state_after  = ConvertToD3d12ResourceState(next_state);
    }
    barrier_index[barrier_valid_pass_index][per_pass_barrier_position_index]++;
  }
  if constexpr (fill_mode == BarrierFillMode::kNumOnly) {
    for (uint32_t i = 0; i < render_pass_num; i++) {
      barrier_num[i] = AllocateArray<uint32_t>(memory_type, kBarrierExecutionTimingNum);
      barrier_num[i][0] = barrier_index[i][0];
      barrier_num[i][1] = barrier_index[i][1];
    }
  }
}
} // namespace
BarrierTransitions ConfigureBarrierTransitions(const uint32_t buffer_num, const uint32_t render_pass_num, const uint32_t* render_pass_buffer_num,
                                               const uint32_t* const * render_pass_buffer_allocation_index_list, const ResourceStateType* const * render_pass_resource_state_list,
                                               const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                               const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                               const uint32_t buffer_num_with_initial_state, const uint32_t* const buffer_allocation_index_with_initial_state, const ResourceStateType* initial_state,
                                               const uint32_t buffer_num_with_final_state, const uint32_t* const buffer_allocation_index_with_final_state, const ResourceStateType* final_state, 
                                               const MemoryType& memory_type) {
  auto barrier_num = AllocateArray<uint32_t*>(memory_type, render_pass_num);
  ConfigureBarrierTransitions<BarrierFillMode::kNumOnly>(buffer_num, render_pass_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list,
                                                         wait_pass_num, signal_pass_index,
                                                         render_pass_command_queue_index, command_queue_type,
                                                         buffer_num_with_initial_state, buffer_allocation_index_with_initial_state, initial_state,
                                                         buffer_num_with_final_state, buffer_allocation_index_with_final_state, final_state,
                                                         MemoryType::kFrame, barrier_num, nullptr);
  auto barrier_config_list = AllocateArray<BarrierConfig**>(memory_type, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    barrier_config_list[i] = AllocateArray<BarrierConfig*>(memory_type, kBarrierExecutionTimingNum);
    barrier_config_list[i][0] = AllocateArray<BarrierConfig>(memory_type, barrier_num[i][0]);
    barrier_config_list[i][1] = AllocateArray<BarrierConfig>(memory_type, barrier_num[i][1]);
  }
  ConfigureBarrierTransitions<BarrierFillMode::kFillParams>(buffer_num, render_pass_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list,
                                                            wait_pass_num, signal_pass_index,
                                                            render_pass_command_queue_index, command_queue_type,
                                                            buffer_num_with_initial_state, buffer_allocation_index_with_initial_state, initial_state,
                                                            buffer_num_with_final_state, buffer_allocation_index_with_final_state, final_state,
                                                            MemoryType::kFrame, barrier_num, barrier_config_list);
  return {barrier_num, barrier_config_list};
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("barrier/swapchain") {
  using namespace illuminate;
  const uint32_t buffer_num = 1;
  const uint32_t render_pass_num = 1;
  const uint32_t render_pass_buffer_num[render_pass_num] = {1};
  const uint32_t render_pass_buffer_allocation_index_list_pass_0[] = {99};
  const uint32_t* render_pass_buffer_allocation_index_list[render_pass_num] = {
    render_pass_buffer_allocation_index_list_pass_0,
  };
  const ResourceStateType render_pass_resource_state_list_pass_0[] = {ResourceStateType::kRtv};
  const ResourceStateType* render_pass_resource_state_list[render_pass_num] = {
    render_pass_resource_state_list_pass_0,
  };
  const uint32_t buffer_num_with_initial_state = 1;
  const uint32_t buffer_allocation_index_with_initial_state[buffer_num_with_initial_state] = {99};
  const ResourceStateType initial_state[buffer_num_with_initial_state] = {ResourceStateType::kPresent};
  const uint32_t buffer_num_with_final_state = 1;
  const uint32_t buffer_allocation_index_with_final_state[buffer_num_with_final_state] = {99};
  const ResourceStateType final_state[buffer_num_with_final_state] = {ResourceStateType::kPresent};
  const uint32_t wait_pass_num[] = {0};
  const uint32_t render_pass_command_queue_index[] = {0};
  const D3D12_COMMAND_LIST_TYPE command_queue_type[] = { D3D12_COMMAND_LIST_TYPE_DIRECT };
  auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(buffer_num, render_pass_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list,
                                                                        wait_pass_num, nullptr, render_pass_command_queue_index, command_queue_type,
                                                                        buffer_num_with_initial_state, buffer_allocation_index_with_initial_state, initial_state,
                                                                        buffer_num_with_final_state, buffer_allocation_index_with_final_state, final_state,
                                                                        MemoryType::kFrame);
  CHECK_EQ(barrier_num[0][0], 1);
  CHECK_EQ(barrier_config_list[0][0][0].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[0][0][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][0][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][0][0].state_before, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[0][0][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_num[0][1], 1);
  CHECK_EQ(barrier_config_list[0][1][0].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[0][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][0].state_after, D3D12_RESOURCE_STATE_PRESENT);
  ClearAllAllocations();
}
TEST_CASE("barrier/simple") {
  using namespace illuminate;
  const uint32_t buffer_num = 2;
  const uint32_t render_pass_num = 2;
  const uint32_t render_pass_buffer_num[render_pass_num] = {1, 2};
  const uint32_t render_pass_buffer_allocation_index_list_pass_0[] = {35};
  const uint32_t render_pass_buffer_allocation_index_list_pass_1[] = {35, 99};
  const uint32_t* render_pass_buffer_allocation_index_list[render_pass_num] = {
    render_pass_buffer_allocation_index_list_pass_0,
    render_pass_buffer_allocation_index_list_pass_1,
  };
  const ResourceStateType render_pass_resource_state_list_pass_0[] = {ResourceStateType::kRtv};
  const ResourceStateType render_pass_resource_state_list_pass_1[] = {ResourceStateType::kSrvPs, ResourceStateType::kRtv};
  const ResourceStateType* render_pass_resource_state_list[render_pass_num] = {
    render_pass_resource_state_list_pass_0,
    render_pass_resource_state_list_pass_1,
  };
  const uint32_t buffer_num_with_initial_state = 1;
  const uint32_t buffer_allocation_index_with_initial_state[buffer_num_with_initial_state] = {99};
  const ResourceStateType initial_state[buffer_num_with_initial_state] = {ResourceStateType::kPresent};
  const uint32_t buffer_num_with_final_state = 1;
  const uint32_t buffer_allocation_index_with_final_state[buffer_num_with_final_state] = {99};
  const ResourceStateType final_state[buffer_num_with_final_state] = {ResourceStateType::kPresent};
  const uint32_t wait_pass_num[] = {0,0};
  const uint32_t render_pass_command_queue_index[] = {0,0};
  const D3D12_COMMAND_LIST_TYPE command_queue_type[] = { D3D12_COMMAND_LIST_TYPE_DIRECT };
  auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(buffer_num, render_pass_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list,
                                                                        wait_pass_num, nullptr, render_pass_command_queue_index, command_queue_type,
                                                                        buffer_num_with_initial_state, buffer_allocation_index_with_initial_state, initial_state,
                                                                        buffer_num_with_final_state, buffer_allocation_index_with_final_state, final_state,
                                                                        MemoryType::kFrame);
  CHECK_EQ(barrier_num[0][0], 0);
  CHECK_EQ(barrier_num[0][1], 0);
  CHECK_EQ(barrier_num[1][0], 2);
  CHECK_EQ(barrier_config_list[1][0][0].buffer_allocation_index, 35);
  CHECK_EQ(barrier_config_list[1][0][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][0][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][0][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][0][0].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][0][1].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[1][0][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][0][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][0][1].state_before, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[1][0][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_num[1][1], 2);
  CHECK_EQ(barrier_config_list[1][1][0].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[1][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][1][0].state_after, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[1][1][1].buffer_allocation_index, 35);
  CHECK_EQ(barrier_config_list[1][1][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][1].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][1][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  ClearAllAllocations();
}
TEST_CASE("barrier/pinpong") {
  using namespace illuminate;
  const uint32_t buffer_num = 7;
  const uint32_t render_pass_num = 3;
  const uint32_t render_pass_buffer_num[render_pass_num] = {5, 5, 6};
  const uint32_t render_pass_buffer_allocation_index_list_pass_0[] = {35, 18, 14, 21, 11};
  const uint32_t render_pass_buffer_allocation_index_list_pass_1[] = {18, 29, 35, 14, 11};
  const uint32_t render_pass_buffer_allocation_index_list_pass_2[] = {21, 18, 29, 99, 14, 11};
  const uint32_t* render_pass_buffer_allocation_index_list[render_pass_num] = {
    render_pass_buffer_allocation_index_list_pass_0,
    render_pass_buffer_allocation_index_list_pass_1,
    render_pass_buffer_allocation_index_list_pass_2,
  };
  const ResourceStateType render_pass_resource_state_list_pass_0[] = {ResourceStateType::kRtv, ResourceStateType::kRtv, ResourceStateType::kRtv, ResourceStateType::kRtv, ResourceStateType::kRtv};
  const ResourceStateType render_pass_resource_state_list_pass_1[] = {ResourceStateType::kSrvPs, ResourceStateType::kRtv, ResourceStateType::kSrvPs, ResourceStateType::kRtv, ResourceStateType::kSrvPs};
  const ResourceStateType render_pass_resource_state_list_pass_2[] = {ResourceStateType::kSrvPs, ResourceStateType::kSrvPs, ResourceStateType::kSrvPs, ResourceStateType::kRtv, ResourceStateType::kSrvPs, ResourceStateType::kRtv};
  const ResourceStateType* render_pass_resource_state_list[render_pass_num] = {
    render_pass_resource_state_list_pass_0,
    render_pass_resource_state_list_pass_1,
    render_pass_resource_state_list_pass_2,
  };
  const uint32_t buffer_num_with_initial_state = 1;
  const uint32_t buffer_allocation_index_with_initial_state[buffer_num_with_initial_state] = {99};
  const ResourceStateType initial_state[buffer_num_with_initial_state] = {ResourceStateType::kPresent};
  const uint32_t buffer_num_with_final_state = 1;
  const uint32_t buffer_allocation_index_with_final_state[buffer_num_with_final_state] = {99};
  const ResourceStateType final_state[buffer_num_with_final_state] = {ResourceStateType::kPresent};
  const uint32_t wait_pass_num[] = {0,0,0};
  const uint32_t render_pass_command_queue_index[] = {0,0,0};
  const D3D12_COMMAND_LIST_TYPE command_queue_type[] = { D3D12_COMMAND_LIST_TYPE_DIRECT };
  auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(buffer_num, render_pass_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list,
                                                                        wait_pass_num, nullptr, render_pass_command_queue_index, command_queue_type,
                                                                        buffer_num_with_initial_state, buffer_allocation_index_with_initial_state, initial_state,
                                                                        buffer_num_with_final_state, buffer_allocation_index_with_final_state, final_state,
                                                                        MemoryType::kFrame);
  CHECK_EQ(barrier_num[0][0], 0);
  CHECK_EQ(barrier_num[0][1], 0);
  CHECK_EQ(barrier_num[1][0], 3);
  CHECK_EQ(barrier_config_list[1][0][0].buffer_allocation_index, 18);
  CHECK_EQ(barrier_config_list[1][0][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][0][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][0][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][0][0].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][0][1].buffer_allocation_index, 35);
  CHECK_EQ(barrier_config_list[1][0][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][0][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][0][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][0][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][0][2].buffer_allocation_index, 11);
  CHECK_EQ(barrier_config_list[1][0][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][0][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][0][2].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][0][2].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_num[1][1], 1);
  CHECK_EQ(barrier_config_list[1][1][0].buffer_allocation_index, 35);
  CHECK_EQ(barrier_config_list[1][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][0].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][1][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_num[2][0], 5);
  CHECK_EQ(barrier_config_list[2][0][0].buffer_allocation_index, 21);
  CHECK_EQ(barrier_config_list[2][0][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][0][0].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][0][1].buffer_allocation_index, 29);
  CHECK_EQ(barrier_config_list[2][0][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][0][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][0][2].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[2][0][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0][2].state_before, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[2][0][2].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][0][3].buffer_allocation_index, 14);
  CHECK_EQ(barrier_config_list[2][0][3].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0][3].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0][3].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][0][3].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][0][4].buffer_allocation_index, 11);
  CHECK_EQ(barrier_config_list[2][0][4].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0][4].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0][4].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][0][4].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_num[2][1], 5);
  CHECK_EQ(barrier_config_list[2][1][0].buffer_allocation_index, 99);
  CHECK_EQ(barrier_config_list[2][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][1][0].state_after, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[2][1][1].buffer_allocation_index, 18);
  CHECK_EQ(barrier_config_list[2][1][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1][1].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][1][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][1][2].buffer_allocation_index, 14);
  CHECK_EQ(barrier_config_list[2][1][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1][2].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][1][2].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][1][3].buffer_allocation_index, 21);
  CHECK_EQ(barrier_config_list[2][1][3].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1][3].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1][3].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][1][3].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[2][1][4].buffer_allocation_index, 29);
  CHECK_EQ(barrier_config_list[2][1][4].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1][4].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1][4].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[2][1][4].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  ClearAllAllocations();
}
