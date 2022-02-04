#ifndef ILLUMINATE_RENDER_PASS_D3D12_COMMON_H
#define ILLUMINATE_RENDER_PASS_D3D12_COMMON_H
#include "../d3d12_descriptors.h"
#include "../d3d12_header_common.h"
#include "../d3d12_scene.h"
#include "../d3d12_shader_compiler.h"
#include "../d3d12_src_common.h"
namespace illuminate {
template <typename A>
struct RenderPassInitArgs {
  const nlohmann::json* json{nullptr};
  const char* shader_code{nullptr};
  ShaderCompiler* shader_compiler{nullptr};
  DescriptorCpu<MemoryAllocationJanitor>* descriptor_cpu{nullptr};
  D3d12Device* device{nullptr};
  const MainBufferFormat& main_buffer_format{};
  HWND hwnd{nullptr};
  uint32_t frame_buffer_num{0};
  A* allocator{nullptr};
};
struct RenderPassArgs {
  D3d12CommandList* command_list;
  const MainBufferSize* main_buffer_size;
  const void* pass_vars_ptr;
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles;
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles;
  ID3D12Resource** resources;
  const SceneData* scene_data;
};
using RenderPassFunction = void (*)(RenderPassArgs*);
uint32_t GetShaderCompilerArgs(const nlohmann::json& j, const char* const name, MemoryAllocationJanitor* allocator, std::wstring** wstr_args, const wchar_t*** args);
}
#endif
