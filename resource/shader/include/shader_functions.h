#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
float GetLinearDepth(const float zbuffer_depth, const float4 compact_projection_param) {
  // works in pair with SetCompactProjectionParam
  return compact_projection_param.z * rcp(zbuffer_depth - compact_projection_param.w);
}
float3 RecoverViewSpacePosition(uint2 coord_xy, float linear_depth, const float4 compact_projection_param) {
  // works in pair with SetCompactProjectionParam
  float2 pos = linear_depth * (float2)coord_xy * compact_projection_param.xy;
  return float3(pos, linear_depth);
}
#endif
