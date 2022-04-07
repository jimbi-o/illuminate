#include "d3d12_render_pass_cs_dispatch.h"
namespace illuminate {
namespace {
struct Param {
  uint32_t thread_group_count_x{0};
  uint32_t thread_group_count_y{0};
};
} // namespace anonymous
void* RenderPassCsDispatch::Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index) {
  auto param = Allocate<Param>(args->allocator);
  *param = {};
  param->thread_group_count_x = args->json->at("thread_group_count_x");
  param->thread_group_count_y = args->json->at("thread_group_count_y");
  return param;
}
void RenderPassCsDispatch::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  args_per_pass->command_list->SetComputeRootSignature(GetRenderPassRootSig(args_common, args_per_pass));
  args_per_pass->command_list->SetPipelineState(GetRenderPassPso(args_common, args_per_pass));
  args_per_pass->command_list->SetComputeRootDescriptorTable(0, args_per_pass->gpu_handles[0]);
  args_per_pass->command_list->Dispatch(args_common->main_buffer_size->swapchain.width / pass_vars->thread_group_count_x,
                                        args_common->main_buffer_size->swapchain.width / pass_vars->thread_group_count_y,
                                        1);
}
} // namespace illuminate
