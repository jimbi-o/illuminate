#include "illuminate/math/math.h"
#include "doctest/doctest.h"
#include "directxmath.h"
TEST_CASE("math/vectors simple") { // NOLINT
  using namespace illuminate; // NOLINT
  float3 a{1.0f, 0.5f, 0.1f}, b{2.0f, 1.0f, 0.01f}, c{};
  SubtractVector(a, b, c);
  for (uint32_t i = 0; i < 3; i++) {
    CAPTURE(i);
    CHECK_EQ(a[i] - b[i], doctest::Approx(c[i]));
  }
  Normalize(a);
  CHECK_EQ(Magnitude(a), 1.0f);
}
TEST_CASE("math/vector products") { // NOLINT
  using namespace illuminate; // NOLINT
  float3 a{1.0f, 0.0f, 0.0f}, b{0.0f, 1.0f, 0.0f}, c{};
  CrossVecotor(a, b, c);
  CHECK_EQ(c[0], doctest::Approx(0.0f));
  CHECK_EQ(c[1], doctest::Approx(0.0f));
  CHECK_EQ(c[2], doctest::Approx(1.0f));
  CHECK_EQ(Dot(a, a), doctest::Approx(1.0f));
  CHECK_EQ(Dot(a, b), doctest::Approx(0.0f));
  CHECK_EQ(Dot(a, c), doctest::Approx(0.0f));
  CHECK_EQ(Dot(b, a), doctest::Approx(0.0f));
  CHECK_EQ(Dot(b, b), doctest::Approx(1.0f));
  CHECK_EQ(Dot(b, c), doctest::Approx(0.0f));
  CHECK_EQ(Dot(c, a), doctest::Approx(0.0f));
  CHECK_EQ(Dot(c, b), doctest::Approx(0.0f));
  CHECK_EQ(Dot(c, c), doctest::Approx(1.0f));
}
namespace {
auto GetDxVector(illuminate::float3 v) {
  DirectX::XMFLOAT3 f3(v);
  return XMLoadFloat3(&f3);
}
}
TEST_CASE("math/lookat") { // NOLINT
  using namespace illuminate; // NOLINT
  using namespace DirectX;
  matrix m{};
  float3 at{0.5f, 1.2f, -3.0f}, eye{2.0f, 1.3f, 1.2f,}, up{0.0f, 1.0f, 0.0f,};
  GetLookAtLH(eye, at, up, m);
  auto lookat = XMMatrixLookAtLH(GetDxVector(eye), GetDxVector(at), GetDxVector(up));
  for (uint32_t i = 0; i < 4; i++) {
    CAPTURE(i);
    for (uint32_t j = 0; j < 4; j++) {
      CAPTURE(j);
      CHECK_EQ(m[i][j], doctest::Approx(XMVectorGetByIndex(lookat.r[i], j)));
    }
  }
}
