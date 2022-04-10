#include <fstream>
#include <nlohmann/json.hpp>
#include "tiny_gltf.h"
#include "d3d12_barriers.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_material_category.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_scene.h"
#include "d3d12_shader_compiler.h"
#include "d3d12_swapchain.h"
#include "d3d12_view_util.h"
#include "d3d12_win32_window.h"
#include "illuminate/math/math.h"
#include "render_pass/d3d12_render_pass_common.h"
#include "render_pass/d3d12_render_pass_common_util.h"
#include "render_pass/d3d12_render_pass_copy_resource.h"
#include "render_pass/d3d12_render_pass_cs_dispatch.h"
#include "render_pass/d3d12_render_pass_debug_render_selected_buffer.h"
#include "render_pass/d3d12_render_pass_imgui.h"
#include "render_pass/d3d12_render_pass_mesh_transform.h"
#include "render_pass/d3d12_render_pass_postprocess.h"
#include "shader_defines.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
namespace {
static const uint32_t kFrameLoopNum = 100000;
static const uint32_t kInvalidIndex = ~0U;
auto GetTestJson(const char* const filename) {
  std::ifstream file(filename);
  nlohmann::json json;
  file >> json;
  return json;
}
auto AddSystemBuffers(nlohmann::json* json) {
  auto system_buffers = R"(
  [
    {
      "name": "swapchain",
      "descriptor_only": true
    },
    {
      "name": "imgui_font",
      "descriptor_only": true
    },
    {
      "name": "scene_data",
      "descriptor_only": true,
      "frame_buffered": true,
      "descriptor_types": ["cbv"]
    },
    {
      "name": "transforms",
      "descriptor_only": true,
      "descriptor_types": ["srv"]
    }
  ]
  )"_json;
  auto& buffer_list = json->at("buffer");
  for (const auto& s : system_buffers) {
    buffer_list.push_back(s);
  }
}
auto GetRenderPassSwapchainState(const nlohmann::json& render_pass) {
  if (!render_pass.contains("buffer_list")) { return std::make_pair(false, std::string_view()); }
  auto& buffer_list = render_pass.at("buffer_list");
  for (auto& buffer : buffer_list) {
    if (buffer.at("name") == "swapchain") {
      return std::make_pair(true, GetStringView(buffer, "state"));
    }
  }
  return std::make_pair(false, std::string_view());
}
auto AddSystemBarrierJson(nlohmann::json* render_pass, const uint32_t barrier_timing, const std::string_view& buffer_state) {
  const std::string barrier_timing_string = barrier_timing == 0 ? "prepass_barrier" : "postpass_barrier";
  if (!render_pass->contains(barrier_timing_string)) {
    (*render_pass)[barrier_timing_string] = R"([])"_json;
  }
  auto& barriers = render_pass->at(barrier_timing_string);
  barriers.push_back(R"(
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": [],
          "state_after": []
        }
      )"_json);
  if (barrier_timing == 0) {
    barriers.back().at("state_before").push_back("present");
    barriers.back().at("state_after").push_back(buffer_state);
  } else {
    barriers.back().at("state_before").push_back(buffer_state);
    barriers.back().at("state_after").push_back("present");
  }
}
auto AddSystemBarriers(nlohmann::json* json) {
  auto& render_pass_list = json->at("render_pass");
  for (auto& render_pass : render_pass_list) {
    auto [swapchain_found, buffer_state] = GetRenderPassSwapchainState(render_pass);
    if (swapchain_found) {
      AddSystemBarrierJson(&render_pass, 0, buffer_state);
      break;
    }
  }
  for (auto rit = render_pass_list.rbegin(); rit != render_pass_list.rend(); rit++) {
    auto [swapchain_found, buffer_state] = GetRenderPassSwapchainState(*rit);
    if (swapchain_found) {
      AddSystemBarrierJson(&(*rit), 1, buffer_state);
      break;
    }
  }
}
auto PrepareRenderPassFunctions(const uint32_t render_pass_num, const RenderPass* render_pass_list, MemoryAllocationJanitor* allocator) {
  RenderPassFunctionList funcs{};
  funcs.init = AllocateArray<RenderPassFuncInit>(allocator, render_pass_num);
  funcs.term = AllocateArray<RenderPassFuncTerm>(allocator, render_pass_num);
  funcs.update = AllocateArray<RenderPassFuncUpdate>(allocator, render_pass_num);
  funcs.is_render_needed = AllocateArray<RenderPassFuncIsRenderNeeded>(allocator, render_pass_num);
  funcs.render = AllocateArray<RenderPassFuncRender>(allocator, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    funcs.init[i] = nullptr;
    funcs.term[i] = nullptr;
    funcs.update[i] = nullptr;
    funcs.is_render_needed[i] = nullptr;
    funcs.render[i] = nullptr;
    switch (render_pass_list[i].type) {
      case SID("mesh transform"): {
        funcs.init[i] = RenderPassMeshTransform::Init;
        funcs.render[i] = RenderPassMeshTransform::Render;
        break;
      }
      case SID("dispatch cs"): {
        funcs.init[i] = RenderPassCsDispatch::Init;
        funcs.render[i] = RenderPassCsDispatch::Render;
        break;
      }
      case SID("copy resource"): {
        funcs.init[i] = RenderPassCopyResource::Init;
        funcs.update[i] = RenderPassCopyResource::Update;
        funcs.is_render_needed[i] = RenderPassCopyResource::IsRenderNeeded;
        funcs.render[i] = RenderPassCopyResource::Render;
        break;
      }
      case SID("postprocess"): {
        funcs.init[i] = RenderPassPostprocess::Init;
        funcs.update[i] = RenderPassPostprocess::Update;
        funcs.render[i] = RenderPassPostprocess::Render;
        break;
      }
      case SID("imgui"): {
        funcs.init[i] = RenderPassImgui::Init;
        funcs.term[i] = RenderPassImgui::Term;
        funcs.update[i] = RenderPassImgui::Update;
        funcs.render[i] = RenderPassImgui::Render;
        break;
      }
      case SID("debug buffer"): {
        funcs.render[i] = RenderPassDebugRenderSelectedBuffer::Render;
        break;
      }
      default: {
        logerror("render pass function type not registered. {}", render_pass_list[i].type);
        assert(false && "render pass function type not registered.");
        break;
      }
    }
  }
  return funcs;
}
auto PrepareResourceCpuHandleList(const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, const BufferList& buffer_list, const bool** write_to_sub, const BufferConfig* buffer_config_list, const uint32_t frame_index, MemoryAllocationJanitor* allocator) {
  auto resource_list = AllocateArray<ID3D12Resource*>(allocator, render_pass.buffer_num);
  auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(allocator, render_pass.buffer_num);
  for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
    auto& buffer = render_pass.buffer_list[b];
    const auto& buffer_config = buffer_config_list[buffer.buffer_index];
    const auto local_index = GetBufferLocalIndex(buffer_config, buffer.state, write_to_sub[buffer.buffer_index][render_pass.index], frame_index);
    const auto buffer_allocation_index = GetBufferAllocationIndex(buffer_list, buffer.buffer_index, local_index);
    resource_list[b] = buffer_list.resource_list[buffer_allocation_index];
    const auto descriptor_type = ConvertToDescriptorType(buffer.state);
    if (descriptor_type != DescriptorType::kNum) {
      cpu_handle_list[b].ptr = descriptor_cpu->GetHandle(buffer_allocation_index, descriptor_type).ptr;
    } else {
      cpu_handle_list[b].ptr = 0;
    }
  }
  return std::make_tuple(resource_list, cpu_handle_list);
}
auto PrepareGpuHandleList(D3d12Device* device, const RenderPass& render_pass, const BufferList& buffer_list, const bool** write_to_sub, const BufferConfig* buffer_config_list, const uint32_t frame_index, const DescriptorCpu& descriptor_cpu, DescriptorGpu* const descriptor_gpu, MemoryAllocationJanitor* allocator) {
  auto gpu_handle_list = AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, 2); // 0:view 1:sampler
  {
    // view
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto buffer_id_list = AllocateArray<uint32_t>(&tmp_allocator, render_pass.buffer_num);
    auto descriptor_type_list = AllocateArray<DescriptorType>(&tmp_allocator, render_pass.buffer_num);
    for (uint32_t j = 0; j < render_pass.buffer_num; j++) {
      const auto buffer_index = render_pass.buffer_list[j].buffer_index;
      const auto local_index = GetBufferLocalIndex(buffer_config_list[buffer_index], render_pass.buffer_list[j].state, write_to_sub[buffer_index][render_pass.index], frame_index);
      buffer_id_list[j] = GetBufferAllocationIndex(buffer_list, buffer_index, local_index);
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
  if (buffer_config.descriptor_only) { return true; }
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
auto PrepareBarrierResourceList(const uint32_t render_pass_num, const uint32_t** barrier_num, const Barrier*** barrier_config_list, const BufferList& buffer_list, MemoryAllocationJanitor* allocator) {
  auto barrier_resource_list = AllocateArray<ID3D12Resource***>(allocator, kBarrierExecutionTimingNum);
  for (uint32_t i = 0; i < kBarrierExecutionTimingNum; i++) {
    barrier_resource_list[i] = AllocateArray<ID3D12Resource**>(allocator, render_pass_num);
    for (uint32_t j = 0; j < render_pass_num; j++) {
      barrier_resource_list[i][j] = (barrier_num[i][j] == 0) ? nullptr : AllocateArray<ID3D12Resource*>(allocator, barrier_num[i][j]);
      for (uint32_t k = 0; k < barrier_num[i][j]; k++) {
        auto& barrier_config = barrier_config_list[i][j][k];
        const auto buffer_allocation_index = GetBufferAllocationIndex(buffer_list, barrier_config.buffer_index, barrier_config.local_index);
        barrier_resource_list[i][j][k] = buffer_list.resource_list[buffer_allocation_index];
      }
    }
  }
  return barrier_resource_list;
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
auto GetSceneData(D3D12MA::Allocator* buffer_allocator, BufferList* buffer_list, MemoryAllocationJanitor* allocator, const uint32_t transform_buffer_allocation_index, DescriptorCpu* descriptor_cpu, D3d12Device* device) {
  auto scene_data = GetSceneFromTinyGltfBinary("scenedata/Box.glb", buffer_allocator, allocator);
  auto resource = scene_data.buffer_allocation_default[scene_data.transform_buffer_allocation_index].resource;
  RegisterResource(transform_buffer_allocation_index, resource, buffer_list);
  auto cpu_handle = descriptor_cpu->GetHandle(transform_buffer_allocation_index, DescriptorType::kSrv);
  auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
    .Format = DXGI_FORMAT_UNKNOWN,
    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Buffer = {
      .FirstElement = 0,
      .NumElements = scene_data.transform_element_num,
      .StructureByteStride = scene_data.transform_buffer_stride_size,
      .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
    },
  };
  device->CreateShaderResourceView(resource, &desc, cpu_handle);
  SetD3d12Name(resource, "transforms");
  return scene_data;
}
auto PrepareSceneCbvBuffer(D3D12MA::Allocator* buffer_allocator, BufferList* buffer_list, const uint32_t frame_buffer_num, const uint32_t scene_cbv_buffer_config_index, DescriptorCpu* descriptor_cpu, MemoryAllocationJanitor* allocator, D3d12Device* device) {
  const auto buffer_size = GetUint32(sizeof(shader::SceneCbvData));
  const auto cbv_buffer_size = AlignAddress(buffer_size, 256); // cbv size must be multiple of 256
  auto resource_desc = GetD3d12ResourceDescForBuffer(cbv_buffer_size);
  auto ptr_list = AllocateArray<void*>(allocator, frame_buffer_num);
  for (uint32_t i = 0; i < frame_buffer_num; i++) {
    auto upload_buffer = CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, resource_desc, nullptr, buffer_allocator);
    const auto scene_cbv_buffer_allocation_index = GetBufferAllocationIndex(*buffer_list, scene_cbv_buffer_config_index, i);
    RegisterBufferAllocation(scene_cbv_buffer_allocation_index, upload_buffer, buffer_list);
    auto cpu_handle = descriptor_cpu->GetHandle(scene_cbv_buffer_allocation_index, DescriptorType::kCbv);
    D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc{
      .BufferLocation = upload_buffer.resource->GetGPUVirtualAddress(),
      .SizeInBytes = cbv_buffer_size,
    };
    device->CreateConstantBufferView(&view_desc, cpu_handle);
    SetD3d12Name(upload_buffer.resource, "scenecbv" + std::to_string(i));
    ptr_list[i] = MapResource(upload_buffer.resource, buffer_size);
  }
  return ptr_list;
}
void UpdateSceneCbv(const RenderPassConfigDynamicData& dynamic_data, const Size2d& buffer_size, void *scene_cbv_ptr) {
  matrix view_matrix{}, projection_matrix{};
  GetLookAtLH(dynamic_data.camera_pos, dynamic_data.camera_focus, {0.0f, 1.0f, 0.0f,}, view_matrix);
  const auto aspect_ratio = static_cast<float>(buffer_size.width) / buffer_size.height;
  GetPerspectiveProjectionMatirxLH(ToRadian(dynamic_data.fov_vertical), aspect_ratio, dynamic_data.near_z, dynamic_data.far_z, projection_matrix);
  shader::SceneCbvData scene_cbv{};
  MultiplyMatrix(view_matrix, projection_matrix, scene_cbv.view_projection_matrix);
  TransposeMatrix(scene_cbv.view_projection_matrix);
  memcpy(scene_cbv_ptr, &scene_cbv, sizeof(scene_cbv));
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
  RenderPassFunctionList render_pass_function_list{};
  uint32_t swapchain_buffer_allocation_index{kInvalidIndex};
  uint32_t transform_buffer_allocation_index{kInvalidIndex};
  uint32_t scene_cbv_buffer_config_index{kInvalidIndex};
  uint32_t material_num{0};
  StrHash* material_str_hash_list{nullptr};
  MaterialCategoryList* material_category_list{nullptr};
#ifdef USE_GRAPHICS_DEBUG_SCOPE
  char** render_pass_name{nullptr};
#endif
  auto frame_loop_num = kFrameLoopNum;
  {
    auto material_json = GetTestJson("material.json");
    CHECK_UNARY(pso_rootsig_manager.Init(material_json, device.Get(), &allocator));
    material_num = CreateMaterialStrHashList(material_json, &material_str_hash_list, &allocator);
    CreateMaterialCategoryList(material_json, GetTestJson("material_category.json"), &allocator, material_category_list);
    nlohmann::json json;
    SUBCASE("config.json") {
      json = GetTestJson("config.json");
      frame_loop_num = 5;
    }
    SUBCASE("forward.json") {
      json = GetTestJson("forward.json");
      frame_loop_num = 5;
    }
    AddSystemBuffers(&json);
    AddSystemBarriers(&json);
    ParseRenderGraphJson(json, material_num, material_str_hash_list, &allocator, &render_graph);
    render_pass_function_list = PrepareRenderPassFunctions(render_graph.render_pass_num, render_graph.render_pass_list, &allocator);
    CHECK_UNARY(command_list_set.Init(device.Get(),
                                      render_graph.command_queue_num,
                                      render_graph.command_queue_type,
                                      render_graph.command_queue_priority,
                                      render_graph.command_list_num_per_queue,
                                      render_graph.frame_buffer_num,
                                      render_graph.command_allocator_num_per_queue_type));
    {
      const auto& command_queue_name_json = json.at("command_queue");
      for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
        SetD3d12Name(command_list_set.GetCommandQueue(i), GetStringView(command_queue_name_json[i], "name"));
      }
    }
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
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, render_graph.frame_buffer_num, buffer_allocator, &allocator);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.sampler_num, render_graph.descriptor_handle_num_per_type, &allocator));
    command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
    auto& json_buffer_list = json.at("buffer");
    for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
      const auto buffer_config_index = buffer_list.buffer_config_index[i];
      auto& buffer_config = render_graph.buffer_list[buffer_config_index];
      CHECK_UNARY(CreateCpuHandleWithView(buffer_config, i, buffer_list.resource_list[i], &descriptor_cpu, device.Get()));
      auto str = json_buffer_list[buffer_config_index].at("name").get<std::string>();
      if (buffer_config.pingpong) {
        if (IsPingPongMainBuffer(buffer_list, buffer_config_index, i)) {
          SetD3d12Name(buffer_list.resource_list[i], str + "_A");
        } else {
          SetD3d12Name(buffer_list.resource_list[i], str + "_B");
        }
      } else if (buffer_config.frame_buffered) {
        SetD3d12Name(buffer_list.resource_list[i], str + std::to_string(GetBufferLocalIndex(buffer_list, buffer_config_index, i)));
      } else if (!buffer_config.descriptor_only) {
        SetD3d12Name(buffer_list.resource_list[i], str);
      }
      if (str.compare("swapchain") == 0) {
        swapchain_buffer_allocation_index = i;
      }
      if (str.compare("transforms") == 0) {
        transform_buffer_allocation_index = i;
      }
      if (scene_cbv_buffer_config_index == kInvalidIndex && str.compare("scene_data") == 0) {
        scene_cbv_buffer_config_index = buffer_config_index;
      }
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(i, DescriptorType::kSampler);
      CHECK_NE(cpu_handler.ptr, 0);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], cpu_handler);
    }
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view, render_graph.gpu_handle_num_sampler));
    render_pass_vars = AllocateArray<void*>(&allocator, render_graph.render_pass_num);
    render_pass_name = AllocateArray<char*>(&allocator, render_graph.render_pass_num);
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
        .buffer_list = &buffer_list,
        .buffer_config_list = render_graph.buffer_list,
        .render_pass_num = render_graph.render_pass_num,
        .render_pass_list = render_graph.render_pass_list,
      };
      render_pass_vars[i] = RenderPassInit(&render_pass_function_list, &render_pass_func_args_init, i);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      auto pass_name = GetStringView(render_pass_list_json[i], ("name"));
      const auto str_len = GetUint32(pass_name.size())  + 1;
      render_pass_name[i] = AllocateArray<char>(&allocator, str_len);
      strcpy_s(render_pass_name[i], str_len, GetStringView(pass_name).data());
#endif
    }
  }
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  auto render_pass_signal = AllocateArray<uint64_t>(&allocator, render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_signal[i] = 0UL;
  }
  auto scene_data = GetSceneData(buffer_allocator, &buffer_list, &allocator, transform_buffer_allocation_index, &descriptor_cpu, device.Get());
  auto prev_command_list = AllocateArray<D3d12CommandList*>(&allocator, render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
  }
  auto dynamic_data = InitRenderPassDynamicData(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, &allocator);
  auto scene_cbv_ptr = PrepareSceneCbvBuffer(buffer_allocator, &buffer_list, render_graph.frame_buffer_num, scene_cbv_buffer_config_index, &descriptor_cpu, &allocator, device.Get());
  for (uint32_t i = 0; i < frame_loop_num; i++) {
    if (!window.ProcessMessage()) { break; }
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    auto command_queue_frame_signal_sent = AllocateArray<bool>(&single_frame_allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      command_queue_frame_signal_sent[j] = true;
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
    UpdateSceneCbv(dynamic_data, main_buffer_size.primarybuffer, scene_cbv_ptr[frame_index]);
    ConfigurePingPongBufferWriteToSubList(render_graph.render_pass_num, render_graph.render_pass_list, dynamic_data.render_pass_enable_flag, render_graph.buffer_num, dynamic_data.write_to_sub);
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    RegisterResource(swapchain_buffer_allocation_index, swapchain.GetResource(), &buffer_list);
    descriptor_cpu.RegisterExternalHandle(swapchain_buffer_allocation_index, DescriptorType::kRtv, swapchain.GetRtvHandle());
    // set up render pass args
    auto render_pass_enable_flag = AllocateArray<bool>(&single_frame_allocator, render_graph.render_pass_num);
    auto args_per_pass = AllocateArray<RenderPassFuncArgsRenderPerPass>(&single_frame_allocator, render_graph.render_pass_num);
    auto [resource_state_list, last_user_pass] = ConfigureRenderPassResourceStates(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, render_graph.buffer_list, (const bool**)dynamic_data.write_to_sub, (const bool*)dynamic_data.render_pass_enable_flag, &single_frame_allocator);
    auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, render_graph.buffer_list, resource_state_list, last_user_pass, &single_frame_allocator);
    auto barrier_resource_list = PrepareBarrierResourceList(render_graph.render_pass_num, barrier_num, barrier_config_list, buffer_list, &single_frame_allocator);
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      render_pass_enable_flag[k] = dynamic_data.render_pass_enable_flag[k];
      const auto& render_pass = render_graph.render_pass_list[k];
      if (!render_pass_enable_flag[k]) { continue; }
      args_per_pass[k].pass_vars_ptr = render_pass_vars[k];
      args_per_pass[k].render_pass_index = k;
      std::tie(args_per_pass[k].resources, args_per_pass[k].cpu_handles) = PrepareResourceCpuHandleList(render_pass, &descriptor_cpu, buffer_list, (const bool**)dynamic_data.write_to_sub, render_graph.buffer_list, frame_index, &single_frame_allocator);
      args_per_pass[k].gpu_handles = PrepareGpuHandleList(device.Get(), render_pass, buffer_list, (const bool**)dynamic_data.write_to_sub, render_graph.buffer_list, frame_index, descriptor_cpu, &descriptor_gpu, &single_frame_allocator);
    }
    // update
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
      .resource_state_list = resource_state_list,
      .material_category_list = material_category_list,
    };
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      if (!render_pass_enable_flag[k]) { continue; }
      RenderPassUpdate(&render_pass_function_list, &args_common, &args_per_pass[k]);
    }
    // render
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      if (!render_pass_enable_flag[k]) { continue; }
      const auto& render_pass = render_graph.render_pass_list[k];
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
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      PIXBeginEvent(command_list, 0, render_pass_name[k]); // https://devblogs.microsoft.com/pix/winpixeventruntime/
#endif
      if (command_list && render_graph.command_queue_type[render_pass.command_queue_index] != D3D12_COMMAND_LIST_TYPE_COPY) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, barrier_num[0][k], barrier_config_list[0][k], barrier_resource_list[0][k]);
      RenderPassRender(&render_pass_function_list, &args_common, &args_per_pass[k]);
      ExecuteBarrier(command_list, barrier_num[1][k], barrier_config_list[1][k], barrier_resource_list[1][k]);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      PIXEndEvent(command_list);
#endif
      if (render_pass.execute) {
        command_list_set.ExecuteCommandList(render_pass.command_queue_index);
        prev_command_list[render_pass.command_queue_index] = nullptr;
      }
      if (render_pass.sends_signal) {
        assert(render_pass.execute);
        render_pass_signal[k] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
        frame_signals[frame_index][render_pass.command_queue_index] = render_pass_signal[k];
      }
      command_queue_frame_signal_sent[render_pass.command_queue_index] = render_pass.sends_signal;
    } // render pass
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      if (!command_queue_frame_signal_sent[j]) {
        auto signal_val = command_queue_signals.SucceedSignal(j);
        frame_signals[frame_index][j] = signal_val;
      }
    }
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
  RegisterResource(swapchain_buffer_allocation_index, nullptr, &buffer_list);
  RegisterResource(transform_buffer_allocation_index, nullptr, &buffer_list);
  ReleaseBuffers(&buffer_list);
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
