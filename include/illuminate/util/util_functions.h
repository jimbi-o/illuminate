#ifndef ILLUMINATE_UTIL_UTIL_FUNCTIONS_H
#define ILLUMINATE_UTIL_UTIL_FUNCTIONS_H
namespace illuminate {
bool SucceedMultipleIndex(const uint32_t array_size, const uint32_t* array_size_array, uint32_t* array);
void SetArrayValues(const uint32_t val, const uint32_t array_size, uint32_t* array);
void RotateCamera(const float width, const float height, float camera_pos[3], float camera_focus[3], const float prev_mouse_pos[2], const float current_mouse_pos[2]);
void MoveCameraForward(float camera_pos[3], float camera_focus[3], const float move_amount);
void UpdateTimestamp(uint32_t* frame_count, std::chrono::high_resolution_clock::time_point* last_time_point, float* delta_time_msec, float* duration_msec_sum);
void CalcTimestampAverage(const float frame_count_reset_time_threshold_msec, uint32_t* frame_count, float* duration_msec_sum, float* prev_duration_per_frame_msec_avg);
struct TimeDurationDataSet {
  float frame_count_reset_time_threshold_msec{250.0f};
  uint32_t frame_count{0U};
  std::chrono::high_resolution_clock::time_point last_time_point{std::chrono::high_resolution_clock::now()};
  float delta_time_msec{0.0f};
  float duration_msec_sum{0.0f};
  float prev_duration_per_frame_msec_avg{0.0f};
};
bool UpdateTimeDuration(TimeDurationDataSet* time_duration_data_set);
void ResetTimeDuration(TimeDurationDataSet* time_duration_data_set);
}
#endif
