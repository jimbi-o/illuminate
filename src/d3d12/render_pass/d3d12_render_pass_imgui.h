#ifndef ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#define ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#include "d3d12_render_pass_common.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
namespace illuminate {
class RenderPassImgui {
 public:
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
  static void Term();
  static void Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  static void Render([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
 private:
  RenderPassImgui() = delete;
};
}
#endif
