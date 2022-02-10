#include "d3d12_render_pass_common.h"
#include "../d3d12_render_graph_json_parser.h"
namespace illuminate {
uint32_t GetShaderCompilerArgs(const nlohmann::json& j, const char* const name, MemoryAllocationJanitor* allocator, std::wstring** wstr_args, const wchar_t*** args) {
  auto shader_compile_args = j.at(name);
  auto args_num = static_cast<uint32_t>(shader_compile_args.size());
  *wstr_args = AllocateArray<std::wstring>(allocator, args_num);
  *args = AllocateArray<const wchar_t*>(allocator, args_num);
  for (uint32_t i = 0; i < args_num; i++) {
    auto str = GetStringView(shader_compile_args[i]);
    (*wstr_args)[i] = std::wstring(str.begin(), str.end()); // assuming string content is single-byte charcters only.
    (*args)[i] = (*wstr_args)[i].c_str();
  }
  return args_num;
}
}
