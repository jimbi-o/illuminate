#include "d3d12_render_pass_imgui.h"
namespace illuminate {
void RenderPassImgui::RegisterGUI(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (ImGui::Begin("render pass config", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::Text("select visible buffer:");
    auto label_select_output_buffer = "##select output buffer";
    if (ImGui::BeginListBox(label_select_output_buffer)) {
      static uint32_t current_item = 0;
      for (uint32_t i = 0; i < args_common->buffer_list->buffer_allocation_num; i++) {
        if (args_common->buffer_list->resource_list[i] == nullptr) { continue; }
        const auto len = 128;
        char buffer_name[len]{};
        GetD3d12Name(args_common->buffer_list->resource_list[i], len, buffer_name);
        if (ImGui::Selectable(buffer_name, i == current_item)) {
          current_item = i;
        }
        if (i == current_item) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndListBox();
    }
    ImGui::End();
  }
}
}
