#include "d3d12_render_graph.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
D3D12_HEAP_TYPE GetHeapType(const nlohmann::json& j, const char* entity_name) {
  if (!j.contains(entity_name)) {
    return D3D12_HEAP_TYPE_DEFAULT;
  }
  auto str = GetStringView(j, entity_name);
  if (str.compare("default") == 0) {
    return D3D12_HEAP_TYPE_DEFAULT;
  }
  if (str.compare("upload") == 0) {
    return D3D12_HEAP_TYPE_UPLOAD;
  }
  if (str.compare("readback") == 0) {
    return D3D12_HEAP_TYPE_READBACK;
  }
  assert(false && "invalid heap type");
  return D3D12_HEAP_TYPE_DEFAULT;
}
D3D12_RESOURCE_DIMENSION GetDimension(const nlohmann::json& j, const char* entity_name) {
  if (!j.contains(entity_name)) {
    return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  }
  auto str = GetStringView(j, entity_name);
  if (str.compare("buffer") == 0) {
    return D3D12_RESOURCE_DIMENSION_BUFFER;
  }
  if (str.compare("texture1d") == 0) {
    return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
  }
  if (str.compare("texture2d") == 0) {
    return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  }
  if (str.compare("texture3d") == 0) {
    return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  }
  assert(false && "invalid resource dimension");
  return D3D12_RESOURCE_DIMENSION_UNKNOWN;
}
D3D12_TEXTURE_LAYOUT GetTextureLayout(const nlohmann::json& j, const char* entity_name) {
  if (!j.contains(entity_name)) {
    return D3D12_TEXTURE_LAYOUT_UNKNOWN;
  }
  auto str = GetStringView(j, entity_name);
  if (str.compare("unknown") == 0) {
    return D3D12_TEXTURE_LAYOUT_UNKNOWN;
  }
  if (str.compare("row_major") == 0) {
    return D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  }
  if (str.compare("64kb_undefined_swizzle") == 0) {
    return D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
  }
  if (str.compare("64kb_standard_swizzle") == 0) {
    return D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE;
  }
  assert(false && "invalid texture layout");
  return D3D12_TEXTURE_LAYOUT_UNKNOWN;
}
auto GetBufferSizeRelativeness(const nlohmann::json& j, const char* const name) {
  auto str = GetStringView(j, name);
  if (str.compare("swapchain_relative") == 0) {
    return BufferSizeRelativeness::kSwapchainRelative;
  }
  if (str.compare("primary_relative") == 0) {
    return BufferSizeRelativeness::kPrimaryBufferRelative;
  }
  if (str.compare("absolute") == 0) {
    return BufferSizeRelativeness::kAbsolute;
  }
  logerror("invalid buffer size type {}", name);
  assert(false && "invalid buffer size type");
  return BufferSizeRelativeness::kAbsolute;
}
}
void GetBufferConfig(const nlohmann::json& j, BufferConfig* config) {
  if (j.contains("descriptor_only") && j.at("descriptor_only") == true) {
    config->descriptor_only = true;
    return;
  }
  config->descriptor_only = false;
  config->heap_type = GetHeapType(j, "heap_type");
  config->dimension = GetDimension(j, "dimension");
  config->size_type = GetBufferSizeRelativeness(j, "size_type");
  config->width = j.at("width");
  config->height = GetFloat(j, "height", 1.0f);
  config->depth_or_array_size = GetVal<uint16_t>(j, "depth_or_array_size", 1);
  config->miplevels = GetVal<uint16_t>(j, "miplevels", 1);
  config->format = GetDxgiFormat(j, "format");
  config->sample_count = GetNum(j, "sample_count", 1);
  config->sample_quality = GetNum(j, "sample_quality", 0);
  config->layout = GetTextureLayout(j, "layout");
  if (config->dimension == D3D12_RESOURCE_DIMENSION_BUFFER && config->layout != D3D12_TEXTURE_LAYOUT_ROW_MAJOR) {
    config->layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  }
  config->flags = D3D12_RESOURCE_FLAG_NONE;
  config->mip_width = GetNum(j, "mip_width", 0);
  config->mip_height = GetNum(j, "mip_height", 0);
  config->mip_depth = GetNum(j, "mip_depth", 0);
  config->initial_state = GetResourceStateType(j, "initial_state");
  config->clear_value.Format = config->format;
  config->pingpong = GetBool(j, "pingpong", false);
}
D3D12_FILTER_TYPE GetFilterType(const nlohmann::json& j, const char* const name) {
  if (!j.contains(name)) { return D3D12_FILTER_TYPE_POINT; }
  auto str = GetStringView(j, name);
  return (strcmp(str.data(), "linear") == 0) ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
}
D3D12_TEXTURE_ADDRESS_MODE GetAddressMode(const nlohmann::json& j) {
  auto str = GetStringView(j).data();
  if (strcmp(str, "wrap") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  }
  if (strcmp(str, "mirror") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
  }
  if (strcmp(str, "clamp") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  }
  if (strcmp(str, "border") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  }
  if (strcmp(str, "mirror_once") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
  }
  logerror("invalid texture address mode:{}", str);
  assert(false && "invalid texture address mode");
  return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}
D3D12_COMPARISON_FUNC GetComparisonFunc(const nlohmann::json& j, const char* const name) {
  if (!j.contains(name)) { return D3D12_COMPARISON_FUNC_NEVER; }
  auto str = GetStringView(j, name).data();
  if (strcmp(str, "never") == 0) {
    return D3D12_COMPARISON_FUNC_NEVER;
  }
  if (strcmp(str, "less") == 0) {
    return D3D12_COMPARISON_FUNC_LESS;
  }
  if (strcmp(str, "equal") == 0) {
    return D3D12_COMPARISON_FUNC_EQUAL;
  }
  if (strcmp(str, "less_equal") == 0) {
    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
  }
  if (strcmp(str, "greater") == 0) {
    return D3D12_COMPARISON_FUNC_GREATER;
  }
  if (strcmp(str, "not_equal") == 0) {
    return D3D12_COMPARISON_FUNC_NOT_EQUAL;
  }
  if (strcmp(str, "greater_equal") == 0) {
    return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
  }
  if (strcmp(str, "always") == 0) {
    return D3D12_COMPARISON_FUNC_ALWAYS;
  }
  logerror("invalid comparison func:{}", str);
  assert(false && "invalid comparison func");
  return D3D12_COMPARISON_FUNC_NEVER;
}
void GetSamplerConfig(const nlohmann::json& j, D3D12_SAMPLER_DESC* sampler_desc) {
  {
    auto min_filter = GetFilterType(j, "filter_min");
    auto mag_filter = GetFilterType(j, "filter_mag");
    auto mip_filter = GetFilterType(j, "filter_mip");
    sampler_desc->Filter = D3D12_ENCODE_BASIC_FILTER(min_filter, mag_filter, mip_filter, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
  }
  if (j.contains("address_mode")) {
    auto& address_mode = j.at("address_mode");
    auto num = static_cast<uint32_t>(address_mode.size());
    sampler_desc->AddressU = GetAddressMode(address_mode[0]);
    sampler_desc->AddressV = num > 1 ? GetAddressMode(address_mode[1]) : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc->AddressW = num > 2 ? GetAddressMode(address_mode[2]) : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  } else {
    sampler_desc->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  }
  sampler_desc->MipLODBias = GetFloat(j, "mip_lod_bias", 0.0f);
  sampler_desc->MaxAnisotropy = GetNum(j, "max_anisotropy", 16);
  sampler_desc->ComparisonFunc = GetComparisonFunc(j, "comparison_func");
  if (j.contains("border_color")) {
    auto& border_color = j.at("border_color");
    for (uint32_t i = 0; i < 4; i++) {
      sampler_desc->BorderColor[i] = border_color[i];
    }
  } else {
    for (uint32_t i = 0; i < 4; i++) {
      sampler_desc->BorderColor[i] = 0.0f;
    }
  }
  sampler_desc->MinLOD = GetFloat(j, "min_lod", 0.0f);
  sampler_desc->MaxLOD = GetFloat(j, "max_lod", 65535.0f);
}
void GetBarrierList(const nlohmann::json& j, const nlohmann::json& buffer_json, const uint32_t barrier_num, Barrier* barrier_list) {
  for (uint32_t barrier_index = 0; barrier_index < barrier_num; barrier_index++) {
    auto& dst_barrier = barrier_list[barrier_index];
    auto& src_barrier = j[barrier_index];
    {
      auto buffer_name = GetStringView(src_barrier, "buffer_name");
      const auto num = static_cast<uint32_t>(buffer_json.size());
      for (uint32_t i = 0; i < num; i++) {
        if (buffer_name.compare(GetStringView(buffer_json[i], "name")) == 0) {
          dst_barrier.buffer_index = i;
          break;
        }
      }
    }
    {
      auto type_str = GetStringView(src_barrier, "type");
      if (type_str.compare("transition") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      } else if (type_str.compare("aliasing") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
      } else if (type_str.compare("uav") == 0) {
        dst_barrier.type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      } else {
        logerror("invalid barrier type: {} {} {}", type_str.data(), GetStringView(src_barrier, "buffer_name").data(), barrier_index);
        assert(false && "invalid barrier type");
      }
    } // type
    if (src_barrier.contains("split_type")) {
      auto flag_str = GetStringView(src_barrier, "split_type");
      if (flag_str.compare("begin") == 0) {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
      } else if (flag_str.compare("end") == 0) {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
      } else {
        dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      }
    } else {
      dst_barrier.flag = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    } // flag
    switch (dst_barrier.type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        dst_barrier.state_before = D3D12_RESOURCE_STATE_COMMON;
        dst_barrier.state_after  = D3D12_RESOURCE_STATE_COMMON;
        {
          const auto& state_before = src_barrier.at("state_before");
          for (const auto& state : state_before) {
            dst_barrier.state_before |= ConvertToD3d12ResourceState(GetResourceStateType(state));
          }
        }
        {
          const auto& state_after = src_barrier.at("state_after");
          for (const auto& state : state_after) {
            dst_barrier.state_after |= ConvertToD3d12ResourceState(GetResourceStateType(state));
          }
        }
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        assert(false && "aliasing barrier not implemented yet");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        break;
      }
      default: {
        logerror("invalid barrier type. {}", dst_barrier.type);
        assert(false);
        break;
      }
    } // switch
  }
}
}
