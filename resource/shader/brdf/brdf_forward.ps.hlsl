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
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(SRV(t2, numDescriptors=unbounded,      \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
  DescriptorTable(Sampler(s0, numDescriptors=unbounded,  \
                  flags = DESCRIPTORS_VOLATILE),         \
                  visibility=SHADER_VISIBILITY_PIXEL),   \
"
ConstantBuffer<MaterialCommonSettings> material_common_settings : register(b2);
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
  return float4(material_misc.emissive, 1.0f);
}
