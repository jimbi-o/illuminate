#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_FUNCTIONS_H
float GetLinearDepth(const float zbuffer_depth, const float4 zbuffer_to_linear_params) {
  // https://stackoverflow.com/questions/11277501/how-to-recover-view-space-position-given-view-space-depth-value-and-ndc-xy
  // https://docs.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
  return zbuffer_to_linear_params.z * rcp(zbuffer_to_linear_params.w + zbuffer_depth);
}
float3 RecoverViewSpacePosition(uint2 coord_xy, float linear_depth, const float4 zbuffer_to_linear_params) {
  // https://stackoverflow.com/questions/11277501/how-to-recover-view-space-position-given-view-space-depth-value-and-ndc-xy
  // https://docs.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
  float2 pos = linear_depth * (float2)coord_xy * zbuffer_to_linear_params.xy;
  return float3(pos, linear_depth);
}
#endif
