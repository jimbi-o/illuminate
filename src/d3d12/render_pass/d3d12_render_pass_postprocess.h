#ifndef ILLUMINATE_RENDER_PASS_D3D12_POSTPROCESS_H
#define ILLUMINATE_RENDER_PASS_D3D12_POSTPROCESS_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPostprocess {
 public:
  struct Param {
    ID3D12RootSignature* rootsig{nullptr};
    ID3D12PipelineState* pso{nullptr};
  };
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    auto allocator = GetTemporalMemoryAllocator();
    std::wstring* wstr_args_vs{nullptr};
    const wchar_t** args_vs{nullptr};
    auto args_num_vs = GetShaderCompilerArgs(*args->json, "shader_compile_args_vs", &allocator, &wstr_args_vs, &args_vs);
    assert(args_num_vs > 0);
    std::wstring* wstr_args_ps{nullptr};
    const wchar_t** args_ps{nullptr};
    auto args_num_ps = GetShaderCompilerArgs(*args->json, "shader_compile_args_ps", &allocator, &wstr_args_ps, &args_ps);
    assert(args_num_ps > 0);
    ShaderCompiler::PsoDescVsPs pso_desc{};
    pso_desc.render_target_formats.RTFormats[0] = args->main_buffer_format.swapchain;
    pso_desc.render_target_formats.NumRenderTargets = 1;
    pso_desc.depth_stencil1.DepthEnable = false;
    if (!args->shader_compiler->CreateRootSignatureAndPsoVsPs(args->shader_code, static_cast<uint32_t>(strlen(args->shader_code)), args_num_vs, args_vs,
                                                              args->shader_code, static_cast<uint32_t>(strlen(args->shader_code)), args_num_ps, args_ps,
                                                              args->device, &pso_desc, &param->rootsig, &param->pso)) {
      logerror("vs ps parse error");
      assert(false && "vs ps parse error");
    }
    SetD3d12Name(param->rootsig, "rootsig_swapchain");
    SetD3d12Name(param->pso, "pso_swapchain");
    return param;
  }
  static void Term(void* ptr) {
    auto param = static_cast<Param*>(ptr);
    if (param->pso->Release() != 0) {
      logwarn("ReleaseResourceDispatchCs pso ref left.");
    }
    if (param->rootsig->Release() != 0) {
      logwarn("ReleaseResourceDispatchCs rootsig ref left.");
    }
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsUpdate* args) {
  }
  static auto IsRenderNeeded([[maybe_unused]]const void* args) { return true; }
  static auto Render(RenderPassFuncArgsRender* args) {
    auto pass_vars = static_cast<const Param*>(args->pass_vars_ptr);
    auto& width = args->main_buffer_size->swapchain.width;
    auto& height = args->main_buffer_size->swapchain.height;
    args->command_list->SetGraphicsRootSignature(pass_vars->rootsig);
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    args->command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    args->command_list->RSSetScissorRects(1, &scissor_rect);
    args->command_list->SetPipelineState(pass_vars->pso);
    args->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    args->command_list->OMSetRenderTargets(1, &args->cpu_handles[2], true, nullptr);
    args->command_list->SetGraphicsRootDescriptorTable(0, args->gpu_handles[0]); // srv
    args->command_list->SetGraphicsRootDescriptorTable(1, args->gpu_handles[1]); // sampler
    args->command_list->DrawInstanced(3, 1, 0, 0);
  }
 private:
  RenderPassPostprocess() = delete;
};
}
#endif
