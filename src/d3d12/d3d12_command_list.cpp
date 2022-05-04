#include "d3d12_command_list.h"
#include "d3d12_src_common.h"
namespace illuminate {
void CommandAllocatorPool::Init(const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type) {
  allocator_num_per_frame_ = 0;
  for (uint32_t i = 0; i < kCommandQueueTypeNum; i++) {
    allocator_pool_size_[i] = command_allocator_num_per_queue_type[i];
    allocator_pool_[i] = AllocateArray<D3d12CommandAllocator*>(gSystemMemoryAllocator, allocator_pool_size_[i]);
    for (uint32_t j = 0; j < allocator_pool_size_[i]; j++) {
      allocator_pool_[i][j] = nullptr;
    }
    allocator_num_per_frame_ += allocator_pool_size_[i];
  }
  frame_buffer_num_ = frame_buffer_num;
  frame_index_ = 0;
  used_allocator_num_per_frame_ = AllocateArray<uint32_t>(gSystemMemoryAllocator, frame_buffer_num_);
  allocator_in_use_ = AllocateArray<D3d12CommandAllocator**>(gSystemMemoryAllocator, frame_buffer_num_);
  allocator_type_in_use_ = AllocateArray<D3D12_COMMAND_LIST_TYPE*>(gSystemMemoryAllocator, frame_buffer_num_);
  for (uint32_t i = 0; i < frame_buffer_num_; i++) {
    used_allocator_num_per_frame_[i] = 0;
    allocator_in_use_[i] = AllocateArray<D3d12CommandAllocator*>(gSystemMemoryAllocator, allocator_num_per_frame_);
    allocator_type_in_use_[i] = AllocateArray<D3D12_COMMAND_LIST_TYPE>(gSystemMemoryAllocator, allocator_num_per_frame_);
    for (uint32_t j = 0; j < allocator_num_per_frame_; j++) {
      allocator_in_use_[i][j] = nullptr;
    }
  }
}
void CommandAllocatorPool::Term() {
  for (uint32_t i = 0; i < kCommandQueueTypeNum; i++) {
    for (uint32_t j = 0; j < allocator_pool_size_[i]; j++) {
      if (allocator_pool_[i][j] != nullptr) {
        auto refval = allocator_pool_[i][j]->Release();
        if (refval != 0) {
          logwarn("allocator_pool_[{}][{}] still referenced.", i, j);
        }
        allocator_pool_[i][j] = nullptr;
      }
    }
  }
  for (uint32_t i = 0; i < frame_buffer_num_; i++) {
    for (uint32_t j = 0; j < used_allocator_num_per_frame_[i]; j++) {
      auto refval = allocator_in_use_[i][j]->Release();
      if (refval != 0) {
        logwarn("allocator_in_use_[{}][{}] still referenced.", i, j);
      }
    }
    used_allocator_num_per_frame_[i] = 0;
  }
}
void CommandAllocatorPool::SucceedFrame() {
  frame_index_++;
  if (frame_index_ >= frame_buffer_num_) { frame_index_ = 0; }
  for (uint32_t i = 0; i < used_allocator_num_per_frame_[frame_index_]; i++) {
    auto hr = allocator_in_use_[frame_index_][i]->Reset();
    if (FAILED(hr)) {
      logerror("command allocator reset failed. {} {} {}", hr, frame_index_, i);
      assert(false && "command allocator reset failed");
      auto refval = allocator_in_use_[frame_index_][i]->Release();
      if (refval != 0) {
        logwarn("allocator_in_use_[{}][{}] still referenced(1). {}", frame_index_, i, refval);
      }
      continue;
    }
    auto type_index = GetCommandQueueTypeIndex(allocator_type_in_use_[frame_index_][i]);
    bool pooled = false;
    for (uint32_t j = 0; j < allocator_pool_size_[type_index]; j++) {
      if (allocator_pool_[type_index][j] == nullptr) {
        allocator_pool_[type_index][j] = allocator_in_use_[frame_index_][i];
        pooled = true;
        break;
      }
    }
    if (!pooled) {
      logwarn("command allocator exceeded pool size. {} {} {} {}", frame_index_, i, type_index, allocator_pool_size_[type_index]);
      auto refval = allocator_in_use_[frame_index_][i]->Release();
      if (refval != 0) {
        logwarn("allocator_in_use_[{}][{}] still referenced(2). {}", frame_index_, i, refval);
      }
    }
  }
  used_allocator_num_per_frame_[frame_index_] = 0;
}
D3d12CommandAllocator* CommandAllocatorPool::RetainCommandAllocator(D3d12Device* device, const D3D12_COMMAND_LIST_TYPE type) {
  if (used_allocator_num_per_frame_[frame_index_] >= allocator_num_per_frame_) {
    logerror("used command allocator exceeds pool size. {} {} {} {}", allocator_num_per_frame_, used_allocator_num_per_frame_[frame_index_], frame_index_, type);
    assert(false);
  }
  D3d12CommandAllocator* allocator{nullptr};
  auto type_index = GetCommandQueueTypeIndex(type);
  for (uint32_t j = 0; j < allocator_pool_size_[type_index]; j++) {
    if (allocator_pool_[type_index][j] != nullptr) {
      std::swap(allocator, allocator_pool_[type_index][j]);
      break;
    }
  }
  if (allocator == nullptr) {
    auto hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator));
    if (FAILED(hr)) {
      logerror("CreateCommandAllocator failed. {} {}", hr, type);
      assert(false && "CreateCommandAllocator failed.");
      return nullptr;
    }
    SetD3d12Name(allocator, "allocator" + std::to_string(frame_index_) + "_" + std::to_string(used_allocator_num_per_frame_[frame_index_]));
  }
  allocator_in_use_[frame_index_][used_allocator_num_per_frame_[frame_index_]] = allocator;
  allocator_type_in_use_[frame_index_][used_allocator_num_per_frame_[frame_index_]] = type;
  used_allocator_num_per_frame_[frame_index_]++;
  return allocator;
}
void CommandListPool::Init(const uint32_t* command_list_num_per_queue_type, const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type) {
  for (uint32_t i = 0; i < kCommandQueueTypeNum; i++) {
    command_list_num_per_queue_type_[i] = command_list_num_per_queue_type[i];
    if (command_list_num_per_queue_type_[i] > 0) {
      command_list_pool_[i] = AllocateArray<D3d12CommandList*>(gSystemMemoryAllocator, command_list_num_per_queue_type_[i]);
      for (uint32_t j = 0; j < command_list_num_per_queue_type_[i]; j++) {
        command_list_pool_[i][j] = nullptr;
      }
    } else {
      command_list_pool_[i] = nullptr;
    }
  }
  command_allocator_pool_.Init(frame_buffer_num, command_allocator_num_per_queue_type);
  created_num_ = 0;
}
void CommandListPool::Term() {
  command_allocator_pool_.Term();
  for (uint32_t i = 0; i < kCommandQueueTypeNum; i++) {
    for (uint32_t j = 0; j < command_list_num_per_queue_type_[i]; j++) {
      if (command_list_pool_[i][j]) {
        auto val = command_list_pool_[i][j]->Release();
        if (val != 0) {
          logwarn("command_list_pool_[{}][{}] reference left. {}", i, j, val);
        }
      }
    }
  }
}
D3d12CommandList* CommandListPool::RetainCommandList(D3d12Device* device, const D3D12_COMMAND_LIST_TYPE type) {
  auto type_index = GetCommandQueueTypeIndex(type);
  for (uint32_t i = 0; i < command_list_num_per_queue_type_[type_index]; i++) {
    if (command_list_pool_[type_index][i] == nullptr) { continue; }
    auto command_list = command_list_pool_[type_index][i];
    auto hr = command_list->Reset(command_allocator_pool_.RetainCommandAllocator(device, type), nullptr);
    if (FAILED(hr)) {
      logwarn("command_list->Reset in pool failed. {} {} {}", hr, type, i);
      continue;
    }
    command_list_pool_[type_index][i] = nullptr;
    return command_list;
  }
  D3d12CommandList* command_list{nullptr};
  auto hr = device->CreateCommandList1(0/*multi-GPU*/, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&command_list));
  if (FAILED(hr)) {
    logerror("device->CreateCommandList1 failed. {} {}", hr, type);
    assert(false && "device->CreateCommandList1 failed");
    return nullptr;
  }
  hr = command_list->Reset(command_allocator_pool_.RetainCommandAllocator(device, type), nullptr);
  if (FAILED(hr)) {
    logerror("command_list->Reset on creation failed. {} {}", hr, type);
    auto val = command_list->Release();
    if (val != 0) {
      logwarn("command_list reference left. {} {}", type, val);
    }
    assert(false && "command_list->Reset on creation failed");
    return nullptr;
  }
  SetD3d12Name(command_list, "command_list" + std::to_string(created_num_));
  created_num_++;
  return command_list;
}
void CommandListPool::ReturnCommandList(const D3D12_COMMAND_LIST_TYPE type, const uint32_t num, D3d12CommandList** list) {
  auto type_index = GetCommandQueueTypeIndex(type);
  uint32_t processed_num = 0;
  for (uint32_t i = 0; i < command_list_num_per_queue_type_[type_index]; i++) {
    if (command_list_pool_[type_index][i] != nullptr) { continue; }
    command_list_pool_[type_index][i] = list[processed_num];
    processed_num++;
    if (processed_num >= num) { break; }
  }
  if (processed_num != num) {
    logwarn("ReturnCommandList exceeded pool size {} {} {} {}", type, command_list_num_per_queue_type_[type_index], processed_num, num);
    for (; processed_num < num; processed_num++) {
      auto val = list[processed_num]->Release();
      if (val != 0) {
        logwarn("command_list reference left. {} {} {} {}", type, processed_num, num, val);
      }
    }
  }
}
void CommandListInUse::Init(const uint32_t command_queue_num, const uint32_t* command_list_num_per_queue) {
  command_queue_num_ =  command_queue_num;
  command_list_num_per_queue_ = AllocateArray<uint32_t>(gSystemMemoryAllocator, command_queue_num_);
  pushed_command_list_num_ = AllocateArray<uint32_t>(gSystemMemoryAllocator, command_queue_num_);
  pushed_command_list_ = AllocateArray<D3d12CommandList**>(gSystemMemoryAllocator, command_queue_num_);
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    pushed_command_list_num_[i] = 0;
    command_list_num_per_queue_[i] = command_list_num_per_queue[i];
    pushed_command_list_[i] = AllocateArray<D3d12CommandList*>(gSystemMemoryAllocator, command_list_num_per_queue_[i]);
    for (uint32_t j = 0; j < command_list_num_per_queue_[i]; j++) {
      pushed_command_list_[i][j] = nullptr;
    }
  }
}
void CommandListInUse::Term() {
  for (uint32_t i = 0; i < command_queue_num_; i++) {
    if (pushed_command_list_num_[i] != 0) {
      logwarn("CommandListInUse left. {} {}", i, pushed_command_list_num_[i]);
    }
    for (uint32_t j = 0; j < command_list_num_per_queue_[i]; j++) {
      if (pushed_command_list_[i][j] != nullptr) {
        logwarn("CommandListInUse ptr left. {} {}", i, j);
        auto val = pushed_command_list_[i][j]->Release();
        if (val != 0) {
          logwarn("CommandListInUse reference left. {} {} {}", i, j, val);
        }
      }
    }
  }
}
void CommandListInUse::PushCommandList(const uint32_t command_queue_index, D3d12CommandList* command_list) {
  pushed_command_list_[command_queue_index][pushed_command_list_num_[command_queue_index]] = command_list;
  pushed_command_list_num_[command_queue_index]++;
}
void CommandListInUse::FreePushedCommandList(const uint32_t command_queue_index) {
  for (uint32_t i = 0; i < pushed_command_list_num_[command_queue_index]; i++) {
    pushed_command_list_[command_queue_index][i] = nullptr;
  }
  pushed_command_list_num_[command_queue_index] = 0;
}
bool CommandListSet::Init(D3d12Device* device, const uint32_t command_queue_num, const D3D12_COMMAND_LIST_TYPE* command_queue_type, const D3D12_COMMAND_QUEUE_PRIORITY* command_queue_priority, const uint32_t* command_list_num_per_queue, const uint32_t frame_buffer_num, const uint32_t* command_allocator_num_per_queue_type) {
  auto allocator = GetTemporalMemoryAllocator();
  auto command_list_num_per_queue_type = AllocateArray<uint32_t>(&allocator, kCommandQueueTypeNum);
  for (uint32_t i = 0; i < kCommandQueueTypeNum; i++) {
    command_list_num_per_queue_type[i] = 0;
  }
  auto raw_command_queue_list = AllocateArray<D3d12CommandQueue*>(&allocator, command_queue_num);
  command_queue_type_ = AllocateArray<D3D12_COMMAND_LIST_TYPE>(gSystemMemoryAllocator, command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    command_queue_type_[i] = command_queue_type[i];
    command_list_num_per_queue_type[GetCommandQueueTypeIndex(command_queue_type_[i])] += command_list_num_per_queue[i];
    raw_command_queue_list[i] = CreateCommandQueue(device, command_queue_type_[i], command_queue_priority[i]);
    if (raw_command_queue_list[i] == nullptr) {
      for (uint32_t j = 0; j < i; j++) {
        raw_command_queue_list[j]->Release();
        raw_command_queue_list[j] = nullptr;
      }
      return false;
    }
  }
  command_queue_list_.Init(command_queue_num, raw_command_queue_list);
  command_list_pool_.Init(command_list_num_per_queue_type, frame_buffer_num, command_allocator_num_per_queue_type);
  command_list_in_use_.Init(command_queue_num, command_list_num_per_queue);
  return true;
}
void CommandListSet::Term() {
  command_list_in_use_.Term();
  command_list_pool_.Term();
  command_queue_list_.Term();
}
D3d12CommandList* CommandListSet::GetCommandList(D3d12Device* device, const uint32_t command_queue_index) {
  auto command_list = command_list_pool_.RetainCommandList(device, command_queue_type_[command_queue_index]);
  command_list_in_use_.PushCommandList(command_queue_index, command_list);
  return command_list;
}
void CommandListSet::ExecuteCommandList(const uint32_t command_queue_index) {
  const auto num = command_list_in_use_.GetPushedCommandListNum(command_queue_index);
  auto list = command_list_in_use_.GetPushedCommandList(command_queue_index);
  for (uint32_t i = 0; i < num; i++) {
    auto hr = list[i]->Close();
    if (FAILED(hr)) {
      logwarn("failed to close command list. {} {}", hr, i);
    }
  }
  command_queue_list_.Get(command_queue_index)->ExecuteCommandLists(num, reinterpret_cast<ID3D12CommandList**>(list));
  command_list_pool_.ReturnCommandList(command_queue_type_[command_queue_index], num, list);
  command_list_in_use_.FreePushedCommandList(command_queue_index);
}
}
