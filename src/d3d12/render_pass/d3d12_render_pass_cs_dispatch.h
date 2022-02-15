#ifndef ILLUMINATE_D3D12_RENDER_PASS_CS_DISPATCH_H
#define ILLUMINATE_D3D12_RENDER_PASS_CS_DISPATCH_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCsDispatch {
 public:
  struct Param {
    uint32_t thread_group_count_x{0};
    uint32_t thread_group_count_y{0};
  };
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    param->thread_group_count_x = args->json->at("thread_group_count_x");
    param->thread_group_count_y = args->json->at("thread_group_count_y");
    return param;
  }
  static void Term([[maybe_unused]]void* ptr) {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsUpdate* args) {
  }
  static auto IsRenderNeeded([[maybe_unused]]const void* args) { return true; }
  static auto Render(RenderPassFuncArgsRender* args) {
    auto pass_vars = static_cast<const Param*>(args->pass_vars_ptr);
    args->command_list->SetComputeRootSignature(GetRenderPassRootSig(args));
    args->command_list->SetPipelineState(GetRenderPassPso(args));
    args->command_list->SetComputeRootDescriptorTable(0, args->gpu_handles[0]);
    args->command_list->Dispatch(args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_x, args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_y, 1);
  }
 private:
  RenderPassCsDispatch() = delete;
};
}
#endif
