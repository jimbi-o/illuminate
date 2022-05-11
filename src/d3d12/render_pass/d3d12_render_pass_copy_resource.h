#ifndef ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#define ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCopyResource {
 public:
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassCopyResource() = delete;
};
}
#endif
