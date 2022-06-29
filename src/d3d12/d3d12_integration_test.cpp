#include <fstream>
#include "imgui.h"
#include <nlohmann/json.hpp>
#include "SimpleMath.h"
#include "tiny_gltf.h"
#include "d3d12_barriers.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_resource_transfer.h"
#include "d3d12_scene.h"
#include "d3d12_shader_compiler.h"
#include "d3d12_swapchain.h"
#include "d3d12_texture_util.h"
#include "d3d12_view_util.h"
#include "d3d12_win32_window.h"
#include "illuminate/math/math.h"
#include "illuminate/util/util_functions.h"
#include "render_pass/d3d12_render_pass_common.h"
#include "render_pass/d3d12_render_pass_copy_resource.h"
#include "render_pass/d3d12_render_pass_cs_dispatch.h"
#include "render_pass/d3d12_render_pass_imgui.h"
#include "render_pass/d3d12_render_pass_mesh_transform.h"
#include "render_pass/d3d12_render_pass_postprocess.h"
#include "render_pass/d3d12_render_pass_util.h"
#include "shader_defines.h"
#define FORCE_SRV_FOR_ALL
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
namespace {
static const uint32_t kFrameLoopNum = 100000;
static const uint32_t kInvalidIndex = ~0U;
static const uint32_t kExtraDescriptorHandleNumCbvSrvUav = 1; // imgui font
static const uint32_t kImguiGpuHandleIndex = 0;
static const uint32_t kSceneGpuHandleIndex = 1;
enum SystemBuffer : uint32_t {
  kSystemBufferCamera = 0,
  kSystemBufferLight,
  kSystemBufferNum,
};
const char* kSystemBufferNames[] = {
  "camera",
  "light",
};
const uint32_t kSystemBufferSize[] = {
  AlignAddress(GetUint32(sizeof(shader::SceneCameraData)), 256),
  AlignAddress(GetUint32(sizeof(shader::SceneLightData)), 256),
};
auto GetTestJson(const char* const filename) {
  std::ifstream file(filename);
  nlohmann::json json;
  file >> json;
  return json;
}
auto AddSystemBuffers(nlohmann::json* json) {
  auto& buffer_list = json->at("buffer");
  {
    auto buffer_json = R"(
    {
      "name": "swapchain",
      "descriptor_only": true
    }
    )"_json;
    buffer_list.push_back(buffer_json);
  }
  {
    static_assert(std::size(kSystemBufferNames) == kSystemBufferNum);
    static_assert(std::size(kSystemBufferSize) == kSystemBufferNum);
    auto buffer_json = R"(
    {
      "frame_buffered": true,
      "format": "UNKNOWN",
      "heap_type": "upload",
      "dimension": "buffer",
      "size_type": "absolute",
      "initial_state": "generic_read"
    }
    )"_json;
    for (uint32_t i = 0; i < kSystemBufferNum; i++) {
      buffer_json["name"]  = kSystemBufferNames[i];
      buffer_json["width"] = kSystemBufferSize[i];
      buffer_list.push_back(buffer_json);
    }
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
    funcs.init[i]             = nullptr;
    funcs.term[i]             = nullptr;
    funcs.update[i]           = nullptr;
    funcs.is_render_needed[i] = nullptr;
    funcs.render[i]           = nullptr;
    switch (render_pass_list[i].type) {
      case SID("mesh transform"): {
        funcs.init[i]             = RenderPassMeshTransform::Init;
        funcs.render[i]           = RenderPassMeshTransform::Render;
        break;
      }
      case SID("dispatch cs"): {
        funcs.init[i]             = RenderPassCsDispatch::Init;
        funcs.render[i]           = RenderPassCsDispatch::Render;
        break;
      }
      case SID("copy resource"): {
        funcs.render[i]           = RenderPassCopyResource::Render;
        break;
      }
      case SID("postprocess"): {
        funcs.init[i]             = RenderPassPostprocess::Init;
        funcs.update[i]           = RenderPassPostprocess::Update;
        funcs.render[i]           = RenderPassPostprocess::Render;
        break;
      }
      case SID("imgui"): {
        funcs.render[i]           = RenderPassImgui::Render;
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
auto PrepareResourceCpuHandleList(const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, const BufferList& buffer_list, const bool** write_to_sub, const BufferConfig* buffer_config_list, const uint32_t frame_index, const D3D12_CPU_DESCRIPTOR_HANDLE* scene_cpu_handles, MemoryAllocationJanitor* allocator) {
  auto resource_list = AllocateArray<ID3D12Resource*>(allocator, render_pass.buffer_num);
  auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(allocator, render_pass.buffer_num);
  for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
    const auto& buffer = render_pass.buffer_list[b];
    if (IsSceneBuffer(buffer.buffer_index)) {
      if (!IsGpuHandleAvailableType(buffer.state)) {
        logwarn("invalid state for scene buffer when searching buffer handle {} {} {}", b, buffer.buffer_index, buffer.state);
      }
      cpu_handle_list[b].ptr = scene_cpu_handles[GetDecodedSceneBufferIndex(buffer.buffer_index)].ptr;
      continue;
    }
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
    logtrace("cpu handle/pass:{} b:{} config:{} local:{} alloc:{} desc:{} ptr:{:x}", render_pass.index, b, buffer.buffer_index, local_index, buffer_allocation_index, descriptor_type, cpu_handle_list[b].ptr);
  }
  return std::make_tuple(resource_list, cpu_handle_list);
}
auto PrepareGpuHandlesViewList(D3d12Device* device, const RenderPass& render_pass, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle_list, DescriptorGpu* const descriptor_gpu, const D3D12_CPU_DESCRIPTOR_HANDLE& texture_list_cpu_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& texture_list_gpu_handle, MemoryAllocationJanitor* allocator) {
  if (render_pass.buffer_num == 0) { return (D3D12_GPU_DESCRIPTOR_HANDLE*)nullptr; }
  const auto view_list_num = render_pass.max_buffer_index_offset + 1;
  auto gpu_handle_list = AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, view_list_num);
  std::fill(gpu_handle_list, gpu_handle_list + view_list_num, D3D12_GPU_DESCRIPTOR_HANDLE{});
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto desc_num_list = AllocateArray<uint32_t>(&tmp_allocator, view_list_num);
  std::fill(desc_num_list, desc_num_list + view_list_num, 0);
  for (uint32_t i = 0; i < render_pass.buffer_num; i++) {
    if (!IsGpuHandleAvailableType(render_pass.buffer_list[i].state)) { continue; }
    desc_num_list[render_pass.buffer_list[i].index_offset]++;
  }
  auto copy_src_cpu_handle_num = AllocateArray<uint32_t>(&tmp_allocator, view_list_num);
  std::fill(copy_src_cpu_handle_num, copy_src_cpu_handle_num + view_list_num, 0);
  auto copy_src_cpu_handles = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE*>(&tmp_allocator, view_list_num);
  for (uint32_t i = 0; i < view_list_num; i++) {
    copy_src_cpu_handles[i] = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, desc_num_list[i]);
  }
  for (uint32_t i = 0; i < render_pass.buffer_num; i++) {
    const auto& buffer = render_pass.buffer_list[i];
    if (!IsGpuHandleAvailableType(buffer.state)) { continue; }
    assert(gpu_handle_list[buffer.index_offset].ptr == 0UL);
    if (cpu_handle_list[i].ptr == texture_list_cpu_handle.ptr) {
      assert(copy_src_cpu_handle_num[buffer.index_offset] == 0);
      gpu_handle_list[buffer.index_offset].ptr = texture_list_gpu_handle.ptr;
      continue;
    }
    copy_src_cpu_handles[buffer.index_offset][copy_src_cpu_handle_num[buffer.index_offset]].ptr = cpu_handle_list[i].ptr;
    copy_src_cpu_handle_num[buffer.index_offset]++;
  }
  for (uint32_t i = 0; i < view_list_num; i++) {
    if (gpu_handle_list[i].ptr != 0UL) { continue; }
    assert(copy_src_cpu_handle_num[i] == desc_num_list[i]);
    gpu_handle_list[i].ptr = descriptor_gpu->WriteToTransientViewHandleRange(copy_src_cpu_handle_num[i], copy_src_cpu_handles[i], device).ptr;
  }
  return gpu_handle_list;
}
auto PrepareGpuHandlesSamplerList(D3d12Device* device, const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, DescriptorGpu* descriptor_gpu, const D3D12_GPU_DESCRIPTOR_HANDLE& scene_sampler, MemoryAllocationJanitor* allocator) {
  if (render_pass.sampler_num == 0) { return (D3D12_GPU_DESCRIPTOR_HANDLE*)nullptr; }
  bool need_scene_sampler = false;
  for (uint32_t s = 0; s < render_pass.sampler_num; s++) {
    if (render_pass.sampler_index_list[s] == kSceneSamplerId) {
      need_scene_sampler = true;
      break;
    }
  }
  const uint32_t sampler_list_num = (need_scene_sampler && render_pass.sampler_num > 1) ? 2 : 1;
  auto gpu_handle_list = AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, sampler_list_num);
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, render_pass.sampler_num);
  uint32_t sampler_index = 0;
  for (uint32_t s = 0; s < render_pass.sampler_num; s++) {
    if (render_pass.sampler_index_list[s] == kSceneSamplerId) { continue; }
    cpu_handle_list[sampler_index].ptr = descriptor_cpu->GetHandle(render_pass.sampler_index_list[s], DescriptorType::kSampler).ptr;
    sampler_index++;
  }
  std::fill(gpu_handle_list, gpu_handle_list + sampler_list_num, D3D12_GPU_DESCRIPTOR_HANDLE{});
  if (sampler_index > 0) {
    gpu_handle_list[0] = descriptor_gpu->WriteToTransientSamplerHandleRange(render_pass.sampler_num, cpu_handle_list, device);
  }
  if (need_scene_sampler) {
    const uint32_t index = render_pass.sampler_num > 1 ? 1 : 0;
    gpu_handle_list[index].ptr = scene_sampler.ptr;
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
auto PrepareSceneCbvBuffer(BufferList* buffer_list, const uint32_t frame_buffer_num, const uint32_t scene_cbv_buffer_config_index, const uint32_t buffer_size, MemoryAllocationJanitor* allocator) {
  auto ptr_list = AllocateArray<void*>(allocator, frame_buffer_num);
  for (uint32_t i = 0; i < frame_buffer_num; i++) {
    ptr_list[i] = MapResource(GetResource(*buffer_list, scene_cbv_buffer_config_index, i), buffer_size);
  }
  return ptr_list;
}
auto SetValue(const DirectX::SimpleMath::Vector3& src, shader::float3* dst) {
  dst->x = src.x;
  dst->y = src.y;
  dst->z = src.z;
}
void UpdateSceneBuffer(const RenderPassConfigDynamicData& dynamic_data, const Size2d& buffer_size, void *scene_camera_ptr, void* scene_light_ptr) {
  using namespace DirectX::SimpleMath;
  const auto view_matrix = Matrix::CreateLookAt(Vector3(dynamic_data.camera_pos), Vector3(dynamic_data.camera_focus), Vector3::Up);
  const auto aspect_ratio = static_cast<float>(buffer_size.width) / buffer_size.height;
  const auto projection_matrix = Matrix::CreatePerspectiveFieldOfView(ToRadian(dynamic_data.fov_vertical), aspect_ratio, dynamic_data.near_z, dynamic_data.far_z);
  shader::SceneCameraData scene_camera{};
  CopyMatrix(view_matrix.m, scene_camera.view_matrix);
  CopyMatrix(projection_matrix.m, scene_camera.projection_matrix);
  memcpy(scene_camera_ptr, &scene_camera, sizeof(scene_camera));
  auto light_direction = Vector3::TransformNormal(Vector3(dynamic_data.light_direction), view_matrix);
  light_direction.Normalize();
  shader::SceneLightData scene_light{};
  scene_light.light_color = {dynamic_data.light_color[0], dynamic_data.light_color[1], dynamic_data.light_color[2], dynamic_data.light_intensity};
  scene_light.light_direction_vs = {light_direction.x, light_direction.y, light_direction.z};
  scene_light.exposure_rate = 4.0f / dynamic_data.light_intensity;
  memcpy(scene_light_ptr, &scene_light, sizeof(scene_light));
}
auto GetRenderPassQueueIndexList(const uint32_t render_pass_num, const RenderPass* render_pass_list, MemoryAllocationJanitor* allocator) {
  auto render_pass_queue_index = AllocateArray<uint32_t>(allocator, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_queue_index[i] = render_pass_list[i].command_queue_index;
  }
  return render_pass_queue_index;
}
auto GetRenderPassIndexPerQueue(const uint32_t command_queue_num, const uint32_t render_pass_num, const uint32_t* render_pass_queue_index, MemoryAllocationJanitor* allocator) {
  auto render_pass_num_per_queue = AllocateArray<uint32_t>(allocator, command_queue_num);
  auto render_pass_index_per_queue = AllocateArray<uint32_t>(allocator, render_pass_num);
  std::fill(render_pass_num_per_queue, render_pass_num_per_queue + command_queue_num, 0);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_index_per_queue[i] = render_pass_num_per_queue[render_pass_queue_index[i]];
    render_pass_num_per_queue[render_pass_queue_index[i]]++;
  }
  return std::make_pair(render_pass_num_per_queue, render_pass_index_per_queue);
}
auto GetLastPassPerQueue(const uint32_t command_queue_num, const uint32_t render_pass_num, const uint32_t* render_pass_queue_index, MemoryAllocationJanitor* allocator) {
  auto last_pass_per_queue = AllocateArray<uint32_t>(allocator, command_queue_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    last_pass_per_queue[render_pass_queue_index[i]] = i;
  }
  return last_pass_per_queue;
}
auto CreateTimestampQueryHeaps(const uint32_t command_queue_num, [[maybe_unused]]const D3D12_COMMAND_LIST_TYPE* command_queue_type, const uint32_t* render_pass_num_per_queue, const uint32_t query_num_per_pass, D3d12Device* device, MemoryAllocationJanitor* allocator) {
  auto timestamp_query_heaps = AllocateArray<ID3D12QueryHeap*>(allocator, command_queue_num);
  D3D12_QUERY_HEAP_DESC desc{};
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (render_pass_num_per_queue[i] == 0) {
      timestamp_query_heaps[i] = nullptr;
      continue;
    }
#ifdef NDEBUG
    desc.Type = (command_queue_type[i] == D3D12_COMMAND_LIST_TYPE_COPY) ? D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
#else
    if (IsDebuggerPresent()) {
      // D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP issues error even on D3D12_COMMAND_LIST_TYPE_COPY when graphics debugger is enabled.
      desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    } else {
      desc.Type = (command_queue_type[i] == D3D12_COMMAND_LIST_TYPE_COPY) ? D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    }
#endif
    desc.Count = render_pass_num_per_queue[i] * query_num_per_pass;
    auto hr = device->CreateQueryHeap(&desc, IID_PPV_ARGS(&timestamp_query_heaps[i]));
    if (FAILED(hr)) {
      assert(timestamp_query_heaps[i] == nullptr);
      logwarn("CreateQueryHeap failed. {} {} {} {}", desc.Type, desc.Count, i, hr);
    }
  }
  return timestamp_query_heaps;
}
auto CreateTimestampQuertyDstResource(const uint32_t command_queue_num, const uint32_t* render_pass_num_per_queue, const uint32_t timestamp_query_dst_resource_num, D3D12MA::Allocator* buffer_allocator, MemoryAllocationJanitor* allocator) {
  auto timestamp_query_dst_resource = AllocateArray<ID3D12Resource**>(allocator, command_queue_num);
  auto timestamp_query_dst_allocator = AllocateArray<D3D12MA::Allocation**>(allocator, command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (render_pass_num_per_queue[i] == 0) {
      timestamp_query_dst_resource[i]  = nullptr;
      timestamp_query_dst_allocator[i] = nullptr;
      continue;
    }
    timestamp_query_dst_resource[i]  = AllocateArray<ID3D12Resource*>(allocator, timestamp_query_dst_resource_num);
    timestamp_query_dst_allocator[i] = AllocateArray<D3D12MA::Allocation*>(allocator, timestamp_query_dst_resource_num);
    const auto desc = GetBufferDesc(GetUint32(sizeof(uint64_t)) * render_pass_num_per_queue[i] * timestamp_query_dst_resource_num);
    for (uint32_t j = 0; j < timestamp_query_dst_resource_num; j++) {
      CreateBuffer(D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, desc, nullptr, buffer_allocator, &timestamp_query_dst_allocator[i][j], &timestamp_query_dst_resource[i][j]);
    }
  }
  return std::make_pair(timestamp_query_dst_resource, timestamp_query_dst_allocator);
}
auto InitImgui(HWND hwnd, D3d12Device* device, const uint32_t frame_buffer_num, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE& imgui_font_cpu_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& imgui_font_gpu_handle) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device, frame_buffer_num, format, nullptr/*descriptor heap not used in single viewport mode*/, imgui_font_cpu_handle, imgui_font_gpu_handle);
  if (!ImGui_ImplDX12_CreateDeviceObjects()) {
    logerror("ImGui_ImplDX12_CreateDeviceObjects failed.");
    assert(false);
  }
}
auto TermImgui() {
  ImPlot::DestroyContext();
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}
struct TimeDurationDataSet {
  float frame_count_reset_time_threshold_msec{250.0f};
  uint32_t frame_count{0U};
  std::chrono::high_resolution_clock::time_point last_time_point{std::chrono::high_resolution_clock::now()};
  float delta_time_msec{0.0f};
  float duration_msec_sum{0.0f};
  float prev_duration_per_frame_msec_avg{0.0f};
};
auto RegisterGuiPerformance(const TimeDurationDataSet& time_duration_data_set) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (!ImGui::Begin("performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::Text("CPU:%f", time_duration_data_set.prev_duration_per_frame_msec_avg);
  {
    auto left_top = ImGui::GetWindowPos();
    ImVec2 right_bottom(left_top.x + 200.0f, left_top.y + 100.0f);
    auto color = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    // ImGui::GetWindowDrawList()->AddRectFilled(left_top, right_bottom, color);
    // ImGui::SetNextWindowPos(ImVec2(left_top.x, right_bottom.y));
  }
  ImGui::End();
}
auto RegisterGuiCamera(RenderPassConfigDynamicData* dynamic_data) {
  if (!ImGui::Begin("camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::SetNextWindowSize(ImVec2{});
  ImGui::SliderFloat3("camera pos", dynamic_data->camera_pos, -10.0f, 10.0f, "%.1f");
  ImGui::SliderFloat3("camera focus", dynamic_data->camera_focus, -10.0f, 10.0f, "%.1f");
  ImGui::SliderFloat("camera fov(vertical)", &dynamic_data->fov_vertical, 1.0f, 360.0f, "%.0f");
  ImGui::SliderFloat("near_z", &dynamic_data->near_z, 0.001f, 1.0f, "%.2f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
  ImGui::SliderFloat("far_z", &dynamic_data->far_z, 1.0f, 10000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
  ImGui::End();
}
auto RegisterGuiLight(RenderPassConfigDynamicData* dynamic_data) {
  if (!ImGui::Begin("light", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::SetNextWindowSize(ImVec2{});
  ImGui::SliderFloat3("light direction", dynamic_data->light_direction, -1.0f, 1.0f, "%.3f");
  ImGui::SliderFloat3("light color", dynamic_data->light_color, 0.0f, 1.0f, "%.3f");
  ImGui::SliderFloat("light intensity", &dynamic_data->light_intensity, 0.0f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
  ImGui::End();
}
auto RegisterGuiDebugBufferView(const uint32_t render_pass_index_debug_show_selected_buffer, const uint32_t render_pass_index_output_to_swapchain, const BufferList& buffer_list, const uint32_t debug_viewable_buffer_allocation_index_len, const uint32_t* debug_viewable_buffer_allocation_index_list, bool* render_pass_enable_flag, uint32_t* debug_render_selected_buffer_allocation_index) {
  if (!ImGui::Begin("debug buffer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  bool debug_render_selected_buffer = render_pass_enable_flag[render_pass_index_debug_show_selected_buffer];
  ImGui::Checkbox("render selected buffer mode", &debug_render_selected_buffer);
  if (!debug_render_selected_buffer) {
    if (render_pass_enable_flag[render_pass_index_debug_show_selected_buffer]) {
      render_pass_enable_flag[render_pass_index_debug_show_selected_buffer] = false;
      render_pass_enable_flag[render_pass_index_output_to_swapchain] = true;
    }
    ImGui::End();
    return;
  }
  if (!render_pass_enable_flag[render_pass_index_debug_show_selected_buffer]) {
    render_pass_enable_flag[render_pass_index_debug_show_selected_buffer] = true;
    render_pass_enable_flag[render_pass_index_output_to_swapchain] = false;
  }
  auto label_select_output_buffer = "##select output buffer";
  if (!ImGui::BeginListBox(label_select_output_buffer)) {
    ImGui::End();
    return;
  }
  auto tmp_allocator = GetTemporalMemoryAllocator();
  for (uint32_t i = 0; i < debug_viewable_buffer_allocation_index_len; i++) {
    const auto index = debug_viewable_buffer_allocation_index_list[i];
    const auto len = 128;
    char buffer_name[len]{};
    GetD3d12Name(buffer_list.resource_list[index], len, buffer_name);
    if (ImGui::Selectable(buffer_name, i == *debug_render_selected_buffer_allocation_index)) {
      *debug_render_selected_buffer_allocation_index = i;
    }
    if (i == *debug_render_selected_buffer_allocation_index) {
      ImGui::SetItemDefaultFocus();
    }
  }
  ImGui::EndListBox();
  ImGui::End();
}
auto RegisterGui(RenderPassConfigDynamicData* dynamic_data, const TimeDurationDataSet& time_duration_data_set, const uint32_t render_pass_index_debug_show_selected_buffer, const uint32_t render_pass_index_output_to_swapchain, const BufferList buffer_list, const uint32_t debug_viewable_buffer_allocation_index_len, const uint32_t* debug_viewable_buffer_allocation_index_list, bool* render_pass_enable_flag, uint32_t* debug_render_selected_buffer_allocation_index) {
  RegisterGuiPerformance(time_duration_data_set);
  RegisterGuiCamera(dynamic_data);
  RegisterGuiLight(dynamic_data);
  RegisterGuiDebugBufferView(render_pass_index_debug_show_selected_buffer, render_pass_index_output_to_swapchain, buffer_list, debug_viewable_buffer_allocation_index_len, debug_viewable_buffer_allocation_index_list, render_pass_enable_flag, debug_render_selected_buffer_allocation_index);
}
RenderPassConfigDynamicData InitRenderPassDynamicData() {
  RenderPassConfigDynamicData dynamic_data{};
  dynamic_data.camera_focus[1] = 1.0f;
  dynamic_data.camera_pos[0] = 5.0f;
  dynamic_data.camera_pos[1] = 1.5f;
  dynamic_data.light_direction[0] = 1.0f;
  dynamic_data.light_direction[1] = 1.0f;
  dynamic_data.light_direction[2] = 1.0f;
  dynamic_data.light_color[0] = 1.0f;
  dynamic_data.light_color[1] = 1.0f;
  dynamic_data.light_color[2] = 1.0f;
  dynamic_data.light_intensity = 10000.0f;
  return dynamic_data;
}
auto UpdateCameraFromUserInput(const Size2d& buffer_size, float camera_pos[3], float camera_focus[3], float prev_mouse_pos[2]) {
  if (ImGui::GetIO().WantCaptureMouse) { return; }
  const auto& mouse_pos = ImGui::GetMousePos();
  if (ImGui::IsMousePosValid(&mouse_pos) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const float current_mouse_pos[2]{mouse_pos.x, mouse_pos.y};
    RotateCamera(static_cast<float>(buffer_size.width), static_cast<float>(buffer_size.height), camera_pos, camera_focus, prev_mouse_pos, current_mouse_pos);
  }
  prev_mouse_pos[0] = mouse_pos.x;
  prev_mouse_pos[1] = mouse_pos.y;
  if (const auto mouse_wheel = ImGui::GetIO().MouseWheel; mouse_wheel != 0.0f) {
    MoveCameraForward(camera_pos, camera_focus, mouse_wheel);
  }
}
auto GetRenderPassIndexToSwapOnDebugBufferView(const uint32_t render_pass_num, const RenderPass* render_pass_list) {
  uint32_t render_pass_index_debug_show_selected_buffer{};
  uint32_t render_pass_index_output_to_swapchain{};
  for (uint32_t i = 0; i < render_pass_num; i++) {
    switch (render_pass_list[i].name) {
      case SID("output to swapchain"): {
        render_pass_index_output_to_swapchain = i;
        break;
      }
      case SID("debug buffer"): {
        render_pass_index_debug_show_selected_buffer = i;
        break;
      }
    }
  }
  return std::make_pair(render_pass_index_output_to_swapchain, render_pass_index_debug_show_selected_buffer);
}
auto GetDebugViewableBufferAllocationListImpl(const BufferList& buffer_list, const BufferConfig* buffer_config_list, uint32_t* dst_buffer_allocation_index_list) {
  uint32_t index = 0;
  for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
    if (buffer_list.buffer_allocation_list[i] == nullptr) { continue; }
    auto& buffer_config = buffer_config_list[buffer_list.buffer_config_index[i]];
    if (buffer_config.dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) { continue; }
    if (dst_buffer_allocation_index_list) {
      dst_buffer_allocation_index_list[index] = i;
    }
    index++;
  }
  return index;
}
auto GetDebugViewableBufferAllocationList(const BufferList& buffer_list, const BufferConfig* buffer_config_list, MemoryAllocationJanitor* allocator) {
  const auto debug_viewable_buffer_allocation_index_len = GetDebugViewableBufferAllocationListImpl(buffer_list, buffer_config_list, nullptr);
  auto debug_viewable_buffer_allocation_index_list = AllocateArray<uint32_t>(allocator, debug_viewable_buffer_allocation_index_len);
  GetDebugViewableBufferAllocationListImpl(buffer_list, buffer_config_list, debug_viewable_buffer_allocation_index_list);
  return std::make_pair(debug_viewable_buffer_allocation_index_len, debug_viewable_buffer_allocation_index_list);
}
auto ConfigureDebugBufferPass(const BufferList& buffer_list, const uint32_t debug_render_selected_buffer_allocation_index, const uint32_t render_pass_index_debug_show_selected_buffer, MemoryAllocationJanitor* allocator) {
  uint32_t debug_buffer_state_num = 1;
  auto debug_buffer_state_list = AllocateArray<RenderPassBufferState>(allocator, debug_buffer_state_num);
  debug_buffer_state_list[0].pass_index = render_pass_index_debug_show_selected_buffer;
  debug_buffer_state_list[0].buffer_config_index = buffer_list.buffer_config_index[debug_render_selected_buffer_allocation_index];
  debug_buffer_state_list[0].state_type = ResourceStateType::kSrvPs;
  return std::make_pair(debug_buffer_state_num, debug_buffer_state_list);
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
  auto extra_descriptor_heap_cbv_srv_uav = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kExtraDescriptorHandleNumCbvSrvUav);
  MainBufferSize main_buffer_size{};
  MainBufferFormat main_buffer_format{};
  Window window;
  CommandListSet command_list_set;
  Swapchain swapchain;
  CommandQueueSignals command_queue_signals;
  DescriptorCpu descriptor_cpu;
  BufferList buffer_list;
  DescriptorGpu descriptor_gpu;
  RenderGraph render_graph;
  void** render_pass_vars{nullptr};
  RenderPassFunctionList render_pass_function_list{};
  uint32_t swapchain_buffer_allocation_index{kInvalidIndex};
  uint32_t system_buffer_index_list[kSystemBufferNum]{};
  std::fill(system_buffer_index_list, system_buffer_index_list + kSystemBufferNum, kInvalidIndex);
  MaterialList material_list{};
  float prev_mouse_pos[2]{};
#ifdef USE_GRAPHICS_DEBUG_SCOPE
  char** render_pass_name{nullptr};
#endif
  auto frame_loop_num = kFrameLoopNum;
  {
    auto material_json = GetTestJson("material.json");
    ShaderCompiler shader_compiler;
    CHECK_UNARY(shader_compiler.Init());
    material_list = shader_compiler.BuildMaterials(material_json, device.Get(), &allocator);
    shader_compiler.Term();
    nlohmann::json json;
    SUBCASE("config.json") {
      json = GetTestJson("config.json");
      frame_loop_num = 5;
    }
    SUBCASE("forward.json") {
      json = GetTestJson("forward.json");
      frame_loop_num = 100000;
    }
    AddSystemBuffers(&json);
    AddSystemBarriers(&json);
    ParseRenderGraphJson(json, material_list.material_num, material_list.material_hash_list, &allocator, &render_graph);
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
#ifdef FORCE_SRV_FOR_ALL
    for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
      auto& buffer_config = render_graph.buffer_list[i];
      if ((buffer_config.descriptor_type_flags & kDescriptorTypeFlagCbv) == 0
          && (buffer_config.descriptor_type_flags & kDescriptorTypeFlagSrv) == 0
          && (buffer_config.descriptor_type_flags & ~kDescriptorTypeFlagCbv) != 0) {
        auto original_flag = buffer_config.descriptor_type_flags;
        buffer_config.descriptor_type_flags = kDescriptorTypeFlagSrv;
        render_graph.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSrv)] += GetBufferAllocationNum(buffer_config, render_graph.frame_buffer_num);
        buffer_config.descriptor_type_flags |= original_flag;
        buffer_config.flags = buffer_config.flags & (~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
      }
    }
#endif
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, render_graph.frame_buffer_num, buffer_allocator, &allocator);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.descriptor_handle_num_per_type, &allocator));
    CHECK_UNARY(command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList()));
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
      for (uint32_t j = 0; j < kSystemBufferNum; j++) {
        if (system_buffer_index_list[j] != kInvalidIndex) { continue; }
        if (str.compare(kSystemBufferNames[j]) == 0) {
          system_buffer_index_list[j] = buffer_config_index;
          break;
        }
      }
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(i, DescriptorType::kSampler);
      CHECK_NE(cpu_handler.ptr, 0);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], cpu_handler);
    }
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view + render_graph.max_material_num * kTextureNumPerMaterial, render_graph.gpu_handle_num_sampler));
    render_pass_vars = AllocateArray<void*>(&allocator, render_graph.render_pass_num);
    render_pass_name = AllocateArray<char*>(&allocator, render_graph.render_pass_num);
    for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
      const auto& render_pass_list_json = json.at("render_pass");
      RenderPassFuncArgsInit render_pass_func_args_init{
        .json =  render_pass_list_json[i].contains("pass_vars") ? &render_pass_list_json[i].at("pass_vars") : nullptr,
        .frame_buffer_num = render_graph.frame_buffer_num,
        .allocator = &allocator,
        .buffer_list = &buffer_list,
        .buffer_config_list = render_graph.buffer_list,
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
  auto resource_transfer = PrepareResourceTransferer(render_graph.frame_buffer_num, std::max(render_graph.max_model_num, render_graph.max_material_num), render_graph.max_mipmap_num);
  auto scene_data = GetSceneFromTinyGltf(TEST_MODEL_PATH, 0, device.Get(), buffer_allocator, &allocator, &resource_transfer);
  descriptor_gpu.SetPersistentViewHandleNum(scene_data.texture_num + 1/*for imgui font*/);
  const auto scene_gpu_handles_view = descriptor_gpu.WriteToPersistentViewHandleRange(kSceneGpuHandleIndex, scene_data.texture_num, scene_data.cpu_handles[kSceneDescriptorTexture], device.Get());
  descriptor_gpu.SetPersistentSamplerHandleNum(scene_data.sampler_num);
  const auto scene_gpu_handles_sampler = descriptor_gpu.WriteToPersistentSamplerHandleRange(0, scene_data.sampler_num, scene_data.cpu_handles[kSceneDescriptorSampler], device.Get());
  auto dynamic_data = InitRenderPassDynamicData();
  TimeDurationDataSet time_duration_data_set{};
  void** scene_cbv_ptr[kSystemBufferNum]{};
  for (uint32_t i = 0; i < kSystemBufferNum; i++) {
    scene_cbv_ptr[i] = PrepareSceneCbvBuffer(&buffer_list, render_graph.frame_buffer_num, system_buffer_index_list[i], kSystemBufferSize[i], &allocator);
  }
  auto prev_command_list = AllocateArray<D3d12CommandList*>(&allocator, render_graph.command_queue_num);
  auto gpu_timestamp_frequency = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
    command_list_set.GetCommandQueue(i)->GetTimestampFrequency(&gpu_timestamp_frequency[i]);
  }
  const auto render_pass_queue_index = GetRenderPassQueueIndexList(render_graph.render_pass_num, render_graph.render_pass_list, &allocator);
  const auto [render_pass_num_per_queue, render_pass_index_per_queue] = GetRenderPassIndexPerQueue(render_graph.command_queue_num, render_graph.render_pass_num, render_pass_queue_index, &allocator);
  const auto last_pass_per_queue = GetLastPassPerQueue(render_graph.command_queue_num, render_graph.render_pass_num, render_pass_queue_index, &allocator);
  const uint32_t timestamp_query_num_per_pass = 2;
  auto timestamp_query_heaps = CreateTimestampQueryHeaps(render_graph.command_queue_num, render_graph.command_queue_type, render_pass_num_per_queue, timestamp_query_num_per_pass, device.Get(), &allocator);
  const auto timestamp_query_dst_resource_num = render_graph.timestamp_query_dst_resource_num;
  uint32_t timestamp_query_dst_resource_index = 0;
  auto [timestamp_query_dst_resource, timestamp_query_dst_allocator] = CreateTimestampQuertyDstResource(render_graph.command_queue_num, render_pass_num_per_queue, timestamp_query_dst_resource_num, buffer_allocator, &allocator);
  {
    const auto imgui_font_cpu_handle = extra_descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart();
    const auto imgui_font_gpu_handle = descriptor_gpu.GetViewGpuHandle(kImguiGpuHandleIndex);
    InitImgui(window.GetHwnd(), device.Get(), render_graph.frame_buffer_num, swapchain.GetDxgiFormat(), imgui_font_cpu_handle, imgui_font_gpu_handle);
    descriptor_gpu.WriteToPersistentViewHandleRange(kImguiGpuHandleIndex, 1, imgui_font_cpu_handle, device.Get());
  }
  auto render_pass_enable_flag = AllocateArray<bool>(&allocator, render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_enable_flag[i] = render_graph.render_pass_list[i].enabled;
  }
  auto write_to_sub = AllocateArray<bool*>(&allocator, render_graph.buffer_num);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    write_to_sub[i] = AllocateArray<bool>(&allocator, render_graph.render_pass_num);
  }
  auto [render_pass_index_output_to_swapchain, render_pass_index_debug_show_selected_buffer] = GetRenderPassIndexToSwapOnDebugBufferView(render_graph.render_pass_num, render_graph.render_pass_list);
  auto [debug_viewable_buffer_allocation_index_len, debug_viewable_buffer_allocation_index_list] = GetDebugViewableBufferAllocationList(buffer_list, render_graph.buffer_list, &allocator);
  uint32_t debug_render_selected_buffer_allocation_index{0U};
  for (uint32_t i = 0; i < frame_loop_num; i++) {
    if (!window.ProcessMessage()) { break; }
    UpdateTimeDuration(time_duration_data_set.frame_count_reset_time_threshold_msec, &time_duration_data_set.frame_count, &time_duration_data_set.last_time_point, &time_duration_data_set.delta_time_msec, &time_duration_data_set.duration_msec_sum, &time_duration_data_set.prev_duration_per_frame_msec_avg);
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    const auto frame_index = i % render_graph.frame_buffer_num;
    UpdateSceneBuffer(dynamic_data, main_buffer_size.primarybuffer, scene_cbv_ptr[kSystemBufferCamera][frame_index], scene_cbv_ptr[kSystemBufferLight][frame_index]);
    ConfigurePingPongBufferWriteToSubList(render_graph.render_pass_num, render_graph.render_pass_list, render_pass_enable_flag, render_graph.buffer_num, write_to_sub);
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    RegisterResource(swapchain_buffer_allocation_index, swapchain.GetResource(), &buffer_list);
    descriptor_cpu.RegisterExternalHandle(swapchain_buffer_allocation_index, DescriptorType::kRtv, swapchain.GetRtvHandle());
    // set up render pass args
    const auto [additional_buffer_state_num, additional_buffer_state_list] = render_pass_enable_flag[render_pass_index_debug_show_selected_buffer]
        ? ConfigureDebugBufferPass(buffer_list, debug_render_selected_buffer_allocation_index, render_pass_index_debug_show_selected_buffer, &single_frame_allocator)
        : std::make_pair(0U, (RenderPassBufferState*)nullptr);
    auto [resource_state_list, last_user_pass] = ConfigureRenderPassResourceStates(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, render_graph.buffer_list, (const bool**)write_to_sub, &render_pass_enable_flag[0], additional_buffer_state_num, additional_buffer_state_list, &single_frame_allocator);
    auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(render_graph.render_pass_num, render_graph.render_pass_list, render_graph.buffer_num, render_graph.buffer_list, resource_state_list, last_user_pass, &single_frame_allocator);
    auto barrier_resource_list = PrepareBarrierResourceList(render_graph.render_pass_num, barrier_num, barrier_config_list, buffer_list, &single_frame_allocator);
    auto args_per_pass = AllocateArray<RenderPassFuncArgsRenderPerPass>(&single_frame_allocator, render_graph.render_pass_num);
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      if (!render_pass_enable_flag[k]) { continue; }
      args_per_pass[k].pass_vars_ptr = render_pass_vars[k];
      args_per_pass[k].render_pass_index = k;
      std::tie(args_per_pass[k].resources, args_per_pass[k].cpu_handles) = PrepareResourceCpuHandleList(render_pass, &descriptor_cpu, buffer_list, (const bool**)write_to_sub, render_graph.buffer_list, frame_index, &scene_data.cpu_handles[0], &single_frame_allocator);
      args_per_pass[k].gpu_handles_view = PrepareGpuHandlesViewList(device.Get(), render_pass, args_per_pass[k].cpu_handles, &descriptor_gpu, scene_data.cpu_handles[kSceneDescriptorTexture], scene_gpu_handles_view, &single_frame_allocator);
      args_per_pass[k].gpu_handles_sampler = PrepareGpuHandlesSamplerList(device.Get(), render_pass, &descriptor_cpu, &descriptor_gpu, scene_gpu_handles_sampler, &single_frame_allocator);
    }
    {
      // imgui
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();
      UpdateCameraFromUserInput(main_buffer_size.swapchain, dynamic_data.camera_pos, dynamic_data.camera_focus, prev_mouse_pos);
    }
    RegisterGui(&dynamic_data, time_duration_data_set, render_pass_index_debug_show_selected_buffer, render_pass_index_output_to_swapchain, buffer_list, debug_viewable_buffer_allocation_index_len, debug_viewable_buffer_allocation_index_list, render_pass_enable_flag, &debug_render_selected_buffer_allocation_index);
    // update
    RenderPassFuncArgsRenderCommon args_common {
      .main_buffer_size = &main_buffer_size,
      .scene_data = &scene_data,
      .frame_index = frame_index,
      .dynamic_data = &dynamic_data,
      .render_pass_list = render_graph.render_pass_list,
      .material_list = &material_list,
      .resource_transfer = &resource_transfer,
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
        CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(render_pass.signal_queue_index[l], render_pass_queue_index[k], render_pass_signal[render_pass.signal_pass_index[l]]));
      }
      if (render_pass.prepass_barrier_num == 0 && render_pass.postpass_barrier_num == 0 && !IsRenderPassRenderNeeded(&render_pass_function_list, &args_common, &args_per_pass[k])) { continue; }
      auto command_list = prev_command_list[render_pass_queue_index[k]];
      if (command_list == nullptr) {
        command_list = command_list_set.GetCommandList(device.Get(), render_pass_queue_index[k]); // TODO decide command list reuse policy for multi-thread
        prev_command_list[render_pass_queue_index[k]] = command_list;
      }
      args_per_pass[k].command_list = command_list;
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      PIXBeginEvent(command_list, 0, render_pass_name[k]); // https://devblogs.microsoft.com/pix/winpixeventruntime/
#endif
      const auto query_index = render_pass_index_per_queue[k] * 2;
      command_list->EndQuery(timestamp_query_heaps[render_pass_queue_index[k]], D3D12_QUERY_TYPE_TIMESTAMP, query_index);
      if (command_list && render_graph.command_queue_type[render_pass_queue_index[k]] != D3D12_COMMAND_LIST_TYPE_COPY) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, barrier_num[0][k], barrier_config_list[0][k], barrier_resource_list[0][k]);
      RenderPassRender(&render_pass_function_list, &args_common, &args_per_pass[k]);
      ExecuteBarrier(command_list, barrier_num[1][k], barrier_config_list[1][k], barrier_resource_list[1][k]);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      PIXEndEvent(command_list);
#endif
      command_list->EndQuery(timestamp_query_heaps[render_pass_queue_index[k]], D3D12_QUERY_TYPE_TIMESTAMP, query_index + 1);
      const auto is_last_pass_per_queue = (last_pass_per_queue[render_pass_queue_index[k]] == k);
      if (is_last_pass_per_queue) {
        assert(render_pass.execute);
        const auto query_num = render_pass_num_per_queue[render_pass_queue_index[k]] * timestamp_query_num_per_pass;
        auto dst_resource = timestamp_query_dst_resource[render_pass_queue_index[k]][timestamp_query_dst_resource_index];
        timestamp_query_dst_resource_index++;
        if (timestamp_query_dst_resource_index >= timestamp_query_dst_resource_num) {
          timestamp_query_dst_resource_index = 0;
        }
        // TODO read from buffer
        command_list->ResolveQueryData(timestamp_query_heaps[render_pass_queue_index[k]], D3D12_QUERY_TYPE_TIMESTAMP, 0, query_num, dst_resource, 0);
      }
      if (render_pass.execute) {
        command_list_set.ExecuteCommandList(render_pass_queue_index[k]);
        prev_command_list[render_pass_queue_index[k]] = nullptr;
      }
      if (render_pass.sends_signal || is_last_pass_per_queue) {
        assert(render_pass.execute);
        render_pass_signal[k] = command_queue_signals.SucceedSignal(render_pass_queue_index[k]);
        frame_signals[frame_index][render_pass_queue_index[k]] = render_pass_signal[k];
      }
    } // render pass
    swapchain.Present();
  }
  command_queue_signals.WaitAll(device.Get());
  TermImgui();
  ClearResourceTransfer(render_graph.frame_buffer_num, &resource_transfer);
  ReleaseSceneData(&scene_data);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    if (timestamp_query_dst_resource[i] == nullptr) { continue; }
    for (uint32_t j = 0; j < timestamp_query_dst_resource_num; j++) {
      timestamp_query_dst_resource[i][j]->Release();
      timestamp_query_dst_allocator[i][j]->Release();
    }
  }
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    if (timestamp_query_heaps[i]) {
      timestamp_query_heaps[i]->Release();
    }
  }
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    RenderPassTerm(&render_pass_function_list, i);
  }
  ReleasePsoAndRootsig(&material_list);
  swapchain.Term();
  descriptor_gpu.Term();
  descriptor_cpu.Term();
  RegisterResource(swapchain_buffer_allocation_index, nullptr, &buffer_list);
  ReleaseBuffers(&buffer_list);
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  extra_descriptor_heap_cbv_srv_uav->Release();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
