#ifndef ILLUMINATE_RESOURCE_SHADER_MATERIAL_FUNCITONS_H
#define ILLUMINATE_RESOURCE_SHADER_MATERIAL_FUNCITONS_H
struct MaterialMisc {
  float3 normal;
  float  occlusion;
  float3 emissive;
  float  metallic;
  float  roughness;
};
void GetColorInfo(uint material_offset, ByteAddressBuffer material_index_list, Texture2D<float4> textures[], sampler samplers[], float2 uv, out float4 color, out float alpha_cutoff) {
  AlbedoInfo albedo_info = material_index_list.Load<AlbedoInfo>(material_offset * sizeof(AlbedoInfo));
  color = textures[albedo_info.tex].Sample(samplers[albedo_info.sampler], uv);
  alpha_cutoff = albedo_info.alpha_cutoff;
  color *= albedo_info.factor;
}
MaterialMisc GetMaterialMisc(uint material_offset, uint misc_offset, ByteAddressBuffer material_index_list, Texture2D<float4> textures[], sampler samplers[], float2 uv) {
  MaterialMiscInfo misc_info = material_index_list.Load<MaterialMiscInfo>(material_offset * sizeof(MaterialMiscInfo) + misc_offset);
  MaterialMisc info;
  float4 metallic_roughness_tex_val = textures[misc_info.metallic_roughness_tex].Sample(samplers[misc_info.metallic_roughness_sampler], uv);
  info.normal    = textures[misc_info.normal_tex].Sample(samplers[misc_info.normal_sampler], uv).rgb;
  info.emissive  = textures[misc_info.emissive_tex].Sample(samplers[misc_info.emissive_sampler], uv).rgb;
  info.occlusion = textures[misc_info.occlusion_tex].Sample(samplers[misc_info.occlusion_sampler], uv).r;
  info.normal    = normalize((info.normal * 2.0f - 1.0f) * float3(misc_info.normal_scale, misc_info.normal_scale, 1.0f));
  info.metallic  = metallic_roughness_tex_val.b * misc_info.metallic_factor;
  info.roughness = metallic_roughness_tex_val.g * misc_info.roughness_factor;
  info.emissive *= misc_info.emissive_factor;
  info.occlusion = 1.0f + misc_info.occlusion_strength * (info.occlusion - 1.0f);
  return info;
}
#endif
