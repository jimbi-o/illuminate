#include "illuminate/illuminate.h"
#include "../d3d12_header_common.h"
#include "d3d12_render_pass_mesh_transform.h"
#include "d3d12_render_pass_util.h"
namespace illuminate {
namespace {
struct Param {
  uint32_t stencil_val{};
  uint32_t rtv_index{};
  uint32_t rtv_num{};
  uint32_t dsv_index{};
  bool clear_depth{false};
  bool use_material{false};
  uint32_t gpu_handle_num{1U};
};
} // namespace anonymous
void* RenderPassMeshTransform::Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index) {
  auto param = Allocate<Param>(args->allocator);
  *param = {};
  param->stencil_val = GetNum(*args->json, "stencil_val", 0);
  param->rtv_index = ~0U;
  param->rtv_num = 0;
  param->dsv_index = ~0U;
  for (uint32_t i = 0; i < args->render_pass_list[render_pass_index].buffer_num; i++) {
    if (args->render_pass_list[render_pass_index].buffer_list[i].state == ResourceStateType::kRtv) {
      if (param->rtv_index == ~0U) {
        param->rtv_index = i;
      }
      param->rtv_num++;
    }
    if (param->dsv_index == ~0U && args->render_pass_list[render_pass_index].buffer_list[i].state == ResourceStateType::kDsvWrite) {
      param->dsv_index = i;
    }
  }
  param->gpu_handle_num = args->render_pass_list[render_pass_index].max_buffer_index_offset + 1;
  param->clear_depth = GetBool(*args->json, "clear_depth", false);
  param->use_material = GetBool(*args->json, "use_material", false);
  return param;
}
void RenderPassMeshTransform::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto command_list = args_per_pass->command_list;
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  const auto rtv_handle = pass_vars->rtv_num == 0 ? nullptr : &args_per_pass->cpu_handles[pass_vars->rtv_index];
  const auto dsv_handle = &args_per_pass->cpu_handles[pass_vars->dsv_index];
  for (uint32_t i = 0; i < pass_vars->rtv_num; i++) {
    // TODO remove (this is only for debug purpose).
    float clear_color[4] = {};
    command_list->ClearRenderTargetView(rtv_handle[i], clear_color, 0, nullptr);
  command_list->OMSetRenderTargets(pass_vars->rtv_num, rtv_handle, true, dsv_handle);
  }
  if (pass_vars->clear_depth) {
    command_list->ClearDepthStencilView(*dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
  }
  auto& width = args_common->main_buffer_size->primarybuffer.width;
  auto& height = args_common->main_buffer_size->primarybuffer.height;
  {
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    command_list->RSSetViewports(1, &viewport);
  }
  {
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    command_list->RSSetScissorRects(1, &scissor_rect);
  }
  const auto material_id = GetRenderPassMaterial(args_common, args_per_pass);
  command_list->SetGraphicsRootSignature(GetMaterialRootsig(*args_common->material_list, material_id));
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command_list->OMSetRenderTargets(pass_vars->rtv_num, rtv_handle, true, dsv_handle);
  command_list->OMSetStencilRef(pass_vars->stencil_val);
  for (uint32_t i = 0; i < pass_vars->gpu_handle_num; i++) {
    command_list->SetGraphicsRootDescriptorTable(1 + i, args_per_pass->gpu_handles_view[i]);
    logtrace("mesh transform gpu handle. i:{}/{} ptr:{:x}", i, pass_vars->gpu_handle_num, args_per_pass->gpu_handles_view[i].ptr);
  }
  if (args_per_pass->gpu_handles_sampler) {
    command_list->SetGraphicsRootDescriptorTable(1 + pass_vars->gpu_handle_num, args_per_pass->gpu_handles_sampler[0]);
  }
  const auto scene_data = args_common->scene_data;
  auto prev_variation_index = MaterialList::kInvalidIndex;
  uint32_t vertex_buffer_type_num = 0;
  uint32_t vertex_buffer_type_index[kVertexBufferTypeNum]{};
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[kVertexBufferTypeNum]{};
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    if (scene_data->model_instance_num[i] == 0) { continue; }
    for (uint32_t j = 0; j < scene_data->model_submesh_num[i]; j++) {
      const auto submesh_index = scene_data->model_submesh_index[i][j];
      {
        uint32_t val_num = 1;
        uint32_t val[] = {scene_data->transform_offset[i], 0};
        if (pass_vars->use_material) {
          val[1] = scene_data->submesh_material_index[submesh_index];
          val_num++;
        }
        command_list->SetGraphicsRoot32BitConstants(0, val_num, &val[0], 0);
      }
      auto variation_index = FindMaterialVariationIndex(*args_common->material_list, material_id, scene_data->submesh_material_variation_hash[submesh_index]);
      if (variation_index == MaterialList::kInvalidIndex) {
        logwarn("material variation not found. pass:{} material:{} submesh:{}", GetRenderPass(args_common, args_per_pass).name, material_id, submesh_index);
        variation_index = 0;
      }
      if (prev_variation_index != variation_index) {
        logtrace("mesh transform material variation.mesh:{}-{} variation:{}->{}", i, j, prev_variation_index, variation_index);
        prev_variation_index = variation_index;
        const auto pso_index = GetMaterialPsoIndex(*args_common->material_list, material_id, variation_index);
        command_list->SetPipelineState(args_common->material_list->pso_list[pso_index]);
        const auto vertex_buffer_type_flags = args_common->material_list->vertex_buffer_type_flags[pso_index];
        vertex_buffer_type_num = GetVertexBufferTypeNum(vertex_buffer_type_flags);
        for (uint32_t k = 0; k < vertex_buffer_type_num; k++) {
          vertex_buffer_type_index[k] = GetVertexBufferTypeAtIndex(vertex_buffer_type_flags, k);
        }
      }
      command_list->IASetIndexBuffer(&scene_data->submesh_index_buffer_view[submesh_index]);
      for (uint32_t k = 0; k < vertex_buffer_type_num; k++) {
        const auto vertex_buffer_view_index = scene_data->submesh_vertex_buffer_view_index[vertex_buffer_type_index[k]][submesh_index];
        vertex_buffer_view[k] = scene_data->submesh_vertex_buffer_view[vertex_buffer_view_index];
      }
      command_list->IASetVertexBuffers(0, vertex_buffer_type_num, vertex_buffer_view);
      command_list->DrawIndexedInstanced(scene_data->submesh_index_buffer_len[submesh_index], scene_data->model_instance_num[i], 0, 0, 0);
    }
  }
}
} // namespace illuminate
