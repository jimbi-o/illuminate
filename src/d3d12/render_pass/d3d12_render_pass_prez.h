#ifndef ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#define ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPrez {
 public:
  struct Param {
    uint32_t stencil_val;
  };
  static void* Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    param->stencil_val = GetNum(*args->json, "stencil_val", 0);
    return param;
  }
  static void Term() {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) {
  }
  static auto IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) { return true; }
  static auto Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    PIXScopedEvent(args_per_pass->command_list, 0, "prez"); // https://devblogs.microsoft.com/pix/winpixeventruntime/
    args_per_pass->command_list->ClearDepthStencilView(args_per_pass->cpu_handles[0], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    auto& width = args_common->main_buffer_size->primarybuffer.width;
    auto& height = args_common->main_buffer_size->primarybuffer.height;
    args_per_pass->command_list->SetGraphicsRootSignature(GetRenderPassRootSig(args_common, args_per_pass));
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    args_per_pass->command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    args_per_pass->command_list->RSSetScissorRects(1, &scissor_rect);
    args_per_pass->command_list->SetPipelineState(GetRenderPassPso(args_common, args_per_pass));
    args_per_pass->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    args_per_pass->command_list->OMSetRenderTargets(0, nullptr, true, args_per_pass->cpu_handles);
    args_per_pass->command_list->OMSetStencilRef(pass_vars->stencil_val);
    for (uint32_t i = 0; i < args_common->scene_data->model_num; i++) {
      auto mesh_index = args_common->scene_data->model_mesh_index[i];
      args_per_pass->command_list->IASetIndexBuffer(&args_common->scene_data->mesh_index_buffer_view[mesh_index]);
      D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[] = {
        args_common->scene_data->mesh_vertex_buffer_view_position[mesh_index],
      };
      args_per_pass->command_list->IASetVertexBuffers(0, 1, vertex_buffer_view);
      args_per_pass->command_list->DrawIndexedInstanced(args_common->scene_data->mesh_index_buffer_len[mesh_index], 1, 0, 0, 0);
      // auto material_index = scene_data->model_material_index[i];
    }
  }
 private:
  RenderPassPrez() = delete;
};
}
#endif
