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
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout{nullptr};
  uint32_t* num_rows{nullptr};
  uint64_t* row_size_in_bytes{nullptr};
  uint32_t total_size_in_bytes{0};
};
TextureCreationInfo GatherTextureCreationInfo(D3d12Device* device, const wchar_t* filepath, MemoryAllocationJanitor* allocator);
void FillUploadResource(const TextureCreationInfo& info, ID3D12Resource* upload_resource);
void SetCopyCommand(const uint32_t subresource_num, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout, ID3D12Resource* upload_resource, ID3D12Resource* default_resource, D3d12CommandList* command_list);
}
#endif
