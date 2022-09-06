#ifndef ILLUMINATE_RESOURCE_SHADER_BRDF_LIGHTING_CS_H
#define ILLUMINATE_RESOURCE_SHADER_BRDF_LIGHTING_CS_H
#ifdef __cplusplus
#include "shader/include/shader_defines.h"
namespace illuminate::shader {
#endif
struct BrdfLightingCBuffer {
  float4 compact_projection_param;
  float4 light_color;
  float3 light_direction_vs;
  float  exposure_rate;
};
#ifdef __cplusplus
} // namespace illuminate::shader
#endif
#endif
