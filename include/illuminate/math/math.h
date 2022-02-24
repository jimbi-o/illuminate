#ifndef ILLUMINATE_MATH_H
#define ILLUMINATE_MATH_H
namespace illuminate {
using float3 = float[3];
using float4 = float[4];
using matrix = float4[4];
constexpr inline void SetVector(const float4& src, float4& dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}
constexpr inline void SubtractVector(const float3& a, const float3& b, float3& dst) {
  dst[0] = b[0] - a[0];
  dst[1] = b[1] - a[1];
  dst[2] = b[2] - a[2];
}
inline void Normalize(float3& v) {
  auto base = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  base = sqrtf(base);
  base = 1.0f / base;
  v[0] *= base;
  v[1] *= base;
  v[2] *= base;
}
constexpr inline void CrossVecotor(const float3& a, const float3& b, float3& dst) {
  dst[0] = a[1] * b[2] - a[2] * b[1];
  dst[1] = a[2] * b[0] - a[0] * b[2];
  dst[2] = a[0] * b[1] - a[1] * b[0];
}
constexpr inline auto Dot(const float3& a, const float3& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
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
inline void GetLookAtLH(const float3& at, const float3& eye, const float3& up, matrix& m) {
  float3 xaxis{}, yaxis{}, zaxis{};
  SubtractVector(at, eye, zaxis);
  Normalize(zaxis);
  CrossVecotor(up, zaxis, xaxis);
  Normalize(xaxis);
  CrossVecotor(zaxis, xaxis, yaxis);
  SetVector(float4{xaxis[0], yaxis[0], zaxis[0], 0.0f}, m[0]);
  SetVector(float4{xaxis[1], yaxis[1], zaxis[1], 0.0f}, m[1]);
  SetVector(float4{xaxis[2], yaxis[2], zaxis[2], 0.0f}, m[2]);
  SetVector(float4{-Dot(xaxis, eye), -Dot(yaxis, eye), -Dot(zaxis, eye), 1.0f}, m[3]);
}
}
#endif

