#ifndef ILLUMINATE_D3D12_RESOURCE_COPIER_H
#define ILLUMINATE_D3D12_RESOURCE_COPIER_H
#include "d3d12_header_common.h"
#include "d3d12_render_graph.h"
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
struct ResourceTransfer {
  uint32_t                               max_transfer_buffer_num_per_frame{};
  uint32_t                               layout_num_per_texture{};
  uint32_t*                              transfer_reserved_buffer_num{};
  ID3D12Resource***                      transfer_reserved_buffer_src{};
  D3D12MA::Allocation***                 transfer_reserved_buffer_src_allocation{};
  ID3D12Resource***                      transfer_reserved_buffer_dst{};
  uint32_t*                              transfer_reserved_texture_num{};
  ID3D12Resource***                      transfer_reserved_texture_src{};
  D3D12MA::Allocation***                 transfer_reserved_texture_src_allocation{};
  ID3D12Resource***                      transfer_reserved_texture_dst{};
  uint32_t**                             texture_subresource_num{};
  uint32_t**                             texture_layout_index_start{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT***  texture_layout{};
  uint32_t*                              transfer_processed_buffer_num{};
  ID3D12Resource***                      transfer_processed_buffer{};
  D3D12MA::Allocation***                 transfer_processed_buffer_allocation{};
};
ResourceTransfer PrepareResourceTransferer(const uint32_t frame_buffer_num, const uint32_t resource_num_per_frame, const uint32_t max_mipmap_num);
void ClearResourceTransfer(const uint32_t frame_buffer_num, ResourceTransfer* resource_transfer);
bool ReserveResourceTransfer(const uint32_t frame_index, ID3D12Resource* src, D3D12MA::Allocation* src_allocation, ID3D12Resource* dst, ResourceTransfer* resource_transfer);
bool ReserveResourceTransfer(const uint32_t frame_index, const uint32_t subresource_num, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout, ID3D12Resource* src, D3D12MA::Allocation* src_allocation, ID3D12Resource* dst, ResourceTransfer* resource_transfer);
bool IsResourceTransferReserved(const uint32_t frame_index, ResourceTransfer* resource_transfer);
void NotifyTransferReservedResourcesProcessed(const uint32_t frame_index, ResourceTransfer* resource_transfer);
}
#endif
