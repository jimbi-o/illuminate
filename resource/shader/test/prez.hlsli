#ifndef ILLUMINATE_RESOURCE_SHADER_TEST_PREZ_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_TEST_PREZ_HLSLI
#include "shader/include/shader_defines.h"
#include "shader/include/mesh_deform_defines.hlsli"
#include "shader/include/material_defines.hlsli"
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
  DescriptorTable(CBV(b1, numDescriptors=1),                            \
                  SRV(t0, numDescriptors=1),                            \
                  visibility=SHADER_VISIBILITY_VERTEX),"
struct PrezVsOutput {
  float4 position : SV_POSITION;
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  float2 uv : TEXCOORD0;
#endif
};
#endif
