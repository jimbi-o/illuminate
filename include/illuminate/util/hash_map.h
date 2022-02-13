#ifndef ILLUMINATE_UTIL_HASH_MAP_H
#define ILLUMINATE_UTIL_HASH_MAP_H
#include "illuminate/core/strid.h"
namespace illuminate {
template <typename T, typename A>
class HashMap {
 public:
  static const uint32_t kDefaultTableSize = 32;
  HashMap() {}
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
  void SetAllocator(A* allocator, const uint32_t table_size = kDefaultTableSize) {
    allocator_ = allocator;
    table_size_ = table_size;
    table_ = AllocateArray<T*>(allocator_, table_size_);
    for (uint32_t i = 0; i < table_size_; i++) {
      table_[i] = nullptr;
    }
  }
  constexpr const uint32_t GetIndex(const StrHash key) const { return key % table_size_; }
  const T* Get(const StrHash key) const { return table_[GetIndex(key)]; }
  T* Get(const StrHash key) { return table_[GetIndex(key)]; }
  bool Insert(const StrHash key, T&& val) {
    auto index = GetIndex(key);
    if (table_[index] != nullptr) { return false; }
    table_[index] = Allocate<T>(allocator_);
    (*table_[index]) = std::move(val);
    return true;
  }
  bool InsertCopy(const StrHash key, const T& val) {
    auto v = val;
    return Insert(key, std::move(v));
  }
  void Reserve(const StrHash key) {
    table_[GetIndex(key)] = Allocate<T>(allocator_);
  }
  void Replace(const StrHash key, T&& val) {
    *(table_[GetIndex(key)]) = std::move(val);
  }
 private:
  A* allocator_{nullptr};
  uint32_t table_size_{0};
  T** table_{nullptr};
};
}
#endif
