#ifndef ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#define ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassDebugRenderSelectedBuffer {
 public:
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
  static void Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassDebugRenderSelectedBuffer() = delete;
};
}
#endif
