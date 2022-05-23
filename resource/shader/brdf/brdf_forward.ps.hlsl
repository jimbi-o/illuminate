#include "shader/brdf/brdf.hlsli"
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
                  SRV(t0, numDescriptors=1),             \
                  visibility=SHADER_VISIBILITY_VERTEX),  \
  DescriptorTable(CBV(b2, numDescriptors=1),             \
                  SRV(t1, numDescriptors=1),             \
                  SRV(t2, numDescriptors=1),             \
                  SRV(t3, numDescriptors=1),             \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(SRV(t4, numDescriptors=unbounded,      \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded,  \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
ConstantBuffer<MaterialCommonSettings> material_common_settings : register(b2);
ByteAddressBuffer  material_index_list : register(t1);
Buffer<float4>     colors              : register(t2);
Buffer<float>      alpha_cutoffs       : register(t3);
Texture2D<float4>  textures[]          : register(t4);
sampler            samplers[]          : register(s0);
[RootSignature(BrdfForwardRootsig)]
float4 main(MeshTransformVsOutput input) : SV_TARGET0 {
  AlbedoInfo albedo_info = GetAlbedoInfo(model_info.material_offset, material_index_list, colors, alpha_cutoffs, textures, samplers);
  float4 color = albedo_info.tex.Sample(albedo_info.sampler, input.uv0);
  float4 albedo = color * albedo_info.factor;
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  if (albedo.a < albedo_info.alpha_cutoff) { discard; }
#endif
  // MetallicRoughnessIndexList metallic_roughness_indices = material_index_list.Load<MetallicRoughnessIndexList>(material_common_settings.metallic_roughness_offset + model_info.material_offset);
  return albedo;
}
