#include "d3d12_shader_compiler.h"
#include "d3d12_json_parser.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
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
bool IsCompileSuccessful(IDxcResult* result) {
  auto hr = result->HasOutput(DXC_OUT_ERRORS);
  if (SUCCEEDED(hr)) {
    IDxcBlobUtf8* error_text = nullptr;
    hr = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error_text), nullptr);
    if (SUCCEEDED(hr) && error_text && error_text->GetStringLength() > 0) {
      loginfo("{}", error_text->GetStringPointer());
    }
    if (error_text) {
      error_text->Release();
    }
  }
  HRESULT compile_result = S_FALSE;
  hr = result->GetStatus(&compile_result);
  if (FAILED(hr) || FAILED(compile_result)) { return false; }
  return result->HasOutput(DXC_OUT_OBJECT);
}
ID3D12RootSignature* CreateRootSignatureLocal(D3d12Device* device, IDxcResult* result) {
  if (!result->HasOutput(DXC_OUT_ROOT_SIGNATURE)) { return nullptr; }
  IDxcBlob* root_signature_blob = nullptr;
  auto hr = result->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&root_signature_blob), nullptr);
  if (FAILED(hr)) {
    logwarn("failed to extract root signature. {}", hr);
    if (root_signature_blob) {
      root_signature_blob->Release();
    }
    return nullptr;
  }
  ID3D12RootSignature* root_signature = nullptr;
  hr = device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
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
struct PsoDescRootSignature {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
  ID3D12RootSignature* root_signature{nullptr};
};
struct PsoDescShaderBytecode {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{};
  D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
};
struct PsoDescGraphicsMisc {
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
struct PsoConfig {
  uint32_t shader_object_num{0};
  uint32_t* compile_unit_list_index{nullptr};
  uint32_t rootsig_index{0};
  void* pso_desc{nullptr};
  uint32_t pso_size{0};
};
struct ShaderConfig {
  uint32_t file_num{0};
  char** filename_list{nullptr};
  uint32_t compile_unit_num{0};
  uint32_t* compile_unit_filename_index_list{nullptr};
  uint32_t* compile_args_num{nullptr};
  wchar_t*** compile_args{nullptr};
  uint32_t rootsig_num{0};
  uint32_t* rootsig_compile_unit_index{nullptr};
  uint32_t pso_num{0};
  PsoConfig* pso_config_list{nullptr};
};
auto IncrementPtr(void* ptr, const std::size_t& increment_size) {
  return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + increment_size);
}
template <typename T>
auto IncrementPtr(void* ptr, const std::size_t& increment_size) {
  return static_cast<T*>(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ptr) + increment_size));
}
void FillPsoDescWithJsonConfig(const uint32_t shader_bytecode_num, uint32_t* compile_unit_index_list, const nlohmann::json& compile_unit_list, const nlohmann::json& pso, MemoryAllocationJanitor* allocator, void* dst) {
  auto rootsig =  static_cast<PsoDescRootSignature*>(dst);
  *rootsig = {};
  dst = IncrementPtr(dst, sizeof(PsoDescRootSignature));
  auto shader_bytecode_list = static_cast<PsoDescShaderBytecode*>(dst);
  for (uint32_t i = 0; i < shader_bytecode_num; i++) {
    shader_bytecode_list[i] = {};
    auto target = compile_unit_list[compile_unit_index_list[i]].at("target");
    if (target == "cs") {
      assert(i == 0 && shader_bytecode_num == 1 && "invalid shader bytecode index for cs");
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
      return;
    }
    if (target == "ps") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
      continue;
    }
    if (target == "vs") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
      continue;
    }
    if (target == "gs") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS;
      continue;
    }
    if (target == "hs") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS;
      continue;
    }
    if (target == "ds") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS;
      continue;
    }
    if (target == "ms") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
      continue;
    }
    if (target == "as") {
      shader_bytecode_list[i].type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
      continue;
    }
    logerror("target not implemented {}", target);
    assert(false && "target not implemented");
    return;
  }
  dst = IncrementPtr(dst, sizeof(shader_bytecode_list[0]) * shader_bytecode_num);
  auto graphics_misc = static_cast<PsoDescGraphicsMisc*>(dst);
  *graphics_misc = {};
  if (pso.contains("render_target_formats")) {
    auto& render_target_formats = pso.at("render_target_formats");
    graphics_misc->render_target_formats.NumRenderTargets = GetUint32(render_target_formats.size());
    for (uint32_t i = 0; i < graphics_misc->render_target_formats.NumRenderTargets; i++) {
      graphics_misc->render_target_formats.RTFormats[i] = GetDxgiFormat(GetStringView(render_target_formats[i]).data());
    }
  }
  if (pso.contains("depth_stencil")) {
    auto& depth_stencil = pso.at("depth_stencil");
    graphics_misc->depth_stencil.DepthEnable = GetBool(depth_stencil,  "depth_enable", true);
    if (graphics_misc->depth_stencil.DepthEnable) {
      graphics_misc->depth_stencil_format = GetDxgiFormat(depth_stencil, "format");
    }
  }
  if (pso.contains("input_element")) {
    const auto& input_element_json = pso.at("input_element");
    graphics_misc->input_layout.NumElements = GetUint32(input_element_json.size());
    auto input_element = AllocateArray<D3D12_INPUT_ELEMENT_DESC>(allocator, graphics_misc->input_layout.NumElements);
    for (uint32_t i = 0; i < graphics_misc->input_layout.NumElements; i++) {
      input_element[i].SemanticName = GetStringView(input_element_json[i], "name").data();
      input_element[i].SemanticIndex = GetNum(input_element_json[i], "index", 0);
      input_element[i].Format = GetDxgiFormat(input_element_json[i], "format");
      input_element[i].InputSlot = GetNum(input_element_json[i], "slot", 0);
      if (input_element_json[i].contains("aligned_byte_offset")) {
        auto& aligned_byte_offset = input_element_json[i].at("aligned_byte_offset");
        if (aligned_byte_offset.is_string()) {
          assert(aligned_byte_offset == "APPEND_ALIGNED_ELEMENT" && "only D3D12_APPEND_ALIGNED_ELEMENT is implemented");
          input_element[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        } else {
          input_element[i].AlignedByteOffset = aligned_byte_offset;
        }
      } else {
        input_element[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
      }
      if (input_element_json[i].contains("slot_class")) {
        input_element[i].InputSlotClass = (input_element_json[i].at("slot_class") == "PER_INSTANCE_DATA") ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      } else {
        input_element[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      }
      if (input_element[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA) {
        input_element[i].InstanceDataStepRate = 0;
      } else {
        input_element[i].InstanceDataStepRate = GetNum(input_element_json[i], "instance_data_step_rate", 0);
      }
    }
    graphics_misc->input_layout.pInputElementDescs = input_element;
  }
}
auto ParseShaderConfigJson(const nlohmann::json& json, MemoryAllocationJanitor* allocator) {
  ShaderConfig config{};
  const auto& compile_unit = json.at("compile_unit");
  {
    config.compile_unit_num = GetUint32(compile_unit.size());
    for (uint32_t i = 0; i < config.compile_unit_num; i++) {
      const auto str = GetStringView(compile_unit[i], "filename");
      bool duplicate = false;
      for (uint32_t j = 0; j < i; j++) {
        if (str.compare(GetStringView(compile_unit[j], "filename")) == 0) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        config.file_num++;
      }
    }
    config.filename_list = AllocateArray<char*>(allocator, config.file_num);
    uint32_t file_index = 0;
    for (uint32_t i = 0; i < config.compile_unit_num; i++) {
      const auto str = GetStringView(compile_unit[i], "filename");
      bool duplicate = false;
      for (uint32_t j = 0; j < i; j++) {
        if (str.compare(GetStringView(compile_unit[j], "filename")) == 0) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        const auto char_num = GetUint32(str.size() + 1);
        config.filename_list[file_index] = AllocateArray<char>(allocator, char_num);
        strcpy_s(config.filename_list[file_index], char_num, str.data());
        file_index++;
      }
    }
    assert(file_index == config.file_num && "pso creation file count bug");
  }
  {
    const auto& compile_default = json.at("compile_default");
    config.compile_unit_filename_index_list = AllocateArray<uint32_t>(allocator, config.compile_unit_num);
    config.compile_args_num = AllocateArray<uint32_t>(allocator, config.compile_unit_num);
    config.compile_args = AllocateArray<wchar_t**>(allocator, config.compile_unit_num);
    for (uint32_t i = 0; i < config.compile_unit_num; i++) {
      config.compile_args_num[i] = 4;
      const auto& unit = compile_unit[i];
      {
        const auto filename = GetStringView(unit, "filename");
        for (uint32_t j = 0; j < config.file_num; j++) {
          if (strcmp(filename.data(), config.filename_list[j]) == 0) {
            config.compile_unit_filename_index_list[i] = j;
            break;
          }
        }
      }
      if (unit.contains("args")) {
        config.compile_args_num[i] += GetUint32(unit.at("args").size());
      }
      config.compile_args[i] = AllocateArray<wchar_t*>(allocator, config.compile_args_num[i]);
      uint32_t args_index = 0;
      CopyStrToWstrContainer(&config.compile_args[i][args_index], "-T", allocator);
      args_index++;
      const auto target_str = GetStringView(unit, "target");
      if (target_str.compare("ps") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_ps"), allocator);
      } else if (target_str.compare("vs") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_vs"), allocator);
      } else if (target_str.compare("gs") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_gs"), allocator);
      } else if (target_str.compare("hs") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_hs"), allocator);
      } else if (target_str.compare("ds") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_ds"), allocator);
      } else if (target_str.compare("cs") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_cs"), allocator);
      } else if (target_str.compare("lib") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_lib"), allocator);
      } else if (target_str.compare("ms") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_ms"), allocator);
      } else if (target_str.compare("s") == 0) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], GetStringView(compile_default, "target_s"), allocator);
      } else {
        logerror("invalid shader compile target {}", target_str);
        assert(false && "invalid shader compile target");
      }
      args_index++;
      CopyStrToWstrContainer(&config.compile_args[i][args_index], "-E", allocator);
      args_index++;
      if (unit.contains("entry")) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], unit.at("entry"), allocator);
      } else {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], "main", allocator);
      }
      args_index++;
      if (unit.contains("args")) {
        const auto& args = unit.at("args");
        for (uint32_t j = 0; j < args.size(); j++) {
          CopyStrToWstrContainer(&config.compile_args[i][args_index + j], args[j], allocator);
        }
      }
    }
  }
  const auto& rootsig = json.at("rootsig");
  {
    config.rootsig_num = GetUint32(rootsig.size());
    config.rootsig_compile_unit_index = AllocateArray<uint32_t>(allocator, config.rootsig_num);
    for (uint32_t i = 0; i < config.rootsig_num; i++) {
      const auto unit_name = GetStringView(rootsig[i], "unit_name");
      for (uint32_t j = 0; j < config.compile_unit_num; j++) {
        if (unit_name.compare(GetStringView(compile_unit[j], "name")) == 0) {
          config.rootsig_compile_unit_index[i] = j;
          break;
        }
      }
    }
  }
  {
    const auto& pso = json.at("pso");
    config.pso_num = GetUint32(pso.size());
    config.pso_config_list = AllocateArray<PsoConfig>(allocator, config.pso_num);
    for (uint32_t i = 0; i < config.pso_num; i++) {
      const auto& pso_unit = pso[i];
      const auto rootsig_name = GetStringView(pso_unit, "rootsig");
      for (uint32_t j = 0; j < config.rootsig_num; j++) {
        if (rootsig_name.compare(GetStringView(rootsig[j], "name")) == 0) {
          config.pso_config_list[i].rootsig_index = j;
          break;
        }
      }
      const auto& unit_name_list = pso_unit.at("unit_list");
      config.pso_config_list[i].shader_object_num = GetUint32(unit_name_list.size());
      config.pso_config_list[i].compile_unit_list_index = AllocateArray<uint32_t>(allocator, config.pso_config_list[i].shader_object_num);
      for (uint32_t j = 0; j < config.pso_config_list[i].shader_object_num; j++) {
        for (uint32_t k = 0; k < config.compile_unit_num; k++) {
          if (unit_name_list[j] == compile_unit[k].at("name")) {
            config.pso_config_list[i].compile_unit_list_index[j] = k;
            break;
          }
        }
      }
      config.pso_config_list[i].pso_size = (compile_unit[config.pso_config_list[i].compile_unit_list_index[0]].at("target") == "cs") ? (sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode)) : (sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode) * config.pso_config_list[i].shader_object_num + sizeof(PsoDescGraphicsMisc));
      config.pso_config_list[i].pso_desc = allocator->Allocate(config.pso_config_list[i].pso_size);
      FillPsoDescWithJsonConfig(config.pso_config_list[i].shader_object_num, config.pso_config_list[i].compile_unit_list_index, compile_unit, pso_unit, allocator, config.pso_config_list[i].pso_desc);
    }
  }
  return config;
}
IDxcResult* CompileShader(IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, const char* shader_code, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  auto result = CompileShader(compiler, GetUint32(strlen(shader_code)), shader_code, compile_args_num, compile_args, include_handler);
  if (IsCompileSuccessful(result)) { return result; }
  result->Release();
  return nullptr;
}
auto FillPsoPtr(ID3D12RootSignature* rootsig, const uint32_t shader_object_num, IDxcBlob** shader_object_list, void* dst) {
  {
    auto dst_rootsig =  static_cast<PsoDescRootSignature*>(dst);
    dst_rootsig->root_signature = rootsig;
    dst = IncrementPtr(dst, sizeof(PsoDescRootSignature));
  }
  auto shader_bytecode_list = static_cast<PsoDescShaderBytecode*>(dst);
  for (uint32_t i = 0; i < shader_object_num; i++) {
    shader_bytecode_list[i].shader_bytecode.pShaderBytecode = shader_object_list[i]->GetBufferPointer();
    shader_bytecode_list[i].shader_bytecode.BytecodeLength = shader_object_list[i]->GetBufferSize();
  }
}
void ParseJson(const nlohmann::json& json, const char** shader_code_list, IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, D3d12Device* device,
               HashMap<uint32_t, MemoryAllocationJanitor>* rootsig_index_map, uint32_t* rootsig_num, ID3D12RootSignature*** rootsig_list,
               HashMap<uint32_t, MemoryAllocationJanitor>* pso_index_map, uint32_t* pso_num, ID3D12PipelineState*** pso_list,
               MemoryAllocationJanitor* allocator) {
  auto func_allocator = GetTemporalMemoryAllocator();
  auto shader_config = ParseShaderConfigJson(json, &func_allocator);
  auto compile_unit_list = AllocateArray<IDxcResult*>(&func_allocator, shader_config.compile_unit_num);
  auto shader_object_list = AllocateArray<IDxcBlob*>(&func_allocator, shader_config.compile_unit_num);
  *rootsig_num = shader_config.rootsig_num;
  *pso_num = shader_config.pso_num;
  *rootsig_list = AllocateArray<ID3D12RootSignature*>(allocator, shader_config.rootsig_num);
  *pso_list = AllocateArray<ID3D12PipelineState*>(allocator, shader_config.pso_num);
  for (uint32_t i = 0; i < shader_config.compile_unit_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    compile_unit_list[i] = CompileShader(compiler, include_handler, shader_code_list[shader_config.compile_unit_filename_index_list[i]], shader_config.compile_args_num[i], (LPCWSTR*)shader_config.compile_args[i]);
    shader_object_list[i] = GetShaderObject(compile_unit_list[i]);
  }
  for (uint32_t i = 0; i < shader_config.rootsig_num; i++) {
    (*rootsig_list)[i] = CreateRootSignatureLocal(device, compile_unit_list[shader_config.rootsig_compile_unit_index[i]]);
  }
  for (uint32_t i = 0; i < shader_config.pso_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto& config = shader_config.pso_config_list[i];
    auto pso_shader_object_list = AllocateArray<IDxcBlob*>(&tmp_allocator, config.shader_object_num);
    for (uint32_t j = 0; j < config.shader_object_num; j++) {
      pso_shader_object_list[j] = shader_object_list[config.compile_unit_list_index[j]];
    }
    FillPsoPtr((*rootsig_list)[config.rootsig_index], config.shader_object_num, pso_shader_object_list, config.pso_desc);
    (*pso_list)[i] = CreatePipelineState(device, config.pso_size, config.pso_desc);
  }
  for (uint32_t i = 0; i < shader_config.compile_unit_num; i++) {
    shader_object_list[i]->Release();
    compile_unit_list[i]->Release();
  }
  const auto& json_pso_list = json.at("pso");
  for (uint32_t i = 0; i < json_pso_list.size(); i++) {
    const auto name = GetStringView(json_pso_list[i], "name");
    const auto hash = CalcStrHash(name.data());
    rootsig_index_map->Insert(hash, std::move(shader_config.pso_config_list[i].rootsig_index));
    pso_index_map->InsertCopy(hash, i);
    SetD3d12Name((*rootsig_list)[i], json_pso_list[i].at("rootsig"));
    SetD3d12Name((*pso_list)[i], json_pso_list[i].at("name"));
  }
}
} // anonymous namespace
bool PsoRootsigManager::Init(const nlohmann::json& material_json, const char** shader_code_list, D3d12Device* device, MemoryAllocationJanitor* allocator) {
  library_ = LoadDxcLibrary();
  if (library_ == nullptr) { return false; }
  compiler_ = CreateDxcShaderCompiler(library_);
  if (compiler_ == nullptr) { return false; }
   utils_ = CreateDxcUtils(library_);
  if (compiler_ == nullptr) { return false; }
  include_handler_ = CreateDxcIncludeHandler(utils_);
  if (include_handler_ == nullptr) { return false; }
  rootsig_index_.SetAllocator(allocator);
  pso_index_.SetAllocator(allocator);
  ParseJson(material_json, shader_code_list, compiler_, include_handler_, device, &rootsig_index_, &rootsig_num_, &rootsig_list_, &pso_index_, &pso_num_, &pso_list_, allocator);
  return true;
}
void PsoRootsigManager::Term() {
  for (uint32_t i = 0; i < pso_num_; i++) {
    pso_list_[i]->Release();
  }
  for (uint32_t i = 0; i < rootsig_num_; i++) {
    rootsig_list_[i]->Release();
  }
  include_handler_->Release();
  utils_->Release();
  compiler_->Release();
  FreeLibrary(library_);
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
TEST_CASE("compile shader") { // NOLINT
  auto data = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
  const wchar_t* compiler_args[] = {
    L"-T", L"cs_6_6",
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
  auto result = CompileShader(compiler, static_cast<uint32_t>(strlen(data)), data, sizeof(compiler_args) / sizeof(compiler_args[0]), compiler_args, include_handler);
  CHECK_NE(result, nullptr);
  CHECK_UNARY(IsCompileSuccessful(result));
  {
    DxgiCore dxgi_core;
    CHECK_UNARY(dxgi_core.Init()); // NOLINT
    Device device;
    CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
    CHECK_UNARY(result->HasOutput(DXC_OUT_ROOT_SIGNATURE));
    auto root_signature = CreateRootSignatureLocal(device.Get(), result);
    CHECK_NE(root_signature, nullptr);
    auto shader_object = GetShaderObject(result);
    CHECK_NE(shader_object, nullptr);
    struct {
      PsoDescRootSignature rootsig;
      PsoDescShaderBytecode shader_object;
    } desc{};
    desc.rootsig.root_signature = root_signature;
    desc.shader_object.type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
    desc.shader_object.shader_bytecode = {.pShaderBytecode=shader_object->GetBufferPointer(), .BytecodeLength=shader_object->GetBufferSize()};
    auto pso_cs = CreatePipelineState(device.Get(), sizeof(desc), &desc);
    shader_object->Release();
    CHECK_NE(pso_cs, nullptr);
    CHECK_EQ(pso_cs->Release(), 0);
    CHECK_EQ(root_signature->Release(), 0);
    device.Term();
    dxgi_core.Term();
  }
  CHECK_EQ(result->Release(), 0);
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
  CHECK_EQ(compiler->Release(), 0);
  CHECK_UNARY(FreeLibrary(library));
}
TEST_CASE("rootsig/pso") {
  using namespace illuminate;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  auto library = LoadDxcLibrary();
  CHECK_NE(library, nullptr);
  auto compiler = CreateDxcShaderCompiler(library);
  CHECK_NE(compiler, nullptr); // NOLINT
  auto utils = CreateDxcUtils(library);
  CHECK_NE(utils, nullptr); // NOLINT
  auto include_handler = CreateDxcIncludeHandler(utils);
  CHECK_NE(include_handler, nullptr); // NOLINT
  auto json = R"(
{
  "compile_default": {
    "target_ps": "ps_6_6",
    "target_vs": "vs_6_6",
    "target_gs": "gs_6_6",
    "target_hs": "hs_6_6",
    "target_ds": "ds_6_6",
    "target_cs": "cs_6_6",
    "target_lib": "lib_6_6",
    "target_ms": "ms_6_6",
    "target_as": "as_6_6"
  },
  "compile_unit": [
    {
      "name": "compile unit dispatch cs",
      "filename": "dispatch cs",
      "target": "cs",
      "args": ["-Zi","-Qstrip_reflect"]
    },
    {
      "name": "compile unit output to swapchain vs",
      "filename": "output to swapchain",
      "target": "vs",
      "entry": "MainVs",
      "args": []
    },
    {
      "name": "compile unit output to swapchain ps",
      "filename": "output to swapchain",
      "target": "ps",
      "entry": "MainPs",
      "args": ["-Qstrip_reflect"]
    }
  ],
  "rootsig": [
    {
      "name": "rootsig dispatch cs",
      "unit_name": "compile unit dispatch cs"
    },
    {
      "name": "rootsig output to swapchain",
      "unit_name": "compile unit output to swapchain ps"
    }
  ],
  "pso": [
    {
      "name": "pso dispatch cs",
      "rootsig": "rootsig dispatch cs",
      "unit_list": ["compile unit dispatch cs"]
    },
    {
      "name": "pso output to swapchain",
      "rootsig": "rootsig output to swapchain",
      "unit_list": ["compile unit output to swapchain vs", "compile unit output to swapchain ps"],
      "render_target_formats": ["R8G8B8A8_UNORM"],
      "depth_stencil": {
        "depth_enable": false
      }
    }
  ]
}
)"_json;
  auto allocator = GetTemporalMemoryAllocator();
  auto shader_config = ParseShaderConfigJson(json, &allocator);
  CHECK_EQ(shader_config.file_num, 2);
  CHECK_EQ(strcmp(shader_config.filename_list[0], "dispatch cs"), 0);
  CHECK_EQ(strcmp(shader_config.filename_list[1], "output to swapchain"), 0);
  CHECK_EQ(shader_config.compile_unit_num, 3);
  CHECK_EQ(shader_config.compile_unit_filename_index_list[0], 0);
  CHECK_EQ(shader_config.compile_unit_filename_index_list[1], 1);
  CHECK_EQ(shader_config.compile_unit_filename_index_list[2], 1);
  CHECK_EQ(shader_config.compile_args_num[0], 6);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][0], L"-T"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][1], L"cs_6_6"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][2], L"-E"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][3], L"main"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][4], L"-Zi"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][5], L"-Qstrip_reflect"), 0);
  CHECK_EQ(shader_config.compile_args_num[1], 4);
  CHECK_EQ(wcscmp(shader_config.compile_args[1][0], L"-T"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[1][1], L"vs_6_6"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[1][2], L"-E"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[1][3], L"MainVs"), 0);
  CHECK_EQ(shader_config.compile_args_num[2], 5);
  CHECK_EQ(wcscmp(shader_config.compile_args[2][0], L"-T"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[2][1], L"ps_6_6"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[2][2], L"-E"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[2][3], L"MainPs"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[2][4], L"-Qstrip_reflect"), 0);
  CHECK_EQ(shader_config.rootsig_num, 2);
  CHECK_EQ(shader_config.rootsig_compile_unit_index[0], 0);
  CHECK_EQ(shader_config.rootsig_compile_unit_index[1], 2);
  CHECK_EQ(shader_config.pso_num, 2);
  CHECK_EQ(shader_config.pso_config_list[0].shader_object_num, 1);
  CHECK_EQ(shader_config.pso_config_list[0].compile_unit_list_index[0], 0);
  CHECK_EQ(shader_config.pso_config_list[0].rootsig_index, 0);
  CHECK_EQ(static_cast<PsoDescRootSignature*>(shader_config.pso_config_list[0].pso_desc)->type_root_signature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE);
  CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[0].pso_desc, sizeof(PsoDescRootSignature))->type_shader_bytecode, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS);
  CHECK_EQ(shader_config.pso_config_list[1].shader_object_num, 2);
  CHECK_EQ(shader_config.pso_config_list[1].compile_unit_list_index[0], 1);
  CHECK_EQ(shader_config.pso_config_list[1].compile_unit_list_index[1], 2);
  CHECK_EQ(shader_config.pso_config_list[1].rootsig_index, 1);
  CHECK_EQ(static_cast<PsoDescRootSignature*>(shader_config.pso_config_list[1].pso_desc)->type_root_signature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE);
  CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature))->type_shader_bytecode, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS);
  CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode))->type_shader_bytecode, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS);
  auto shader_code_dispatch_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
  auto shader_code_output_to_swapchain = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
Texture2D src : register(t0);
Texture2D src1;
Texture2D pingpong;
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  color.r = src1.Sample(tex_sampler, input.texcoord).r;
  color.g = pingpong.Sample(tex_sampler, input.texcoord).b;
  return color;
}
)";
  const char* shader_code_list[] = {
    shader_code_dispatch_cs,
    shader_code_output_to_swapchain,
  };
  auto compile_unit_list = AllocateArray<IDxcResult*>(&allocator, shader_config.compile_unit_num);
  auto shader_object_list = AllocateArray<IDxcBlob*>(&allocator, shader_config.compile_unit_num);
  auto rootsig_list = AllocateArray<ID3D12RootSignature*>(&allocator, shader_config.rootsig_num);
  auto pso_list = AllocateArray<ID3D12PipelineState*>(&allocator, shader_config.pso_num);
  for (uint32_t i = 0; i < shader_config.compile_unit_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    compile_unit_list[i] = CompileShader(compiler, include_handler, shader_code_list[shader_config.compile_unit_filename_index_list[i]], shader_config.compile_args_num[i], (LPCWSTR*)shader_config.compile_args[i]);
    CHECK_UNARY(compile_unit_list[i]);
    shader_object_list[i] = GetShaderObject(compile_unit_list[i]);
    CHECK_UNARY(shader_object_list[i]);
  }
  for (uint32_t i = 0; i < shader_config.rootsig_num; i++) {
    rootsig_list[i] = CreateRootSignatureLocal(device.Get(), compile_unit_list[shader_config.rootsig_compile_unit_index[i]]);
    CHECK_UNARY(rootsig_list[i]);
  }
  for (uint32_t i = 0; i < shader_config.pso_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto& config = shader_config.pso_config_list[i];
    auto pso_shader_object_list = AllocateArray<IDxcBlob*>(&tmp_allocator, config.shader_object_num);
    for (uint32_t j = 0; j < config.shader_object_num; j++) {
      pso_shader_object_list[j] = shader_object_list[config.compile_unit_list_index[j]];
    }
    FillPsoPtr(rootsig_list[config.rootsig_index], config.shader_object_num, pso_shader_object_list, config.pso_desc);
    if (i == 0) {
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[0].pso_desc, sizeof(PsoDescRootSignature))->shader_bytecode.pShaderBytecode, shader_object_list[0]->GetBufferPointer());
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[0].pso_desc, sizeof(PsoDescRootSignature))->shader_bytecode.BytecodeLength, shader_object_list[0]->GetBufferSize());
    } else {
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature))->shader_bytecode.pShaderBytecode, shader_object_list[1]->GetBufferPointer());
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature))->shader_bytecode.BytecodeLength, shader_object_list[1]->GetBufferSize());
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode))->shader_bytecode.pShaderBytecode, shader_object_list[2]->GetBufferPointer());
      CHECK_EQ(IncrementPtr<PsoDescShaderBytecode>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode))->shader_bytecode.BytecodeLength, shader_object_list[2]->GetBufferSize());
      auto graphics_misc = IncrementPtr<PsoDescGraphicsMisc>(shader_config.pso_config_list[1].pso_desc, sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode) * 2);
      CHECK_EQ(graphics_misc->render_target_formats.NumRenderTargets, 1);
      CHECK_EQ(graphics_misc->render_target_formats.RTFormats[0], DXGI_FORMAT_R8G8B8A8_UNORM);
      CHECK_EQ(graphics_misc->depth_stencil.DepthEnable, FALSE);
    }
    pso_list[i] = CreatePipelineState(device.Get(), config.pso_size, config.pso_desc);
  }
  for (uint32_t i = 0; i < shader_config.pso_num; i++) {
    pso_list[i]->Release();
  }
  for (uint32_t i = 0; i < shader_config.rootsig_num; i++) {
    rootsig_list[i]->Release();
  }
  for (uint32_t i = 0; i < shader_config.compile_unit_num; i++) {
    shader_object_list[i]->Release();
    compile_unit_list[i]->Release();
  }
  {
    HashMap<uint32_t, MemoryAllocationJanitor> rootsig_index(&allocator);
    HashMap<uint32_t, MemoryAllocationJanitor> pso_index(&allocator);
    uint32_t rootsig_num{0};
    uint32_t pso_num{0};
    ID3D12RootSignature** rootsig_list_check_parse_json{nullptr};
    ID3D12PipelineState** pso_list_check_parse_json{nullptr};
    ParseJson(json, shader_code_list, compiler, include_handler, device.Get(), &rootsig_index, &rootsig_num, &rootsig_list_check_parse_json, &pso_index, &pso_num, &pso_list_check_parse_json, &allocator);
    CHECK_EQ(rootsig_num, 2);
    CHECK_EQ(pso_num, 2);
    CHECK_NE(rootsig_list_check_parse_json[0], nullptr);
    CHECK_NE(rootsig_list_check_parse_json[1], nullptr);
    CHECK_NE(pso_list_check_parse_json[0], nullptr);
    CHECK_NE(pso_list_check_parse_json[1], nullptr);
    CHECK_EQ(*(rootsig_index.Get(SID("pso dispatch cs"))), 0);
    CHECK_EQ(*(rootsig_index.Get(SID("pso output to swapchain"))), 1);
    CHECK_EQ(*(pso_index.Get(SID("pso dispatch cs"))), 0);
    CHECK_EQ(*(pso_index.Get(SID("pso output to swapchain"))), 1);
    rootsig_list_check_parse_json[0]->Release();
    rootsig_list_check_parse_json[1]->Release();
    pso_list_check_parse_json[0]->Release();
    pso_list_check_parse_json[1]->Release();
  }
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
  CHECK_EQ(compiler->Release(), 0);
  CHECK_UNARY(FreeLibrary(library));
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
