#ifndef ILLUMINATE_D3D12_RENDER_PASS_POSTPROCESS_H
#define ILLUMINATE_D3D12_RENDER_PASS_POSTPROCESS_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPostprocess {
 public:
  struct Param {
    BufferSizeRelativeness size_type{BufferSizeRelativeness::kPrimaryBufferRelative};
    uint32_t rtv_index{0};
    bool use_views{true};
    bool use_sampler{true};
    void** cbv_ptr{nullptr};
    uint32_t cbv_size{0};
  };
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static void Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassPostprocess() = delete;
};
} // namespace illuminate
#endif
