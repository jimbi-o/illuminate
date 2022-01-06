#ifndef ILLUMINATE_CORE_STRID_H
#define ILLUMINATE_CORE_STRID_H
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
#ifdef BUILD_WITH_TEST
const char* RegisterDebugString(const char* debug_str);
#endif
class StrId {
 public:
#ifdef BUILD_WITH_TEST
  StrId(const char* str);
#else
  template <size_t N>
  constexpr explicit StrId(const char (&str)[N])
      : hash_(CompileTimeStrHash(str))
  {}
#endif
#ifdef BUILD_WITH_TEST
  constexpr const char* GetDebugString() const { return debug_string_hash_map_.Get(hash_); }
#endif
 private:
#ifdef BUILD_WITH_TEST
  class HashMap {
   public:
    void Insert(const StrHash& hash, const char* str) {
      auto index = hash % kEntityNum;
      if (table_[index] == nullptr) {
        table_[index] = RegisterDebugString(str);
      }
    }
    constexpr const char* Get(const StrHash& hash) const {
      return table_[hash % kEntityNum];
    }
   private:
    static const uint32_t kEntityNum = 256;
    const char* table_[kEntityNum]{};
  };
  static HashMap debug_string_hash_map_;
#endif
  StrHash hash_;
};
}
#define SID CompileTimeStrHash
#endif
