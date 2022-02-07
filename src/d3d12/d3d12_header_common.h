#ifndef ILLUMINATE_D3D12_HEADER_COMMON_H
#define ILLUMINATE_D3D12_HEADER_COMMON_H
#include "../header_common.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include "WinPixEventRuntime/pix3.h"
namespace illuminate {
using DxgiFactory = IDXGIFactory7;
using DxgiAdapter = IDXGIAdapter4;
using DxgiSwapchain = IDXGISwapChain4;
#if 0
using D3d12Device = ID3D12Device10; // creation fails with graphics debugger
#else
using D3d12Device = ID3D12Device9;
#endif
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
using DescriptorTypeFlag = uint32_t;
static const DescriptorTypeFlag kDescriptorTypeFlagNone = 0x00;
static const DescriptorTypeFlag kDescriptorTypeFlagCbv  = 0x01;
static const DescriptorTypeFlag kDescriptorTypeFlagSrv  = 0x02;
static const DescriptorTypeFlag kDescriptorTypeFlagUav  = 0x04;
static const DescriptorTypeFlag kDescriptorTypeFlagRtv  = 0x08;
static const DescriptorTypeFlag kDescriptorTypeFlagDsv  = 0x10;
enum class ResourceStateType : uint8_t { kCbv = 0, kSrvPs, kSrvNonPs, kUav, kRtv, kDsvWrite, kCopySrc, kCopyDst, kCommon, kPresent, };
constexpr auto ConvertToD3d12ResourceState(const ResourceStateType type) {
  switch (type) {
    case ResourceStateType::kCbv:      { return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; }
    case ResourceStateType::kSrvPs:    { return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; }
    case ResourceStateType::kSrvNonPs: { return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; }
    case ResourceStateType::kUav:      { return D3D12_RESOURCE_STATE_UNORDERED_ACCESS; }
    case ResourceStateType::kRtv:      { return D3D12_RESOURCE_STATE_RENDER_TARGET; }
    case ResourceStateType::kDsvWrite: { return D3D12_RESOURCE_STATE_DEPTH_WRITE; }
    case ResourceStateType::kCommon:   { return D3D12_RESOURCE_STATE_COMMON; }
    case ResourceStateType::kPresent:  { return D3D12_RESOURCE_STATE_PRESENT; }
  }
  return D3D12_RESOURCE_STATE_COMMON;
}
constexpr auto ConvertToDescriptorType(const ResourceStateType& state) {
  switch (state) {
    case ResourceStateType::kCbv:      { return DescriptorType::kCbv; };
    case ResourceStateType::kSrvPs:    { return DescriptorType::kSrv; };
    case ResourceStateType::kSrvNonPs: { return DescriptorType::kSrv; };
    case ResourceStateType::kUav:      { return DescriptorType::kUav; };
    case ResourceStateType::kRtv:      { return DescriptorType::kRtv; };
    case ResourceStateType::kDsvWrite: { return DescriptorType::kDsv; };
  }
  return DescriptorType::kNum;
}
constexpr auto ConvertToDescriptorTypeFlag(const ResourceStateType& state) {
  switch (state) {
    case ResourceStateType::kCbv:      { return kDescriptorTypeFlagCbv; };
    case ResourceStateType::kSrvPs:    { return kDescriptorTypeFlagSrv; };
    case ResourceStateType::kSrvNonPs: { return kDescriptorTypeFlagSrv; };
    case ResourceStateType::kUav:      { return kDescriptorTypeFlagUav; };
    case ResourceStateType::kRtv:      { return kDescriptorTypeFlagRtv; };
    case ResourceStateType::kDsvWrite: { return kDescriptorTypeFlagDsv; };
  }
  return kDescriptorTypeFlagNone;
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
uint32_t GetDxgiFormatPerPixelSizeInBytes(const DXGI_FORMAT);
}
#endif
