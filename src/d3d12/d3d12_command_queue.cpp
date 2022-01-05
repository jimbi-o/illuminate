#include "d3d12_command_queue.h"
#include "d3d12_src_common.h"
namespace illuminate {
D3d12CommandQueue* CreateCommandQueue(D3d12Device* const device, const D3D12_COMMAND_LIST_TYPE type, const D3D12_COMMAND_QUEUE_PRIORITY priority, const D3D12_COMMAND_QUEUE_FLAGS flags, const UINT node_mask) {
  D3D12_COMMAND_QUEUE_DESC desc = { .Type = type, .Priority = priority, .Flags = flags, .NodeMask = node_mask, };
  D3d12CommandQueue* command_queue = nullptr;
  auto hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    logerror("CreateCommandQueue failed. {} {} {} {}", type, priority, flags, node_mask);
  }
  return command_queue;
}
void CommandQueueList::Init(const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list) {
  command_queue_num_ = command_queue_num;
  command_queue_list_ = AllocateArray<D3d12CommandQueue*>(gSystemMemoryAllocator, command_queue_num);
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    command_queue_list_[i] = command_queue_list[i];
  }
}
void CommandQueueList::Term() {
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    if (command_queue_list_[i]) {
      command_queue_list_[i]->Release();
      command_queue_list_[i] = nullptr;
    }
  }
  command_queue_num_ = 0;
  command_queue_list_ = nullptr;
}
namespace {
HANDLE CreateEventHandle() {
  auto handle = CreateEvent(nullptr, false, false, nullptr);
  if (handle == nullptr) {
    logwarn("CreateEvent failed. {}", GetLastError());
  }
  return handle;
}
bool CloseHandle(HANDLE handle) {
  if (::CloseHandle(handle)) return true;
  logwarn("CloseHandle failed. {}", GetLastError());
  return false;
}
} // anonymous namespace
bool CommandQueueSignals::Init(D3d12Device* const device, const uint32_t command_queue_num, D3d12CommandQueue** command_queue_list) {
  command_queue_num_ = command_queue_num;
  command_queue_list_ = command_queue_list;
  fence_ = AllocateArray<D3d12Fence*>(gSystemMemoryAllocator, command_queue_num);
  used_signal_val_list_ = AllocateArray<uint64_t>(gSystemMemoryAllocator, command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    used_signal_val_list_[i] = 0UL;
    ID3D12Fence* fence = nullptr;
    auto hr = device->CreateFence(used_signal_val_list_[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) {
      logerror("CreateFence failed. {} {}", i, used_signal_val_list_[i]);
      assert(false && "CreateFence failed.");
      return false;
    }
    hr = fence->QueryInterface(IID_PPV_ARGS(&fence_[i]));
    if (FAILED(hr)) {
      logerror("fence->QueryInterface failed. {} {}", i, used_signal_val_list_[i]);
      assert(false && "fence->QueryInterface failed.");
      return false;
    }
    fence->Release();
  }
  handle_ = CreateEventHandle();
  if (handle_ == nullptr) {
    return false;
  }
  return true;
}
void CommandQueueSignals::Term() {
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    fence_[i]->Release();
  }
  used_signal_val_list_ = nullptr;
  if (handle_) {
    CloseHandle(handle_);
    handle_ = nullptr;
  }
}
uint64_t CommandQueueSignals::SetSignalVal(const uint32_t producer_queue_index, const uint64_t signal_val) {
  auto hr = command_queue_list_[producer_queue_index]->Signal(fence_[producer_queue_index], signal_val);
  if (FAILED(hr)) {
    logerror("CommandQueue signal failed. {} {}", producer_queue_index, signal_val);
    assert(false && "CommandQueue signal failed");
    return kInvalidSignalVal;
  }
  used_signal_val_list_[producer_queue_index] = signal_val;
  return signal_val;
}
uint64_t CommandQueueSignals::SucceedSignal(const uint32_t producer_queue_index) {
  return SetSignalVal(producer_queue_index, used_signal_val_list_[producer_queue_index] + 1);
}
bool CommandQueueSignals::RegisterWaitOnCommandQueue(const uint32_t producer_queue_index, const uint32_t consumer_queue_index, const uint64_t wait_signal_val) {
  if (producer_queue_index == consumer_queue_index) {
    logwarn("same command queue selected. {} {} {}", producer_queue_index, consumer_queue_index, wait_signal_val);
    return false;
  }
  auto hr = command_queue_list_[consumer_queue_index]->Wait(fence_[producer_queue_index], wait_signal_val);
  if (FAILED(hr)) {
    logerror("command queue wait failed. {} {} {}", producer_queue_index, consumer_queue_index, wait_signal_val);
    return false;
  }
  return true;
}
bool CommandQueueSignals::WaitOnCpu(D3d12Device* const device, const uint64_t* signal_val_list) {
  uint32_t wait_queue_num = 0;
  auto allocator = GetTemporalMemoryAllocator();
  auto wait_queue_index = AllocateArray<uint32_t>(&allocator, command_queue_num_);
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    auto comp_val = fence_[i]->GetCompletedValue();
    if (comp_val >= signal_val_list[i]) {
      continue;
    }
    wait_queue_index[wait_queue_num] = i;
    wait_queue_num++;
  }
  if (wait_queue_num == 0) { return true; }
  if (wait_queue_num == 1) {
    auto hr = fence_[wait_queue_index[0]]->SetEventOnCompletion(signal_val_list[wait_queue_index[0]], handle_);
    if (FAILED(hr)) {
      logerror("SetEventOnCompletion failed. {} {} {}", hr, wait_queue_index[0], signal_val_list[wait_queue_index[0]]);
      assert(false && "SetEventOnCompletion failed");
      return false;
    }
  } else {
    auto waiting_queue_fences = AllocateArray<ID3D12Fence*>(&allocator, wait_queue_num);
    auto waiting_signals = AllocateArray<uint64_t>(&allocator, wait_queue_num);
    for (uint32_t i = 0; i < wait_queue_num; i++) {
      waiting_queue_fences[i] = fence_[wait_queue_index[i]];
      waiting_signals[i] = signal_val_list[wait_queue_index[i]];
    }
    auto hr = device->SetEventOnMultipleFenceCompletion(waiting_queue_fences, waiting_signals, wait_queue_num, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, handle_);
    if (FAILED(hr)) {
      logerror("SetEventOnMultipleFenceCompletion failed. {} {}", hr, wait_queue_num);
      for (uint32_t i = 0; i < command_queue_num_; i++) {
        logerror("command queue {} {} {}", i, signal_val_list[i], wait_queue_index[i]);
      }
      assert(false && "SetEventOnMultipleFenceCompletion failed");
      return false;
    }
  }
  auto hr = WaitForSingleObject(handle_, INFINITE);
  if (FAILED(hr)) {
    logerror("WaitForSingleObject failed. {} {}", hr, wait_queue_num);
    return false;
  }
  return true;
}
bool CommandQueueSignals::WaitAll(D3d12Device* const device) {
  auto allocator = GetTemporalMemoryAllocator();
  auto signal_val_list = AllocateArray<uint64_t>(&allocator, command_queue_num_);
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    signal_val_list[i] = SucceedSignal(i);
  }
  if (!WaitOnCpu(device, signal_val_list)) { return false; }
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    if (SetSignalVal(i, 0UL) == kInvalidSignalVal) { return false; }
  }
  return true;
}
} // namespace illuminate
#ifdef BUILD_WITH_TEST
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
TEST_CASE("command queue") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK(device.Init(dxgi_core.GetAdapter())); // NOLINT
  auto command_queue = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
  CHECK(command_queue);
  CHECK(command_queue->Release() == 0);
  command_queue = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
  CHECK(command_queue);
  CHECK(command_queue->Release() == 0);
  command_queue = CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
  CHECK(command_queue);
  CHECK(command_queue->Release() == 0);
  device.Term();
  dxgi_core.Term();
}
TEST_CASE("class CommandQueueList") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK(device.Init(dxgi_core.GetAdapter())); // NOLINT
  const uint32_t command_queue_num = 3;
  D3d12CommandQueue* raw_command_queue_list[] = {
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT),
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE),
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COPY),
  };
  CommandQueueList command_queue_list;
  command_queue_list.Init(command_queue_num, raw_command_queue_list);
  CHECK_EQ(command_queue_list.Num(), 3);
  CHECK_EQ(command_queue_list.Get(0), raw_command_queue_list[0]);
  CHECK_EQ(command_queue_list.Get(1), raw_command_queue_list[1]);
  CHECK_EQ(command_queue_list.Get(2), raw_command_queue_list[2]);
  command_queue_list.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
TEST_CASE("class CommandQueueSignals") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK(device.Init(dxgi_core.GetAdapter())); // NOLINT
  const uint32_t command_queue_num = 3;
  D3d12CommandQueue* command_queue_list[] = {
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT),
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE),
    CreateCommandQueue(device.Get(), D3D12_COMMAND_LIST_TYPE_COPY),
  };
  CommandQueueSignals command_queue_signals;
  CHECK_UNARY(command_queue_signals.Init(device.Get(), command_queue_num, command_queue_list));
  const uint32_t producer_queue_index = 0U;
  const uint32_t consumer_queue_index = 1U;
  CHECK_EQ(command_queue_signals.SucceedSignal(producer_queue_index), 1UL);
  CHECK_FALSE(command_queue_signals.RegisterWaitOnCommandQueue(producer_queue_index, producer_queue_index, 1UL));
  CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(producer_queue_index, consumer_queue_index, 1UL));
  uint64_t signal_val_list[] = {1UL, 0UL, 0UL};
  CHECK_UNARY(command_queue_signals.WaitOnCpu(device.Get(), signal_val_list));
  signal_val_list[0] = command_queue_signals.SucceedSignal(0);
  signal_val_list[1] = command_queue_signals.SucceedSignal(1);
  CHECK_UNARY(command_queue_signals.WaitOnCpu(device.Get(), signal_val_list));
  CHECK_UNARY(command_queue_signals.WaitAll(device.Get()));
  CHECK_EQ(command_queue_signals.SucceedSignal(producer_queue_index), 1UL);
  CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(producer_queue_index, consumer_queue_index, 1UL));
  signal_val_list[0] = 1UL;
  signal_val_list[1] = 0UL;
  CHECK_UNARY(command_queue_signals.WaitOnCpu(device.Get(), signal_val_list));
  CHECK_UNARY(command_queue_signals.WaitAll(device.Get()));
  for (uint32_t i = 0; i < command_queue_num; i++) {
    command_queue_list[i]->Release();
  }
  command_queue_signals.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
#endif
