#include "d3d12_shader_compiler.h"
#include "d3d12_render_graph_json_parser.h"
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
ID3D12PipelineState* CreatePipelineStateCs(D3d12Device* device, IDxcResult* result, ID3D12RootSignature* root_signature) {
  auto shader_object = GetShaderObject(result);
  assert(shader_object);
  struct {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
    ID3D12RootSignature* root_signature{nullptr};
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS};
    D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
  } desc_local {
    .root_signature = root_signature,
    .shader_bytecode = D3D12_SHADER_BYTECODE{shader_object->GetBufferPointer(), shader_object->GetBufferSize()},
  };
  auto pso = CreatePipelineState(device, sizeof(desc_local), &desc_local);
  shader_object->Release();
  return pso;
}
ID3D12PipelineState* CreatePipelineStateVsPs(D3d12Device* device, IDxcResult* result_vs, IDxcResult* result_ps, ShaderCompiler::PsoDescVsPs* pso_desc) {
  auto shader_object_vs = GetShaderObject(result_vs);
  assert(shader_object_vs);
  auto shader_object_ps = GetShaderObject(result_ps);
  assert(shader_object_ps);
  pso_desc->shader_bytecode_vs = D3D12_SHADER_BYTECODE{shader_object_vs->GetBufferPointer(), shader_object_vs->GetBufferSize()};
  pso_desc->shader_bytecode_ps = D3D12_SHADER_BYTECODE{shader_object_ps->GetBufferPointer(), shader_object_ps->GetBufferSize()};
  auto pso = CreatePipelineState(device, sizeof(*pso_desc), pso_desc);
  shader_object_ps->Release();
  shader_object_vs->Release();
  return pso;
}
} // namespace anonymous
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
  include_handler_->Release();
  utils_->Release();
  compiler_->Release();
  FreeLibrary(library_);
}
bool ShaderCompiler::CreateRootSignatureAndPsoCs(const char* const shadercode, const uint32_t shadercode_len, const uint32_t args_num, const wchar_t** args, D3d12Device* device, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
  auto result = CompileShader(compiler_, shadercode_len, shadercode, args_num, args, include_handler_);
  assert(result && "CompileShader failed.");
  *rootsig = CreateRootSignatureLocal(device, result);
  assert(*rootsig && "CreateRootSignature failed.");
  *pso = CreatePipelineStateCs(device, result, *rootsig);
  assert(*pso && "CreatePipelineState failed.");
  result->Release();
  return true;
}
bool ShaderCompiler::CreateRootSignatureAndPsoVs(const char* const shadercode, const uint32_t shadercode_len, const uint32_t args_num, const wchar_t** args,
                                                 D3d12Device* device, PsoDescVsPs* pso_desc, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
  return CreateRootSignatureAndPsoVsPs(shadercode, shadercode_len, args_num, args, nullptr, 0, 0, nullptr, device, pso_desc, rootsig, pso);
}
bool ShaderCompiler::CreateRootSignatureAndPsoVsPs(const char* const shadercode_vs, const uint32_t shadercode_vs_len, const uint32_t args_num_vs, const wchar_t** args_vs,
                                                   const char* const shadercode_ps, const uint32_t shadercode_ps_len, const uint32_t args_num_ps, const wchar_t** args_ps,
                                                   D3d12Device* device, PsoDescVsPs* pso_desc, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
  auto result_vs = CompileShader(compiler_, shadercode_vs_len, shadercode_vs, args_num_vs, args_vs, include_handler_);
  assert(result_vs && "CompileShader vs failed.");
  auto result_ps = CompileShader(compiler_, shadercode_ps_len, shadercode_ps, args_num_ps, args_ps, include_handler_);
  assert(result_ps && "CompileShader ps failed.");
  if (shadercode_ps_len > 0) {
    *rootsig = CreateRootSignatureLocal(device, result_ps);
    assert(*rootsig && "CreateRootSignatureLocal(ps) failed.");
  }
  if (*rootsig == nullptr) {
    *rootsig = CreateRootSignatureLocal(device, result_vs);
  }
  pso_desc->root_signature = *rootsig;
  *pso = CreatePipelineStateVsPs(device, result_vs, result_ps, pso_desc);
  assert(*pso && "CreatePipelineStateVsPs failed.");
  result_vs->Release();
  result_ps->Release();
  return true;
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
    auto pso_cs = CreatePipelineStateCs(device.Get(), result, root_signature);
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
namespace illuminate {
namespace {
static const uint32_t kPsoDescShaderObjectMaxNum = 2;
struct PsoDescGraphicsShaderObject {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{};
  D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
};
struct PsoDescGraphics {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
  ID3D12RootSignature* root_signature{nullptr};
  PsoDescGraphicsShaderObject shader_object[kPsoDescShaderObjectMaxNum]{};
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
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_depth_stencil1{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1};
  D3D12_DEPTH_STENCIL_DESC1 depth_stencil1{
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
struct PsoDescCompute {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
  ID3D12RootSignature* root_signature{nullptr};
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS};
  D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
};
union PsoDesc {
  PsoDescGraphics graphics;
  PsoDescCompute compute;
};
struct PsoConfig {
  uint32_t shader_object_num{0};
  uint32_t* compile_result_list_index{nullptr};
  uint32_t rootsig_index{0};
  PsoDesc pso_desc{};
};
struct ShaderConfig {
  uint32_t compile_result_num{0};
  uint32_t* compile_args_num{nullptr};
  wchar_t*** compile_args{nullptr};
  uint32_t rootsig_num{0};
  uint32_t* rootsig_compile_result_index{nullptr};
  uint32_t pso_num{0};
  PsoConfig* pso_config_list{nullptr};
};
auto ParseShaderConfigJson(const nlohmann::json json, MemoryAllocationJanitor* allocator) {
  ShaderConfig config{};
  const auto& compile_entity = json.at("compile_entity");
  {
    const auto& compile_default = json.at("compile_default");
    config.compile_result_num = GetUint32(compile_entity.size());
    config.compile_args_num = AllocateArray<uint32_t>(allocator, config.compile_result_num);
    config.compile_args = AllocateArray<wchar_t**>(allocator, config.compile_result_num);
    for (uint32_t i = 0; i < config.compile_result_num; i++) {
      config.compile_args_num[i] = 4;
      const auto& entity = compile_entity[i];
      if (entity.contains("args")) {
        config.compile_args_num[i] += GetUint32(entity.at("args").size());
      }
      config.compile_args[i] = AllocateArray<wchar_t*>(allocator, config.compile_args_num[i]);
      uint32_t args_index = 0;
      CopyStrToWstrContainer(&config.compile_args[i][args_index], "-T", allocator);
      args_index++;
      const auto target_str = GetStringView(entity, "target");
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
      if (entity.contains("entry")) {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], entity.at("entry"), allocator);
      } else {
        CopyStrToWstrContainer(&config.compile_args[i][args_index], "main", allocator);
      }
      args_index++;
      if (entity.contains("args")) {
        const auto& args = entity.at("args");
        for (uint32_t j = 0; j < args.size(); j++) {
          CopyStrToWstrContainer(&config.compile_args[i][args_index + j], args[j], allocator);
        }
      }
    }
  }
  const auto& rootsig = json.at("rootsig");
  {
    config.rootsig_num = GetUint32(rootsig.size());
    config.rootsig_compile_result_index = AllocateArray<uint32_t>(allocator, config.rootsig_num);
    for (uint32_t i = 0; i < config.rootsig_num; i++) {
      const auto entity_name = GetStringView(rootsig[i], "entity_name");
      for (uint32_t j = 0; j < config.compile_result_num; j++) {
        if (entity_name.compare(GetStringView(compile_entity[j], "name")) == 0) {
          config.rootsig_compile_result_index[i] = j;
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
      const auto& pso_entity = pso[i];
      const auto rootsig_name = GetStringView(pso_entity, "rootsig");
      for (uint32_t j = 0; j < config.rootsig_num; j++) {
        if (rootsig_name.compare(GetStringView(rootsig[j], "name")) == 0) {
          config.pso_config_list[i].rootsig_index = j;
          break;
        }
      }
      const auto& entity_name_list = pso_entity.at("entity_list");
      config.pso_config_list[i].shader_object_num = GetUint32(entity_name_list.size());
      config.pso_config_list[i].compile_result_list_index = AllocateArray<uint32_t>(allocator, config.pso_config_list[i].shader_object_num);
      for (uint32_t j = 0; j < config.pso_config_list[i].shader_object_num; j++) {
        for (uint32_t k = 0; k < config.compile_result_num; k++) {
          const auto entity_name = GetStringView(entity_name_list[j]);
          if (entity_name.compare(GetStringView(compile_entity[k], "name")) == 0) {
            config.pso_config_list[i].compile_result_list_index[j] = k;
            if (compile_entity[k].at("target") == "cs") {
              config.pso_config_list[i].pso_desc.compute.type_root_signature = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
              config.pso_config_list[i].pso_desc.compute.type_shader_bytecode = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
            } else {
              assert(false && "graphics pso not implemented yet");
            }
            break;
          }
        }
      }
    }
  }
  return config;
}
auto CompileShader(IDxcCompiler3* compiler, IDxcIncludeHandler* include_handler, const char* shader_code, const uint32_t compile_args_num, LPCWSTR* compile_args) {
  return CompileShader(compiler, GetUint32(strlen(shader_code)), shader_code, compile_args_num, compile_args, include_handler);
}
auto FillPsoConfig(ID3D12RootSignature* rootsig, const uint32_t shader_object_num, IDxcBlob** shader_object_list, PsoDesc* desc) {
  if (desc->compute.type_shader_bytecode == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS) {
    assert(shader_object_num == 1);
    desc->compute.root_signature = rootsig;
    desc->compute.shader_bytecode.pShaderBytecode = shader_object_list[0]->GetBufferPointer();
    desc->compute.shader_bytecode.BytecodeLength = shader_object_list[0]->GetBufferSize();
    return sizeof(desc->compute);
  }
  assert(shader_object_num <= kPsoDescShaderObjectMaxNum && "Currently, only vs-ps pso is implemented");
  desc->graphics.root_signature = rootsig;
  for (uint32_t i = 0; i < shader_object_num; i++) {
    desc->graphics.shader_object[i].shader_bytecode.pShaderBytecode = shader_object_list[i]->GetBufferPointer();
    desc->graphics.shader_object[i].shader_bytecode.BytecodeLength = shader_object_list[i]->GetBufferSize();
  }
  return sizeof(desc->graphics);
}
} // anonymous namespace
} // namespace illuminate
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
  "compile_entity": [
    {
      "name": "compile entity dispatch cs",
      "target": "cs",
      "entry": "main",
      "args": ["-Zi","-Qstrip_reflect"]
    }
  ],
  "rootsig": [
    {
      "name": "rootsig dispatch cs",
      "entity_name": "compile entity dispatch cs"
    }
  ],
  "pso": [
    {
      "name": "pso dispatch cs",
      "rootsig": "rootsig dispatch cs",
      "entity_list": ["compile entity dispatch cs"]
    }
  ]
}
)"_json;
  auto shader_code_dispatch_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
  const char* shader_code_list[] = {
    shader_code_dispatch_cs,
  };
  auto allocator = GetTemporalMemoryAllocator();
  auto shader_config = ParseShaderConfigJson(json, &allocator);
  CHECK_EQ(shader_config.compile_result_num, 1);
  CHECK_EQ(shader_config.compile_args_num[0], 6);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][0], L"-T"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][1], L"cs_6_6"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][2], L"-E"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][3], L"main"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][4], L"-Zi"), 0);
  CHECK_EQ(wcscmp(shader_config.compile_args[0][5], L"-Qstrip_reflect"), 0);
  CHECK_EQ(shader_config.rootsig_num, 1);
  CHECK_EQ(shader_config.rootsig_compile_result_index[0], 0);
  CHECK_EQ(shader_config.pso_num, 1);
  CHECK_EQ(shader_config.pso_config_list[0].shader_object_num, 1);
  CHECK_EQ(shader_config.pso_config_list[0].compile_result_list_index[0], 0);
  CHECK_EQ(shader_config.pso_config_list[0].rootsig_index, 0);
  CHECK_EQ(shader_config.pso_config_list[0].pso_desc.compute.type_root_signature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE);
  CHECK_EQ(shader_config.pso_config_list[0].pso_desc.compute.type_shader_bytecode, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS);
  auto compile_result_list = AllocateArray<IDxcResult*>(&allocator, shader_config.compile_result_num);
  auto shader_object_list = AllocateArray<IDxcBlob*>(&allocator, shader_config.compile_result_num);
  auto rootsig_list = AllocateArray<ID3D12RootSignature*>(&allocator, shader_config.rootsig_num);
  auto pso_list = AllocateArray<ID3D12PipelineState*>(&allocator, shader_config.pso_num);
  for (uint32_t i = 0; i < shader_config.compile_result_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    compile_result_list[i] = CompileShader(compiler, include_handler, shader_code_list[i], shader_config.compile_args_num[i], (LPCWSTR*)shader_config.compile_args[i]);
    CHECK_UNARY(compile_result_list[i]);
    shader_object_list[i] = GetShaderObject(compile_result_list[i]);
    CHECK_UNARY(shader_object_list[i]);
  }
  for (uint32_t i = 0; i < shader_config.rootsig_num; i++) {
    rootsig_list[i] = CreateRootSignatureLocal(device.Get(), compile_result_list[shader_config.rootsig_compile_result_index[i]]);
    CHECK_UNARY(rootsig_list[i]);
  }
  for (uint32_t i = 0; i < shader_config.pso_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto& config = shader_config.pso_config_list[i];
    auto pso_shader_object_list = AllocateArray<IDxcBlob*>(&tmp_allocator, config.shader_object_num);
    for (uint32_t j = 0; j < config.shader_object_num; j++) {
      pso_shader_object_list[j] = shader_object_list[config.compile_result_list_index[j]];
      CHECK_UNARY(pso_shader_object_list[j]);
    }
    const auto desc_size = FillPsoConfig(rootsig_list[config.rootsig_index], config.shader_object_num, shader_object_list, &config.pso_desc);
    CHECK_EQ(shader_config.pso_config_list[i].pso_desc.compute.root_signature, rootsig_list[config.rootsig_index]);
    CHECK_EQ(shader_config.pso_config_list[i].pso_desc.compute.shader_bytecode.pShaderBytecode, shader_object_list[0]->GetBufferPointer());
    CHECK_EQ(shader_config.pso_config_list[i].pso_desc.compute.shader_bytecode.BytecodeLength, shader_object_list[0]->GetBufferSize());
    CHECK_EQ(desc_size, sizeof(PsoDescCompute));
    pso_list[i] = CreatePipelineState(device.Get(), desc_size, &config.pso_desc);
  }
  for (uint32_t i = 0; i < shader_config.pso_num; i++) {
    pso_list[i]->Release();
  }
  for (uint32_t i = 0; i < shader_config.rootsig_num; i++) {
    rootsig_list[i]->Release();
  }
  for (uint32_t i = 0; i < shader_config.compile_result_num; i++) {
    shader_object_list[i]->Release();
    compile_result_list[i]->Release();
  }
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
  CHECK_EQ(compiler->Release(), 0);
  CHECK_UNARY(FreeLibrary(library));
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
