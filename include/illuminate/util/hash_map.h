#ifndef ILLUMINATE_UTIL_HASH_MAP_H
#define ILLUMINATE_UTIL_HASH_MAP_H
#include "illuminate/core/strid.h"
namespace illuminate {
template <typename T, typename A>
class HashMap {
 public:
  static const uint32_t kDefaultTableSize = 32;
  HashMap(A* allocator, const uint32_t table_size = kDefaultTableSize)
      : allocator_(allocator)
      , table_size_(table_size)
      , table_(AllocateArray<T*>(allocator_, table_size_))
  {
    for (uint32_t i = 0; i < table_size_; i++) {
      table_[i] = nullptr;
    }
  }
  virtual ~HashMap() {}
  constexpr const uint32_t GetIndex(const StrHash key) const { return key % table_size_; }
  const T* Get(const StrHash key) const { return table_[GetIndex(key)]; }
  T* Get(const StrHash key) { return table_[GetIndex(key)]; }
  bool Insert(const StrHash key, T&& val) {
    auto index = GetIndex(key);
    if (table_[index] != nullptr) return false;
    table_[index] = Allocate<T>(allocator_);
    (*table_[index]) = std::move(val);
    return true;
  }
 private:
  A* allocator_;
  uint32_t table_size_;
  T** table_;
};
}
#endif
