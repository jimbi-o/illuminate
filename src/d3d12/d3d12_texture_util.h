#ifndef ILLUMINATE_D3D12_TEXTURE_UTIL_H
#define ILLUMINATE_D3D12_TEXTURE_UTIL_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
struct TextureCreationInfo {
  D3D12_RESOURCE_DESC1 resource_desc{};
  uint32_t subresource_num{0};
  D3D12_SUBRESOURCE_DATA* subresources{nullptr};
  std::unique_ptr<std::uint8_t[]> dds_data;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout{nullptr};
  uint32_t* num_rows{nullptr};
  uint64_t* row_size_in_bytes{nullptr};
  uint32_t total_size_in_bytes{0};
};
D3D12_RESOURCE_DESC1 GetBufferDesc(const uint32_t size_in_bytes);
TextureCreationInfo GatherTextureCreationInfo(D3d12Device* device, const wchar_t* filepath);
void FillUploadResource(const TextureCreationInfo& info, ID3D12Resource* upload_resource);
}
#endif
