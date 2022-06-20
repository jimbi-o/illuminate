#ifndef ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
#define ILLUMINATE_D3D12_RENDER_PASS_COMMON_H
namespace illuminate {
class DescriptorCpu;
class DescriptorGpu;
struct BufferConfig;
struct BufferList;
struct MaterialList;
struct RenderPass;
struct ResourceTransfer;
struct SceneData;
struct RenderPassFuncArgsInit {
  const nlohmann::json* json{nullptr};
  uint32_t frame_buffer_num{0};
  MemoryAllocationJanitor* allocator{nullptr};
  BufferList* buffer_list{nullptr};
  BufferConfig* buffer_config_list{nullptr};
  RenderPass* render_pass_list{nullptr};
};
struct RenderPassConfigDynamicData {
  float camera_pos[3]{};
  float camera_focus[3]{};
  float fov_vertical{40.0f};
  float near_z{0.001f};
  float far_z{1000.0f};
  float light_direction[3]{};
  float light_color[3]{};
  float light_intensity{};
};
struct RenderPassFuncArgsRenderCommon {
  const MainBufferSize* main_buffer_size{nullptr};
  SceneData* scene_data{nullptr};
  uint32_t frame_index{0};
  RenderPassConfigDynamicData* dynamic_data{nullptr};
  const RenderPass* render_pass_list{nullptr};
  MaterialList* material_list{nullptr};
  ResourceTransfer* resource_transfer{nullptr};
};
struct RenderPassFuncArgsRenderPerPass {
  D3d12CommandList* command_list{nullptr};
  void* pass_vars_ptr{nullptr};
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles_view{nullptr};
  const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles_sampler{nullptr};
  const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles{nullptr};
  ID3D12Resource** resources{nullptr};
  uint32_t render_pass_index{};
};
using RenderPassFuncInit = void* (*)(RenderPassFuncArgsInit* args, const uint32_t render_pass_index);
using RenderPassFuncTerm = void (*)(void);
using RenderPassFuncUpdate = void (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
using RenderPassFuncIsRenderNeeded = bool (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
using RenderPassFuncRender = void (*)(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
struct RenderPassFunctionList {
  RenderPassFuncInit* init{nullptr};
  RenderPassFuncTerm* term{nullptr};
  RenderPassFuncUpdate* update{nullptr};
  RenderPassFuncIsRenderNeeded* is_render_needed{nullptr};
  RenderPassFuncRender* render{nullptr};
};
}
#endif
