#ifndef ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#define ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#include "d3d12_json_parser.h"
#include "d3d12_render_graph.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
#include <nlohmann/json.hpp>
namespace illuminate {
void GetBufferConfig(const nlohmann::json& j, BufferConfig* buffer_config);
void GetSamplerConfig(const nlohmann::json& j, D3D12_SAMPLER_DESC* sampler_desc);
void GetBarrierList(const nlohmann::json& j, const nlohmann::json& buffer_json, const uint32_t barrier_num, Barrier* barrier_list);
void ConfigureBarrierTransition(const nlohmann::json& json_render_pass_list, const char* const barrier_entity_name, const uint32_t barrier_num, Barrier* barrier_list, const BufferConfig* buffer_config_list, D3D12_RESOURCE_STATES** buffer_state, bool* write_to_sub);
template <typename A>
void ParseRenderGraphJson(const nlohmann::json& j, A* allocator, RenderGraph* graph) {
  auto& r = *graph;
  j.at("frame_buffer_num").get_to(r.frame_buffer_num);
  j.at("frame_loop_num").get_to(r.frame_loop_num);
  r.primarybuffer_width = GetNum(j, "primarybuffer_width", 1);
  r.primarybuffer_height = GetNum(j, "primarybuffer_height", 1);
  r.primarybuffer_format = GetDxgiFormat(j, "primarybuffer_format");
  {
    auto& window = j.at("window");
    auto window_title = GetStringView(window, "title");
    auto window_title_len = static_cast<uint32_t>(window_title.size()) + 1;
    r.window_title = AllocateArray<char>(allocator, window_title_len);
    strcpy_s(r.window_title, window_title_len, window_title.data());
    window.at("width").get_to(r.window_width);
    window.at("height").get_to(r.window_height);
  }
  {
    auto& command_queues = j.at("command_queue");
    r.command_queue_num = static_cast<uint32_t>(command_queues.size());
    r.command_queue_name = AllocateArray<StrHash>(allocator, r.command_queue_num);
    r.command_queue_type = AllocateArray<D3D12_COMMAND_LIST_TYPE>(allocator, r.command_queue_num);
    r.command_queue_priority = AllocateArray<D3D12_COMMAND_QUEUE_PRIORITY>(allocator, r.command_queue_num);
    r.command_list_num_per_queue = AllocateArray<uint32_t>(allocator, r.command_queue_num);
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
  if (j.contains("buffer")) {
    auto& buffer_list = j.at("buffer");
    r.buffer_num = static_cast<uint32_t>(buffer_list.size());
    r.buffer_list = AllocateArray<BufferConfig>(allocator, r.buffer_num);
    for (uint32_t i = 0; i < r.buffer_num; i++) {
      r.buffer_list[i].buffer_index = i;
      GetBufferConfig(buffer_list[i], &r.buffer_list[i]);
    }
  }
  if (j.contains("sampler")) {
    auto& sampler_list = j.at("sampler");
    r.sampler_num = static_cast<uint32_t>(sampler_list.size());
    r.sampler_list = AllocateArray<D3D12_SAMPLER_DESC>(allocator, r.sampler_num);
    for (uint32_t i = 0; i < r.sampler_num; i++) {
      GetSamplerConfig(sampler_list[i], &r.sampler_list[i]);
    }
  } // sampler
  { // pass_list
    auto& render_pass_list = j.at("render_pass");
    r.render_pass_num = static_cast<uint32_t>(render_pass_list.size());
    r.render_pass_list = AllocateArray<RenderPass>(allocator, r.render_pass_num);
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      dst_pass.name = CalcEntityStrHash(src_pass, "name");
      dst_pass.index = i;
      dst_pass.command_queue_index = FindIndex(src_pass, "command_queue", r.command_queue_num, r.command_queue_name);
      if (src_pass.contains("buffer_list")) {
        auto& buffer_list = src_pass.at("buffer_list");
        dst_pass.buffer_num = static_cast<uint32_t>(buffer_list.size());
        dst_pass.buffer_list = AllocateArray<RenderPassBuffer>(allocator, dst_pass.buffer_num);
        const auto& graph_buffer_list = j.at("buffer");
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffer_list[buffer_index];
          auto& src_buffer = buffer_list[buffer_index];
          dst_buffer.state = GetResourceStateType(GetStringView(src_buffer, "state"));
          auto buffer_name = GetStringView(src_buffer, "name");
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
        dst_pass.sampler_index_list = AllocateArray<uint32_t>(allocator, dst_pass.sampler_num);
        const auto& graph_sampler_list = j.at("sampler");
        for (uint32_t s = 0; s < dst_pass.sampler_num; s++) {
          auto sampler_name = GetStringView(sampler[s]).data();
          for (uint32_t graph_s = 0; graph_s < r.sampler_num; graph_s++) {
            if (GetStringView(graph_sampler_list[graph_s], "name").compare(sampler_name) == 0) {
              dst_pass.sampler_index_list[s] = graph_s;
              break;
            }
          }
        }
      } // sampler
      if (src_pass.contains("prepass_barrier")) {
        auto& prepass_barrier = src_pass.at("prepass_barrier");
        dst_pass.prepass_barrier_num = static_cast<uint32_t>(prepass_barrier.size());
        dst_pass.prepass_barrier = AllocateArray<Barrier>(allocator, dst_pass.prepass_barrier_num);
        GetBarrierList(prepass_barrier, j.at("buffer"), dst_pass.prepass_barrier_num, dst_pass.prepass_barrier);
      }
      if (src_pass.contains("postpass_barrier")) {
        auto& postpass_barrier = src_pass.at("postpass_barrier");
        dst_pass.postpass_barrier_num = static_cast<uint32_t>(postpass_barrier.size());
        dst_pass.postpass_barrier = AllocateArray<Barrier>(allocator, dst_pass.postpass_barrier_num);
        GetBarrierList(postpass_barrier, j.at("buffer"), dst_pass.postpass_barrier_num, dst_pass.postpass_barrier);
      } // barriers
      dst_pass.execute = GetBool(src_pass, "execute", false);
      if (src_pass.contains("flip_pingpong")) {
        const auto& pingpong = src_pass.at("flip_pingpong");
        dst_pass.flip_pingpong_num = static_cast<uint32_t>(pingpong.size());
        dst_pass.flip_pingpong_index_list = AllocateArray<uint32_t>(allocator, dst_pass.flip_pingpong_num);
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
        dst_pass.signal_queue_index = AllocateArray<uint32_t>(allocator, dst_pass.wait_pass_num);
        dst_pass.signal_pass_index = AllocateArray<uint32_t>(allocator, dst_pass.wait_pass_num);
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
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagCbv) {
        r.descriptor_handle_num_per_type[cbv_index]++;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagSrv) {
        const uint32_t add_val = r.buffer_list[i].pingpong ? 2 : 1;
        r.descriptor_handle_num_per_type[srv_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagUav) {
        r.descriptor_handle_num_per_type[uav_index]++;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagRtv) {
        const uint32_t add_val = r.buffer_list[i].pingpong ? 2 : 1;
        r.descriptor_handle_num_per_type[rtv_index] += add_val;
      }
      if (r.buffer_list[i].descriptor_type_flags & kDescriptorTypeFlagDsv) {
        const uint32_t add_val = r.buffer_list[i].pingpong ? 2 : 1;
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
  {
    r.gpu_handle_num_view = GetNum(j, "gpu_handle_num_view", 1024);
    r.gpu_handle_num_sampler = GetNum(j, "gpu_handle_num_sampler", 16);
  }
  // barrier transition
  {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto buffer_state = AllocateArray<D3D12_RESOURCE_STATES*>(&tmp_allocator, r.buffer_num);
    auto write_to_sub = AllocateArray<bool>(&tmp_allocator, r.buffer_num);
    for (uint32_t i = 0; i < r.buffer_num; i++) {
      buffer_state[i] = AllocateArray<D3D12_RESOURCE_STATES>(&tmp_allocator, 2);
      buffer_state[i][0] = ConvertToD3d12ResourceState(r.buffer_list[i].initial_state);
      buffer_state[i][1] = buffer_state[i][0];
      write_to_sub[i] = 0;
    }
    const auto& json_render_pass_list = j.at("render_pass");
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      ConfigureBarrierTransition(json_render_pass_list[i], "prepass_barrier", r.render_pass_list[i].prepass_barrier_num, r.render_pass_list[i].prepass_barrier, r.buffer_list, buffer_state, write_to_sub);
      ConfigureBarrierTransition(json_render_pass_list[i], "postpass_barrier", r.render_pass_list[i].postpass_barrier_num, r.render_pass_list[i].postpass_barrier, r.buffer_list, buffer_state, write_to_sub);
      for (uint32_t f = 0; f < r.render_pass_list[i].flip_pingpong_num; f++) {
        const auto& buffer_index = r.render_pass_list[i].flip_pingpong_index_list[f];
        write_to_sub[buffer_index] = !write_to_sub[buffer_index];
      }
    }
  } // barrier transition
  // do not allocate from allocator below this line.
}
}
#endif
