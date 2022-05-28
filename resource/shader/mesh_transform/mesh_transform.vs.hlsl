#include "shader/include/shader_defines.h"
#include "shader/mesh_transform/mesh_transform.hlsli"
struct VsInput {
  float3 position : POSITION;
  float3 normal   : NORMAL;
  float4 tangent  : TANGENT;
  float2 uv0      : TEXCOORD0;
};
MeshTransformVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID) {
  MeshTransformVsOutput output;
  const matrix world_matrix = transforms.Load<matrix>((model_info.transform_offset + instance_id) * sizeof(matrix));
  const matrix world_view_matrix = mul(world_matrix, scene_data.view_matrix);
  output.position = mul(float4(input.position, 1.0f), world_matrix);
  output.position = mul(output.position, scene_data.view_projection_matrix);
  output.normal   = mul(float4(input.normal, 0.0f), world_view_matrix).xyz;
  output.tangent  = float4(mul(input.tangent, world_view_matrix).xyz, input.tangent.w);
  output.uv0      = input.uv0;
  return output;
}
