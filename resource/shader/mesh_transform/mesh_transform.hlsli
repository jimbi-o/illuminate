#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define MeshTransformRootsig                            \
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
  DescriptorTable(CBV(b1, numDescriptors=1),                            \
                  SRV(t0, numDescriptors=1),                            \
                  visibility=SHADER_VISIBILITY_VERTEX),"
struct MeshTransformVsOutput {
  float4 position : SV_Position;
};
#endif
