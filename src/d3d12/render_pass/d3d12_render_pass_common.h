#ifndef ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
#define ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
#include "../d3d12_descriptors.h"
#include "../d3d12_header_common.h"
#include "../d3d12_json_parser.h"
#include "../d3d12_scene.h"
#include "../d3d12_shader_compiler.h"
#include "../d3d12_src_common.h"
namespace illuminate {
struct RenderPassFuncArgsInit {
  const nlohmann::json* json{nullptr};
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
struct RenderPassFuncArgsRenderCommon {
  const MainBufferSize* main_buffer_size{nullptr};
  SceneData* scene_data{nullptr};
  uint32_t frame_index{0};
  PsoRootsigManager* pso_rootsig_manager{nullptr};
  BufferList* buffer_list{nullptr};
  BufferConfig* buffer_config_list{nullptr};
};
struct RenderPassFuncArgsRenderPerPass {
  D3d12CommandList* command_list{nullptr};
  void* pass_vars_ptr{nullptr};
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles{nullptr};
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles{nullptr};
  ID3D12Resource** resources{nullptr};
  const RenderPass* render_pass{nullptr};
  void* ptr{nullptr};
};
constexpr inline ID3D12RootSignature* GetRenderPassRootSig(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass, const uint32_t index = 0) {
  return args_common->pso_rootsig_manager->GetRootsig(args_per_pass->render_pass->material_list[index]);
}
constexpr inline ID3D12PipelineState* GetRenderPassPso(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass, const uint32_t index = 0) {
  return args_common->pso_rootsig_manager->GetPso(args_per_pass->render_pass->material_list[index]);
}
}
#endif
