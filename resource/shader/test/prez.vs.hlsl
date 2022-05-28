#include "shader/test/prez.hlsli"
struct VsInput {
  float3 position : POSITION;
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  float2 uv : TEXCOORD0;
#endif
};
uint                          transform_offset : register(b0);
ConstantBuffer<SceneCbvData>  scene_data       : register(b1);
ByteAddressBuffer             transforms       : register(t0);
[RootSignature(PrezRootsig)]
PrezVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID)  {
  PrezVsOutput output;
  const matrix world_matrix = transforms.Load<matrix>((transform_offset + instance_id) * sizeof(matrix));
  output.position = mul(float4(input.position, 1.0f), world_matrix);
  output.position = mul(output.position, scene_data.view_projection_matrix);
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  output.uv = input.uv;
#endif
  return output;
}
