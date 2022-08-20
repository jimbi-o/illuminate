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
  if (!j.contains(name)) {
    return BufferSizeRelativeness::kPrimaryBufferRelative;
  }
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
void GetBufferConfig(const nlohmann::json& j, BufferConfig* config) {
  config->pingpong = GetBool(j, "pingpong", false);
  config->frame_buffered = GetBool(j, "frame_buffered", false);
  if (config->pingpong) {
    assert(!config->frame_buffered);
  }
  if (config->frame_buffered) {
    assert(!config->pingpong);
  }
  config->descriptor_type_flags = kDescriptorTypeFlagNone;
  if (j.contains("descriptor_types")) {
    for (auto& d : j.at("descriptor_types")) {
      config->descriptor_type_flags |= ConvertToDescriptorTypeFlag(GetDescriptorType(d));
    }
  }
  config->descriptor_only = GetBool(j, "descriptor_only", false);
  if (config->descriptor_only) { return; }
  config->heap_type = GetHeapType(j, "heap_type");
  config->dimension = GetDimension(j, "dimension");
  config->size_type = GetBufferSizeRelativeness(j, "size_type");
  config->width = GetFloat(j, "width", 1.0f);
  config->height = GetFloat(j, "height", 1.0f);
  config->depth_or_array_size = GetVal<uint16_t>(j, "depth_or_array_size", 1);
  config->miplevels = GetVal<uint16_t>(j, "miplevels", 1);
  config->format = j.contains("format") ? GetDxgiFormat(j, "format") : DXGI_FORMAT_UNKNOWN;
  config->sample_count = GetNum(j, "sample_count", 1);
  config->sample_quality = GetNum(j, "sample_quality", 0);
  config->layout = GetTextureLayout(j, "layout");
  if (config->dimension == D3D12_RESOURCE_DIMENSION_BUFFER && config->layout != D3D12_TEXTURE_LAYOUT_ROW_MAJOR) {
    config->layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  }
  config->mip_width = GetNum(j, "mip_width", 0);
  config->mip_height = GetNum(j, "mip_height", 0);
  config->mip_depth = GetNum(j, "mip_depth", 0);
  config->initial_state = GetResourceStateType(j, "initial_state", ResourceStateType::kCommon);
  config->clear_value.Format = config->format;
  config->num_elements = GetNum(j, "num_elements", 0);
  config->stride_bytes = GetNum(j, "stride_bytes", 0);
  config->raw_buffer = GetBool(j, "raw_buffer", false);
  // clear color/depth/stencil are set later.
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
auto GetDxgiFormatFromMaterial(const RenderPassBuffer* render_graph_buffer_list, const BufferConfig& buffer_config, const uint32_t render_pass_buffer_index, const ResourceStateType state, const DXGI_FORMAT* rtv_format_list, const DXGI_FORMAT dsv_format) {
  if (buffer_config.format != DXGI_FORMAT_UNKNOWN) { return buffer_config.format; }
  switch (state) {
    case ResourceStateType::kRtv: {
      uint32_t rtv_index = 0;
      for (uint32_t i = 0; i < render_pass_buffer_index; i++) {
        if (render_graph_buffer_list[i].state == state) {
          rtv_index++;
        }
      }
      return rtv_format_list[rtv_index];
    }
    case ResourceStateType::kDsvWrite:
    case ResourceStateType::kDsvRead: {
      return dsv_format;
    }
  }
  return DXGI_FORMAT_UNKNOWN;
}
auto FindBufferIndex(const uint32_t buffer_num, const StrHash* name_list, const StrHash& name) {
  for (uint32_t i = 0; i < buffer_num; i++) {
    if (name_list[i] == name) { return i; }
  }
  return buffer_num;
}
auto InitializeBufferConfig(const uint32_t buffer_index, BufferConfig* buffer) {
  buffer->buffer_index          = buffer_index;
  buffer->heap_type             = D3D12_HEAP_TYPE_DEFAULT;
  buffer->dimension             = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  buffer->alignment             = 0;
  buffer->size_type             = BufferSizeRelativeness::kPrimaryBufferRelative;
  buffer->width                 = 1.0f;
  buffer->height                = 1.0f;
  buffer->depth_or_array_size   = 1;
  buffer->miplevels             = 1;
  buffer->format                = DXGI_FORMAT_UNKNOWN;
  buffer->sample_count          = 1;
  buffer->sample_quality        = 0;
  buffer->layout                = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  buffer->mip_width             = 0;
  buffer->mip_height            = 0;
  buffer->mip_depth             = 0;
  buffer->initial_state         = ResourceStateType::kCommon;
  buffer->descriptor_type_flags = kDescriptorTypeFlagNone;
  buffer->descriptor_only       = false;
  buffer->pingpong              = false;
  buffer->frame_buffered        = false;
  buffer->num_elements          = 0;
  buffer->stride_bytes          = 0;
  buffer->raw_buffer            = false;
}
} // namespace
std::pair<char**, StrHash*> ParseRenderGraphJson(const nlohmann::json& j, const uint32_t material_num, StrHash* material_hash_list, const DXGI_FORMAT* const * rtv_format_list, const DXGI_FORMAT* dsv_format, RenderGraphConfig* graph) {
  auto& r = *graph;
  j.at("frame_buffer_num").get_to(r.frame_buffer_num);
  r.primarybuffer_width = GetNum(j, "primarybuffer_width", 1);
  r.primarybuffer_height = GetNum(j, "primarybuffer_height", 1);
  r.primarybuffer_format = GetDxgiFormat(j, "primarybuffer_format");
  {
    auto& window = j.at("window");
    auto window_title = GetStringView(window, "title");
    auto window_title_len = static_cast<uint32_t>(window_title.size()) + 1;
    r.window_title = AllocateArraySystem<char>(window_title_len);
    strcpy_s(r.window_title, window_title_len, window_title.data());
    window.at("width").get_to(r.window_width);
    window.at("height").get_to(r.window_height);
  }
  {
    auto& command_queues = j.at("command_queue");
    r.command_queue_num = static_cast<uint32_t>(command_queues.size());
    r.command_queue_name = AllocateArraySystem<StrHash>(r.command_queue_num);
    r.command_queue_type = AllocateArraySystem<D3D12_COMMAND_LIST_TYPE>(r.command_queue_num);
    r.command_queue_priority = AllocateArraySystem<D3D12_COMMAND_QUEUE_PRIORITY>(r.command_queue_num);
    r.command_list_num_per_queue = AllocateArraySystem<uint32_t>(r.command_queue_num);
    for (uint32_t i = 0; i < r.command_queue_num; i++) {
      r.command_queue_name[i] = CalcEntityStrHash(command_queues[i], "name");
      auto command_queue_type_str = GetStringView(command_queues[i], "type");
      if (command_queue_type_str.compare("compute") == 0) {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COMPUTE;
      } else if (command_queue_type_str.compare("copy") == 0) {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COPY;
      } else {
        r.command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_DIRECT;
      }
      auto command_queue_priority_str = GetStringView(command_queues[i], "priority");
      if (command_queue_priority_str.compare("high") == 0) {
        r.command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
      } else if (command_queue_priority_str.compare("global realtime") == 0) {
        r.command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME;
      } else {
        r.command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
      }
      r.command_list_num_per_queue[i] = command_queues[i].at("command_list_num");
    }
  }
  {
    auto& swapchain = j.at("swapchain");
    r.swapchain_command_queue_index = FindIndex(swapchain, "command_queue", r.command_queue_num, r.command_queue_name);
    r.swapchain_format = GetDxgiFormat(swapchain, "format");
    if (swapchain.contains("usage")) {
      auto usage_list = swapchain.at("usage");
      for (auto& usage : usage_list) {
        auto usage_str = GetStringView(usage);
        if (usage_str.compare("READ_ONLY") == 0) {
          r.swapchain_usage = DXGI_USAGE_READ_ONLY;
        }
        if (usage_str.compare("RENDER_TARGET_OUTPUT") == 0) {
          r.swapchain_usage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
        }
        if (usage_str.compare("SHADER_INPUT") == 0) {
          r.swapchain_usage |= DXGI_USAGE_SHADER_INPUT;
        }
        if (usage_str.compare("SHARED") == 0) {
          r.swapchain_usage |= DXGI_USAGE_SHARED;
        }
        if (usage_str.compare("UNORDERED_ACCESS") == 0) {
          r.swapchain_usage |= DXGI_USAGE_UNORDERED_ACCESS;
        }
      }
    }
  }
  const uint32_t default_buffer_num = 16;
  auto buffer_name_list_len = default_buffer_num;
  auto buffer_name_hash_list = AllocateArrayFrame<StrHash>(buffer_name_list_len);
  auto buffer_name_list = AllocateArrayFrame<char*>(buffer_name_list_len);
  uint32_t next_vacant_buffer_index = 0;
  auto buffer_config_list = AllocateArrayFrame<BufferConfig>(buffer_name_list_len);
  if (j.contains("buffer")) {
    auto& buffer_list = j.at("buffer");
    const auto buffer_num = GetUint32(buffer_list.size());
    next_vacant_buffer_index = buffer_num;
    for (uint32_t i = 0; i < buffer_num; i++) {
      buffer_config_list[i].buffer_index = i;
      GetBufferConfig(buffer_list[i], &buffer_config_list[i]);
      auto str = GetStringView(buffer_list[i], "name");
      const auto namelen = GetUint32(str.size()) + 1;
      buffer_name_list[i] = AllocateArrayFrame<char>(namelen);
      strncpy_s(buffer_name_list[i], namelen, str.data(), namelen);
      buffer_name_hash_list[i] = CalcStrHash(buffer_name_list[i]);
    }
  }
  {
    // add swapchain buffer
    const auto swapchain_index = next_vacant_buffer_index;
    next_vacant_buffer_index++;
    buffer_config_list[swapchain_index].buffer_index = swapchain_index;
    buffer_config_list[swapchain_index].descriptor_only = true;
    const char name[] = "swapchain";
    const auto namelen = GetUint32(strlen(name)) + 1;
    buffer_name_list[swapchain_index] = AllocateArrayFrame<char>(namelen);
    strncpy_s(buffer_name_list[swapchain_index], namelen, name, namelen);
    buffer_name_hash_list[swapchain_index] = CalcStrHash(name);
  }
  if (j.contains("sampler")) {
    auto& sampler_list = j.at("sampler");
    r.sampler_num = static_cast<uint32_t>(sampler_list.size());
    r.sampler_list = AllocateArraySystem<D3D12_SAMPLER_DESC>(r.sampler_num);
    for (uint32_t i = 0; i < r.sampler_num; i++) {
      GetSamplerConfig(sampler_list[i], &r.sampler_list[i]);
    }
  } // sampler
  { // pass_list
    auto& render_pass_list = j.at("render_pass");
    r.render_pass_num = static_cast<uint32_t>(render_pass_list.size());
    r.render_pass_list = AllocateArraySystem<RenderPass>(r.render_pass_num);
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      dst_pass.name = CalcEntityStrHash(src_pass, "name");
      dst_pass.type = CalcEntityStrHash(src_pass, "type");
      dst_pass.enabled = GetBool(src_pass, "enabled", true);
      dst_pass.index = i;
      dst_pass.command_queue_index = FindIndex(src_pass, "command_queue", r.command_queue_num, r.command_queue_name);
      if (src_pass.contains("material")) {
        const auto hash = CalcEntityStrHash(src_pass, "material");
        dst_pass.material = FindHashIndex(material_num, material_hash_list, hash);
        assert(dst_pass.material < material_num);
      } // material
      if (src_pass.contains("buffer_list")) {
        auto& buffer_list = src_pass.at("buffer_list");
        dst_pass.buffer_num = static_cast<uint32_t>(buffer_list.size());
        dst_pass.buffer_list = AllocateArraySystem<RenderPassBuffer>(dst_pass.buffer_num);
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffer_list[buffer_index];
          auto& src_buffer = buffer_list[buffer_index];
          dst_buffer.state = GetResourceStateType(GetStringView(src_buffer, "state"));
          dst_buffer.index_offset = GetNum(src_buffer, "index_offset", 0);
          dst_pass.max_buffer_index_offset = std::max(dst_buffer.index_offset, dst_pass.max_buffer_index_offset);
          auto buffer_name = GetStringView(src_buffer, "name");
          auto buffer_name_hash = CalcStrHash(buffer_name.data());
          if (IsSceneBufferName(buffer_name_hash)) {
            dst_buffer.buffer_index = EncodeSceneBufferIndex(buffer_name_hash);
            continue;
          }
          auto graph_buffer_index = FindBufferIndex(next_vacant_buffer_index, buffer_name_hash_list, buffer_name_hash);
          if (graph_buffer_index >= next_vacant_buffer_index) {
            if (next_vacant_buffer_index >= buffer_name_list_len) {
              const auto prev_buffer_name_list_len = buffer_name_list_len;
              buffer_name_list_len *= 2;
              auto tmp = buffer_name_hash_list;
              buffer_name_hash_list = AllocateArrayFrame<StrHash>(buffer_name_list_len);
              memcpy(buffer_name_hash_list, tmp, sizeof(StrHash) * prev_buffer_name_list_len);
              auto tmp2 = buffer_name_list;
              buffer_name_list = AllocateArrayFrame<char*>(buffer_name_list_len);
              memcpy(buffer_name_list, tmp2, sizeof(char*) * prev_buffer_name_list_len);
            }
            graph_buffer_index = next_vacant_buffer_index;
            next_vacant_buffer_index++;
            buffer_name_hash_list[graph_buffer_index] = buffer_name_hash;
            {
              const auto namelen = GetUint32(buffer_name.size()) + 1;
              buffer_name_list[graph_buffer_index] = AllocateArrayFrame<char>(namelen);
              strncpy_s(buffer_name_list[graph_buffer_index], namelen, buffer_name.data(), namelen);
            }
            InitializeBufferConfig(graph_buffer_index, &buffer_config_list[graph_buffer_index]);
            buffer_config_list[graph_buffer_index].initial_state = dst_buffer.state;
          }
          dst_buffer.buffer_index = graph_buffer_index;
          buffer_config_list[graph_buffer_index].descriptor_type_flags |= ConvertToDescriptorTypeFlag(dst_buffer.state);
          auto format = GetDxgiFormatFromMaterial(dst_pass.buffer_list, buffer_config_list[graph_buffer_index], buffer_index, dst_buffer.state, rtv_format_list[dst_pass.material], dsv_format[dst_pass.material]);
          if (format != buffer_config_list[graph_buffer_index].format && format != DXGI_FORMAT_UNKNOWN) {
            buffer_config_list[graph_buffer_index].format = format;
            buffer_config_list[graph_buffer_index].clear_value.Format = format;
          }
        }
      } // buffer_list
      if (src_pass.contains("sampler")) {
        auto& sampler = src_pass.at("sampler");
        dst_pass.sampler_num = static_cast<uint32_t>(sampler.size());
        dst_pass.sampler_index_list = AllocateArraySystem<uint32_t>(dst_pass.sampler_num);
        const auto& graph_sampler_list = j.at("sampler");
        for (uint32_t s = 0; s < dst_pass.sampler_num; s++) {
          auto sampler_name = GetStringView(sampler[s]).data();
          if (strcmp(sampler_name, kSceneSamplerName) == 0) {
            dst_pass.sampler_index_list[s] = kSceneSamplerId;
            continue;
          }
          for (uint32_t graph_s = 0; graph_s < r.sampler_num; graph_s++) {
            if (GetStringView(graph_sampler_list[graph_s], "name").compare(sampler_name) == 0) {
              dst_pass.sampler_index_list[s] = graph_s;
              break;
            }
          }
        }
      } // sampler
      if (src_pass.contains("flip_pingpong")) {
        const auto& pingpong = src_pass.at("flip_pingpong");
        dst_pass.flip_pingpong_num = static_cast<uint32_t>(pingpong.size());
        dst_pass.flip_pingpong_index_list = AllocateArraySystem<uint32_t>(dst_pass.flip_pingpong_num);
        for (uint32_t p = 0; p < dst_pass.flip_pingpong_num; p++) {
          const auto strhash = CalcEntityStrHash(pingpong[p]);
          const auto pingpong_buffer_index = FindBufferIndex(buffer_name_list_len, buffer_name_hash_list, strhash);
          dst_pass.flip_pingpong_index_list[p] = pingpong_buffer_index;
          buffer_config_list[pingpong_buffer_index].pingpong = true;
        }
      } // pingpong
    } // pass
    for (uint32_t i = 0; i < r.render_pass_num; i++) { // wait_pass
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      if (src_pass.contains("wait_pass")) {
        auto& wait_pass = src_pass.at("wait_pass");
        dst_pass.wait_pass_num = static_cast<uint32_t>(wait_pass.size());
        dst_pass.signal_queue_index = AllocateArraySystem<uint32_t>(dst_pass.wait_pass_num);
        dst_pass.signal_pass_index = AllocateArraySystem<uint32_t>(dst_pass.wait_pass_num);
        for (uint32_t p = 0; p < dst_pass.wait_pass_num; p++) {
          const auto str = GetStringView(wait_pass[p]).data();
          const auto hash = CalcStrHash(str);
          for (uint32_t graph_index = 0; graph_index < r.render_pass_num; graph_index++) {
            if (dst_pass.index == graph_index) { continue; }
            if (r.render_pass_list[graph_index].name == hash) {
              dst_pass.signal_pass_index[p] = graph_index;
              break;
            }
          }
        }
      }
    } // wait_pass
  } // pass_list
  // copy buffer list
  r.buffer_num = next_vacant_buffer_index;
  r.buffer_list = AllocateArraySystem<BufferConfig>(r.buffer_num);
  for (uint32_t i = 0; i < r.buffer_num; i++) {
    memcpy(&r.buffer_list[i], &buffer_config_list[i], sizeof(r.buffer_list[i]));
    auto& config = r.buffer_list[i];
    if (config.descriptor_type_flags & kDescriptorTypeFlagDsv) {
      config.clear_value.DepthStencil.Depth = 1.0f;
      config.clear_value.DepthStencil.Stencil = 0;
    }
    if (config.descriptor_type_flags & kDescriptorTypeFlagCbv) {
      config.frame_buffered = true;
      config.format = DXGI_FORMAT_UNKNOWN;
      config.layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      config.heap_type = D3D12_HEAP_TYPE_UPLOAD;
      config.dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      config.size_type = BufferSizeRelativeness::kAbsolute;
      config.initial_state = ResourceStateType::kGenericRead;
    }
  }
  // signals
  for (uint32_t i = 0; i < r.render_pass_num; i++) {
    for (uint32_t w = 0; w < r.render_pass_list[i].wait_pass_num; w++) {
      for (uint32_t k = 0; k < r.render_pass_num; k++) {
        if (i == k) { continue; }
        if (r.render_pass_list[i].signal_pass_index[w] == k) {
          r.render_pass_list[k].sends_signal = true;
          r.render_pass_list[i].signal_queue_index[w] = r.render_pass_list[k].command_queue_index;
          break;
        }
      }
    }
  }
  // descriptor num
  {
    const auto cbv_index = static_cast<uint32_t>(DescriptorType::kCbv);
    const auto srv_index = static_cast<uint32_t>(DescriptorType::kSrv);
    const auto uav_index = static_cast<uint32_t>(DescriptorType::kUav);
    const auto rtv_index = static_cast<uint32_t>(DescriptorType::kRtv);
    const auto dsv_index = static_cast<uint32_t>(DescriptorType::kDsv);
    for (uint32_t i = 0; i < r.buffer_num; i++) {
      const auto add_val = GetBufferAllocationNum(r.buffer_list[i], r.frame_buffer_num);
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagCbv) {
        r.descriptor_handle_num_per_type[cbv_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagSrv) {
        r.descriptor_handle_num_per_type[srv_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagUav) {
        r.descriptor_handle_num_per_type[uav_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagRtv) {
        r.descriptor_handle_num_per_type[rtv_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagDsv) {
        r.descriptor_handle_num_per_type[dsv_index] += add_val;
      }
    }
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSampler)] = r.sampler_num;
  } // descriptor num
  // command allocator num
  {
    const auto index_direct = GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_DIRECT);
    const auto index_compute = GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    const auto index_copy = GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COPY);
    // +1 for last pass execution
    r.command_allocator_num_per_queue_type[index_direct] = 1;
    r.command_allocator_num_per_queue_type[index_compute] = 1;
    r.command_allocator_num_per_queue_type[index_copy] = 1;
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      const auto& pass = r.render_pass_list[i];
      if (!pass.sends_signal) { continue; }
      switch (r.command_queue_type[pass.command_queue_index]) {
        case D3D12_COMMAND_LIST_TYPE_DIRECT: {
          r.command_allocator_num_per_queue_type[index_direct]++;
          break;
        }
        case D3D12_COMMAND_LIST_TYPE_COMPUTE: {
          r.command_allocator_num_per_queue_type[index_compute]++;
          break;
        }
        case D3D12_COMMAND_LIST_TYPE_COPY: {
          r.command_allocator_num_per_queue_type[index_copy]++;
          break;
        }
      }
    }
  } // command allocator num
  //
  r.gpu_handle_num_view = GetNum(j, "gpu_handle_num_view", r.gpu_handle_num_view);
  r.gpu_handle_num_sampler = GetNum(j, "gpu_handle_num_sampler", r.gpu_handle_num_sampler);
  r.max_model_num = GetNum(j, "max_model_num", r.max_model_num);
  r.max_material_num = GetNum(j, "max_material_num", r.max_material_num);
  r.max_mipmap_num = GetNum(j, "max_mipmap_num", r.max_mipmap_num);
  r.timestamp_query_dst_resource_num = GetNum(j, "timestamp_query_dst_resource_num", r.timestamp_query_dst_resource_num);
  return std::make_pair(buffer_name_list, buffer_name_hash_list);
}
}
