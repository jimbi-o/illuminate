#include "illuminate/core/strid.h"
namespace illuminate {
StrHash CalcStrHash(const char* const str, const StrHash prime) {
  if (str == nullptr) { return 0U; }
  StrHash hash = 0;
  for (uint32_t i = 0; str[i] != 0; i++) {
    hash = prime * hash + static_cast<StrHash>(str[i]);
  }
  return hash;
}
#ifdef BUILD_WITH_TEST
static const uint32_t debug_string_buffer_size_in_bytes{1024};
static char debug_string_buffer[debug_string_buffer_size_in_bytes];
static uint32_t debug_string_index{0};
const char* RegisterDebugString(const char* debug_str) {
  const auto len = static_cast<uint32_t>(strlen(debug_str)) + 1;
  if (debug_string_index + len >= debug_string_buffer_size_in_bytes) { debug_string_index = 0; }
  auto dst = &debug_string_buffer[debug_string_index];
  strcpy_s(dst, len, debug_str);
  debug_string_index += len;
  return dst;
}
StrId::StrId(const char* str) : hash_(CalcStrHash(str, kStrHashPrime)) {
  debug_string_hash_map_.Insert(hash_, str);
}
StrId::HashMap StrId::debug_string_hash_map_;
#endif
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("strhash") {
  using namespace illuminate;
  auto hash = CalcStrHash("str", kStrHashPrime);
  CHECK_NE(hash, 0);
  StrId sid("str");
  StrId sid2("str2");
  StrId sid3("str3");
#ifdef BUILD_WITH_TEST
  CHECK_EQ(std::string_view("str"), std::string_view(sid.GetDebugString()));
  CHECK_EQ(std::string_view("str2"), std::string_view(sid2.GetDebugString()));
  CHECK_EQ(std::string_view("str3"), std::string_view(sid3.GetDebugString()));
#endif
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
