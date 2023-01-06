#ifndef ILLUMINATE_D3D12_COMMAND_LIST_H
#define ILLUMINATE_D3D12_COMMAND_LIST_H
#include "d3d12_header_common.h"
#include "d3d12_command_queue.h"
namespace illuminate {
class CommandAllocatorPool {
 public:
  void Init(const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type);
  void Term();
  void SucceedFrame();
  D3d12CommandAllocator* RetainCommandAllocator(D3d12Device* device, const D3D12_COMMAND_LIST_TYPE);
 private:
  uint32_t allocator_pool_size_[kCommandQueueTypeNum]{};
  D3d12CommandAllocator** allocator_pool_[kCommandQueueTypeNum]{};
  uint32_t allocator_num_per_frame_{0};
  uint32_t frame_buffer_num_{0};
  uint32_t frame_index_{0};
  uint32_t* used_allocator_num_per_frame_{nullptr};
  D3d12CommandAllocator*** allocator_in_use_{nullptr};
  D3D12_COMMAND_LIST_TYPE** allocator_type_in_use_{nullptr};
};
class CommandListPool {
 public:
  void Init(const uint32_t* command_list_num_per_queue_type, const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type);
  void Term();
  void SucceedFrame() { command_allocator_pool_.SucceedFrame(); }
  D3d12CommandList* RetainCommandList(D3d12Device* device, const D3D12_COMMAND_LIST_TYPE);
  void ReturnCommandList(const D3D12_COMMAND_LIST_TYPE, const uint32_t num, D3d12CommandList**);
 private:
  uint32_t command_list_num_per_queue_type_[kCommandQueueTypeNum]{};
  D3d12CommandList** command_list_pool_[kCommandQueueTypeNum]{};
  CommandAllocatorPool command_allocator_pool_;
  uint32_t created_num_{0};
};
class CommandListInUse {
 public:
  void Init(const uint32_t command_queue_num, const uint32_t* command_list_num_per_queue);
  void Term();
  void PushCommandList(const uint32_t command_queue_index, D3d12CommandList* command_list);
  void FreePushedCommandList(const uint32_t command_queue_index);
  constexpr auto GetPushedCommandListNum(const uint32_t command_queue_index) const { return pushed_command_list_num_[command_queue_index]; }
  constexpr auto GetPushedCommandList(const uint32_t command_queue_index) const { return pushed_command_list_[command_queue_index]; }
 private:
  uint32_t command_queue_num_{0};
  uint32_t* command_list_num_per_queue_{nullptr};
  uint32_t* pushed_command_list_num_{nullptr};
  D3d12CommandList*** pushed_command_list_{nullptr};
};
class CommandListSet {
 public:
  bool Init(D3d12Device* device, const uint32_t command_queue_num, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const D3D12_COMMAND_QUEUE_PRIORITY* command_queue_priority, const uint32_t* command_list_num_per_queue, const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type);
  void Term();
  constexpr auto GetCommandQueueList() { return command_queue_list_.GetList(); }
  constexpr auto GetCommandQueue(const uint32_t command_queue_index) { return command_queue_list_.Get(command_queue_index); }
  constexpr auto GetCommandQueueTypeList() const { return command_queue_type_; }
  constexpr auto GetCommandQueueType(const uint32_t i) const { return command_queue_type_[i]; }
  D3d12CommandList* GetCommandList(D3d12Device* device, const uint32_t command_queue_index);
  void ExecuteCommandList(const uint32_t command_queue_index);
  void SucceedFrame() { command_list_pool_.SucceedFrame(); }
 private:
  CommandQueueList command_queue_list_;
  CommandListPool command_list_pool_;
  CommandListInUse command_list_in_use_;
  D3D12_COMMAND_LIST_TYPE* command_queue_type_{nullptr};
};
}
#endif
