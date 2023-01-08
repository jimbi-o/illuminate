#include "d3d12_resource_transfer.h"
#include "d3d12_src_common.h"
namespace illuminate {
ResourceTransfer PrepareResourceTransferer(const uint32_t frame_buffer_num, const uint32_t resource_num_per_frame, const uint32_t max_mipmap_num) {
  // TODO use double buffered memory?
  ResourceTransfer r {
    .max_transfer_buffer_num_per_frame        = resource_num_per_frame,
    .layout_num_per_texture                   = max_mipmap_num,
    .transfer_reserved_buffer_num             = AllocateArraySystem<uint32_t>(frame_buffer_num),
    .transfer_reserved_buffer_src             = AllocateArraySystem<ID3D12Resource**>(frame_buffer_num),
    .transfer_reserved_buffer_src_allocation  = AllocateArraySystem<D3D12MA::Allocation**>(frame_buffer_num),
    .transfer_reserved_buffer_dst             = AllocateArraySystem<ID3D12Resource**>(frame_buffer_num),
    .transfer_reserved_texture_num            = AllocateArraySystem<uint32_t>(frame_buffer_num),
    .transfer_reserved_texture_src            = AllocateArraySystem<ID3D12Resource**>(frame_buffer_num),
    .transfer_reserved_texture_src_allocation = AllocateArraySystem<D3D12MA::Allocation**>(frame_buffer_num),
    .transfer_reserved_texture_dst            = AllocateArraySystem<ID3D12Resource**>(frame_buffer_num),
    .texture_subresource_num                  = AllocateArraySystem<uint32_t*>(frame_buffer_num),
    .texture_layout                           = AllocateArraySystem<D3D12_PLACED_SUBRESOURCE_FOOTPRINT**>(frame_buffer_num),
    .transfer_processed_buffer_num            = AllocateArraySystem<uint32_t>(frame_buffer_num),
    .transfer_processed_buffer                = AllocateArraySystem<ID3D12Resource**>(frame_buffer_num),
    .transfer_processed_buffer_allocation     = AllocateArraySystem<D3D12MA::Allocation**>(frame_buffer_num),
  };
  for (uint32_t i = 0; i < frame_buffer_num; i++) {
    r.transfer_reserved_buffer_num[i]             = 0;
    r.transfer_reserved_buffer_src[i]             = AllocateArraySystem<ID3D12Resource*>(resource_num_per_frame);
    r.transfer_reserved_buffer_src_allocation[i]  = AllocateArraySystem<D3D12MA::Allocation*>(resource_num_per_frame);
    r.transfer_reserved_buffer_dst[i]             = AllocateArraySystem<ID3D12Resource*>(resource_num_per_frame);
    r.transfer_reserved_texture_num[i]            = 0;
    r.texture_subresource_num[i]                  = AllocateArraySystem<uint32_t>(resource_num_per_frame);
    r.texture_layout[i]                           = AllocateArraySystem<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(resource_num_per_frame);
    r.transfer_reserved_texture_src[i]            = AllocateArraySystem<ID3D12Resource*>(resource_num_per_frame);
    r.transfer_reserved_texture_src_allocation[i] = AllocateArraySystem<D3D12MA::Allocation*>(resource_num_per_frame);
    r.transfer_reserved_texture_dst[i]            = AllocateArraySystem<ID3D12Resource*>(resource_num_per_frame);
    r.transfer_processed_buffer_num[i]            = 0;
    r.transfer_processed_buffer[i]                = AllocateArraySystem<ID3D12Resource*>(resource_num_per_frame * 2);
    r.transfer_processed_buffer_allocation[i]     = AllocateArraySystem<D3D12MA::Allocation*>(resource_num_per_frame * 2);
    for (uint32_t j = 0; j < resource_num_per_frame; j++) {
      r.texture_layout[i][j] = AllocateArraySystem<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>(max_mipmap_num);
    }
  }
  return r;
}
void ClearResourceTransfer(const uint32_t frame_buffer_num, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < frame_buffer_num; i++) {
    NotifyTransferReservedResourcesProcessed(i, resource_transfer);
    NotifyTransferReservedResourcesProcessed(i, resource_transfer);
  }
}
bool IsResourceTransferReserved(const uint32_t frame_index, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_reserved_buffer_num[frame_index] > 0) { return true; }
  if ( resource_transfer->transfer_reserved_texture_num[frame_index] > 0) { return true; }
  return false;
}
void NotifyTransferReservedResourcesProcessed(const uint32_t frame_index, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_processed_buffer_num[frame_index] == 0
      && !IsResourceTransferReserved(frame_index, resource_transfer)) {
    return;
  }
  for (uint32_t i = 0; i < resource_transfer->transfer_processed_buffer_num[frame_index]; i++) {
    resource_transfer->transfer_processed_buffer[frame_index][i]->Release();
    resource_transfer->transfer_processed_buffer_allocation[frame_index][i]->Release();
  }
  const auto buffer_num  = resource_transfer->transfer_reserved_buffer_num[frame_index];
  const auto texture_num = resource_transfer->transfer_reserved_texture_num[frame_index];
  resource_transfer->transfer_reserved_buffer_num[frame_index]  = 0;
  resource_transfer->transfer_reserved_texture_num[frame_index] = 0;
  resource_transfer->transfer_processed_buffer_num[frame_index] = buffer_num + texture_num;
  std::copy(resource_transfer->transfer_reserved_buffer_src[frame_index],
            &resource_transfer->transfer_reserved_buffer_src[frame_index][buffer_num],
            resource_transfer->transfer_processed_buffer[frame_index]);
  std::copy(resource_transfer->transfer_reserved_buffer_src_allocation[frame_index],
            &resource_transfer->transfer_reserved_buffer_src_allocation[frame_index][buffer_num],
            resource_transfer->transfer_processed_buffer_allocation[frame_index]);
  std::copy(resource_transfer->transfer_reserved_texture_src[frame_index],
            &resource_transfer->transfer_reserved_texture_src[frame_index][texture_num],
            &resource_transfer->transfer_processed_buffer[frame_index][buffer_num]);
  std::copy(resource_transfer->transfer_reserved_texture_src_allocation[frame_index],
            &resource_transfer->transfer_reserved_texture_src_allocation[frame_index][texture_num],
            &resource_transfer->transfer_processed_buffer_allocation[frame_index][buffer_num]);
}
bool ReserveResourceTransfer(const uint32_t frame_index, ID3D12Resource* src, D3D12MA::Allocation* src_allocation, ID3D12Resource* dst, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_reserved_buffer_num[frame_index] >= resource_transfer->max_transfer_buffer_num_per_frame) { return false; }
  const auto index = resource_transfer->transfer_reserved_buffer_num[frame_index];
  resource_transfer->transfer_reserved_buffer_num[frame_index]++;
  resource_transfer->transfer_reserved_buffer_src[frame_index][index] = src;
  resource_transfer->transfer_reserved_buffer_src_allocation[frame_index][index] = src_allocation;
  resource_transfer->transfer_reserved_buffer_dst[frame_index][index] = dst;
  return true;
}
bool ReserveResourceTransfer(const uint32_t frame_index, const uint32_t subresource_num, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout, ID3D12Resource* src, D3D12MA::Allocation* src_allocation, ID3D12Resource* dst, ResourceTransfer* resource_transfer) {
  if (resource_transfer->transfer_reserved_texture_num[frame_index] >= resource_transfer->max_transfer_buffer_num_per_frame) { return false; }
  if (subresource_num > resource_transfer->layout_num_per_texture) {
    logwarn(" exceeded ResourceTransfer mipmap num. allocated:{} > in:{}", subresource_num, resource_transfer->layout_num_per_texture);
    return false;
  }
  const auto index = resource_transfer->transfer_reserved_texture_num[frame_index];
  resource_transfer->transfer_reserved_texture_num[frame_index]++;
  resource_transfer->transfer_reserved_texture_src[frame_index][index] = src;
  resource_transfer->transfer_reserved_texture_src_allocation[frame_index][index] = src_allocation;
  resource_transfer->transfer_reserved_texture_dst[frame_index][index] = dst;
  resource_transfer->texture_subresource_num[frame_index][index] = subresource_num;
  std::copy(layout, &layout[subresource_num], resource_transfer->texture_layout[frame_index][index]);
  return true;
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_texture_util.h"
TEST_CASE("ResourceTransfer") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  // main process
  uint32_t frame_index = 0;
  auto resource_transfer = PrepareResourceTransferer(2, 1024, 11);
  ID3D12Resource* resource_upload{};
  D3D12MA::Allocation* allocation_upload{};
  auto create_buffer = [](const D3D12_RESOURCE_DESC1& desc, D3D12MA::Allocator* allocator, ID3D12Resource** resource_upload, D3D12MA::Allocation** allocation_upload) {
    CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, nullptr, allocator, allocation_upload, resource_upload);
  };
  SUBCASE("empty") {
  }
  SUBCASE("buffer") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("texture") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("buffer+texture/single") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("buffer+texture/multiple") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  NotifyTransferReservedResourcesProcessed(frame_index, &resource_transfer);
  NotifyTransferReservedResourcesProcessed(frame_index, &resource_transfer);
  // term
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
TEST_CASE("ClearResourceTransfer") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  // main process
  uint32_t frame_index = 0;
  auto resource_transfer = PrepareResourceTransferer(2, 1024, 11);
  ID3D12Resource* resource_upload{};
  D3D12MA::Allocation* allocation_upload{};
  auto create_buffer = [](const D3D12_RESOURCE_DESC1& desc, D3D12MA::Allocator* allocator, ID3D12Resource** resource_upload, D3D12MA::Allocation** allocation_upload) {
    CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, nullptr, allocator, allocation_upload, resource_upload);
  };
  SUBCASE("empty") {
  }
  SUBCASE("buffer") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("texture") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("buffer+texture/single") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  SUBCASE("buffer+texture/multiple") {
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, 0, nullptr, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
    create_buffer(GetBufferDesc(256), buffer_allocator, &resource_upload, &allocation_upload);
    CHECK_UNARY(ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, nullptr, &resource_transfer));
  }
  ClearResourceTransfer(2, &resource_transfer);
  // term
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
