#ifndef ILLUMINATE_D3D12_SRC_COMMON_H
#define ILLUMINATE_D3D12_SRC_COMMON_H
#include "../src_common.h"
#include "d3d12_memory_allocators.h"
#include "spdlog/spdlog.h"
#define logtrace spdlog::trace
#define logdebug spdlog::debug
#define loginfo  spdlog::info
#define logwarn  spdlog::warn
#define logerror spdlog::error
#define logfatal spdlog::critical
namespace illuminate {
void SetD3d12Name(ID3D12Object* obj, const std::string_view name);
uint32_t GetD3d12Name(ID3D12Object* obj, const uint32_t dst_size, char* dst);
void CopyStrToWstrContainer(wchar_t** dst, const std::string_view src, MemoryAllocationJanitor* allocator);
inline auto CopyStrToWstrContainer(const std::string_view src, MemoryAllocationJanitor* allocator) {
  wchar_t* dst = nullptr;
  CopyStrToWstrContainer(&dst, src, allocator);
  return dst;
}
template <typename N>
uint32_t GetUint32(const N& n) {
  return static_cast<uint32_t>(n);
}
// https://stackoverflow.com/questions/49454005/how-to-get-an-array-size-at-compile-time
template<uint32_t N, class T>
constexpr auto countof(T(&)[N]) { return N; }
constexpr inline auto GetD3d12ResourceDescForBuffer(const uint32_t buffer_size) {
  return D3D12_RESOURCE_DESC1{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = buffer_size,
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {
      .Count = 1,
      .Quality = 0,
    },
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE,
    .SamplerFeedbackMipRegion = {
      .Width = 0,
      .Height = 0,
      .Depth = 0,
    },
  };
}
}
#endif
