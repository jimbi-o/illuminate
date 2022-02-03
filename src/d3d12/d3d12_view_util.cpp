#include "d3d12_view_util.h"
#include "d3d12_render_graph.h"
#include "d3d12_src_common.h"
namespace illuminate {
auto GetUavDimension(const D3D12_RESOURCE_DIMENSION dimension, const uint16_t depth_or_array_size) {
  switch (dimension) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
      assert(false && "D3D12_RESOURCE_DIMENSION_UNKNOWN selected");
      return D3D12_UAV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_BUFFER: {
      return D3D12_UAV_DIMENSION_BUFFER;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
      if (depth_or_array_size > 1) {
        return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
      }
      return D3D12_UAV_DIMENSION_TEXTURE1D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
      if (depth_or_array_size > 1) {
        return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
      }
      return D3D12_UAV_DIMENSION_TEXTURE2D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
      return D3D12_UAV_DIMENSION_TEXTURE3D;
    }
  }
  logerror("GetUavDimension invalid dimension {} {}", dimension, depth_or_array_size);
  assert(false && "invalid dimension");
  return D3D12_UAV_DIMENSION_UNKNOWN;
}
auto GetDsvDimension(const D3D12_RESOURCE_DIMENSION dimension, const uint16_t depth_or_array_size) {
  switch (dimension) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
      assert(false && "D3D12_RESOURCE_DIMENSION_UNKNOWN selected");
      return D3D12_DSV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_BUFFER: {
      assert(false && "D3D12_RESOURCE_DIMENSION_BUFFER selected");
      return D3D12_DSV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
      if (depth_or_array_size > 1) {
        return D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
      }
      return D3D12_DSV_DIMENSION_TEXTURE1D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
      if (depth_or_array_size > 1) {
        return D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
      }
      return D3D12_DSV_DIMENSION_TEXTURE2D;
    }
  }
  logerror("GetDsvDimension invalid dimension {} {}", dimension, depth_or_array_size);
  assert(false && "invalid dimension");
  return D3D12_DSV_DIMENSION_UNKNOWN;
}
auto GetSrvDimension(const D3D12_RESOURCE_DIMENSION dimension, const uint16_t depth_or_array_size) {
  switch (dimension) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
      assert(false && "D3D12_RESOURCE_DIMENSION_UNKNOWN selected");
      return D3D12_SRV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_BUFFER: {
      return D3D12_SRV_DIMENSION_BUFFER;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
      if (depth_or_array_size > 1) {
        return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
      }
      return D3D12_SRV_DIMENSION_TEXTURE1D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
      if (depth_or_array_size > 1) {
        return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      }
      return D3D12_SRV_DIMENSION_TEXTURE2D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
      return D3D12_SRV_DIMENSION_TEXTURE3D;
    }
  }
  // cubemap?
  logerror("GetSrvDimension invalid dimension {} {}", dimension, depth_or_array_size);
  assert(false && "invalid dimension");
  return D3D12_SRV_DIMENSION_UNKNOWN;
}
auto GetSrvValidDxgiFormat(const DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_D32_FLOAT: { return DXGI_FORMAT_R32_FLOAT; }
    case DXGI_FORMAT_D24_UNORM_S8_UINT: { return DXGI_FORMAT_R24_UNORM_X8_TYPELESS; }
  }
  return format;
}
bool CreateView(D3d12Device* device, const DescriptorType& descriptor_type, const BufferConfig& config, ID3D12Resource* resource, const D3D12_CPU_DESCRIPTOR_HANDLE* handle) {
  switch (descriptor_type) {
    case DescriptorType::kCbv: {
      assert(false && "CreateView cbv not implemented");
      return false;
    }
    case DescriptorType::kSrv: {
      auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = GetSrvValidDxgiFormat(config.format),
        .ViewDimension = GetSrvDimension(config.dimension, config.depth_or_array_size),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      };
      switch (desc.ViewDimension) {
        case D3D12_SRV_DIMENSION_UNKNOWN: {
          assert(false && "invalid srv view dimension");
          break;
        }
        case D3D12_SRV_DIMENSION_BUFFER: {
          assert(false && "D3D12_SRV_DIMENSION_BUFFER not implemented");
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE1D: {
          desc.Texture1D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
          desc.Texture1DArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2D: {
          desc.Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
          desc.Texture2DArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
          desc.Texture2DMS = {};
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
          desc.Texture2DMSArray = {};
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE3D: {
          desc.Texture3D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURECUBE: {
          desc.TextureCube = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
          desc.TextureCubeArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .First2DArrayFace = 0,
            .NumCubes = config.depth_or_array_size,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE: {
          desc.RaytracingAccelerationStructure = {
            .Location = resource->GetGPUVirtualAddress(),
          };
          break;
        }
        default: {
          assert(false && "invalid srv ViewDimension");
          break;
        }
      }
      device->CreateShaderResourceView(resource, &desc, *handle);
      return true;
    }
    case DescriptorType::kUav: {
      auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = config.format,
        .ViewDimension = GetUavDimension(config.dimension, config.depth_or_array_size),
      };
      switch (desc.ViewDimension) {
        case D3D12_UAV_DIMENSION_UNKNOWN: {
          assert(false && "invalid uav view dimension");
          break;
        }
        case D3D12_UAV_DIMENSION_BUFFER: {
          assert(false && "D3D12_UAV_DIMENSION_BUFFER not implemented");
          desc.Buffer = {
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE1D: {
          desc.Texture1D = {
            .MipSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
          desc.Texture1DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE2D: {
          desc.Texture2D = {
            .MipSlice = 0,
            .PlaneSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
          desc.Texture2DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .PlaneSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE3D: {
          desc.Texture3D = {
            .MipSlice = 0,
            .FirstWSlice = 0,
            .WSize = config.depth_or_array_size,
          };
          break;
        }
        default: {
          assert(false && "invalid uav ViewDimension");
          break;
        }
      }
      device->CreateUnorderedAccessView(resource, nullptr, &desc, *handle);
      return true;
    }
    case DescriptorType::kRtv: {
      assert(false && "CreateView rtv not implemented");
      return false;
    }
    case DescriptorType::kDsv: {
      D3D12_DEPTH_STENCIL_VIEW_DESC desc{
        .Format = config.format,
        .ViewDimension = GetDsvDimension(config.dimension, config.depth_or_array_size),
        .Flags = D3D12_DSV_FLAG_NONE, // read+write state. additional implementation needed for read only dsv.
      };
      switch (desc.ViewDimension) {
        case D3D12_DSV_DIMENSION_UNKNOWN: {
          assert(false && "invalid dsv view dimension");
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE1D: {
          desc.Texture1D = {
            .MipSlice = 0,
          };
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: {
          desc.Texture1DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
          };
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE2D: {
          desc.Texture2D = {
            .MipSlice = 0,
          };
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: {
          desc.Texture2DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
          };
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE2DMS: {
          desc.Texture2DMS = {};
          break;
        }
        case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: {
          desc.Texture2DMSArray = {
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
          };
          break;
        }
      }
      device->CreateDepthStencilView(resource, &desc, *handle);
      return true;
    }
  }
  logerror("CreateView invalid view type:{} format:{} dimension:{} depth_or_array_size:{}", descriptor_type, config.format, config.dimension, config.depth_or_array_size);
  assert(false && "CreateView invalid view type");
  return false;
}
}
