#include "d3d12_texture_util.h"
#include <memory>
#include "DDSTextureLoader.h"
#include "d3dx12.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
auto ConvertToResourceDesc1(const D3D12_RESOURCE_DESC& desc) {
  return D3D12_RESOURCE_DESC1 {
    .Dimension = desc.Dimension,
    .Alignment = desc.Alignment,
    .Width = desc.Width,
    .Height = desc.Height,
    .DepthOrArraySize = desc.DepthOrArraySize,
    .MipLevels = desc.MipLevels,
    .Format = desc.Format,
    .SampleDesc = desc.SampleDesc,
    .Layout = desc.Layout,
    .Flags = desc.Flags,
    .SamplerFeedbackMipRegion = {
      .Width = 0,
      .Height = 0,
      .Depth = 0,
    },
  };
}
void SetCopyCommand(const uint32_t subresource_num, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout, ID3D12Resource* upload_resource, ID3D12Resource* default_resource, D3d12CommandList* command_list) {
  for (uint32_t i = 0; i < subresource_num; i++) {
    CD3DX12_TEXTURE_COPY_LOCATION dst(default_resource, i);
    CD3DX12_TEXTURE_COPY_LOCATION src(upload_resource, layout[i]);
    command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  }
}
}
D3D12_RESOURCE_DESC1 GetBufferDesc(const uint32_t size_in_bytes) {
  return D3D12_RESOURCE_DESC1 {
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = size_in_bytes,
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {
      .Count = 1,
      .Quality = 0,
    },
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE,
    .SamplerFeedbackMipRegion = {
      .Width = 0,
      .Height = 0,
      .Depth = 0,
    },
  };
}
TextureCreationInfo GatherTextureCreationInfo(D3d12Device* device, const wchar_t* filepath, MemoryAllocationJanitor* allocator) {
  ID3D12Resource* committed_resource{};
  std::unique_ptr<std::uint8_t[]> dds_data;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  auto hr = DirectX::LoadDDSTextureFromFile(device, filepath, &committed_resource, dds_data, subresources);
  if (FAILED(hr)) {
    if (committed_resource) {
      committed_resource->Release();
    }
    return {};
  }
  const auto resource_desc_src = committed_resource->GetDesc();
  committed_resource->Release();
  TextureCreationInfo info{};
  info.resource_desc = ConvertToResourceDesc1(resource_desc_src);
  info.subresource_num = GetUint32(subresources.size());
  info.subresources = AllocateArray<D3D12_SUBRESOURCE_DATA>(allocator, info.subresource_num);
  for (uint32_t i = 0; i < info.subresource_num; i++) {
    memcpy(&info.subresources[i], &subresources[i], sizeof(info.subresources));
  }
  info.dds_data = std::move(dds_data);
  info.layout = AllocateArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>(allocator, info.subresource_num);
  info.num_rows = AllocateArray<uint32_t>(allocator, info.subresource_num);
  info.row_size_in_bytes = AllocateArray<uint64_t>(allocator, info.subresource_num);
  uint64_t total_bytes{};
  device->GetCopyableFootprints(&resource_desc_src, 0, info.subresource_num, 0, info.layout, info.num_rows, info.row_size_in_bytes, &total_bytes);
  info.total_size_in_bytes = GetUint32(total_bytes);
  return info;
}
void FillUploadResource(const TextureCreationInfo& info, ID3D12Resource* upload_resource) {
  auto upload_buffer = MapResource(upload_resource, info.total_size_in_bytes);
  for (uint32_t i = 0; i < info.subresource_num; i++) {
    auto ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(upload_buffer) + info.layout[i].Offset);
    D3D12_MEMCPY_DEST DestData = {ptr, info.layout[i].Footprint.RowPitch, SIZE_T(info.layout[i].Footprint.RowPitch) * SIZE_T(info.num_rows[i])};
    MemcpySubresource(&DestData, &info.subresources[i], static_cast<SIZE_T>(info.row_size_in_bytes[i]), info.num_rows[i], info.layout[i].Footprint.Depth);
  }
  UnmapResource(upload_resource);
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
TEST_CASE("create white texture") { // NOLINT
  // https://www.shader.jp/?page_id=2076
  // ../tools/texconv.exe -nologo -fl 12.1 -keepcoverage 0.75 -sepalpha -if CUBIC -f BC7_UNORM -y ../tmp/white.png
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  CommandListSet command_list_set;
  {
    const auto type = D3D12_COMMAND_LIST_TYPE_COPY;
    const auto priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    const uint32_t command_list_num_per_queue = 1;
    const uint32_t command_allocator_num_per_queue_type[kCommandQueueTypeNum] = {0,0,1};
    command_list_set.Init(device.Get(), 1, &type, &priority, &command_list_num_per_queue, 2, command_allocator_num_per_queue_type);
  }
  CommandQueueSignals command_queue_signals;
  uint64_t signal_val_list[] = {0UL};
  command_queue_signals.Init(device.Get(), 1, command_list_set.GetCommandQueueList());
  // main process
  ID3D12Resource* committed_resource{};
  std::unique_ptr<std::uint8_t[]> dds_data;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  auto hr = DirectX::LoadDDSTextureFromFile(device.Get(), L"textures/white.dds", &committed_resource, dds_data, subresources);
  CHECK_UNARY(SUCCEEDED(hr));
  CHECK_NE(committed_resource, nullptr);
  auto resource_desc_src = committed_resource->GetDesc();
  auto resource_desc = ConvertToResourceDesc1(resource_desc_src);
  committed_resource->Release();
  auto tmp_allocator = GetTemporalMemoryAllocator();
  const auto subresource_num = GetUint32(subresources.size());
  auto layout = AllocateArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>(&tmp_allocator, subresource_num);
  auto num_rows = AllocateArray<uint32_t>(&tmp_allocator, subresource_num);
  auto row_size_in_bytes = AllocateArray<uint64_t>(&tmp_allocator, subresource_num);
  uint64_t total_bytes{};
  device.Get()->GetCopyableFootprints(&resource_desc_src, 0, subresource_num, 0, layout, num_rows, row_size_in_bytes, &total_bytes);
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  D3D12MA::Allocation* upload_allocation{nullptr};
  ID3D12Resource* upload_resource{nullptr};
  CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, GetBufferDesc(GetUint32(total_bytes)), nullptr, buffer_allocator, &upload_allocation, &upload_resource);
  CHECK_NE(upload_allocation, nullptr);
  CHECK_NE(upload_resource, nullptr);
  // based on UpdateSubresources.
  auto upload_buffer = MapResource(upload_resource, GetUint32(total_bytes));
  for (uint32_t i = 0; i < subresource_num; i++) {
    auto ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(upload_buffer) + layout[i].Offset);
    D3D12_MEMCPY_DEST DestData = { ptr, layout[i].Footprint.RowPitch, SIZE_T(layout[i].Footprint.RowPitch) * SIZE_T(num_rows[i]) };
    MemcpySubresource(&DestData, &subresources[i], static_cast<SIZE_T>(row_size_in_bytes[i]), num_rows[i], layout[i].Footprint.Depth);
  }
  UnmapResource(upload_resource);
  D3D12MA::Allocation* default_allocation{nullptr};
  ID3D12Resource* default_resource{nullptr};
  CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, buffer_allocator, &default_allocation, &default_resource);
  CHECK_NE(default_allocation, nullptr);
  CHECK_NE(default_resource, nullptr);
  auto command_list = command_list_set.GetCommandList(device.Get(), 0);
  for (uint32_t i = 0; i < subresource_num; i++) {
    CD3DX12_TEXTURE_COPY_LOCATION dst(default_resource, i);
    CD3DX12_TEXTURE_COPY_LOCATION src(upload_resource, layout[i]);
    command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  }
  command_list_set.ExecuteCommandList(0);
  // term
  command_queue_signals.WaitOnCpu(device.Get(), signal_val_list);
  command_queue_signals.WaitAll(device.Get());
  default_allocation->Release();
  default_resource->Release();
  upload_allocation->Release();
  upload_resource->Release();
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
TEST_CASE("gather texture creation info") { // NOLINT
  // https://www.shader.jp/?page_id=2076
  // ../tools/texconv.exe -nologo -fl 12.1 -keepcoverage 0.75 -sepalpha -if CUBIC -f BC7_UNORM -y ../tmp/white.png
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CommandListSet command_list_set;
  {
    const auto type = D3D12_COMMAND_LIST_TYPE_COPY;
    const auto priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    const uint32_t command_list_num_per_queue = 1;
    const uint32_t command_allocator_num_per_queue_type[kCommandQueueTypeNum] = {0,0,1};
    command_list_set.Init(device.Get(), 1, &type, &priority, &command_list_num_per_queue, 2, command_allocator_num_per_queue_type);
  }
  CommandQueueSignals command_queue_signals;
  uint64_t signal_val_list[] = {0UL};
  command_queue_signals.Init(device.Get(), 1, command_list_set.GetCommandQueueList());
  // main process
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto texture_creation_info = GatherTextureCreationInfo(device.Get(), L"textures/white.dds", &tmp_allocator);
  D3D12MA::Allocation* upload_allocation{nullptr};
  ID3D12Resource* upload_resource{nullptr};
  CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, GetBufferDesc(texture_creation_info.total_size_in_bytes), nullptr, buffer_allocator, &upload_allocation, &upload_resource);
  CHECK_NE(upload_allocation, nullptr);
  CHECK_NE(upload_resource, nullptr);
  FillUploadResource(texture_creation_info, upload_resource);
  D3D12MA::Allocation* default_allocation{nullptr};
  ID3D12Resource* default_resource{nullptr};
  CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, texture_creation_info.resource_desc, nullptr, buffer_allocator, &default_allocation, &default_resource);
  CHECK_NE(default_allocation, nullptr);
  CHECK_NE(default_resource, nullptr);
  auto command_list = command_list_set.GetCommandList(device.Get(), 0);
  SetCopyCommand(texture_creation_info.subresource_num, texture_creation_info.layout, upload_resource, default_resource, command_list);
  command_list_set.ExecuteCommandList(0);
  // term
  command_queue_signals.WaitOnCpu(device.Get(), signal_val_list);
  command_queue_signals.WaitAll(device.Get());
  default_allocation->Release();
  default_resource->Release();
  upload_allocation->Release();
  upload_resource->Release();
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
