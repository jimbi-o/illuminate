#ifndef ILLUMINATE_D3D12_RENDER_PASS_POSTPROCESS_H
#define ILLUMINATE_D3D12_RENDER_PASS_POSTPROCESS_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassPostprocess {
 public:
  struct Param {
    BufferSizeRelativeness size_type{BufferSizeRelativeness::kPrimaryBufferRelative};
    uint32_t rtv_index{0};
    bool use_views{true};
    bool use_sampler{true};
    void* cbv_ptr{nullptr};
    uint32_t cbv_size{0};
  };
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    if (args->json->contains("size_type")) {
      param->size_type = (GetStringView(args->json->at("size_type")).compare("swapchain_relative") == 0) ? BufferSizeRelativeness::kSwapchainRelative : BufferSizeRelativeness::kPrimaryBufferRelative;
    }
    param->rtv_index = GetNum(*args->json, "rtv_index", 0);
    param->use_views = GetBool(*args->json, "use_views", true);
    param->use_sampler = GetBool(*args->json, "use_sampler", true);
    if (args->json->contains("cbv_index")) {
      const auto buffer_config_index = args->render_pass_list[render_pass_index].buffer_list[GetNum(*args->json, "cbv_index", 0)].buffer_index;
      auto resource = GetResource(*args->buffer_list, buffer_config_index);
      param->cbv_size = static_cast<uint32_t>(args->buffer_config_list[buffer_config_index].width);
      param->cbv_ptr = MapResource(resource, param->cbv_size);
    }
    return param;
  }
  static void Term() {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    if (!args_per_pass->ptr) { return; }
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    if (!pass_vars->cbv_ptr) { return; }
    memcpy(pass_vars->cbv_ptr, args_per_pass->ptr, args_per_pass->ptr_size);
  }
  static auto IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) { return true; }
  static auto Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    auto& buffer_size = (pass_vars->size_type == BufferSizeRelativeness::kPrimaryBufferRelative) ? args_common->main_buffer_size->primarybuffer : args_common->main_buffer_size->swapchain;
    auto& width = buffer_size.width;
    auto& height = buffer_size.height;
    DrawTriangle(args_per_pass->command_list, width, height, GetRenderPassRootSig(args_common, args_per_pass), GetRenderPassPso(args_common, args_per_pass), args_per_pass->cpu_handles[pass_vars->rtv_index], pass_vars->use_views, pass_vars->use_sampler, args_per_pass->gpu_handles);
  }
  static void DrawTriangle(D3d12CommandList* command_list, const uint32_t width, const uint32_t height, ID3D12RootSignature* rootsig, ID3D12PipelineState* pso, const D3D12_CPU_DESCRIPTOR_HANDLE& rtv, const bool use_views, const bool use_sampler, const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles) {
    command_list->SetGraphicsRootSignature(rootsig);
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    command_list->RSSetScissorRects(1, &scissor_rect);
    command_list->SetPipelineState(pso);
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->OMSetRenderTargets(1, &rtv, true, nullptr);
    if (use_views) {
      command_list->SetGraphicsRootDescriptorTable(0, gpu_handles[0]);
    }
    if (use_sampler) {
      command_list->SetGraphicsRootDescriptorTable(1, gpu_handles[1]);
    }
    command_list->DrawInstanced(3, 1, 0, 0);
  }
 private:
  RenderPassPostprocess() = delete;
};
}
#endif
