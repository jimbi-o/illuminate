#ifndef ILLUMINATE_UTIL_DEFINES_H
#define ILLUMINATE_UTIL_DEFINES_H
namespace illuminate {
template <typename T>
struct ArrayOf {
  uint32_t size;
  T* array;
};
template <typename T>
auto CreateArray(const uint32_t size, T* ptr) {
  return ArrayOf<T>{size, ptr};
}
}
#endif
