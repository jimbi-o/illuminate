#include "d3d12_src_common.h"
#include "D3D12MemAlloc.h"
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
    },
    {
      "name": "queue_compute",
      "type": "compute",
      "priority": "normal",
      "command_list_num": 1
    }
  ],
  "command_allocator": {
    "direct": 1,
    "compute": 1
  },
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
  },
  "buffers": [
    {
      "name": "uav",
      "format": "R8G8B8A8_UNORM"
    }
  ],
  "render_pass": [
    {
      "name": "clear uav",
      "command_queue": "queue_compute",
      "buffers": [
        {
          "name": "uav",
          "state": "uav"
        }
      ],
      "pass_vars": {
        "clear_color": [0, 1, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ]
    },
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
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    switch (barrier.Type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource   = resource[i];
        barrier.Transition.StateBefore = config.state_before;
        barrier.Transition.StateAfter  = config.state_after;
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        assert(false && "aliasing barrier not implemented yet");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        barrier.UAV.pResource   = resource[i];
        break;
      }
    }
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
void ClearUav(D3d12CommandList* command_list, const void* pass_vars, const D3D12_CPU_DESCRIPTOR_HANDLE* uav) {
  // command_list->ClearRenderTargetView(*uav, static_cast<const FLOAT*>(pass_vars), 0, nullptr);
}
void ParsePassParamClearUav(const nlohmann::json& j, void* dst) {
  // TODO
}
void* D3d12BufferAllocatorAllocCallback(size_t Size, size_t Alignment, [[maybe_unused]] void* pUserData) {
  return gSystemMemoryAllocator->Allocate(Size, Alignment);
}
void D3d12BufferAllocatorFreeCallback([[maybe_unused]] void* pMemory, [[maybe_unused]] void* pUserData) {}
auto GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device) {
  using namespace D3D12MA;
  ALLOCATION_CALLBACKS allocation_callbacks{
    .pAllocate = D3d12BufferAllocatorAllocCallback,
    .pFree = D3d12BufferAllocatorFreeCallback,
    .pUserData = nullptr,
  };
  ALLOCATOR_DESC desc{
    .Flags = ALLOCATOR_FLAG_NONE,
    .pDevice = device,
    .PreferredBlockSize = 0, // will use default (64MiB)
    .pAllocationCallbacks = &allocation_callbacks,
    .pAdapter = adapter,
  };
  Allocator* allocator{nullptr};
  auto hr = CreateAllocator(&desc, &allocator);
  if (FAILED(hr)) {
    logerror("CreateAllocator failed. {}", hr);
  }
  return allocator;
}
struct BufferAllocation {
  D3D12MA::Allocation* allocation{nullptr};
  ID3D12Resource* resource{nullptr};
};
auto CreateBuffer(const BufferConfig& config, D3D12MA::Allocator* allocator) {
  using namespace D3D12MA;
  ALLOCATION_DESC allocation_desc{
    .Flags = ALLOCATION_FLAG_NONE,
    .HeapType = config.heap_type,
    .ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
    .CustomPool = nullptr,
  };
  D3D12_RESOURCE_DESC1 resource_desc{
    .Dimension = config.dimension,
    .Alignment = config.alignment,
    .Width = config.width,
    .Height = config.height,
    .DepthOrArraySize = config.depth_or_array_size,
    .MipLevels = config.miplevels,
    .Format = config.format,
    .SampleDesc = {
      .Count = config.sample_count,
      .Quality = config.sample_quality,
    },
    .Layout = config.layout,
    .Flags = config.flags,
    .SamplerFeedbackMipRegion = {
      .Width = config.mip_width,
      .Height = config.mip_height,
      .Depth = config.mip_depth,
    },
  };
  BufferAllocation buffer_allocation{};
  auto hr = allocator->CreateResource2(&allocation_desc, &resource_desc, config.initial_state, &config.clear_value, nullptr, &buffer_allocation.allocation, IID_PPV_ARGS(&buffer_allocation.resource));
  if (FAILED(hr)) {
    logerror("failed to create resource. {}", hr);
    assert(false && "failed to create resource");
    if (buffer_allocation.allocation) {
      buffer_allocation.allocation->Release();
      buffer_allocation.allocation = nullptr;
    }
    if (buffer_allocation.resource) {
      buffer_allocation.resource->Release();
      buffer_allocation.resource = nullptr;
    }
  }
  return buffer_allocation;
}
} // namespace anonymous
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(&allocator);
  HashMap<uint32_t, MemoryAllocationJanitor> render_pass_var_size(&allocator);
  HashMap<RenderPassVarParseFunction, MemoryAllocationJanitor> render_pass_var_parse_functions(&allocator);
  CHECK(render_pass_functions.Insert(SID("output to swapchain"), ClearRtv));
  CHECK(render_pass_var_size.Insert(SID("output to swapchain"), sizeof(FLOAT) * 4));
  CHECK(render_pass_var_parse_functions.Insert(SID("output to swapchain"), ParsePassParamClearRtv));
  CHECK(render_pass_functions.Insert(SID("clear uav"), ClearUav));
  CHECK(render_pass_var_size.Insert(SID("clear uav"), sizeof(FLOAT) * 4)); // TODO
  CHECK(render_pass_var_parse_functions.Insert(SID("clear uav"), ParsePassParamClearUav));
  RenderGraph render_graph;
  ParseRenderGraphJson(GetTestJson(), render_pass_var_size, render_pass_var_parse_functions, &allocator, &render_graph);
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
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  auto buffer = CreateBuffer(render_graph.buffer[0], buffer_allocator);
  CHECK_NE(buffer.allocation, nullptr);
  CHECK_NE(buffer.resource, nullptr);
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
#if 0
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
#endif
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  loginfo("memory global:{} temp:{}", gSystemMemoryAllocator->GetOffset(), tmp_memory_max_offset);
  gSystemMemoryAllocator->Reset();
}