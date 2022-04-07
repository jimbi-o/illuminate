#ifndef ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#define ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCopyResource {
 public:
  static void* Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index);
  static void Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static bool IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassCopyResource() = delete;
};
}
#endif
