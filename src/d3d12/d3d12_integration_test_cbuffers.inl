#include "shader/postprocess/linear_depth.cs.h"
#include "shader/postprocess/screen_space_shadow.cs.h"
namespace illuminate {
namespace {
void SetCompactProjectionParam(const DirectX::SimpleMath::Matrix& projection_matrix, shader::float4* compact_projection_param) {
  compact_projection_param->x = projection_matrix.m[0][0];
  compact_projection_param->y = projection_matrix.m[1][1];
  compact_projection_param->z = projection_matrix.m[3][2];
  compact_projection_param->w = -projection_matrix.m[2][2];
}
void SetCameraCbv(const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  using namespace DirectX::SimpleMath;
  shader::SceneCameraData scene_camera{};
  CopyMatrix(dynamic_data.view_matrix.m, scene_camera.view_matrix);
  CopyMatrix(dynamic_data.projection_matrix.m, scene_camera.projection_matrix);
  memcpy(dst, &scene_camera, sizeof(scene_camera));
}
void SetLightCbv(const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  using namespace DirectX::SimpleMath;
  auto light_direction = Vector3::TransformNormal(Vector3(dynamic_data.light_direction), dynamic_data.view_matrix);
  light_direction.Normalize();
  shader::SceneLightData scene_light{};
  scene_light.light_color = {dynamic_data.light_color[0], dynamic_data.light_color[1], dynamic_data.light_color[2], dynamic_data.light_intensity};
  scene_light.light_direction_vs = {light_direction.x, light_direction.y, light_direction.z};
  scene_light.exposure_rate = 10.0f / dynamic_data.light_intensity;
  memcpy(dst, &scene_light, sizeof(scene_light));
}
void SetPingpongACbv([[maybe_unused]]const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  float c[4]{0.0f,1.0f,1.0f,1.0f};
  memcpy(dst, c, GetUint32(sizeof(float)) * 4);
}
void SetPingpongBCbv([[maybe_unused]]const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  float c[4]{1.0f,0.0f,1.0f,1.0f};
  memcpy(dst, c, GetUint32(sizeof(float)) * 4);
}
void SetPingpongCCbv([[maybe_unused]]const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  float c[4]{1.0f,1.0f,1.0f,1.0f};
  memcpy(dst, c, GetUint32(sizeof(float)) * 4);
}
void SetBrdfLightingCbv(const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  using namespace DirectX::SimpleMath;
  auto light_direction = Vector3::TransformNormal(Vector3(dynamic_data.light_direction), dynamic_data.view_matrix);
  light_direction.Normalize();
  shader::BrdfLightingCBuffer params{};
  SetCompactProjectionParam(dynamic_data.projection_matrix, &params.compact_projection_param);
  params.light_color = {dynamic_data.light_color[0], dynamic_data.light_color[1], dynamic_data.light_color[2], dynamic_data.light_intensity};
  params.light_direction_vs = {light_direction.x, light_direction.y, light_direction.z};
  params.exposure_rate = 10.0f / dynamic_data.light_intensity;
  memcpy(dst, &params, sizeof(params));
}
void SetLinearDepthCbv(const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  using namespace DirectX::SimpleMath;
  auto light_direction = Vector3::TransformNormal(Vector3(dynamic_data.light_direction), dynamic_data.view_matrix);
  light_direction.Normalize();
  shader::LinearDepthCBuffer params{};
  SetCompactProjectionParam(dynamic_data.projection_matrix, &params.compact_projection_param);
  memcpy(dst, &params, sizeof(params));
}
void SetScreenSpaceShadowCbv(const RenderPassConfigDynamicData& dynamic_data, void* dst) {
  using namespace DirectX::SimpleMath;
  auto light_direction = Vector3::TransformNormal(Vector3(dynamic_data.light_direction), dynamic_data.view_matrix);
  light_direction.Normalize();
  auto light_origin = Vector3::Transform(light_direction * 100000.0f, dynamic_data.projection_matrix);
  light_origin = (light_origin + Vector3::One) * 0.5f;
  shader::ScreenSpaceShadowCBuffer params{};
  SetCompactProjectionParam(dynamic_data.projection_matrix, &params.compact_projection_param);
  params.step_num = dynamic_data.screen_space_shadow_step_num;
  params.light_origin_location = {
    static_cast<int32_t>(std::round(light_origin.x * dynamic_data.primarybuffer_width)),
    static_cast<int32_t>(std::round(light_origin.y * dynamic_data.primarybuffer_height))
  };
  params.light_slope_zx = light_direction.z / light_direction.x;
  memcpy(dst, &params, sizeof(params));
}
static const StrHash kCBufferNameHash[] = {
  SID("camera"),
  SID("light"),
  SID("deferred_lighting_cbuffer"),
  SID("cbv-a"),
  SID("cbv-b"),
  SID("cbv-c"),
  SID("linear_depth_cbuffer"),
  SID("screen space shadow cbuffer"),
};
static const uint32_t kCBufferSize[] = {
  GetUint32(sizeof(shader::SceneCameraData)),
  GetUint32(sizeof(shader::SceneLightData)),
  GetUint32(sizeof(shader::BrdfLightingCBuffer)),
  GetUint32(sizeof(float) * 4),
  GetUint32(sizeof(float) * 4),
  GetUint32(sizeof(float) * 4),
  GetUint32(sizeof(shader::LinearDepthCBuffer)),
  GetUint32(sizeof(shader::ScreenSpaceShadowCBuffer)),
};
using CbvUpdateFunction = void (*)(const RenderPassConfigDynamicData&, void*);
static const CbvUpdateFunction kCBufferFunctions[] = {
  SetCameraCbv,
  SetLightCbv,
  SetBrdfLightingCbv,
  SetPingpongACbv,
  SetPingpongBCbv,
  SetPingpongCCbv,
  SetLinearDepthCbv,
  SetScreenSpaceShadowCbv,
};
} // namespace
} // namespace illuminate
