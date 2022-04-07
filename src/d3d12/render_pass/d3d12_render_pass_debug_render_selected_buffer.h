#ifndef ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#define ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassDebugRenderSelectedBuffer {
 public:
  static void* Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static uint32_t GetBufferAllocationIndexList(const BufferList& buffer_list, const BufferConfig* buffer_config_list, MemoryAllocationJanitor* allocator, uint32_t** dst_buffer_allocation_index_list);
 private:
  RenderPassDebugRenderSelectedBuffer() = delete;
};
}
#endif
