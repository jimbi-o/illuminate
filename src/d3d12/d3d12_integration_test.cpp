#include <fstream>
#include <nlohmann/json.hpp>
#include "tiny_gltf.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_scene.h"
#include "d3d12_shader_compiler.h"
#include "d3d12_swapchain.h"
#include "d3d12_view_util.h"
#include "d3d12_win32_window.h"
#include "render_pass/d3d12_render_pass_copy_resource.h"
#include "render_pass/d3d12_render_pass_cs_dispatch.h"
#include "render_pass/d3d12_render_pass_debug_render_selected_buffer.h"
#include "render_pass/d3d12_render_pass_imgui.h"
#include "render_pass/d3d12_render_pass_postprocess.h"
#include "render_pass/d3d12_render_pass_prez.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
namespace {
static const uint32_t kFrameLoopNum = 1000;
auto GetTestJson() {
  std::ifstream file("config.json");
  nlohmann::json json;
  file >> json;
  return json;
}
auto GetTestTinyGltf() {
  return R"(
{
  "scene": 0,
  "scenes" : [
    {
      "nodes" : [ 0 ]
    }
  ],
  "nodes" : [
    {
      "mesh" : 0
    }
  ],
  "meshes" : [
    {
      "primitives" : [ {
        "attributes" : {
          "POSITION" : 1
        },
        "indices" : 0
      } ]
    }
  ],
  "buffers" : [
    {
      "uri" : "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=",
      "byteLength" : 44
    }
  ],
  "bufferViews" : [
    {
      "buffer" : 0,
      "byteOffset" : 0,
      "byteLength" : 6,
      "target" : 34963
    },
    {
      "buffer" : 0,
      "byteOffset" : 8,
      "byteLength" : 36,
      "target" : 34962
    }
  ],
  "accessors" : [
    {
      "bufferView" : 0,
      "byteOffset" : 0,
      "componentType" : 5123,
      "count" : 3,
      "type" : "SCALAR",
      "max" : [ 2 ],
      "min" : [ 0 ]
    },
    {
      "bufferView" : 1,
      "byteOffset" : 0,
      "componentType" : 5126,
      "count" : 3,
      "type" : "VEC3",
      "max" : [ 1.0, 1.0, 0.0 ],
      "min" : [ 0.0, 0.0, 0.0 ]
    }
  ],
  "asset" : {
    "version" : "2.0"
  }
}
)";
}
void UpdatePingpongA(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  float c[4]{0.0f,1.0f,1.0f,1.0f};
  args_per_pass->ptr = c;
  args_per_pass->ptr_size = GetUint32(sizeof(float)) * 4;
  RenderPassPostprocess::Update(args_common, args_per_pass);
}
void UpdatePingpongB(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  float c[4]{1.0f,0.0f,1.0f,1.0f};
  args_per_pass->ptr = c;
  args_per_pass->ptr_size = GetUint32(sizeof(float)) * 4;
  RenderPassPostprocess::Update(args_common, args_per_pass);
}
void UpdatePingpongC(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  float c[4]{1.0f,1.0f,1.0f,1.0f};
  args_per_pass->ptr = c;
  args_per_pass->ptr_size = GetUint32(sizeof(float)) * 4;
  RenderPassPostprocess::Update(args_common, args_per_pass);
}
auto PrepareRenderPassFunctions(const uint32_t render_pass_num, const RenderPass* render_pass_list, MemoryAllocationJanitor* allocator) {
  RenderPassFunctionList funcs{};
  funcs.init = AllocateArray<RenderPassFuncInit>(allocator, render_pass_num);
  funcs.term = AllocateArray<RenderPassFuncTerm>(allocator, render_pass_num);
  funcs.update = AllocateArray<RenderPassFuncUpdate>(allocator, render_pass_num);
  funcs.is_render_needed = AllocateArray<RenderPassFuncIsRenderNeeded>(allocator, render_pass_num);
  funcs.render = AllocateArray<RenderPassFuncRender>(allocator, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    switch (render_pass_list[i].name) {
      case SID("dispatch cs"): {
        funcs.init[i] = RenderPassCsDispatch::Init;
        funcs.term[i] = RenderPassCsDispatch::Term;
        funcs.update[i] = RenderPassCsDispatch::Update;
        funcs.is_render_needed[i] = RenderPassCsDispatch::IsRenderNeeded;
        funcs.render[i] = RenderPassCsDispatch::Render;
        break;
      }
      case SID("prez"): {
        funcs.init[i] = RenderPassPrez::Init;
        funcs.term[i] = RenderPassPrez::Term;
        funcs.update[i] = RenderPassPrez::Update;
        funcs.is_render_needed[i] = RenderPassPrez::IsRenderNeeded;
        funcs.render[i] = RenderPassPrez::Render;
        break;
      }
      case SID("pingpong-a"):
      case SID("pingpong-b"):
      case SID("pingpong-c"):
      case SID("output to swapchain"): {
        funcs.init[i] = RenderPassPostprocess::Init;
        funcs.term[i] = RenderPassPostprocess::Term;
        switch (render_pass_list[i].name) {
          case SID("pingpong-a"): {
            funcs.update[i] = UpdatePingpongA;
            break;
          }
          case SID("pingpong-b"): {
            funcs.update[i] = UpdatePingpongB;
            break;
          }
          case SID("pingpong-c"): {
            funcs.update[i] = UpdatePingpongC;
            break;
          }
          case SID("output to swapchain"): {
            funcs.update[i] = RenderPassPostprocess::Update;
            break;
          }
        }
        funcs.is_render_needed[i] = RenderPassPostprocess::IsRenderNeeded;
        funcs.render[i] = RenderPassPostprocess::Render;
        break;
      }
      case SID("imgui"): {
        funcs.init[i] = RenderPassImgui::Init;
        funcs.term[i] = RenderPassImgui::Term;
        funcs.update[i] = RenderPassImgui::Update;
        funcs.is_render_needed[i] = RenderPassImgui::IsRenderNeeded;
        funcs.render[i] = RenderPassImgui::Render;
        break;
      }
      case SID("copy resource"): {
        funcs.init[i] = RenderPassCopyResource::Init;
        funcs.term[i] = RenderPassCopyResource::Term;
        funcs.update[i] = RenderPassCopyResource::Update;
        funcs.is_render_needed[i] = RenderPassCopyResource::IsRenderNeeded;
        funcs.render[i] = RenderPassCopyResource::Render;
        break;
      }
      case SID("debug buffer"): {
        funcs.init[i] = RenderPassDebugRenderSelectedBuffer::Init;
        funcs.term[i] = RenderPassDebugRenderSelectedBuffer::Term;
        funcs.update[i] = RenderPassDebugRenderSelectedBuffer::Update;
        funcs.is_render_needed[i] = RenderPassDebugRenderSelectedBuffer::IsRenderNeeded;
        funcs.render[i] = RenderPassDebugRenderSelectedBuffer::Render;
        break;
      }
      default: {
        logerror("render pass function not registered. {}", render_pass_list[i].name);
        assert(false && "render pass function not registered.");
        break;
      }
    }
  }
  return funcs;
}
auto PrepareResourceCpuHandleList(const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, const BufferList& buffer_list, const bool** write_to_sub, MemoryAllocationJanitor* allocator) {
  auto resource_list = AllocateArray<ID3D12Resource*>(allocator, render_pass.buffer_num);
  auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(allocator, render_pass.buffer_num);
  for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
    auto& buffer = render_pass.buffer_list[b];
    const auto pingpong_type = GetPingPongBufferType(buffer.state, write_to_sub[buffer.buffer_index][render_pass.index]);
    resource_list[b] = GetResource(buffer_list, buffer.buffer_index, pingpong_type);
    const auto descriptor_type = ConvertToDescriptorType(buffer.state);
    if (descriptor_type != DescriptorType::kNum) {
      cpu_handle_list[b].ptr = descriptor_cpu->GetHandle(GetBufferAllocationIndex(buffer_list, buffer.buffer_index, pingpong_type), descriptor_type).ptr;
    } else {
      cpu_handle_list[b].ptr = 0;
    }
  }
  return std::make_tuple(resource_list, cpu_handle_list);
}
auto PrepareGpuHandleList(D3d12Device* device, const RenderPass& render_pass, const BufferList& buffer_list, const bool** write_to_sub, const DescriptorCpu& descriptor_cpu, DescriptorGpu* const descriptor_gpu, MemoryAllocationJanitor* allocator) {
  auto gpu_handle_list = AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, 2); // 0:view 1:sampler
  {
    // view
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto buffer_id_list = AllocateArray<uint32_t>(&tmp_allocator, render_pass.buffer_num);
    auto descriptor_type_list = AllocateArray<DescriptorType>(&tmp_allocator, render_pass.buffer_num);
    for (uint32_t j = 0; j < render_pass.buffer_num; j++) {
      const auto buffer_index = render_pass.buffer_list[j].buffer_index;
      buffer_id_list[j] = GetBufferAllocationIndex(buffer_list, buffer_index, GetPingPongBufferType(render_pass.buffer_list[j].state, write_to_sub[buffer_index][render_pass.index]));
      descriptor_type_list[j] = ConvertToDescriptorType(render_pass.buffer_list[j].state);
    }
    gpu_handle_list[0] = descriptor_gpu->CopyViewDescriptors(device, render_pass.buffer_num, buffer_id_list, descriptor_type_list, descriptor_cpu);
  }
  {
    // sampler
    gpu_handle_list[1] = descriptor_gpu->CopySamplerDescriptors(device, render_pass.sampler_num, render_pass.sampler_index_list, descriptor_cpu);
  }
  return gpu_handle_list;
}
auto CreateCpuHandleWithViewImpl(const BufferConfig& buffer_config, const uint32_t buffer_id, const DescriptorType& descriptor_type, ID3D12Resource* resource, DescriptorCpu* descriptor_cpu, D3d12Device* device) {
  auto cpu_handle = descriptor_cpu->CreateHandle(buffer_id, descriptor_type);
  if (cpu_handle.ptr == 0) {
    logwarn("no cpu_handle {} {}", buffer_config.buffer_index, descriptor_type);
    return false;
  }
  if (resource == nullptr) { return true; }
  return CreateView(device, descriptor_type, buffer_config, resource, cpu_handle);
}
auto CreateCpuHandleWithView(const BufferConfig& buffer_config, const uint32_t buffer_id, ID3D12Resource* resource, DescriptorCpu* descriptor_cpu, D3d12Device* device) {
  bool ret = true;
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagCbv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kCbv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(cbv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagSrv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kSrv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(srv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagUav) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kUav, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(uav) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagRtv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kRtv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(rtv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagDsv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kDsv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(dsv) failed.");
    }
  }
  return ret;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  if (barrier_num == 0) { return; }
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
        barrier.UAV.pResource = resource[i];
        break;
      }
    }
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) { return true; }
  switch (msg) {
    case WM_DESTROY: {
      ::PostQuitMessage(0);
      return 0;
    }
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
} // namespace anonymous
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  MainBufferSize main_buffer_size{};
  MainBufferFormat main_buffer_format{};
  Window window;
  CommandListSet command_list_set;
  Swapchain swapchain;
  CommandQueueSignals command_queue_signals;
  DescriptorCpu descriptor_cpu;
  BufferList buffer_list;
  DescriptorGpu descriptor_gpu;
  PsoRootsigManager pso_rootsig_manager;
  RenderGraph render_graph;
  void** render_pass_vars{nullptr};
  HashMap<uint32_t, MemoryAllocationJanitor> named_buffer_config_index(&allocator);
  HashMap<uint32_t, MemoryAllocationJanitor> named_buffer_allocator_index(&allocator);
  RenderPassFunctionList render_pass_function_list{};
  {
    auto json = GetTestJson();
    ParseRenderGraphJson(json, &allocator, &render_graph);
    render_pass_function_list = PrepareRenderPassFunctions(render_graph.render_pass_num, render_graph.render_pass_list, &allocator);
    CHECK_UNARY(command_list_set.Init(device.Get(),
                                      render_graph.command_queue_num,
                                      render_graph.command_queue_type,
                                      render_graph.command_queue_priority,
                                      render_graph.command_list_num_per_queue,
                                      render_graph.frame_buffer_num,
                                      render_graph.command_allocator_num_per_queue_type));
    CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height, WndProc)); // NOLINT
    CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, render_graph.frame_buffer_num + 1, render_graph.frame_buffer_num, render_graph.swapchain_usage)); // NOLINT
    main_buffer_size = MainBufferSize{
      .swapchain = {
        .width = swapchain.GetWidth(),
        .height = swapchain.GetHeight(),
      },
      .primarybuffer = {
        render_graph.primarybuffer_width,
        render_graph.primarybuffer_height,
      },
    };
    main_buffer_format = MainBufferFormat{
      .swapchain = render_graph.swapchain_format,
      .primarybuffer = render_graph.primarybuffer_format,
    };
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, buffer_allocator, &allocator);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.sampler_num, render_graph.descriptor_handle_num_per_type, &allocator));
    command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
    auto& json_buffer_list = json.at("buffer");
    for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
      const auto buffer_config_index = buffer_list.buffer_config_index[i];
      auto& buffer_config = render_graph.buffer_list[buffer_config_index];
      CHECK_UNARY(CreateCpuHandleWithView(buffer_config, i, buffer_list.resource_list[i], &descriptor_cpu, device.Get()));
      auto str = json_buffer_list[buffer_config_index].at("name").get<std::string>();
      if (buffer_config.pingpong) {
        if (buffer_list.is_sub[i]) {
          SetD3d12Name(buffer_list.resource_list[i], str + "_B");
        } else {
          SetD3d12Name(buffer_list.resource_list[i], str + "_A");
        }
      } else {
        SetD3d12Name(buffer_list.resource_list[i], str);
      }
      if (buffer_config.need_name_cache) {
        auto index = buffer_config_index;
        CHECK_UNARY(named_buffer_config_index.Insert(CalcEntityStrHash(json_buffer_list[buffer_config_index], "name"), std::move(index)));
        index = i;
        CHECK_UNARY(named_buffer_allocator_index.Insert(CalcEntityStrHash(json_buffer_list[buffer_config_index], "name"), std::move(index)));
      }
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(i, DescriptorType::kSampler);
      CHECK_NE(cpu_handler.ptr, 0);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], cpu_handler);
    }
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view, render_graph.gpu_handle_num_sampler));
    CHECK_UNARY(pso_rootsig_manager.Init(json.at("material"), device.Get(), &allocator));
    render_pass_vars = AllocateArray<void*>(&allocator, render_graph.render_pass_num);
    for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
      const auto& render_pass_list_json = json.at("render_pass");
      RenderPassFuncArgsInit render_pass_func_args_init{
        .json =  render_pass_list_json[i].contains("pass_vars") ? &render_pass_list_json[i].at("pass_vars") : nullptr,
        .descriptor_cpu = &descriptor_cpu,
        .device = device.Get(),
        .main_buffer_format = main_buffer_format,
        .hwnd = window.GetHwnd(),
        .frame_buffer_num = render_graph.frame_buffer_num,
        .allocator = &allocator,
        .named_buffer_allocator_index = &named_buffer_allocator_index,
        .named_buffer_config_index = &named_buffer_config_index,
        .buffer_list = &buffer_list,
        .buffer_config_list = render_graph.buffer_list,
        .render_graph = &render_graph,
      };
      render_pass_vars[i] = RenderPassInit(&render_pass_function_list, &render_pass_func_args_init, i);
    }
  }
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
  auto render_pass_signal = AllocateArray<uint64_t>(&allocator, render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_signal[i] = 0UL;
  }
  auto scene_data = GetSceneFromTinyGltfText(GetTestTinyGltf(), "", buffer_allocator, &allocator);
  auto prev_command_list = AllocateArray<D3d12CommandList*>(&allocator, render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
  }
  auto dynamic_data = InitRenderPassDynamicData(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, &allocator);
  for (uint32_t i = 0; i < kFrameLoopNum; i++) {
    if (!window.ProcessMessage()) { break; }
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    const auto frame_index = i % render_graph.frame_buffer_num;
    ConfigurePingPongBufferWriteToSubList(render_graph.render_pass_num, render_graph.render_pass_list, dynamic_data.render_pass_enable_flag, render_graph.buffer_num, dynamic_data.write_to_sub);
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    RegisterResource(*named_buffer_config_index.Get(SID("swapchain")), swapchain.GetResource(), &buffer_list);
    descriptor_cpu.RegisterExternalHandle(*named_buffer_allocator_index.Get(SID("swapchain")), DescriptorType::kRtv, swapchain.GetRtvHandle());
    gpu_descriptor_offset_start[frame_index] = descriptor_gpu.GetViewHandleCount();
    if (gpu_descriptor_offset_start[frame_index] == descriptor_gpu.GetViewHandleTotal()) {
      gpu_descriptor_offset_start[frame_index] = 0;
    }
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
    // set up render pass args
    RenderPassFuncArgsRenderCommon args_common {
      .main_buffer_size = &main_buffer_size,
      .scene_data = &scene_data,
      .frame_index = frame_index,
      .pso_rootsig_manager = &pso_rootsig_manager,
      .buffer_list = &buffer_list,
      .buffer_config_list = render_graph.buffer_list,
      .dynamic_data = &dynamic_data,
      .render_pass_list = render_graph.render_pass_list,
      .device = device.Get(),
      .descriptor_gpu = &descriptor_gpu,
      .descriptor_cpu = &descriptor_cpu,
    };
    auto render_pass_enable_flag = AllocateArray<bool>(&single_frame_allocator, render_graph.render_pass_num);
    auto args_per_pass = AllocateArray<RenderPassFuncArgsRenderPerPass>(&single_frame_allocator, render_graph.render_pass_num);
    uint32_t* barrier_num[kBarrierExecutionTimingNum]{};
    Barrier** barrier_config_list[kBarrierExecutionTimingNum]{};
    ID3D12Resource*** barrier_resource_list[kBarrierExecutionTimingNum]{};
    for (uint32_t k = 0; k < kBarrierExecutionTimingNum; k++) {
      barrier_num[k] = AllocateArray<uint32_t>(&single_frame_allocator, render_graph.render_pass_num);
      barrier_config_list[k] = AllocateArray<Barrier*>(&single_frame_allocator, render_graph.render_pass_num);
      barrier_resource_list[k] = AllocateArray<ID3D12Resource**>(&single_frame_allocator, render_graph.render_pass_num);
    }
    // auto resource_state_list = ConfigureResourceStateChanges(); // TODO
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      render_pass_enable_flag[k] = dynamic_data.render_pass_enable_flag[k];
      const auto& render_pass = render_graph.render_pass_list[k];
      if (!render_pass_enable_flag[k]) { continue; }
      args_per_pass[k].pass_vars_ptr = render_pass_vars[k];
      args_per_pass[k].render_pass_index = k;
      std::tie(args_per_pass[k].resources, args_per_pass[k].cpu_handles) = PrepareResourceCpuHandleList(render_pass, &descriptor_cpu, buffer_list, (const bool**)dynamic_data.write_to_sub, &single_frame_allocator);
      args_per_pass[k].gpu_handles = PrepareGpuHandleList(device.Get(), render_pass, buffer_list, (const bool**)dynamic_data.write_to_sub, descriptor_cpu, &descriptor_gpu, &single_frame_allocator);
      // TODO prepare barrier configs
    }
    // update
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      if (!render_pass_enable_flag[k]) { continue; }
      RenderPassUpdate(&render_pass_function_list, &args_common, &args_per_pass[k]);
    }
    // render
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      if (!render_pass_enable_flag[k]) { continue; }
      const auto& render_pass = render_graph.render_pass_list[k];
      auto render_pass_allocator = GetTemporalMemoryAllocator();
      for (uint32_t l = 0; l < render_pass.wait_pass_num; l++) {
        CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(render_pass.signal_queue_index[l], render_pass.command_queue_index, render_pass_signal[render_pass.signal_pass_index[l]]));
      }
      if (render_pass.prepass_barrier_num == 0 && render_pass.postpass_barrier_num == 0 && !IsRenderPassRenderNeeded(&render_pass_function_list, &args_common, &args_per_pass[k])) { continue; }
      auto command_list = prev_command_list[render_pass.command_queue_index];
      if (command_list == nullptr) {
        command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
        prev_command_list[render_pass.command_queue_index] = command_list;
      }
      args_per_pass[k].command_list = command_list;
      if (command_list && render_graph.command_queue_type[render_pass.command_queue_index] != D3D12_COMMAND_LIST_TYPE_COPY) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, barrier_num[0][k], barrier_config_list[0][k], barrier_resource_list[0][k]);
      RenderPassRender(&render_pass_function_list, &args_common, &args_per_pass[k]);
      ExecuteBarrier(command_list, barrier_num[1][k], barrier_config_list[1][k], barrier_resource_list[1][k]);
      if (render_pass.execute) {
        command_list_set.ExecuteCommandList(render_pass.command_queue_index);
        prev_command_list[render_pass.command_queue_index] = nullptr;
      }
      if (render_pass.sends_signal) {
        assert(render_pass.execute);
        render_pass_signal[k] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
        frame_signals[frame_index][render_pass.command_queue_index] = render_pass_signal[k];
      }
    } // render pass
    gpu_descriptor_offset_end[frame_index] = descriptor_gpu.GetViewHandleCount();
    swapchain.Present();
  }
  command_queue_signals.WaitAll(device.Get());
  ReleaseSceneData(&scene_data);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    RenderPassTerm(&render_pass_function_list, i);
  }
  swapchain.Term();
  pso_rootsig_manager.Term();
  descriptor_gpu.Term();
  descriptor_cpu.Term();
  RegisterResource(*named_buffer_config_index.Get(SID("swapchain")), nullptr, &buffer_list);
  ReleaseBuffers(&buffer_list);
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
