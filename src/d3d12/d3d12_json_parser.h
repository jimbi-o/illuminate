#ifndef ILLUMINATE_D3D12_JSON_PARSER_H
#define ILLUMINATE_D3D12_JSON_PARSER_H
#include "d3d12_header_common.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
#include <nlohmann/json.hpp>
namespace illuminate {
inline auto GetStringView(const nlohmann::json& j) {
  return j.get<std::string_view>();
}
inline auto GetStringView(const nlohmann::json& j, const char* const name) {
  return j.at(name).get<std::string_view>();
}
inline auto CalcEntityStrHash(const nlohmann::json& j) {
  return CalcStrHash(GetStringView(j).data());
}
inline auto CalcEntityStrHash(const nlohmann::json& j, const char* const name) {
  if (!j.contains(name)) { return StrHash{}; }
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
uint32_t FindIndex(const nlohmann::json& j, const char* const entity_name, const StrHash& name);
D3D12_RESOURCE_STATES GetD3d12ResourceState(const nlohmann::json& j, const char* const entity_name);
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j, const char* const entity_name);
DXGI_FORMAT GetDxgiFormat(const nlohmann::json& j);
ResourceStateType GetResourceStateType(const nlohmann::json& j);
ResourceStateType GetResourceStateType(const nlohmann::json& j, const char* const name);
DescriptorType GetDescriptorType(const nlohmann::json& j);
DescriptorType GetDescriptorType(const nlohmann::json& j, const char* const name);
D3D12_RESOURCE_FLAGS GetD3d12ResourceFlags(const DescriptorTypeFlag descriptor_type_flags);
void SetClearColor(const D3D12_RESOURCE_FLAGS flag, const nlohmann::json& j, D3D12_CLEAR_VALUE* clear_value);
}
#endif
