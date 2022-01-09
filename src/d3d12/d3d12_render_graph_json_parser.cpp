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
D3D12_RESOURCE_STATES GetD3d12ResourceState(const nlohmann::json& j, const char* const name) {
  auto state_str = j.at(name).get<std::string_view>();
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
  assert(val_set && "no valid resource state");
  return state;
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
