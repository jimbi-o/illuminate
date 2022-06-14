#include "illuminate/illuminate.h"
#include "../d3d12_descriptors.h"
#include "../d3d12_header_common.h"
#include "d3d12_render_pass_debug_render_selected_buffer.h"
#include "d3d12_render_pass_util.h"
namespace illuminate {
void RenderPassDebugRenderSelectedBuffer::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  D3D12_GPU_DESCRIPTOR_HANDLE srv_list{};
  D3D12_RESOURCE_BARRIER* barrier{};
  uint32_t barrier_num = 0;
  {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    uint32_t* buffer_allocation_index_list = nullptr;
    auto buffer_allocation_index_len = GetBufferAllocationIndexList(*args_common->buffer_list, args_common->buffer_config_list, &tmp_allocator, &buffer_allocation_index_list);
    auto cpu_handles = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, buffer_allocation_index_len);
    for (uint32_t i = 0; i < buffer_allocation_index_len; i++) {
      cpu_handles[i].ptr = args_common->descriptor_cpu->GetHandle(buffer_allocation_index_list[i], DescriptorType::kSrv).ptr;
    }
    srv_list = args_common->descriptor_gpu->WriteToTransientViewHandleRange(buffer_allocation_index_len, cpu_handles, args_common->device);
    barrier = AllocateArray<D3D12_RESOURCE_BARRIER>(&tmp_allocator, buffer_allocation_index_len);
    for (uint32_t i = 0; i < buffer_allocation_index_len; i++) {
      const auto& buffer_allocation_index = buffer_allocation_index_list[i];
      const auto& buffer_index = args_common->buffer_list->buffer_config_index[buffer_allocation_index];
      const auto& state = args_common->resource_state_list[buffer_index][args_per_pass->render_pass_index][IsPingPongMainBuffer(*args_common->buffer_list, buffer_index, buffer_allocation_index) ? 0 : 1];
      if (state != ResourceStateType::kSrvPs) {
        barrier[barrier_num].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier[barrier_num].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier[barrier_num].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier[barrier_num].Transition.pResource   = args_common->buffer_list->resource_list[buffer_allocation_index];
        barrier[barrier_num].Transition.StateBefore = ConvertToD3d12ResourceState(state);
        barrier[barrier_num].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier_num++;
      }
    }
  }
  auto command_list = args_per_pass->command_list;
  if (barrier_num > 0) {
    command_list->ResourceBarrier(barrier_num, barrier);
  }
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
  command_list->SetGraphicsRootDescriptorTable(2, args_per_pass->gpu_handles_sampler[0]);
  command_list->DrawInstanced(3, 1, 0, 0);
  if (barrier_num > 0) {
    for (uint32_t i = 0; i < barrier_num; i++) {
      std::swap(barrier[i].Transition.StateBefore, barrier[i].Transition.StateAfter);
    }
    command_list->ResourceBarrier(barrier_num, barrier);
  }
}
uint32_t RenderPassDebugRenderSelectedBuffer::GetBufferAllocationIndexList(const BufferList& buffer_list, const BufferConfig* buffer_config_list, MemoryAllocationJanitor* allocator, uint32_t** dst_buffer_allocation_index_list) {
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
} // namespace illuminate
