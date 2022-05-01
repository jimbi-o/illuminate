#include "shader/mesh_transform/mesh_transform.hlsli"
struct VsInput {
  float3 position : POSITION;
  float2 uv0      : TEXCOORD0;
};
[RootSignature(MeshTransformRootsig)]
MeshTransformVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID) {
  MeshTransformVsOutput output;
  output.position = mul(float4(input.position, 1.0f), transforms[model_info.transform_offset + instance_id]);
  output.position = mul(output.position, scene_data.view_projection_matrix);
  output.uv0 = input.uv0;
  return output;
}
