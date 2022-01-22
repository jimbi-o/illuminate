#include "D3D12MemAlloc.h"
#include "doctest/doctest.h"
#include <nlohmann/json.hpp>
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_swapchain.h"
#include "d3d12_view_util.h"
#include "d3d12_win32_window.h"
namespace illuminate {
namespace {
auto GetTestJson() {
  return R"(
{
  "frame_buffer_num": 2,
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
      "priority": "normal",
      "command_list_num": 1
    },
    {
      "name": "queue_compute",
      "type": "compute",
      "priority": "normal",
      "command_list_num": 1
    }
  ],
  "command_allocator": {
    "direct": 1,
    "compute": 1
  },
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
  },
  "buffer": [
    {
      "name": "uav",
      "format": "R8G8B8A8_UNORM",
      "heap_type": "default",
      "dimension": "texture2d",
      "width": 100,
      "height": 20,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "format": "R8G8B8A8_UNORM",
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "flags": ["uav"],
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "uav",
      "clear_color": [1,0,0,1],
      "descriptor_type": ["uav", "srv"]
    }
  ],
  "render_pass": [
    {
      "name": "clear uav",
      "command_queue": "queue_compute",
      "buffer_list": [
        {
          "name": "uav",
          "state": "uav"
        }
      ],
      "pass_vars": {
        "clear_color": [1, 0, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ]
    },
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "buffer_list": [
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
  ],
  "descriptor_handle_num_per_view_type_or_sampler": {
    "cbv": 0,
    "srv": 1,
    "uav": 1,
    "sampler": 0,
    "rtv": 0,
    "dsv": 0
  },
  "gpu_handle_num_view":10,
  "gpu_handle_num_sampler":1
}
)"_json;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    switch (barrier.Type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource   = resource[i];
        barrier.Transition.StateBefore = config.state_before;
        barrier.Transition.StateAfter  = config.state_after;
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        assert(false && "aliasing barrier not implemented yet");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        barrier.UAV.pResource   = resource[i];
        break;
      }
    }
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
template <typename A>
auto GetRenderPassVarSizeMap(A* allocator) {
  HashMap<uint32_t, A> render_pass_var_size(allocator);
  render_pass_var_size.Insert(SID("output to swapchain"), sizeof(FLOAT) * 4);
  render_pass_var_size.Insert(SID("clear uav"), sizeof(UINT) * 4);
  return render_pass_var_size;
}
using RenderPassFunction = void (*)(D3d12CommandList*, const void*, const D3D12_GPU_DESCRIPTOR_HANDLE*, const D3D12_CPU_DESCRIPTOR_HANDLE*, ID3D12Resource**);
void ClearRtv(D3d12CommandList* command_list, const void* pass_vars, [[maybe_unused]]const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, [[maybe_unused]]ID3D12Resource** resources) {
  command_list->ClearRenderTargetView(*cpu_handles, static_cast<const FLOAT*>(pass_vars), 0, nullptr);
}
void ParsePassParamClearRtv(const nlohmann::json& j, void* dst) {
  auto src_color = j.at("clear_color");
  auto dst_color = static_cast<FLOAT*>(dst);
  for (uint32_t i = 0; i < 4; i++) {
    dst_color[i] = src_color[i];
  }
}
void ClearUav(D3d12CommandList* command_list, const void* pass_vars, const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, ID3D12Resource** resources) {
  command_list->ClearUnorderedAccessViewUint(*gpu_handles, *cpu_handles, *resources, static_cast<const UINT*>(pass_vars), 0, nullptr);
}
void ParsePassParamClearUav(const nlohmann::json& j, void* dst) {
  auto src_color = j.at("clear_color");
  auto dst_color = static_cast<UINT*>(dst);
  for (uint32_t i = 0; i < 4; i++) {
    dst_color[i] = src_color[i];
  }
}
void* D3d12BufferAllocatorAllocCallback(size_t Size, size_t Alignment, [[maybe_unused]] void* pUserData) {
  return gSystemMemoryAllocator->Allocate(Size, Alignment);
}
void D3d12BufferAllocatorFreeCallback([[maybe_unused]] void* pMemory, [[maybe_unused]] void* pUserData) {}
auto GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device) {
  using namespace D3D12MA;
  ALLOCATION_CALLBACKS allocation_callbacks{
    .pAllocate = D3d12BufferAllocatorAllocCallback,
    .pFree = D3d12BufferAllocatorFreeCallback,
    .pUserData = nullptr,
  };
  ALLOCATOR_DESC desc{
    .Flags = ALLOCATOR_FLAG_NONE,
    .pDevice = device,
    .PreferredBlockSize = 0, // will use default (64MiB)
    .pAllocationCallbacks = &allocation_callbacks,
    .pAdapter = adapter,
  };
  Allocator* allocator{nullptr};
  auto hr = CreateAllocator(&desc, &allocator);
  if (FAILED(hr)) {
    logerror("CreateAllocator failed. {}", hr);
  }
  return allocator;
}
struct BufferAllocation {
  D3D12MA::Allocation* allocation{nullptr};
  ID3D12Resource* resource{nullptr};
};
void ReleaseBufferAllocation(BufferAllocation* b) {
  b->allocation->Release();
  b->resource->Release();
}
auto CreateBuffer(const BufferConfig& config, D3D12MA::Allocator* allocator) {
  using namespace D3D12MA;
  ALLOCATION_DESC allocation_desc{
    .Flags = ALLOCATION_FLAG_NONE,
    .HeapType = config.heap_type,
    .ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
    .CustomPool = nullptr,
  };
  D3D12_RESOURCE_DESC1 resource_desc{
    .Dimension = config.dimension,
    .Alignment = config.alignment,
    .Width = config.width,
    .Height = config.height,
    .DepthOrArraySize = config.depth_or_array_size,
    .MipLevels = config.miplevels,
    .Format = config.format,
    .SampleDesc = {
      .Count = config.sample_count,
      .Quality = config.sample_quality,
    },
    .Layout = config.layout,
    .Flags = config.flags,
    .SamplerFeedbackMipRegion = {
      .Width = config.mip_width,
      .Height = config.mip_height,
      .Depth = config.mip_depth,
    },
  };
  auto clear_value = (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0) ? &config.clear_value : nullptr;
  BufferAllocation buffer_allocation{};
  auto hr = allocator->CreateResource2(&allocation_desc, &resource_desc, config.initial_state, clear_value, nullptr, &buffer_allocation.allocation, IID_PPV_ARGS(&buffer_allocation.resource));
  if (FAILED(hr)) {
    logerror("failed to create resource. {}", hr);
    assert(false && "failed to create resource");
    if (buffer_allocation.allocation) {
      buffer_allocation.allocation->Release();
      buffer_allocation.allocation = nullptr;
    }
    if (buffer_allocation.resource) {
      buffer_allocation.resource->Release();
      buffer_allocation.resource = nullptr;
    }
  }
  return buffer_allocation;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, const HashMap<BufferAllocation, MemoryAllocationJanitor>& buffer_list, const HashMap<ID3D12Resource*, MemoryAllocationJanitor>& extra_buffer_list) {
  auto allocator = GetTemporalMemoryAllocator();
  auto resource_list = AllocateArray<ID3D12Resource*>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& buffer_name = barrier_config[i].buffer_name;
    auto buffer_allocation = buffer_list.Get(buffer_name);
    resource_list[i] = (buffer_allocation == nullptr) ? *extra_buffer_list.Get(buffer_name) : buffer_allocation->resource;
  }
  ExecuteBarrier(command_list, barrier_num, barrier_config, resource_list);
}
auto PrepareGpuHandleList(D3d12Device* device, const uint32_t render_pass_num, const RenderPass* render_pass_list, const DescriptorCpu<MemoryAllocationJanitor>& descriptor_cpu, DescriptorGpu* const descriptor_gpu, MemoryAllocationJanitor* allocator) {
  HashMap<D3D12_GPU_DESCRIPTOR_HANDLE, MemoryAllocationJanitor> gpu_handle_list(allocator);
  auto total_handle_count = descriptor_gpu->GetViewHandleTotal();
  uint32_t occupied_handle_num = 0;
  for (uint32_t i = 0; i < render_pass_num; i++) {
    auto prev_handle_count = descriptor_gpu->GetViewHandleCount();
    auto& render_pass = render_pass_list[i];
    if (!gpu_handle_list.Insert(render_pass.name, descriptor_gpu->CopyDescriptors(device, render_pass.buffer_num, render_pass.buffer_list, descriptor_cpu))) {
      logerror("gpu_handle_list.Insert failed. {}", i);
      assert(false && "gpu_handle_list.Insert failed.");
    }
    auto current_handle_count = descriptor_gpu->GetViewHandleCount();
    if (prev_handle_count <= current_handle_count) {
      occupied_handle_num += current_handle_count - prev_handle_count;
    } else {
      occupied_handle_num += total_handle_count - prev_handle_count + current_handle_count;
    }
    assert(occupied_handle_num <= total_handle_count && "increase RenderGraph::gpu_handle_num_view");
  }
  return gpu_handle_list;
}
} // namespace anonymous
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(&allocator);
  HashMap<RenderPassVarParseFunction, MemoryAllocationJanitor> render_pass_var_parse_functions(&allocator);
  CHECK_UNARY(render_pass_functions.Insert(SID("output to swapchain"), ClearRtv));
  CHECK_UNARY(render_pass_var_parse_functions.Insert(SID("output to swapchain"), ParsePassParamClearRtv));
  CHECK_UNARY(render_pass_functions.Insert(SID("clear uav"), ClearUav));
  CHECK_UNARY(render_pass_var_parse_functions.Insert(SID("clear uav"), ParsePassParamClearUav));
  auto render_pass_var_size = GetRenderPassVarSizeMap(&allocator);
  RenderGraph render_graph;
  ParseRenderGraphJson(GetTestJson(), render_pass_var_size, render_pass_var_parse_functions, &allocator, &render_graph);
  const uint32_t swapchain_buffer_num = render_graph.frame_buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
  CommandListSet command_list_set;
  CHECK_UNARY(command_list_set.Init(device.Get(),
                                    render_graph.command_queue_num,
                                    render_graph.command_queue_type,
                                    render_graph.command_queue_priority,
                                    render_graph.command_list_num_per_queue,
                                    render_graph.frame_buffer_num,
                                    render_graph.command_allocator_num_per_queue_type));
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  DescriptorCpu<MemoryAllocationJanitor> descriptor_cpu;
  CHECK_UNARY(descriptor_cpu.Init(device.Get(), render_graph.descriptor_handle_num_per_view_type_or_sampler, &allocator));
  HashMap<BufferAllocation, MemoryAllocationJanitor> buffer_list(&allocator, 256);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    CAPTURE(i);
    auto buffer = CreateBuffer(render_graph.buffer_list[i], buffer_allocator);
    CHECK_NE(buffer.allocation, nullptr);
    CHECK_NE(buffer.resource, nullptr);
    CHECK_UNARY(buffer_list.Insert(render_graph.buffer_list[i].name, std::move(buffer)));
    for (uint32_t j = 0; j < render_graph.buffer_list[i].descriptor_type_num; j++) {
      CAPTURE(j);
      auto cpu_handle = descriptor_cpu.CreateHandle(render_graph.buffer_list[i].name, render_graph.buffer_list[i].descriptor_type[j]);
      CHECK_NE(cpu_handle, nullptr);
      CHECK_UNARY(CreateView(device.Get(), render_graph.buffer_list[i].descriptor_type[j], render_graph.buffer_list[i], buffer.resource, cpu_handle));
    }
  }
  DescriptorGpu descriptor_gpu;
  CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view, render_graph.gpu_handle_num_sampler));
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, swapchain_buffer_num, render_graph.frame_buffer_num, render_graph.swapchain_usage)); // NOLINT
  HashMap<ID3D12Resource*, MemoryAllocationJanitor> extra_buffer_list(&allocator);
  extra_buffer_list.Reserve(SID("swapchain")); // because MemoryAllocationJanitor is a stack allocator, all allocations must be done before frame loop starts.
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_start = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_end = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
    gpu_descriptor_offset_start[i] = ~0u;
    gpu_descriptor_offset_end[i] = ~0u;
  }
  uint32_t tmp_memory_max_offset = 0U;
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    auto used_command_queue = AllocateArray<bool>(&single_frame_allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      used_command_queue[j] = false;
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    extra_buffer_list.Replace(SID("swapchain"), swapchain.GetResource());
    gpu_descriptor_offset_start[frame_index] = descriptor_gpu.GetViewHandleCount();
    if (gpu_descriptor_offset_start[frame_index] == descriptor_gpu.GetViewHandleTotal()) {
      gpu_descriptor_offset_start[frame_index] = 0;
    }
    auto gpu_handle_list = PrepareGpuHandleList(device.Get(), render_graph.render_pass_num, render_graph.render_pass_list, descriptor_cpu, &descriptor_gpu, &single_frame_allocator);
    gpu_descriptor_offset_end[frame_index] = descriptor_gpu.GetViewHandleCount();
    if (i > 0) {
      for (uint32_t j = 0; j < render_graph.frame_buffer_num; j++) {
        if (frame_index == j) { continue; }
        if (gpu_descriptor_offset_start[j] == ~0u) { continue; }
        if (gpu_descriptor_offset_start[j] <= gpu_descriptor_offset_end[j]) {
          if (gpu_descriptor_offset_start[frame_index] <= gpu_descriptor_offset_end[frame_index]) {
            assert(gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] || gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && "increase RenderGraph::gpu_handle_num_view");
            continue;
          }
          assert(gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] && "increase RenderGraph::gpu_handle_num_view");
          continue;
        }
        if (gpu_descriptor_offset_start[frame_index] <= gpu_descriptor_offset_end[frame_index]) {
          assert(gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] && gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && "increase RenderGraph::gpu_handle_num_view");
          continue;
        }
        assert(false  && "increase RenderGraph::gpu_handle_num_view");
      }
    }
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      auto command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
      descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, buffer_list, extra_buffer_list);
      {
        auto render_pass_allocator = GetTemporalMemoryAllocator();
        auto resource_list = AllocateArray<ID3D12Resource*>(&render_pass_allocator, render_pass.buffer_num);
        auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&render_pass_allocator, render_pass.buffer_num);
        for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
          auto& buffer = render_pass.buffer_list[b];
          auto buffer_allocation = buffer_list.Get(buffer.buffer_name);
          resource_list[b] = (buffer_allocation != nullptr) ? buffer_allocation->resource : *extra_buffer_list.Get(buffer.buffer_name);
          cpu_handle_list[b] = (buffer.buffer_name != SID("swapchain")) ? *descriptor_cpu.GetHandle(buffer.buffer_name, buffer.state) : swapchain.GetRtvHandle();
          tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
        }
        (**render_pass_functions.Get(render_pass.name))(command_list, render_pass.pass_vars, gpu_handle_list.Get(render_pass.name), cpu_handle_list, resource_list);
        tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
      }
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, buffer_list, extra_buffer_list);
      used_command_queue[render_pass.command_queue_index] = true;
      command_list_set.ExecuteCommandList(render_pass.command_queue_index); // TODO
      frame_signals[frame_index][render_pass.command_queue_index] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  descriptor_gpu.Term();
  descriptor_cpu.Term();
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    ReleaseBufferAllocation(buffer_list.Get(render_graph.buffer_list[i].name));
  }
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  loginfo("memory global:{} temp:{}", gSystemMemoryAllocator->GetOffset(), tmp_memory_max_offset);
  gSystemMemoryAllocator->Reset();
}
