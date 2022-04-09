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
  assert(result->HasOutput(DXC_OUT_ROOT_SIGNATURE));
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
IDxcResult* CompileShader(IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, const char* shader_code, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  auto result = CompileShader(compiler, GetUint32(strlen(shader_code)), shader_code, compile_args_num, compile_args, include_handler);
  if (IsCompileSuccessful(result)) { return result; }
  result->Release();
  return nullptr;
}
IDxcResult* CompileShaderFile(IDxcUtils* utils, IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, LPCWSTR filename, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  auto blob = LoadShaderFile(utils, filename);
  if (blob == nullptr) { return nullptr; }
  auto result = CompileShader(compiler, static_cast<uint32_t>(blob->GetBufferSize()), blob->GetBufferPointer(), compile_args_num, compile_args, include_handler);
  blob->Release();
  if (IsCompileSuccessful(result)) { return result; }
  result->Release();
  return nullptr;
}
auto ParseCompileArgs(const nlohmann::json& common_settings, const nlohmann::json& shader_setting, MemoryAllocationJanitor* allocator) {
  uint32_t args_num = 4; // target+entry
  if (common_settings.contains("args")) {
    args_num += GetUint32(common_settings.at("args").size());
  }
  if (shader_setting.contains("args")) {
    args_num += GetUint32(shader_setting.at("args").size());
  }
  auto args = AllocateArray<wchar_t*>(allocator, args_num);
  uint32_t args_index = 0;
  if (common_settings.contains("args")) {
    const auto& args_list = common_settings.at("args");
    for (; args_index < args_list.size(); args_index++) {
      args[args_index] = CopyStrToWstrContainer(args_list[args_index], allocator);
    }
  }
  if (shader_setting.contains("args")) {
    const auto& args_list = shader_setting.at("args");
    for (; args_index < args_list.size(); args_index++) {
      args[args_index] = CopyStrToWstrContainer(args_list[args_index], allocator);
    }
  }
  args[args_index] = CopyStrToWstrContainer("-E", allocator);
  args_index++;
  args[args_index] = CopyStrToWstrContainer(shader_setting.contains("entry") ? shader_setting.at("entry") : common_settings.at("entry"), allocator);
  args_index++;
  args[args_index] = CopyStrToWstrContainer("-T", allocator);
  args_index++;
  args[args_index] = CopyStrToWstrContainer(GetStringView(common_settings, GetStringView(shader_setting, ("target")).data()), allocator);
  args_index++;
  assert(args_index == args_num);
  return std::make_pair(args_num, args);
}
auto CreatePsoDesc(const nlohmann::json& material_json, ID3D12RootSignature* rootsignature, const uint32_t shader_object_num, IDxcBlob** shader_object_list, MemoryAllocationJanitor* allocator) {
  if (shader_object_num == 1 && material_json.at("shaders")[0].at("target") == "cs") {
    struct Desc {
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{};
      ID3D12RootSignature* root_signature{};
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{};
      D3D12_SHADER_BYTECODE shader_bytecode{};
    };
    auto desc = Allocate<Desc>(allocator);
    desc->type_root_signature = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
    desc->root_signature = rootsignature;
    desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
    desc->shader_bytecode = {
      .pShaderBytecode = shader_object_list[0]->GetBufferPointer(),
      .BytecodeLength = shader_object_list[0]->GetBufferSize()
    };
    const auto size = GetUint32(sizeof(*desc));
    return std::make_pair(size, static_cast<void*>(desc));
  }
  struct PsoDescRootSignature {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{};
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
  const auto size = GetUint32(sizeof(PsoDescRootSignature) + sizeof(PsoDescShaderBytecode) * shader_object_num + sizeof(PsoDescGraphicsMisc));
  auto head_ptr = allocator->Allocate(size);
  auto ptr = head_ptr;
  {
    // rootsignature
    auto desc = static_cast<PsoDescRootSignature*>(ptr);
    ptr = SucceedPtrWithStructSize<PsoDescRootSignature>(ptr);
    desc->type_root_signature = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
    desc->root_signature = rootsignature;
  }
  {
    // shader objects
    for (uint32_t i = 0; i < shader_object_num; i++) {
      auto desc = static_cast<PsoDescShaderBytecode*>(ptr);
      ptr = SucceedPtrWithStructSize<PsoDescShaderBytecode>(ptr);
      const auto target = GetStringView(material_json.at("shaders")[i], "target");
      if (target == "ps") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
      } else if (target == "vs") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
      } else if (target == "gs") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS;
      } else if (target == "hs") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS;
      } else if (target == "ds") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS;
      } else if (target == "ms") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
      } else if (target == "as") {
        desc->type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
      }
      desc->shader_bytecode = {
        .pShaderBytecode = shader_object_list[i]->GetBufferPointer(),
        .BytecodeLength  = shader_object_list[i]->GetBufferSize()
      };
    }
  }
  {
    // misc. graphics settings
    auto desc = static_cast<PsoDescGraphicsMisc*>(ptr);
    ptr = SucceedPtrWithStructSize<PsoDescGraphicsMisc>(ptr);
    *desc = {};
    if (material_json.contains("render_target_formats")) {
      auto& render_target_formats = material_json.at("render_target_formats");
      desc->render_target_formats.NumRenderTargets = GetUint32(render_target_formats.size());
      for (uint32_t i = 0; i < desc->render_target_formats.NumRenderTargets; i++) {
        desc->render_target_formats.RTFormats[i] = GetDxgiFormat(GetStringView(render_target_formats[i]).data());
      }
    }
    if (material_json.contains("depth_stencil")) {
      auto& depth_stencil = material_json.at("depth_stencil");
      desc->depth_stencil.DepthEnable = GetBool(depth_stencil,  "depth_enable", true);
      if (desc->depth_stencil.DepthEnable) {
        desc->depth_stencil_format = GetDxgiFormat(depth_stencil, "format");
      }
    }
    if (material_json.contains("input_element")) {
      const auto& input_element_json = material_json.at("input_element");
      desc->input_layout.NumElements = GetUint32(input_element_json.size());
      auto input_element = AllocateArray<D3D12_INPUT_ELEMENT_DESC>(allocator, desc->input_layout.NumElements);
      for (uint32_t i = 0; i < desc->input_layout.NumElements; i++) {
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
      desc->input_layout.pInputElementDescs = input_element;
    }
  }
  assert(reinterpret_cast<std::uintptr_t>(ptr) == reinterpret_cast<std::uintptr_t>(head_ptr) + size);
  return std::make_pair(size, head_ptr);
}
void ParseRootsigAndPsoFromJson(const nlohmann::json& material_json, const nlohmann::json& common_settings,
                                IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, IDxcUtils* utils, D3d12Device* device,
                                ID3D12RootSignature** rootsignature, ID3D12PipelineState** pso) {
  const auto& shader_list = material_json.at("shaders");
  const auto shader_num = GetUint32(shader_list.size());
  auto allocator = GetTemporalMemoryAllocator();
  auto compile_unit_list = AllocateArray<IDxcResult*>(&allocator, shader_num);
  auto shader_object_list = AllocateArray<IDxcBlob*>(&allocator, shader_num);
  uint32_t rootsig_index = shader_num - 1;
  for (uint32_t i = 0; i < shader_num; i++) {
    const auto& shader_setting = shader_list[i];
    if (GetBool(shader_setting, "has_rootsignature", false)) {
      rootsig_index = i;
    }
    auto tmp_allocator = GetTemporalMemoryAllocator();
    const auto [args_num, args] = ParseCompileArgs(common_settings, shader_setting, &tmp_allocator);
    auto filename = CopyStrToWstrContainer(GetStringView(shader_setting, "file"), &tmp_allocator);
    compile_unit_list[i] = CompileShaderFile(utils, compiler, include_handler, filename, args_num, (const wchar_t**)args);
    shader_object_list[i] = GetShaderObject(compile_unit_list[i]);
  }
  *rootsignature = CreateRootSignatureLocal(device, compile_unit_list[rootsig_index]);
  const auto [desc_size, desc] = CreatePsoDesc(material_json, *rootsignature, shader_num, shader_object_list, &allocator);
  *pso = CreatePipelineState(device, desc_size, desc);
  // set instance name
  const auto name = GetStringView(material_json, "name");
  SetD3d12Name(*rootsignature, name);
  SetD3d12Name(*pso, name);
  for (uint32_t i = 0; i < shader_num; i++) {
    shader_object_list[i]->Release();
    compile_unit_list[i]->Release();
  }
}
void ParseJson(const nlohmann::json& json, IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, IDxcUtils* utils, D3d12Device* device,
               uint32_t* pso_num, ID3D12RootSignature*** rootsig_list, ID3D12PipelineState*** pso_list, MemoryAllocationJanitor* allocator) {
  const auto& common_settings = json.at("common_settings");
  const auto& material_json_list = json.at("materials");
  *pso_num = GetUint32(material_json_list.size());
  *rootsig_list = AllocateArray<ID3D12RootSignature*>(allocator, *pso_num);
  *pso_list = AllocateArray<ID3D12PipelineState*>(allocator, *pso_num);
  for (uint32_t i = 0; i < *pso_num; i++) {
    ParseRootsigAndPsoFromJson(material_json_list[i], common_settings, compiler, include_handler, utils, device, &(*rootsig_list)[i], &(*pso_list)[i]);
  }
}
} // anonymous namespace
bool PsoRootsigManager::Init(const nlohmann::json& material_json, D3d12Device* device, MemoryAllocationJanitor* allocator) {
  library_ = LoadDxcLibrary();
  if (library_ == nullptr) { return false; }
  compiler_ = CreateDxcShaderCompiler(library_);
  if (compiler_ == nullptr) { return false; }
   utils_ = CreateDxcUtils(library_);
  if (compiler_ == nullptr) { return false; }
  include_handler_ = CreateDxcIncludeHandler(utils_);
  if (include_handler_ == nullptr) { return false; }
  ParseJson(material_json, compiler_, include_handler_, utils_, device, &pso_num_, &rootsig_list_, &pso_list_, allocator);
  return true;
}
void PsoRootsigManager::Term() {
  for (uint32_t i = 0; i < pso_num_; i++) {
    pso_list_[i]->Release();
  }
  for (uint32_t i = 0; i < pso_num_; i++) {
    rootsig_list_[i]->Release();
  }
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
uint32_t CreateMaterialStrHashList(const nlohmann::json& material_json, StrHash** hash_list_ptr, MemoryAllocationJanitor* allocator) {
  const auto& json_material_list = material_json.at("materials");
  const auto material_num = GetUint32(json_material_list.size());
  auto hash_list = AllocateArray<StrHash>(allocator, material_num);
  for (uint32_t i = 0; i < material_num; i++) {
    hash_list[i] = CalcEntityStrHash(json_material_list[i], "name");
  }
  *hash_list_ptr = hash_list;
  return material_num;
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
