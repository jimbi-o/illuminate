#include "d3d12_resource_transferer.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
}
ResourceTransfer PrepareResourceTransferer(const uint32_t frame_buffer_num, const uint32_t resource_num_per_frame) {
  ResourceTransfer r {
    .max_transfer_buffer_num_per_frame = resource_num_per_frame,
    .transfer_reserved_buffer_num      = AllocateArray<uint32_t>(gSystemMemoryAllocator, frame_buffer_num),
    .transfer_reserved_buffer_src      = AllocateArray<ID3D12Resource**>(gSystemMemoryAllocator, frame_buffer_num),
    .transfer_reserved_buffer_dst      = AllocateArray<ID3D12Resource**>(gSystemMemoryAllocator, frame_buffer_num),
    .transfer_processed_buffer_num     = AllocateArray<uint32_t>(gSystemMemoryAllocator, frame_buffer_num),
    .transfer_processed_buffer_src     = AllocateArray<ID3D12Resource**>(gSystemMemoryAllocator, frame_buffer_num),
  };
  for (uint32_t i = 0; i < frame_buffer_num; i++) {
    r.transfer_reserved_buffer_num[i]  = 0;
    r.transfer_reserved_buffer_src[i]  = AllocateArray<ID3D12Resource*>(gSystemMemoryAllocator, resource_num_per_frame);
    r.transfer_reserved_buffer_dst[i]  = AllocateArray<ID3D12Resource*>(gSystemMemoryAllocator, resource_num_per_frame);
    r.transfer_processed_buffer_num[i] = 0;
    r.transfer_processed_buffer_src[i] = AllocateArray<ID3D12Resource*>(gSystemMemoryAllocator, resource_num_per_frame);
  }
  return r;
}
namespace {
void ClearResourcePoolIfNeeded(ID3D12Resource* resource, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < ResourceTransfer::kResourcePoolLen; i++) {
    if (resource_transfer->resource_pool[i] == nullptr || resource_transfer->resource_pool[i][0] != resource) { continue; }
    for (uint32_t j = 0; j < resource_transfer->resource_pool_len[i]; j++) {
      resource_transfer->allocation_pool[i][j]->Release();
    }
    // delete[] resource_transfer->resource_pool[i];
    // delete[] resource_transfer->allocation_pool[i];
    resource_transfer->resource_pool[i] = nullptr;
    resource_transfer->allocation_pool[i] = nullptr;
    resource_transfer->resource_pool_len[i] = 0;
    break;
  }
}
}
void NotifyTransferReservedResourcesProcessed(const uint32_t frame_index, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < resource_transfer->transfer_processed_buffer_num[frame_index]; i++) {
    resource_transfer->transfer_processed_buffer_src[frame_index][i]->Release();
    ClearResourcePoolIfNeeded(resource_transfer->transfer_processed_buffer_src[frame_index][i], resource_transfer);
  }
  resource_transfer->transfer_processed_buffer_num[frame_index] = resource_transfer->transfer_reserved_buffer_num[frame_index];
  resource_transfer->transfer_reserved_buffer_num[frame_index] = 0;
  std::swap(resource_transfer->transfer_reserved_buffer_src[frame_index], resource_transfer->transfer_processed_buffer_src[frame_index]);
}
ID3D12Resource** RetainUploadBuffer(const uint32_t num, const D3D12_RESOURCE_DESC1& desc, MemoryAllocationJanitor* allocator, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < ResourceTransfer::kResourcePoolLen; i++) {
    if (resource_transfer->resource_pool[i] != nullptr) { continue; }
    resource_transfer->resource_pool_len[i] = num;
    resource_transfer->resource_pool[i] = AllocateArray<ID3D12Resource*>(allocator, num);
    resource_transfer->allocation_pool[i] = AllocateArray<D3D12MA::Allocation*>(allocator, num);
    for (uint32_t j = 0; j < num; j++) {
      CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, nullptr, buffer_allocator,
                   &resource_transfer->allocation_pool[i][j],
                   &resource_transfer->resource_pool[i][j]);
    }
    return resource_transfer->resource_pool[i];
  }
  assert(false && "RetainUploadBuffer::resource_pool is full.");
  return nullptr;
}
void ReturnUploadBuffer(const uint32_t num, ID3D12Resource** list, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < num; i++) {
    list[i]->Release();
    ClearResourcePoolIfNeeded(list[i], resource_transfer);
  }
}
bool ReserveResourceTransfer(const uint32_t frame_index, ID3D12Resource* src, ID3D12Resource* dst, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_reserved_buffer_num[frame_index] + 1 > resource_transfer->max_transfer_buffer_num_per_frame) { return false; }
  const auto index = resource_transfer->transfer_reserved_buffer_num[frame_index];
  resource_transfer->transfer_reserved_buffer_num[frame_index]++;
  resource_transfer->transfer_reserved_buffer_src[frame_index][index] = src;
  resource_transfer->transfer_reserved_buffer_dst[frame_index][index] = dst;
  return true;
}
bool ReserveResourceTransfer(const uint32_t frame_index, const uint32_t num, ID3D12Resource** src, ID3D12Resource** dst, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_reserved_buffer_num[frame_index] + num > resource_transfer->max_transfer_buffer_num_per_frame) { return false; }
  const auto index = resource_transfer->transfer_reserved_buffer_num[frame_index];
  resource_transfer->transfer_reserved_buffer_num[frame_index] += num;
  memcpy(&resource_transfer->transfer_reserved_buffer_src[frame_index][index], src, sizeof(resource_transfer->transfer_reserved_buffer_src[frame_index][index]) * num);
  memcpy(&resource_transfer->transfer_reserved_buffer_dst[frame_index][index], dst, sizeof(resource_transfer->transfer_reserved_buffer_dst[frame_index][index]) * num);
  return true;
}
} // namespace illuminate
