#ifndef ILLUMINATE_D3D12_COMMAND_QUEUE_H
#define ILLUMINATE_D3D12_COMMAND_QUEUE_H
#include "d3d12_header_common.h"
namespace illuminate {
D3d12CommandQueue* CreateCommandQueue(D3d12Device* const device,
                                      const D3D12_COMMAND_LIST_TYPE type,
                                      const D3D12_COMMAND_QUEUE_PRIORITY priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                      const D3D12_COMMAND_QUEUE_FLAGS flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                      const UINT node_mask = 0);
class CommandQueueList {
 public:
  void Init(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list);
  void Term();
  constexpr auto Num() { return command_queue_num_; }
  constexpr auto Get(const uint32_t index) { return command_queue_list_[index]; }
  constexpr auto GetList() { return command_queue_list_; }
 private:
  uint32_t command_queue_num_{0};
  D3d12CommandQueue** command_queue_list_{nullptr};
};
class CommandQueueSignals {
 public:
  static const uint64_t kInvalidSignalVal = ~0UL;
  bool Init(D3d12Device* const device, const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list);
  void Term();
  uint64_t SetSignalVal(const uint32_t producer_queue_index, const uint64_t signal_val); // returns kInvalidSignalVal on failure.
  uint64_t SucceedSignal(const uint32_t producer_queue_index); // returns kInvalidSignalVal on failure.
  bool RegisterWaitOnCommandQueue(const uint32_t producer_queue_index, const uint32_t consumer_queue_index, const uint64_t wait_signal_val);
  bool WaitOnCpu(D3d12Device* const device, const uint64_t* signal_val_list);
  bool WaitAll(D3d12Device* const device);
 private:
  uint32_t command_queue_num_{0};
  D3d12CommandQueue** command_queue_list_{nullptr};
  D3d12Fence** fence_{nullptr};
  uint64_t* used_signal_val_list_{nullptr};
  HANDLE handle_{};
};
}
#endif
