#include "d3d12_render_pass_imgui.h"
namespace illuminate {
namespace {
struct Param {
  uint32_t render_pass_index_output_to_swapchain{};
  uint32_t render_pass_index_debug_show_selected_buffer{};
  uint32_t selected_debug_buffer{};
};
} // namespace anonymous
void* RenderPassImgui::PrepareParam(const uint32_t render_pass_num, RenderPass* render_pass_list, MemoryAllocationJanitor* allocator) {
  auto param = Allocate<Param>(allocator);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    switch (render_pass_list[i].name) {
      case SID("output to swapchain"): {
        param->render_pass_index_output_to_swapchain = i;
        break;
      }
      case SID("debug buffer"): {
        param->render_pass_index_debug_show_selected_buffer = i;
        break;
      }
    }
  }
  param->selected_debug_buffer = 0;
  return param;
}
void RenderPassImgui::RegisterGUI(RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (!ImGui::Begin("render pass config", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) { return; }
  auto param = static_cast<Param*>(args_per_pass->pass_vars_ptr);
  bool debug_render_selected_buffer = args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_debug_show_selected_buffer];
  ImGui::Checkbox("render selected buffer mode", &debug_render_selected_buffer);
  if (!debug_render_selected_buffer) {
    if (args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_debug_show_selected_buffer]) {
      args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_debug_show_selected_buffer] = false;
      args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_output_to_swapchain] = true;
    }
    ImGui::End();
    return;
  }
  if (!args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_debug_show_selected_buffer]) {
    args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_debug_show_selected_buffer] = true;
    args_common->dynamic_data->render_pass_enable_flag[param->render_pass_index_output_to_swapchain] = false;
  }
  auto label_select_output_buffer = "##select output buffer";
  if (!ImGui::BeginListBox(label_select_output_buffer)) {
    ImGui::End();
    return;
  }
  auto selected_item = &param->selected_debug_buffer;
  for (uint32_t i = 0; i < args_common->buffer_list->buffer_allocation_num; i++) {
    if (args_common->buffer_list->resource_list[i] == nullptr) { continue; }
    const auto len = 128;
    char buffer_name[len]{};
    GetD3d12Name(args_common->buffer_list->resource_list[i], len, buffer_name);
    if (strncmp(buffer_name, "swapchain", 9) == 0) { continue; }
    if (ImGui::Selectable(buffer_name, i == *selected_item)) {
      *selected_item = i;
    }
    if (i == *selected_item) {
      ImGui::SetItemDefaultFocus();
    }
  }
  ImGui::EndListBox();
  ImGui::End();
}
}
