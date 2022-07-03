#ifndef ILLUMINATE_MEMORY_ALLOCATION_H
#define ILLUMINATE_MEMORY_ALLOCATION_H
#include <cstdint>
#include <cstddef>
#include <memory>
namespace illuminate {
template <typename T>
constexpr auto SucceedPtrWithStructSize(const void* src) {
  return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(src) + sizeof(T));
}
inline auto SucceedPtr(const void* src, const uint32_t size_in_byte) {
  return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(src) + size_in_byte);
}
static const size_t kDefaultAlignmentSize = 8;
constexpr inline auto AlignAddress(const std::uintptr_t addr, const size_t align/*power of 2*/) {
  const auto mask = align - 1;
  return (addr + mask) & ~mask;
}
constexpr inline auto AlignAddress(const uint32_t addr, const uint32_t align/*power of 2*/) {
  const auto mask = align - 1;
  return (addr + mask) & ~mask;
}
class LinearAllocator {
 public:
  explicit LinearAllocator(const std::byte* buffer, const uint32_t size_in_byte) : head_(reinterpret_cast<uintptr_t>(buffer)), size_in_byte_(size_in_byte), offset_in_byte_(0) {}
  ~LinearAllocator() {}
  LinearAllocator() = delete;
  LinearAllocator(const LinearAllocator&) = delete;
  LinearAllocator& operator=(const LinearAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    auto addr_aligned = AlignAddress(head_ + offset_in_byte_, alignment_in_bytes);
    offset_in_byte_ = addr_aligned - head_ + bytes;
    if (offset_in_byte_ > size_in_byte_) return nullptr;
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffset() const { return offset_in_byte_; }
  constexpr void Reset() { offset_in_byte_ = 0; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
  auto GetBuffer() const { return reinterpret_cast<std::byte*>(head_); }
 private:
  const std::uintptr_t head_;
  const size_t size_in_byte_;
  size_t offset_in_byte_;
};
class DoubleBufferedAllocator {
 public:
  explicit DoubleBufferedAllocator(const std::byte* buffer1, const std::byte* buffer2, const uint32_t size_in_byte) : head_{reinterpret_cast<uintptr_t>(buffer1), reinterpret_cast<uintptr_t>(buffer2)}, size_in_byte_(size_in_byte), offset_in_byte_(0), head_index_(0) {}
  ~DoubleBufferedAllocator() {}
  DoubleBufferedAllocator() = delete;
  DoubleBufferedAllocator(const DoubleBufferedAllocator&) = delete;
  DoubleBufferedAllocator& operator=(const DoubleBufferedAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
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
class DoubleEndedLinearAllocator {
 public:
  explicit DoubleEndedLinearAllocator(const std::byte* buffer, const uint32_t size_in_byte)
      : head_(reinterpret_cast<uintptr_t>(buffer))
      , size_in_byte_(size_in_byte)
      , offset_in_byte_lower_(0)
      , offset_in_byte_higher_(0) {}
  ~DoubleEndedLinearAllocator() {}
  inline void* AllocateLower(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    auto addr_aligned = AlignAddress(head_ + offset_in_byte_lower_, alignment_in_bytes);
    auto offset = addr_aligned - head_ + bytes;
    if (offset + offset_in_byte_higher_ > size_in_byte_) return nullptr;
    offset_in_byte_lower_ = offset;
    return reinterpret_cast<void*>(addr_aligned);
  }
  inline void* AllocateHigher(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    auto addr_aligned = AlignAddress(head_ + size_in_byte_ - offset_in_byte_higher_ - bytes - kDefaultAlignmentSize, alignment_in_bytes);
    if (addr_aligned < head_ + offset_in_byte_lower_) { return nullptr; }
    offset_in_byte_higher_ = size_in_byte_ - (addr_aligned - head_);
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffsetLower() const { return offset_in_byte_lower_; }
  constexpr auto GetOffsetHigher() const { return offset_in_byte_higher_; }
  constexpr void ResetLower() { offset_in_byte_lower_ = 0; }
  constexpr void ResetHigher() { offset_in_byte_higher_ = 0; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
  auto GetBuffer() const { return reinterpret_cast<std::byte*>(head_); }
 private:
  DoubleEndedLinearAllocator(const DoubleEndedLinearAllocator&) = delete;
  DoubleEndedLinearAllocator& operator=(const DoubleEndedLinearAllocator&) = delete;
  const std::uintptr_t head_;
  const size_t size_in_byte_;
  size_t offset_in_byte_lower_;
  size_t offset_in_byte_higher_;
};
class StackAllocator {
 public:
  explicit StackAllocator(const std::byte* buffer, const uint32_t size_in_byte) : head_(reinterpret_cast<uintptr_t>(buffer)), size_in_byte_(size_in_byte), offset_in_byte_(0) {}
  ~StackAllocator() {}
  StackAllocator(const StackAllocator&) = delete;
  StackAllocator& operator=(const StackAllocator&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
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
class DoubleEndedStackAllocator {
 public:
  explicit DoubleEndedStackAllocator(const std::byte* buffer, const uint32_t size_in_byte)
      : head_(reinterpret_cast<uintptr_t>(buffer))
      , size_in_byte_(size_in_byte)
      , offset_in_byte_lower_(0)
      , offset_in_byte_higher_(0) {}
  ~DoubleEndedStackAllocator() {}
  inline void* AllocateLower(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    auto addr_aligned = AlignAddress(head_ + offset_in_byte_lower_, alignment_in_bytes);
    auto offset = addr_aligned - head_ + bytes;
    if (offset + offset_in_byte_higher_ > size_in_byte_) return nullptr;
    offset_in_byte_lower_ = offset;
    return reinterpret_cast<void*>(addr_aligned);
  }
  inline void* AllocateHigher(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    auto addr_aligned = AlignAddress(head_ + size_in_byte_ - offset_in_byte_higher_ - bytes - kDefaultAlignmentSize, alignment_in_bytes);
    if (addr_aligned < head_ + offset_in_byte_lower_) { return nullptr; }
    offset_in_byte_higher_ = size_in_byte_ - (addr_aligned - head_);
    return reinterpret_cast<void*>(addr_aligned);
  }
  constexpr auto GetOffsetLower() const { return offset_in_byte_lower_; }
  constexpr auto GetOffsetHigher() const { return offset_in_byte_higher_; }
  constexpr void ResetLowerToMarker(const uintptr_t marker) { offset_in_byte_lower_ = marker; }
  constexpr void ResetHigherToMarker(const uintptr_t marker) { offset_in_byte_higher_ = marker; }
  constexpr auto GetBufferSizeInByte() const { return size_in_byte_; }
 private:
  DoubleEndedStackAllocator(const DoubleEndedStackAllocator&) = delete;
  DoubleEndedStackAllocator& operator=(const DoubleEndedStackAllocator&) = delete;
  const std::uintptr_t head_;
  const size_t size_in_byte_;
  size_t offset_in_byte_lower_;
  size_t offset_in_byte_higher_;
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
  MemoryAllocationJanitor() = delete;
  MemoryAllocationJanitor(const MemoryAllocationJanitor&) = delete;
  MemoryAllocationJanitor& operator=(const MemoryAllocationJanitor&) = delete;
  inline void* Allocate(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) { return allocator_->Allocate(bytes, alignment_in_bytes); }
  constexpr auto GetMarker() const { return marker_; }
 private:
  StackAllocator* allocator_{};
  std::uintptr_t marker_{};
};
template <typename A, size_t N>
class TemporalAllocator {
 public:
  TemporalAllocator() : allocator_(buffer_, N) {}
  void* Allocate(size_t bytes, size_t alignment_in_bytes = kDefaultAlignmentSize) {
    return allocator_.Allocate(bytes, alignment_in_bytes);
  }
  constexpr auto GetOffset() const { return allocator_.GetOffset(); }
 private:
  A allocator_;
  std::byte buffer_[N];
};
template <typename T, typename A>
auto Allocate(A* allocator, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(allocator->Allocate(sizeof(T), alignment_in_bytes)) T;
}
template <typename T, typename A>
auto AllocateArray(A* allocator, const uint32_t len, const size_t alignment_in_bytes = kDefaultAlignmentSize) {
  return new(allocator->Allocate(sizeof(T) * len, alignment_in_bytes)) T[len];
}
}
#endif
