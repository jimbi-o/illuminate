#include "illuminate/core/strid.h"
#include <cstring>
#include <string>
#include "../src_common.h"
namespace illuminate {
StrHash CalcStrHash(const char* const str, const StrHash prime) {
  if (str == nullptr) { return 0U; }
  StrHash hash = 0;
  for (uint32_t i = 0; str[i] != 0; i++) {
    hash = prime * hash + static_cast<StrHash>(str[i]);
  }
  spdlog::trace("SID {}:{}", str, hash);
  return hash;
}
StrHash CombineHash(const StrHash& a, const StrHash& b) {
  // https://www.boost.org/doc/libs/1_55_0/doc/html/hash/reference.html#boost.hash_combine
  StrHash seed{a};
  seed ^= b + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
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
