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
  config->format = j.contains("format") ? GetDxgiFormat(j, "format") : DXGI_FORMAT_R8G8B8A8_UNORM;
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
} // namespace
void ParseRenderGraphJson(const nlohmann::json& j, const uint32_t material_num, StrHash* material_hash_list, RenderGraph* graph) {
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
  if (j.contains("buffer")) {
    auto& buffer_list = j.at("buffer");
    r.buffer_num = static_cast<uint32_t>(buffer_list.size());
    r.buffer_list = AllocateArraySystem<BufferConfig>(r.buffer_num);
    for (uint32_t i = 0; i < r.buffer_num; i++) {
      r.buffer_list[i].buffer_index = i;
      GetBufferConfig(buffer_list[i], &r.buffer_list[i]);
      logdebug("buffer name:{} id:{}", GetStringView(buffer_list[i], "name"), i);
    }
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
      if (src_pass.contains("buffer_list")) {
        auto& buffer_list = src_pass.at("buffer_list");
        dst_pass.buffer_num = static_cast<uint32_t>(buffer_list.size());
        dst_pass.buffer_list = AllocateArraySystem<RenderPassBuffer>(dst_pass.buffer_num);
        const auto& graph_buffer_list = j.at("buffer");
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffer_list[buffer_index];
          auto& src_buffer = buffer_list[buffer_index];
          dst_buffer.state = GetResourceStateType(GetStringView(src_buffer, "state"));
          dst_buffer.index_offset = GetNum(src_buffer, "index_offset", 0);
          dst_pass.max_buffer_index_offset = std::max(dst_buffer.index_offset, dst_pass.max_buffer_index_offset);
          auto buffer_name = GetStringView(src_buffer, "name");
          if (IsSceneBuffer(buffer_name.data())) {
            dst_buffer.buffer_index = EncodeSceneBufferIndex(buffer_name.data());
            continue;
          }
          for (uint32_t graph_buffer_index = 0; graph_buffer_index < r.buffer_num; graph_buffer_index++) {
            if (GetStringView(graph_buffer_list[graph_buffer_index], "name").compare(buffer_name) == 0) {
              dst_buffer.buffer_index = graph_buffer_index;
              r.buffer_list[graph_buffer_index].descriptor_type_flags |= ConvertToDescriptorTypeFlag(dst_buffer.state);
              break;
            }
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
      if (src_pass.contains("material")) {
        const auto hash = CalcEntityStrHash(src_pass, "material");
        dst_pass.material = FindHashIndex(material_num, material_hash_list, hash);
        assert(dst_pass.material < material_num);
      } // material
      dst_pass.execute = GetBool(src_pass, "execute", false);
      if (src_pass.contains("flip_pingpong")) {
        const auto& pingpong = src_pass.at("flip_pingpong");
        dst_pass.flip_pingpong_num = static_cast<uint32_t>(pingpong.size());
        dst_pass.flip_pingpong_index_list = AllocateArraySystem<uint32_t>(dst_pass.flip_pingpong_num);
        auto& buffer_config_list = j.at("buffer");
        for (uint32_t p = 0; p < dst_pass.flip_pingpong_num; p++) {
          const auto str = GetStringView(pingpong[p]);
          for (uint32_t b = 0; b < r.buffer_num; b++) {
            if (str.compare(buffer_config_list[b].at("name")) == 0) {
              dst_pass.flip_pingpong_index_list[p] = b;
              break;
            }
          }
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
  {
    auto& json_buffer_list = j.at("buffer");
    for (uint32_t i = 0; i < r.buffer_num; i++) {
      r.buffer_list[i].flags = GetD3d12ResourceFlags(r.buffer_list[i].descriptor_type_flags);
      SetClearColor(r.buffer_list[i].flags, json_buffer_list[i], &r.buffer_list[i].clear_value);
    }
  }
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
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      const auto& pass = r.render_pass_list[i];
      if (!pass.execute) { continue; }
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
}
}
