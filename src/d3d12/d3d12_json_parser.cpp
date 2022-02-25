#include "d3d12_json_parser.h"
namespace illuminate {
uint32_t FindIndex(const nlohmann::json& j, const char* const name, const uint32_t num, StrHash* list) {
  if (!j.contains(name)) {
    logwarn("{} not containted in FindIndex json. {}", name, num);
    return 0U;
  }
  auto hash = CalcEntityStrHash(j, name);
  for (uint32_t i = 0; i < num; i++) {
    if (list[i] == hash) { return i; }
  }
  logwarn("FindIndex: {} not found. {}", name, num);
  return ~0U;
}
uint32_t FindIndex(const nlohmann::json& j, const char* const entity_name, const StrHash& name) {
  const auto num = static_cast<uint32_t>(j.size());
  for (uint32_t i = 0; i < num; i++) {
    if (CalcEntityStrHash(j[i], entity_name) == name) { return i; }
  }
  logwarn("FindIndex {} not found. {} {}", entity_name, name, num);
  assert(false&& "FindIndex not found (2)");
  return ~0U;
}
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j) {
  auto format_str = GetStringView(j);
  if (format_str.compare("UNKNOWN") == 0) {
    return DXGI_FORMAT_UNKNOWN;
  }
  if (format_str.compare("R16G16B16A16_FLOAT") == 0) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  }
  if (format_str.compare("B8G8R8A8_UNORM") == 0) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  }
  if (format_str.compare("R8G8B8A8_UNORM") == 0) {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
  if (format_str.compare("D24_UNORM_S8_UINT") == 0) {
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
  }
  if (format_str.compare("R32G32B32_FLOAT") == 0) {
    return DXGI_FORMAT_R32G32B32_FLOAT;
  }
  if (format_str.compare("R32_UINT") == 0) {
    return DXGI_FORMAT_R32_UINT;
  }
  logerror("invalid format specified. {}", format_str.data());
  assert(false && "invalid format specified");
  return DXGI_FORMAT_UNKNOWN;
}
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j, const char* const entity_name) {
  if (!j.contains(entity_name)) {
    logwarn("entity not found. {}", entity_name);
    return DXGI_FORMAT_UNKNOWN;
  }
  return GetDxgiFormat(j.at(entity_name));
}
ResourceStateType GetResourceStateType(const nlohmann::json& j) {
  auto str = GetStringView(j);
  if (str.compare("cbv") == 0) {
    return ResourceStateType::kCbv;
  }
  if (str.compare("srv_ps") == 0) {
    return ResourceStateType::kSrvPs;
  }
  if (str.compare("srv_non_ps") == 0) {
    return ResourceStateType::kSrvNonPs;
  }
  if (str.compare("uav") == 0) {
    return ResourceStateType::kUav;
  }
  if (str.compare("rtv") == 0) {
    return ResourceStateType::kRtv;
  }
  if (str.compare("dsv_write") == 0) {
    return ResourceStateType::kDsvWrite;
  }
  if (str.compare("copy_source") == 0) {
    return ResourceStateType::kCopySrc;
  }
  if (str.compare("copy_dest") == 0) {
    return ResourceStateType::kCopyDst;
  }
  if (str.compare("common") == 0) {
    return ResourceStateType::kCommon;
  }
  if (str.compare("present") == 0) {
    return ResourceStateType::kPresent;
  }
  if (str.compare("generic_read") == 0) {
    return ResourceStateType::kGenericRead;
  }
  logerror("invalid ResourceStateType {}", str);
  assert(false && "invalid ResourceStateType");
  return ResourceStateType::kCommon;
}
ResourceStateType GetResourceStateType(const nlohmann::json& j, const char* const name) {
  assert(j.contains(name));
  return GetResourceStateType(j.at(name));
}
DescriptorType GetDescriptorType(const  nlohmann::json& j, const char* const name) {
  assert(j.contains(name));
  auto str = GetStringView(j, name);
  if (str.compare("cbv") == 0) {
    return DescriptorType::kCbv;
  }
  if (str.compare("srv") == 0) {
    return DescriptorType::kSrv;
  }
  if (str.compare("uav") == 0) {
    return DescriptorType::kUav;
  }
  if (str.compare("sampler") == 0) {
    return DescriptorType::kSampler;
  }
  if (str.compare("rtv") == 0) {
    return DescriptorType::kRtv;
  }
  if (str.compare("dsv") == 0) {
    return DescriptorType::kDsv;
  }
  logerror("invalid DescriptorType {}", name);
  assert(false && "invalid DescriptorType");
  return DescriptorType::kNum;
}
D3D12_RESOURCE_FLAGS GetD3d12ResourceFlags(const DescriptorTypeFlag descriptor_type_flags) {
  D3D12_RESOURCE_FLAGS flag{D3D12_RESOURCE_FLAG_NONE};
  if (descriptor_type_flags & kDescriptorTypeFlagRtv) {
    flag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  if (descriptor_type_flags & kDescriptorTypeFlagDsv) {
    flag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if ((descriptor_type_flags & kDescriptorTypeFlagSrv) == 0) {
      flag |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
  }
  if (descriptor_type_flags & kDescriptorTypeFlagUav) {
    flag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  return flag;
}
void SetClearColor(const D3D12_RESOURCE_FLAGS flag, const nlohmann::json& j, D3D12_CLEAR_VALUE* clear_value) {
  if (flag & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
    if (j.contains("clear_color")) {
      auto& color = j.at("clear_color");
      for (uint32_t i = 0; i < 4; i++) {
        clear_value->Color[i] = color[i];
      }
      return;
    }
  }
  if (flag & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
    clear_value->DepthStencil.Depth = GetVal<FLOAT>(j, "clear_depth", 1.0f);
    clear_value->DepthStencil.Stencil = GetVal<UINT8>(j, "clear_stencil", 0);
    return;
  }
  for (uint32_t i = 0; i < 4; i++) {
    clear_value->Color[i] = 0.0f;
  }
}
}
