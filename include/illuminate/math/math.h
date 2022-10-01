#ifndef ILLUMINATE_MATH_H
#define ILLUMINATE_MATH_H
#include <math.h>
#include "gfxminimath/gfxminimath.h"
namespace illuminate {
static const auto PI = static_cast<float>(M_PI);
static const auto kDegreesToRadian = PI / 180.0f;
static const auto kRadianToDegrees = 1.0f / kDegreesToRadian;
}
#endif
