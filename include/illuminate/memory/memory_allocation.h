#ifndef ILLUMINATE_MEMORY_ALLOCATION_H
#define ILLUMINATE_MEMORY_ALLOCATION_H
#include <cstdint>
#include <cstddef>
#include <memory>
namespace illuminate {
constexpr inline auto AlignAddress(const std::uintptr_t addr, const size_t align/*power of 2*/) {
  const auto mask = align - 1;
  return (addr + mask) & ~mask;
}
class LinearAllocator {
 public:
  explicit LinearAllocator(const std::byte* buffer, const uint32_t size_in_byte) : head_(reinterpret_cast<uintptr_t>(buffer)), size_in_byte_(size_in_byte), offset_in_byte_(0) {}
  LinearAllocator() : head_(0), size_in_byte_(0), offset_in_byte_(0) {}
  ~LinearAllocator() {}
  LinearAllocator(const LinearAllocator&) = delete;
  LinearAllocator& operator=(const LinearAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = 8) {
    auto addr_aligned = AlignAddress(head_ + offset_in_byte_, alignment_in_bytes);
    offset_in_byte_ = addr_aligned - head_ + bytes;
    if (offset_in_byte_ > size_in_byte_) return nullptr;
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffset() const { return offset_in_byte_; }
  constexpr void Reset() { offset_in_byte_ = 0; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
 private:
  const std::uintptr_t head_;
  const size_t size_in_byte_;
  size_t offset_in_byte_;
};
class DoubleBufferedAllocator {
 public:
  explicit DoubleBufferedAllocator(const std::byte* buffer1, const std::byte* buffer2, const uint32_t size_in_byte) : head_{reinterpret_cast<uintptr_t>(buffer1), reinterpret_cast<uintptr_t>(buffer2)}, size_in_byte_(size_in_byte), offset_in_byte_(0), head_index_(0) {}
  DoubleBufferedAllocator() : head_{}, size_in_byte_(0), offset_in_byte_(0), head_index_(0) {}
  ~DoubleBufferedAllocator() {}
  DoubleBufferedAllocator(const DoubleBufferedAllocator&) = delete;
  DoubleBufferedAllocator& operator=(const DoubleBufferedAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = 8) {
    auto addr_aligned = AlignAddress(head_[head_index_] + offset_in_byte_, alignment_in_bytes);
    offset_in_byte_ = addr_aligned - head_[head_index_] + bytes;
    if (offset_in_byte_ > size_in_byte_) return nullptr;
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffset() const { return offset_in_byte_; }
  constexpr void Reset() { (head_index_ == 0) ? head_index_ = 1 : head_index_ = 0; offset_in_byte_ = 0; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
 private:
  const std::uintptr_t head_[2];
  const size_t size_in_byte_;
  size_t offset_in_byte_;
  uint32_t head_index_;
  [[maybe_unused]] std::byte _pad[4]{};
};
class StackAllocator {
 public:
  explicit StackAllocator(const std::byte* buffer, const uint32_t size_in_byte) : head_(reinterpret_cast<uintptr_t>(buffer)), size_in_byte_(size_in_byte), offset_in_byte_(0) {}
  StackAllocator() : head_(0), size_in_byte_(0), offset_in_byte_(0) {}
  ~StackAllocator() {}
  StackAllocator(const StackAllocator&) = delete;
  StackAllocator& operator=(const StackAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = 8) {
    auto addr_aligned = AlignAddress(head_ + offset_in_byte_, alignment_in_bytes);
    offset_in_byte_ = addr_aligned - head_ + bytes;
    if (offset_in_byte_ > size_in_byte_) return nullptr;
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffset() const { return offset_in_byte_; }
  constexpr void ResetToMarker(const uintptr_t marker) { offset_in_byte_ = marker; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
 private:
  const std::uintptr_t head_;
  const size_t size_in_byte_;
  size_t offset_in_byte_;
};
class MemoryAllocationJanitor {
 public:
  MemoryAllocationJanitor(StackAllocator* allocator)
      : allocator_(allocator)
      , marker_(allocator_->GetOffset())
  {}
  virtual ~MemoryAllocationJanitor() {
    allocator_->ResetToMarker(marker_);
  }
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = 8) { return allocator_->Allocate(bytes, alignment_in_bytes); }
  constexpr auto GetMarker() const { return marker_; }
 private:
  StackAllocator* allocator_{};
  std::uintptr_t marker_{};
};
template <typename T, typename A>
auto Allocate(A* allocator) {
  return new(allocator->Allocate(sizeof(T*))) T;
}
template <typename T, typename A>
auto AllocateArray(A* allocator, const uint32_t len) {
  return new(allocator->Allocate(sizeof(T) * len)) T[len];
}
}
#endif
