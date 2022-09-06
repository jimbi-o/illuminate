#ifndef ILLUMINATE_RESOURCE_SHADER_SCREEN_SPACE_SHADOW_CS_H
#define ILLUMINATE_RESOURCE_SHADER_SCREEN_SPACE_SHADOW_CS_H
#ifdef __cplusplus
#include "shader/include/shader_defines.h"
namespace illuminate::shader {
#endif
struct ScreenSpaceShadowCBuffer {
  float4 compact_projection_param;
  int2   light_origin_location;
  uint   step_num;
  float  light_slope_zx;
};
#ifdef __cplusplus
} // namespace illuminate::shader
#endif
#endif
