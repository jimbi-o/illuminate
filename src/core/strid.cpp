#include "illuminate/core/strid.h"
#include <cstring>
#include <string>
namespace illuminate {
StrHash CalcStrHash(const char* const str, const StrHash prime) {
  if (str == nullptr) { return 0U; }
  StrHash hash = 0;
  for (uint32_t i = 0; str[i] != 0; i++) {
    hash = prime * hash + static_cast<StrHash>(str[i]);
  }
  return hash;
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("strhash") {
  using namespace illuminate;
  auto hash = CalcStrHash("str", kStrHashPrime);
  CHECK_NE(hash, 0);
  auto a = CalcStrHash("a");
  switch (a) {
    case SID("a"):
      CHECK_UNARY(true);
      break;
    case SID("b"):
      CHECK_UNARY(false);
      break;
    default:
      CHECK_UNARY(false);
      break;
  }
}
