#ifndef ILLUMINATE_MATH_H
#define ILLUMINATE_MATH_H
namespace illuminate {
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
constexpr inline void MultiplyMatrix(const matrix& a, const matrix& b, matrix& dst) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      dst[i][j] = a[i][0] * b[0][j]
                + a[i][1] * b[1][j]
                + a[i][2] * b[2][j]
                + a[i][3] * b[3][j];
    }
  }
}
constexpr inline void TransposeMatrix(const matrix& src, matrix& dst) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      dst[i][j] = src[j][i];
    }
  }
}
constexpr inline void GetIdentityMatrix(matrix& m) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }
}
}
#endif

