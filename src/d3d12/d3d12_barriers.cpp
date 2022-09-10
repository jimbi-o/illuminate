#include "d3d12_barriers.h"
#include <bit>
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
struct ResourceStateTransitionInfo {
  uint32_t pass{};
  uint32_t timing{};
  ResourceStateTypeFlags::FlagType state_before{ResourceStateTypeFlags::kNone};
  ResourceStateTypeFlags::FlagType state_after{ResourceStateTypeFlags::kNone};
};
auto CollectStateTransitionInfo(const uint32_t render_pass_num,
                                const uint32_t buffer_allocation_num,
                                const uint32_t* render_pass_buffer_num,
                                const uint32_t * const * render_pass_buffer_allocation_index_list,
                                const ResourceStateTypeFlags::FlagType* const * render_pass_resource_state_list,
                                const ResourceStateTypeFlags::FlagType* initial_state,
                                const ResourceStateTypeFlags::FlagType* final_state) {
  auto prev_state = AllocateArrayFrame<ResourceStateTypeFlags::FlagType>(buffer_allocation_num);
  auto merged_state = AllocateArrayFrame<ResourceStateTypeFlags::FlagType>(buffer_allocation_num);
  auto initial_user_pass = AllocateArrayFrame<uint32_t>(buffer_allocation_num);
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    prev_state[i] = initial_state[i];
    merged_state[i] = initial_state[i];
    initial_user_pass[i] = render_pass_num;
  }
  auto max_transition_num = AllocateAndFillArrayFrame(buffer_allocation_num, 1U /*+1 for transitioning back to initial_state for non-graphics queue buffers*/);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    for (uint32_t j = 0; j < render_pass_buffer_num[i]; j++) {
      const auto& buffer_id = render_pass_buffer_allocation_index_list[i][j];
      if (buffer_id >= buffer_allocation_num) { continue; }
      if (initial_user_pass[buffer_id] == render_pass_num) {
        initial_user_pass[buffer_id] = i;
      }
      merged_state[buffer_id] |= render_pass_resource_state_list[i][j];
      if (prev_state[buffer_id] != render_pass_resource_state_list[i][j]) {
        max_transition_num[buffer_id]++;
        prev_state[buffer_id] = render_pass_resource_state_list[i][j];
      }
    }
  }
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    if (final_state[i] == ResourceStateTypeFlags::kNone) { continue; }
    merged_state[i] |= final_state[i];
    if (prev_state[i] != final_state[i]) {
      max_transition_num[i]++;
    }
  }
  return std::make_tuple(max_transition_num, merged_state, initial_user_pass);
}
constexpr auto GetGraphicsQueueOnlyStateFlags() {
  return ResourceStateTypeFlags::kRtv | ResourceStateTypeFlags::kDsvWrite | ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kPresent;
}
auto IsStateValidForQueue(const D3D12_COMMAND_LIST_TYPE command_queue_type, const ResourceStateTypeFlags::FlagType state) {
  switch (command_queue_type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT: {
      return true;
    }
    case D3D12_COMMAND_LIST_TYPE_COMPUTE: {
      return (state & GetGraphicsQueueOnlyStateFlags()) == 0;
    }
    case D3D12_COMMAND_LIST_TYPE_COPY: {
      return state == ResourceStateTypeFlags::kCopySrc || state == ResourceStateTypeFlags::kCopyDst;
    }
  }
  return false;
}
auto GetResourceStateTransitionInfo(const uint32_t render_pass_num, const uint32_t *  render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                    const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                    const uint32_t buffer_allocation_num,
                                    const uint32_t* render_pass_buffer_num,
                                    const uint32_t * const * render_pass_buffer_allocation_index_list,
                                    const ResourceStateTypeFlags::FlagType* const * render_pass_resource_state_list,
                                    const ResourceStateTypeFlags::FlagType* initial_state,
                                    const ResourceStateTypeFlags::FlagType* final_state) {
  auto resource_state_traisition_info_list = AllocateArrayFrame<ArrayOf<ResourceStateTransitionInfo>>(buffer_allocation_num);
  auto [max_transition_num, merged_state, initial_user_pass] = CollectStateTransitionInfo(render_pass_num, buffer_allocation_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list, initial_state, final_state);
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    resource_state_traisition_info_list[i].size = 0;
    resource_state_traisition_info_list[i].array = AllocateArrayFrame<ResourceStateTransitionInfo>(max_transition_num[i]);
  }
  uint32_t graphics_queue_last_pass = 0;
  auto last_sync_graphics_queue_pass = AllocateAndFillArrayFrame(render_pass_num, render_pass_num);
  // collect naive transitions
  for (uint32_t i = 0; i < render_pass_num; i++) {
    if (command_queue_type[render_pass_command_queue_index[i]] == D3D12_COMMAND_LIST_TYPE_DIRECT) {
      graphics_queue_last_pass = i;
    } else if (i > 0) {
      last_sync_graphics_queue_pass[i] = last_sync_graphics_queue_pass[i - 1];
      for (uint32_t j = 0; j < wait_pass_num[i]; j++) {
        if (command_queue_type[render_pass_command_queue_index[signal_pass_index[i][j]]] == D3D12_COMMAND_LIST_TYPE_DIRECT) {
          last_sync_graphics_queue_pass[i] = signal_pass_index[i][j];
          break;
        }
      }
    }
    for (uint32_t j = 0; j < render_pass_buffer_num[i]; j++) {
      const auto& buffer_id = render_pass_buffer_allocation_index_list[i][j];
      if (buffer_id >= buffer_allocation_num) { continue; }
      const auto current_size = resource_state_traisition_info_list[buffer_id].size;
      const auto prev_state = (current_size == 0) ? initial_state[buffer_id] : resource_state_traisition_info_list[buffer_id].array[current_size - 1].state_after;
      if (prev_state == render_pass_resource_state_list[i][j]) { continue; }
      resource_state_traisition_info_list[buffer_id].size++;
      resource_state_traisition_info_list[buffer_id].array[current_size].pass         = i;
      resource_state_traisition_info_list[buffer_id].array[current_size].timing       = 0;
      resource_state_traisition_info_list[buffer_id].array[current_size].state_before = prev_state;
      resource_state_traisition_info_list[buffer_id].array[current_size].state_after  = render_pass_resource_state_list[i][j];
    }
  }
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    if (final_state[i] == ResourceStateTypeFlags::kNone) { continue; }
    const auto current_size = resource_state_traisition_info_list[i].size;
    const auto prev_state = (current_size == 0) ? initial_state[i] : resource_state_traisition_info_list[i].array[current_size - 1].state_after;
    if (prev_state == final_state[i]) { continue; }
    assert(graphics_queue_last_pass < render_pass_num);
    resource_state_traisition_info_list[i].size++;
    resource_state_traisition_info_list[i].array[current_size].pass         = graphics_queue_last_pass;
    resource_state_traisition_info_list[i].array[current_size].timing       = 1;
    resource_state_traisition_info_list[i].array[current_size].state_before = prev_state;
    resource_state_traisition_info_list[i].array[current_size].state_after  = final_state[i];
  }
  const auto dsv_write_read = ResourceStateTypeFlags::kDsvWrite | ResourceStateTypeFlags::kDsvRead;
  const auto generic_read_cbv = ResourceStateTypeFlags::kCbv | ResourceStateTypeFlags::kGenericRead;
  const auto mergeable_read_states = ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs;
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    if (merged_state[i] == dsv_write_read) {
      // merge buffer with dsv write-read state only
      if (resource_state_traisition_info_list[i].size > 0 && initial_state[i] != dsv_write_read) {
        assert(initial_user_pass[i] < render_pass_num);
        resource_state_traisition_info_list[i].size = 1;
        resource_state_traisition_info_list[i].array[0].pass         = initial_user_pass[i];
        resource_state_traisition_info_list[i].array[0].timing       = 0;
        resource_state_traisition_info_list[i].array[0].state_before = initial_state[i];
        resource_state_traisition_info_list[i].array[0].state_after  = dsv_write_read;
      } else {
        resource_state_traisition_info_list[i].size = 0;
      }
    } else if (merged_state[i] == generic_read_cbv) {
      // no state change needed
      resource_state_traisition_info_list[i].size = 0;
    } else if (const auto state_after = merged_state[i] & mergeable_read_states; std::popcount(state_after) > 1) {
      // merge all readable states
      for (uint32_t j = 0; j < resource_state_traisition_info_list[i].size; j++) {
        if ((state_after & resource_state_traisition_info_list[i].array[j].state_after) != 0) {
          for (uint32_t k = j + 1; k < resource_state_traisition_info_list[i].size; k++) {
            const auto following_pass = resource_state_traisition_info_list[i].array[k].pass;
            if (command_queue_type[render_pass_command_queue_index[following_pass]] == D3D12_COMMAND_LIST_TYPE_DIRECT) { continue; }
            if (resource_state_traisition_info_list[i].array[j].pass > last_sync_graphics_queue_pass[following_pass]) {
              assert(last_sync_graphics_queue_pass[following_pass] < render_pass_num);
              resource_state_traisition_info_list[i].array[j].pass = last_sync_graphics_queue_pass[following_pass];
              resource_state_traisition_info_list[i].array[j].timing = 1;
            }
          }
          if (const auto current_pass = resource_state_traisition_info_list[i].array[j].pass;
              !IsStateValidForQueue(command_queue_type[render_pass_command_queue_index[current_pass]], state_after)) {
            assert(last_sync_graphics_queue_pass[current_pass] < render_pass_num);
            if (j > 0 && resource_state_traisition_info_list[i].array[j - 1].pass >= last_sync_graphics_queue_pass[current_pass]) {
              continue;
            }
            resource_state_traisition_info_list[i].array[j].pass = last_sync_graphics_queue_pass[current_pass];
            resource_state_traisition_info_list[i].array[j].timing = 1;
          }
          resource_state_traisition_info_list[i].size = j + 1;
          resource_state_traisition_info_list[i].array[j].state_after = state_after;
          break;
        }
      }
    } else if (last_sync_graphics_queue_pass[initial_user_pass[i]] >= render_pass_num && command_queue_type[render_pass_command_queue_index[initial_user_pass[i]]] != D3D12_COMMAND_LIST_TYPE_DIRECT && (merged_state[i] & GetGraphicsQueueOnlyStateFlags()) != 0) {
      const auto index = resource_state_traisition_info_list[i].size;
      resource_state_traisition_info_list[i].size++;
      resource_state_traisition_info_list[i].array[index].pass         = graphics_queue_last_pass;
      resource_state_traisition_info_list[i].array[index].timing       = 1;
      resource_state_traisition_info_list[i].array[index].state_before = resource_state_traisition_info_list[i].array[index - 1].state_after;
      resource_state_traisition_info_list[i].array[index].state_after  = initial_state[i];
    }
  }
  // move state transition timing to valid queue
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    for (uint32_t j = 0; j < resource_state_traisition_info_list[i].size; j++) {
      auto& transition = resource_state_traisition_info_list[i].array[j];
      if (command_queue_type[render_pass_command_queue_index[transition.pass]] == D3D12_COMMAND_LIST_TYPE_DIRECT) { continue; }
      const auto queue = command_queue_type[render_pass_command_queue_index[transition.pass]];
      if (IsStateValidForQueue(queue, transition.state_before) && IsStateValidForQueue(queue, transition.state_after)) { continue; }
      assert(last_sync_graphics_queue_pass[transition.pass] < render_pass_num);
      transition.pass = last_sync_graphics_queue_pass[transition.pass];
      transition.timing = 1;
    }
    D3D12_COMMAND_LIST_TYPE initial_queue = resource_state_traisition_info_list[i].size > 0 ? command_queue_type[render_pass_command_queue_index[resource_state_traisition_info_list[i].array[0].pass]] : D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (auto last_state = (resource_state_traisition_info_list[i].size == 0) ? initial_state[i] : resource_state_traisition_info_list[i].array[resource_state_traisition_info_list[i].size - 1].state_after;
        initial_queue != D3D12_COMMAND_LIST_TYPE_DIRECT && !IsStateValidForQueue(initial_queue, last_state)) {
      if (resource_state_traisition_info_list[i].size == 0 || last_sync_graphics_queue_pass[resource_state_traisition_info_list[i].array[0].pass] == render_pass_num) {
        auto& transition = resource_state_traisition_info_list[i].array[resource_state_traisition_info_list[i].size];
        resource_state_traisition_info_list[i].size++;
        assert(graphics_queue_last_pass < render_pass_num);
        transition.pass         = graphics_queue_last_pass;
        transition.timing       = 1;
        transition.state_before = last_state;
        transition.state_after  = (resource_state_traisition_info_list[i].size == 1) ? initial_state[i] : resource_state_traisition_info_list[i].array[0].state_before;
      }
    }
  }
  return resource_state_traisition_info_list;
}
auto ConvertStateTransitionsToBarrierPerPass(const uint32_t render_pass_num, const uint32_t buffer_allocation_num, const ArrayOf<ResourceStateTransitionInfo>* resource_state_traisition_info_list, const MemoryType& memory_type) {
  // count barrier num
  auto barrier_num = AllocateArrayFrame<uint32_t*>(render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    barrier_num[i] = AllocateArray<uint32_t>(memory_type, kBarrierExecutionTimingNum);
    for (uint32_t j = 0; j < kBarrierExecutionTimingNum; j++) {
      barrier_num[i][j] = 0;
    }
  }
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    for (uint32_t j = 0; j < resource_state_traisition_info_list[i].size; j++) {
      const auto& transition = resource_state_traisition_info_list[i].array[j];
      barrier_num[transition.pass][transition.timing]++;
    }
  }
  // allocate all barriers
  auto barrier_config_list = AllocateArray<ArrayOf<BarrierConfig>*>(memory_type, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    barrier_config_list[i] = AllocateArray<ArrayOf<BarrierConfig>>(memory_type, kBarrierExecutionTimingNum);
    for (uint32_t j = 0; j < kBarrierExecutionTimingNum; j++) {
      barrier_config_list[i][j] = InitializeArray<BarrierConfig>(barrier_num[i][j], memory_type);
      barrier_num[i][j] = 0; // reused for barrier index
    }
  }
  // fill barriers
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    for (uint32_t j = 0; j < resource_state_traisition_info_list[i].size; j++) {
      const auto& transition = resource_state_traisition_info_list[i].array[j];
      auto& dst_barrier = barrier_config_list[transition.pass][transition.timing].array[barrier_num[transition.pass][transition.timing]];
      barrier_num[transition.pass][transition.timing]++;
      dst_barrier.buffer_allocation_index = i;
      dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      dst_barrier.state_before = ResourceStateTypeFlags::ConvertToD3d12ResourceState(transition.state_before);
      dst_barrier.state_after  = ResourceStateTypeFlags::ConvertToD3d12ResourceState(transition.state_after);
    }
  }
  return barrier_config_list;
}
auto GetStateAtFrameEnd(const uint32_t buffer_num, const ArrayOf<ResourceStateTransitionInfo>* resource_state_traisition_info_list, const ResourceStateTypeFlags::FlagType* initial_state, const MemoryType memory_type) {
  auto state_at_frame_end = AllocateArray<ResourceStateTypeFlags::FlagType>(memory_type, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    if (resource_state_traisition_info_list[i].size == 0) {
      state_at_frame_end[i] = initial_state[i];
      continue;
    }
    state_at_frame_end[i] = resource_state_traisition_info_list[i].array[resource_state_traisition_info_list[i].size - 1].state_after;
  }
  return state_at_frame_end;
}
} // namespace
BarrierTransitionInfo ConfigureBarrierTransitions(const uint32_t buffer_num, const uint32_t render_pass_num, const uint32_t* render_pass_buffer_num,
                                                  const uint32_t* const * render_pass_buffer_allocation_index_list,
                                                  const ResourceStateTypeFlags::FlagType* const * render_pass_resource_state_list,
                                                  const uint32_t* wait_pass_num, const uint32_t* const * signal_pass_index,
                                                  const uint32_t* const render_pass_command_queue_index, const D3D12_COMMAND_LIST_TYPE* command_queue_type,
                                                  const ResourceStateTypeFlags::FlagType* initial_state, const ResourceStateTypeFlags::FlagType* final_state,
                                                  const MemoryType& memory_type) {
  auto resource_state_traisition_info = GetResourceStateTransitionInfo(render_pass_num, render_pass_command_queue_index, command_queue_type, wait_pass_num, signal_pass_index,
                                                                       buffer_num, render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list, initial_state, final_state);
  auto barrier_config_list = ConvertStateTransitionsToBarrierPerPass(render_pass_num, buffer_num, resource_state_traisition_info, memory_type);
  auto state_at_frame_end = GetStateAtFrameEnd(buffer_num, resource_state_traisition_info, initial_state, memory_type);
  return {barrier_config_list, state_at_frame_end};
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("resource state transition") {
  using namespace illuminate;
  D3D12_COMMAND_LIST_TYPE command_queue_type[] = {
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    D3D12_COMMAND_LIST_TYPE_COMPUTE,
  };
  uint32_t render_pass_command_queue_index[] = {0,0,1,0,1,1,0,1,};
  uint32_t wait_pass_num[] = {0,0,0,1,1,0,1,1,};
  uint32_t signal_pass_index_3[] = {2,};
  uint32_t signal_pass_index_4[] = {1,};
  uint32_t signal_pass_index_6[] = {5,};
  uint32_t signal_pass_index_7[] = {3,};
  uint32_t* signal_pass_index[] = {
    nullptr,
    nullptr,
    nullptr,
    signal_pass_index_3,
    signal_pass_index_4,
    nullptr,
    signal_pass_index_6,
    signal_pass_index_7,
  };
  ResourceStateTypeFlags::FlagType initial_state[] = {
    /*0:*/ ResourceStateTypeFlags::kPresent,
    /*1:*/ ResourceStateTypeFlags::kRtv,
    /*2:*/ ResourceStateTypeFlags::kSrvPs,
    /*3:*/ ResourceStateTypeFlags::kRtv,
    /*4:*/ ResourceStateTypeFlags::kRtv,
    /*5:*/ ResourceStateTypeFlags::kRtv,
    /*6:*/ ResourceStateTypeFlags::kRtv,
    /*7:*/ ResourceStateTypeFlags::kUav,
    /*8:*/ ResourceStateTypeFlags::kUav,
    /*9:*/ ResourceStateTypeFlags::kUav,
    /*10:*/ ResourceStateTypeFlags::kUav,
    /*11:*/ ResourceStateTypeFlags::kSrvPs,
    /*12:*/ ResourceStateTypeFlags::kDsvWrite,
    /*13:*/ ResourceStateTypeFlags::kDsvWrite | ResourceStateTypeFlags::kDsvRead,
    /*14:*/ ResourceStateTypeFlags::kDsvWrite,
    /*15:*/ ResourceStateTypeFlags::kDsvWrite,
    /*16:*/ ResourceStateTypeFlags::kDsvWrite,
    /*17:*/ ResourceStateTypeFlags::kDsvWrite,
    /*18:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*19:*/ ResourceStateTypeFlags::kDsvWrite,
    /*20:*/ ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs,
    /*21:*/ ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs,
    /*22:*/ ResourceStateTypeFlags::kDsvWrite,
    /*23:*/ ResourceStateTypeFlags::kRtv,
    /*24:*/ ResourceStateTypeFlags::kRtv,
    /*25:*/ ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
    /*26:*/ ResourceStateTypeFlags::kGenericRead,
    /*27:*/ ResourceStateTypeFlags::kUav,
  };
  ResourceStateTypeFlags::FlagType final_state[] = {
    /*0:*/ ResourceStateTypeFlags::kPresent,
    /*1:*/ ResourceStateTypeFlags::kNone,
    /*2:*/ ResourceStateTypeFlags::kNone,
    /*3:*/ ResourceStateTypeFlags::kNone,
    /*4:*/ ResourceStateTypeFlags::kNone,
    /*5:*/ ResourceStateTypeFlags::kNone,
    /*6:*/ ResourceStateTypeFlags::kNone,
    /*7:*/ ResourceStateTypeFlags::kNone,
    /*8:*/ ResourceStateTypeFlags::kNone,
    /*9:*/ ResourceStateTypeFlags::kNone,
    /*10:*/ ResourceStateTypeFlags::kNone,
    /*11:*/ ResourceStateTypeFlags::kNone,
    /*12:*/ ResourceStateTypeFlags::kNone,
    /*13:*/ ResourceStateTypeFlags::kNone,
    /*14:*/ ResourceStateTypeFlags::kNone,
    /*15:*/ ResourceStateTypeFlags::kNone,
    /*16:*/ ResourceStateTypeFlags::kNone,
    /*17:*/ ResourceStateTypeFlags::kNone,
    /*18:*/ ResourceStateTypeFlags::kNone,
    /*19:*/ ResourceStateTypeFlags::kNone,
    /*20:*/ ResourceStateTypeFlags::kNone,
    /*21:*/ ResourceStateTypeFlags::kNone,
    /*22:*/ ResourceStateTypeFlags::kNone,
    /*23:*/ ResourceStateTypeFlags::kSrvPs,
    /*24:*/ ResourceStateTypeFlags::kNone,
    /*25:*/ ResourceStateTypeFlags::kNone,
    /*26:*/ ResourceStateTypeFlags::kNone,
    /*27:*/ ResourceStateTypeFlags::kNone,
  };
  // pass 0: graphics (async w/pass 2)
  uint32_t render_pass_buffer_allocation_index_list_0[] = {1,2,3,4,5,6,12,13,14,15,16,17,18,19,20,21,22,24,26,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_0[] = {
    /*1:*/ ResourceStateTypeFlags::kRtv,
    /*2:*/ ResourceStateTypeFlags::kRtv,
    /*3:*/ ResourceStateTypeFlags::kRtv,
    /*4:*/ ResourceStateTypeFlags::kRtv,
    /*5:*/ ResourceStateTypeFlags::kRtv,
    /*6:*/ ResourceStateTypeFlags::kRtv,
    /*12:*/ ResourceStateTypeFlags::kDsvWrite,
    /*13:*/ ResourceStateTypeFlags::kDsvWrite,
    /*14:*/ ResourceStateTypeFlags::kDsvWrite,
    /*15:*/ ResourceStateTypeFlags::kDsvWrite,
    /*16:*/ ResourceStateTypeFlags::kDsvWrite,
    /*17:*/ ResourceStateTypeFlags::kDsvWrite,
    /*18:*/ ResourceStateTypeFlags::kDsvWrite,
    /*19:*/ ResourceStateTypeFlags::kDsvWrite,
    /*20:*/ ResourceStateTypeFlags::kDsvWrite,
    /*21:*/ ResourceStateTypeFlags::kDsvWrite,
    /*22:*/ ResourceStateTypeFlags::kDsvWrite,
    /*24:*/ ResourceStateTypeFlags::kRtv,
    /*26:*/ ResourceStateTypeFlags::kCbv,
  };
  // pass 1: graphics (async w/pass 2)
  uint32_t render_pass_buffer_allocation_index_list_1[] = {1,2,12,13,15,16,17,20,21,25,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_1[] = {
    /*1:*/ ResourceStateTypeFlags::kSrvPs,
    /*2:*/ ResourceStateTypeFlags::kSrvPs,
    /*12:*/ ResourceStateTypeFlags::kDsvRead,
    /*13:*/ ResourceStateTypeFlags::kDsvRead,
    /*15:*/ ResourceStateTypeFlags::kDsvRead,
    /*16:*/ ResourceStateTypeFlags::kSrvPs,
    /*17:*/ ResourceStateTypeFlags::kDsvRead,
    /*20:*/ ResourceStateTypeFlags::kDsvRead,
    /*21:*/ ResourceStateTypeFlags::kDsvRead,
    /*25:*/ ResourceStateTypeFlags::kRtv,
  };
  // pass 2: compute (async w/pass 0,1)
  uint32_t render_pass_buffer_allocation_index_list_2[] = {7,8,9,10,27,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_2[] = {
    /*7:*/ ResourceStateTypeFlags::kUav,
    /*8:*/ ResourceStateTypeFlags::kUav,
    /*9:*/ ResourceStateTypeFlags::kUav,
    /*10:*/ ResourceStateTypeFlags::kUav,
    /*27:*/ ResourceStateTypeFlags::kUav,
  };
  // pass 3: graphics (async w/pass 4,5)
  uint32_t render_pass_buffer_allocation_index_list_3[] = {3,4,7,17,21,24,25,27,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_3[] = {
    /*3:*/ ResourceStateTypeFlags::kSrvPs,
    /*4:*/ ResourceStateTypeFlags::kRtv,
    /*7:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*17:*/ ResourceStateTypeFlags::kSrvPs,
    /*21:*/ ResourceStateTypeFlags::kSrvPs,
    /*24:*/ ResourceStateTypeFlags::kSrvPs,
    /*25:*/ ResourceStateTypeFlags::kSrvPs,
    /*27:*/ ResourceStateTypeFlags::kSrvPs,
  };
  // pass 4: compute (async w/pass 3)
  uint32_t render_pass_buffer_allocation_index_list_4[] = {5,9,14,15,17,19,20,21,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_4[] = {
    /*5:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*9:*/ ResourceStateTypeFlags::kUav,
    /*14:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*15:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*17:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*19:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*20:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*21:*/ ResourceStateTypeFlags::kSrvNonPs,
  };
  // pass 5: compute (async w/pass 3)
  uint32_t render_pass_buffer_allocation_index_list_5[] = {6,18,24,25,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_5[] = {
    /*6:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*18:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*24:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*25:*/ ResourceStateTypeFlags::kSrvNonPs,
  };
  // pass 6: graphics (async w/pass 7)
  uint32_t render_pass_buffer_allocation_index_list_6[] = {0,4,8,9,19,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_6[] = {
    /*0:*/ ResourceStateTypeFlags::kRtv,
    /*4:*/ ResourceStateTypeFlags::kSrvPs,
    /*8:*/ ResourceStateTypeFlags::kSrvPs,
    /*9:*/ ResourceStateTypeFlags::kSrvPs,
    /*19:*/ ResourceStateTypeFlags::kDsvRead,
  };
  // pass 7: compute (async w/pass 6)
  uint32_t render_pass_buffer_allocation_index_list_7[] = {10,22,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_7[] = {
    /*10:*/ ResourceStateTypeFlags::kUav,
    /*22:*/ ResourceStateTypeFlags::kSrvNonPs,
  };
  uint32_t render_pass_buffer_num[] = {
    countof(render_pass_buffer_allocation_index_list_0),
    countof(render_pass_buffer_allocation_index_list_1),
    countof(render_pass_buffer_allocation_index_list_2),
    countof(render_pass_buffer_allocation_index_list_3),
    countof(render_pass_buffer_allocation_index_list_4),
    countof(render_pass_buffer_allocation_index_list_5),
    countof(render_pass_buffer_allocation_index_list_6),
    countof(render_pass_buffer_allocation_index_list_7),
  };
  const uint32_t* render_pass_buffer_allocation_index_list[] = {
    render_pass_buffer_allocation_index_list_0,
    render_pass_buffer_allocation_index_list_1,
    render_pass_buffer_allocation_index_list_2,
    render_pass_buffer_allocation_index_list_3,
    render_pass_buffer_allocation_index_list_4,
    render_pass_buffer_allocation_index_list_5,
    render_pass_buffer_allocation_index_list_6,
    render_pass_buffer_allocation_index_list_7,
  };
  ResourceStateTypeFlags::FlagType* render_pass_resource_state_list[] = {
    render_pass_resource_state_list_0,
    render_pass_resource_state_list_1,
    render_pass_resource_state_list_2,
    render_pass_resource_state_list_3,
    render_pass_resource_state_list_4,
    render_pass_resource_state_list_5,
    render_pass_resource_state_list_6,
    render_pass_resource_state_list_7,
  };
  auto resource_state_traisition_info = GetResourceStateTransitionInfo(countof(render_pass_command_queue_index), render_pass_command_queue_index, command_queue_type,
                                                                       wait_pass_num, signal_pass_index,
                                                                       countof(initial_state),
                                                                       render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list, initial_state, final_state);
  CHECK_EQ(resource_state_traisition_info[0].size, 2);
  CHECK_EQ(resource_state_traisition_info[0].array[0].pass, 6);
  CHECK_EQ(resource_state_traisition_info[0].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[0].array[0].state_before, ResourceStateTypeFlags::kPresent);
  CHECK_EQ(resource_state_traisition_info[0].array[0].state_after,  ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[0].array[1].pass, 6);
  CHECK_EQ(resource_state_traisition_info[0].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[0].array[1].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[0].array[1].state_after,  ResourceStateTypeFlags::kPresent);
  CHECK_EQ(resource_state_traisition_info[1].size, 1);
  CHECK_EQ(resource_state_traisition_info[1].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[1].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[1].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[1].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[2].size, 2);
  CHECK_EQ(resource_state_traisition_info[2].array[0].pass, 0);
  CHECK_EQ(resource_state_traisition_info[2].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[2].array[0].state_before, ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[2].array[0].state_after,  ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[2].array[1].pass, 1);
  CHECK_EQ(resource_state_traisition_info[2].array[1].timing, 0);
  CHECK_EQ(resource_state_traisition_info[2].array[1].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[2].array[1].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[3].size, 1);
  CHECK_EQ(resource_state_traisition_info[3].array[0].pass, 3);
  CHECK_EQ(resource_state_traisition_info[3].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[3].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[3].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[4].size, 1);
  CHECK_EQ(resource_state_traisition_info[4].array[0].pass, 6);
  CHECK_EQ(resource_state_traisition_info[4].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[4].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[4].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[5].size, 1);
  CHECK_EQ(resource_state_traisition_info[5].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[5].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[5].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[5].array[0].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[6].size, 1);
  CHECK_EQ(resource_state_traisition_info[6].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[6].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[6].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[6].array[0].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[7].size, 1);
  CHECK_EQ(resource_state_traisition_info[7].array[0].pass, 3);
  CHECK_EQ(resource_state_traisition_info[7].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[7].array[0].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[7].array[0].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[8].size, 2);
  CHECK_EQ(resource_state_traisition_info[8].array[0].pass, 6);
  CHECK_EQ(resource_state_traisition_info[8].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[8].array[0].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[8].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[8].array[1].pass, 6);
  CHECK_EQ(resource_state_traisition_info[8].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[8].array[1].state_before, ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[8].array[1].state_after,  ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[9].size, 2);
  CHECK_EQ(resource_state_traisition_info[9].array[0].pass, 6);
  CHECK_EQ(resource_state_traisition_info[9].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[9].array[0].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[9].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[9].array[1].pass, 6);
  CHECK_EQ(resource_state_traisition_info[9].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[9].array[1].state_before, ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[9].array[1].state_after,  ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[10].size, 0);
  CHECK_EQ(resource_state_traisition_info[11].size, 0);
  CHECK_EQ(resource_state_traisition_info[12].size, 1);
  CHECK_EQ(resource_state_traisition_info[12].array[0].pass, 0);
  CHECK_EQ(resource_state_traisition_info[12].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[12].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[12].array[0].state_after,  ResourceStateTypeFlags::kDsvWrite | ResourceStateTypeFlags::kDsvRead);
  CHECK_EQ(resource_state_traisition_info[13].size, 0);
  CHECK_EQ(resource_state_traisition_info[14].size, 1);
  CHECK_EQ(resource_state_traisition_info[14].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[14].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[14].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[14].array[0].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[15].size, 1);
  CHECK_EQ(resource_state_traisition_info[15].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[15].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[15].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[15].array[0].state_after,  ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[16].size, 1);
  CHECK_EQ(resource_state_traisition_info[16].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[16].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[16].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[16].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[17].size, 1);
  CHECK_EQ(resource_state_traisition_info[17].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[17].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[17].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[17].array[0].state_after,  ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[18].size, 2);
  CHECK_EQ(resource_state_traisition_info[18].array[0].pass, 0);
  CHECK_EQ(resource_state_traisition_info[18].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[18].array[0].state_before, ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[18].array[0].state_after,  ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[18].array[1].pass, 1);
  CHECK_EQ(resource_state_traisition_info[18].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[18].array[1].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[18].array[1].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[19].size, 1);
  CHECK_EQ(resource_state_traisition_info[19].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[19].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[19].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[19].array[0].state_after,  ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[20].size, 2);
  CHECK_EQ(resource_state_traisition_info[20].array[0].pass, 0);
  CHECK_EQ(resource_state_traisition_info[20].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[20].array[0].state_before, ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[20].array[0].state_after,  ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[20].array[1].pass, 1);
  CHECK_EQ(resource_state_traisition_info[20].array[1].timing, 0);
  CHECK_EQ(resource_state_traisition_info[20].array[1].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[20].array[1].state_after,  ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[21].size, 2);
  CHECK_EQ(resource_state_traisition_info[21].array[0].pass, 0);
  CHECK_EQ(resource_state_traisition_info[21].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[21].array[0].state_before, ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[21].array[0].state_after,  ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[21].array[1].pass, 1);
  CHECK_EQ(resource_state_traisition_info[21].array[1].timing, 0);
  CHECK_EQ(resource_state_traisition_info[21].array[1].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[21].array[1].state_after,  ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[22].size, 1);
  CHECK_EQ(resource_state_traisition_info[22].array[0].pass, 3);
  CHECK_EQ(resource_state_traisition_info[22].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[22].array[0].state_before, ResourceStateTypeFlags::kDsvWrite);
  CHECK_EQ(resource_state_traisition_info[22].array[0].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[23].size, 1);
  CHECK_EQ(resource_state_traisition_info[23].array[0].pass, 6);
  CHECK_EQ(resource_state_traisition_info[23].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[23].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[23].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[24].size, 1);
  CHECK_EQ(resource_state_traisition_info[24].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[24].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[24].array[0].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[24].array[0].state_after,  ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[25].size, 2);
  CHECK_EQ(resource_state_traisition_info[25].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[25].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[25].array[0].state_before, ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[25].array[0].state_after,  ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[25].array[1].pass, 1);
  CHECK_EQ(resource_state_traisition_info[25].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[25].array[1].state_before, ResourceStateTypeFlags::kRtv);
  CHECK_EQ(resource_state_traisition_info[25].array[1].state_after,  ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[26].size, 0);
  CHECK_EQ(resource_state_traisition_info[27].size, 2);
  CHECK_EQ(resource_state_traisition_info[27].array[0].pass, 3);
  CHECK_EQ(resource_state_traisition_info[27].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[27].array[0].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[27].array[0].state_after,  ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[27].array[1].pass, 6);
  CHECK_EQ(resource_state_traisition_info[27].array[1].timing, 1);
  CHECK_EQ(resource_state_traisition_info[27].array[1].state_before, ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[27].array[1].state_after,  ResourceStateTypeFlags::kUav);
}
TEST_CASE("resource state transition:uav<->srv-ps/nonps") {
  using namespace illuminate;
  D3D12_COMMAND_LIST_TYPE command_queue_type[] = {
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    D3D12_COMMAND_LIST_TYPE_COMPUTE,
    D3D12_COMMAND_LIST_TYPE_COPY,
  };
  // 0:copy resource, 1:prez, 2:gbuffer, 3:linear depth, 4:screen space shadow, 5:lighting, 6:output to swapchain, 7:imgui
  uint32_t render_pass_command_queue_index[] = {2,0,0,1,1,1,0,0,};
  uint32_t wait_pass_num[] = {0,1,0,1,0,1,1,0,};
  uint32_t signal_pass_index_1[] = {0,};
  uint32_t signal_pass_index_3[] = {1,};
  uint32_t signal_pass_index_5[] = {2,};
  uint32_t signal_pass_index_6[] = {5,};
  uint32_t* signal_pass_index[] = {
    nullptr,
    signal_pass_index_1,
    nullptr,
    signal_pass_index_3,
    nullptr,
    signal_pass_index_5,
    signal_pass_index_6,
    nullptr,
  };
  ResourceStateTypeFlags::FlagType initial_state[] = {
    /*0:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*1:*/ ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
  };
  ResourceStateTypeFlags::FlagType final_state[] = {
    ResourceStateTypeFlags::kNone,
    ResourceStateTypeFlags::kNone,
  };
  // pass 3: linear depth
  uint32_t render_pass_buffer_allocation_index_list_3[] = {0,1,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_3[] = {
    /*0:*/ ResourceStateTypeFlags::kUav,
    /*1:*/ ResourceStateTypeFlags::kUav,
  };
  // pass 4: screen space shadow
  uint32_t render_pass_buffer_allocation_index_list_4[] = {0,1,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_4[] = {
    /*0:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*1:*/ ResourceStateTypeFlags::kSrvNonPs,
  };
  // pass 5: lighting
  uint32_t render_pass_buffer_allocation_index_list_5[] = {0,1,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_5[] = {
    /*0:*/ ResourceStateTypeFlags::kSrvNonPs,
    /*1:*/ ResourceStateTypeFlags::kSrvNonPs,
  };
  // pass 6: output to swapchain
  uint32_t render_pass_buffer_allocation_index_list_6[] = {0,1,};
  ResourceStateTypeFlags::FlagType render_pass_resource_state_list_6[] = {
    /*0:*/ ResourceStateTypeFlags::kSrvPs,
    /*1:*/ ResourceStateTypeFlags::kSrvPs,
  };
  uint32_t render_pass_buffer_num[] = {
    0,
    0,
    0,
    countof(render_pass_buffer_allocation_index_list_3),
    countof(render_pass_buffer_allocation_index_list_4),
    countof(render_pass_buffer_allocation_index_list_5),
    countof(render_pass_buffer_allocation_index_list_6),
    0,
  };
  const uint32_t* render_pass_buffer_allocation_index_list[] = {
    nullptr,
    nullptr,
    nullptr,
    render_pass_buffer_allocation_index_list_3,
    render_pass_buffer_allocation_index_list_4,
    render_pass_buffer_allocation_index_list_5,
    render_pass_buffer_allocation_index_list_6,
    nullptr,
  };
  ResourceStateTypeFlags::FlagType* render_pass_resource_state_list[] = {
    nullptr,
    nullptr,
    nullptr,
    render_pass_resource_state_list_3,
    render_pass_resource_state_list_4,
    render_pass_resource_state_list_5,
    render_pass_resource_state_list_6,
    nullptr,
  };
  auto resource_state_traisition_info = GetResourceStateTransitionInfo(countof(render_pass_command_queue_index), render_pass_command_queue_index, command_queue_type,
                                                                       wait_pass_num, signal_pass_index,
                                                                       countof(initial_state),
                                                                       render_pass_buffer_num, render_pass_buffer_allocation_index_list, render_pass_resource_state_list, initial_state, final_state);
  CHECK_EQ(resource_state_traisition_info[0].size, 3);
  CHECK_EQ(resource_state_traisition_info[0].array[0].pass, 3);
  CHECK_EQ(resource_state_traisition_info[0].array[0].timing, 0);
  CHECK_EQ(resource_state_traisition_info[0].array[0].state_before, ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[0].array[0].state_after,  ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[0].array[1].pass, 4);
  CHECK_EQ(resource_state_traisition_info[0].array[1].timing, 0);
  CHECK_EQ(resource_state_traisition_info[0].array[1].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[0].array[1].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[0].array[2].pass, 6);
  CHECK_EQ(resource_state_traisition_info[0].array[2].timing, 0);
  CHECK_EQ(resource_state_traisition_info[0].array[2].state_before, ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[0].array[2].state_after,  ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[1].size, 3);
  CHECK_EQ(resource_state_traisition_info[1].array[0].pass, 1);
  CHECK_EQ(resource_state_traisition_info[1].array[0].timing, 1);
  CHECK_EQ(resource_state_traisition_info[1].array[0].state_before, ResourceStateTypeFlags::kSrvNonPs | ResourceStateTypeFlags::kSrvPs);
  CHECK_EQ(resource_state_traisition_info[1].array[0].state_after,  ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[1].array[1].pass, 4);
  CHECK_EQ(resource_state_traisition_info[1].array[1].timing, 0);
  CHECK_EQ(resource_state_traisition_info[1].array[1].state_before, ResourceStateTypeFlags::kUav);
  CHECK_EQ(resource_state_traisition_info[1].array[1].state_after,  ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[1].array[2].pass, 6);
  CHECK_EQ(resource_state_traisition_info[1].array[2].timing, 0);
  CHECK_EQ(resource_state_traisition_info[1].array[2].state_before, ResourceStateTypeFlags::kSrvNonPs);
  CHECK_EQ(resource_state_traisition_info[1].array[2].state_after,  ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs);
}
TEST_CASE("fill barrier list") {
  using namespace illuminate;
  ResourceStateTransitionInfo buffer0[] = {
    {
      .pass = 0,
      .timing = 0,
      .state_before = ResourceStateTypeFlags::kRtv,
      .state_after  = ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
    },
    {
      .pass = 2,
      .timing = 1,
      .state_before = ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
      .state_after  = ResourceStateTypeFlags::kRtv,
    },
  };
  ResourceStateTransitionInfo buffer1[] = {
    {
      .pass = 0,
      .timing = 1,
      .state_before = ResourceStateTypeFlags::kUav,
      .state_after  = ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
    },
  };
  ResourceStateTransitionInfo buffer2[] = {
    {
      .pass = 2,
      .timing = 0,
      .state_before = ResourceStateTypeFlags::kDsvWrite,
      .state_after  = ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs,
    },
  };
  ResourceStateTransitionInfo buffer3[] = {
    {
      .pass = 0,
      .timing = 0,
      .state_before = ResourceStateTypeFlags::kDsvWrite,
      .state_after  = ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs,
    },
    {
      .pass = 2,
      .timing = 1,
      .state_before = ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs,
      .state_after  = ResourceStateTypeFlags::kRtv,
    },
  };
  ArrayOf<ResourceStateTransitionInfo> transitions[] = {
    { .size = countof(buffer0), .array = buffer0, },
    { .size = countof(buffer1), .array = buffer1, },
    { .size = countof(buffer2), .array = buffer2, },
    { .size = countof(buffer3), .array = buffer3, },
  };
  auto barrier_config_list = ConvertStateTransitionsToBarrierPerPass(3, countof(transitions), transitions, MemoryType::kFrame);
  // barrier_config_list[pass_index][timing(0/1)]
  CHECK_EQ(barrier_config_list[0][0].size, 2);
  CHECK_EQ(barrier_config_list[0][0].array[0].buffer_allocation_index, 0);
  CHECK_EQ(barrier_config_list[0][0].array[0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][0].array[0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][0].array[0].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kRtv));
  CHECK_EQ(barrier_config_list[0][0].array[0].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[0][0].array[1].buffer_allocation_index, 3);
  CHECK_EQ(barrier_config_list[0][0].array[1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][0].array[1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][0].array[1].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kDsvWrite));
  CHECK_EQ(barrier_config_list[0][0].array[1].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[0][1].size, 1);
  CHECK_EQ(barrier_config_list[0][1].array[0].buffer_allocation_index, 1);
  CHECK_EQ(barrier_config_list[0][1].array[0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1].array[0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1].array[0].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kUav));
  CHECK_EQ(barrier_config_list[0][1].array[0].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[1][0].size, 0);
  CHECK_EQ(barrier_config_list[1][1].size, 0);
  CHECK_EQ(barrier_config_list[2][0].size, 1);
  CHECK_EQ(barrier_config_list[2][0].array[0].buffer_allocation_index, 2);
  CHECK_EQ(barrier_config_list[2][0].array[0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][0].array[0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][0].array[0].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kDsvWrite));
  CHECK_EQ(barrier_config_list[2][0].array[0].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kDsvRead | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[2][1].size, 2);
  CHECK_EQ(barrier_config_list[2][1].array[0].buffer_allocation_index, 0);
  CHECK_EQ(barrier_config_list[2][1].array[0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1].array[0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1].array[0].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[2][1].array[0].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kRtv));
  CHECK_EQ(barrier_config_list[2][1].array[1].buffer_allocation_index, 3);
  CHECK_EQ(barrier_config_list[2][1].array[1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[2][1].array[1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[2][1].array[1].state_before, ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kSrvPs | ResourceStateTypeFlags::kSrvNonPs));
  CHECK_EQ(barrier_config_list[2][1].array[1].state_after,  ResourceStateTypeFlags::ConvertToD3d12ResourceState(ResourceStateTypeFlags::kRtv));
}
