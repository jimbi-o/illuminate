#ifndef ILLUMINATE_D3D12_HEADER_COMMON_H
#define ILLUMINATE_D3D12_HEADER_COMMON_H
#include "../header_common.h"
#include <dxgi1_6.h>
#include <d3d12.h>
namespace illuminate {
using DxgiFactory = IDXGIFactory7;
using DxgiAdapter = IDXGIAdapter4;
using DxgiSwapchain = IDXGISwapChain4;
using D3d12Device = ID3D12Device10;
using D3d12CommandQueue = ID3D12CommandQueue;
using D3d12Fence = ID3D12Fence1;
using D3d12CommandAllocator = ID3D12CommandAllocator;
using D3d12CommandList = ID3D12GraphicsCommandList7;
static const uint32_t kCommandQueueTypeNum = 3;
constexpr auto GetCommandQueueTypeIndex(const D3D12_COMMAND_LIST_TYPE type) {
  switch (type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:  { return 0; }
    case D3D12_COMMAND_LIST_TYPE_COMPUTE: { return 1; }
    case D3D12_COMMAND_LIST_TYPE_COPY:    { return 2; }
    default: { return 0; }
  }
}
}
#endif
