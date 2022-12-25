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
struct MaterialCommonSettings { // TODO remove
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
  float3 emissive_factor;
  float  metallic_factor;
  float  roughness_factor;
  float  occlusion_strength;
  float  normal_scale;
  uint   metallic_roughness_occlusion_tex;
  uint   metallic_roughness_occlusion_sampler;
  uint   normal_tex;
  uint   normal_sampler;
  uint   emissive_tex;
  uint   emissive_sampler;
  uint   _pad0;
  uint   _pad1;
  uint   _pad2;
  uint   metallic_roughness_tex; // TODO remove
  uint   metallic_roughness_sampler; // TODO remove
  uint   occlusion_tex; // TODO remove
  uint   occlusion_sampler; // TODO remove
};
#ifdef __cplusplus
}
#endif
#endif
