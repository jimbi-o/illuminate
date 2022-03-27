#include "d3d12_render_pass_imgui.h"
#include "d3d12_render_pass_debug_render_selected_buffer.h"
namespace illuminate {
namespace {
struct Param {
  uint32_t render_pass_index_output_to_swapchain{};
  uint32_t render_pass_index_debug_show_selected_buffer{};
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
  return param;
}
void RenderPassImgui::RegisterGUI(RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (!ImGui::Begin("render pass config", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) { return; }
  {
    // camera settings
    ImGui::SliderFloat3("camera pos", args_common->dynamic_data->camera_pos, -10.0f, 10.0f, "%.1f");
    ImGui::SliderFloat3("camera focus", args_common->dynamic_data->camera_focus, -10.0f, 10.0f, "%.1f");
    ImGui::SliderFloat("camera fov(vertical)", &args_common->dynamic_data->fov_vertical, 1.0f, 360.0f, "%.0f");
    ImGui::SliderFloat("near_z", &args_common->dynamic_data->near_z, 0.001f, 1.0f, "%.2f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
    ImGui::SliderFloat("far_z", &args_common->dynamic_data->far_z, 1.0f, 10000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
  }
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
  auto selected_item = &args_common->dynamic_data->debug_render_selected_buffer_allocation_index;
  auto tmp_allocator = GetTemporalMemoryAllocator();
  uint32_t* buffer_allocation_index_list = nullptr;
  auto buffer_allocation_index_len = RenderPassDebugRenderSelectedBuffer::GetBufferAllocationIndexList(*args_common->buffer_list, args_common->buffer_config_list, &tmp_allocator, &buffer_allocation_index_list);
  for (uint32_t i = 0; i < buffer_allocation_index_len; i++) {
    const auto index = buffer_allocation_index_list[i];
    const auto len = 128;
    char buffer_name[len]{};
    GetD3d12Name(args_common->buffer_list->resource_list[index], len, buffer_name);
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
