#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
float GetLinearDepth(const float zbuffer_depth, const float far_div_near) {
  // https://shikihuiku.github.io/post/projection_matrix/
  return -zbuffer_depth * rcp(far_div_near - zbuffer_depth * (far_div_near - 1.0f));
}
float3 RecoverViewSpacePosition(uint2 coord_xy, float linear_depth, const float4 compact_projection_param) {
  float2 pos = linear_depth * (float2)coord_xy * compact_projection_param.xy;
  return float3(pos, linear_depth);
}
#endif
