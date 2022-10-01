#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#ifdef __cplusplus
#include "illuminate/math/math.h"
namespace illuminate::shader {
using uint = uint32_t;
struct float3 {
  float x, y, z;
};
struct float4 {
  float x, y, z, w;
};
struct int2 {
  int32_t x, y;
};
struct matrix {
  float4 m0;
  float4 m1;
  float4 m2;
  float4 m3;
};
#endif
struct SceneCameraData {
  matrix view_matrix;
  matrix projection_matrix;
};
struct MaterialCommonSettings {
  uint  misc_offset;
  uint  _pad[3];
};
struct AlbedoInfo {
  float4 factor;
  uint   tex;
  uint   sampler;
  float  alpha_cutoff;
  uint   _pad;
};
struct MaterialMiscInfo {
  float  metallic_factor;
  float  roughness_factor;
  uint   metallic_roughness_tex;
  uint   metallic_roughness_sampler;
  float  normal_scale;
  uint   normal_tex;
  uint   normal_sampler;
  float  occlusion_strength;
  uint   occlusion_tex;
  uint   occlusion_sampler;
  uint   emissive_tex;
  uint   emissive_sampler;
  float3 emissive_factor;
  uint   _pad;
};
#ifdef __cplusplus
}
#endif
#endif
