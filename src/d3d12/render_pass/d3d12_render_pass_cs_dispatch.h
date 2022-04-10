#ifndef ILLUMINATE_D3D12_RENDER_PASS_CS_DISPATCH_H
#define ILLUMINATE_D3D12_RENDER_PASS_CS_DISPATCH_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCsDispatch {
 public:
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassCsDispatch() = delete;
};
}
#endif
