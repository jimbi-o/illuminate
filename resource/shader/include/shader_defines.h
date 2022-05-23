#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#ifdef __cplusplus
#include "illuminate/math/math.h"
namespace illuminate::shader {
using uint = uint32_t;
#endif
struct SceneCbvData {
  matrix view_projection_matrix;
};
struct MaterialCommonSettings {
  uint metallic_roughness_offset;
  uint normal_offset;
  uint emissive_offset;
  uint _pad;
};
struct AlbedoIndexList {
  uint factor;
  uint tex;
  uint sampler;
  uint alpha_cutoff;
};
struct MetallicRoughnessIndexList {
  uint  metallic_factor;
  uint  roughness_factor;
  uint  tex;
  uint  sampler;
};
struct NormalIndexList{
  float factor;
  uint  tex;
  uint  sampler;
  uint _pad;
};
struct OcclusionIndexList {
  float factor;
  uint  tex;
  uint  sampler;
  uint _pad;
};
struct EmissiveIndexList {
  uint  factor_index;
  uint  tex;
  uint  sampler;
  uint _pad;
};
#ifdef __cplusplus
}
#endif
#endif
