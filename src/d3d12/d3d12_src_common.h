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
void SetD3d12Name(ID3D12Object* obj, const char* name);
uint32_t GetD3d12Name(ID3D12Object* obj, const uint32_t dst_size, char* dst);
void CopyStrToWstrContainer(wchar_t** dst, const std::string_view src, const MemoryType memory_type);
inline auto CopyStrToWstrContainer(const std::string_view src, const MemoryType memory_type) {
  wchar_t* dst = nullptr;
  CopyStrToWstrContainer(&dst, src, memory_type);
  return dst;
}
// https://stackoverflow.com/questions/49454005/how-to-get-an-array-size-at-compile-time
template<uint32_t N, class T>
constexpr auto countof(T(&)[N]) { return N; }
const char* CreateString(const char* const str, const MemoryType memory_type);
}
#endif
