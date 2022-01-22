#ifndef ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#define ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#include "d3d12_render_graph.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
#include <nlohmann/json.hpp>
namespace illuminate {
using RenderPassVarParseFunction = void (*)(const nlohmann::json&, void*);
inline auto GetStringView(const nlohmann::json& j, const char* const name) {
  return j.at(name).get<std::string_view>();
}
inline auto CalcEntityStrHash(const nlohmann::json& j, const char* const name) {
  return CalcStrHash(GetStringView(j, name).data());
}
inline auto GetNum(const nlohmann::json& j, const char* const name, const uint32_t default_val) {
  return j.contains(name) ? j.at(name).get<uint32_t>() : default_val;
}
inline auto GetFloat(const nlohmann::json& j, const char* const name, const float default_val) {
  return j.contains(name) ? j.at(name).get<float>() : default_val;
}
inline auto GetBool(const nlohmann::json& j, const char* const name, const bool default_val) {
  return j.contains(name) ? j.at(name).get<bool>() : default_val;
}
template <typename T>
inline auto GetVal(const nlohmann::json& j, const char* const name, const T& default_val) {
  return j.contains(name) ? j.at(name).get<T>() : default_val;
}
uint32_t FindIndex(const nlohmann::json& j, const char* const name, const uint32_t num, StrHash* list);
D3D12_RESOURCE_STATES GetD3d12ResourceState(const nlohmann::json& j, const char* const entity_name);
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j, const char* const entity_name);
void GetBufferConfig(const nlohmann::json& j, BufferConfig* buffer_config);
void GetBarrierList(const nlohmann::json& j, const uint32_t barrier_num, Barrier* barrier_list);
DescriptorType GetDescriptorType(const nlohmann::json& j);
ResourceStateType GetResourceStateType(const nlohmann::json& j);
template <typename A1, typename A2, typename A3>
void ParseRenderGraphJson(const nlohmann::json& j, const HashMap<uint32_t, A1>& pass_var_size, const HashMap<RenderPassVarParseFunction, A2>& pass_var_func, A3* allocator, RenderGraph* graph) {
  auto& r = *graph;
  j.at("frame_buffer_num").get_to(r.frame_buffer_num);
  j.at("frame_loop_num").get_to(r.frame_loop_num);
  r.primarybuffer_width = GetNum(j, "primarybuffer_width", 1);
  r.primarybuffer_height = GetNum(j, "primarybuffer_height", 1);
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
    auto& allocators = j.at("command_allocator");
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_DIRECT)] = GetNum(allocators, "direct", 0);
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COMPUTE)] = GetNum(allocators, "compute", 0);
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COPY)] = GetNum(allocators, "copy", 0);
  }
  {
    auto& swapchain = j.at("swapchain");
    r.swapchain_command_queue_index = FindIndex(swapchain, "command_queue", r.command_queue_num, r.command_queue_name);
    r.swapchain_format = GetDxgiFormat(swapchain, "format");
    auto usage_list = swapchain.at("usage");
    for (auto& usage : usage_list) {
      auto usage_str = usage.get<std::string_view>();
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
      GetBufferConfig(buffer_list[i], &r.buffer_list[i]);
      auto& descriptor_type_list = buffer_list[i].at("descriptor_type");
      r.buffer_list[i].descriptor_type_num = static_cast<uint32_t>(descriptor_type_list.size());
      r.buffer_list[i].descriptor_type = AllocateArray<DescriptorType>(allocator, r.buffer_list[i].descriptor_type_num);
      for (uint32_t d = 0; d < r.buffer_list[i].descriptor_type_num; d++) {
        r.buffer_list[i].descriptor_type[d] = GetDescriptorType(descriptor_type_list[d]);
      }
    }
  }
  {
    auto& render_pass_list = j.at("render_pass");
    r.render_pass_num = static_cast<uint32_t>(render_pass_list.size());
    r.render_pass_list = AllocateArray<RenderPass>(allocator, r.render_pass_num);
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      dst_pass.name = CalcEntityStrHash(src_pass, "name");
      dst_pass.command_queue_index = FindIndex(src_pass, "command_queue", r.command_queue_num, r.command_queue_name);
      if (src_pass.contains("buffer_list")) {
        auto& buffer_list = src_pass.at("buffer_list");
        dst_pass.buffer_num = static_cast<uint32_t>(buffer_list.size());
        dst_pass.buffer_list = AllocateArray<RenderPassBuffer>(allocator, dst_pass.buffer_num);
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffer_list[buffer_index];
          auto& src_buffer = buffer_list[buffer_index];
          dst_buffer.buffer_name = CalcEntityStrHash(src_buffer, "name");
          dst_buffer.state = GetResourceStateType(GetStringView(src_buffer, "state"));
        }
      } // buffer_list
      if (pass_var_func.Get(dst_pass.name) != nullptr && src_pass.contains("pass_vars")) {
        dst_pass.pass_vars = allocator->Allocate(*pass_var_size.Get(dst_pass.name));
        (**pass_var_func.Get(dst_pass.name))(src_pass.at("pass_vars"), dst_pass.pass_vars);
      }
      if (src_pass.contains("prepass_barrier")) {
        auto& prepass_barrier = src_pass.at("prepass_barrier");
        dst_pass.prepass_barrier_num = static_cast<uint32_t>(prepass_barrier.size());
        dst_pass.prepass_barrier = AllocateArray<Barrier>(allocator, dst_pass.prepass_barrier_num);
        GetBarrierList(prepass_barrier, dst_pass.prepass_barrier_num, dst_pass.prepass_barrier);
      }
      if (src_pass.contains("postpass_barrier")) {
        auto& postpass_barrier = src_pass.at("postpass_barrier");
        dst_pass.postpass_barrier_num = static_cast<uint32_t>(postpass_barrier.size());
        dst_pass.postpass_barrier = AllocateArray<Barrier>(allocator, dst_pass.postpass_barrier_num);
        GetBarrierList(postpass_barrier, dst_pass.postpass_barrier_num, dst_pass.postpass_barrier);
      } // barriers
      dst_pass.execute = GetBool(src_pass, "execute", false);
      if (src_pass.contains("wait_pass")) {
        auto& wait_pass = src_pass.at("wait_pass");
        dst_pass.wait_pass_num = static_cast<uint32_t>(wait_pass.size());
        dst_pass.signal_queue_index = AllocateArray<uint32_t>(allocator, dst_pass.wait_pass_num);
        dst_pass.signal_pass_name = AllocateArray<StrHash>(allocator, dst_pass.wait_pass_num);
        for (uint32_t p = 0; p < dst_pass.wait_pass_num; p++) {
          dst_pass.signal_pass_name[p] = CalcStrHash(wait_pass[p].get<std::string_view>().data());
        }
      }
    } // pass
  } // pass_list
  for (uint32_t i = 0; i < r.render_pass_num; i++) {
    for (uint32_t w = 0; w < r.render_pass_list[i].wait_pass_num; w++) {
      for (uint32_t k = 0; k < r.render_pass_num; k++) {
        if (i == k) { continue; }
        if (r.render_pass_list[i].signal_pass_name[w] == r.render_pass_list[k].name) {
          r.render_pass_list[i].signal_queue_index[w] = r.render_pass_list[k].command_queue_index;
          break;
        }
      }
    }
  }
  if (j.contains("descriptor_handle_num_per_type")) {
    auto& list = j.at("descriptor_handle_num_per_type");
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kCbv)] = GetNum(list, "cbv", 0);
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSrv)] = GetNum(list, "srv", 0);
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kUav)] = GetNum(list, "uav", 0);
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kSampler)] = GetNum(list, "sampler", 0);
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kRtv)] = GetNum(list, "rtv", 0);
    r.descriptor_handle_num_per_type[static_cast<uint32_t>(DescriptorType::kDsv)] = GetNum(list, "dsv", 0);
  }
  r.gpu_handle_num_view = GetNum(j, "gpu_handle_num_view", 1);
  r.gpu_handle_num_sampler = GetNum(j, "gpu_handle_num_sampler", 1);
}
}
#endif
