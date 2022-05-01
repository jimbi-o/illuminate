#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#include "shader/include/shader_defines.h"
#include "shader/include/mesh_deform_defines.hlsli"
#include "shader/include/material_defines.hlsli"
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
  RootConstants(num32BitConstants=2, b0,                                \
                visibility=SHADER_VISIBILITY_VERTEX),                   \
  DescriptorTable(CBV(b1, numDescriptors=1),                            \
                  SRV(t0, numDescriptors=1),                            \
                  visibility=SHADER_VISIBILITY_VERTEX),"
struct MeshTransformVsOutput {
  float4 position : SV_POSITION;
  float2 uv0      : TEXCOORD0;
};
struct ModelInfo {
  uint transform_offset;
  uint material_offset;
};
ConstantBuffer<ModelInfo>     model_info : register(b0);
ConstantBuffer<SceneCbvData>  scene_data : register(b1);
StructuredBuffer<matrix>      transforms : register(t0);
#endif
