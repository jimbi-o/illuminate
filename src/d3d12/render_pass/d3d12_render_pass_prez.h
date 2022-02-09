#ifndef ILLUMINATE_RENDER_PASS_D3D12_PREZ_H
#define ILLUMINATE_RENDER_PASS_D3D12_PREZ_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPrez {
 public:
  struct Param {
    ID3D12RootSignature* rootsig{nullptr};
    ID3D12PipelineState* pso{nullptr};
    uint32_t stencil_val;
  };
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    std::wstring* wstr_shader_args{nullptr};
    const wchar_t** shader_args{nullptr};
    auto allocator = GetTemporalMemoryAllocator();
    auto shader_args_num = GetShaderCompilerArgs(*args->json, "shader_compile_args", &allocator, &wstr_shader_args, &shader_args);
    assert(shader_args_num > 0);
    ShaderCompiler::PsoDescVsPs pso_desc{};
    pso_desc.depth_stencil_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
      {
        .SemanticName = "POSITION",
        .SemanticIndex = 0,
        .Format = DXGI_FORMAT_R32G32B32_FLOAT,
        .InputSlot = 0,
        .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        .InstanceDataStepRate = 0,
      },
    };
    pso_desc.depth_stencil1.StencilEnable = true;
    pso_desc.input_layout.pInputElementDescs = input_element_descs;
    pso_desc.input_layout.NumElements = 1;
    if (!args->shader_compiler->CreateRootSignatureAndPsoVs(args->shader_code, static_cast<uint32_t>(strlen(args->shader_code)), shader_args_num, shader_args, args->device, &pso_desc, &param->rootsig, &param->pso)) {
      logerror("vs parse error");
      assert(false && "vs parse error");
    }
    SetD3d12Name(param->rootsig, "rootsig_prez");
    SetD3d12Name(param->pso, "pso_prez");
    param->stencil_val = GetNum(*args->json, "stencil_val", 0);
    return param;
  }
  static void Term(void* ptr) {
    auto param = static_cast<Param*>(ptr);
    param->pso->Release();
    param->rootsig->Release();
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
    args->command_list->SetGraphicsRootSignature(pass_vars->rootsig);
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    args->command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    args->command_list->RSSetScissorRects(1, &scissor_rect);
    args->command_list->SetPipelineState(pass_vars->pso);
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
