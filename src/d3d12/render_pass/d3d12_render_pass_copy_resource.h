#ifndef ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#define ILLUMINATE_D3D12_RENDER_PASS_COPY_RESOURCE_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCopyResource {
 public:
  enum class State : uint8_t { kUpload = 0, kUploading, kDone, };
  struct Param {
    uint32_t initial_frame_index{};
    State state;
  };
  static void* Init([[maybe_unused]]RenderPassFuncArgsInit* args) {
    auto param = Allocate<Param>(args->allocator);
    param->initial_frame_index = 0;
    param->state = State::kUpload;
    return param;
  }
  static void Term([[maybe_unused]]void* ptr) {}
  static void Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    auto param = static_cast<Param*>(args_per_pass->pass_vars_ptr);
    if (param->state != State::kUploading) { return; }
    if (param->initial_frame_index != args_common->frame_index) { return; }
    for (uint32_t i = 0; i < args_common->scene_data->buffer_allocation_num; i++) {
      ReleaseBufferAllocation(&args_common->scene_data->buffer_allocation_upload[i]);
      args_common->scene_data->buffer_allocation_upload[i].resource   = nullptr;
      args_common->scene_data->buffer_allocation_upload[i].allocation = nullptr;
    }
    param->state = State::kDone;
  }
  static auto IsRenderNeeded(const void* args) {
    auto param = static_cast<const Param*>(args);
    return param->state == State::kUpload;
  }
  static auto Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    auto param = static_cast<Param*>(args_per_pass->pass_vars_ptr);
    assert(param->state == State::kUpload);
    for (uint32_t i = 0; i < args_common->scene_data->buffer_allocation_num; i++) {
      args_per_pass->command_list->CopyResource(args_common->scene_data->buffer_allocation_default[i].resource, args_common->scene_data->buffer_allocation_upload[i].resource);
    }
    param->initial_frame_index = args_common->frame_index;
    param->state = State::kUploading;
  }
 private:
  RenderPassCopyResource() = delete;
};
}
#endif
