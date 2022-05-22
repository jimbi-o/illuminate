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
struct AlbedoIndexList {
  uint factor;
  uint tex;
  uint sampler;
  uint alpha_cutoff;
};
#ifdef __cplusplus
}
#endif
#endif
