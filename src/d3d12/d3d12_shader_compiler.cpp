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
  *rootsig = CreateRootSignature(device, result);
  assert(*rootsig && "CreateRootSignature failed.");
  *pso = CreatePipelineStateCs(device, result, *rootsig);
  assert(*pso && "CreatePipelineState failed.");
  result->Release();
  return true;
}
bool ShaderCompiler::CreateRootSignatureAndPsoVsPs(const char* const shadercode_vs, const uint32_t shadercode_vs_len, const uint32_t args_num_vs, const wchar_t** args_vs,
                                                   const char* const shadercode_ps, const uint32_t shadercode_ps_len, const uint32_t args_num_ps, const wchar_t** args_ps,
                                                   D3d12Device* device, PsoDescVsPs* pso_desc, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
  auto result_vs = CompileShader(compiler_, shadercode_vs_len, shadercode_vs, args_num_vs, args_vs, include_handler_);
  assert(result_vs && "CompileShader vs failed.");
  auto result_ps = CompileShader(compiler_, shadercode_ps_len, shadercode_ps, args_num_ps, args_ps, include_handler_);
  assert(result_ps && "CompileShader ps failed.");
  *rootsig = CreateRootSignature(device, result_ps);
  assert(*rootsig && "CreateRootSignature failed.");
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
    auto root_signature = CreateRootSignature(device.Get(), result);
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
