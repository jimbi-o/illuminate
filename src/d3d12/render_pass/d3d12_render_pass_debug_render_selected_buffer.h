#ifndef ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#define ILLUMINATE_D3D12_RENDER_PASS_DEBUG_RENDER_SELECTED_BUFFER_H
#include "d3d12_render_pass_common.h"
#include "d3d12_render_pass_postprocess.h"
namespace illuminate {
class RenderPassDebugRenderSelectedBuffer {
 public:
  struct Param {
  };
  static void* Init(RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    *param = {};
    return param;
  }
  static void Term() {
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) {
  }
  static auto IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) { return true; }
  static auto Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    D3D12_GPU_DESCRIPTOR_HANDLE srv_list{};
    {
      auto tmp_allocator = GetTemporalMemoryAllocator();
      uint32_t* buffer_allocation_index_list = nullptr;
      auto buffer_allocation_index_len = GetBufferAllocationIndexList(*args_common->buffer_list, args_common->buffer_config_list, &tmp_allocator, &buffer_allocation_index_list);
      auto descriptor_type_list = AllocateArray<DescriptorType>(&tmp_allocator, buffer_allocation_index_len);
      for (uint32_t i = 0; i < buffer_allocation_index_len; i++) {
        descriptor_type_list[i] = DescriptorType::kSrv;
      }
      srv_list = args_common->descriptor_gpu->CopyViewDescriptors(args_common->device, buffer_allocation_index_len, buffer_allocation_index_list, descriptor_type_list, *args_common->descriptor_cpu);
    }
    auto command_list = args_per_pass->command_list;
    auto& width = args_common->main_buffer_size->swapchain.width;
    auto& height = args_common->main_buffer_size->swapchain.height;
    auto rootsig = GetRenderPassRootSig(args_common, args_per_pass);
    auto pso = GetRenderPassPso(args_common, args_per_pass);
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
    command_list->RSSetViewports(1, &viewport);
    D3D12_RECT scissor_rect{0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    command_list->RSSetScissorRects(1, &scissor_rect);
    command_list->SetGraphicsRootSignature(rootsig);
    command_list->SetPipelineState(pso);
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->OMSetRenderTargets(1, args_per_pass->cpu_handles, true, nullptr);
    command_list->SetGraphicsRoot32BitConstant(0, args_common->dynamic_data->debug_render_selected_buffer_allocation_index, 0);
    command_list->SetGraphicsRootDescriptorTable(1, srv_list);
    command_list->SetGraphicsRootDescriptorTable(2, args_per_pass->gpu_handles[1]);
    command_list->DrawInstanced(3, 1, 0, 0);
  }
  static uint32_t GetBufferAllocationIndexList(const BufferList& buffer_list, const BufferConfig* buffer_config_list, MemoryAllocationJanitor* allocator, uint32_t** dst_buffer_allocation_index_list) {
    *dst_buffer_allocation_index_list = AllocateArray<uint32_t>(allocator, buffer_list.buffer_allocation_num);
    uint32_t index = 0;
    for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
      if (buffer_list.buffer_allocation_list[i] == nullptr) { continue; }
      auto& buffer_config = buffer_config_list[buffer_list.buffer_config_index[i]];
      if (buffer_config.dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) { continue; }
      (*dst_buffer_allocation_index_list)[index] = i;
      index++;
    }
    return index;
  }
 private:
  RenderPassDebugRenderSelectedBuffer() = delete;
};
}
#endif
