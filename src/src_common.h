#ifndef ILLUMINATE_SRC_COMMON_H
#define ILLUMINATE_SRC_COMMON_H
#include <cassert>
#include "illuminate/illuminate.h"
namespace illuminate {
template <typename N>
uint32_t GetUint32(const N& n) {
  return static_cast<uint32_t>(n);
}
uint32_t FindHashIndex(const uint32_t num, const StrHash* hash_list, const StrHash& hash);
}
#endif
