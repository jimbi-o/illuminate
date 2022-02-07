#ifndef ILLUMINATE_VIEW_UTIL_H
#define ILLUMINATE_VIEW_UTIL_H
#include "d3d12_header_common.h"
namespace illuminate {
struct BufferConfig;
bool CreateView(D3d12Device* device, const DescriptorType& descriptor_type, const BufferConfig& config, ID3D12Resource* resource, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
}
#endif
