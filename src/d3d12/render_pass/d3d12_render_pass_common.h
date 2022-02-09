#ifndef ILLUMINATE_RENDER_PASS_D3D12_COMMON_H
#define ILLUMINATE_RENDER_PASS_D3D12_COMMON_H
#include "../d3d12_descriptors.h"
#include "../d3d12_header_common.h"
#include "../d3d12_scene.h"
#include "../d3d12_shader_compiler.h"
#include "../d3d12_src_common.h"
namespace illuminate {
struct RenderPassFuncArgsInit {
  const nlohmann::json* json{nullptr};
  const char* shader_code{nullptr};
  ShaderCompiler* shader_compiler{nullptr};
  DescriptorCpu* descriptor_cpu{nullptr};
  D3d12Device* device{nullptr};
  const MainBufferFormat& main_buffer_format{};
  HWND hwnd{nullptr};
  uint32_t frame_buffer_num{0};
  MemoryAllocationJanitor* allocator{nullptr};
  const HashMap<uint32_t, MemoryAllocationJanitor>* named_buffer_allocator_index{nullptr};
  const HashMap<uint32_t, MemoryAllocationJanitor>* named_buffer_config_index{nullptr};
  BufferList* buffer_list{nullptr};
  BufferConfig* buffer_config_list{nullptr};
};
struct RenderPassFuncArgsUpdate {
  void* pass_vars_ptr{nullptr};
  SceneData* scene_data{nullptr};
  uint32_t frame_index{0};
  void* ptr{nullptr};
};
struct RenderPassFuncArgsRender {
  D3d12CommandList* command_list{nullptr};
  const MainBufferSize* main_buffer_size{nullptr};
  void* pass_vars_ptr{nullptr};
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles{nullptr};
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles{nullptr};
  ID3D12Resource** resources{nullptr};
  SceneData* scene_data{nullptr};
  uint32_t frame_index{0};
};
using RenderPassInitFunction = void* (*)(RenderPassFuncArgsInit*);
using RenderPassUpdateFunction = void (*)(RenderPassFuncArgsUpdate*);
using RenderPassRenderFunction = void (*)(RenderPassFuncArgsRender*);
uint32_t GetShaderCompilerArgs(const nlohmann::json& j, const char* const name, MemoryAllocationJanitor* allocator, std::wstring** wstr_args, const wchar_t*** args);
}
#endif
