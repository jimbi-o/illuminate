#include "D3D12MemAlloc.h"
#include "doctest/doctest.h"
#include <nlohmann/json.hpp>
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_render_graph_json_parser.h"
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
  "command_allocator": {
    "direct": 2,
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
      "initial_state": "uav",
      "descriptor_type": ["uav", "srv"]
    },
    {
      "name": "dsv",
      "format": "D24_UNORM_S8_UINT",
      "heap_type": "default",
      "dimension": "texture2d",
      "size_type": "primarybuffer_relative",
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
      "descriptor_type": ["dsv"],
      "clear_depth": 1,
      "clear_stencil": 0
    }
  ],
  "descriptor_handle_num_per_type": {
    "cbv": 0,
    "srv": 1,
    "uav": 1,
    "sampler": 0,
    "rtv": 0,
    "dsv": 1
  },
  "gpu_handle_num_view":10,
  "gpu_handle_num_sampler":1,
  "render_pass": [
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
        "shader_compile_args":["-T", "vs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
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
          "name": "swapchain",
          "state": "rtv"
        }
      ],
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
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "rtv",
          "state_after": "present"
        },
        {
          "buffer_name": "uav",
          "type": "transition",
          "split_type": "none",
          "state_before": "srv_ps",
          "state_after": "uav"
        }
      ],
      "pass_vars": {
        "shader_vs": "test.vs.hlsl",
        "shader_compile_args_vs":["-T", "vs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"],
        "shader_ps": "test.ps.hlsl",
        "shader_compile_args_ps":["-T", "ps_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      }
    }
  ]
}
)"_json;
}
struct CsDispatchParams {
  ID3D12RootSignature* rootsig{nullptr};
  ID3D12PipelineState* pso{nullptr};
  uint32_t thread_group_count_x{0};
  uint32_t thread_group_count_y{0};
};
struct ShaderResourceSet {
  ID3D12RootSignature* rootsig{nullptr};
  ID3D12PipelineState* pso{nullptr};
};
using RenderPassFunction = void (*)(D3d12CommandList*, const MainBufferSize&, const void*, const D3D12_GPU_DESCRIPTOR_HANDLE*, const D3D12_CPU_DESCRIPTOR_HANDLE*, ID3D12Resource**);
void CopyResource(D3d12CommandList* command_list, [[maybe_unused]]const MainBufferSize& main_buffer_size, [[maybe_unused]]const void* pass_vars, [[maybe_unused]]const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, [[maybe_unused]]const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, ID3D12Resource** resources) {
  command_list->CopyResource(resources[1], resources[0]);
}
void DispatchCs(D3d12CommandList* command_list, const MainBufferSize& main_buffer_size, const void* pass_vars_ptr, const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, [[maybe_unused]]const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, [[maybe_unused]]ID3D12Resource** resources) {
  auto pass_vars = static_cast<const CsDispatchParams*>(pass_vars_ptr);
  command_list->SetComputeRootSignature(pass_vars->rootsig);
  command_list->SetPipelineState(pass_vars->pso);
  command_list->SetComputeRootDescriptorTable(0, *gpu_handles);
  command_list->Dispatch(main_buffer_size.swapchain.width / pass_vars->thread_group_count_x, main_buffer_size.swapchain.width / pass_vars->thread_group_count_y, 1);
}
void PreZ(D3d12CommandList* command_list, const MainBufferSize& main_buffer_size, const void* pass_vars_ptr, [[maybe_unused]]const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, [[maybe_unused]]ID3D12Resource** resources) {
  auto pass_vars = static_cast<const ShaderResourceSet*>(pass_vars_ptr);
  auto& width = main_buffer_size.primarybuffer.width;
  auto& height = main_buffer_size.primarybuffer.height;
  command_list->SetGraphicsRootSignature(pass_vars->rootsig);
  D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
  command_list->RSSetViewports(1, &viewport);
  D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
  command_list->RSSetScissorRects(1, &scissor_rect);
  command_list->SetPipelineState(pass_vars->pso);
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command_list->OMSetRenderTargets(0, nullptr, true, cpu_handles);
  command_list->DrawInstanced(0, 1, 0, 0);
}
void CopyResourceVsPs(D3d12CommandList* command_list, const MainBufferSize& main_buffer_size, const void* pass_vars_ptr, const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, [[maybe_unused]]ID3D12Resource** resources) {
  auto pass_vars = static_cast<const ShaderResourceSet*>(pass_vars_ptr);
  auto& width = main_buffer_size.swapchain.width;
  auto& height = main_buffer_size.swapchain.height;
  command_list->SetGraphicsRootSignature(pass_vars->rootsig);
  D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
  command_list->RSSetViewports(1, &viewport);
  D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
  command_list->RSSetScissorRects(1, &scissor_rect);
  command_list->SetPipelineState(pass_vars->pso);
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command_list->OMSetRenderTargets(1, &cpu_handles[1], true, nullptr);
  command_list->SetGraphicsRootDescriptorTable(0, *gpu_handles);
  command_list->DrawInstanced(3, 1, 0, 0);
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
void ParsePassParamDispatchCs(const nlohmann::json& j, void* dst, ShaderCompiler* shader_compiler, D3d12Device* device, const char* shader_code) {
  auto param = static_cast<CsDispatchParams*>(dst);
  *param = {};
  std::wstring* wstr_args{nullptr};
  const wchar_t** args{nullptr};
  auto allocator = GetTemporalMemoryAllocator();
  auto args_num = GetShaderCompilerArgs(j, "shader_compile_args", &allocator, &wstr_args, &args);
  assert(args_num > 0);
  if (!shader_compiler->CreateRootSignatureAndPsoCs(shader_code, static_cast<uint32_t>(strlen(shader_code)), args_num, args, device, &param->rootsig, &param->pso)) {
    logerror("compute shader parse error");
    assert(false && "compute shader parse error");
  }
  param->thread_group_count_x = j.at("thread_group_count_x");
  param->thread_group_count_y = j.at("thread_group_count_y");
}
void ParsePassParamPreZ(const nlohmann::json& j, void* dst, ShaderCompiler* shader_compiler, D3d12Device* device, const char* shader_code) {
  auto param = static_cast<ShaderResourceSet*>(dst);
  *param = {};
  std::wstring* wstr_args{nullptr};
  const wchar_t** args{nullptr};
  auto allocator = GetTemporalMemoryAllocator();
  auto args_num = GetShaderCompilerArgs(j, "shader_compile_args", &allocator, &wstr_args, &args);
  assert(args_num > 0);
  ShaderCompiler::PsoDescVsPs pso_desc{};
  pso_desc.depth_stencil_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  if (!shader_compiler->CreateRootSignatureAndPsoVs(shader_code, static_cast<uint32_t>(strlen(shader_code)), args_num, args, device, &pso_desc, &param->rootsig, &param->pso)) {
    logerror("vs parse error");
    assert(false && "vs parse error");
  }
}
void ParsePassParamVsPsCopyResource(const nlohmann::json& j, void* dst, ShaderCompiler* shader_compiler, D3d12Device* device, const MainBufferFormat& main_buffer_format, const char* shader_code_vs, const char* shader_code_ps) {
  auto param = static_cast<ShaderResourceSet*>(dst);
  *param = {};
  auto allocator = GetTemporalMemoryAllocator();
  std::wstring* wstr_args_vs{nullptr};
  const wchar_t** args_vs{nullptr};
  auto args_num_vs = GetShaderCompilerArgs(j, "shader_compile_args_vs", &allocator, &wstr_args_vs, &args_vs);
  assert(args_num_vs > 0);
  std::wstring* wstr_args_ps{nullptr};
  const wchar_t** args_ps{nullptr};
  auto args_num_ps = GetShaderCompilerArgs(j, "shader_compile_args_ps", &allocator, &wstr_args_ps, &args_ps);
  assert(args_num_ps > 0);
  ShaderCompiler::PsoDescVsPs pso_desc{};
  pso_desc.render_target_formats.RTFormats[0] = main_buffer_format.swapchain;
  pso_desc.render_target_formats.NumRenderTargets = 1;
  pso_desc.depth_stencil1.DepthEnable = false;
  if (!shader_compiler->CreateRootSignatureAndPsoVsPs(shader_code_vs, static_cast<uint32_t>(strlen(shader_code_vs)), args_num_vs, args_vs,
                                                      shader_code_ps, static_cast<uint32_t>(strlen(shader_code_ps)), args_num_ps, args_ps,
                                                      device, &pso_desc, &param->rootsig, &param->pso)) {
    logerror("vs ps parse error");
    assert(false && "vs ps parse error");
  }
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
void ReleaseResourceShaderResourceSet(void* ptr) {
  auto param = static_cast<ShaderResourceSet*>(ptr);
  if (param->pso->Release() != 0) {
    logwarn("ReleaseResourceShaderResourceSet pso ref left.");
  }
  if (param->rootsig->Release() != 0) {
    logwarn("ReleaseResourceShaderResourceSet rootsig ref left.");
  }
}
void PrepareRenderPassResources(const nlohmann::json& src_render_pass_list, const uint32_t render_pass_num, MemoryAllocationJanitor* allocator, D3d12Device* device, const MainBufferFormat& main_buffer_format, RenderPass* dst_render_pass_list) {
  ShaderCompiler shader_compiler;
  if (!shader_compiler.Init()) {
    logerror("shader_compiler.Init failed");
    assert(false && "shader_compiler.Init failed");
  }
  for (uint32_t i = 0; i < render_pass_num; i++) {
    auto& src_json = src_render_pass_list[i].at("pass_vars");
    auto& dst_render_pass = dst_render_pass_list[i];
    switch (dst_render_pass.name) {
      case SID("dispatch cs"): {
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(CsDispatchParams));
        auto shader_code_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
        ParsePassParamDispatchCs(src_json, dst_render_pass.pass_vars, &shader_compiler, device, shader_code_cs);
        break;
      }
      case SID("prez"): {
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(ShaderResourceSet));
        auto shader_code_vs = R"(
#define EmptyRootSig ""
[RootSignature(EmptyRootSig)]
float4 main(uint id : SV_VertexID) : SV_Position {
  float4 output;
  output = 1.0f;
  return output;
}
)";
        ParsePassParamPreZ(src_json, dst_render_pass.pass_vars, &shader_compiler, device, shader_code_vs);
        break;
      }
      case SID("output to swapchain"): {
        dst_render_pass.pass_vars = allocator->Allocate(sizeof(ShaderResourceSet));
        auto shader_code_vs = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput main(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 1) ?  2.0 :  0.0;
  output.texcoord.y = (id == 2) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
)";
        auto shader_code_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
Texture2D src : register(t0);
#define CopyFullscreenRootsig "DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL) "
[RootSignature(CopyFullscreenRootsig)]
float4 main(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Load(int3(input.position.x, input.position.y, 0));
  return color;
}
)";
        ParsePassParamVsPsCopyResource(src_json, dst_render_pass.pass_vars, &shader_compiler, device, main_buffer_format, shader_code_vs, shader_code_ps);
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
      case SID("prez"):
      case SID("output to swapchain"): {
        ReleaseResourceShaderResourceSet(render_pass.pass_vars);
        break;
      }
    }
  }
}
auto GetRenderPassFunctions(MemoryAllocationJanitor* allocator) {
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(allocator);
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
  return render_pass_functions;
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
auto CreateBuffer(const BufferConfig& config, const MainBufferSize& main_buffer_size, D3D12MA::Allocator* allocator) {
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
    .Width = GetPhysicalWidth(main_buffer_size, config.size_type, config.width),
    .Height = GetPhysicalHeight(main_buffer_size, config.size_type, config.height),
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
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  MainBufferSize main_buffer_size{};
  MainBufferFormat main_buffer_format{};
  Window window;
  CommandListSet command_list_set;
  Swapchain swapchain;
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
    PrepareRenderPassResources(json.at("render_pass"), render_graph.render_pass_num, &allocator, device.Get(), main_buffer_format, render_graph.render_pass_list);
  }
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  DescriptorCpu<MemoryAllocationJanitor> descriptor_cpu;
  CHECK_UNARY(descriptor_cpu.Init(device.Get(), render_graph.descriptor_handle_num_per_type, &allocator));
  HashMap<BufferAllocation, MemoryAllocationJanitor> buffer_list(&allocator, 256);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    CAPTURE(i);
    auto buffer = CreateBuffer(render_graph.buffer_list[i], main_buffer_size, buffer_allocator);
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
    if (render_pass.execute) {
      render_pass_signal.Reserve(render_pass.name);
    }
  }
  auto render_pass_functions = GetRenderPassFunctions(&allocator);
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
      for (uint32_t l = 0; l < render_pass.wait_pass_num; l++) {
        CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(render_pass.signal_queue_index[l], render_pass.command_queue_index, *render_pass_signal.Get(render_pass.signal_pass_name[l])));
      }
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
          const auto descriptor_type = ConvertToDescriptorType(buffer.state);
          if (descriptor_type != DescriptorType::kNum) {
            cpu_handle_list[b] = (buffer.buffer_name != SID("swapchain")) ? *descriptor_cpu.GetHandle(buffer.buffer_name, descriptor_type) : swapchain.GetRtvHandle();
          } else {
            cpu_handle_list[b] = {};
          }
          tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
        }
        auto gpu_handles = gpu_handle_list.Get(render_pass.name);
        (**render_pass_functions.Get(render_pass.name))(command_list, main_buffer_size, render_pass.pass_vars, gpu_handles, cpu_handle_list, resource_list);
        tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
      }
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, buffer_list, extra_buffer_list);
      if (render_pass.execute) {
        used_command_queue[render_pass.command_queue_index] = true;
        command_list_set.ExecuteCommandList(render_pass.command_queue_index);
        auto signal_val = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
        frame_signals[frame_index][render_pass.command_queue_index] = signal_val;
        render_pass_signal.Replace(render_pass.name, std::move(signal_val));
      }
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
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
