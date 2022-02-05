#ifndef ILLUMINATE_RENDER_PASS_D3D12_COPY_RESOURCE_H
#define ILLUMINATE_RENDER_PASS_D3D12_COPY_RESOURCE_H
#include "d3d12_render_pass_common.h"
namespace illuminate {
class RenderPassCopyResource {
 public:
  enum class State :uint8_t { kInit = 0, kWait, kDone, };
  struct Param {
    uint32_t initial_frame_index{};
    State state;
  };
  template <typename A>
  static Param* Init([[maybe_unused]]RenderPassInitArgs<A>* args) {
    auto param = Allocate<Param>(args->allocator);
    param->state = State::kInit;
    param->initial_frame_index = 0;
    return param;
  }
  static void Term([[maybe_unused]]void* ptr) {}
  static void Exec(RenderPassArgs* args) {
    auto param = static_cast<Param*>(args->pass_vars_ptr);
    switch (param->state) {
      case State::kInit: {
        param->initial_frame_index = args->frame_index;
        for (uint32_t i = 0; i < args->scene_data->buffer_allocation_num; i++) {
          args->command_list->CopyResource(args->scene_data->buffer_allocation_default[i].resource, args->scene_data->buffer_allocation_upload[i].resource);
        }
        param->state = State::kWait;
        break;
      }
      case State::kWait: {
        if (param->initial_frame_index != args->frame_index) { break; }
        for (uint32_t i = 0; i < args->scene_data->buffer_allocation_num; i++) {
          ReleaseBufferAllocation(&args->scene_data->buffer_allocation_upload[i]);
          args->scene_data->buffer_allocation_upload[i].resource   = nullptr;
          args->scene_data->buffer_allocation_upload[i].allocation = nullptr;
        }
        param->state = State::kDone;
        break;
      }
      case State::kDone: {
        break;
      }
    }
  }
 private:
  RenderPassCopyResource() = delete;
};
}
#endif
