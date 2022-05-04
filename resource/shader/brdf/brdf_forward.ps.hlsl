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
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded), \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
ByteAddressBuffer  material_index_list : register(t1);
Buffer<float4>     colors              : register(t2);
Buffer<float>      alpha_cutoffs       : register(t3);
Texture2D<float4>  textures[]          : register(t4);
sampler            samplers[]          : register(s0);
[RootSignature(BrdfForwardRootsig)]
float4 main(MeshTransformVsOutput input) : SV_TARGET0 {
#if 1
  MaterialIndexList material_indices = material_index_list.Load<MaterialIndexList>(model_info.material_offset * 4);
  float4 albedo_factor = colors.Load(material_indices.albedo_factor);
  return albedo_factor;
#else
  MaterialIndexList  material_indices = material_index_list.Load<MaterialIndexList>(model_info.material_offset);
  Texture2D<float4>  albedo_tex       = textures[material_indices.albedo_tex];
  sampler            albedo_sampler   = samplers[material_indices.albedo_sampler];
  float4             albedo           = albedo_tex.Sample(albedo_sampler, input.uv0);
  float4             albedo_factor    = colors.Load(material_indices.albedo_factor);
  float              alpha_cutoff     = alpha_cutoffs.Load(material_indices.alpha_cutoff);
  albedo *= albedo_factor;
  if (albedo.w < alpha_cutoff) { discard; }
  return albedo;
#endif
}
#if 0
    {
      "name": "colors"
    },
    {
      "name": "alpha_cutoffs"
    },
    {
      "name": "textures"
    },
        {
          "name": "colors",
          "state": "srv_ps",
          "index_offset" : 1
        },
        {
          "name": "alpha_cutoffs",
          "state": "srv_ps",
          "index_offset" : 1
        },
        {
          "name": "textures",
          "state": "srv_ps",
          "index_offset" : 1
        },
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
                  SRV(t4, numDescriptors=unbounded),     \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded), \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
#endif
