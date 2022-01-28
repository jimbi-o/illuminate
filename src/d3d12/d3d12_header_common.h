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
enum class DescriptorType : uint8_t { kCbv = 0, kSrv, kUav, kSampler, kRtv, kDsv, kNum, };
static const auto kDescriptorTypeNum = static_cast<uint32_t>(DescriptorType::kNum);
enum class ResourceStateType : uint8_t { kCbv = 0, kSrv, kUav, kRtv, kDsv, kCopySrc, kCopyDst, kNum, };
static const auto kResourceStateTypeNum = static_cast<uint32_t>(ResourceStateType::kNum);
constexpr auto ConvertToDescriptorType(const ResourceStateType& state) {
  switch (state) {
    case ResourceStateType::kCbv: { return DescriptorType::kCbv; };
    case ResourceStateType::kSrv: { return DescriptorType::kSrv; };
    case ResourceStateType::kUav: { return DescriptorType::kUav; };
    case ResourceStateType::kRtv: { return DescriptorType::kRtv; };
    case ResourceStateType::kDsv: { return DescriptorType::kDsv; };
  }
  return DescriptorType::kNum;
}
enum class BufferSizeRelativeness : uint8_t { kAbsolute, kSwapchainRelative, kPrimaryBufferRelative, };
struct Size2d {
  uint32_t width{};
  uint32_t height{};
};
struct MainBufferSize {
  Size2d swapchain{};
  Size2d primarybuffer{};
};
struct MainBufferFormat {
  DXGI_FORMAT swapchain{};
  DXGI_FORMAT primarybuffer{};
};
uint32_t GetPhysicalWidth(const MainBufferSize& buffer_size, const BufferSizeRelativeness& relativeness, const float scale);
uint32_t GetPhysicalHeight(const MainBufferSize& buffer_size, const BufferSizeRelativeness& relativeness, const float scale);
}
#endif
