#ifndef ILLUMINATE_D3D12_RENDER_GRAPH_H
#define ILLUMINATE_D3D12_RENDER_GRAPH_H
#include "d3d12_header_common.h"
#include "illuminate/core/strid.h"
#include "illuminate/util/util_defines.h"
namespace illuminate {
struct BufferConfig {
  uint32_t buffer_index{};
  D3D12_HEAP_TYPE heap_type{};
  D3D12_RESOURCE_DIMENSION dimension{};
  uint32_t alignment{};
  BufferSizeRelativeness size_type{};
  float width{};
  float height{};
  uint16_t depth_or_array_size{};
  uint16_t miplevels{};
  DXGI_FORMAT format{};
  uint32_t sample_count{};
  uint32_t  sample_quality{};
  D3D12_TEXTURE_LAYOUT layout{};
  uint32_t mip_width{};
  uint32_t mip_height{};
  uint32_t mip_depth{};
  ResourceStateType initial_state{};
  D3D12_CLEAR_VALUE clear_value{};
  DescriptorTypeFlag descriptor_type_flags{kDescriptorTypeFlagNone};
  bool descriptor_only{false};
  bool pingpong{false};
  bool frame_buffered{false};
  uint32_t num_elements{0};
  uint32_t stride_bytes{0};
  bool raw_buffer{false};
};
struct RenderPassBuffer {
  uint32_t buffer_index{~0U};
  uint32_t index_offset{0U};
  ResourceStateType state{};
};
struct RenderPass {
  StrHash name{};
  StrHash type{};
  bool enabled{false};
  uint32_t index{~0U};
  uint32_t command_queue_index{0};
  uint32_t buffer_num{0};
  RenderPassBuffer* buffer_list{nullptr};
  uint32_t max_buffer_index_offset{0};
  uint32_t material{};
  bool sends_signal{false};
  uint32_t wait_pass_num{0};
  uint32_t* signal_queue_index{nullptr};
  uint32_t* signal_pass_index{nullptr};
  uint32_t sampler_num{0};
  uint32_t* sampler_index_list{nullptr};
  uint32_t flip_pingpong_num{0};
  uint32_t* flip_pingpong_index_list{nullptr};
};
enum CBufferParamType : uint8_t {
  kSpecial = 0,
  kFloat,
  kUint,
};
struct CBufferParam {
  const char* name{};
  StrHash name_hash{};
  CBufferParamType type{};
  float min{};
  float max{};
  float initial_val{};
  uint32_t size_in_bytes;
};
struct CBuffer {
  uint32_t buffer_index{};
  uint32_t need_ui_param_num{};
  ArrayOf<CBufferParam> params{};
};
struct RenderGraphConfig {
  uint32_t frame_buffer_num{0};
  uint32_t primarybuffer_width{0};
  uint32_t primarybuffer_height{0};
  DXGI_FORMAT primarybuffer_format{};
  const char* window_title{nullptr};
  uint32_t window_width{0};
  uint32_t window_height{0};
  uint32_t command_queue_num{0};
  StrHash* command_queue_name{nullptr};
  D3D12_COMMAND_LIST_TYPE* command_queue_type{nullptr};
  D3D12_COMMAND_QUEUE_PRIORITY* command_queue_priority{nullptr};
  uint32_t* command_list_num_per_queue{nullptr};
  uint32_t command_allocator_num_per_queue_type[kCommandQueueTypeNum]{};
  uint32_t swapchain_command_queue_index{0};
  DXGI_FORMAT swapchain_format{};
  DXGI_USAGE swapchain_usage{};
  uint32_t render_pass_num{0};
  RenderPass* render_pass_list{nullptr};
  uint32_t buffer_num{0};
  BufferConfig* buffer_list{nullptr};
  uint32_t sampler_num{0};
  D3D12_SAMPLER_DESC* sampler_list{nullptr};
  uint32_t descriptor_handle_num_per_type[kDescriptorTypeNum]{};
  uint32_t gpu_handle_num_view{1024};
  uint32_t gpu_handle_num_sampler{16};
  uint32_t max_model_num{1024};
  uint32_t max_material_num{1024};
  uint32_t max_mipmap_num{12};
  uint32_t timestamp_query_dst_resource_num{5};
  ArrayOf<CBuffer> cbuffer_list{};
};
}
#endif
