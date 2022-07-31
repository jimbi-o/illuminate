#include "d3d12_shader_compiler.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include "d3d12_json_parser.h"
#include "d3d12_src_common.h"
#include "illuminate/util/util_functions.h"
#include "../win32_dll_util_macro.h"
namespace illuminate {
namespace {
// https://simoncoenen.com/blog/programming/graphics/DxcCompiling
HMODULE LoadDxcLibrary() {
  auto library = LoadLibrary("dxcompiler.dll");
  assert(library && "failed to load dxcompiler.dll");
  return library;
}
IDxcCompiler3* CreateDxcShaderCompiler(HMODULE library) {
  IDxcCompiler3* compiler = nullptr;
  auto hr = CALL_DLL_FUNCTION(library, DxcCreateInstance)(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
  if (SUCCEEDED(hr)) { return compiler; }
  logerror("CreateDxcShaderCompiler failed. {}", hr);
  return nullptr;
}
IDxcUtils* CreateDxcUtils(HMODULE library) {
  IDxcUtils* utils = nullptr;
  auto hr = CALL_DLL_FUNCTION(library, DxcCreateInstance)(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
  if (SUCCEEDED(hr)) { return utils; }
  logerror("CreateDxcUtils failed. {}", hr);
  return nullptr;
}
IDxcIncludeHandler* CreateDxcIncludeHandler(IDxcUtils* utils) {
  IDxcIncludeHandler* include_handler = nullptr;
  auto hr = utils->CreateDefaultIncludeHandler(&include_handler);
  if (SUCCEEDED(hr)) { return include_handler; }
  logerror("CreateDefaultIncludeHandler failed. {}", hr);
  return nullptr;
}
IDxcBlobEncoding* LoadShaderFile(IDxcUtils* utils, LPCWSTR filename) {
  IDxcBlobEncoding* blob{nullptr};
  auto hr = utils->LoadFile(filename, nullptr, &blob);
  if (SUCCEEDED(hr)) { return blob; }
  if (blob) {
    blob->Release();
  }
  return nullptr;
}
IDxcResult* CompileShader(IDxcCompiler3* compiler, const uint32_t len, const void* data, const uint32_t compiler_arg_num, LPCWSTR* compiler_args, IDxcIncludeHandler* include_handler) {
  DxcBuffer buffer{
    .Ptr = data,
    .Size = len,
    .Encoding = DXC_CP_ACP,
  };
  IDxcResult* result = nullptr;
  auto hr = compiler->Compile(&buffer, compiler_args, compiler_arg_num, include_handler, IID_PPV_ARGS(&result));
  if (SUCCEEDED(hr)) {
    return result;
  }
  logwarn("compile failed.", hr);
  if (result) {
    result->Release();
  }
  return nullptr;
}
template <typename BlobType>
BlobType* GetDxcOutput(IDxcResult* result, const DXC_OUT_KIND kind) {
  if (!result->HasOutput(kind)) { return nullptr; }
  BlobType* blob{};
  auto hr = result->GetOutput(kind, IID_PPV_ARGS(&blob), nullptr);
  if (SUCCEEDED(hr)) {
    return blob;
  }
  if (blob) { blob->Release(); }
  logwarn("GetDxcOutput failed. {} {}", kind, hr);
  return nullptr;
}
template <typename BlobType, typename BlobTypeExtra>
std::pair<BlobType*, BlobTypeExtra*> GetDxcOutput(IDxcResult* result, const DXC_OUT_KIND kind) {
  if (!result->HasOutput(kind)) { return {}; }
  BlobType* blob{};
  BlobTypeExtra* extra{};
  auto hr = result->GetOutput(kind, IID_PPV_ARGS(&blob), &extra);
  if (SUCCEEDED(hr)) {
    return {blob, extra};
  }
  if (blob)  { blob->Release(); }
  if (extra) { extra->Release(); }
  logwarn("GetDxcOutput failed. {} {}", kind, hr);
  return {};
}
bool IsCompileSuccessful(IDxcResult* result) {
  auto hr = result->HasOutput(DXC_OUT_ERRORS);
  if (SUCCEEDED(hr)) {
    auto error_text = GetDxcOutput<IDxcBlobUtf8>(result, DXC_OUT_ERRORS);
    if (error_text) {
      if (error_text->GetStringLength() > 0) {
        loginfo("{}", error_text->GetStringPointer());
      }
      error_text->Release();
    }
  }
  HRESULT compile_result = S_FALSE;
  hr = result->GetStatus(&compile_result);
  if (FAILED(hr) || FAILED(compile_result)) { return false; }
  return result->HasOutput(DXC_OUT_OBJECT);
}
ID3D12RootSignature* CreateRootSignatureLocal(D3d12Device* device, IDxcResult* result) {
  auto root_signature_blob = GetDxcOutput<IDxcBlob>(result, DXC_OUT_ROOT_SIGNATURE);
  if (!root_signature_blob) {
    logwarn("failed to extract root signature.");
    return nullptr;
  }
  ID3D12RootSignature* root_signature = nullptr;
  const auto hr = device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
  root_signature_blob->Release();
  if (SUCCEEDED(hr)) { return root_signature; }
  logwarn("failed to create root signature. {}", hr);
  if (root_signature) {
    root_signature->Release();
  }
  return nullptr;
}
IDxcBlob* GetShaderObject(IDxcResult* result) {
  IDxcBlob* shader_object = nullptr;
  auto hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_object), nullptr);
  if (SUCCEEDED(hr)) { return shader_object; }
  logwarn("failed to extract shader object. {}", hr);
  if (shader_object) {
    shader_object->Release();
  }
  return nullptr;
}
ID3D12PipelineState* CreatePipelineState(D3d12Device* device, const std::size_t& size_in_byte, void* desc_ptr) {
  D3D12_PIPELINE_STATE_STREAM_DESC desc{size_in_byte, desc_ptr};
  ID3D12PipelineState* pso = nullptr;
  auto hr = device->CreatePipelineState(&desc, IID_PPV_ARGS(&pso));
  if (SUCCEEDED(hr)) { return pso; }
  logwarn("failed to create pso. {}", hr);
  if (pso) {
    pso->Release();
  }
  return nullptr;
}
IDxcResult* CompileShader(IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, const char* shader_code, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  auto result = CompileShader(compiler, GetUint32(strlen(shader_code)), shader_code, compile_args_num, compile_args, include_handler);
  if (IsCompileSuccessful(result)) { return result; }
  if (result) { result->Release(); }
  return nullptr;
}
IDxcResult* CompileShaderFile(IDxcUtils* utils, IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, LPCWSTR filename, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  auto blob = LoadShaderFile(utils, filename);
  if (blob == nullptr) { return nullptr; }
  auto result = CompileShader(compiler, static_cast<uint32_t>(blob->GetBufferSize()), blob->GetBufferPointer(), compile_args_num, compile_args, include_handler);
  blob->Release();
  if (!IsCompileSuccessful(result)) {
    if (result) { result->Release(); }
    return nullptr;
  }
#ifdef SHADER_DEBUG_INFO_PATH
  // https://simoncoenen.com/blog/programming/graphics/DxcCompiling
  // https://devblogs.microsoft.com/pix/using-automatic-shader-pdb-resolution-in-pix/
  auto [pdb, pdb_filename_blob] = GetDxcOutput<IDxcBlob, IDxcBlobUtf16>(result, DXC_OUT_PDB);
  if (pdb && pdb_filename_blob) {
    auto pdb_filename = reinterpret_cast<const wchar_t*>(pdb_filename_blob->GetBufferPointer());
    std::filesystem::path path(SHADER_DEBUG_INFO_PATH);
    path.replace_filename(pdb_filename);
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(pdb->GetBufferPointer()), pdb->GetBufferSize());
    ofs.close();
  }
  if (pdb) { pdb->Release(); }
  if (pdb_filename_blob) { pdb_filename_blob->Release(); }
#endif
  return result;
}
auto CountParamCombinationNum(const nlohmann::json& variation) {
  if (!variation.contains("params")) {
    return 1U;
  }
  uint32_t v = 1;
  for (const auto& param : variation.at("params")) {
    v *= GetUint32(param.at("val").size());
  }
  return v;
}
auto CountVariationNum(const nlohmann::json& material) {
  uint32_t count = 0;
  for (const auto& variation : material.at("variations")) {
    count += CountParamCombinationNum(variation);
  }
  return count;
}
auto CountPsoNum(const nlohmann::json& json) {
  uint32_t count = 0;
  for (const auto& material : json) {
    count += CountVariationNum(material);
  }
  return count;
}
struct PsoDescGraphics {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_rasterizer{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER};
  D3D12_RASTERIZER_DESC rasterizer{
    .FillMode = D3D12_FILL_MODE_SOLID,
    .CullMode = D3D12_CULL_MODE_BACK,
    .FrontCounterClockwise = true,
    .DepthBias = 0,
    .DepthBiasClamp = 0.0f,
    .SlopeScaledDepthBias = 1.0f,
    .DepthClipEnable = true,
    .MultisampleEnable = false,
    .AntialiasedLineEnable = false,
    .ForcedSampleCount = 0,
    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
  };
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_input_layout{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT};
  D3D12_INPUT_LAYOUT_DESC input_layout{};
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_primitive_topology{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY};
  D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology{D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE};
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_render_target_formats{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS};
  D3D12_RT_FORMAT_ARRAY render_target_formats{};
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_depth_stencil_format{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT};
  DXGI_FORMAT depth_stencil_format{};
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_depth_stencil{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1};
  D3D12_DEPTH_STENCIL_DESC1 depth_stencil{
    .DepthEnable = true,
    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
    .StencilEnable = false,
    .StencilReadMask = 255U,
    .StencilWriteMask = 255U,
    .FrontFace = {
      .StencilFailOp = D3D12_STENCIL_OP_KEEP,
      .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
      .StencilPassOp = D3D12_STENCIL_OP_REPLACE,
      .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
    },
    .BackFace = {
      .StencilFailOp = D3D12_STENCIL_OP_KEEP,
      .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
      .StencilPassOp = D3D12_STENCIL_OP_KEEP,
      .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
    },
    .DepthBoundsTestEnable = false,
  };
};
auto GetD3d12ComparisonFunc(const std::string_view& func_name) {
  if (func_name.compare("never") == 0) {
    return D3D12_COMPARISON_FUNC_NEVER;
  }
  if (func_name.compare("less") == 0) {
    return D3D12_COMPARISON_FUNC_LESS;
  }
  if (func_name.compare("equal") == 0) {
    return D3D12_COMPARISON_FUNC_EQUAL;
  }
  if (func_name.compare("less_equal") == 0) {
    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
  }
  if (func_name.compare("greater") == 0) {
    return D3D12_COMPARISON_FUNC_GREATER;
  }
  if (func_name.compare("not_equal") == 0) {
    return D3D12_COMPARISON_FUNC_NOT_EQUAL;
  }
  if (func_name.compare("greater_equal") == 0) {
    return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
  }
  if (func_name.compare("always") == 0) {
    return D3D12_COMPARISON_FUNC_ALWAYS;
  }
  logerror("invalid D3D12_COMPARISON_FUNC found. {}", func_name);
  assert(false && "invalid D3D12_COMPARISON_FUNC");
  return D3D12_COMPARISON_FUNC_NEVER;
}
auto PreparePsoDescGraphics(const nlohmann::json& material, const nlohmann::json& variation, const nlohmann::json& common_input_element_list) {
  if (variation.at("shaders").size() == 1 && variation.at("shaders")[0].at("target") == "cs") {
    return (PsoDescGraphics*)nullptr;
  }
  auto desc = AllocateFrame<PsoDescGraphics>();
  *desc = {};
  if (material.contains("render_target_formats")) {
    auto& render_target_formats = material.at("render_target_formats");
    desc->render_target_formats.NumRenderTargets = GetUint32(render_target_formats.size());
    for (uint32_t i = 0; i < desc->render_target_formats.NumRenderTargets; i++) {
      desc->render_target_formats.RTFormats[i] = GetDxgiFormat(GetStringView(render_target_formats[i]).data());
    }
  }
  if (material.contains("depth_stencil")) {
    auto& depth_stencil = material.at("depth_stencil");
    desc->depth_stencil.DepthEnable = GetBool(depth_stencil,  "depth_enable", true);
    if (desc->depth_stencil.DepthEnable) {
      desc->depth_stencil_format = GetDxgiFormat(depth_stencil, "format");
      if (depth_stencil.contains("comparison_func")) {
        desc->depth_stencil.DepthFunc = GetD3d12ComparisonFunc(GetStringView(depth_stencil, "comparison_func"));
        if (desc->depth_stencil.DepthFunc == D3D12_COMPARISON_FUNC_EQUAL) {
          desc->depth_stencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }
      }
    }
  }
  if (variation.contains("input_element")) {
    const auto& shader_input_element_json = variation.at("input_element");
    desc->input_layout.NumElements = GetUint32(shader_input_element_json.size());
    auto input_element = AllocateArrayFrame<D3D12_INPUT_ELEMENT_DESC>(desc->input_layout.NumElements);
    for (uint32_t i = 0; i < desc->input_layout.NumElements; i++) {
      const auto shader_input_element_name = GetStringView(shader_input_element_json[i]);
      uint32_t index = ~0U;
      for (uint32_t j = 0; j < common_input_element_list.size(); j++) {
        if (shader_input_element_name.compare(common_input_element_list[j].at("name")) == 0) {
          index = j;
          break;
        }
      }
      assert(index < common_input_element_list.size());
      const auto& input_element_json = common_input_element_list[index];
      input_element[i].SemanticName = input_element_json.contains("plain_name") ? GetStringView(input_element_json, "plain_name").data() : shader_input_element_name.data();
      input_element[i].SemanticIndex = GetNum(input_element_json, "index", 0);
      input_element[i].Format = GetDxgiFormat(input_element_json, "format");
      input_element[i].InputSlot = i;
      if (input_element_json.contains("aligned_byte_offset")) {
        auto& aligned_byte_offset = input_element_json.at("aligned_byte_offset");
        if (aligned_byte_offset.is_string()) {
          assert(aligned_byte_offset == "APPEND_ALIGNED_ELEMENT" && "only D3D12_APPEND_ALIGNED_ELEMENT is implemented");
          input_element[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        } else {
          input_element[i].AlignedByteOffset = aligned_byte_offset;
        }
      } else {
        input_element[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
      }
      if (input_element_json.contains("slot_class")) {
        input_element[i].InputSlotClass = (input_element_json.at("slot_class") == "PER_INSTANCE_DATA") ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      } else {
        input_element[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      }
      if (input_element[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA) {
        input_element[i].InstanceDataStepRate = 0;
      } else {
        input_element[i].InstanceDataStepRate = GetNum(input_element_json, "instance_data_step_rate", 0);
      }
    }
    desc->input_layout.pInputElementDescs = input_element;
  }
  return desc;
}
auto PrepareCompileArgsFromCommonSettings(const nlohmann::json& common_settings, const uint32_t params_num) {
  uint32_t args_num = 4 + params_num; // target+entry
  uint32_t entry_index = 0, target_index = 0, params_index = 0;
  if (common_settings.contains("args")) {
    args_num += GetUint32(common_settings.at("args").size());
  }
  auto args = AllocateArrayFrame<wchar_t*>(args_num);
  uint32_t args_index = 0;
  if (common_settings.contains("args")) {
    const auto& args_list = common_settings.at("args");
    for (; args_index < args_list.size(); args_index++) {
      args[args_index] = CopyStrToWstrContainer(args_list[args_index], MemoryType::kFrame);
    }
  }
  args[args_index] = CopyStrToWstrContainer("-E", MemoryType::kFrame);
  args_index++;
  entry_index = args_index;
  args_index++;
  args[args_index] = CopyStrToWstrContainer("-T", MemoryType::kFrame);
  args_index++;
  target_index = args_index;
  args_index++;
  params_index = args_index;
  assert(args_index + params_num == args_num);
  return std::make_tuple(args_num, args, entry_index, target_index, params_index);
}
auto WriteVariationParamsToCompileArgs(const nlohmann::json& common_settings, const nlohmann::json& shader_json, const nlohmann::json* params_json, const uint32_t* param_index_list, wchar_t** args, const uint32_t entry_index, const uint32_t target_index, const uint32_t params_index) {
  args[entry_index] = CopyStrToWstrContainer(shader_json.contains("entry") ? shader_json.at("entry") : common_settings.at("entry"), MemoryType::kFrame);
  auto target = GetStringView(shader_json, "target");
  args[target_index] = CopyStrToWstrContainer(common_settings.at(target.data()), MemoryType::kFrame);
  if (!params_json) { return params_index; }
  const auto params_num = GetUint32(params_json->size());
  for (uint32_t i = 0; i < params_num; i++) {
    auto name = GetStringView((*params_json)[i], "name");
    auto val = GetStringView((*params_json)[i].at("val")[param_index_list[i]]);
    auto str = std::string("-D") + name.data() + std::string("=") + val.data();
    args[params_index + i] = CopyStrToWstrContainer(str, MemoryType::kFrame);
  }
  return params_index + params_num;
}
auto GetD3d12PipelineStateSubobjectTypeShader(const char* const target) {
  if (strcmp(target, "cs") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
  }
  if (strcmp(target, "ps") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
  }
  if (strcmp(target, "vs") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
  }
  if (strcmp(target, "gs") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS;
  }
  if (strcmp(target, "hs") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS;
  }
  if (strcmp(target, "ds") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS;
  }
  if (strcmp(target, "ms") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
  }
  if (strcmp(target, "as") == 0) {
    return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
  }
  logerror("invalid shader target {}", target);
  assert(false && "invalid shader target");
  return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
}
auto CreatePsoDesc(ID3D12RootSignature* rootsig, const nlohmann::json& shader_json, const uint32_t shader_object_num, IDxcBlob** shader_object_list, PsoDescGraphics* pso_desc_graphics) {
  struct PsoDescRootSignature {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{};
    ID3D12RootSignature* root_signature{nullptr};
  };
  struct PsoDescShaderBytecode {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{};
    D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
  };
  const auto size = GetUint32(sizeof(PsoDescRootSignature)) + GetUint32(sizeof(PsoDescShaderBytecode)) * shader_object_num + (pso_desc_graphics == nullptr ? 0 : GetUint32(sizeof(*pso_desc_graphics)));
  auto ptr = AllocateFrame(size);
  auto head = ptr;
  if (pso_desc_graphics != nullptr) {
    memcpy(ptr, pso_desc_graphics, sizeof(*pso_desc_graphics));
    ptr = SucceedPtrWithStructSize<PsoDescGraphics>(ptr);
  }
  {
    auto desc = new(ptr) PsoDescRootSignature;
    ptr = SucceedPtrWithStructSize<PsoDescRootSignature>(ptr);
    desc->type_root_signature = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
    desc->root_signature = rootsig;
  }
  for (uint32_t i = 0; i < shader_object_num; i++) {
    auto desc = new(ptr) PsoDescShaderBytecode;
    ptr = SucceedPtrWithStructSize<PsoDescShaderBytecode>(ptr);
    desc->type_shader_bytecode = GetD3d12PipelineStateSubobjectTypeShader(GetStringView(shader_json[i], "target").data());
    desc->shader_bytecode = {
      .pShaderBytecode = shader_object_list[i]->GetBufferPointer(),
      .BytecodeLength  = shader_object_list[i]->GetBufferSize()
    };
  }
  return std::make_pair(size, head);
}
auto GetPsoName(const std::string_view& material_name, const nlohmann::json* params_json, const uint32_t param_num, const uint32_t* param_index_list) {
  std::string name(material_name);
  if (params_json) {
    for (uint32_t i = 0; i < param_num; i++) {
      name += " ";
      name += (*params_json)[i].at("val")[param_index_list[i]];
    }
  }
  return name;
}
auto CreateRootsigNameList(const nlohmann::json& json) {
  const auto num = GetUint32(json.size());
  auto hash_list = AllocateArrayFrame<StrHash>(num);
  uint32_t used_hash = 0;
  for (uint32_t i = 0; i < num; i++) {
    const auto rootsig_name_hash = CalcEntityStrHash(json[i], "rootsig");
    bool found = false;
    for (uint32_t j = 0; j < used_hash; j++) {
      if (rootsig_name_hash == hash_list[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      hash_list[used_hash] = rootsig_name_hash;
      used_hash++;
    }
  }
  return std::make_pair(used_hash, hash_list);
}
uint32_t GetVertexBufferTypeFlags(const nlohmann::json& variation) {
  if (!variation.contains("input_element")) { return 0; }
  const char* const names[] = {
    "POSITION",
    "NORMAL",
    "TANGENT",
    "TEXCOORD_0",
  };
  static_assert(std::size(names) == kVertexBufferTypeNum);
  const auto& input_element = variation.at("input_element");
  uint32_t flags = 0;
  for (const auto& name : input_element) {
    for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
      if (name == names[i]) {
        flags |= GetVertexBufferTypeFlag(static_cast<VertexBufferType>(i));
        break;
      }
    }
  }
  return flags;
}
void ParseJson(const nlohmann::json& json, IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, IDxcUtils* utils, D3d12Device* device,
               uint32_t* rootsig_num, ID3D12RootSignature*** rootsig_list,
               uint32_t* pso_num, ID3D12PipelineState*** pso_list, uint32_t** material_pso_offset, uint32_t** vertex_buffer_type_flags) {
  const auto& common_settings = json.at("common_settings");
  const auto& material_json_list = json.at("materials");
  auto [rootsig_num_counted, rootsig_name_list] = CreateRootsigNameList(material_json_list);
  *rootsig_num = rootsig_num_counted;
  *rootsig_list = AllocateArraySystem<ID3D12RootSignature*>(*rootsig_num);
  for (uint32_t i = 0; i < *rootsig_num; i++) {
    (*rootsig_list)[i] = nullptr;
  }
  *pso_num = CountPsoNum(material_json_list);
  *pso_list = AllocateArraySystem<ID3D12PipelineState*>(*pso_num);
  const auto& input_element_list = common_settings.at("input_elements");
  const auto material_num = GetUint32(material_json_list.size());
  *material_pso_offset = AllocateArraySystem<uint32_t>(material_num);
  *vertex_buffer_type_flags = AllocateArraySystem<uint32_t>(*pso_num);
  uint32_t pso_index = 0;
  for (uint32_t i = 0; i < material_num; i++) {
    (*material_pso_offset)[i] = pso_index;
    const auto& material = material_json_list[i];
    const auto rootsig_index = FindHashIndex(*rootsig_num, rootsig_name_list, CalcEntityStrHash(material, "rootsig"));
    const auto& variations = material.at("variations");
    for (const auto& variation : variations) {
      const auto& shader_json = variation.at("shaders");
      const auto shader_num = GetUint32(shader_json.size());
      const auto pso_desc_graphics = PreparePsoDescGraphics(material, variation, input_element_list);
      const auto variation_vertex_buffer_type_flags = GetVertexBufferTypeFlags(variation);
      const auto& params_json = variation.contains("params") ? &variation.at("params") : nullptr;
      const auto params_num = params_json ? GetUint32(params_json->size()) : 1;
      auto [compile_args_num, compile_args, entry_index, target_index, params_index] = PrepareCompileArgsFromCommonSettings(common_settings, params_num);
      auto param_array_size_list = AllocateArrayFrame<uint32_t>(params_num);
      auto param_index_list = AllocateArrayFrame<uint32_t>(params_num);
      SetArrayValues(0, params_num, param_index_list);
      SetArrayValues(1, params_num, param_array_size_list);
      if (params_json) {
        for (uint32_t j = 0; j < params_num; j++) {
          param_array_size_list[j] = GetUint32((*params_json)[j].at("val").size());
        }
      }
      while (true) {
        auto compile_unit_list = AllocateArrayFrame<IDxcResult*>(shader_num);
        auto shader_object_list = AllocateArrayFrame<IDxcBlob*>(shader_num);
        for (uint32_t j = 0; j < shader_num; j++) {
          const auto args_num = WriteVariationParamsToCompileArgs(common_settings, shader_json[j], params_json, param_index_list, compile_args, entry_index, target_index, params_index);
          auto filename = CopyStrToWstrContainer(GetStringView(shader_json[j], ("file")), MemoryType::kFrame);
          compile_unit_list[j] = CompileShaderFile(utils, compiler, include_handler, filename, args_num, (const wchar_t**)compile_args);
          shader_object_list[j] = GetShaderObject(compile_unit_list[j]);
        }
        auto rootsig = (*rootsig_list)[rootsig_index];
        if (rootsig == nullptr) {
          rootsig = CreateRootSignatureLocal(device, compile_unit_list[shader_num - 1]);
          SetD3d12Name(rootsig, GetStringView(material, "rootsig"));
          (*rootsig_list)[rootsig_index] = rootsig;
        }
        auto [pso_desc_size, pso_desc] = CreatePsoDesc(rootsig, shader_json, shader_num, shader_object_list, pso_desc_graphics);
        (*pso_list)[pso_index] = CreatePipelineState(device, pso_desc_size, pso_desc);
        SetD3d12Name((*pso_list)[pso_index], GetPsoName(material.at("name"), params_json, params_num, param_index_list));
        (*vertex_buffer_type_flags)[pso_index] = variation_vertex_buffer_type_flags;
        pso_index++;
        for (uint32_t j = 0; j < shader_num; j++) {
          shader_object_list[j]->Release();
          compile_unit_list[j]->Release();
        }
        if (!SucceedMultipleIndex(params_num, param_array_size_list, param_index_list)) {
          break;
        }
      }
    }
  }
}
auto CalcMaterialVariationParamsHash(const nlohmann::json& params, const uint32_t* param_index) {
  StrHash hash{};
  for (uint32_t i = 0; i < params.size(); i++) {
    hash = CombineHash(CalcEntityStrHash(params[i], "name"), hash);
    hash = CombineHash(CalcEntityStrHash(params[i].at("val")[param_index[i]]), hash);
  }
  return hash;
}
void SetMaterialVariationValues(const nlohmann::json& material_json, uint32_t* material_num, uint32_t** material_hash_list, uint32_t** variation_hash_list_len, uint32_t*** variation_hash_list) {
  const auto& material_list_json = material_json.at("materials");
  *material_num = CreateJsonStrHashList(material_list_json, "name", material_hash_list, MemoryType::kSystem);
  *variation_hash_list_len = AllocateArraySystem<uint32_t>(*material_num);
  *variation_hash_list = AllocateArraySystem<StrHash*>(*material_num);
  for (uint32_t i = 0; i < *material_num; i++) {
    const auto& material = material_list_json[i];
    const auto variation_num = CountVariationNum(material);
    (*variation_hash_list_len)[i] = variation_num;
    (*variation_hash_list)[i] = AllocateArraySystem<StrHash>(variation_num);
    uint32_t hash_index = 0;
    for (const auto& variation : material.at("variations")) {
      const auto& params_json = variation.contains("params") ? &variation.at("params") : nullptr;
      const auto params_num = params_json ? GetUint32(params_json->size()) : 1;
      auto param_array_size_list = AllocateArrayFrame<uint32_t>(params_num);
      auto param_index_list = AllocateArrayFrame<uint32_t>(params_num);
      SetArrayValues(0, params_num, param_index_list);
      SetArrayValues(1, params_num, param_array_size_list);
      if (params_json) {
        for (uint32_t j = 0; j < params_num; j++) {
          param_array_size_list[j] = GetUint32((*params_json)[j].at("val").size());
        }
      }
      while (true) {
        (*variation_hash_list)[i][hash_index] = params_json ? CalcMaterialVariationParamsHash(*params_json, param_index_list) : StrHash();
        hash_index++;
        if (!SucceedMultipleIndex(params_num, param_array_size_list, param_index_list)) {
          break;
        }
      }
    }
    assert(hash_index == (*variation_hash_list_len)[i]);
  }
}
} // anonymous namespace
bool ShaderCompiler::Init() {
  library_ = LoadDxcLibrary();
  if (library_ == nullptr) { return false; }
  compiler_ = CreateDxcShaderCompiler(library_);
  if (compiler_ == nullptr) { return false; }
  utils_ = CreateDxcUtils(library_);
  if (compiler_ == nullptr) { return false; }
  include_handler_ = CreateDxcIncludeHandler(utils_);
  if (include_handler_ == nullptr) { return false; }
  return true;
}
void ShaderCompiler::Term() {
  if (include_handler_) {
    include_handler_->Release();
  }
  if (utils_) {
    utils_->Release();
  }
  if (compiler_) {
    compiler_->Release();
  }
  if (library_) {
    FreeLibrary(library_);
  }
}
MaterialList ShaderCompiler::BuildMaterials(const nlohmann::json& material_json, D3d12Device* device) {
  MaterialList list{};
  ParseJson(material_json, compiler_, include_handler_, utils_, device,
            &list.rootsig_num, &list.rootsig_list, &list.pso_num, &list.pso_list, &list.material_pso_offset, &list.vertex_buffer_type_flags);
  SetMaterialVariationValues(material_json, &list.material_num, &list.material_hash_list, &list.variation_hash_list_len, &list.variation_hash_list);
  return list;
}
void ReleasePsoAndRootsig(MaterialList* list) {
  for (uint32_t i = 0; i < list->pso_num; i++) {
    list->pso_list[i]->Release();
  }
  for (uint32_t i = 0; i < list->rootsig_num; i++) {
    list->rootsig_list[i]->Release();
  }
}
uint32_t FindMaterialVariationIndex(const MaterialList& material_list, const uint32_t material, const StrHash variation_hash) {
  if (material >= material_list.material_num) {
    if (material_list.material_num == 0) { return MaterialList::kInvalidIndex; }
    return 0;
  }
  for (uint32_t i = 0; i < material_list.variation_hash_list_len[material]; i++) {
    if (material_list.variation_hash_list[material][i] == variation_hash) {
      return i;
    }
  }
  return 0;
}
MaterialList BuildMaterialList(D3d12Device* device, const nlohmann::json& material_json) {
  ShaderCompiler shader_compiler;
  if (!shader_compiler.Init()) {
    logwarn("shader compilation failed.");
    return MaterialList{};
  }
  auto material_list = shader_compiler.BuildMaterials(material_json, device);
  shader_compiler.Term();
  return material_list;
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
TEST_CASE("compile shader file") { // NOLINT
  const wchar_t* compiler_args[] = {
    L"-T", L"ps_6_5",
    L"-E", L"main",
    L"-Zi",
    L"-Zpr",
    L"-Qstrip_debug",
    L"-Qstrip_reflect",
    L"-Qstrip_rootsignature",
  };
  using namespace illuminate; // NOLINT
  auto library = LoadDxcLibrary();
  CHECK_NE(library, nullptr);
  auto compiler = CreateDxcShaderCompiler(library);
  CHECK_NE(compiler, nullptr); // NOLINT
  auto utils = CreateDxcUtils(library);
  CHECK_NE(utils, nullptr); // NOLINT
  auto include_handler = CreateDxcIncludeHandler(utils);
  CHECK_NE(include_handler, nullptr); // NOLINT
  auto blob = LoadShaderFile(utils, L"shader/test/shader.test.hlsl");
  CHECK_NE(blob, nullptr); // NOLINT
  auto result = CompileShader(compiler, static_cast<uint32_t>(blob->GetBufferSize()), blob->GetBufferPointer(), sizeof(compiler_args) / sizeof(compiler_args[0]), compiler_args, include_handler);
  CHECK_NE(result, nullptr);
  CHECK_UNARY(IsCompileSuccessful(result));
  CHECK_EQ(blob->Release(), 0);
  result = CompileShaderFile(utils, compiler, include_handler, L"shader/test/shader.test.hlsl", sizeof(compiler_args) / sizeof(compiler_args[0]), compiler_args);
  CHECK_NE(result, nullptr);
  CHECK_UNARY(IsCompileSuccessful(result));
  CHECK_EQ(result->Release(), 0);
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
  CHECK_EQ(compiler->Release(), 0);
  CHECK_UNARY(FreeLibrary(library));
}
