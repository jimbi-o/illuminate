struct VsInput {
  float3 position : POSITION;
};
uint transform_offset : register(b0);
StructuredBuffer<matrix> transforms : register(t0);
#define PrezRootsig                                     \
  "RootFlags(                                           \
  ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |                  \
  DENY_HULL_SHADER_ROOT_ACCESS |                        \
  DENY_DOMAIN_SHADER_ROOT_ACCESS |                      \
  DENY_GEOMETRY_SHADER_ROOT_ACCESS |                    \
  DENY_PIXEL_SHADER_ROOT_ACCESS |                       \
  DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |                               \
  DENY_MESH_SHADER_ROOT_ACCESS                                          \
  ),                                                                    \
  RootConstants(num32BitConstants=1, b0,                                \
                visibility=SHADER_VISIBILITY_VERTEX),                   \
  DescriptorTable(SRV(t0, numDescriptors=1),                            \
                  visibility=SHADER_VISIBILITY_VERTEX),"
[RootSignature(PrezRootsig)]
float4 main(const VsInput input, const uint instance_id : SV_InstanceID) : SV_Position {
  float4 output = mul(float4(input.position, 1.0f), transforms[transform_offset + instance_id]);
  return output;
}
