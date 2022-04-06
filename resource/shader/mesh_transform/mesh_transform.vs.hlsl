#include "shader/mesh_transform/mesh_transform.hlsli"
#include "shader/include/shader_defines.h"
struct VsInput {
  float3 position : POSITION;
};
uint transform_offset : register(b0);
ConstantBuffer<SceneCbvData> scene_data : register(b1);
StructuredBuffer<matrix> transforms : register(t0);
[RootSignature(MeshTransformRootsig)]
MeshTransformVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID) {
  MeshTransformVsOutput output;
  output.position = mul(float4(input.position, 1.0f), transforms[transform_offset + instance_id]);
  output.position = mul(output.position, scene_data.view_projection_matrix);
  return output;
}
