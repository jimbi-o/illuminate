#ifndef ILLUMINATE_RESOURCE_SHADER_LINEAR_DEPTH_CS_H
#define ILLUMINATE_RESOURCE_SHADER_LINEAR_DEPTH_CS_H
#ifdef __cplusplus
#include "shader/include/shader_defines.h"
namespace illuminate::shader {
#endif
struct LinearDepthCBuffer {
  float4 zbuffer_to_linear_params;
};
#ifdef __cplusplus
} // namespace illuminate::shader
#endif
#endif
