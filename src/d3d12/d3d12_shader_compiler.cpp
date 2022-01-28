#include "d3d12_shader_compiler.h"
#include "d3d12_src_common.h"
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
IDxcResult* CompileShader(IDxcCompiler3* compiler, const uint32_t len, const void* data, const uint32_t compiler_arg_num, LPCWSTR* compiler_args, IDxcIncludeHandler* include_handler){
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
ID3D12RootSignature* CreateRootSignature(D3d12Device* device, IDxcResult* result) {
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
ID3D12PipelineState* CreatePipelineState(D3d12Device* device, IDxcResult* result, const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE shader_object_type, ID3D12RootSignature* root_signature) {
  IDxcBlob* shader_object = nullptr;
  auto hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_object), nullptr);
  if (FAILED(hr)) {
    logwarn("failed to extract shader object. {}", hr);
    if (shader_object) {
      shader_object->Release();
    }
    return nullptr;
  }
  struct {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
    ID3D12RootSignature* root_signature{nullptr};
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode{shader_object_type};
    D3D12_SHADER_BYTECODE shader_bytecode{nullptr};
  } desc_local {
    .root_signature = root_signature,
    .shader_bytecode = D3D12_SHADER_BYTECODE{shader_object->GetBufferPointer(), shader_object->GetBufferSize()},
  };
  D3D12_PIPELINE_STATE_STREAM_DESC desc{sizeof(desc_local), &desc_local};
  ID3D12PipelineState* pso = nullptr;
  hr = device->CreatePipelineState(&desc, IID_PPV_ARGS(&pso));
  shader_object->Release();
  if (SUCCEEDED(hr)) { return pso; }
  logwarn("failed to create pso. {}", hr);
  if (pso) {
    pso->Release();
  }
  return nullptr;
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
bool ShaderCompiler::CreateRootSignatureAndPso(const char* const shadercode, const uint32_t shadercode_len, const uint32_t args_num, const wchar_t** args, D3d12Device* device, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
  auto result = CompileShader(compiler_, shadercode_len, shadercode, args_num, args, include_handler_);
  assert(result && "CompileShader failed.");
  *rootsig = CreateRootSignature(device, result);
  assert(*rootsig && "CreateRootSignature failed.");
  auto shader_type{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS};
  for (uint32_t i = 0; i < args_num; i++) {
    if (wcscmp(L"-T", args[i]) != 0) { continue; }
    assert(i + 1 < args_num && "invalid shader compiler option num");
    const auto target_index = i + 1;
    if (wcsncmp(L"vs_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
      break;
    }
    if (wcsncmp(L"ps_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
      break;
    }
    if (wcsncmp(L"ds_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS;
      break;
    }
    if (wcsncmp(L"hs_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS;
      break;
    }
    if (wcsncmp(L"gs_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS;
      break;
    }
    if (wcsncmp(L"cs_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
      break;
    }
    if (wcsncmp(L"as_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
      break;
    }
    if (wcsncmp(L"ms_", args[target_index], 3) == 0) {
      shader_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
      break;
    }
    assert(false && "invalid shader compile target");
    break;
  }
  *pso =  CreatePipelineState(device, result, shader_type, *rootsig);
  assert(*pso && "CreatePipelineState failed.");
  result->Release();
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
    auto root_signature = CreateRootSignature(device.Get(), result);
    CHECK_NE(root_signature, nullptr);
    auto pso_cs = CreatePipelineState(device.Get(), result, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, root_signature);
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
