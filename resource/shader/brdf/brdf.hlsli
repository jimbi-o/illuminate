#ifndef ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#include "shader/include/shader_defines.h"
// https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/source/Renderer/shaders/brdf.glsl
struct Albedo {
  float4    factor;
  Texture2D tex;
  sampler   sampler;
  float     alpha_cutoff;
};
Albedo GetAlbedoInfo(uint material_offset, ByteAddressBuffer material_index_list, Texture2D<float4> textures[], sampler samplers[]) {
  AlbedoInfo albedo_info = material_index_list.Load<AlbedoInfo>(material_offset * sizeof(AlbedoInfo));
  Albedo info;
  info.tex = textures[albedo_info.tex];
  info.sampler = samplers[albedo_info.sampler];
  info.factor = albedo_info.factor;
  info.alpha_cutoff = albedo_info.alpha_cutoff;
  return info;
}
#endif
