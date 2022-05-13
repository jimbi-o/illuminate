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
    command_list->SetGraphicsRootDescriptorTable(1 + i, args_per_pass->gpu_handles[i]);
    logtrace("mesh transform gpu handle. i:{}/{} ptr:{:x}", i, pass_vars->gpu_handle_num, args_per_pass->gpu_handles[i].ptr);
  }
  const auto scene_data = args_common->scene_data;
  uint32_t prev_material_index = ~0U;
  auto prev_variation_index = MaterialList::kInvalidIndex;
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    if (scene_data->model_instance_num[i] == 0) { continue; }
    command_list->SetGraphicsRoot32BitConstant(0, scene_data->transform_offset[i], 0);
    for (uint32_t j = 0; j < scene_data->model_submesh_num[i]; j++) {
      const auto submesh_index = scene_data->model_submesh_index[i][j];
      if (const auto material_index = scene_data->submesh_material_index[submesh_index]; material_index != prev_material_index && pass_vars->use_material) {
        command_list->SetGraphicsRoot32BitConstant(0, material_index * sizeof(shader::MaterialIndexList), 1);
        logtrace("mesh transform material.mesh:{}-{} material:{}->{}", i, j, prev_material_index, material_index);
        prev_material_index = material_index;
      }
      auto variation_index = FindMaterialVariationIndex(*args_common->material_list, material_id, scene_data->submesh_material_variation_hash[submesh_index]);
      if (variation_index == MaterialList::kInvalidIndex) {
        logwarn("material variation not found. pass:{} material:{} submesh:{}", GetRenderPass(args_common, args_per_pass).name, material_id, submesh_index);
        variation_index = 0;
      }
      if (prev_variation_index != variation_index) {
        command_list->SetPipelineState(GetMaterialPso(*args_common->material_list, material_id, variation_index));
        logtrace("mesh transform material variation.mesh:{}-{} variation:{}->{}", i, j, prev_variation_index, variation_index);
        prev_variation_index = variation_index;
      }
      command_list->IASetIndexBuffer(&scene_data->submesh_index_buffer_view[submesh_index]);
      D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[] = {
        scene_data->submesh_vertex_buffer_view[kVertexBufferTypePosition][submesh_index],
      };
      command_list->IASetVertexBuffers(0, 1, vertex_buffer_view);
      command_list->DrawIndexedInstanced(scene_data->submesh_index_buffer_len[submesh_index], scene_data->model_instance_num[i], 0, 0, 0);
    }
  }
}
} // namespace illuminate
