#ifndef ILLUMINATE_D3D12_RENDER_PASS_MESH_TRANSFORM_H
#define ILLUMINATE_D3D12_RENDER_PASS_MESH_TRANSFORM_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassMeshTransform {
 public:
  static void* Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassMeshTransform() = delete;
};
}
#endif
