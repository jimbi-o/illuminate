#ifndef ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#define ILLUMINATE_RESOURCE_SHADER_SHADER_DEFINE_H
#ifdef __cplusplus
namespace illuminate::shader {
using uint = uint32_t;
#endif
struct SceneCbvData {
  matrix view_projection_matrix;
};
struct MaterialIndexList {
  uint albedo_factor;
  uint albedo_tex;
  uint albedo_sampler;
  uint alpha_cutoff;
};
#ifdef __cplusplus
}
#endif
#endif
