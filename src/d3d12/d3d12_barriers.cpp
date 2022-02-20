#include "d3d12_barriers.h"
#include "d3d12_src_common.h"
namespace illuminate {
const ResourceStateType*** ConfigureRenderPassResourceStates(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const bool** pingpong_buffer_write_to_sub_list, const bool* render_pass_enable_flag, MemoryAllocationJanitor* allocator) {
  auto resource_state_list = AllocateArray<ResourceStateType**>(allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto pingpong_buffer = buffer_config_list[i].pingpong;
    const auto buffer_allocation_num = pingpong_buffer ? 2U : 1U;
    resource_state_list[i] = AllocateArray<ResourceStateType*>(allocator, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      resource_state_list[i][j] = AllocateArray<ResourceStateType>(allocator, buffer_allocation_num);
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
    auto last_pass_index_main = 0;
    auto last_pass_index_sub = 0;
    for (uint32_t j = 0; j < render_pass_num; j++) {
      if (render_pass_enable_flag[j]) {
        uint32_t buffer_count = 0;
        for (uint32_t k = 0; k < render_pass_list[j].buffer_num; k++) {
          if (render_pass_list[j].buffer_list[k].buffer_index == i) {
            if (pingpong_buffer) {
              auto read_only = IsResourceStateReadOnly(render_pass_list[j].buffer_list[k].state);
              auto write_to_sub = pingpong_buffer_write_to_sub_list[i][j];
              if (read_only && write_to_sub || !read_only && !write_to_sub) {
                state_main = render_pass_list[j].buffer_list[k].state;
                last_pass_index_main = j;
              } else {
                state_sub = render_pass_list[j].buffer_list[k].state;
                last_pass_index_sub = j;
              }
            } else {
              state_main = render_pass_list[j].buffer_list[k].state;
              last_pass_index_main = j;
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
    if (resource_state_list[i][last_pass_index_main][0] != buffer_config_list[i].initial_state) {
      for (uint32_t j = last_pass_index_main + 1; j < render_pass_num; j++) {
        resource_state_list[i][j][0] = buffer_config_list[i].initial_state;
      }
    }
    if (pingpong_buffer && resource_state_list[i][last_pass_index_sub][1] != buffer_config_list[i].initial_state) {
      for (uint32_t j = last_pass_index_sub + 1; j < render_pass_num; j++) {
        resource_state_list[i][j][1] = buffer_config_list[i].initial_state;
      }
    }
  }
  return (const ResourceStateType***)resource_state_list;
}
std::tuple<const uint32_t**, const Barrier***> ConfigureBarrierTransitions(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const ResourceStateType*** resource_state_list, MemoryAllocationJanitor* allocator) {
  // command queue type not considered yet.
  auto barrier_num = AllocateArray<uint32_t*>(allocator, kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_num[i] = AllocateArray<uint32_t>(allocator, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      barrier_num[i][j] = (i == 0) ? render_pass_list[j].prepass_barrier_num : render_pass_list[j].postpass_barrier_num;
    }
  }
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto is_pingpong_buffer = buffer_config_list[i].pingpong;
    const auto check_buffer_num = is_pingpong_buffer ? 2U : 1U;
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (buffer_config_list[i].initial_state != resource_state_list[i][0][k]) {
        barrier_num[0][0]++;
      }
    }
    for (uint32_t j = 1; j < render_pass_num; j++) {
      for (uint32_t k = 0; k < check_buffer_num; k++) {
        if (resource_state_list[i][j][k] != resource_state_list[i][j - 1][k]) {
          barrier_num[0][j]++;
        }
      }
    }
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (resource_state_list[i][render_pass_num - 1][k] != buffer_config_list[i].initial_state) {
        barrier_num[1][render_pass_num - 1]++;
      }
    }
  }
  auto barrier_config_list = AllocateArray<Barrier**>(allocator, kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_config_list[i] = AllocateArray<Barrier*>(allocator, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      barrier_config_list[i][j] = (barrier_num[i][j] == 0) ? nullptr : AllocateArray<Barrier>(allocator, barrier_num[i][j]);
    }
  }
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto barrier_index = AllocateArray<uint32_t*>(&tmp_allocator, kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_index[i] = AllocateArray<uint32_t>(allocator, render_pass_num);
    for (uint32_t j = 1; j < render_pass_num; j++) {
      barrier_index[i][j] = 0;
    }
  }
  for (uint32_t i = 0; i < render_pass_num; i++) {
    for (uint32_t j = 0; j < render_pass_list[i].prepass_barrier_num; j++) {
      memcpy(&barrier_config_list[0][i][j], &render_pass_list[i].prepass_barrier[j], sizeof(barrier_config_list[0][i][j]));
      barrier_index[0][i]++;
    }
    for (uint32_t j = 0; j < render_pass_list[i].postpass_barrier_num; j++) {
      memcpy(&barrier_config_list[1][i][j], &render_pass_list[i].postpass_barrier[j], sizeof(barrier_config_list[1][i][j]));
      barrier_index[1][i]++;
    }
  }
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto is_pingpong_buffer = buffer_config_list[i].pingpong;
    const auto check_buffer_num = is_pingpong_buffer ? 2U : 1U;
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (buffer_config_list[i].initial_state != resource_state_list[i][0][k]) {
        auto& barrier = barrier_config_list[0][0][barrier_index[0][0]];
        barrier.buffer_index = i;
        barrier.pingpong_buffer_type = (k == 0) ? PingPongBufferType::kMain : PingPongBufferType::kSub;
        barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.state_before = ConvertToD3d12ResourceState(buffer_config_list[i].initial_state);
        barrier.state_after  = ConvertToD3d12ResourceState(resource_state_list[i][0][k]);
        barrier_index[0][0]++;
      }
    }
    for (uint32_t j = 1; j < render_pass_num; j++) {
      for (uint32_t k = 0; k < check_buffer_num; k++) {
        if (resource_state_list[i][j][k] != resource_state_list[i][j - 1][k]) {
          auto& barrier = barrier_config_list[0][j][barrier_index[0][j]];
          barrier.buffer_index = i;
          barrier.pingpong_buffer_type = (k == 0) ? PingPongBufferType::kMain : PingPongBufferType::kSub;
          barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
          barrier.state_before = ConvertToD3d12ResourceState(resource_state_list[i][j - 1][k]);
          barrier.state_after  = ConvertToD3d12ResourceState(resource_state_list[i][j][k]);
          barrier_index[0][j]++;
        }
      }
    }
    for (uint32_t k = 0; k < check_buffer_num; k++) {
      if (resource_state_list[i][render_pass_num - 1][k] != buffer_config_list[i].initial_state) {
        auto& barrier = barrier_config_list[1][render_pass_num - 1][barrier_index[1][render_pass_num - 1]];
        barrier.buffer_index = i;
        barrier.pingpong_buffer_type = (k == 0) ? PingPongBufferType::kMain : PingPongBufferType::kSub;
        barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.state_before = ConvertToD3d12ResourceState(resource_state_list[i][render_pass_num - 1][k]);
        barrier.state_after  = ConvertToD3d12ResourceState(buffer_config_list[i].initial_state);
        barrier_index[1][render_pass_num - 1]++;
      }
    }
  }
  return std::make_tuple((const uint32_t**)barrier_num, (const Barrier***)barrier_config_list);
}
} // namespace illuminate
#include "doctest/doctest.h"
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
      .pingpong_buffer_type = PingPongBufferType::kMain,
      .type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .state_before = D3D12_RESOURCE_STATE_PRESENT,
      .state_after  = D3D12_RESOURCE_STATE_RENDER_TARGET,
    },
  };
  Barrier postpass_barrier[] = {
    {
      .buffer_index = 2,
      .pingpong_buffer_type = PingPongBufferType::kMain,
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
  auto allocator = GetTemporalMemoryAllocator();
  auto pingpong_buffer_write_to_sub_list = AllocateArray<bool*>(&allocator, countof(buffer_config_list));
  pingpong_buffer_write_to_sub_list[0] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[1] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[2] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[3] = AllocateArray<bool>(&allocator, countof(render_pass_list));
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
  auto resouce_state_list = ConfigureRenderPassResourceStates(countof(render_pass_list), render_pass_list, countof(buffer_config_list), buffer_config_list, (const bool**)pingpong_buffer_write_to_sub_list, render_pass_enable_flag, &allocator);
  // resouce_state_list[buffer_index][pass_index][main(0)/sub(1)]
  CHECK_EQ(resouce_state_list[0][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][1][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[0][2][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][0][1], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][1][1], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][2][1], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[1][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[1][1][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[1][2][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[3][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[3][1][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[3][2][0], ResourceStateType::kRtv);
  auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(countof(render_pass_list), render_pass_list, countof(buffer_config_list), buffer_config_list, (const ResourceStateType***)resouce_state_list, &allocator);
  // barrier_num[prepass(0)/postpass(1)][pass_index][barrier_index]
  CHECK_EQ(barrier_num[0][0], 0);
  CHECK_EQ(barrier_num[1][0], 0);
  CHECK_EQ(barrier_num[0][1], 3);
  CHECK_EQ(barrier_num[1][1], 1);
  CHECK_EQ(barrier_num[0][2], 4);
  CHECK_EQ(barrier_num[1][2], 2);
  // barrier_config_list[prepass(0)/postpass(1)][pass_index][barrier_index]
  CHECK_EQ(barrier_config_list[0][1][0].buffer_index, 2);
  CHECK_EQ(barrier_config_list[0][1][0].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][0].state_before, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[0][1][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][1].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][1][1].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][1][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][1][2].buffer_index, 3);
  CHECK_EQ(barrier_config_list[0][1][2].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][1][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][1][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][1][2].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][1][2].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][1][0].buffer_index, 2);
  CHECK_EQ(barrier_config_list[1][1][0].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[1][1][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][1][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][1][0].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][1][0].state_after, D3D12_RESOURCE_STATE_PRESENT);
  CHECK_EQ(barrier_config_list[0][2][0].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][2][0].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][2][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][0].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][1].buffer_index, 0);
  CHECK_EQ(barrier_config_list[0][2][1].pingpong_buffer_type, PingPongBufferType::kSub);
  CHECK_EQ(barrier_config_list[0][2][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][1].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][1].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][2].buffer_index, 1);
  CHECK_EQ(barrier_config_list[0][2][2].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][2][2].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][2].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][2].state_before, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[0][2][2].state_after, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][3].buffer_index, 3);
  CHECK_EQ(barrier_config_list[0][2][3].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[0][2][3].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[0][2][3].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[0][2][3].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[0][2][3].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][2][0].buffer_index, 0);
  CHECK_EQ(barrier_config_list[1][2][0].pingpong_buffer_type, PingPongBufferType::kSub);
  CHECK_EQ(barrier_config_list[1][2][0].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][2][0].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][2][0].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][2][0].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
  CHECK_EQ(barrier_config_list[1][2][1].buffer_index, 1);
  CHECK_EQ(barrier_config_list[1][2][1].pingpong_buffer_type, PingPongBufferType::kMain);
  CHECK_EQ(barrier_config_list[1][2][1].type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
  CHECK_EQ(barrier_config_list[1][2][1].flag, D3D12_RESOURCE_BARRIER_FLAG_NONE);
  CHECK_EQ(barrier_config_list[1][2][1].state_before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  CHECK_EQ(barrier_config_list[1][2][1].state_after, D3D12_RESOURCE_STATE_RENDER_TARGET);
}
