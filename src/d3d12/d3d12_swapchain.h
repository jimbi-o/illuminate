#ifndef ILLUMINATE_D3D12_SWAPCHAIN_H
#define ILLUMINATE_D3D12_SWAPCHAIN_H
#include "d3d12_header_common.h"
namespace illuminate {
class Swapchain {
 public:
  // https://developer.nvidia.com/dx12-dos-and-donts
  // Try using about 1-2 more swap-chain buffers than you are intending to queue frames (in terms of command allocators and dynamic data and the associated frame fences) and set the "max frame latency" to this number of swap-chain buffers.
  bool Init(DxgiFactory* factory, D3d12CommandQueue* command_queue, D3d12Device* const device, HWND hwnd, const DXGI_FORMAT format, const uint32_t swapchain_buffer_num, const uint32_t frame_latency, const DXGI_USAGE usage = DXGI_USAGE_RENDER_TARGET_OUTPUT);
  void Term();
  void EnableVsync(const bool b) { vsync_ = b; }
  void UpdateBackBufferIndex();
  bool Present();
  auto GetResource() { return resources_[buffer_index_]; }
  const auto& GetRtvHandle() const { return cpu_handles_rtv_[buffer_index_]; }
  constexpr auto GetDxgiFormat() const { return format_; }
  constexpr auto GetWidth() const { return width_; }
  constexpr auto GetHeight() const { return height_; }
 private:
  DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
  uint32_t swapchain_buffer_num_{0};
  bool vsync_{true};
  bool tearing_support_{false};
  uint32_t width_{0};
  uint32_t height_{0};
  DxgiSwapchain* swapchain_{nullptr};
  HANDLE frame_latency_waitable_object_{nullptr};
  ID3D12Resource** resources_{nullptr};
  ID3D12DescriptorHeap* descriptor_heap_{nullptr};
  D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles_rtv_{nullptr};
  uint32_t buffer_index_{0};
};
}
#endif
