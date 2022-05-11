#include "d3d12_render_pass_copy_resource.h"
#include "d3d12_render_pass_util.h"
#include "../d3d12_resource_transfer.h"
#include "../d3d12_texture_util.h"
namespace illuminate {
void RenderPassCopyResource::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  for (uint32_t i = 0; i < args_common->resource_transfer->transfer_reserved_buffer_num[args_common->frame_index]; i++) {
    args_per_pass->command_list->CopyResource(args_common->resource_transfer->transfer_reserved_buffer_dst[args_common->frame_index][i],
                                              args_common->resource_transfer->transfer_reserved_buffer_src[args_common->frame_index][i]);
  }
  for (uint32_t i = 0; i < args_common->resource_transfer->transfer_reserved_texture_num[args_common->frame_index]; i++) {
    for (uint32_t j = 0; j < args_common->resource_transfer->texture_subresource_num[args_common->frame_index][i]; j++) {
      CD3DX12_TEXTURE_COPY_LOCATION src(args_common->resource_transfer->transfer_reserved_texture_src[args_common->frame_index][i],
                                        args_common->resource_transfer->texture_layout[args_common->frame_index][i][j]);
      CD3DX12_TEXTURE_COPY_LOCATION dst(args_common->resource_transfer->transfer_reserved_texture_dst[args_common->frame_index][i], j);
      args_per_pass->command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
  }
  NotifyTransferReservedResourcesProcessed(args_common->frame_index, args_common->resource_transfer);
}
} // namespace illuminate
