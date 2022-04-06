#include "d3d12_render_pass_common.h"
namespace illuminate {
RenderPassConfigDynamicData InitRenderPassDynamicData(const uint32_t render_pass_num, const RenderPass* render_pass_list, const uint32_t buffer_num, MemoryAllocationJanitor* allocator) {
  RenderPassConfigDynamicData dynamic_data{};
  dynamic_data.render_pass_enable_flag = AllocateArray<bool>(allocator, render_pass_num);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    dynamic_data.render_pass_enable_flag[i] = render_pass_list[i].enabled;
  }
  dynamic_data.write_to_sub = AllocateArray<bool*>(allocator, buffer_num);
  for (uint32_t i = 0; i < buffer_num; i++) {
    dynamic_data.write_to_sub[i] = AllocateArray<bool>(allocator, render_pass_num);
  }
  return dynamic_data;
}
}
