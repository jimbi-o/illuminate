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
  DescriptorTable(SRV(t1, numDescriptors=1),             \
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
ByteAddressBuffer  albedo_index_list : register(t1);
Buffer<float4>     colors            : register(t2);
Buffer<float>      alpha_cutoffs     : register(t3);
Texture2D<float4>  textures[]        : register(t4);
sampler            samplers[]        : register(s0);
[RootSignature(BrdfForwardRootsig)]
float4 main(MeshTransformVsOutput input) : SV_TARGET0 {
  AlbedoIndexList albedo_indices = albedo_index_list.Load<AlbedoIndexList>(model_info.material_offset);
  Texture2D<float4> tex = textures[albedo_indices.tex];
  sampler sampler = samplers[albedo_indices.sampler];
  float4 color = tex.Sample(sampler, input.uv0);
  float4 factor = colors.Load(albedo_indices.factor);
  float alpha_cutoff = alpha_cutoffs.Load(albedo_indices.alpha_cutoff);
  float4 albedo = color * factor;
#if OPACITY_TYPE == OPACITY_TYPE_ALPHA_MASK
  if (albedo.a < alpha_cutoff) { discard; }
#endif
  return albedo;
}
