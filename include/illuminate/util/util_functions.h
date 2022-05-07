#ifndef ILLUMINATE_UTIL_UTIL_FUNCTIONS_H
#define ILLUMINATE_UTIL_UTIL_FUNCTIONS_H
namespace illuminate {
bool SucceedMultipleIndex(const uint32_t array_size, const uint32_t* array_size_array, uint32_t* array);
void SetArrayValues(const uint32_t val, const uint32_t array_size, uint32_t* array);
}
#endif
