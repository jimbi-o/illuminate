#include "d3d12_render_pass_copy_resource.h"
#include "d3d12_render_pass_util.h"
namespace illuminate {
namespace {
} // namespace anonymous
void RenderPassCopyResource::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  for (uint32_t i = 0; i < args_common->dynamic_data->copy_buffer_num; i++) {
    args_per_pass->command_list->CopyResource(args_common->dynamic_data->copy_buffer_resource_default[i], args_common->dynamic_data->copy_buffer_resource_upload[i]);
  }
}
} // namespace illuminate
