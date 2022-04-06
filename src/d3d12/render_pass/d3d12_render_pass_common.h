#ifndef ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
#define ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
#include "illuminate/math/math.h"
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
  BufferList* buffer_list{nullptr};
  BufferConfig* buffer_config_list{nullptr};
  uint32_t render_pass_num{0};
  RenderPass* render_pass_list{nullptr};
};
struct RenderPassConfigDynamicData {
  bool* render_pass_enable_flag{nullptr};
  uint32_t debug_render_selected_buffer_allocation_index{0};
  bool** write_to_sub{nullptr};
  float camera_pos[3]{0.0f, 1.2f, -5.0f};
  float camera_focus[3]{};
  float fov_vertical{30.0f}, near_z{0.001f}, far_z{1000.0f};
};
struct RenderPassFuncArgsRenderCommon {
  const MainBufferSize* main_buffer_size{nullptr};
  SceneData* scene_data{nullptr};
  uint32_t frame_index{0};
  PsoRootsigManager* pso_rootsig_manager{nullptr};
  BufferList* buffer_list{nullptr};
  BufferConfig* buffer_config_list{nullptr};
  RenderPassConfigDynamicData* dynamic_data{nullptr};
  const RenderPass* render_pass_list{nullptr};
  D3d12Device* device{nullptr};
  DescriptorGpu* descriptor_gpu{nullptr};
  DescriptorCpu* descriptor_cpu{nullptr};
  const ResourceStateType*** resource_state_list{nullptr};
};
struct RenderPassFuncArgsRenderPerPass {
  D3d12CommandList* command_list{nullptr};
  void* pass_vars_ptr{nullptr};
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles{nullptr};
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles{nullptr};
  ID3D12Resource** resources{nullptr};
  uint32_t render_pass_index{};
};
using RenderPassFuncInit = void* (*)(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
using RenderPassFuncTerm = void (*)(void);
using RenderPassFuncUpdate = void (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
using RenderPassFuncIsRenderNeeded = bool (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
using RenderPassFuncRender = void (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
struct RenderPassFunctionList {
  RenderPassFuncInit* init{nullptr};
  RenderPassFuncTerm* term{nullptr};
  RenderPassFuncUpdate* update{nullptr};
  RenderPassFuncIsRenderNeeded* is_render_needed{nullptr};
  RenderPassFuncRender* render{nullptr};
};
constexpr inline const RenderPass& GetRenderPass(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  return args_common->render_pass_list[args_per_pass->render_pass_index];
}
constexpr inline auto RenderPassInit(RenderPassFunctionList* render_pass_function_list, RenderPassFuncArgsInit* args, const uint32_t render_pass_index) {
  auto f = render_pass_function_list->init[render_pass_index];
  if (f) { return (*f)(args, render_pass_index); }
  return (void*)nullptr;
}
constexpr inline auto RenderPassTerm(RenderPassFunctionList* render_pass_function_list, const uint32_t render_pass_index) {
  auto f = render_pass_function_list->term[render_pass_index];
  if (f) { (*f)(); }
}
constexpr inline auto RenderPassUpdate(RenderPassFunctionList* render_pass_function_list, RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto f = render_pass_function_list->update[args_per_pass->render_pass_index];
  if (f) { (*f)(args_common, args_per_pass); }
}
constexpr inline auto IsRenderPassRenderNeeded(RenderPassFunctionList* render_pass_function_list, RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto f = render_pass_function_list->is_render_needed[args_per_pass->render_pass_index];
  if (f) { return (*f)(args_common, args_per_pass); }
  return true;
}
constexpr inline auto RenderPassRender(RenderPassFunctionList* render_pass_function_list, RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto f = render_pass_function_list->render[args_per_pass->render_pass_index];
  if (f) { (*f)(args_common, args_per_pass); }
}
constexpr inline ID3D12RootSignature* GetRenderPassRootSig(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass, const uint32_t material_index = 0) {
  return args_common->pso_rootsig_manager->GetRootsig(GetRenderPass(args_common, args_per_pass).material_list[material_index]);
}
constexpr inline ID3D12PipelineState* GetRenderPassPso(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass, const uint32_t material_index = 0) {
  return args_common->pso_rootsig_manager->GetPso(GetRenderPass(args_common, args_per_pass).material_list[material_index]);
}
RenderPassConfigDynamicData InitRenderPassDynamicData(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, MemoryAllocationJanitor* allocator);
}
#endif
