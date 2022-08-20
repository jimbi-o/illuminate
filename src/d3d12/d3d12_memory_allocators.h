#ifndef ILLUMINATE_D3D12_MEMORY_ALLOCATOR_H
#define ILLUMINATE_D3D12_MEMORY_ALLOCATOR_H
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
enum class MemoryType : uint8_t { kSystem, kScene, kFrame, };
void* AllocateSystem(const size_t bytes, const size_t alignment_in_bytes = kDefaultAlignmentSize);
void* AllocateScene(const size_t bytes, const size_t alignment_in_bytes = kDefaultAlignmentSize);
void* AllocateFrame(const size_t bytes, const size_t alignment_in_bytes = kDefaultAlignmentSize);
inline auto Allocate(const MemoryType type, const size_t bytes, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  switch (type) {
    case MemoryType::kSystem: return AllocateSystem(bytes, alignment_in_bytes);
    case MemoryType::kScene:  return AllocateScene(bytes, alignment_in_bytes);
    case MemoryType::kFrame:  return AllocateFrame(bytes, alignment_in_bytes);
  }
  return (void*)nullptr;
}
template <typename T>
auto AllocateSystem(const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateSystem(sizeof(T), alignment_in_bytes)) T;
}
template <typename T>
auto AllocateArraySystem(const uint32_t len, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateSystem(sizeof(T) * len, alignment_in_bytes)) T[len];
}
template <typename T>
auto AllocateScene(const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateScene(sizeof(T), alignment_in_bytes)) T;
}
template <typename T>
auto AllocateArrayScene(const uint32_t len, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateScene(sizeof(T) * len, alignment_in_bytes)) T[len];
}
template <typename T>
auto AllocateFrame(const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateFrame(sizeof(T), alignment_in_bytes)) T;
}
template <typename T>
auto AllocateArrayFrame(const uint32_t len, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(AllocateFrame(sizeof(T) * len, alignment_in_bytes)) T[len];
}
template <typename T>
auto Allocate(const MemoryType type, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(Allocate(type, sizeof(T), alignment_in_bytes)) T;
}
template <typename T>
auto AllocateArray(const MemoryType type, const uint32_t len, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(Allocate(type, sizeof(T) * len, alignment_in_bytes)) T[len];
}
template <typename T>
auto AllocateAndFillArraySystem(const uint32_t len, const T& fill_value, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  auto ptr = AllocateArraySystem<T>(len, alignment_in_bytes);
  std::fill(ptr, ptr + len, fill_value);
  return ptr;
}
template <typename T>
auto AllocateAndFillArrayScene(const uint32_t len, const T& fill_value, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  auto ptr = AllocateArrayScene<T>(len, alignment_in_bytes);
  std::fill(ptr, ptr + len, fill_value);
  return ptr;
}
template <typename T>
auto AllocateAndFillArrayFrame(const uint32_t len, const T& fill_value, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  auto ptr = AllocateArrayFrame<T>(len, alignment_in_bytes);
  std::fill(ptr, ptr + len, fill_value);
  return ptr;
}
void ResetAllocation(const MemoryType type);
void ClearAllAllocations();
}
#endif
