#include "d3d12_src_common.h"
#include "doctest/doctest.h"
#include <nlohmann/json.hpp>
#include "illuminate/util/hash_map.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_render_graph.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
namespace {
auto GetTestJson() {
  return R"(
{
  "frame_buffer_num": 2,
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
      "priority": "normal",
      "command_list_num": 1
    }
  ],
  "command_allocator": {
    "direct": 1
  },
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
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
template <typename A1, typename A2, typename A3>
RenderGraph GetTestRenderGraph(const HashMap<uint32_t, A1>& pass_var_size, const HashMap<RenderPassVarParseFunction, A2>& pass_var_func, A3* allocator) {
  RenderGraph graph;
  ParseRenderGraphJson(GetTestJson(), pass_var_size, pass_var_func, allocator, &graph);
  return graph;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = resource[i];
    barrier.Transition.StateBefore = config.state_before;
    barrier.Transition.StateAfter  = config.state_after;
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
typedef void (*RenderPassFunction)(D3d12CommandList*, const void*, const D3D12_CPU_DESCRIPTOR_HANDLE*);
void ClearRtv(D3d12CommandList* command_list, const void* pass_vars, const D3D12_CPU_DESCRIPTOR_HANDLE* rtv) {
  command_list->ClearRenderTargetView(*rtv, static_cast<const FLOAT*>(pass_vars), 0, nullptr);
}
void ParsePassParamClearRtv(const nlohmann::json& j, void* dst) {
  auto src_color = j.at("clear_color");
  auto dst_color = reinterpret_cast<FLOAT*>(dst);
  for (uint32_t i = 0; i < 4; i++) {
    dst_color[i] = src_color[i];
  }
}
} // namespace anonymous
} // namespace illuminate
TEST_CASE("d3d12 render graph") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(&allocator);
  HashMap<uint32_t, MemoryAllocationJanitor> render_pass_var_size(&allocator);
  HashMap<RenderPassVarParseFunction, MemoryAllocationJanitor> render_pass_var_parse_functions(&allocator);
  CHECK(render_pass_functions.Insert(SID("output to swapchain"), ClearRtv));
  CHECK(render_pass_var_size.Insert(SID("output to swapchain"), sizeof(FLOAT) * 4));
  CHECK(render_pass_var_parse_functions.Insert(SID("output to swapchain"), ParsePassParamClearRtv));
  auto render_graph = GetTestRenderGraph(render_pass_var_size, render_pass_var_parse_functions,  &allocator);
  const uint32_t swapchain_buffer_num = render_graph.frame_buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
  CommandListSet command_list_set;
  CHECK_UNARY(command_list_set.Init(device.Get(),
                                    render_graph.command_queue_num,
                                    render_graph.command_queue_type,
                                    render_graph.command_queue_priority,
                                    render_graph.command_list_num_per_queue,
                                    render_graph.frame_buffer_num,
                                    render_graph.command_allocator_num_per_queue_type));
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, swapchain_buffer_num, render_graph.frame_buffer_num, render_graph.swapchain_usage)); // NOLINT
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  uint32_t tmp_memory_max_offset = 0U;
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    auto used_command_queue = AllocateArray<bool>(&single_frame_allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      used_command_queue[j] = false;
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      auto command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
      auto resource = swapchain.GetResource(); // TODO
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, &resource);
      auto rtv = swapchain.GetRtvHandle(); // TODO
      (*render_pass_functions.Get(render_pass.name))(command_list, render_pass.pass_vars, &rtv);
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, &resource);
      used_command_queue[render_pass.command_queue_index] = true;
      command_list_set.ExecuteCommandList(render_pass.command_queue_index); // TODO
      frame_signals[frame_index][render_pass.command_queue_index] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  loginfo("memory global:{} temp:{}", gSystemMemoryAllocator->GetOffset(), tmp_memory_max_offset);
  gSystemMemoryAllocator->Reset();
}
