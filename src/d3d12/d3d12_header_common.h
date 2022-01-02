#ifndef ILLUMINATE_HEADER_COMMON_H
#define ILLUMINATE_HEADER_COMMON_H
#include <dxgi1_6.h>
#include <d3d12.h>
namespace illuminate {
using DxgiFactory = IDXGIFactory7;
using DxgiAdapter = IDXGIAdapter4;
using DxgiSwapchain = IDXGISwapChain4;
using D3d12Device = ID3D12Device10;
using D3d12CommandList = ID3D12GraphicsCommandList7;
}
#endif
