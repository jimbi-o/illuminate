#include "shader/include/shader_functions.h"
#include "shader/postprocess/screen_space_shadow.cs.h"
// https://panoskarabelas.com/posts/screen_space_shadows/
ConstantBuffer<ScreenSpaceShadowCBuffer> params : register(b0);
Texture2D<float> linear_depth_tex : register(t0);
RWTexture2D<float> shadow_tex : register(u0);
#define ScreenSpaceShadowRootsig               \
  "DescriptorTable(                            \
   CBV(b0),                                    \
   SRV(t0),                                    \
   UAV(u0),                                    \
   visibility=SHADER_VISIBILITY_ALL)"
float GetShadowValue(uint2 start, float3 initial_pos) {
  // Bresenham
  int2 p0  = int2(start);
  int2 p1  = params.light_origin_location;
  int2 dxy  = abs(p1 - p0);
  int2 sxy  = int2(p0.x < p1.x ? 1 : -1, p0.y < p1.y ? 1 : -1);
  int error = dxy.x + dxy.y;
  for (uint i = 0; i < params.step_num; i++) {
    if (!all(p0 - p1)) { break; }
    int e2 = error * 2;
    if (e2 > dxy.y) {
      if (p0.x == p1.x) { break; }
      error += dxy.y;
      p0.x += sxy.x;
    }
    if (e2 <= dxy.x) {
      if (p0.y == p1.y) { break; }
      error += dxy.x;
      dxy.y += sxy.y;
    }
    float depth = linear_depth_tex.Load(uint3(p0, 0));
    float3 pos_viewspace = RecoverViewSpacePosition(p0, depth, params.compact_projection_param);
    float ray_pos_depth = params.light_slope_zx * (pos_viewspace.x - initial_pos.x) + initial_pos.z;
    if (ray_pos_depth > pos_viewspace.z) { return 0.0f; }
  }
  return 1.0f;
}
[RootSignature(ScreenSpaceShadowRootsig)]
[numthreads(16,16,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uint3  location = uint3(thread_id.xy, 0);
  float  initial_depth = linear_depth_tex.Load(location).r;
  float3 initial_pos = RecoverViewSpacePosition(thread_id.xy, initial_depth, params.compact_projection_param);
  shadow_tex[location.xy] = GetShadowValue(location.xy, initial_pos);
}