#ifndef ILLUMINATE_RENDER_PASS_D3D12_CS_DISPATCH_H
#define ILLUMINATE_RENDER_PASS_D3D12_CS_DISPATCH_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCsDispatch {
 public:
  struct Param {
    ID3D12RootSignature* rootsig{nullptr};
    ID3D12PipelineState* pso{nullptr};
    uint32_t thread_group_count_x{0};
    uint32_t thread_group_count_y{0};
  };
  template <typename A>
  static Param* Init(RenderPassInitArgs<A>* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    std::wstring* wstr_shader_args{nullptr};
    const wchar_t** shader_args{nullptr};
    auto allocator = GetTemporalMemoryAllocator();
    auto shader_args_num = GetShaderCompilerArgs(*args->json, "shader_compile_args", &allocator, &wstr_shader_args, &shader_args);
    assert(shader_args_num > 0);
    if (!args->shader_compiler->CreateRootSignatureAndPsoCs(args->shader_code, static_cast<uint32_t>(strlen(args->shader_code)), shader_args_num, shader_args, args->device, &param->rootsig, &param->pso)) {
      logerror("compute shader parse error");
      assert(false && "compute shader parse error");
    }
    SetD3d12Name(param->rootsig, "rootsig_cs");
    SetD3d12Name(param->pso, "pso_cs");
    param->thread_group_count_x = args->json->at("thread_group_count_x");
    param->thread_group_count_y = args->json->at("thread_group_count_y");
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
  static void Exec(RenderPassArgs* args) {
    auto pass_vars = static_cast<const Param*>(args->pass_vars_ptr);
    args->command_list->SetComputeRootSignature(pass_vars->rootsig);
    args->command_list->SetPipelineState(pass_vars->pso);
    args->command_list->SetComputeRootDescriptorTable(0, args->gpu_handles[0]);
    args->command_list->Dispatch(args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_x, args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_y, 1);
  }
 private:
  RenderPassCsDispatch() = delete;
};
}
#endif
