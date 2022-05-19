#include "d3d12_render_pass_util.h"
namespace illuminate {
RenderPassConfigDynamicData InitRenderPassDynamicData(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, MemoryAllocationJanitor* allocator) {
  RenderPassConfigDynamicData dynamic_data{};
  dynamic_data.render_pass_enable_flag = AllocateArray<bool>(allocator, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    dynamic_data.render_pass_enable_flag[i] = render_pass_list[i].enabled;
  }
  dynamic_data.write_to_sub = AllocateArray<bool*>(allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    dynamic_data.write_to_sub[i] = AllocateArray<bool>(allocator, render_pass_num);
  }
  return dynamic_data;
}
namespace {
static const float kCameraRotationSpeed = 0.005f;
static const auto kPi2 = 2.0f * static_cast<float>(M_PI);
void RotateCameraHorizontal(const float delta, const float camera_focus[3], float* theta, float camera_pos[3]) {
  *theta += delta * kCameraRotationSpeed;
  if (*theta >= kPi2) { *theta -= kPi2; }
  const float diff[2] = {
    camera_pos[0] - camera_focus[0],
    camera_pos[2] - camera_focus[2],
  };
  const auto length = sqrtf(diff[0] * diff[0] + diff[1] * diff[1]);
  camera_pos[0] = length * cosf(*theta) + camera_focus[0];
  camera_pos[2] = length * sinf(*theta) + camera_focus[2];
}
} // namespace
void InitCamera(float camera_pos[3], float camera_rotation[3]) {
  camera_pos[0] = 0.15f;
  float camera_focus[3]{};
  RotateCameraHorizontal(0.0f, camera_focus, &camera_rotation[0], camera_pos);
}
void UpdateCamera(float camera_pos[3], float camera_focus[3], float camera_rotation[3], float* fov_vertical) {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const auto& io = ImGui::GetIO();
    const auto& prev_mouse_pos = io.MousePosPrev;
    const auto& current_mouse_pos = io.MousePos;
    if (ImGui::IsMousePosValid(&prev_mouse_pos) && ImGui::IsMousePosValid(&current_mouse_pos)) {
      if (auto diff = current_mouse_pos.x - prev_mouse_pos.x) {
        RotateCameraHorizontal(diff, camera_focus, &camera_rotation[0], camera_pos);
      }
    }
  }
}
}
