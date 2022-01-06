#include "d3d12_src_common.h"
namespace illuminate {
}
#ifdef BUILD_WITH_TEST
#include "doctest/doctest.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include <nlohmann/json.hpp>
#include "d3d12_dxgi_core.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
namespace {
auto GetJson() {
  return R"(
{
  "buffer_num": 2,
  "frame_loop_num": 5,
  "window": {
    "title": "integration test",
    "width": 500,
    "height" : 300
  },
  "command_queue": [
    {
      "name": "queue_graphics",
      "type": "3d",
      "priority" : "normal"
    }
  ],
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM"
  },
  "render_pass": [
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "buffers": [
        {
          "name": "swapchain",
          "state": "rtv"
        }
      ],
      "pass_vars": {
        "clear_color": [0, 1, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "present",
          "state_after": "rtv"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "rtv",
          "state_after": "present"
        }
      ]
    }
  ]
}
)"_json;
}
struct RenderGraph {
  uint32_t buffer_num{0};
  uint32_t frame_loop_num{0};
  char* window_title{nullptr};
  uint32_t window_width{0};
  uint32_t window_height{0};
  uint32_t command_queue_num{0};
};
void to_json(nlohmann::json& j, const RenderGraph& r) {
  j = nlohmann::json{
    {"buffer_num", r.buffer_num},
    {"frame_loop_num", r.frame_loop_num},
  };
  j["window"] = nlohmann::json{
    {"title", r.window_title},
    {"width", r.window_width},
    {"height", r.window_height},
  };
}
void from_json(const nlohmann::json& j, RenderGraph& r) {
  j.at("buffer_num").get_to(r.buffer_num);
  j.at("frame_loop_num").get_to(r.frame_loop_num);
  auto window_title = j.at("window").at("title").get<std::string_view>();
  auto window_title_len = static_cast<uint32_t>(window_title.size()) + 1;
  r.window_title = AllocateArray<char>(gSystemMemoryAllocator, window_title_len);
  strcpy_s(r.window_title, window_title_len, window_title.data());
  j.at("window").at("width").get_to(r.window_width);
  j.at("window").at("height").get_to(r.window_height);
  auto& command_queues = j.at("command_queue");
  r.command_queue_num = static_cast<uint32_t>(command_queues.size());
}
RenderGraph GetRenderGraph() {
  return GetJson().get<RenderGraph>();
}
} // anonymous namespace
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  auto render_graph = GetRenderGraph();
  const uint32_t swapchain_buffer_num = render_graph.buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
  const uint32_t command_queue_num = 1;
  auto raw_command_queue_list = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
  const uint32_t command_queue_direct_index = 0;
  CommandQueueList command_queue_list;
  command_queue_list.Init(command_queue_num, &raw_command_queue_list);
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), command_queue_list.Num(), command_queue_list.GetList());
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_queue_list.Get(command_queue_direct_index), device.Get(), window.GetHwnd(), DXGI_FORMAT_R8G8B8A8_UNORM, swapchain_buffer_num, swapchain_buffer_num - 1, DXGI_USAGE_RENDER_TARGET_OUTPUT)); // NOLINT
  const uint32_t frame_num = 20;
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.buffer_num);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, command_queue_num);
    for (uint32_t j = 0; j < command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    const auto frame_index = i % render_graph.buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    swapchain.UpdateBackBufferIndex();
#if 0
    auto command_list = RetainCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);
    {
      D3D12_RESOURCE_BARRIER barrier{};
      {
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource   = swapchain.GetResource();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
      }
      command_list->ResourceBarrier(1, &barrier);
      const FLOAT clear_color[4] = {0.0f,1.0f,1.0f,1.0f};
      command_list->ClearRenderTargetView(swapchain.GetRtvHandle(), clear_color, 0, nullptr);
      {
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
      }
      command_list->ResourceBarrier(1, &barrier);
    }
    command_queue_list.Get(command_queue_direct_index)->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&command_list));
#endif
    frame_signals[frame_index][command_queue_direct_index] = command_queue_signals.SucceedSignal(command_queue_direct_index);
    swapchain.Present();
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  command_queue_signals.Term();
  command_queue_list.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
#endif
