#ifndef ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#define ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPrez {
 public:
  struct Param {
    uint32_t stencil_val{};
    uint32_t dsv_index{};
  };
  static void* Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    param->stencil_val = GetNum(*args->json, "stencil_val", 0);
    for (uint32_t i = 0; i < args->render_pass_list[render_pass_index].buffer_num; i++) {
      if (args->render_pass_list[render_pass_index].buffer_list[i].state == ResourceStateType::kDsvWrite) {
        param->dsv_index = i;
        break;
      }
    }
    return param;
  }
  static auto Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    PIXScopedEvent(args_per_pass->command_list, 0, "prez"); // https://devblogs.microsoft.com/pix/winpixeventruntime/
    auto command_list = args_per_pass->command_list;
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    const auto& dsv_handle = args_per_pass->cpu_handles[pass_vars->dsv_index];
    command_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
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
    command_list->SetGraphicsRootSignature(GetRenderPassRootSig(args_common, args_per_pass));
    command_list->SetPipelineState(GetRenderPassPso(args_common, args_per_pass));
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->OMSetRenderTargets(0, nullptr, true, &dsv_handle);
    command_list->OMSetStencilRef(pass_vars->stencil_val);
    command_list->SetGraphicsRootDescriptorTable(1, args_per_pass->gpu_handles[0]);
    const auto scene_data = args_common->scene_data;
    for (uint32_t i = 0; i < scene_data->model_num; i++) {
      if (scene_data->model_instance_num[i] == 0) { continue; }
      command_list->SetGraphicsRoot32BitConstant(0, scene_data->transform_offset[i], 0);
      for (uint32_t j = 0; j < scene_data->model_submesh_num[i]; j++) {
        const auto submesh_index = scene_data->model_submesh_index[i][j];
        command_list->IASetIndexBuffer(&scene_data->submesh_index_buffer_view[submesh_index]);
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[] = {
          scene_data->submesh_vertex_buffer_view[kVertexBufferTypePosition][submesh_index],
        };
        command_list->IASetVertexBuffers(0, 1, vertex_buffer_view);
        command_list->DrawIndexedInstanced(scene_data->submesh_index_buffer_len[submesh_index], scene_data->model_instance_num[i], 0, 0, 0);
      }
    }
  }
 private:
  RenderPassPrez() = delete;
};
}
#endif
