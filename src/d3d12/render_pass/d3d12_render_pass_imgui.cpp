#include "d3d12_render_pass_imgui.h"
namespace illuminate {
void RenderPassImgui::RegisterGUI(RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (ImGui::Begin("render pass config", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
    auto& selected_item = args_common->dynamic_data->selected_debug_buffer;
    bool debug_render_selected_buffer = (selected_item < args_common->buffer_list->buffer_allocation_num);
    ImGui::Checkbox("render selected buffer mode", &debug_render_selected_buffer);
    if (!debug_render_selected_buffer) {
      if (selected_item < args_common->buffer_list->buffer_allocation_num) {
        selected_item += args_common->buffer_list->buffer_allocation_num;
      }
    } else {
      if (selected_item >= args_common->buffer_list->buffer_allocation_num) {
        selected_item = (selected_item == ~0U) ? 0 : selected_item - args_common->buffer_list->buffer_allocation_num;
      }
      auto label_select_output_buffer = "##select output buffer";
      if (ImGui::BeginListBox(label_select_output_buffer)) {
        for (uint32_t i = 0; i < args_common->buffer_list->buffer_allocation_num; i++) {
          if (args_common->buffer_list->resource_list[i] == nullptr) { continue; }
          const auto len = 128;
          char buffer_name[len]{};
          GetD3d12Name(args_common->buffer_list->resource_list[i], len, buffer_name);
          if (strncmp(buffer_name, "swapchain", 9) == 0) { continue; }
          if (ImGui::Selectable(buffer_name, i == selected_item)) {
            selected_item = i;
          }
          if (i == selected_item) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndListBox();
      }
    }
    ImGui::End();
  }
}
}
