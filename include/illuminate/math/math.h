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
constexpr inline void SetVector(const float4& src, float4& dst) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}
constexpr inline void SubtractVector(const float3& a, const float3& b, float3& dst) {
  dst[0] = a[0] - b[0];
  dst[1] = a[1] - b[1];
  dst[2] = a[2] - b[2];
}
inline auto Magnitude(const float3& v) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
inline void Normalize(float3& v) {
  auto base = Magnitude(v);
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
constexpr inline void TransposeMatrix(matrix& m) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (i > j) {
        std::swap(m[i][j], m[j][i]);
      }
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
constexpr inline void GetZeroMatrix(matrix& m) {
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      m[i][j] = 0.0f;
    }
  }
}
inline void GetLookAtLH(const float3& eye_position, const float3& look_at, const float3& up_vector, matrix& m) {
  // https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
  float3 xaxis{}, yaxis{}, zaxis{};
  SubtractVector(look_at, eye_position, zaxis);
  Normalize(zaxis);
  CrossVecotor(up_vector, zaxis, xaxis);
  Normalize(xaxis);
  CrossVecotor(zaxis, xaxis, yaxis);
  SetVector(float4{xaxis[0], yaxis[0], zaxis[0], 0.0f}, m[0]);
  SetVector(float4{xaxis[1], yaxis[1], zaxis[1], 0.0f}, m[1]);
  SetVector(float4{xaxis[2], yaxis[2], zaxis[2], 0.0f}, m[2]);
  SetVector(float4{-Dot(xaxis, eye_position), -Dot(yaxis, eye_position), -Dot(zaxis, eye_position), 1.0f}, m[3]);
}
inline void GetPerspectiveProjectionMatirxLH(const float fov_vertical, const float aspect_ratio, const float near_z, const float far_z, matrix& m) {
  // https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
  const auto y_scale = 1.0f / tanf(fov_vertical * 0.5f);
  const auto x_scale = y_scale / aspect_ratio;
  const auto q = far_z / (far_z - near_z);
  GetZeroMatrix(m);
  m[0][0] = x_scale;
  m[1][1] = y_scale;
  m[2][2] = q;
  m[3][2] = -q * near_z;
  m[2][3] = 1.0f;
}
}
#endif
