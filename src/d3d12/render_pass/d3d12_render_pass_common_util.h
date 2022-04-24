#ifndef ILLUMINATE_D3D12_RENDER_PASS_COMMON_UTIL_H
#define ILLUMINATE_D3D12_RENDER_PASS_COMMON_UTIL_H
#include "d3d12_render_pass_common.h"
#include "illuminate/math/math.h"
#include "../d3d12_header_common.h"
#include "../d3d12_json_parser.h"
#include "../d3d12_render_graph.h"
#include "../d3d12_scene.h"
#include "../d3d12_src_common.h"
#include "../d3d12_shader_compiler.h"
namespace illuminate {
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
constexpr inline auto GetRenderPassMaterial(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  return GetRenderPass(args_common, args_per_pass).material;
}
constexpr inline auto GetRenderPassRootSig(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  return args_common->pso_rootsig_manager->GetMaterialRootsig(GetRenderPassMaterial(args_common, args_per_pass));
}
constexpr inline auto GetRenderPassPso(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  return args_common->pso_rootsig_manager->GetMaterialPso(GetRenderPassMaterial(args_common, args_per_pass));
}
constexpr inline auto GetRenderPassPso(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass, const uint32_t variation_index) {
  return args_common->pso_rootsig_manager->GetMaterialPso(GetRenderPassMaterial(args_common, args_per_pass), variation_index);
}
RenderPassConfigDynamicData InitRenderPassDynamicData(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, MemoryAllocationJanitor* allocator);
}
#endif
