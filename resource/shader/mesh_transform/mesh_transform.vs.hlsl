#include "shader/include/shader_defines.h"
#include "shader/mesh_transform/mesh_transform.hlsli"
struct VsInput {
  float3 position : POSITION;
#ifdef PREZ
#if OPACITY_TYPE==OPACITY_TYPE_ALPHA_MASK
  float2 uv0      : TEXCOORD0;
#endif
#else
  float3 normal   : NORMAL;
  float4 tangent  : TANGENT;
  float2 uv0      : TEXCOORD0;
#endif
};
MeshTransformVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID) {
  MeshTransformVsOutput output;
  const matrix world_matrix = transforms.Load<matrix>((model_info.transform_offset + instance_id) * sizeof(matrix));
  const matrix world_view_matrix = mul(world_matrix, camera_data.view_matrix);
  float4 position = mul(float4(input.position, 1.0f), world_view_matrix);
  output.position = mul(position, camera_data.projection_matrix);
#ifdef PREZ
#if OPACITY_TYPE==OPACITY_TYPE_ALPHA_MASK
  output.uv0      = input.uv0;
#endif
#else
  output.position_vs = position.xyz * rcp(position.w);
  output.normal   = mul(input.normal, (float3x3)world_view_matrix).xyz;
  output.tangent  = float4(mul(input.tangent.xyz, (float3x3)world_view_matrix), input.tangent.w);
  output.uv0      = input.uv0;
#endif
  return output;
}
