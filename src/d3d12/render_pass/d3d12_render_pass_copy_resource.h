#ifndef ILLUMINATE_RENDER_PASS_D3D12_COPY_RESOURCE_H
#define ILLUMINATE_RENDER_PASS_D3D12_COPY_RESOURCE_H
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
  static auto Update(RenderPassFuncArgsUpdate* args) {
    auto param = static_cast<Param*>(args->pass_vars_ptr);
    if (param->state != State::kUploading) { return; }
    if (param->initial_frame_index != args->frame_index) { return; }
    for (uint32_t i = 0; i < args->scene_data->buffer_allocation_num; i++) {
      ReleaseBufferAllocation(&args->scene_data->buffer_allocation_upload[i]);
      args->scene_data->buffer_allocation_upload[i].resource   = nullptr;
      args->scene_data->buffer_allocation_upload[i].allocation = nullptr;
    }
    param->state = State::kDone;
  }
  static auto Render(RenderPassFuncArgsRender* args) {
    auto param = static_cast<Param*>(args->pass_vars_ptr);
    if (param->state != State::kUpload) { return false; }
    for (uint32_t i = 0; i < args->scene_data->buffer_allocation_num; i++) {
      args->command_list->CopyResource(args->scene_data->buffer_allocation_default[i].resource, args->scene_data->buffer_allocation_upload[i].resource);
    }
    param->initial_frame_index = args->frame_index;
    param->state = State::kUploading;
    return true;
  }
 private:
  RenderPassCopyResource() = delete;
};
}
#endif
