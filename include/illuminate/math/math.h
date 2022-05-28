#ifndef ILLUMINATE_MATH_H
#define ILLUMINATE_MATH_H
#define _USE_MATH_DEFINES
#include <math.h>
namespace illuminate {
static const auto PI = static_cast<float>(M_PI);
inline auto ToRadian(const float degrees) {
  return degrees * PI / 180.0f;
}
using float3 = float[3];
using float4 = float[4];
using matrix = float4[4];
constexpr inline void CopyMatrix(const matrix& src, matrix& dst) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      dst[i][j] = src[i][j];
    }
  }
}
constexpr inline void FillMatrix(const double* d, matrix& dst) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      dst[i][j] = static_cast<float>(d[i * 4 + j]);
    }
  }
}
constexpr inline void FillMatrixTransposed(const double* d, matrix& dst) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      dst[i][j] = static_cast<float>(d[j * 4 + i]);
    }
  }
}
}
#endif
