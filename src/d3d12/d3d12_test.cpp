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
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  const uint32_t buffer_num = 2;
  const uint32_t swapchain_buffer_num = buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init("rtv clear test", 160, 90)); // NOLINT
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
  auto frame_signals = AllocateArray<uint64_t*>(gSystemMemoryAllocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(gSystemMemoryAllocator, command_queue_num);
    for (uint32_t j = 0; j < command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  for (uint32_t i = 0; i < frame_num; i++) {
    const auto frame_index = i % buffer_num;
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
