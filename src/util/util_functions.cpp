#include "illuminate/util/util_functions.h"
#include "SimpleMath.h"
namespace illuminate {
bool SucceedMultipleIndex(const uint32_t array_size, const uint32_t* array_size_array, uint32_t* array) {
  for (uint32_t i = 0; i < array_size; i++) {
    auto index = array_size - i - 1;
    if (array[index] + 1 < array_size_array[index]) {
      array[index]++;
      return true;
    }
    array[index] = 0;
  }
  return false;
}
void SetArrayValues(const uint32_t val, const uint32_t array_size, uint32_t* array) {
  for (uint32_t i = 0; i < array_size; i++) {
    array[i] = val;
  }
}
namespace {
DirectX::SimpleMath::Vector3 GetArcPos(const float width, const float height, const float mouse_pos[2]) {
  using namespace DirectX::SimpleMath;
  const auto center_x = width * 0.5f;
  const auto center_y = height * 0.5f;
  const auto radius = std::min(center_x, center_y);
  Vector3 xyz(mouse_pos[0] - center_x, center_y - mouse_pos[1], 0.0f);
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
} // namespace
void RotateCamera(const float width, const float height, float camera_pos[3], float camera_focus[3], const float prev_mouse_pos[2], const float current_mouse_pos[2]) {
  // arcball camera
  // http://orion.lcg.ufrj.br/roma/WebGL/extras/doc/Arcball.pdf
  // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
  using namespace DirectX::SimpleMath;
  if (prev_mouse_pos[0] == current_mouse_pos[0] && prev_mouse_pos[1] == current_mouse_pos[1]) { return; }
  auto prev_arc_pos = GetArcPos(width, height, prev_mouse_pos);
  auto current_arc_pos = GetArcPos(width, height, current_mouse_pos);
  const auto theta = acosf(std::min(prev_arc_pos.Dot(current_arc_pos), 1.0f));
  const auto axis_view_space = prev_arc_pos.Cross(current_arc_pos);
  if (axis_view_space.Dot(axis_view_space) <= 0.001f) { return; }
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
  direction *= move_amount * 0.01f;
  camera_focus[0] += direction.x;
  camera_pos[0] += direction.x;
  camera_focus[1] += direction.y;
  camera_pos[1] += direction.y;
  camera_focus[2] += direction.z;
  camera_pos[2] += direction.z;
}
void UpdateTimestamp(uint32_t* frame_count, std::chrono::high_resolution_clock::time_point* last_time_point, float* delta_time_msec, float* duration_msec_sum) {
  (*frame_count)++;
  const auto current_time_point = std::chrono::high_resolution_clock::now();
  *delta_time_msec = std::chrono::duration<float, std::milli>(current_time_point - *last_time_point).count();
  *last_time_point = current_time_point;
  *duration_msec_sum += *delta_time_msec;
}
void CalcTimestampAverage(uint32_t* frame_count, float* duration_msec_sum, float* prev_duration_per_frame_msec_avg) {
  *prev_duration_per_frame_msec_avg = *duration_msec_sum / static_cast<float>(*frame_count);
  *duration_msec_sum = 0.0f;
  *frame_count = 0;
}
bool UpdateTimeDuration(const float frame_count_reset_time_threshold_msec, uint32_t* frame_count, std::chrono::high_resolution_clock::time_point* last_time_point, float* delta_time_msec, float* duration_msec_sum, float* prev_duration_per_frame_msec_avg) {
  UpdateTimestamp(frame_count, last_time_point, delta_time_msec, duration_msec_sum);
  if (*duration_msec_sum >= frame_count_reset_time_threshold_msec) {
    CalcTimestampAverage(frame_count, duration_msec_sum, prev_duration_per_frame_msec_avg);
    return true;
  }
  return false;
}
}
#include "doctest/doctest.h"
#include "illuminate/memory/memory_allocation.h"
TEST_CASE("succeed multiple index") {
  using namespace illuminate;
  const uint32_t buffer_size = 128;
  std::byte buffer[buffer_size]{};
  LinearAllocator allocator(buffer, buffer_size);
  const uint32_t array_size = 4;
  auto array_size_array = AllocateArray<uint32_t>(&allocator, array_size);
  array_size_array[0] = 2;
  array_size_array[1] = 4;
  array_size_array[2] = 1;
  array_size_array[3] = 3;
  auto array = AllocateArray<uint32_t>(&allocator, array_size);
  for (uint32_t i = 0; i < array_size; i++) {
    array[i] = 0;
  }
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 1);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 2);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 1);
  CHECK_UNARY(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 1);
  CHECK_EQ(array[1], 3);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 2);
  CHECK_UNARY_FALSE(SucceedMultipleIndex(array_size, array_size_array, array));
  CHECK_EQ(array[0], 0);
  CHECK_EQ(array[1], 0);
  CHECK_EQ(array[2], 0);
  CHECK_EQ(array[3], 0);
}
TEST_CASE("UpdateTimeDuration") { // NOLINT
  using namespace illuminate; // NOLINT
  float frame_count_reset_time_threshold_msec = 500.0f;
  uint32_t frame_count = 0U;
  const auto time_point_initial_val = std::chrono::high_resolution_clock::now() - std::chrono::milliseconds(16);
  auto last_time_point = time_point_initial_val;
  float delta_time_msec = 0.0f;
  float duration_msec_sum = 0.0f;
  float prev_duration_per_frame_msec_avg = 0.0f;
  UpdateTimeDuration(frame_count_reset_time_threshold_msec, &frame_count, &last_time_point, &delta_time_msec, &duration_msec_sum, &prev_duration_per_frame_msec_avg);
  CHECK_EQ(frame_count, 1U);
  CHECK_GE(last_time_point, time_point_initial_val);
  CHECK_LE(last_time_point, std::chrono::high_resolution_clock::now());
  CHECK_GE(delta_time_msec, 16.0f);
  CHECK_EQ(duration_msec_sum, delta_time_msec);
  CHECK_EQ(prev_duration_per_frame_msec_avg, 0.0f);
  const auto prev_time_point_val = last_time_point;
  auto prev_duration = duration_msec_sum;
  UpdateTimeDuration(frame_count_reset_time_threshold_msec, &frame_count, &last_time_point, &delta_time_msec, &duration_msec_sum, &prev_duration_per_frame_msec_avg);
  CHECK_EQ(frame_count, 2U);
  CHECK_GE(last_time_point, prev_time_point_val);
  CHECK_LE(last_time_point, std::chrono::high_resolution_clock::now());
  CHECK_GE(delta_time_msec, 0.0f);
  CHECK_EQ(duration_msec_sum, prev_duration + delta_time_msec);
  CHECK_EQ(prev_duration_per_frame_msec_avg, 0.0f);
  prev_duration = duration_msec_sum;
  last_time_point = std::chrono::high_resolution_clock::now() - std::chrono::milliseconds(500);
  UpdateTimeDuration(frame_count_reset_time_threshold_msec, &frame_count, &last_time_point, &delta_time_msec, &duration_msec_sum, &prev_duration_per_frame_msec_avg);
  CHECK_EQ(frame_count, 0U);
  CHECK_GE(last_time_point, std::chrono::high_resolution_clock::now() - std::chrono::milliseconds(500));
  CHECK_LE(last_time_point, std::chrono::high_resolution_clock::now());
  CHECK_GE(delta_time_msec, 500.0f);
  CHECK_EQ(duration_msec_sum, 0.0f);
  CHECK_GT(prev_duration_per_frame_msec_avg, (500.0f + prev_duration) / 3.0f);
}
