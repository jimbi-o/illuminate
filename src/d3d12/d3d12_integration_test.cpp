#include "d3d12_src_common.h"
#include "illuminate/core/strid.h"
namespace illuminate {
}
#ifdef BUILD_WITH_TEST
#include "doctest/doctest.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include <nlohmann/json.hpp>
#include "d3d12_dxgi_core.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
namespace {
auto GetJson() {
  return R"(
{
  "buffer_num": 2,
  "frame_loop_num": 5,
  "window": {
    "title": "integration test",
    "width": 500,
    "height" : 300
  },
  "command_queue": [
    {
      "name": "queue_graphics",
      "type": "3d",
      "priority" : "normal"
    }
  ],
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
  },
  "render_pass": [
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "buffers": [
        {
          "name": "swapchain",
          "state": "rtv"
        }
      ],
      "pass_vars": {
        "clear_color": [0, 1, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "present",
          "state_after": "rtv"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "rtv",
          "state_after": "present"
        }
      ]
    }
  ]
}
)"_json;
}
struct RenderPassBuffer {
  StrHash buffer_name{};
  D3D12_RESOURCE_STATES state{};
};
struct Barrier {
  StrHash buffer_name{};
  D3D12_RESOURCE_BARRIER_TYPE type{};
  D3D12_RESOURCE_BARRIER_FLAGS flag{}; // split begin/end/none
  D3D12_RESOURCE_STATES state_before{};
  D3D12_RESOURCE_STATES state_after{};
};
struct RenderPass {
  StrHash name{};
  uint32_t command_queue_index{0};
  uint32_t buffer_num{0};
  RenderPassBuffer* buffers{nullptr};
  void* pass_vars{nullptr};
  uint32_t prepass_barrier_num{0};
  Barrier* prepass_barrier{nullptr};
  uint32_t postpass_barrier_num{0};
  Barrier* postpass_barrier{nullptr};
};
struct RenderGraph {
  uint32_t buffer_num{0};
  uint32_t frame_loop_num{0};
  char* window_title{nullptr};
  uint32_t window_width{0};
  uint32_t window_height{0};
  uint32_t command_queue_num{0};
  StrHash* command_queue_name{nullptr};
  D3D12_COMMAND_LIST_TYPE* command_queue_type{nullptr};
  uint32_t swapchain_command_queue_index{0};
  DXGI_FORMAT swapchain_format{};
  DXGI_USAGE swapchain_usage{};
  uint32_t render_pass_num{0};
  RenderPass* render_pass_list{nullptr};
};
auto GetStringView(const nlohmann::json& j, const char* const name) {
  return j.at(name).get<std::string_view>();
}
auto CalcEntityStrHash(const nlohmann::json& j, const char* const name) {
  return CalcStrHash(GetStringView(j, name).data());
}
auto FindIndex(const nlohmann::json& j, const char* const name, const uint32_t num, StrHash* list) {
  auto hash = CalcEntityStrHash(j, name);
  for (uint32_t i = 0; i < num; i++) {
    if (list[i] == hash) { return i; }
  }
  logwarn("FindIndex: {} not found. {}", name, num);
  return ~0U;
}
auto GetD3d12ResourceState(const nlohmann::json& j, const char* const name) {
  auto state_str = j.at(name).get<std::string_view>();
  D3D12_RESOURCE_STATES state{};
  // TODO
  if (state_str.compare("present") == 0) {
    state |= D3D12_RESOURCE_STATE_PRESENT;
  }
  if (state_str.compare("rtv") == 0) {
    state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
  }
  return state;
}
template <typename A>
auto GetBarrierList(const nlohmann::json& j, const uint32_t barrier_num, A* allocator) {
  auto barrier_list = AllocateArray<Barrier>(allocator, barrier_num);
  for (uint32_t barrier_index = 0; barrier_index < barrier_num; barrier_index++) {
    auto& dst_barrier = barrier_list[barrier_index];
    auto& src_barrier = j[barrier_index];
    dst_barrier.buffer_name = CalcEntityStrHash(src_barrier, "buffer_name");
    {
      auto type_str = GetStringView(src_barrier, "type");
      if (type_str.compare("transition") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      } else if (type_str.compare("aliasing") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
      } else if (type_str.compare("uav") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      } else {
        logerror("invalid barrier type: {} {} {}", type_str.data(), GetStringView(src_barrier, "buffer_name").data(), barrier_index);
        assert(false && "invalid barrier type");
      }
    } // type
    if (src_barrier.contains("split_type")) {
      auto flag_str = GetStringView(src_barrier, "split_type");
      if (flag_str.compare("begin") == 0) {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
      } else if (flag_str.compare("end") == 0) {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
      } else {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      }
    } else {
      dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    } // flag
    switch (dst_barrier.type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        dst_barrier.state_before = GetD3d12ResourceState(src_barrier, "state_before");
        dst_barrier.state_after  = GetD3d12ResourceState(src_barrier, "state_after");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        // TODO
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        // TODO
        break;
      }
      default: {
        logerror("invalid barrier type. {}", dst_barrier.type);
        assert(false);
        break;
      }
    } // switch
  }
  return barrier_list;
}
template <typename A>
void SetRenderGraph(const nlohmann::json& j, A* allocator, RenderGraph& r) {
  j.at("buffer_num").get_to(r.buffer_num);
  j.at("frame_loop_num").get_to(r.frame_loop_num);
  {
    auto& window = j.at("window");
    auto window_title = GetStringView(window, "title");
    auto window_title_len = static_cast<uint32_t>(window_title.size()) + 1;
    r.window_title = AllocateArray<char>(allocator, window_title_len);
    strcpy_s(r.window_title, window_title_len, window_title.data());
    window.at("width").get_to(r.window_width);
    window.at("height").get_to(r.window_height);
  }
  {
    auto& command_queues = j.at("command_queue");
    r.command_queue_num = static_cast<uint32_t>(command_queues.size());
    r.command_queue_name = AllocateArray<StrHash>(allocator, r.command_queue_num);
    r.command_queue_type = AllocateArray<D3D12_COMMAND_LIST_TYPE>(allocator, r.command_queue_num);
    for (uint32_t i = 0; i < r.command_queue_num; i++) {
      r.command_queue_name[i] = CalcEntityStrHash(command_queues[i], "name");
      auto command_queue_type_str = GetStringView(command_queues[i], "type");
      if (command_queue_type_str.compare("compute") == 0) {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COMPUTE;
      } else if (command_queue_type_str.compare("copy") == 0) {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COPY;
      } else {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_DIRECT;
      }
    }
  }
  {
    auto& swapchain = j.at("swapchain");
    r.swapchain_command_queue_index = FindIndex(swapchain, "command_queue", r.command_queue_num, r.command_queue_name);
    auto format_str = GetStringView(swapchain, "format");
    if (format_str.compare("R16G16B16A16_FLOAT") == 0) {
      r.swapchain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    } else if (format_str.compare("B8G8R8A8_UNORM") == 0) {
      r.swapchain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else {
      r.swapchain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    auto usage_list = swapchain.at("usage");
    for (auto& usage : usage_list) {
      auto usage_str = usage.get<std::string_view>();
      if (usage_str.compare("READ_ONLY") == 0) {
        r.swapchain_usage = DXGI_USAGE_READ_ONLY;
      }
      if (usage_str.compare("RENDER_TARGET_OUTPUT") == 0) {
        r.swapchain_usage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
      }
      if (usage_str.compare("SHADER_INPUT") == 0) {
        r.swapchain_usage |= DXGI_USAGE_SHADER_INPUT;
      }
      if (usage_str.compare("SHARED") == 0) {
        r.swapchain_usage |= DXGI_USAGE_SHARED;
      }
      if (usage_str.compare("UNORDERED_ACCESS") == 0) {
        r.swapchain_usage |= DXGI_USAGE_UNORDERED_ACCESS;
      }
    }
  }
  {
    auto& render_pass_list = j.at("render_pass");
    r.render_pass_num = static_cast<uint32_t>(render_pass_list.size());
    r.render_pass_list = AllocateArray<RenderPass>(allocator, r.render_pass_num);
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      dst_pass.name = CalcEntityStrHash(src_pass, "name");
      dst_pass.command_queue_index = FindIndex(src_pass, "command_queue", r.command_queue_num, r.command_queue_name);
      {
        auto& buffers = src_pass.at("buffers");
        dst_pass.buffer_num = static_cast<uint32_t>(buffers.size());
        dst_pass.buffers = AllocateArray<RenderPassBuffer>(allocator, dst_pass.buffer_num);
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffers[buffer_index];
          auto& src_buffer = buffers[buffer_index];
          dst_buffer.buffer_name = CalcEntityStrHash(src_buffer, "name");
          dst_buffer.state = GetD3d12ResourceState(src_buffer, "state");
        }
      } // buffers
      dst_pass.pass_vars = nullptr; // TODO
      {
        auto& prepass_barrier = src_pass.at("prepass_barrier");
        dst_pass.prepass_barrier_num = static_cast<uint32_t>(prepass_barrier.size());
        dst_pass.prepass_barrier = GetBarrierList(prepass_barrier, dst_pass.prepass_barrier_num, allocator);
        auto& postpass_barrier = src_pass.at("postpass_barrier");
        dst_pass.postpass_barrier_num = static_cast<uint32_t>(postpass_barrier.size());
        dst_pass.postpass_barrier = GetBarrierList(postpass_barrier, dst_pass.postpass_barrier_num, allocator);
      } // barriers
    } // pass
  } // pass_list
}
template <typename A>
RenderGraph GetRenderGraph(A* allocator) {
  RenderGraph graph;
  SetRenderGraph(GetJson(), allocator, graph);
  return graph;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = resource[i];
    barrier.Transition.StateBefore = config.state_before;
    barrier.Transition.StateAfter  = config.state_after;
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
} // anonymous namespace
class CommandListPool {
 public:
  bool Init(const uint32_t max_command_list_num_per_queue_type);
  void Term();
  void SucceedFrame();
  D3d12CommandList* RetainCommandList(const D3D12_COMMAND_LIST_TYPE);
  void ReturnCommandList(const D3D12_COMMAND_LIST_TYPE, const uint32_t num, D3d12CommandList**);
 private:
  D3d12CommandList*** command_list_pool_;
};
class CommandListInUse {
 public:
  bool Init();
  void Term();
  void PushCommandList(const uint32_t command_queue_index);
  void FreePushedCommandList(const uint32_t command_queue_index);
  constexpr auto GetPushedCommandListNum(const uint32_t command_queue_index) const { return pushed_command_list_num_[command_queue_index]; }
  constexpr auto GetPushedCommandList(const uint32_t command_queue_index) const { return pushed_command_list_[command_queue_index]; }
 private:
  uint32_t* pushed_command_list_num_{nullptr};
  D3d12CommandList*** pushed_command_list_{nullptr};
};
class CommandListSet {
 public:
  bool Init(D3d12Device* device, const uint32_t command_queue_num, D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t max_command_list_num_per_queue_type);
  void Term();
  constexpr auto GetCommandQueueList() { return command_queue_list_.GetList(); }
  constexpr auto GetCommandQueue(const uint32_t command_queue_index) { return command_queue_list_.Get(command_queue_index); }
  D3d12CommandList* GetCommandList(const uint32_t command_queue_index);
  bool ExecuteCommandList(const uint32_t command_queue_index);
 private:
  CommandQueueList command_queue_list_;
  CommandListPool command_list_pool_;
  CommandListInUse command_list_in_use_;
};
bool CommandListPool::Init(const uint32_t max_command_list_num_per_queue_type) {
  // TODO
  return false;
}
void CommandListPool::Term() {
  // TODO
}
void CommandListPool::SucceedFrame() {
  // TODO
}
D3d12CommandList* CommandListPool::RetainCommandList(const D3D12_COMMAND_LIST_TYPE type) {
  // TODO
  return nullptr;
}
void CommandListPool::ReturnCommandList(const D3D12_COMMAND_LIST_TYPE type, const uint32_t num, D3d12CommandList** list) {
  // TODO
}
bool CommandListInUse::Init() {
  // TODO
  return false;
}
void CommandListInUse::Term() {
  // TODO
}
void CommandListInUse::PushCommandList(const uint32_t command_queue_index) {
  // TODO
}
void CommandListInUse::FreePushedCommandList(const uint32_t command_queue_index) {
  // TODO
}
bool CommandListSet::Init(D3d12Device* device, const uint32_t command_queue_num, D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t max_command_list_num_per_queue_type) {
  auto allocator = GetTemporalMemoryAllocator();
  auto raw_command_queue_list = AllocateArray<D3d12CommandQueue*>(&allocator, command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    raw_command_queue_list[i] = CreateCommandQueue(device, command_queue_type[i]);
    if (raw_command_queue_list[i] == nullptr) {
      for (uint32_t j = 0; j < i; j++) {
        raw_command_queue_list[j]->Release();
        raw_command_queue_list[j] = nullptr;
      }
      return false;
    }
  }
  command_queue_list_.Init(command_queue_num, raw_command_queue_list);
  if (!command_list_pool_.Init(max_command_list_num_per_queue_type)) { return false; }
  if (!command_list_in_use_.Init()) { return false; }
  return true;
}
void CommandListSet::Term() {
  command_list_in_use_.Term();
  command_list_pool_.Term();
  command_queue_list_.Term();
}
D3d12CommandList* CommandListSet::GetCommandList(const uint32_t command_queue_index) {
  // TODO
  return nullptr;
}
bool CommandListSet::ExecuteCommandList(const uint32_t command_queue_index) {
  // TODO
  // command_queue_list.Get(render_pass.command_queue_index)->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&command_list)); // TODO
  return false;
}
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  auto render_graph = GetRenderGraph(&allocator);
  const uint32_t swapchain_buffer_num = render_graph.buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
  CommandListSet command_list_set;
  const uint32_t max_command_list_num_per_queue_type = 8;
  CHECK_UNARY(command_list_set.Init(device.Get(), render_graph.command_queue_num, render_graph.command_queue_type, max_command_list_num_per_queue_type));
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, swapchain_buffer_num, render_graph.buffer_num, render_graph.swapchain_usage)); // NOLINT
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.buffer_num);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  uint32_t tmp_memory_max_offset = 0U;
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    auto used_command_queue = AllocateArray<bool>(&single_frame_allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      used_command_queue[j] = false;
    }
    const auto frame_index = i % render_graph.buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    swapchain.UpdateBackBufferIndex();
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      auto command_list = command_list_set.GetCommandList(render_pass.command_queue_index);
      auto resource = swapchain.GetResource(); // TODO
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, &resource);
      {
        // TODO use pass name & pass vars in render_graph
        const FLOAT clear_color[4] = {0.0f,1.0f,1.0f,1.0f};
        command_list->ClearRenderTargetView(swapchain.GetRtvHandle(), clear_color, 0, nullptr);
      }
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, &resource);
      used_command_queue[render_pass.command_queue_index] = true;
      command_list_set.ExecuteCommandList(render_pass.command_queue_index); // TODO
      frame_signals[frame_index][render_pass.command_queue_index] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  loginfo("memory global:{} temp:{}", gSystemMemoryAllocator->GetOffset(), tmp_memory_max_offset);
  gSystemMemoryAllocator->Reset();
}
#endif
