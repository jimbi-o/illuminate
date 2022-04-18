#include "shader/test/prez.hlsli"
struct VsInput {
  float3 position : POSITION;
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  float2 uv : TEXCOORD0;
#endif
};
uint transform_offset : register(b0);
ConstantBuffer<SceneCbvData> scene_data : register(b1);
StructuredBuffer<matrix> transforms : register(t0);
[RootSignature(PrezRootsig)]
PrezVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID)  {
  PrezVsOutput output;
  output.position = mul(float4(input.position, 1.0f), transforms[transform_offset + instance_id]);
  output.position = mul(output.position, scene_data.view_projection_matrix);
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  output.uv = 0.0f;
#endif
  return output;
}
