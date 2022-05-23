#ifndef ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#include "shader/include/shader_defines.h"
// https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/source/Renderer/shaders/brdf.glsl
struct AlbedoInfo {
  float4    factor;
  Texture2D tex;
  sampler   sampler;
  float     alpha_cutoff;
};
AlbedoInfo GetAlbedoInfo(uint material_offset, ByteAddressBuffer material_index_list,
                         Buffer<float4> colors, Buffer<float> alpha_cutoffs,
                         Texture2D<float4> textures[], sampler samplers[]) {
  AlbedoIndexList albedo_indices = material_index_list.Load<AlbedoIndexList>(material_offset);
  AlbedoInfo info;
  info.tex = textures[albedo_indices.tex];
  info.sampler = samplers[albedo_indices.sampler];
  info.factor = colors.Load(albedo_indices.factor);
  info.alpha_cutoff = alpha_cutoffs.Load(albedo_indices.alpha_cutoff);
  return info;
}
#endif
