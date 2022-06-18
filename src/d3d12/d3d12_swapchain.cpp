#include "d3d12_swapchain.h"
#include "d3d12_src_common.h"
namespace illuminate {
bool Swapchain::Init(DxgiFactory* factory, D3d12CommandQueue* command_queue, D3d12Device* const device, HWND hwnd, const DXGI_FORMAT format, const uint32_t swapchain_buffer_num, const uint32_t frame_latency, const DXGI_USAGE usage) {
  format_ = format;
  swapchain_buffer_num_ = swapchain_buffer_num;
  // tearing support
  {
    BOOL result = false; // doesn't work with "bool".
    auto hr = factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &result, sizeof(result));
    if (FAILED(hr)) {
      logwarn("CheckFeatureSupport(tearing) failed. {}", hr);
    }
    logtrace("Tearing support:{}", result);
    tearing_support_ = result;
  }
  // disable alt+enter fullscreen
  {
    auto hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
      logwarn("MakeWindowAssociation failed. {}", hr);
    }
  }
  // create swapchain
  {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = desc.Height = 0; // get value from hwnd
    desc.Format = format_;
    desc.Stereo = false;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = usage;
    desc.BufferCount = swapchain_buffer_num_;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = (tearing_support_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    IDXGISwapChain1* swapchain = nullptr;
    auto hr = factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, nullptr, nullptr, &swapchain);
    if (FAILED(hr)) {
      logerror("CreateSwapChainForHwnd failed. {} {} {} {}", hr, swapchain_buffer_num, frame_latency, usage);
      assert(false && "CreateSwapChainForHwnd failed.");
      return false;
    }
    hr = swapchain->QueryInterface(IID_PPV_ARGS(&swapchain_));
    swapchain->Release();
    if (FAILED(hr)) {
      logerror("swapchain->QueryInterface failed. {} {} {} {}", hr, swapchain_buffer_num, frame_latency, usage);
      assert(false && "swapchain->QueryInterface failed.");
      return false;
    }
  }
  // set max frame latency
  {
    auto hr = swapchain_->SetMaximumFrameLatency(frame_latency);
    if (FAILED(hr)) {
      logwarn("SetMaximumFrameLatency failed. {} {}", hr, frame_latency);
    }
  }
  // get frame latency object
  {
    frame_latency_waitable_object_ = swapchain_->GetFrameLatencyWaitableObject();
  }
  // get swapchain params
  {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    auto hr = swapchain_->GetDesc1(&desc);
    if (FAILED(hr)) {
      logwarn("swapchain_->GetDesc1 failed. {}", hr);
    } else {
      width_ = desc.Width;
      height_ = desc.Height;
      format_ = desc.Format;
      swapchain_buffer_num_ = desc.BufferCount;
    }
  }
  // get swapchain resource buffers for rtv
  {
    resources_ = AllocateArray<ID3D12Resource*>(gSystemMemoryAllocator, swapchain_buffer_num_);
    for (uint32_t i = 0; i < swapchain_buffer_num_; i++) {
      ID3D12Resource* resource = nullptr;
      auto hr = swapchain_->GetBuffer(i, IID_PPV_ARGS(&resource));
      if (FAILED(hr)) {
        logerror("swapchain_->GetBuffer failed. {} {}", i, hr);
        assert(false && "swapchain_->GetBuffer failed.");
        for (uint32_t j = 0; j < i; j++) {
          resources_[i]->Release();
        }
        for (uint32_t j = 0; j < swapchain_buffer_num_; j++) {
          resources_[i] = nullptr;
        }
        return false;
      }
      resources_[i] = resource;
      SetD3d12Name(resources_[i], "swapchain" + std::to_string(i));
    }
  }
  // prepare rtv
  {
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc {
      .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
      .NumDescriptors = swapchain_buffer_num_,
      .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
      .NodeMask = 0
    };
    auto hr = device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap_));
    if (FAILED(hr)) {
      logerror("swapchain CreateDescriptorHeap failed. {} {}", hr, swapchain_buffer_num_);
      assert(false && "swapchain CreateDescriptorHeap failed.");
      return false;
    }
    descriptor_heap_->SetName(L"swapchain descriptor heap");
    const D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
      .Format = format_,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
      .Texture2D = {},
    };
    auto rtv_handle = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
    auto rtv_step_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    cpu_handles_rtv_ = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(gSystemMemoryAllocator, swapchain_buffer_num_);
    for (uint32_t i = 0; i < swapchain_buffer_num_; i++) {
      device->CreateRenderTargetView(resources_[i], &rtv_desc, rtv_handle);
      cpu_handles_rtv_[i] = rtv_handle;
      rtv_handle.ptr += rtv_step_size;
    }
  }
  return true;
}
void Swapchain::Term() {
  if (descriptor_heap_) {
    auto refval = descriptor_heap_->Release();
    if (refval != 0UL) {
      logerror("swapchain descriptor_heap_ reference left. {}", refval);
    }
  }
  for (uint32_t i = 0; i < swapchain_buffer_num_; i++) {
    if (resources_[i]) {
      resources_[i]->Release();
    }
  }
  if (swapchain_) {
    auto refval = swapchain_->Release();
    if (refval != 0UL) {
      logerror("swapchain reference left. {}", refval);
    }
  }
}
void Swapchain::UpdateBackBufferIndex() {
  auto hr = WaitForSingleObjectEx(frame_latency_waitable_object_, INFINITE, FALSE);
  if (FAILED(hr)) {
    logwarn("WaitForSingleObjectEx failed. {}", hr);
    return;
  }
  buffer_index_ = swapchain_->GetCurrentBackBufferIndex();
}
bool Swapchain::Present() {
  auto hr = swapchain_->Present(vsync_ ? 1 : 0, (!vsync_ && tearing_support_) ? DXGI_PRESENT_ALLOW_TEARING : 0);
  if (FAILED(hr)) {
    logwarn("Swapchain::Present failed. {}", hr);
    return false;
  }
  return true;
}
}
#ifdef BUILD_WITH_TEST
#include "doctest/doctest.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_win32_window.h"
TEST_CASE("swapchain/direct queue") { // NOLINT
  const uint32_t swapchain_buffer_num = 3;
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init("swapchain test/direct", 160, 90, nullptr)); // NOLINT
  auto raw_command_queue_list = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), 1, &raw_command_queue_list);
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), raw_command_queue_list, device.Get(), window.GetHwnd(), DXGI_FORMAT_R8G8B8A8_UNORM, swapchain_buffer_num, swapchain_buffer_num - 1, DXGI_USAGE_RENDER_TARGET_OUTPUT)); // NOLINT
  CHECK_EQ(swapchain.GetDxgiFormat(), DXGI_FORMAT_R8G8B8A8_UNORM);
  CHECK_GT(swapchain.GetWidth(), 0);
  CHECK_GT(swapchain.GetHeight(), 0);
  CHECK_LE(swapchain.GetWidth(), 160);
  CHECK_LE(swapchain.GetHeight(), 90);
  for (uint32_t i = 0; i < swapchain_buffer_num; i++) {
    CHECK_GT(swapchain.GetRtvHandle().ptr, 0);
  }
  for (uint32_t i = 0; i < swapchain_buffer_num * 3; i++) {
    CAPTURE(i);
    CHECK_UNARY(swapchain.GetResource());
    swapchain.UpdateBackBufferIndex();
    CHECK_UNARY(swapchain.Present()); // NOLINT
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  command_queue_signals.Term();
  raw_command_queue_list->Release();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
#if 0
// error at Swapchain::Init
TEST_CASE("swapchain/compute queue") { // NOLINT
  const uint32_t swapchain_buffer_num = 3;
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init("swapchain test/compute", 160, 90)); // NOLINT
  const uint32_t command_queue_num = 1;
  auto raw_command_queue_list = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), 1, &raw_command_queue_list);
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), raw_command_queue_list, device.Get(), window.GetHwnd(), DXGI_FORMAT_R8G8B8A8_UNORM, swapchain_buffer_num, swapchain_buffer_num - 1, DXGI_USAGE_UNORDERED_ACCESS)); // NOLINT
  CHECK_EQ(swapchain.GetDxgiFormat(), DXGI_FORMAT_R8G8B8A8_UNORM);
  CHECK_GT(swapchain.GetWidth(), 0);
  CHECK_GT(swapchain.GetHeight(), 0);
  CHECK_LE(swapchain.GetWidth(), 160);
  CHECK_LE(swapchain.GetHeight(), 90);
  for (uint32_t i = 0; i < swapchain_buffer_num; i++) {
    CHECK_GT(swapchain.GetRtvHandle().ptr, 0);
  }
  for (uint32_t i = 0; i < swapchain_buffer_num * 3; i++) {
    CAPTURE(i);
    CHECK_UNARY(swapchain.GetResource());
    swapchain.UpdateBackBufferIndex();
    CHECK_UNARY(swapchain.Present()); // NOLINT
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  command_queue_signals.Term();
  raw_command_queue_list->Release();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
#endif
#endif

