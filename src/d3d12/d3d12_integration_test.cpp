#include "doctest/doctest.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
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
namespace illuminate {
namespace {
auto GetTestJson() {
  return R"(
{
  "frame_buffer_num": 2,
  "frame_loop_num": 5,
  "primarybuffer_width": 300,
  "primarybuffer_height": 400,
  "primarybuffer_format": "R8G8B8A8_UNORM",
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
      "size_type": "swapchain_relative",
      "width": 1.0,
      "height": 1.0,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "flags": ["uav"],
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "uav"
    },
    {
      "name": "dsv",
      "format": "D24_UNORM_S8_UINT",
      "heap_type": "default",
      "dimension": "texture2d",
      "size_type": "primary_relative",
      "width": 1.0,
      "height": 1.0,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "flags": ["dsv"],
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "dsv_write",
      "clear_depth": 1,
      "clear_stencil": 0
    }
  ],
  "sampler": [
    {
      "name": "bilinear",
      "filter_min": "linear",
      "filter_mag": "linear",
      "filter_mip": "point",
      "address_mode": ["clamp", "clamp"],
      "mip_lod_bias": 0,
      "max_anisotropy": 0,
      "comparison_func": "never",
      "min_lod": 0,
      "max_lod": 65535
    },
    {
      "name": "trilinear",
      "filter_min": "linear",
      "filter_mag": "linear",
      "filter_mip": "linear",
      "address_mode": ["clamp", "clamp", "clamp"],
      "mip_lod_bias": 0,
      "max_anisotropy": 0,
      "comparison_func": "never",
      "border_color": [0,0,0,0],
      "min_lod": 0,
      "max_lod": 65535
    }
  ],
  "descriptor": [
    {
      "name": "imgui_font",
      "type": "srv"
    }
  ],
  "render_pass": [
    {
      "name": "imgui_newframe",
      "need_command_list": false,
      "buffer_list": [
        {
          "name": "imgui_font",
          "state": "srv"
        }
      ]
    },
    {
      "name": "dispatch cs",
      "command_queue": "queue_compute",
      "execute": true,
      "buffer_list": [
        {
          "name": "uav",
          "state": "uav"
        }
      ],
      "pass_vars": {
        "thread_group_count_x": 32,
        "thread_group_count_y": 32,
        "shader": "test.cs.hlsl",
        "shader_compile_args":["-T", "cs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      }
    },
    {
      "name": "prez",
      "command_queue": "queue_graphics",
      "execute": true,
      "buffer_list": [
        {
          "name": "dsv",
          "state": "dsv_write"
        }
      ],
      "pass_vars": {
        "shader": "prez.vs.hlsl",
        "shader_compile_args":["-T", "vs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"],
        "stencil_val": 255
      }
    },
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "wait_pass": ["dispatch cs"],
      "execute": true,
      "buffer_list": [
        {
          "name": "uav",
          "state": "srv"
        },
        {
          "name": "dsv",
          "state": "srv"
        },
        {
          "name": "swapchain",
          "state": "rtv"
        }
      ],
      "sampler": ["bilinear"],
      "prepass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "present",
          "state_after": "rtv"
        },
        {
          "buffer_name": "uav",
          "type": "transition",
          "split_type": "none",
          "state_before": "uav",
          "state_after": "srv_ps"
        },
        {
          "buffer_name": "dsv",
          "type": "transition",
          "split_type": "none",
          "state_before": "dsv_write",
          "state_after": "srv_ps"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "uav",
          "type": "transition",
          "split_type": "none",
          "state_before": "srv_ps",
          "state_after": "uav"
        },
        {
          "buffer_name": "dsv",
          "type": "transition",
          "split_type": "none",
          "state_before": "srv_ps",
          "state_after": "dsv_write"
        }
      ],
      "pass_vars": {
        "shader_vs": "test.vs.hlsl",
        "shader_compile_args_vs":["-T", "vs_6_6", "-E", "MainVs", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"],
        "shader_ps": "test.ps.hlsl",
        "shader_compile_args_ps":["-T", "ps_6_6", "-E", "MainPs", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      }
    },
    {
      "name": "imgui",
      "command_queue": "queue_graphics",
      "execute": true,
      "buffer_list": [
        {
          "name": "swapchain",
          "state": "rtv"
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
struct CsDispatchParams {
  ID3D12RootSignature* rootsig{nullptr};
  ID3D12PipelineState* pso{nullptr};
  uint32_t thread_group_count_x{0};
  uint32_t thread_group_count_y{0};
};
struct PrezResourceSet {
  ID3D12RootSignature* rootsig{nullptr};
  ID3D12PipelineState* pso{nullptr};
  uint32_t stencil_val;
};
struct ShaderResourceSet {
  ID3D12RootSignature* rootsig{nullptr};
  ID3D12PipelineState* pso{nullptr};
};
struct RenderPassArgs {
  D3d12CommandList* command_list;
  const MainBufferSize* main_buffer_size;
  const void* pass_vars_ptr;
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles;
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles;
  ID3D12Resource** resources;
  const SceneData* scene_data;
};
using RenderPassFunction = void (*)(RenderPassArgs*);
void RunImguiNewFrame(RenderPassArgs* args) {
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::GetIO().Fonts->SetTexID((ImTextureID)args->gpu_handles[0].ptr);
  ImGui::NewFrame();
}
void DispatchCs(RenderPassArgs* args) {
  auto pass_vars = static_cast<const CsDispatchParams*>(args->pass_vars_ptr);
  args->command_list->SetComputeRootSignature(pass_vars->rootsig);
  args->command_list->SetPipelineState(pass_vars->pso);
  args->command_list->SetComputeRootDescriptorTable(0, args->gpu_handles[0]);
  args->command_list->Dispatch(args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_x, args->main_buffer_size->swapchain.width / pass_vars->thread_group_count_y, 1);
}
void PreZ(RenderPassArgs* args) {
  args->command_list->ClearDepthStencilView(args->cpu_handles[0], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
  auto pass_vars = static_cast<const PrezResourceSet*>(args->pass_vars_ptr);
  auto& width = args->main_buffer_size->primarybuffer.width;
  auto& height = args->main_buffer_size->primarybuffer.height;
  args->command_list->SetGraphicsRootSignature(pass_vars->rootsig);
  D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
  args->command_list->RSSetViewports(1, &viewport);
  D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
  args->command_list->RSSetScissorRects(1, &scissor_rect);
  args->command_list->SetPipelineState(pass_vars->pso);
  args->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  args->command_list->OMSetRenderTargets(0, nullptr, true, args->cpu_handles);
  args->command_list->OMSetStencilRef(pass_vars->stencil_val);
  for (uint32_t i = 0; i < args->scene_data->model_num; i++) {
    auto mesh_index = args->scene_data->model_mesh_index[i];
    args->command_list->IASetIndexBuffer(&args->scene_data->mesh_index_buffer_view[mesh_index]);
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[] = {
      args->scene_data->mesh_vertex_buffer_view_position[mesh_index],
    };
    args->command_list->IASetVertexBuffers(0, 1, vertex_buffer_view);
    args->command_list->DrawIndexedInstanced(args->scene_data->mesh_index_buffer_len[mesh_index], 1, 0, 0, 0);
    // auto material_index = scene_data->model_material_index[i];
  }
}
void CopyResourceVsPs(RenderPassArgs* args) {
  auto pass_vars = static_cast<const ShaderResourceSet*>(args->pass_vars_ptr);
  auto& width = args->main_buffer_size->swapchain.width;
  auto& height = args->main_buffer_size->swapchain.height;
  args->command_list->SetGraphicsRootSignature(pass_vars->rootsig);
  D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
  args->command_list->RSSetViewports(1, &viewport);
  D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
  args->command_list->RSSetScissorRects(1, &scissor_rect);
  args->command_list->SetPipelineState(pass_vars->pso);
  args->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  args->command_list->OMSetRenderTargets(1, &args->cpu_handles[2], true, nullptr);
  args->command_list->SetGraphicsRootDescriptorTable(0, args->gpu_handles[0]); // srv
  args->command_list->SetGraphicsRootDescriptorTable(1, args->gpu_handles[1]); // sampler
  args->command_list->DrawInstanced(3, 1, 0, 0);
}
void RunImgui(RenderPassArgs* args) {
  ImGui::Text("Hello, world %d", 123);
  ImGui::Render();
  args->command_list->OMSetRenderTargets(1, &args->cpu_handles[0], true, nullptr);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args->command_list);
}
auto GetShaderCompilerArgs(const nlohmann::json& j, const char* const name, MemoryAllocationJanitor* allocator, std::wstring** wstr_args, const wchar_t*** args) {
  auto shader_compile_args = j.at(name);
  auto args_num = static_cast<uint32_t>(shader_compile_args.size());
  *wstr_args = AllocateArray<std::wstring>(allocator, args_num);
  *args = AllocateArray<const wchar_t*>(allocator, args_num);
  for (uint32_t i = 0; i < args_num; i++) {
    auto str = shader_compile_args[i].get<std::string_view>();
    (*wstr_args)[i] = std::wstring(str.begin(), str.end()); // assuming string content is single-byte charcters only.
    (*args)[i] = (*wstr_args)[i].c_str();
  }
  return args_num;
}
struct RenderPassInitParams {
  const nlohmann::json* json{nullptr};
  const char* shader_code{nullptr};
  ShaderCompiler* shader_compiler{nullptr};
  DescriptorCpu<MemoryAllocationJanitor>* descriptor_cpu{nullptr};
  D3d12Device* device{nullptr};
  const MainBufferFormat& main_buffer_format{};
  HWND hwnd{nullptr};
  uint32_t frame_buffer_num{0};
};
void ParsePassParamDispatchCs(RenderPassInitParams* init_params, void* dst) {
  auto param = static_cast<CsDispatchParams*>(dst);
  *param = {};
  std::wstring* wstr_args{nullptr};
  const wchar_t** args{nullptr};
  auto allocator = GetTemporalMemoryAllocator();
  auto args_num = GetShaderCompilerArgs(*init_params->json, "shader_compile_args", &allocator, &wstr_args, &args);
  assert(args_num > 0);
  if (!init_params->shader_compiler->CreateRootSignatureAndPsoCs(init_params->shader_code, static_cast<uint32_t>(strlen(init_params->shader_code)), args_num, args, init_params->device, &param->rootsig, &param->pso)) {
    logerror("compute shader parse error");
    assert(false && "compute shader parse error");
  }
  SetD3d12Name(param->rootsig, "rootsig_cs");
  SetD3d12Name(param->pso, "pso_cs");
  param->thread_group_count_x = init_params->json->at("thread_group_count_x");
  param->thread_group_count_y = init_params->json->at("thread_group_count_y");
}
void ParsePassParamPreZ(RenderPassInitParams* init_params, void* dst) {
  auto param = static_cast<PrezResourceSet*>(dst);
  *param = {};
  std::wstring* wstr_args{nullptr};
  const wchar_t** args{nullptr};
  auto allocator = GetTemporalMemoryAllocator();
  auto args_num = GetShaderCompilerArgs(*init_params->json, "shader_compile_args", &allocator, &wstr_args, &args);
  assert(args_num > 0);
  ShaderCompiler::PsoDescVsPs pso_desc{};
  pso_desc.depth_stencil_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  D3D12_INPUT_ELEMENT_DESC input_element_descs[] = {
    {
      .SemanticName = "POSITION",
      .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32_FLOAT,
      .InputSlot = 0,
      .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0,
    },
  };
  pso_desc.depth_stencil1.StencilEnable = true;
  pso_desc.input_layout.pInputElementDescs = input_element_descs;
  pso_desc.input_layout.NumElements = 1;
  if (!init_params->shader_compiler->CreateRootSignatureAndPsoVs(init_params->shader_code, static_cast<uint32_t>(strlen(init_params->shader_code)), args_num, args, init_params->device, &pso_desc, &param->rootsig, &param->pso)) {
    logerror("vs parse error");
    assert(false && "vs parse error");
  }
  SetD3d12Name(param->rootsig, "rootsig_prez");
  SetD3d12Name(param->pso, "pso_prez");
  param->stencil_val = GetNum(*init_params->json, "stencil_val", 0);
}
void ParsePassParamVsPsCopyResource(RenderPassInitParams* init_params, void* dst) {
  auto param = static_cast<ShaderResourceSet*>(dst);
  *param = {};
  auto allocator = GetTemporalMemoryAllocator();
  std::wstring* wstr_args_vs{nullptr};
  const wchar_t** args_vs{nullptr};
  auto args_num_vs = GetShaderCompilerArgs(*init_params->json, "shader_compile_args_vs", &allocator, &wstr_args_vs, &args_vs);
  assert(args_num_vs > 0);
  std::wstring* wstr_args_ps{nullptr};
  const wchar_t** args_ps{nullptr};
  auto args_num_ps = GetShaderCompilerArgs(*init_params->json, "shader_compile_args_ps", &allocator, &wstr_args_ps, &args_ps);
  assert(args_num_ps > 0);
  ShaderCompiler::PsoDescVsPs pso_desc{};
  pso_desc.render_target_formats.RTFormats[0] = init_params->main_buffer_format.swapchain;
  pso_desc.render_target_formats.NumRenderTargets = 1;
  pso_desc.depth_stencil1.DepthEnable = false;
  if (!init_params->shader_compiler->CreateRootSignatureAndPsoVsPs(init_params->shader_code, static_cast<uint32_t>(strlen(init_params->shader_code)), args_num_vs, args_vs,
                                                                   init_params->shader_code, static_cast<uint32_t>(strlen(init_params->shader_code)), args_num_ps, args_ps,
                                                                   init_params->device, &pso_desc, &param->rootsig, &param->pso)) {
    logerror("vs ps parse error");
    assert(false && "vs ps parse error");
  }
  SetD3d12Name(param->rootsig, "rootsig_swapchain");
  SetD3d12Name(param->pso, "pso_swapchain");
}
void InitImgui(RenderPassInitParams* init_params) {
  auto cpu_handle_font = init_params->descriptor_cpu->GetHandle(SID("imgui_font"), DescriptorType::kSrv);
  assert(cpu_handle_font);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ImGui_ImplWin32_Init(init_params->hwnd);
  ImGui_ImplDX12_Init(init_params->device, init_params->frame_buffer_num, init_params->main_buffer_format.swapchain, nullptr/*descriptor heap not used in single viewport mode*/, *cpu_handle_font, {}/*gpu_handle updated every frame before rendering*/);
}
void ReleaseResourceDispatchCs(void* ptr) {
  auto param = static_cast<CsDispatchParams*>(ptr);
  if (param->pso->Release() != 0) {
    logwarn("ReleaseResourceDispatchCs pso ref left.");
  }
  if (param->rootsig->Release() != 0) {
    logwarn("ReleaseResourceDispatchCs rootsig ref left.");
  }
}
void ReleaseResourcePreZ(void* ptr) {
  auto param = static_cast<PrezResourceSet*>(ptr);
  if (param->pso->Release() != 0) {
    logwarn("ReleaseResourceDispatchCs pso ref left.");
  }
  if (param->rootsig->Release() != 0) {
    logwarn("ReleaseResourceDispatchCs rootsig ref left.");
  }
}
void ReleaseResourceShaderResourceSet(void* ptr) {
  auto param = static_cast<ShaderResourceSet*>(ptr);
  if (param->pso->Release() != 0) {
    logwarn("ReleaseResourceShaderResourceSet pso ref left.");
  }
  if (param->rootsig->Release() != 0) {
    logwarn("ReleaseResourceShaderResourceSet rootsig ref left.");
  }
}
void ReleaseImguiResourceSet([[maybe_unused]]void* ptr) {
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}
void PrepareRenderPassResources(const nlohmann::json& src_render_pass_list, const uint32_t render_pass_num, MemoryAllocationJanitor* allocator, D3d12Device* device, const MainBufferFormat& main_buffer_format, DescriptorCpu<MemoryAllocationJanitor>* descriptor_cpu, HWND hwnd, const uint32_t frame_buffer_num, RenderPass* dst_render_pass_list) {
  ShaderCompiler shader_compiler;
  if (!shader_compiler.Init()) {
    logerror("shader_compiler.Init failed");
    assert(false && "shader_compiler.Init failed");
  }
  RenderPassInitParams render_pass_init_params{
    .json = nullptr,
    .shader_code = nullptr,
    .shader_compiler = &shader_compiler,
    .descriptor_cpu = descriptor_cpu,
    .device = device,
    .main_buffer_format = main_buffer_format,
    .hwnd = hwnd,
    .frame_buffer_num = frame_buffer_num,
  };
  for (uint32_t i = 0; i < render_pass_num; i++) {
    render_pass_init_params.json = src_render_pass_list[i].contains("pass_vars") ? &src_render_pass_list[i].at("pass_vars") : nullptr;
    auto& dst_render_pass = dst_render_pass_list[i];
    switch (dst_render_pass.name) {
      case SID("dispatch cs"): {
        auto shader_code_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
        render_pass_init_params.shader_code = shader_code_cs;
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(CsDispatchParams));
        ParsePassParamDispatchCs(&render_pass_init_params, dst_render_pass.pass_vars);
        break;
      }
      case SID("prez"): {
        auto shader_code_vs = R"(
struct VsInput {
  float3 position : POSITION;
};
#define PrezRootsig "RootFlags("                                    \
                         "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | "    \
                         "DENY_VERTEX_SHADER_ROOT_ACCESS | "        \
                         "DENY_HULL_SHADER_ROOT_ACCESS | "          \
                         "DENY_DOMAIN_SHADER_ROOT_ACCESS | "        \
                         "DENY_GEOMETRY_SHADER_ROOT_ACCESS | "      \
                         "DENY_PIXEL_SHADER_ROOT_ACCESS | "         \
                         "DENY_AMPLIFICATION_SHADER_ROOT_ACCESS | " \
                         "DENY_MESH_SHADER_ROOT_ACCESS"             \
                         "), "
[RootSignature(PrezRootsig)]
float4 main(const VsInput input) : SV_Position {
  float4 output = float4(input, 1.0f);
  return output;
}
)";
        render_pass_init_params.shader_code = shader_code_vs;
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(PrezResourceSet));
        ParsePassParamPreZ(&render_pass_init_params, dst_render_pass.pass_vars);
        break;
      }
      case SID("output to swapchain"): {
        auto shader_code_vs_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
Texture2D src : register(t0);
Texture2D src1;
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=2), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  color.r = src1.Sample(tex_sampler, input.texcoord).r;
  return color;
}
)";
        render_pass_init_params.shader_code = shader_code_vs_ps;
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(ShaderResourceSet));
        ParsePassParamVsPsCopyResource(&render_pass_init_params, dst_render_pass.pass_vars);
        break;
      }
      case SID("imgui"): {
        InitImgui(&render_pass_init_params);
        break;
      }
    }
  }
  shader_compiler.Term();
}
void ReleaseRenderPassResources(const uint32_t render_pass_num, RenderPass* render_pass_list) {
  for (uint32_t i = 0; i < render_pass_num; i++) {
    const auto& render_pass = render_pass_list[i];
    switch (render_pass.name) {
      case SID("dispatch cs"): {
        ReleaseResourceDispatchCs(render_pass.pass_vars);
        break;
      }
      case SID("prez"): {
        ReleaseResourcePreZ(render_pass.pass_vars);
        break;
      }
      case SID("output to swapchain"): {
        ReleaseResourceShaderResourceSet(render_pass.pass_vars);
        break;
      }
      case SID("imgui"): {
        ReleaseImguiResourceSet(render_pass.pass_vars);
        break;
      }
    }
  }
}
auto GetRenderPassFunctions(MemoryAllocationJanitor* allocator) {
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(allocator, 64);
  if (!render_pass_functions.Insert(SID("dispatch cs"), DispatchCs)) {
    logerror("failed to insert dispatch cs");
    assert(false && "failed to insert dispatch cs");
  }
  if (!render_pass_functions.Insert(SID("prez"), PreZ)) {
    logerror("failed to insert prez");
    assert(false && "failed to insert prez");
  }
  if (!render_pass_functions.Insert(SID("output to swapchain"), CopyResourceVsPs)) {
    logerror("failed to insert output to swapchain");
    assert(false && "failed to insert output to swapchain");
  }
  if (!render_pass_functions.Insert(SID("imgui"), RunImgui)) {
    logerror("failed to insert imgui");
    assert(false && "failed to insert imgui");
  }
  if (!render_pass_functions.Insert(SID("imgui_newframe"), RunImguiNewFrame)) {
    logerror("failed to insert imgui_newframe");
    assert(false && "failed to insert imgui_newframe");
  }
  return render_pass_functions;
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
        barrier.UAV.pResource = resource[i];
        break;
      }
    }
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, const HashMap<BufferAllocation, MemoryAllocationJanitor>& buffer_list, const HashMap<ID3D12Resource*, MemoryAllocationJanitor>& extra_buffer_list) {
  if (barrier_num == 0) { return; }
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
  HashMap<D3D12_GPU_DESCRIPTOR_HANDLE*, MemoryAllocationJanitor> gpu_handle_list(allocator, 64);
  auto total_handle_count_view = descriptor_gpu->GetViewHandleTotal();
  uint32_t occupied_handle_num_view = 0;
  auto total_handle_count_sampler = descriptor_gpu->GetSamplerHandleTotal();
  uint32_t occupied_handle_num_sampler = 0;
  for (uint32_t i = 0; i < render_pass_num; i++) {
    auto& render_pass = render_pass_list[i];
    if (!gpu_handle_list.Insert(render_pass.name, AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, 2))) { // 0:view 1:sampler
      logerror("gpu_handle_list.Insert failed. {}", i);
      assert(false && "gpu_handle_list.Insert failed.");
      continue;
    }
    auto gpu_handles = *gpu_handle_list.Get(render_pass.name);
    {
      // view
      gpu_handles[0] = descriptor_gpu->CopyViewDescriptors(device, render_pass.buffer_num, render_pass.buffer_list, descriptor_cpu);
      auto prev_handle_count = descriptor_gpu->GetViewHandleCount();
      auto current_handle_count = descriptor_gpu->GetViewHandleCount();
      if (prev_handle_count <= current_handle_count) {
        occupied_handle_num_view += current_handle_count - prev_handle_count;
      } else {
        occupied_handle_num_view += total_handle_count_view - prev_handle_count + current_handle_count;
      }
      assert(occupied_handle_num_view <= total_handle_count_view && "increase RenderGraph::gpu_handle_num_view");
    }
    {
      // sampler
      gpu_handles[1] = descriptor_gpu->CopySamplerDescriptors(device, render_pass, descriptor_cpu);
      auto prev_handle_count = descriptor_gpu->GetSamplerHandleCount();
      auto current_handle_count = descriptor_gpu->GetSamplerHandleCount();
      if (prev_handle_count <= current_handle_count) {
        occupied_handle_num_sampler += current_handle_count - prev_handle_count;
      } else {
        occupied_handle_num_sampler += total_handle_count_sampler - prev_handle_count + current_handle_count;
      }
      assert(occupied_handle_num_sampler <= total_handle_count_sampler && "increase RenderGraph::gpu_handle_num_sampler");
    }
  }
  return gpu_handle_list;
}
template <typename A>
auto CreateCpuHandleWithViewImpl(const BufferConfig& buffer_config, const DescriptorType& descriptor_type, ID3D12Resource* resource, DescriptorCpu<A>* descriptor_cpu, D3d12Device* device) {
  auto cpu_handle = descriptor_cpu->CreateHandle(buffer_config.name, descriptor_type);
  if (cpu_handle == nullptr) {
    logwarn("no cpu_handle {} {}", buffer_config.name, descriptor_type);
    return false;
  }
  return CreateView(device, descriptor_type, buffer_config, resource, cpu_handle);
}
template <typename A>
auto CreateCpuHandleWithView(const BufferConfig& buffer_config, ID3D12Resource* resource, DescriptorCpu<A>* descriptor_cpu, D3d12Device* device) {
  bool ret = true;
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagCbv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, DescriptorType::kCbv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(cbv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagSrv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, DescriptorType::kSrv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(srv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagUav) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, DescriptorType::kUav, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(uav) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagRtv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, DescriptorType::kRtv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(rtv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagDsv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, DescriptorType::kDsv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(dsv) failed.");
    }
  }
  return ret;
}
} // namespace anonymous
} // namespace illuminate
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
  DescriptorCpu<MemoryAllocationJanitor> descriptor_cpu;
  HashMap<BufferAllocation, MemoryAllocationJanitor> buffer_list(&allocator, 256);
  DescriptorGpu descriptor_gpu;
  RenderGraph render_graph;
  {
    auto json = GetTestJson();
    ParseRenderGraphJson(json, &allocator, &render_graph);
    CHECK_UNARY(command_list_set.Init(device.Get(),
                                      render_graph.command_queue_num,
                                      render_graph.command_queue_type,
                                      render_graph.command_queue_priority,
                                      render_graph.command_list_num_per_queue,
                                      render_graph.frame_buffer_num,
                                      render_graph.command_allocator_num_per_queue_type));
    CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
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
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), render_graph.descriptor_handle_num_per_type, &allocator));
    command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
    const auto& json_buffer_list = json.at("buffer");
    for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
      CAPTURE(i);
      auto buffer = CreateBuffer(render_graph.buffer_list[i], main_buffer_size, buffer_allocator);
      CHECK_NE(buffer.allocation, nullptr);
      CHECK_NE(buffer.resource, nullptr);
      CHECK_UNARY(buffer_list.Insert(render_graph.buffer_list[i].name, std::move(buffer)));
      CHECK_UNARY(CreateCpuHandleWithView(render_graph.buffer_list[i], buffer.resource, &descriptor_cpu, device.Get()));
      SetD3d12Name(buffer.resource, json_buffer_list[i].at("name"));
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(render_graph.sampler_name[i], DescriptorType::kSampler);
      CHECK_NE(cpu_handler, nullptr);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], *cpu_handler);
    }
    for (uint32_t i = 0; i < render_graph.descriptor_num; i++) {
      descriptor_cpu.CreateHandle(render_graph.descriptor_name[i], render_graph.descriptor_type[i]);
    }
    const auto gpu_handle_num_view = (render_graph.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kCbv)] + render_graph.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSrv)] + render_graph.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kUav)]) * render_graph.frame_buffer_num;
    const auto gpu_handle_num_sampler = render_graph.sampler_num * render_graph.frame_buffer_num;
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), gpu_handle_num_view, gpu_handle_num_sampler));
    PrepareRenderPassResources(json.at("render_pass"), render_graph.render_pass_num, &allocator, device.Get(), main_buffer_format, &descriptor_cpu, window.GetHwnd(), render_graph.frame_buffer_num, render_graph.render_pass_list);
  }
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
  HashMap<uint64_t, MemoryAllocationJanitor> render_pass_signal(&allocator);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    auto& render_pass = render_graph.render_pass_list[i];
    if (render_pass.sends_signal) {
      render_pass_signal.Reserve(render_pass.name);
    }
  }
  auto render_pass_functions = GetRenderPassFunctions(&allocator);
  auto scene_data = GetSceneFromTinyGltfText(GetTestTinyGltf(), "", buffer_allocator, &allocator);
  RenderPassArgs render_pass_args{
    .command_list = nullptr,
    .main_buffer_size = &main_buffer_size,
    .pass_vars_ptr = nullptr,
    .gpu_handles = nullptr,
    .cpu_handles = nullptr,
    .resources = nullptr,
    .scene_data = &scene_data,
  };
  uint32_t tmp_memory_max_offset = 0U;
  auto prev_command_list = AllocateArray<D3d12CommandList*>(&allocator, render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
  }
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
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
      for (uint32_t l = 0; l < render_pass.wait_pass_num; l++) {
        CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(render_pass.signal_queue_index[l], render_pass.command_queue_index, *render_pass_signal.Get(render_pass.signal_pass_name[l])));
      }
      auto command_list = render_pass.need_command_list ? prev_command_list[render_pass.command_queue_index] : nullptr;
      if (render_pass.need_command_list && command_list == nullptr) {
        command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
        prev_command_list[render_pass.command_queue_index] = command_list;
      }
      if (command_list) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, buffer_list, extra_buffer_list);
      {
        auto render_pass_allocator = GetTemporalMemoryAllocator();
        auto resource_list = AllocateArray<ID3D12Resource*>(&render_pass_allocator, render_pass.buffer_num);
        auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&render_pass_allocator, render_pass.buffer_num);
        for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
          auto& buffer = render_pass.buffer_list[b];
          auto buffer_allocation = buffer_list.Get(buffer.buffer_name);
          if (buffer_allocation != nullptr) {
            resource_list[b] = buffer_allocation->resource;
          } else {
            auto resource = extra_buffer_list.Get(buffer.buffer_name);
            resource_list[b] = resource ? *resource : nullptr;
          }
          const auto descriptor_type = ConvertToDescriptorType(buffer.state);
          if (descriptor_type != DescriptorType::kNum) {
            cpu_handle_list[b] = (buffer.buffer_name != SID("swapchain")) ? *descriptor_cpu.GetHandle(buffer.buffer_name, descriptor_type) : swapchain.GetRtvHandle();
          } else {
            cpu_handle_list[b] = {};
          }
          tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
        }
        render_pass_args.command_list = command_list;
        render_pass_args.pass_vars_ptr = render_pass.pass_vars;
        render_pass_args.gpu_handles = *gpu_handle_list.Get(render_pass.name);
        render_pass_args.cpu_handles = cpu_handle_list;
        render_pass_args.resources = resource_list;
        (**render_pass_functions.Get(render_pass.name))(&render_pass_args);
        tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
      }
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, buffer_list, extra_buffer_list);
      if (render_pass.execute) {
        command_list_set.ExecuteCommandList(render_pass.command_queue_index);
        prev_command_list[render_pass.command_queue_index] = nullptr;
      }
      if (render_pass.sends_signal) {
        assert(render_pass.execute);
        auto signal_val = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
        frame_signals[frame_index][render_pass.command_queue_index] = signal_val;
        render_pass_signal.Replace(render_pass.name, std::move(signal_val));
      }
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
  ReleaseSceneData(&scene_data);
  ReleaseRenderPassResources(render_graph.render_pass_num, render_graph.render_pass_list);
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
