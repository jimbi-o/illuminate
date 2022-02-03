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
void SetD3d12Name(ID3D12Object* obj, const std::string& name);
}
#endif
