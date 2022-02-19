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
}
#endif
