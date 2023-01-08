#include "shader/include/shader_defines.h"
#include "shader/include/material_defines.hlsli"

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

struct MeshTransformVsOutput {
  float4 position    : SV_POSITION;
#ifdef PREZ
#if OPACITY_TYPE==OPACITY_TYPE_ALPHA_MASK
  float2 uv0         : TEXCOORD0;
#endif
#else
  float3 position_vs : POSITION_VS;
  float3 normal      : NORMAL;
  float4 tangent     : TANGENT;
  float2 uv0         : TEXCOORD0;
#endif
};

struct ModelInfo {
  uint model_index;
};

ConstantBuffer<ModelInfo>       model_info                  : register(b0);
ConstantBuffer<SceneCameraData> camera_data                 : register(b1);
ByteAddressBuffer               transform_index_list_offset : register(t0);
ByteAddressBuffer               transform_index_list        : register(t1);
ByteAddressBuffer               transforms                  : register(t2);

#define RootsigGeomTest                                  \
  "RootFlags(                                            \
  ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |                   \
  DENY_HULL_SHADER_ROOT_ACCESS |                         \
  DENY_DOMAIN_SHADER_ROOT_ACCESS |                       \
  DENY_GEOMETRY_SHADER_ROOT_ACCESS |                     \
  DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |                \
  DENY_MESH_SHADER_ROOT_ACCESS                           \
  ),                                                     \
  RootConstants(num32BitConstants=1, b0),                \
  DescriptorTable(CBV(b1, numDescriptors=1),             \
                  SRV(t0, numDescriptors=1),             \
                  SRV(t1, numDescriptors=1),             \
                  SRV(t2, numDescriptors=1)),            \
"

[RootSignature(RootsigGeomTest)]
MeshTransformVsOutput main(const VsInput input, const uint instance_id : SV_InstanceID) {
  MeshTransformVsOutput output;
  const uint transform_index_offset = transform_index_list_offset.Load<uint>(model_info.model_index * sizeof(uint));
  const uint transform_index = transform_index_list.Load<uint>((transform_index_offset + instance_id) * sizeof(uint));
  const matrix world_matrix = transforms.Load<matrix>(transform_index * sizeof(matrix));
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
