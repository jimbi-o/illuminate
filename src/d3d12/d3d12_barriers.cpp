#include "d3d12_barriers.h"
#include "d3d12_src_common.h"
#include "doctest/doctest.h"
namespace illuminate {
constexpr auto IsResourceStateInclusive(const ResourceStateType& a, const ResourceStateType& b) {
  if (a == b) { return true; }
  return a == ResourceStateType::kGenericRead && b == ResourceStateType::kCbv;
}
RenderPassResourceState ConfigureRenderPassResourceStates(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const bool** pingpong_buffer_write_to_sub_list, const bool* render_pass_enable_flag, const uint32_t additional_buffer_state_num, RenderPassBufferState* additional_buffer_state_list, const MemoryType retval_memory_type) {
  // TODO additional_buffer_state_list
  auto resource_state_list = AllocateArray<ResourceStateType**>(retval_memory_type, buffer_num);
  auto last_user_pass = AllocateArray<uint32_t*>(retval_memory_type, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto pingpong_buffer = buffer_config_list[i].pingpong;
    const auto buffer_allocation_num = pingpong_buffer ? 2U : 1U;
    resource_state_list[i] = AllocateArray<ResourceStateType*>(retval_memory_type, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      resource_state_list[i][j] = AllocateArray<ResourceStateType>(retval_memory_type, buffer_allocation_num);
    }
    last_user_pass[i] = AllocateArray<uint32_t>(retval_memory_type, buffer_allocation_num);
    for (uint32_t k = 0; k < buffer_allocation_num; k++) {
      last_user_pass[i][k] = 0;
    }
    auto state_main = buffer_config_list[i].initial_state;
    if (buffer_config_list[i].descriptor_only) {
      for (uint32_t j = 0; j < render_pass_num; j++) {
        for (uint32_t k = 0; k < buffer_allocation_num; k++) {
          resource_state_list[i][j][k] = state_main;
        }
      }
      continue;
    }
    auto state_sub  = state_main;
    for (uint32_t j = 0; j < render_pass_num; j++) {
      if (render_pass_enable_flag[j]) {
        uint32_t buffer_count = 0;
        for (uint32_t k = 0; k < render_pass_list[j].buffer_num; k++) {
          if (render_pass_list[j].buffer_list[k].buffer_index == i) {
            if (pingpong_buffer) {
              auto read_only = IsResourceStateReadOnly(render_pass_list[j].buffer_list[k].state);
              auto write_to_sub = pingpong_buffer_write_to_sub_list[i][j];
              if (read_only && write_to_sub || !read_only && !write_to_sub) {
                if (!IsResourceStateInclusive(state_main, render_pass_list[j].buffer_list[k].state)) {
                  state_main = render_pass_list[j].buffer_list[k].state;
                }
                last_user_pass[i][0] = j;
              } else {
                if (!IsResourceStateInclusive(state_sub, render_pass_list[j].buffer_list[k].state)) {
                  state_sub = render_pass_list[j].buffer_list[k].state;
                }
                last_user_pass[i][1] = j;
              }
            } else {
              if (!IsResourceStateInclusive(state_main, render_pass_list[j].buffer_list[k].state)) {
                state_main = render_pass_list[j].buffer_list[k].state;
              }
              last_user_pass[i][0] = j;
            }
            buffer_count++;
            if (buffer_count >= buffer_allocation_num) { break; }
          }
        }
      }
      resource_state_list[i][j][0] = state_main;
      if (pingpong_buffer) {
        resource_state_list[i][j][1] = state_sub;
      }
    }
    for (uint32_t k = 0; k < buffer_allocation_num; k++) {
      if (!IsResourceStateInclusive(buffer_config_list[i].initial_state, resource_state_list[i][last_user_pass[i][k]][0])) {
        auto pass_index = last_user_pass[i][k] + 1;
        for (; pass_index < render_pass_num && !render_pass_enable_flag[pass_index]; pass_index++);
        for (; pass_index < render_pass_num; pass_index++) {
          resource_state_list[i][pass_index][k] = buffer_config_list[i].initial_state;
        }
      }
    }
  }
  return {resource_state_list, last_user_pass};
}
BarrierTransitions ConfigureBarrierTransitions(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const ResourceStateTypePerPass* resource_state_list, const uint32_t* const * const last_user_pass, const MemoryType retval_memory_type) {
  // command queue type not considered yet.
  auto barrier_num = AllocateArray<uint32_t*>(retval_memory_type, kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_num[i] = AllocateArray<uint32_t>(retval_memory_type, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      barrier_num[i][j] = (i == 0) ? render_pass_list[j].prepass_barrier_num : render_pass_list[j].postpass_barrier_num;
    }
  }
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto check_buffer_num = buffer_config_list[i].pingpong ? 2U : 1U;
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (buffer_config_list[i].initial_state != resource_state_list[i][0][k]) {
        barrier_num[0][0]++;
      }
      for (uint32_t j = 1; j <= last_user_pass[i][k]; j++) {
        if (resource_state_list[i][j][k] != resource_state_list[i][j - 1][k]) {
          barrier_num[0][j]++;
        }
      }
      if (resource_state_list[i][last_user_pass[i][k]][k] != buffer_config_list[i].initial_state) {
        barrier_num[1][last_user_pass[i][k]]++;
      }
    }
  }
  auto barrier_config_list = AllocateArray<Barrier**>(retval_memory_type, kBarrierExecutionTimingNum);
  auto barrier_index = AllocateArrayFrame<uint32_t*>(kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_config_list[i] = AllocateArray<Barrier*>(retval_memory_type, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      barrier_config_list[i][j] = (barrier_num[i][j] == 0) ? nullptr : AllocateArray<Barrier>(retval_memory_type, barrier_num[i][j]);
    }
    barrier_index[i] = AllocateArrayFrame<uint32_t>(render_pass_num);
  }
  for (uint32_t i = 0; i < render_pass_num; i++) {
    for (uint32_t j = 0; j < render_pass_list[i].prepass_barrier_num; j++) {
      memcpy(&barrier_config_list[0][i][j], &render_pass_list[i].prepass_barrier[j], sizeof(barrier_config_list[0][i][j]));
    }
    barrier_index[0][i] = render_pass_list[i].prepass_barrier_num;
    for (uint32_t j = 0; j < render_pass_list[i].postpass_barrier_num; j++) {
      memcpy(&barrier_config_list[1][i][j], &render_pass_list[i].postpass_barrier[j], sizeof(barrier_config_list[1][i][j]));
    }
    barrier_index[1][i] = render_pass_list[i].postpass_barrier_num;
  }
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto check_buffer_num = buffer_config_list[i].pingpong ? 2U : 1U;
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (buffer_config_list[i].initial_state != resource_state_list[i][0][k]) {
        auto& barrier = barrier_config_list[0][0][barrier_index[0][0]];
        barrier.buffer_index = i;
        barrier.local_index = k;
        barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.state_before = ConvertToD3d12ResourceState(buffer_config_list[i].initial_state);
        barrier.state_after  = ConvertToD3d12ResourceState(resource_state_list[i][0][k]);
        barrier_index[0][0]++;
      }
      for (uint32_t j = 1; j <= last_user_pass[i][k]; j++) {
        if (resource_state_list[i][j][k] != resource_state_list[i][j - 1][k]) {
          auto& barrier = barrier_config_list[0][j][barrier_index[0][j]];
          barrier.buffer_index = i;
          barrier.local_index = k;
          barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
          barrier.state_before = ConvertToD3d12ResourceState(resource_state_list[i][j - 1][k]);
          barrier.state_after  = ConvertToD3d12ResourceState(resource_state_list[i][j][k]);
          barrier_index[0][j]++;
        }
      }
      if (resource_state_list[i][last_user_pass[i][k]][k] != buffer_config_list[i].initial_state) {
        auto& barrier = barrier_config_list[1][last_user_pass[i][k]][barrier_index[1][last_user_pass[i][k]]];
        barrier.buffer_index = i;
        barrier.local_index = k;
        barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.state_before = ConvertToD3d12ResourceState(resource_state_list[i][last_user_pass[i][k]][k]);
        barrier.state_after  = ConvertToD3d12ResourceState(buffer_config_list[i].initial_state);
        barrier_index[1][last_user_pass[i][k]]++;
      }
    }
  }
#ifdef BUILD_WITH_TEST
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    CAPTURE(i);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      CAPTURE(j);
      CHECK_EQ(barrier_index[i][j], barrier_num[i][j]);
    }
  }
#endif
  return {barrier_num, barrier_config_list};
}
} // namespace illuminate
TEST_CASE("buffer state change") {
  using namespace illuminate;
  BufferConfig buffer_config_list[]{
    {.buffer_index = 0, .initial_state = ResourceStateType::kRtv, .pingpong = true, },
    {.buffer_index = 1, .initial_state = ResourceStateType::kRtv, .pingpong = false, },
    {.buffer_index = 2, .descriptor_only = true, },
    {.buffer_index = 3, .initial_state = ResourceStateType::kRtv, .pingpong = false, },
  };
  RenderPass render_pass_list[]{{},{},{},};
  RenderPassBuffer buffer_list0[] = {
    {
      .buffer_index = 0,
      .state = ResourceStateType::kRtv,
    },
    {
      .buffer_index = 3,
      .state = ResourceStateType::kRtv,
    },
  };
  RenderPassBuffer buffer_list1[] = {
    {
      .buffer_index = 0,
      .state = ResourceStateType::kRtv,
    },
    {
      .buffer_index = 0,
      .state = ResourceStateType::kSrvPs,
    },
    {
      .buffer_index = 1,
      .state = ResourceStateType::kRtv,
    },
    {
      .buffer_index = 3,
      .state = ResourceStateType::kSrvPs,
    },
  };
  RenderPassBuffer buffer_list2[] = {
    {
      .buffer_index = 0,
      .state = ResourceStateType::kRtv,
    },
    {
      .buffer_index = 0,
      .state = ResourceStateType::kSrvPs,
    },
    {
      .buffer_index = 1,
      .state = ResourceStateType::kSrvPs,
    },
  };
  Barrier prepass_barrier[] = {
    {
      .buffer_index = 2,
      .local_index = 0,
      .type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .state_before = D3D12_RESOURCE_STATE_PRESENT,
      .state_after  = D3D12_RESOURCE_STATE_RENDER_TARGET,
    },
  };
  Barrier postpass_barrier[] = {
    {
      .buffer_index = 2,
      .local_index = 0,
      .type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .state_before = D3D12_RESOURCE_STATE_RENDER_TARGET,
      .state_after  = D3D12_RESOURCE_STATE_PRESENT,
    },
  };
  render_pass_list[0].buffer_num = countof(buffer_list0);
  render_pass_list[0].buffer_list = buffer_list0;
  render_pass_list[1].buffer_num = countof(buffer_list1);
  render_pass_list[1].buffer_list = buffer_list1;
  render_pass_list[1].prepass_barrier_num = countof(prepass_barrier);
  render_pass_list[1].prepass_barrier = prepass_barrier;
  render_pass_list[1].postpass_barrier_num = countof(postpass_barrier);
  render_pass_list[1].postpass_barrier = postpass_barrier;
  render_pass_list[2].buffer_num = countof(buffer_list2);
  render_pass_list[2].buffer_list = buffer_list2;
  auto pingpong_buffer_write_to_sub_list = AllocateArray<bool*>(MemoryType::kScene, countof(buffer_config_list));
  pingpong_buffer_write_to_sub_list[0] = AllocateArray<bool>(MemoryType::kScene, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[1] = AllocateArray<bool>(MemoryType::kScene, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[2] = AllocateArray<bool>(MemoryType::kScene, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[3] = AllocateArray<bool>(MemoryType::kScene, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[0][0] = false;
  pingpong_buffer_write_to_sub_list[0][1] = true;
  pingpong_buffer_write_to_sub_list[0][2] = false;
  pingpong_buffer_write_to_sub_list[1][0] = false;
  pingpong_buffer_write_to_sub_list[1][1] = false;
  pingpong_buffer_write_to_sub_list[1][2] = false;
  pingpong_buffer_write_to_sub_list[2][0] = false;
  pingpong_buffer_write_to_sub_list[2][1] = false;
  pingpong_buffer_write_to_sub_list[2][2] = false;
  pingpong_buffer_write_to_sub_list[3][0] = false;
  pingpong_buffer_write_to_sub_list[3][1] = false;
  pingpong_buffer_write_to_sub_list[3][2] = false;
  const bool render_pass_enable_flag[] = {true,true,true,};
  auto [resource_state_list, last_user_pass] = ConfigureRenderPassResourceStates(countof(render_pass_list), render_pass_list, countof(buffer_config_list), buffer_config_list, (const bool**)pingpong_buffer_write_to_sub_list, render_pass_enable_flag, 0, nullptr, MemoryType::kScene);
  // resource_state_list[buffer_index][pass_index][main(0)/sub(1)]
  CHECK_EQ(resource_state_list[0][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[0][1][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resource_state_list[0][2][0], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[0][0][1], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[0][1][1], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[0][2][1], ResourceStateType::kSrvPs);
  CHECK_EQ(resource_state_list[1][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[1][1][0], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[1][2][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resource_state_list[3][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resource_state_list[3][1][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resource_state_list[3][2][0], ResourceStateType::kRtv);
  CHECK_EQ(last_user_pass[0][0], 2);
  CHECK_EQ(last_user_pass[0][1], 2);
  CHECK_EQ(last_user_pass[1][0], 2);
  CHECK_EQ(last_user_pass[3][0], 1);
  auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(countof(render_pass_list), render_pass_list, countof(buffer_config_list), buffer_config_list, resource_state_list, last_user_pass, MemoryType::kScene);
  // barrier_num[prepass(0)/postpass(1)][pass_index][barrier_index]
  CHECK_EQ(barrier_num[0][0], 0);
  CHECK_EQ(barrier_num[1][0], 0);
  CHECK_EQ(barrier_num[0][1], 3);
  CHECK_EQ(barrier_num[1][1], 2);
  CHECK_EQ(barrier_num[0][2], 3);
  CHECK_EQ(barrier_num[1][2], 2);
  // barrier_config_list[prepass(0)/postpass(1)][pass_index][barrier_index]
  CHECK_EQ(barrier_config_list[0][1][0].buffer_index, 2);
  CHECK_EQ(barrier_config_list[0][1][0].local_index, 0);
  CHECK_EQ(barrier_config_list[0][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][0].state_before, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[0][1][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][1].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][1][1].local_index, 0);
  CHECK_EQ(barrier_config_list[0][1][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][1][2].buffer_index, 3);
  CHECK_EQ(barrier_config_list[0][1][2].local_index, 0);
  CHECK_EQ(barrier_config_list[0][1][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][2].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][2].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][1][0].buffer_index, 2);
  CHECK_EQ(barrier_config_list[1][1][0].local_index, 0);
  CHECK_EQ(barrier_config_list[1][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][1][0].state_after, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[1][1][1].buffer_index, 3);
  CHECK_EQ(barrier_config_list[1][1][1].local_index, 0);
  CHECK_EQ(barrier_config_list[1][1][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][1].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][1][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][0].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][2][0].local_index, 0);
  CHECK_EQ(barrier_config_list[0][2][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][0].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][1].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][2][1].local_index, 1);
  CHECK_EQ(barrier_config_list[0][2][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][2].buffer_index, 1);
  CHECK_EQ(barrier_config_list[0][2][2].local_index, 0);
  CHECK_EQ(barrier_config_list[0][2][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][2].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][2].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][2][0].buffer_index, 0);
  CHECK_EQ(barrier_config_list[1][2][0].local_index, 1);
  CHECK_EQ(barrier_config_list[1][2][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][2][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][2][0].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][2][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][2][1].buffer_index, 1);
  CHECK_EQ(barrier_config_list[1][2][1].local_index, 0);
  CHECK_EQ(barrier_config_list[1][2][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][2][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][2][1].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][2][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  ResetAllocation(MemoryType::kScene);
}
