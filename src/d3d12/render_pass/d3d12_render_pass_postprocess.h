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
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    if (args->json->contains("size_type")) {
      param->size_type = (GetStringView(args->json->at("size_type")).compare("swapchain_relative") == 0) ? BufferSizeRelativeness::kSwapchainRelative : BufferSizeRelativeness::kPrimaryBufferRelative;
    }
    param->rtv_index = GetNum(*args->json, "rtv_index", 0);
    param->use_views = GetBool(*args->json, "use_views", true);
    param->use_sampler = GetBool(*args->json, "use_sampler", true);
    if (args->json->contains("cbv")) {
      const auto cbv_name_hash = CalcEntityStrHash(*args->json, "cbv");
      const auto buffer_config_index = *(args->named_buffer_config_index->Get(cbv_name_hash));
      auto resource = GetResource(*args->buffer_list, buffer_config_index);
      param->cbv_size = static_cast<uint32_t>(args->buffer_config_list[buffer_config_index].width);
      param->cbv_ptr = MapResource(resource, param->cbv_size);
    }
    return param;
  }
  static void Term([[maybe_unused]]void* ptr) {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    if (!args_per_pass->ptr) { return; }
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    if (!pass_vars->cbv_ptr) { return; }
    memcpy(pass_vars->cbv_ptr, args_per_pass->ptr, pass_vars->cbv_size);
  }
  static auto IsRenderNeeded([[maybe_unused]]const void* args) { return true; }
  static auto Render([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
    auto& buffer_size = (pass_vars->size_type == BufferSizeRelativeness::kPrimaryBufferRelative) ? args_common->main_buffer_size->primarybuffer : args_common->main_buffer_size->swapchain;
    auto& width = buffer_size.width;
    auto& height = buffer_size.height;
    args_per_pass->command_list->SetGraphicsRootSignature(GetRenderPassRootSig(args_common, args_per_pass));
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    args_per_pass->command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    args_per_pass->command_list->RSSetScissorRects(1, &scissor_rect);
    args_per_pass->command_list->SetPipelineState(GetRenderPassPso(args_common, args_per_pass));
    args_per_pass->command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    args_per_pass->command_list->OMSetRenderTargets(1, &args_per_pass->cpu_handles[pass_vars->rtv_index], true, nullptr);
    if (pass_vars->use_views) {
      args_per_pass->command_list->SetGraphicsRootDescriptorTable(0, args_per_pass->gpu_handles[0]);
    }
    if (pass_vars->use_sampler) {
      args_per_pass->command_list->SetGraphicsRootDescriptorTable(1, args_per_pass->gpu_handles[1]);
    }
    args_per_pass->command_list->DrawInstanced(3, 1, 0, 0);
  }
 private:
  RenderPassPostprocess() = delete;
};
}
#endif
