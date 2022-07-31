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
#include "d3d12_gpu_timestamp_set.h"
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
auto PrepareRenderPassFunctions(const uint32_t render_pass_num, const RenderPass* render_pass_list) {
  RenderPassFunctionList funcs{};
  funcs.init = AllocateArraySystem<RenderPassFuncInit>(render_pass_num);
  funcs.term = AllocateArraySystem<RenderPassFuncTerm>(render_pass_num);
  funcs.update = AllocateArraySystem<RenderPassFuncUpdate>(render_pass_num);
  funcs.is_render_needed = AllocateArraySystem<RenderPassFuncIsRenderNeeded>(render_pass_num);
  funcs.render = AllocateArraySystem<RenderPassFuncRender>(render_pass_num);
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
auto GetIndexOffsetList(const RenderPass& render_pass) {
  auto index_offset_list = AllocateArrayFrame<uint32_t>(render_pass.buffer_num);
  for (uint32_t i = 0; i < render_pass.buffer_num; i++) {
    index_offset_list[i] = render_pass.buffer_list[i].index_offset;
  }
  return index_offset_list;
}
auto PrepareGpuHandlesViewList(D3d12Device* device, const uint32_t buffer_num, const ResourceStateType* resource_state_list, const uint32_t offset_num, const uint32_t* index_offset_list, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle_list, DescriptorGpu* const descriptor_gpu, const D3D12_CPU_DESCRIPTOR_HANDLE& texture_list_cpu_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& texture_list_gpu_handle) {
  if (buffer_num == 0) { return (D3D12_GPU_DESCRIPTOR_HANDLE*)nullptr; }
  auto desc_num_list = AllocateArrayFrame<uint32_t>(offset_num);
  std::fill(desc_num_list, desc_num_list + offset_num, 0);
  auto copy_src_cpu_handles = AllocateArrayFrame<D3D12_CPU_DESCRIPTOR_HANDLE*>(offset_num);
  for (uint32_t i = 0; i < offset_num; i++) {
    copy_src_cpu_handles[i] = AllocateArrayFrame<D3D12_CPU_DESCRIPTOR_HANDLE>(buffer_num);
  }
  for (uint32_t i = 0; i < buffer_num; i++) {
    if (!IsGpuHandleAvailableType(resource_state_list[i])) { continue; }
    const auto offset_index = index_offset_list[i];
    copy_src_cpu_handles[offset_index][desc_num_list[offset_index]].ptr = cpu_handle_list[i].ptr;
    desc_num_list[offset_index]++;
  }
  auto gpu_handle_list = AllocateArrayFrame<D3D12_GPU_DESCRIPTOR_HANDLE>(offset_num);
  for (uint32_t i = 0; i < offset_num; i++) {
    if (desc_num_list[i] == 0) { continue; }
    if (copy_src_cpu_handles[i][0].ptr == texture_list_cpu_handle.ptr) {
      assert(desc_num_list[i] == 1);
      gpu_handle_list[i].ptr = texture_list_gpu_handle.ptr;
      continue;
    }
    gpu_handle_list[i].ptr = descriptor_gpu->WriteToTransientViewHandleRange(desc_num_list[i], copy_src_cpu_handles[i], device).ptr;
  }
  return gpu_handle_list;
}
auto PrepareGpuHandlesSamplerList(D3d12Device* device, const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, DescriptorGpu* descriptor_gpu, const D3D12_GPU_DESCRIPTOR_HANDLE& scene_sampler) {
  if (render_pass.sampler_num == 0) { return (D3D12_GPU_DESCRIPTOR_HANDLE*)nullptr; }
  bool need_scene_sampler = false;
  for (uint32_t s = 0; s < render_pass.sampler_num; s++) {
    if (render_pass.sampler_index_list[s] == kSceneSamplerId) {
      need_scene_sampler = true;
      break;
    }
  }
  const uint32_t sampler_list_num = (need_scene_sampler && render_pass.sampler_num > 1) ? 2 : 1;
  auto gpu_handle_list = AllocateArrayFrame<D3D12_GPU_DESCRIPTOR_HANDLE>(sampler_list_num);
  auto cpu_handle_list = AllocateArrayFrame<D3D12_CPU_DESCRIPTOR_HANDLE>(render_pass.sampler_num);
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
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const BarrierConfig* barrier_config_list, ID3D12Resource** resource) {
  if (barrier_num == 0) { return; }
  auto barriers = AllocateArrayFrame<D3D12_RESOURCE_BARRIER>(barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config_list[i];
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
auto PrepareSceneCbvBuffer(BufferList* buffer_list, const uint32_t frame_buffer_num, const uint32_t scene_cbv_buffer_config_index, const uint32_t buffer_size) {
  auto ptr_list = AllocateArraySystem<void*>(frame_buffer_num);
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
  scene_light.exposure_rate = 10.0f / dynamic_data.light_intensity;
  memcpy(scene_light_ptr, &scene_light, sizeof(scene_light));
}
auto GetRenderPassQueueIndexList(const uint32_t render_pass_num, const RenderPass* render_pass_list) {
  auto render_pass_queue_index = AllocateArraySystem<uint32_t>(render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_queue_index[i] = render_pass_list[i].command_queue_index;
  }
  return render_pass_queue_index;
}
auto GetRenderPassIndexPerQueue(const uint32_t command_queue_num, const uint32_t render_pass_num, const uint32_t* render_pass_queue_index) {
  auto render_pass_num_per_queue = AllocateArraySystem<uint32_t>(command_queue_num);
  auto render_pass_index_per_queue = AllocateArraySystem<uint32_t>(render_pass_num);
  std::fill(render_pass_num_per_queue, render_pass_num_per_queue + command_queue_num, 0);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_index_per_queue[i] = render_pass_num_per_queue[render_pass_queue_index[i]];
    render_pass_num_per_queue[render_pass_queue_index[i]]++;
  }
  return std::make_pair(render_pass_num_per_queue, render_pass_index_per_queue);
}
auto GetLastPassPerQueue(const uint32_t command_queue_num, const uint32_t render_pass_num, const uint32_t* render_pass_queue_index) {
  auto last_pass_per_queue = AllocateArraySystem<uint32_t>(command_queue_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    last_pass_per_queue[render_pass_queue_index[i]] = i;
  }
  return last_pass_per_queue;
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
auto RegisterGuiPerformance(const TimeDurationDataSet& time_duration_data_set, const GpuTimeDurations& gpu_time_durations) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (!ImGui::Begin("performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::Text("CPU: %7.4f", time_duration_data_set.prev_duration_per_frame_msec_avg);
  ImGui::Text("GPU: %7.4f", gpu_time_durations.total_time_msec);
  for (uint32_t i = 0; i < gpu_time_durations.command_queue_num; i++) {
    ImGui::Text("queue%d", i);
    for (uint32_t j = 0; j < gpu_time_durations.duration_num[i]; j++) {
      if ((j & 1) == 0) {
        ImGui::Text("--: %7.4f", gpu_time_durations.duration_msec[i][j]);
      } else {
        ImGui::Text("%2d: %7.4f", j / 2, gpu_time_durations.duration_msec[i][j]);
      }
    }
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
struct ArrayWithNum {
  uint32_t num;
  uint32_t* const array;
};
auto GetDebugViewableBufferConfigIndexList(const uint32_t buffer_num, const BufferConfig* buffer_config_list, const MemoryType& memory_type) {
  uint32_t index = 0;
  auto debug_viewable_buffer_config_index_list = AllocateArray<uint32_t>(memory_type, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    if (buffer_config_list[i].dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
      debug_viewable_buffer_config_index_list[index] = i;
      index++;
    }
  }
  return ArrayWithNum{index, debug_viewable_buffer_config_index_list};
}
auto GetBufferAllocationIndexList(const uint32_t buffer_config_index_num, const uint32_t* buffer_config_index_list, const BufferConfig* buffer_config_list, const BufferList& buffer_list, const uint32_t frame_buffer_num, const MemoryType& memory_type) {
  uint32_t count = 0;
  for (uint32_t i = 0; i < buffer_config_index_num; i++) {
    count += GetBufferAllocationNum(buffer_config_list[buffer_config_index_list[i]], frame_buffer_num);
  }
  uint32_t index = 0;
  auto buffer_allocation_index_list = AllocateArray<uint32_t>(memory_type, count);
  for (uint32_t i = 0; i < buffer_config_index_num; i++) {
    const auto c = GetBufferAllocationNum(buffer_config_list[buffer_config_index_list[i]], frame_buffer_num);
    for (uint32_t j = 0; j < c; j++) {
      buffer_allocation_index_list[index] = GetBufferAllocationIndex(buffer_list, buffer_config_index_list[i], j);
      index++;
    }
  }
  assert(index == count);
  return ArrayWithNum(count, buffer_allocation_index_list);
}
auto GetBufferResourceList(const uint32_t buffer_num, const uint32_t* buffer_allocation_index_list, const BufferList& buffer_list, const MemoryType& memory_type) {
  auto buffer_resource_list = AllocateArray<ID3D12Resource*>(memory_type, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    buffer_resource_list[i] = GetResource(buffer_list, buffer_allocation_index_list[i]);
  }
  return buffer_resource_list;
}
auto GetBufferNameList(const uint32_t resource_num, ID3D12Resource** resource_list, const MemoryType& memory_type) {
  auto buffer_name_list = AllocateArray<char*>(memory_type, resource_num);
  const uint32_t name_len = 32;
  for (uint32_t i = 0; i < resource_num; i++) {
    buffer_name_list[i] = AllocateArray<char>(memory_type, name_len);
    GetD3d12Name(resource_list[i], name_len, buffer_name_list[i]);
  }
  return buffer_name_list;
}
auto RegisterGuiDebugView(const uint32_t debug_viewable_buffer_num, const char* const * debug_viewable_buffer_name_list, bool* debug_buffer_view_enabled, int32_t* debug_buffer_selected_index) {
  if (!ImGui::Begin("debug buffers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::Checkbox("enable debug buffer view", debug_buffer_view_enabled);
  if (*debug_buffer_view_enabled) {
    ImGui::Combo("buffer name", debug_buffer_selected_index, debug_viewable_buffer_name_list, debug_viewable_buffer_num);
  }
  ImGui::End();
}
auto RegisterGui(RenderPassConfigDynamicData* dynamic_data, const TimeDurationDataSet& time_duration_data_set, const uint32_t debug_viewable_buffer_num, const char* const * debug_viewable_buffer_name_list, bool* debug_buffer_view_enabled, int32_t* debug_buffer_selected_index, const GpuTimeDurations& gpu_time_durations) {
  RegisterGuiPerformance(time_duration_data_set, gpu_time_durations);
  RegisterGuiCamera(dynamic_data);
  RegisterGuiLight(dynamic_data);
  RegisterGuiDebugView(debug_viewable_buffer_num, debug_viewable_buffer_name_list, debug_buffer_view_enabled, debug_buffer_selected_index);
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
auto ConfigureRenderPassBufferAllocationIndex(const uint32_t render_pass_num, const RenderPass* render_pass_list, const BufferList& buffer_list, const bool* const* write_to_sub, const BufferConfig* buffer_config_list, const uint32_t frame_index) {
  auto render_pass_buffer_allocation_index_list = AllocateArrayFrame<uint32_t*>(render_pass_num);
  auto render_pass_buffer_state_list = AllocateArrayFrame<ResourceStateType*>(render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    const auto& render_pass = render_pass_list[i];
    render_pass_buffer_allocation_index_list[i] = AllocateArrayFrame<uint32_t>(render_pass.buffer_num);
    render_pass_buffer_state_list[i] = AllocateArrayFrame<ResourceStateType>(render_pass.buffer_num);
    for (uint32_t j = 0; j < render_pass.buffer_num; j++) {
      const auto buffer_config_index = render_pass.buffer_list[j].buffer_index;
      render_pass_buffer_state_list[i][j] = render_pass.buffer_list[j].state;
      if (IsSceneBuffer(buffer_config_index)) {
        assert(IsGpuHandleAvailableType(render_pass.buffer_list[j].state));
        render_pass_buffer_allocation_index_list[i][j] = buffer_config_index;
        continue;
      }
      const auto local_index = GetBufferLocalIndex(buffer_config_list[buffer_config_index], render_pass.buffer_list[j].state, write_to_sub[buffer_config_index][i], frame_index);
      render_pass_buffer_allocation_index_list[i][j] = GetBufferAllocationIndex(buffer_list, buffer_config_index, local_index);
    }
  }
  return std::make_pair(render_pass_buffer_allocation_index_list, render_pass_buffer_state_list);
}
auto GetRenderPassBufferNumList(const uint32_t render_pass_num, const RenderPass* render_pass_list, const MemoryType memory_type) {
  auto render_pass_buffer_num_list = AllocateArray<uint32_t>(memory_type, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_buffer_num_list[i] = render_pass_list[i].buffer_num;
  }
  return render_pass_buffer_num_list;
}
auto PrepareBarrierResourceList(const uint32_t render_pass_num, const uint32_t* const* barrier_num, const BarrierConfig* const* const* barrier_config_list, const BufferList& buffer_list, const MemoryType memory_type) {
  auto resource_list = AllocateArray<ID3D12Resource***>(memory_type, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    if (barrier_num[i] == 0) {
      resource_list[i] = nullptr;
      continue;
    }
    resource_list[i] = AllocateArray<ID3D12Resource**>(memory_type, kBarrierExecutionTimingNum);
    for (uint32_t j = 0; j < kBarrierExecutionTimingNum; j++) {
      resource_list[i][j] = AllocateArray<ID3D12Resource*>(memory_type, barrier_num[i][j]);
      for (uint32_t k = 0; k < barrier_num[i][j]; k++) {
        resource_list[i][j][k] = GetResource(buffer_list, barrier_config_list[i][j][k].buffer_allocation_index);
      }
    }
  }
  return resource_list;
}
} // namespace anonymous
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
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
  float prev_mouse_pos[2]{};
  uint32_t render_pass_index_output_to_swapchain{};
  uint32_t render_pass_buffer_index_primary_input{};
#ifdef USE_GRAPHICS_DEBUG_SCOPE
  char** render_pass_name{nullptr};
#endif
  auto frame_loop_num = kFrameLoopNum;
  auto material_list = BuildMaterialList(device.Get(), GetTestJson("material.json"));
  {
    nlohmann::json json;
    SUBCASE("deferred.json") {
      json = GetTestJson("deferred.json");
      frame_loop_num = 100000;
    }
    SUBCASE("forward.json") {
      json = GetTestJson("forward.json");
      frame_loop_num = 30;
    }
    SUBCASE("config.json") {
      json = GetTestJson("config.json");
      frame_loop_num = 5;
    }
    AddSystemBuffers(&json);
    ParseRenderGraphJson(json, material_list.material_num, material_list.material_hash_list, &render_graph);
    render_pass_function_list = PrepareRenderPassFunctions(render_graph.render_pass_num, render_graph.render_pass_list);
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
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, render_graph.frame_buffer_num, buffer_allocator);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.descriptor_handle_num_per_type));
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
    render_pass_vars = AllocateArraySystem<void*>(render_graph.render_pass_num);
    render_pass_name = AllocateArraySystem<char*>(render_graph.render_pass_num);
    for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
      const auto& render_pass_list_json = json.at("render_pass");
      RenderPassFuncArgsInit render_pass_func_args_init{
        .json =  render_pass_list_json[i].contains("pass_vars") ? &render_pass_list_json[i].at("pass_vars") : nullptr,
        .frame_buffer_num = render_graph.frame_buffer_num,
        .buffer_list = &buffer_list,
        .buffer_config_list = render_graph.buffer_list,
        .render_pass_list = render_graph.render_pass_list,
      };
      render_pass_vars[i] = RenderPassInit(&render_pass_function_list, &render_pass_func_args_init, i);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      auto pass_name = GetStringView(render_pass_list_json[i], ("name"));
      const auto str_len = GetUint32(pass_name.size())  + 1;
      render_pass_name[i] = AllocateArraySystem<char>(str_len);
      strcpy_s(render_pass_name[i], str_len, GetStringView(pass_name).data());
#endif
      if (render_graph.render_pass_list[i].name == SID("output to swapchain")) {
        render_pass_index_output_to_swapchain = i;
        for (uint32_t j = 0; j < render_graph.render_pass_list[i].buffer_num; j++) {
          if (json_buffer_list[j].at("name").get<std::string>().compare("primary") == 0) {
            render_pass_buffer_index_primary_input = j;
            break;
          }
        }
      }
    }
  }
  auto frame_signals = AllocateArraySystem<uint64_t*>(render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArraySystem<uint64_t>(render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
  }
  auto render_pass_signal = AllocateArraySystem<uint64_t>(render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_signal[i] = 0UL;
  }
  auto resource_transfer = PrepareResourceTransferer(render_graph.frame_buffer_num, std::max(render_graph.max_model_num, render_graph.max_material_num), render_graph.max_mipmap_num);
  auto scene_data = GetSceneFromTinyGltf(TEST_MODEL_PATH, 0, device.Get(), buffer_allocator, &resource_transfer);
  descriptor_gpu.SetPersistentViewHandleNum(scene_data.texture_num + 1/*for imgui font*/);
  const auto scene_gpu_handles_view = descriptor_gpu.WriteToPersistentViewHandleRange(kSceneGpuHandleIndex, scene_data.texture_num, scene_data.cpu_handles[kSceneDescriptorTexture], device.Get());
  descriptor_gpu.SetPersistentSamplerHandleNum(scene_data.sampler_num);
  const auto scene_gpu_handles_sampler = descriptor_gpu.WriteToPersistentSamplerHandleRange(0, scene_data.sampler_num, scene_data.cpu_handles[kSceneDescriptorSampler], device.Get());
  auto dynamic_data = InitRenderPassDynamicData();
  TimeDurationDataSet time_duration_data_set{};
  void** scene_cbv_ptr[kSystemBufferNum]{};
  for (uint32_t i = 0; i < kSystemBufferNum; i++) {
    scene_cbv_ptr[i] = PrepareSceneCbvBuffer(&buffer_list, render_graph.frame_buffer_num, system_buffer_index_list[i], kSystemBufferSize[i]);
  }
  auto prev_command_list = AllocateArraySystem<D3d12CommandList*>(render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
  }
  const auto render_pass_queue_index = GetRenderPassQueueIndexList(render_graph.render_pass_num, render_graph.render_pass_list);
  const auto [render_pass_num_per_queue, render_pass_index_per_queue] = GetRenderPassIndexPerQueue(render_graph.command_queue_num, render_graph.render_pass_num, render_pass_queue_index);
  const auto last_pass_per_queue = GetLastPassPerQueue(render_graph.command_queue_num, render_graph.render_pass_num, render_pass_queue_index);
  {
    const auto imgui_font_cpu_handle = extra_descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart();
    const auto imgui_font_gpu_handle = descriptor_gpu.GetViewGpuHandle(kImguiGpuHandleIndex);
    InitImgui(window.GetHwnd(), device.Get(), render_graph.frame_buffer_num, swapchain.GetDxgiFormat(), imgui_font_cpu_handle, imgui_font_gpu_handle);
    descriptor_gpu.WriteToPersistentViewHandleRange(kImguiGpuHandleIndex, 1, imgui_font_cpu_handle, device.Get());
  }
  auto render_pass_enable_flag = AllocateArraySystem<bool>(render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_enable_flag[i] = render_graph.render_pass_list[i].enabled;
  }
  auto write_to_sub = AllocateArraySystem<bool*>(render_graph.buffer_num);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    write_to_sub[i] = AllocateArraySystem<bool>(render_graph.render_pass_num);
  }
  auto gpu_timestamp_set = CreateGpuTimestampSet(render_graph.command_queue_num, command_list_set.GetCommandQueueList(), render_graph.command_queue_type, render_pass_num_per_queue, device.Get(), buffer_allocator);
  auto gpu_time_durations_accumulated = GetEmptyGpuTimeDurations(render_graph.command_queue_num, render_pass_num_per_queue, MemoryType::kSystem);
  auto gpu_time_durations_average = GetEmptyGpuTimeDurations(render_graph.command_queue_num, render_pass_num_per_queue, MemoryType::kSystem);
  bool debug_buffer_view_enabled = false;
  int32_t debug_buffer_selected_index = 0;
  ResetAllocation(MemoryType::kFrame);
  for (uint32_t i = 0; i < frame_loop_num; i++) {
    if (!window.ProcessMessage()) { break; }
    const auto gpu_time_durations_per_frame = GetGpuTimeDurationsPerFrame(render_graph.command_queue_num, render_pass_num_per_queue, gpu_timestamp_set, MemoryType::kFrame);
    AccumulateGpuTimeDuration(gpu_time_durations_per_frame, &gpu_time_durations_accumulated);
    if (const auto frame_count = time_duration_data_set.frame_count; UpdateTimeDuration(time_duration_data_set.frame_count_reset_time_threshold_msec, &time_duration_data_set.frame_count, &time_duration_data_set.last_time_point, &time_duration_data_set.delta_time_msec, &time_duration_data_set.duration_msec_sum, &time_duration_data_set.prev_duration_per_frame_msec_avg)) {
      CalcAvarageGpuTimeDuration(gpu_time_durations_accumulated, frame_count, &gpu_time_durations_average);
      ClearGpuTimeDuration(&gpu_time_durations_accumulated);
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
    UpdateSceneBuffer(dynamic_data, main_buffer_size.primarybuffer, scene_cbv_ptr[kSystemBufferCamera][frame_index], scene_cbv_ptr[kSystemBufferLight][frame_index]);
    ConfigurePingPongBufferWriteToSubList(render_graph.render_pass_num, render_graph.render_pass_list, render_pass_enable_flag, render_graph.buffer_num, write_to_sub);
    auto [render_pass_buffer_allocation_index_list, render_pass_buffer_state_list] = ConfigureRenderPassBufferAllocationIndex(render_graph.render_pass_num, render_graph.render_pass_list, buffer_list, write_to_sub, render_graph.buffer_list, frame_index);
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    RegisterResource(swapchain_buffer_allocation_index, swapchain.GetResource(), &buffer_list);
    descriptor_cpu.RegisterExternalHandle(swapchain_buffer_allocation_index, DescriptorType::kRtv, swapchain.GetRtvHandle());
    // debug buffer view settings
    auto [debug_viewable_buffer_config_num, debug_viewable_buffer_config_index_list] = GetDebugViewableBufferConfigIndexList(render_graph.buffer_num, render_graph.buffer_list, MemoryType::kFrame);
    auto [debug_viewable_buffer_allocation_num, debug_viewable_buffer_allocation_index_list] = GetBufferAllocationIndexList(debug_viewable_buffer_config_num, debug_viewable_buffer_config_index_list, render_graph.buffer_list, buffer_list, render_graph.frame_buffer_num, MemoryType::kFrame);
    auto debug_viewable_buffer_resource_list = GetBufferResourceList(debug_viewable_buffer_allocation_num, debug_viewable_buffer_allocation_index_list, buffer_list, MemoryType::kFrame);
    auto debug_viewable_buffer_name_list = GetBufferNameList(debug_viewable_buffer_allocation_num, debug_viewable_buffer_resource_list, MemoryType::kFrame);
    if (debug_buffer_view_enabled) {
      render_pass_buffer_allocation_index_list[render_pass_index_output_to_swapchain][render_pass_buffer_index_primary_input] = debug_viewable_buffer_allocation_index_list[debug_buffer_selected_index];
    }
    // setup render pass args
    auto args_per_pass = AllocateArrayFrame<RenderPassFuncArgsRenderPerPass>(render_graph.render_pass_num);
    for (uint32_t j = 0; j < render_graph.render_pass_num; j++) {
      const auto& render_pass = render_graph.render_pass_list[j];
      if (!render_pass_enable_flag[j]) { continue; }
      args_per_pass[j].pass_vars_ptr = render_pass_vars[j];
      args_per_pass[j].render_pass_index = j;
      args_per_pass[j].resources = GetResourceList(render_pass.buffer_num, render_pass_buffer_allocation_index_list[j], buffer_list, MemoryType::kFrame);
      args_per_pass[j].cpu_handles = descriptor_cpu.GetCpuHandleList(render_pass.buffer_num, render_pass_buffer_allocation_index_list[j], render_pass_buffer_state_list[j], scene_data.cpu_handles, MemoryType::kFrame);
      auto index_offset_list = GetIndexOffsetList(render_pass);
      args_per_pass[j].gpu_handles_view = PrepareGpuHandlesViewList(device.Get(), render_pass.buffer_num, render_pass_buffer_state_list[j], render_pass.max_buffer_index_offset + 1, index_offset_list, args_per_pass[j].cpu_handles, &descriptor_gpu, scene_data.cpu_handles[kSceneDescriptorTexture], scene_gpu_handles_view);
      args_per_pass[j].gpu_handles_sampler = PrepareGpuHandlesSamplerList(device.Get(), render_pass, &descriptor_cpu, &descriptor_gpu, scene_gpu_handles_sampler);
    }
    // setup barriers
    const auto swapchain_initial_state = ResourceStateType::kPresent;
    const auto swapchain_final_state = ResourceStateType::kPresent;
    auto render_pass_buffer_num_list = GetRenderPassBufferNumList(render_graph.render_pass_num, render_graph.render_pass_list, MemoryType::kFrame);
    const auto [barrier_num, barrier_config_list] = ConfigureBarrierTransitions(render_graph.buffer_num, render_graph.render_pass_num,
                                                                                render_pass_buffer_num_list, render_pass_buffer_allocation_index_list, render_pass_buffer_state_list,
                                                                                1, &swapchain_buffer_allocation_index, &swapchain_initial_state,
                                                                                1, &swapchain_buffer_allocation_index, &swapchain_final_state,
                                                                                MemoryType::kFrame);
    auto barrier_resource_list = PrepareBarrierResourceList(render_graph.render_pass_num, barrier_num, barrier_config_list, buffer_list, MemoryType::kFrame);
    {
      // imgui
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();
      UpdateCameraFromUserInput(main_buffer_size.swapchain, dynamic_data.camera_pos, dynamic_data.camera_focus, prev_mouse_pos);
    }
    RegisterGui(&dynamic_data, time_duration_data_set, debug_viewable_buffer_allocation_num, debug_viewable_buffer_name_list, &debug_buffer_view_enabled, &debug_buffer_selected_index, gpu_time_durations_average);
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
      if (barrier_num[0][k] == 0 && barrier_num[1][k] == 0 && !IsRenderPassRenderNeeded(&render_pass_function_list, &args_common, &args_per_pass[k])) { continue; }
      auto command_list = prev_command_list[render_pass_queue_index[k]];
      if (command_list == nullptr) {
        command_list = command_list_set.GetCommandList(device.Get(), render_pass_queue_index[k]); // TODO decide command list reuse policy for multi-thread
        prev_command_list[render_pass_queue_index[k]] = command_list;
      }
      args_per_pass[k].command_list = command_list;
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      if (!IsDebuggerPresent() || command_list && render_graph.command_queue_type[render_pass_queue_index[k]] != D3D12_COMMAND_LIST_TYPE_COPY) {
        // debugger issues an error on copy queue
        PIXBeginEvent(command_list, 0, render_pass_name[k]); // https://devblogs.microsoft.com/pix/winpixeventruntime/
      }
#endif
      StartGpuTimestamp(render_pass_index_per_queue, render_pass_queue_index, k, &gpu_timestamp_set, command_list);
      if (command_list && render_graph.command_queue_type[render_pass_queue_index[k]] != D3D12_COMMAND_LIST_TYPE_COPY) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, barrier_num[k][0], barrier_config_list[k][0], barrier_resource_list[k][0]);
      RenderPassRender(&render_pass_function_list, &args_common, &args_per_pass[k]);
      ExecuteBarrier(command_list, barrier_num[k][1], barrier_config_list[k][1], barrier_resource_list[k][1]);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      if (!IsDebuggerPresent() || command_list && render_graph.command_queue_type[render_pass_queue_index[k]] != D3D12_COMMAND_LIST_TYPE_COPY) {
        // debugger issues an error on copy queue
        PIXEndEvent(command_list);
      }
#endif
      EndGpuTimestamp(render_pass_index_per_queue, render_pass_queue_index, k, &gpu_timestamp_set, command_list);
      const auto is_last_pass_per_queue = (last_pass_per_queue[render_pass_queue_index[k]] == k);
      if (is_last_pass_per_queue) {
        assert(render_pass.execute);
        OutputGpuTimestampToCpuVisibleBuffer(render_pass_num_per_queue, render_pass_queue_index, k, &gpu_timestamp_set, command_list);
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
  ReleaseGpuTimestampSet(render_graph.command_queue_num, &gpu_timestamp_set);
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
  ClearAllAllocations();
}
