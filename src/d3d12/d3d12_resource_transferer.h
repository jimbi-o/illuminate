#ifndef ILLUMINATE_D3D12_RESOURCE_COPIER_H
#define ILLUMINATE_D3D12_RESOURCE_COPIER_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_render_graph.h"
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
struct ResourceTransfer {
  static const uint32_t kResourcePoolLen = 4;
  uint32_t max_transfer_buffer_num_per_frame{};
  uint32_t* transfer_reserved_buffer_num{};
  ID3D12Resource*** transfer_reserved_buffer_src{};
  ID3D12Resource*** transfer_reserved_buffer_dst{};
  uint32_t* transfer_processed_buffer_num{};
  ID3D12Resource*** transfer_processed_buffer_src{};
  uint32_t resource_pool_len[kResourcePoolLen]{};
  ID3D12Resource** resource_pool[kResourcePoolLen]{};
  D3D12MA::Allocation** allocation_pool[kResourcePoolLen]{};
};
ResourceTransfer PrepareResourceTransferer(const uint32_t frame_buffer_num, const uint32_t resource_num_per_frame);
void NotifyTransferReservedResourcesProcessed(const uint32_t frame_index, ResourceTransfer* resource_transfer);
class MemoryAllocationJanitor;
ID3D12Resource** RetainUploadBuffer(const uint32_t num, const D3D12_RESOURCE_DESC1& desc, MemoryAllocationJanitor* allocator, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer);
void ReturnUploadBuffer(const uint32_t num, ID3D12Resource** list, ResourceTransfer* resource_transfer);
bool ReserveResourceTransfer(const uint32_t frame_index, ID3D12Resource* src, ID3D12Resource* dst, ResourceTransfer* resource_transfer);
bool ReserveResourceTransfer(const uint32_t frame_index, const uint32_t num, ID3D12Resource** src, ID3D12Resource** dst, ResourceTransfer* resource_transfer);
}
#endif
