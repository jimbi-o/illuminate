#include "d3d12_barriers.h"
#include "d3d12_src_common.h"
namespace illuminate {
auto ConfigureRenderPassResourceStates(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, const BufferConfig* buffer_config_list, const bool** pingpong_buffer_write_to_sub_list, MemoryAllocationJanitor* allocator) {
  auto resource_state_list = AllocateArray<ResourceStateType**>(allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    const auto pingpong_buffer = buffer_config_list[i].pingpong;
    const auto buffer_allocation_num = pingpong_buffer ? 2U : 1U;
    resource_state_list[i] = AllocateArray<ResourceStateType*>(allocator, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      resource_state_list[i][j] = AllocateArray<ResourceStateType>(allocator, buffer_allocation_num);
    }
    auto state_main = buffer_config_list[i].initial_state;
    auto state_sub  = state_main;
    for (uint32_t j = 0; j < render_pass_num; j++) {
      uint32_t buffer_count = 0;
      for (uint32_t k = 0; k < render_pass_list[j].buffer_num; k++) {
        if (render_pass_list[j].buffer_list[k].buffer_index == i) {
          if (pingpong_buffer) {
            auto read_only = IsResourceStateReadOnly(render_pass_list[j].buffer_list[k].state);
            auto write_to_sub = pingpong_buffer_write_to_sub_list[i][j];
            if (read_only && write_to_sub || !read_only && !write_to_sub) {
              state_main = render_pass_list[j].buffer_list[k].state;
            } else {
              state_sub = render_pass_list[j].buffer_list[k].state;
            }
          } else {
            state_main = render_pass_list[j].buffer_list[k].state;
          }
          buffer_count++;
          if (buffer_count >= buffer_allocation_num) { break; }
        }
      }
      resource_state_list[i][j][0] = state_main;
      if (pingpong_buffer) {
        resource_state_list[i][j][1] = state_sub;
      }
    }
  }
  return resource_state_list;
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("buffer state change") {
  using namespace illuminate;
  BufferConfig buffer_config_list[]{
    {.buffer_index = 0, .initial_state = ResourceStateType::kRtv, .pingpong = true, },
    {.buffer_index = 1, .initial_state = ResourceStateType::kRtv, .pingpong = false, },
  };
  RenderPass render_pass_list[]{{},{},{},};
  RenderPassBuffer buffer_list0[] = {
    {
      .buffer_index = 0,
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
  };
  render_pass_list[0].buffer_num = GetUint32(countof(buffer_list0));
  render_pass_list[0].buffer_list = buffer_list0;
  render_pass_list[1].buffer_num = GetUint32(countof(buffer_list1));
  render_pass_list[1].buffer_list = buffer_list1;
  render_pass_list[2].buffer_num = GetUint32(countof(buffer_list2));
  render_pass_list[2].buffer_list = buffer_list2;
  auto allocator = GetTemporalMemoryAllocator();
  auto pingpong_buffer_write_to_sub_list = AllocateArray<bool*>(&allocator, countof(buffer_config_list));
  pingpong_buffer_write_to_sub_list[0] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[1] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[2] = AllocateArray<bool>(&allocator, countof(render_pass_list));
  pingpong_buffer_write_to_sub_list[0][0] = false;
  pingpong_buffer_write_to_sub_list[0][1] = true;
  pingpong_buffer_write_to_sub_list[0][2] = false;
  pingpong_buffer_write_to_sub_list[1][0] = false;
  pingpong_buffer_write_to_sub_list[1][1] = false;
  pingpong_buffer_write_to_sub_list[1][2] = false;
  auto resouce_state_list = ConfigureRenderPassResourceStates(countof(render_pass_list), render_pass_list, countof(buffer_config_list), buffer_config_list, (const bool**)pingpong_buffer_write_to_sub_list, &allocator);
  CHECK_EQ(resouce_state_list[0][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][1][0], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[0][2][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][0][1], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][1][1], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[0][2][1], ResourceStateType::kSrvPs);
  CHECK_EQ(resouce_state_list[1][0][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[1][1][0], ResourceStateType::kRtv);
  CHECK_EQ(resouce_state_list[1][2][0], ResourceStateType::kRtv);
}
// TODO test with render_pass_list[i].pre&post pass_barrier
// TODO remove barrier config in json (except swapchain)
