#include <fstream>
#include "gfxminimath/gfxminimath.h"
#include "imgui.h"
#include <nlohmann/json.hpp>
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
#include "d3d12_integration_test_cbuffers.inl"
#define FORCE_SRV_FOR_ALL
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
namespace {
static const uint32_t kFrameLoopNum = 5;
static const uint32_t kInvalidIndex = ~0U;
static const uint32_t kExtraDescriptorHandleNumCbvSrvUav = 1; // imgui font
static const uint32_t kImguiGpuHandleIndex = 0;
static const uint32_t kSceneGpuHandleIndex = 1;
auto GetTestJson(const char* const filename) {
  std::ifstream file(filename);
  nlohmann::json json;
  file >> json;
  return json;
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
  auto desc_num_list = AllocateAndFillArrayFrame(offset_num, 0U);
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
  auto cpu_handle_list = AllocateArrayFrame<D3D12_CPU_DESCRIPTOR_HANDLE>(render_pass.sampler_num);
  uint32_t sampler_index = 0;
  for (uint32_t s = 0; s < render_pass.sampler_num; s++) {
    if (render_pass.sampler_index_list[s] == kSceneSamplerId) { continue; }
    cpu_handle_list[sampler_index].ptr = descriptor_cpu->GetHandle(render_pass.sampler_index_list[s], DescriptorType::kSampler).ptr;
    sampler_index++;
  }
  auto gpu_handle_list = AllocateAndFillArrayFrame(sampler_list_num, D3D12_GPU_DESCRIPTOR_HANDLE{});
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
auto GetRenderPassQueueIndexList(const uint32_t render_pass_num, const RenderPass* render_pass_list) {
  auto render_pass_queue_index = AllocateArraySystem<uint32_t>(render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_queue_index[i] = render_pass_list[i].command_queue_index;
  }
  return render_pass_queue_index;
}
auto GetRenderPassIndexPerQueue(const uint32_t command_queue_num, const uint32_t render_pass_num, const uint32_t* render_pass_queue_index) {
  auto render_pass_num_per_queue = AllocateAndFillArraySystem(command_queue_num, 0U);
  auto render_pass_index_per_queue = AllocateArraySystem<uint32_t>(render_pass_num);
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
auto RegisterGuiPerformance(const TimeDurationDataSet& time_duration_data_set, const GpuTimeDurations& gpu_time_durations, const char* const * render_pass_name, const uint32_t* const* serialized_render_pass_index) {
  ImGui::SetNextWindowSize(ImVec2{});
  if (!ImGui::Begin("performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  ImGui::Text("CPU: %7.4f", time_duration_data_set.prev_duration_per_frame_msec_avg);
  ImGui::Text("GPU: %7.4f", gpu_time_durations.total_time_msec);
  if (ImGui::BeginTable("render pass", 2)) {
    for (uint32_t i = 0; i < gpu_time_durations.command_queue_num; i++) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("queue%d", i);
      for (uint32_t j = 0; j < gpu_time_durations.duration_num[i]; j++) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if ((j & 1) == 0) {
          ImGui::Text("-");
        } else {
          ImGui::Text("%s", render_pass_name[serialized_render_pass_index[i][j / 2]]);
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%7.4f", gpu_time_durations.duration_msec[i][j]);
      }
    }
    ImGui::EndTable();
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
auto RegisterGuiCBufferParams(const ArrayOf<CBufferParam> cbuffer_params, void* dst) {
  for (uint32_t i = 0; i < cbuffer_params.size; i++) {
    const auto& param = cbuffer_params.array[i];
    switch (param.type) {
      case CBufferParamType::kSpecial: {
        break;
      }
      case CBufferParamType::kUint: {
        auto v = static_cast<int*>(dst);
        ImGui::SliderInt(param.name, v, GetUint32(param.min), GetUint32(param.max));
        break;
      }
      case CBufferParamType::kFloat: {
        auto v = static_cast<float*>(dst);
        ImGui::SliderFloat(param.name, v, param.min, param.max);
        break;
      }
    }
    dst = SucceedPtr(dst, param.size_in_bytes);
  }
}
auto RegisterGuiCBufferList(const ArrayOf<CBuffer>& cbuffer_list, const char* const * const buffer_name_list, void* const * cbuffer_dst_list) {
  if (!ImGui::Begin("cbuffer params", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { return; }
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    if (cbuffer_list.array[i].need_ui_param_num == 0) { continue; }
    ImGui::Separator();
    ImGui::Text(buffer_name_list[cbuffer_list.array[i].buffer_index]);
    RegisterGuiCBufferParams(cbuffer_list.array[i].params, cbuffer_dst_list[i]);
  }
  ImGui::End();
}
auto RegisterGui(RenderPassConfigDynamicData* dynamic_data, const TimeDurationDataSet& time_duration_data_set, const uint32_t debug_viewable_buffer_num, const char* const * debug_viewable_buffer_name_list, bool* debug_buffer_view_enabled, int32_t* debug_buffer_selected_index, const GpuTimeDurations& gpu_time_durations, const char* const * render_pass_name, const uint32_t* const* serialized_render_pass_index, const ArrayOf<CBuffer>& cbuffer_list, const char* const * const buffer_name_list, void* const * cbuffer_dst_list) {
  RegisterGuiPerformance(time_duration_data_set, gpu_time_durations, render_pass_name, serialized_render_pass_index);
  RegisterGuiCamera(dynamic_data);
  RegisterGuiLight(dynamic_data);
  RegisterGuiDebugView(debug_viewable_buffer_num, debug_viewable_buffer_name_list, debug_buffer_view_enabled, debug_buffer_selected_index);
  RegisterGuiCBufferList(cbuffer_list, buffer_name_list, cbuffer_dst_list);
}
RenderPassConfigDynamicData InitRenderPassDynamicData() {
  RenderPassConfigDynamicData dynamic_data{};
  dynamic_data.camera_focus[1] = 1.0f;
  dynamic_data.camera_pos[0] = 5.0f;
  dynamic_data.camera_pos[1] = 1.5f;
  dynamic_data.light_direction[0] = -1.0f;
  dynamic_data.light_direction[1] = -1.0f;
  dynamic_data.light_direction[2] = 1.0f;
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
auto PrepareBarrierResourceList(const uint32_t render_pass_num, const BarrierConfigList* barrier_config_list, const BufferList& buffer_list, const MemoryType memory_type) {
  auto resource_list = AllocateArray<ID3D12Resource***>(memory_type, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    resource_list[i] = AllocateArray<ID3D12Resource**>(memory_type, kBarrierExecutionTimingNum);
    for (uint32_t j = 0; j < kBarrierExecutionTimingNum; j++) {
      resource_list[i][j] = AllocateArray<ID3D12Resource*>(memory_type, barrier_config_list[i][j].size);
      for (uint32_t k = 0; k < barrier_config_list[i][j].size; k++) {
        resource_list[i][j][k] = GetResource(buffer_list, barrier_config_list[i][j].array[k].buffer_allocation_index);
      }
    }
  }
  return resource_list;
}
void PrintNames(const uint32_t num, const char* const * names) {
  for (uint32_t i = 0; i < num; i++) {
    logdebug("{:2}:{}", i, names[i]);
  }
}
auto GatherRenderPassSyncInfoForBarriers(const uint32_t render_pass_num, const RenderPass* render_pass_list) {
  auto wait_pass_num = AllocateArrayFrame<uint32_t>(render_pass_num);
  auto signal_pass_index = AllocateArrayFrame<uint32_t*>(render_pass_num);
  auto render_pass_command_queue_index = AllocateArrayFrame<uint32_t>(render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    wait_pass_num[i] = render_pass_list[i].wait_pass_num;
    signal_pass_index[i] = render_pass_list[i].signal_pass_index;
    render_pass_command_queue_index[i] = render_pass_list[i].command_queue_index;
  }
  return std::make_tuple(wait_pass_num, signal_pass_index, render_pass_command_queue_index);
}
auto GetBufferCreationSize(const uint32_t writable_size_in_bytes) {
  return AlignAddress(writable_size_in_bytes, 256);
}
auto FillCbvBufferCreationSize(const ArrayOf<CBuffer>& cbuffer_list, const uint32_t* cbuffer_writable_size, BufferConfig* buffer_config_list) {
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    const auto buffer_index = cbuffer_list.array[i].buffer_index;
    buffer_config_list[buffer_index].width = static_cast<float>(GetBufferCreationSize(cbuffer_writable_size[i]));
    buffer_config_list[buffer_index].height = 1.0f;
  }
}
auto PrepareCbvPointers(const BufferConfig* buffer_config_list, const ArrayOf<CBuffer>& cbuffer_list, const uint32_t* writable_size_in_bytes, const uint32_t frame_buffer_num, BufferList* buffer_list) {
  auto cbv_ptr_list = AllocateArrayScene<void**>(cbuffer_list.size);
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    const auto buffer_index = cbuffer_list.array[i].buffer_index;
    assert(buffer_config_list[buffer_index].pingpong == false && "cbv pingpong buffer not implmented");
    const auto allocated_num = GetBufferAllocationNum(buffer_config_list[buffer_index], frame_buffer_num);
    cbv_ptr_list[i] = AllocateArrayScene<void*>(allocated_num);
    for (uint32_t j = 0; j < allocated_num; j++) {
      cbv_ptr_list[i][j] = MapResource(GetResource(*buffer_list, buffer_index, j), writable_size_in_bytes[i]);
    }
  }
  return cbv_ptr_list;
}
auto GetAllQueueSeirializedRenderPassIndexInQueueArrayForm(const uint32_t command_queue_num, const uint32_t* render_pass_num_per_queue, const uint32_t render_pass_num, const uint32_t* render_pass_command_queue_index) {
  auto serialized_render_pass_index = AllocateArrayFrame<uint32_t*>(command_queue_num);
  auto counter = AllocateArrayFrame<uint32_t>(command_queue_num);
  for (uint32_t i = 0; i < command_queue_num; i++) {
    serialized_render_pass_index[i] = AllocateArrayFrame<uint32_t>(render_pass_num_per_queue[i]);
    counter[i] = 0;
  }
  for (uint32_t i = 0; i < render_pass_num; i++) {
    serialized_render_pass_index[render_pass_command_queue_index[i]][counter[render_pass_command_queue_index[i]]] = i;
    counter[render_pass_command_queue_index[i]]++;
  }
  return serialized_render_pass_index;
}
auto ConvertToResourceStateTypeFlags(const uint32_t render_pass_num, const uint32_t* const render_pass_buffer_num_list, const ResourceStateType* const* render_pass_buffer_state_list) {
  auto render_pass_buffer_state_list_for_barrier = AllocateAndFillArrayFrame(render_pass_num, (ResourceStateTypeFlags::FlagType*)nullptr);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_buffer_state_list_for_barrier[i] = AllocateArrayFrame<ResourceStateTypeFlags::FlagType>(render_pass_buffer_num_list[i]);
    for (uint32_t j = 0; j < render_pass_buffer_num_list[i]; j++) {
      render_pass_buffer_state_list_for_barrier[i][j] = ResourceStateTypeFlags::GetResourceStateTypeFlag(render_pass_buffer_state_list[i][j]);
    }
  }
  return render_pass_buffer_state_list_for_barrier;
}
auto UpdateBufferFinalState(const uint32_t render_pass_num, const BarrierConfigList * barrier_config_list, ResourceStateTypeFlags::FlagType* prev_buffer_final_state) {
  for (uint32_t i = 0; i < render_pass_num; i++) {
    for (uint32_t j = 0; j < kBarrierExecutionTimingNum; j++) {
      for (uint32_t k = 0; k < barrier_config_list[i][j].size; k++) {
        auto& barrier = barrier_config_list[i][j].array[k];
        prev_buffer_final_state[barrier.buffer_allocation_index] = barrier.state_after;
      }
    }
  }
}
auto GatherBufferInitialState(const uint32_t buffer_allocation_num, const BufferConfig* buffer_config_list, const BufferList& buffer_list) {
  auto buffer_initial_state = AllocateArrayFrame<ResourceStateTypeFlags::FlagType>(buffer_allocation_num);
  for (uint32_t i = 0; i < buffer_allocation_num; i++) {
    const auto& buffer_config = buffer_config_list[buffer_list.buffer_config_index[i]];
    buffer_initial_state[i] = ResourceStateTypeFlags::GetResourceStateTypeFlag(buffer_config.initial_state);
  }
  return buffer_initial_state;
}
auto FillCBufferParamSizeInBytes(ArrayOf<CBuffer>* cbuffer_list) {
  for (uint32_t i = 0; i < cbuffer_list->size; i++) {
    for (uint32_t j = 0; j < cbuffer_list->array[i].params.size; j++) {
      auto& param = cbuffer_list->array[i].params.array[j];
      uint32_t size_in_bytes = 4;
      if (param.type == CBufferParamType::kSpecial) {
        size_in_bytes = GetCBufferParamSizeInBytes(param.name_hash);
      }
      param.size_in_bytes = size_in_bytes;
    }
  }
}
auto GetCBufferSrcWritableSize(const ArrayOf<CBuffer>& cbuffer_list) {
  auto cbuffer_writable_size = AllocateAndFillArrayFrame<uint32_t>(cbuffer_list.size, 0);
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    uint32_t size_in_bytes = 0;
    for (uint32_t j = 0; j < cbuffer_list.array[i].params.size; j++) {
      size_in_bytes += cbuffer_list.array[i].params.array[j].size_in_bytes;
    }
    cbuffer_writable_size[i] = size_in_bytes;
  }
  return cbuffer_writable_size;
}
auto CreateCBufferSrcData(const ArrayOf<CBuffer>& cbuffer_list, const uint32_t* cbuffer_writable_size) {
  auto cbuffer_src_data = AllocateArrayFrame<void*>(cbuffer_list.size);
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    const auto size_in_bytes = cbuffer_writable_size[i];
    cbuffer_src_data[i] = AllocateFrame(size_in_bytes);
    auto dst = cbuffer_src_data[i];
    const auto& params = cbuffer_list.array[i].params;
    for (uint32_t j = 0; j < params.size; j++) {
      const auto& param = params.array[j];
      switch (param.type) {
        case CBufferParamType::kSpecial: {
          break;
        }
        case CBufferParamType::kUint: {
          auto v = static_cast<uint32_t*>(dst);
          *v = static_cast<uint32_t>(param.initial_val);
          break;
        }
        case CBufferParamType::kFloat: {
          auto v = static_cast<float*>(dst);
          *v = param.initial_val;
          break;
        }
      }
      dst = SucceedPtr(dst, param.size_in_bytes);
    }
  }
  return cbuffer_src_data;
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
  RenderGraphConfig render_graph;
  void** render_pass_vars{nullptr};
  RenderPassFunctionList render_pass_function_list{};
  uint32_t swapchain_buffer_allocation_index{kInvalidIndex};
  float prev_mouse_pos[2]{};
  uint32_t render_pass_index_output_to_swapchain{};
  uint32_t render_pass_buffer_index_primary_input{};
  const char** render_pass_name{nullptr};
  auto frame_loop_num = kFrameLoopNum;
  auto material_pack = BuildMaterialList(device.Get(), GetTestJson("material.json"));
  void*** cbv_ptr_list{nullptr}; // [buffer_config_index][frame_index]
  ResourceStateTypeFlags::FlagType* prev_buffer_final_state{nullptr};
  uint32_t* cbuffer_writable_size{nullptr};
  void** cbuffer_src_data{nullptr};
  const char* const * buffer_name_list{};
  {
    nlohmann::json json;
    SUBCASE("deferred.json") {
      json = GetTestJson("deferred.json");
      frame_loop_num = 10000;
    }
    SUBCASE("forward.json") {
      json = GetTestJson("forward.json");
    }
    SUBCASE("config.json") {
      json = GetTestJson("config.json");
    }
    auto [buffer_name_list_tmp, buffer_name_hash_list] = ParseRenderGraphJson(json,
                                                                          material_pack.material_list.material_num,
                                                                          material_pack.config.material_hash_list,
                                                                          material_pack.config.rtv_format_list,
                                                                          material_pack.config.dsv_format,
                                                                          &render_graph);
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
        .width = render_graph.primarybuffer_width,
        .height = render_graph.primarybuffer_height,
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
      }
    }
#endif
    buffer_name_list = CopyStringList(MemoryType::kSystem, render_graph.buffer_num, buffer_name_list_tmp);
    PrintNames(render_graph.buffer_num, buffer_name_list);
    FillCBufferParamSizeInBytes(&render_graph.cbuffer_list);
    cbuffer_writable_size = GetCBufferSrcWritableSize(render_graph.cbuffer_list);
    cbuffer_src_data = CreateCBufferSrcData(render_graph.cbuffer_list, cbuffer_writable_size);
    FillCbvBufferCreationSize(render_graph.cbuffer_list, cbuffer_writable_size, render_graph.buffer_list);
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, render_graph.frame_buffer_num, buffer_allocator);
    prev_buffer_final_state = GatherBufferInitialState(buffer_list.buffer_allocation_num, render_graph.buffer_list, buffer_list);
    cbv_ptr_list = PrepareCbvPointers(render_graph.buffer_list, render_graph.cbuffer_list, cbuffer_writable_size, render_graph.frame_buffer_num, &buffer_list);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.descriptor_handle_num_per_type));
    CHECK_UNARY(command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList()));
    for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
      const auto buffer_config_index = buffer_list.buffer_config_index[i];
      auto& buffer_config = render_graph.buffer_list[buffer_config_index];
      CHECK_UNARY(CreateCpuHandleWithView(buffer_config, i, buffer_list.resource_list[i], &descriptor_cpu, device.Get()));
      if (!buffer_config.descriptor_only) {
        char c = '\0';
        if (buffer_config.pingpong) {
          c = IsPingPongMainBuffer(buffer_list, buffer_config_index, i) ? 'A' : 'B';
        } else if (buffer_config.frame_buffered) {
          c = static_cast<char>(GetBufferLocalIndex(buffer_list, buffer_config_index, i)) + '0';
        }
        auto str = buffer_name_list[buffer_config_index];
        if (c == '\0') {
          SetD3d12Name(buffer_list.resource_list[i], str);
        } else {
          const uint32_t buf_len = 128;
          char buf[buf_len];
          snprintf(buf, buf_len, "%s_%c", str, c);
          SetD3d12Name(buffer_list.resource_list[i], buf);
        }
      }
      auto strhash = buffer_name_hash_list[buffer_config_index];
      if (strhash == SID("swapchain")) {
        swapchain_buffer_allocation_index = i;
        logdebug("swapchain config:{} allocation:{}", buffer_config_index, swapchain_buffer_allocation_index);
      }
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(i, DescriptorType::kSampler);
      CHECK_NE(cpu_handler.ptr, 0);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], cpu_handler);
    }
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view + render_graph.max_material_num * kTextureNumPerMaterial, render_graph.gpu_handle_num_sampler));
    render_pass_vars = AllocateArraySystem<void*>(render_graph.render_pass_num);
    render_pass_name = AllocateArraySystem<const char*>(render_graph.render_pass_num);
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
      auto pass_name = GetStringView(render_pass_list_json[i], ("name"));
      render_pass_name[i] = CreateString(pass_name, MemoryType::kSystem);
      if (render_graph.render_pass_list[i].name == SID("output to swapchain")) {
        render_pass_index_output_to_swapchain = i;
        for (uint32_t j = 0; j < render_graph.render_pass_list[i].buffer_num; j++) {
          if (buffer_name_hash_list[j] == SID("primary")) {
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
  for (uint32_t i = 0; i < frame_loop_num; i++) {
    if (!window.ProcessMessage()) { break; }
    ResetAllocation(MemoryType::kFrame);
    const auto gpu_time_durations_per_frame = GetGpuTimeDurationsPerFrame(render_graph.command_queue_num, render_pass_num_per_queue, gpu_timestamp_set, MemoryType::kFrame);
    AccumulateGpuTimeDuration(gpu_time_durations_per_frame, &gpu_time_durations_accumulated);
    if (const auto frame_count = time_duration_data_set.frame_count; UpdateTimeDuration(time_duration_data_set.frame_count_reset_time_threshold_msec, &time_duration_data_set.frame_count, &time_duration_data_set.last_time_point, &time_duration_data_set.delta_time_msec, &time_duration_data_set.duration_msec_sum, &time_duration_data_set.prev_duration_per_frame_msec_avg)) {
      CalcAvarageGpuTimeDuration(gpu_time_durations_accumulated, frame_count, &gpu_time_durations_average);
      ClearGpuTimeDuration(&gpu_time_durations_accumulated);
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
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
    auto render_pass_buffer_num_list = GetRenderPassBufferNumList(render_graph.render_pass_num, render_graph.render_pass_list, MemoryType::kFrame);
    auto render_pass_buffer_state_list_for_barrier = ConvertToResourceStateTypeFlags(render_graph.render_pass_num, render_pass_buffer_num_list, render_pass_buffer_state_list);
    auto buffer_final_state = AllocateAndFillArrayFrame(buffer_list.buffer_allocation_num, ResourceStateTypeFlags::kNone);
    prev_buffer_final_state[swapchain_buffer_allocation_index] = ResourceStateTypeFlags::kPresent;
    buffer_final_state[swapchain_buffer_allocation_index]      = ResourceStateTypeFlags::kPresent;
    auto [render_pass_wait_pass_num, render_pass_signal_pass_index, render_pass_command_queue_index] = GatherRenderPassSyncInfoForBarriers(render_graph.render_pass_num, render_graph.render_pass_list);
    const auto [barrier_config_list, state_at_frame_end] = ConfigureBarrierTransitions(buffer_list.buffer_allocation_num, render_graph.render_pass_num,
                                                                                       render_pass_buffer_num_list, render_pass_buffer_allocation_index_list, render_pass_buffer_state_list_for_barrier,
                                                                                       render_pass_wait_pass_num, render_pass_signal_pass_index, render_pass_command_queue_index, render_graph.command_queue_type,
                                                                                       prev_buffer_final_state, buffer_final_state,
                                                                                       MemoryType::kFrame);
    memcpy(prev_buffer_final_state, state_at_frame_end, sizeof(ResourceStateTypeFlags::FlagType) * buffer_list.buffer_allocation_num);
    auto barrier_resource_list = PrepareBarrierResourceList(render_graph.render_pass_num, barrier_config_list, buffer_list, MemoryType::kFrame);
    {
      // imgui
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();
      UpdateCameraFromUserInput(main_buffer_size.swapchain, dynamic_data.camera_pos, dynamic_data.camera_focus, prev_mouse_pos);
    }
    auto serialized_render_pass_index = GetAllQueueSeirializedRenderPassIndexInQueueArrayForm(render_graph.command_queue_num, render_pass_num_per_queue, render_graph.render_pass_num, render_pass_command_queue_index);
    auto current_frame_cbv_ptr_list = AllocateArrayFrame<void*>(render_graph.cbuffer_list.size);
    for (uint32_t k = 0; k < render_graph.cbuffer_list.size; k++) {
      const auto cbuffer_index = render_graph.cbuffer_list.array[k].buffer_index;
      const auto cbv_index = render_graph.buffer_list[cbuffer_index].frame_buffered ? frame_index : 0;
      current_frame_cbv_ptr_list[k] = cbv_ptr_list[k][cbv_index];
    }
    RegisterGui(&dynamic_data, time_duration_data_set, debug_viewable_buffer_allocation_num, debug_viewable_buffer_name_list, &debug_buffer_view_enabled, &debug_buffer_selected_index, gpu_time_durations_average, render_pass_name, serialized_render_pass_index, render_graph.cbuffer_list, buffer_name_list, cbuffer_src_data);
    FillShaderBoundCBuffers(dynamic_data, main_buffer_size, render_graph.cbuffer_list, cbuffer_src_data, cbuffer_writable_size, current_frame_cbv_ptr_list);
    // update
    RenderPassFuncArgsRenderCommon args_common {
      .main_buffer_size = &main_buffer_size,
      .scene_data = &scene_data,
      .frame_index = frame_index,
      .dynamic_data = &dynamic_data,
      .render_pass_list = render_graph.render_pass_list,
      .material_list = &material_pack.material_list,
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
      if (barrier_config_list[0][k].size == 0 && barrier_config_list[1][k].size == 0 && !IsRenderPassRenderNeeded(&render_pass_function_list, &args_common, &args_per_pass[k])) { continue; }
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
      ExecuteBarrier(command_list, barrier_config_list[k][0].size, barrier_config_list[k][0].array, barrier_resource_list[k][0]);
      RenderPassRender(&render_pass_function_list, &args_common, &args_per_pass[k]);
      ExecuteBarrier(command_list, barrier_config_list[k][1].size, barrier_config_list[k][1].array, barrier_resource_list[k][1]);
#ifdef USE_GRAPHICS_DEBUG_SCOPE
      if (!IsDebuggerPresent() || command_list && render_graph.command_queue_type[render_pass_queue_index[k]] != D3D12_COMMAND_LIST_TYPE_COPY) {
        // debugger issues an error on copy queue
        PIXEndEvent(command_list);
      }
#endif
      EndGpuTimestamp(render_pass_index_per_queue, render_pass_queue_index, k, &gpu_timestamp_set, command_list);
      const auto is_last_pass_per_queue = (last_pass_per_queue[render_pass_queue_index[k]] == k);
      if (is_last_pass_per_queue) {
        OutputGpuTimestampToCpuVisibleBuffer(render_pass_num_per_queue, render_pass_queue_index, k, &gpu_timestamp_set, command_list);
      }
      if (render_pass.sends_signal || is_last_pass_per_queue) {
        command_list_set.ExecuteCommandList(render_pass_queue_index[k]);
        prev_command_list[render_pass_queue_index[k]] = nullptr;
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
  ReleasePsoAndRootsig(&material_pack.material_list);
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
