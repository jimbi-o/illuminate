#include "d3d12_shader_compiler.h"
#include "d3d12_src_common.h"
#include "../win32_dll_util_macro.h"
namespace illuminate {
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
}
#include "doctest/doctest.h"
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
  CHECK_EQ(result->Release(), 0);
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
  CHECK_EQ(compiler->Release(), 0);
  CHECK_UNARY(FreeLibrary(library));
}
