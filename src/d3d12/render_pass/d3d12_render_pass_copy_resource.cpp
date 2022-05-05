#include "d3d12_render_pass_copy_resource.h"
#include "d3d12_render_pass_util.h"
#include "../d3d12_resource_transferer.h"
namespace illuminate {
namespace {
} // namespace anonymous
void RenderPassCopyResource::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  for (uint32_t i = 0; i < args_common->resource_transfer->transfer_reserved_buffer_num[args_common->frame_index]; i++) {
    args_per_pass->command_list->CopyResource(args_common->resource_transfer->transfer_reserved_buffer_dst[args_common->frame_index][i],
                                              args_common->resource_transfer->transfer_reserved_buffer_src[args_common->frame_index][i]);
  }
  NotifyTransferReservedResourcesProcessed(args_common->frame_index, args_common->resource_transfer);
}
} // namespace illuminate
