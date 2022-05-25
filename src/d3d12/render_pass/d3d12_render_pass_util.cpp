#include "d3d12_render_pass_util.h"
#include "SimpleMath.h"
namespace illuminate {
namespace {
DirectX::SimpleMath::Vector3 GetArcPos(const float width, const float height, const ImVec2& mouse_pos) {
  using namespace DirectX::SimpleMath;
  const auto center_x = width * 0.5f;
  const auto center_y = height * 0.5f;
  const auto radius = std::min(center_x, center_y);
  Vector3 xyz(mouse_pos.x - center_x, center_y - mouse_pos.y, 0.0f);
  xyz /= radius;
  const auto r = xyz.Dot(xyz);
  if (r > 1.0f) {
    const auto s = 1.0f / sqrtf(r);
    xyz *= s;
  } else {
    xyz.z = sqrtf(1.0f - r);
  }
  return xyz;
}
void RotateCameraImpl(const float width, const float height, float camera_pos[3], float camera_focus[3]) {
  // arcball camera
  // http://orion.lcg.ufrj.br/roma/WebGL/extras/doc/Arcball.pdf
  // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
  using namespace DirectX::SimpleMath;
  const auto& io = ImGui::GetIO();
  const auto& prev_mouse_pos = io.MousePosPrev;
  if (!ImGui::IsMousePosValid(&prev_mouse_pos)) { return; }
  const auto& current_mouse_pos = io.MousePos;
  if (!ImGui::IsMousePosValid(&current_mouse_pos)) { return; }
  if (prev_mouse_pos.x == current_mouse_pos.x && prev_mouse_pos.y == current_mouse_pos.y) { return; }
  auto prev_arc_pos = GetArcPos(width, height, prev_mouse_pos);
  auto current_arc_pos = GetArcPos(width, height, current_mouse_pos);
  const auto theta = acosf(std::min(prev_arc_pos.Dot(current_arc_pos), 1.0f));
  const auto axis_view_space = prev_arc_pos.Cross(current_arc_pos);
  const auto view_to_world_matrix = Matrix::CreateLookAt(Vector3(&camera_pos[0]), Vector3(&camera_focus[0]), Vector3::Up).Invert();
  const auto axis_world_space = Vector3::TransformNormal(axis_view_space, view_to_world_matrix);
  Vector3 position(camera_pos[0] - camera_focus[0], camera_pos[1] - camera_focus[1], camera_pos[2] - camera_focus[2]);
  position = Vector3::Transform(position, Quaternion::CreateFromAxisAngle(axis_world_space, theta));
  camera_pos[0] = position.x + camera_focus[0];
  camera_pos[1] = position.y + camera_focus[1];
  camera_pos[2] = position.z + camera_focus[2];
}
void MoveCameraForward(float camera_pos[3], float camera_focus[3], const float move_amount) {
  using namespace DirectX::SimpleMath;
  Vector3 direction(camera_focus[0] - camera_pos[0], camera_focus[1] - camera_pos[1], camera_focus[2] - camera_pos[2]);
  direction.Normalize();
  direction *= move_amount * 0.01f;
  camera_focus[0] += direction.x;
  camera_pos[0] += direction.x;
  camera_focus[1] += direction.y;
  camera_pos[1] += direction.y;
  camera_focus[2] += direction.z;
  camera_pos[2] += direction.z;
}
} // namespace

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
  dynamic_data.camera_pos[2] = 0.15f;
  return dynamic_data;
}
void UpdateCamera(const float width, const float height, float camera_pos[3], float camera_focus[3]) {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    RotateCameraImpl(width, height, camera_pos, camera_focus);
  }
  if (const auto move_amount = ImGui::GetIO().MouseWheel) {
    MoveCameraForward(camera_pos, camera_focus, move_amount);
  }
}
}
