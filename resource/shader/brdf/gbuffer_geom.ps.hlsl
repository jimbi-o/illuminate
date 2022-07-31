#include "shader/brdf/brdf.hlsli"
#include "shader/include/shader_defines.h"
#include "shader/include/material_defines.hlsli"
#include "shader/include/material_functions.hlsli"
#include "shader/mesh_transform/mesh_transform.hlsli"
ConstantBuffer<MaterialCommonSettings> material_common_settings : register(b3);
ByteAddressBuffer  material_index_list : register(t1);
Texture2D<float4>  textures[]          : register(t2);
sampler            samplers[]          : register(s0);
struct GBuffers {
  float4 gbuffer0 : SV_TARGET0;
  float4 gbuffer1 : SV_TARGET1;
  float4 gbuffer2 : SV_TARGET2;
  float4 gbuffer3 : SV_TARGET3;
};
#define GBufferRootsig                                   \
  "RootFlags(                                            \
  ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |                   \
  DENY_HULL_SHADER_ROOT_ACCESS |                         \
  DENY_DOMAIN_SHADER_ROOT_ACCESS |                       \
  DENY_GEOMETRY_SHADER_ROOT_ACCESS |                     \
  DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |                \
  DENY_MESH_SHADER_ROOT_ACCESS                           \
  ),                                                     \
  RootConstants(num32BitConstants=2, b0),                \
  DescriptorTable(CBV(b1, numDescriptors=1),             \
                  SRV(t0, numDescriptors=1)),            \
  DescriptorTable(CBV(b3, numDescriptors=1),             \
                  SRV(t1, numDescriptors=1),             \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(SRV(t2, numDescriptors=unbounded,      \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded,  \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
[RootSignature(GBufferRootsig)]
GBuffers main(MeshTransformVsOutput input) {
  float4 color;
  float  alpha_cutoff;
  GetColorInfo(model_info.material_offset, material_index_list, textures, samplers, input.uv0, color, alpha_cutoff);
  MaterialMisc material_misc = GetMaterialMisc(model_info.material_offset, material_common_settings. misc_offset, material_index_list, textures, samplers, input.uv0);
  float3 normal    = normalize(input.normal);
  float3 tangent   = normalize(input.tangent.xyz);
  float3 bitangent = cross(normal, tangent) * input.tangent.w;
  float3x3 m = float3x3(tangent, bitangent, normal);
  float3 n = normalize(mul(material_misc.normal, m));
  GBuffers gbuffers;
  gbuffers.gbuffer0 = float4(color.rgb, 1.0f);
  gbuffers.gbuffer1 = float4(n, 1.0f);
  gbuffers.gbuffer2 = float4(material_misc.metallic, material_misc.roughness, material_misc.occlusion, material_misc.occlusion_strength);
  gbuffers.gbuffer3 = float4(material_misc.emissive, 1.0f);
  return gbuffers;
}
