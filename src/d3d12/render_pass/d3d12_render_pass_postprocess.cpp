#include "illuminate/illuminate.h"
#include "../d3d12_header_common.h"
#include "d3d12_render_pass_postprocess.h"
#include "d3d12_render_pass_util.h"
namespace illuminate {
namespace {
struct Param {
  BufferSizeRelativeness size_type{BufferSizeRelativeness::kPrimaryBufferRelative};
  uint32_t rtv_index{0};
  void** cbv_ptr{nullptr};
  uint32_t cbv_size{0};
};
auto UpdatePingpongA(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  float c[4]{0.0f,1.0f,1.0f,1.0f};
  memcpy(pass_vars->cbv_ptr[args_common->frame_index],  c, GetUint32(sizeof(float)) * 4);
}
auto UpdatePingpongB(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  float c[4]{1.0f,0.0f,1.0f,1.0f};
  memcpy(pass_vars->cbv_ptr[args_common->frame_index],  c, GetUint32(sizeof(float)) * 4);
}
auto UpdatePingpongC(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  float c[4]{1.0f,1.0f,1.0f,1.0f};
  memcpy(pass_vars->cbv_ptr[args_common->frame_index],  c, GetUint32(sizeof(float)) * 4);
}
} // namespace anonymous
void* RenderPassPostprocess::Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index) {
  auto param = Allocate<Param>(args->allocator);
  *param = {};
  if (args->json && args->json->contains("size_type")) {
    param->size_type = (GetStringView(args->json->at("size_type")).compare("swapchain_relative") == 0) ? BufferSizeRelativeness::kSwapchainRelative : BufferSizeRelativeness::kPrimaryBufferRelative;
  }
  const auto& buffer_list = args->render_pass_list[render_pass_index].buffer_list;
  for (uint32_t i = 0; i < args->render_pass_list[render_pass_index].buffer_num; i++) {
    if (buffer_list[i].state == ResourceStateType::kRtv) {
      param->rtv_index = i;
      break;
    }
  }
  for (uint32_t i = 0; i < args->render_pass_list[render_pass_index].buffer_num; i++) {
    if (buffer_list[i].state == ResourceStateType::kCbv) {
      const auto buffer_config_index = args->render_pass_list[render_pass_index].buffer_list[i].buffer_index;
      param->cbv_size = static_cast<uint32_t>(args->buffer_config_list[buffer_config_index].width);
      param->cbv_ptr = AllocateArray<void*>(args->allocator, args->frame_buffer_num);
      for (uint32_t j = 0; j < args->frame_buffer_num; j++) {
        param->cbv_ptr[j] = MapResource(GetResource(*args->buffer_list, buffer_config_index, j), param->cbv_size);
      }
      break;
    }
  }
  return param;
}
void RenderPassPostprocess::Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  switch (GetRenderPass(args_common, args_per_pass).name) {
    case SID("pingpong-a"): {
      UpdatePingpongA(args_common, args_per_pass);
      break;
    }
    case SID("pingpong-b"): {
      UpdatePingpongB(args_common, args_per_pass);
      break;
    }
    case SID("pingpong-c"): {
      UpdatePingpongC(args_common, args_per_pass);
      break;
    }
  }
}
void RenderPassPostprocess::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto pass_vars = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  const auto& buffer_size = (pass_vars->size_type == BufferSizeRelativeness::kPrimaryBufferRelative) ? args_common->main_buffer_size->primarybuffer : args_common->main_buffer_size->swapchain;
  auto& width = buffer_size.width;
  auto& height = buffer_size.height;
  auto command_list = args_per_pass->command_list;
  command_list->SetGraphicsRootSignature(GetRenderPassRootSig(args_common, args_per_pass));
  D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
  command_list->RSSetViewports(1, &viewport);
  D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
  command_list->RSSetScissorRects(1, &scissor_rect);
  command_list->SetPipelineState(GetRenderPassPso(args_common, args_per_pass));
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command_list->OMSetRenderTargets(1, &args_per_pass->cpu_handles[pass_vars->rtv_index], true, nullptr);
  if (args_per_pass->gpu_handles_view) {
    command_list->SetGraphicsRootDescriptorTable(0, args_per_pass->gpu_handles_view[0]);
  }
  if (args_per_pass->gpu_handles_sampler) {
    command_list->SetGraphicsRootDescriptorTable(1, args_per_pass->gpu_handles_sampler[0]);
  }
  command_list->DrawInstanced(3, 1, 0, 0);
}
} // namespace illuminate
