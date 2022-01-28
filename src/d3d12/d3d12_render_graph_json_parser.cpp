#include "d3d12_render_graph.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_src_common.h"
namespace illuminate {
uint32_t FindIndex(const nlohmann::json& j, const char* const name, const uint32_t num, StrHash* list) {
  auto hash = CalcEntityStrHash(j, name);
  for (uint32_t i = 0; i < num; i++) {
    if (list[i] == hash) { return i; }
  }
  logwarn("FindIndex: {} not found. {}", name, num);
  return ~0U;
}
D3D12_RESOURCE_STATES GetD3d12ResourceState(const nlohmann::json& j, const char* const entity_name) {
  auto state_str = j.at(entity_name).get<std::string_view>();
  D3D12_RESOURCE_STATES state{};
  bool val_set = false;
  if (state_str.compare("present") == 0) {
    state |= D3D12_RESOURCE_STATE_PRESENT;
    val_set = true;
  }
  if (state_str.compare("rtv") == 0) {
    state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    val_set = true;
  }
  if (state_str.compare("uav") == 0) {
    state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    val_set = true;
  }
  if (state_str.compare("copy_source") == 0) {
    state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    val_set = true;
  }
  if (state_str.compare("copy_dest") == 0) {
    state |= D3D12_RESOURCE_STATE_COPY_DEST;
    val_set = true;
  }
  if (state_str.compare("srv_ps") == 0) {
    state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    val_set = true;
  }
  if (!val_set) {
    logerror("invalid resource state specified. {}", state_str.data());
  }
  assert(val_set && "invalid resource state specified");
  return state;
}
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j, const char* const entity_name) {
  if (!j.contains(entity_name)) {
    logwarn("entity not found. {}", entity_name);
    return DXGI_FORMAT_UNKNOWN;
  }
  auto format_str = GetStringView(j, entity_name);
  if (format_str.compare("R16G16B16A16_FLOAT") == 0) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  }
  if (format_str.compare("B8G8R8A8_UNORM") == 0) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  }
  if (format_str.compare("R8G8B8A8_UNORM") == 0) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
  logerror("invalid format specified. {}", format_str.data());
  assert(false && "invalid format specified");
  return DXGI_FORMAT_R8G8B8A8_UNORM;
}
DescriptorType GetDescriptorType(const nlohmann::json& j) {
  auto str = j.get<std::string_view>();
  if (str.compare("cbv") == 0) {
    return DescriptorType::kCbv;
  }
  if (str.compare("srv") == 0) {
    return DescriptorType::kSrv;
  }
  if (str.compare("uav") == 0) {
    return DescriptorType::kUav;
  }
  if (str.compare("rtv") == 0) {
    return DescriptorType::kRtv;
  }
  if (str.compare("dsv") == 0) {
    return DescriptorType::kDsv;
  }
  assert(false && "invalid DescriptorType");
  return DescriptorType::kNum;
}
ResourceStateType GetResourceStateType(const nlohmann::json& j) {
  auto str = j.get<std::string_view>();
  if (str.compare("cbv") == 0) {
    return ResourceStateType::kCbv;
  }
  if (str.compare("srv") == 0) {
    return ResourceStateType::kSrv;
  }
  if (str.compare("uav") == 0) {
    return ResourceStateType::kUav;
  }
  if (str.compare("rtv") == 0) {
    return ResourceStateType::kRtv;
  }
  if (str.compare("dsv") == 0) {
    return ResourceStateType::kDsv;
  }
  if (str.compare("copy_source") == 0) {
    return ResourceStateType::kCopySrc;
  }
  if (str.compare("copy_dest") == 0) {
    return ResourceStateType::kCopyDst;
  }
  assert(false && "invalid ResourceStateType");
  return ResourceStateType::kNum;
}
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
  if (str.compare("rowmajor") == 0) {
    return D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  }
  if (str.compare("64kb undefined swizzle") == 0) {
    return D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
  }
  if (str.compare("64kb standard swizzle") == 0) {
    return D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE;
  }
  assert(false && "invalid texture layout");
  return D3D12_TEXTURE_LAYOUT_UNKNOWN;
}
D3D12_RESOURCE_FLAGS GetD3d12ResourceFlag(const nlohmann::json& j) {
  auto str = j.get<std::string_view>();
  if (str.compare("rtv") == 0) {
    return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  if (str.compare("dsv") == 0) {
    return D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  }
  if (str.compare("uav") == 0) {
    return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  if (str.compare("no shader resource") == 0) {
    return D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
  }
  if (str.compare("cross adapter") == 0) {
    return D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
  }
  if (str.compare("simultaneous access") == 0) {
    return D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
  }
  if (str.compare("video decode") == 0) {
    return D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY;
  }
  if (str.compare("video encode") == 0) {
    return D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY;
  }
  assert(false && "invalid resource flag");
  return D3D12_RESOURCE_FLAG_NONE;
}
auto GetBufferSizeRelativeness(const nlohmann::json& j, const char* const name) {
  auto str = GetStringView(j, name);
  if (str.compare("swapchain_relative") == 0) {
    return BufferSizeRelativeness::kSwapchainRelative;
  }
  if (str.compare("primary_relative") == 0) {
    return BufferSizeRelativeness::kPrimaryBufferRelative;
  }
  return BufferSizeRelativeness::kAbsolute;
}
}
void GetBufferConfig(const nlohmann::json& j, BufferConfig* config) {
  config->name = CalcEntityStrHash(j, "name");
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
  {
    auto& flags = j.at("flags");
    auto flag_num = static_cast<uint32_t>(flags.size());
    config->flags = D3D12_RESOURCE_FLAG_NONE;
    for (uint32_t i = 0; i < flag_num; i++) {
      config->flags |= GetD3d12ResourceFlag(flags[i]);
    }
  }
  config->mip_width = GetNum(j, "mip_width", 0);
  config->mip_height = GetNum(j, "mip_height", 0);
  config->mip_depth = GetNum(j, "mip_depth", 0);
  config->initial_state = GetD3d12ResourceState(j, "initial_state");
  config->clear_value.Format = config->format;
  if (config->flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
    config->clear_value.DepthStencil.Depth = GetVal<FLOAT>(j, "clear_depth", 1.0f);
    config->clear_value.DepthStencil.Stencil = GetVal<UINT8>(j, "clear_stencil", 0);
  } else {
    if (j.contains("clear_color")) {
      auto& color = j.at("clear_color");
      for (uint32_t i = 0; i < 4; i++) {
        config->clear_value.Color[i] = color[i];
      }
    } else {
      for (uint32_t i = 0; i < 4; i++) {
        config->clear_value.Color[i] = 0.0f;
      }
    }
  }
}
void GetBarrierList(const nlohmann::json& j, const uint32_t barrier_num, Barrier* barrier_list) {
  for (uint32_t barrier_index = 0; barrier_index < barrier_num; barrier_index++) {
    auto& dst_barrier = barrier_list[barrier_index];
    auto& src_barrier = j[barrier_index];
    dst_barrier.buffer_name = CalcEntityStrHash(src_barrier, "buffer_name");
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
        dst_barrier.state_before = GetD3d12ResourceState(src_barrier, "state_before");
        dst_barrier.state_after  = GetD3d12ResourceState(src_barrier, "state_after");
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
