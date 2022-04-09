#include "src_common.h"
#include <algorithm>
namespace illuminate {
uint32_t FindHashIndex(const uint32_t num, const StrHash* hash_list, const StrHash& hash) {
  auto it = std::find(hash_list, hash_list + num, hash);
  return GetUint32(std::distance(hash_list, it));
}
} // namespace illuminate
