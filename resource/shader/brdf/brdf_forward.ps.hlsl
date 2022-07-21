#include "shader/brdf/brdf.hlsli"
#include "shader/include/shader_defines.h"
#include "shader/include/material_defines.hlsli"
#include "shader/include/material_functions.hlsli"
#include "shader/mesh_transform/mesh_transform.hlsli"
#define BrdfForwardRootsig                               \
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
  DescriptorTable(CBV(b2, numDescriptors=1),             \
                  CBV(b3, numDescriptors=1),             \
                  SRV(t1, numDescriptors=1),             \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(SRV(t2, numDescriptors=unbounded,      \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded,  \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
ConstantBuffer<SceneLightData> scene_light                      : register(b2);
ConstantBuffer<MaterialCommonSettings> material_common_settings : register(b3);
ByteAddressBuffer  material_index_list : register(t1);
Texture2D<float4>  textures[]          : register(t2);
sampler            samplers[]          : register(s0);
[RootSignature(BrdfForwardRootsig)]
float4 main(MeshTransformVsOutput input) : SV_TARGET0 {
  float4 color;
  float  alpha_cutoff;
  GetColorInfo(model_info.material_offset, material_index_list, textures, samplers, input.uv0, color, alpha_cutoff);
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  if (color.a < alpha_cutoff) { discard; }
#endif
  MaterialMisc material_misc = GetMaterialMisc(model_info.material_offset, material_common_settings. misc_offset, material_index_list, textures, samplers, input.uv0);
  float3 normal    = normalize(input.normal);
  float3 tangent   = normalize(input.tangent.xyz);
  float3 bitangent = cross(normal, tangent) * input.tangent.w;
  float3x3 m = float3x3(tangent, bitangent, normal);
  float3 n = normalize(mul(material_misc.normal, m));
  float3 v = normalize(-input.position_vs); // camera is at origin
  float3 l = scene_light.light_direction_vs;
  float3 h = normalize(l + v); // half vector
  float vh = clamp(dot(v, h), 0.0f, 1.0f);
  float nl = clamp(dot(n, l), 0.0f, 1.0f);
  float nv = clamp(dot(n, v), 0.0f, 1.0f);
  float nh = clamp(dot(n, h), 0.0f, 1.0f);
  float3 brdf = BrdfDefault(color.rgb, material_misc.metallic, material_misc.roughness, vh, nl, nv, nh);
  return float4((brdf * nl * scene_light.light_color.xyz * scene_light.light_color.w) * scene_light.exposure_rate + material_misc.emissive, 1.0f);
}
