#include "illuminate/illuminate.h"
#include "../d3d12_header_common.h"
#include "d3d12_render_pass_imgui.h"
namespace illuminate {
void RenderPassImgui::Render([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  ImGui::Render();
  args_per_pass->command_list->OMSetRenderTargets(1, &args_per_pass->cpu_handles[0], true, nullptr);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args_per_pass->command_list);
}
}
