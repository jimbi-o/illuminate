#ifndef ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#define ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#include "d3d12_render_graph.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
#include <nlohmann/json.hpp>
namespace illuminate {
inline auto GetStringView(const nlohmann::json& j, const char* const name) {
  return j.at(name).get<std::string_view>();
}
inline auto CalcEntityStrHash(const nlohmann::json& j, const char* const name) {
  return CalcStrHash(GetStringView(j, name).data());
}
uint32_t FindIndex(const nlohmann::json& j, const char* const name, const uint32_t num, StrHash* list);
D3D12_RESOURCE_STATES GetD3d12ResourceState(const nlohmann::json& j, const char* const name);
void GetBarrierList(const nlohmann::json& j, const uint32_t barrier_num, Barrier* barrier_list);
typedef void (*RenderPassVarParseFunction)(const nlohmann::json&, void*);
template <typename A1, typename A2, typename A3>
void ParseRenderGraphJson(const nlohmann::json& j, const HashMap<uint32_t, A1>& pass_var_size, const HashMap<RenderPassVarParseFunction, A2>& pass_var_func, A3* allocator, RenderGraph* graph) {
  auto& r = *graph;
  j.at("frame_buffer_num").get_to(r.frame_buffer_num);
  j.at("frame_loop_num").get_to(r.frame_loop_num);
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
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_DIRECT)] = allocators.contains("direct") ? allocators.at("direct").get<uint32_t>() : 0;
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COMPUTE)] = allocators.contains("compute") ? allocators.at("compute").get<uint32_t>() : 0;
    r.command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(D3D12_COMMAND_LIST_TYPE_COPY)] = allocators.contains("copy") ? allocators.at("copy").get<uint32_t>() : 0;
  }
  {
    auto& swapchain = j.at("swapchain");
    r.swapchain_command_queue_index = FindIndex(swapchain, "command_queue", r.command_queue_num, r.command_queue_name);
    auto format_str = GetStringView(swapchain, "format");
    if (format_str.compare("R16G16B16A16_FLOAT") == 0) {
      r.swapchain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    } else if (format_str.compare("B8G8R8A8_UNORM") == 0) {
      r.swapchain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else {
      r.swapchain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
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
  {
    auto& render_pass_list = j.at("render_pass");
    r.render_pass_num = static_cast<uint32_t>(render_pass_list.size());
    r.render_pass_list = AllocateArray<RenderPass>(allocator, r.render_pass_num);
    for (uint32_t i = 0; i < r.render_pass_num; i++) {
      auto& dst_pass = r.render_pass_list[i];
      auto& src_pass = render_pass_list[i];
      dst_pass.name = CalcEntityStrHash(src_pass, "name");
      dst_pass.command_queue_index = FindIndex(src_pass, "command_queue", r.command_queue_num, r.command_queue_name);
      if (src_pass.contains("buffers")) {
        auto& buffers = src_pass.at("buffers");
        dst_pass.buffer_num = static_cast<uint32_t>(buffers.size());
        dst_pass.buffers = AllocateArray<RenderPassBuffer>(allocator, dst_pass.buffer_num);
        for (uint32_t buffer_index = 0; buffer_index < dst_pass.buffer_num; buffer_index++) {
          auto& dst_buffer = dst_pass.buffers[buffer_index];
          auto& src_buffer = buffers[buffer_index];
          dst_buffer.buffer_name = CalcEntityStrHash(src_buffer, "name");
          dst_buffer.state = GetD3d12ResourceState(src_buffer, "state");
        }
      } // buffers
      if (pass_var_func.Get(dst_pass.name) != nullptr && src_pass.contains("pass_vars")) {
        dst_pass.pass_vars = allocator->Allocate(pass_var_size.Get(dst_pass.name));
        (*pass_var_func.Get(dst_pass.name))(src_pass.at("pass_vars"), dst_pass.pass_vars);
      }
      if (src_pass.contains("prepass_barrier")) {
        auto& prepass_barrier = src_pass.at("prepass_barrier");
        dst_pass.prepass_barrier_num = static_cast<uint32_t>(prepass_barrier.size());
        dst_pass.prepass_barrier = AllocateArray<Barrier>(allocator, dst_pass.prepass_barrier_num);
        GetBarrierList(prepass_barrier, dst_pass.prepass_barrier_num, dst_pass.prepass_barrier);
      }
      if (src_pass.contains("postpass_barrier")) {
        auto& postpass_barrier = src_pass.at("postpass_barrier");
        dst_pass.postpass_barrier = AllocateArray<Barrier>(allocator, dst_pass.postpass_barrier_num);
        GetBarrierList(postpass_barrier, dst_pass.postpass_barrier_num, dst_pass.postpass_barrier);
      } // barriers
    } // pass
  } // pass_list
}
}
#endif
