#include "d3d12_render_pass_copy_resource.h"
#include "d3d12_render_pass_common_util.h"
namespace illuminate {
namespace {
enum class State : uint8_t { kUpload = 0, kUploading, kDone, };
struct Param {
  uint32_t initial_frame_index{};
  State state;
};
} // namespace anonymous
void* RenderPassCopyResource::Init(RenderPassFuncArgsInit* args, [[maybe_unused]]const uint32_t render_pass_index) {
  auto param = Allocate<Param>(args->allocator);
  param->initial_frame_index = 0;
  param->state = State::kUpload;
  return param;
}
void RenderPassCopyResource::Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
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
bool RenderPassCopyResource::IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto param = static_cast<const Param*>(args_per_pass->pass_vars_ptr);
  return param->state == State::kUpload;
}
void RenderPassCopyResource::Render(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
  auto param = static_cast<Param*>(args_per_pass->pass_vars_ptr);
  assert(param->state == State::kUpload);
  for (uint32_t i = 0; i < args_common->scene_data->buffer_allocation_num; i++) {
    args_per_pass->command_list->CopyResource(args_common->scene_data->buffer_allocation_default[i].resource, args_common->scene_data->buffer_allocation_upload[i].resource);
  }
  param->initial_frame_index = args_common->frame_index;
  param->state = State::kUploading;
}
} // namespace illuminate
