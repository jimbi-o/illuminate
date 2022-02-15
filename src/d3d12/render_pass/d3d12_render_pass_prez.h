#ifndef ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#define ILLUMINATE_D3D12_RENDER_PASS_PREZ_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPrez {
 public:
  struct Param {
    uint32_t stencil_val;
  };
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    param->stencil_val = GetNum(*args->json, "stencil_val", 0);
    return param;
  }
  static void Term([[maybe_unused]]void* ptr) {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsUpdate* args) {
  }
  static auto IsRenderNeeded([[maybe_unused]]const void* args) { return true; }
  static auto Render(RenderPassFuncArgsRender* args) {
    PIXScopedEvent(args->command_list, 0, "prez"); // https://devblogs.microsoft.com/pix/winpixeventruntime/
    args->command_list->ClearDepthStencilView(args->cpu_handles[0], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    auto pass_vars = static_cast<const Param*>(args->pass_vars_ptr);
    auto& width = args->main_buffer_size->primarybuffer.width;
    auto& height = args->main_buffer_size->primarybuffer.height;
    args->command_list->SetGraphicsRootSignature(GetRenderPassRootSig(args));
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    args->command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    args->command_list->RSSetScissorRects(1, &scissor_rect);
    args->command_list->SetPipelineState(GetRenderPassPso(args));
    args->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    args->command_list->OMSetRenderTargets(0, nullptr, true, args->cpu_handles);
    args->command_list->OMSetStencilRef(pass_vars->stencil_val);
    for (uint32_t i = 0; i < args->scene_data->model_num; i++) {
      auto mesh_index = args->scene_data->model_mesh_index[i];
      args->command_list->IASetIndexBuffer(&args->scene_data->mesh_index_buffer_view[mesh_index]);
      D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[] = {
        args->scene_data->mesh_vertex_buffer_view_position[mesh_index],
      };
      args->command_list->IASetVertexBuffers(0, 1, vertex_buffer_view);
      args->command_list->DrawIndexedInstanced(args->scene_data->mesh_index_buffer_len[mesh_index], 1, 0, 0, 0);
      // auto material_index = scene_data->model_material_index[i];
    }
  }
 private:
  RenderPassPrez() = delete;
};
}
#endif
