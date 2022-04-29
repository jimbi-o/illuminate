#ifndef ILLUMINATE_CORE_STRID_H
#define ILLUMINATE_CORE_STRID_H
#include <type_traits>
namespace illuminate {
using StrHash = uint32_t;
static const StrHash kStrHashPrime = 31;
// https://xueyouchao.github.io/2016/11/16/CompileTimeString/
template <size_t N>
constexpr inline StrHash CompileTimeStrHash(const char (&str)[N], const StrHash prime = kStrHashPrime, const size_t len = N-1)
{
  return (len <= 1) ? static_cast<std::make_unsigned_t<char>>(str[0]) : (prime * CompileTimeStrHash(str, prime, len-1) + static_cast<std::make_unsigned_t<char>>(str[len-1]));
}
StrHash CalcStrHash(const char* const str, const StrHash prime = kStrHashPrime);
StrHash CombineHash(const StrHash& a, const StrHash& b);
}
#define SID CompileTimeStrHash
#endif
