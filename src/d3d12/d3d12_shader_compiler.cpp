#include "d3d12_shader_compiler.h"
#include "d3d12_src_common.h"
namespace illuminate {
// https://simoncoenen.com/blog/programming/graphics/DxcCompiling
IDxcUtils* CreateDxcUtils() {
  IDxcUtils* utils = nullptr;
  auto hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
  if (SUCCEEDED(hr)) return utils;
  logerror("CreateDxcUtils failed. {}", hr);
  return nullptr;
}
IDxcIncludeHandler* CreateDxcIncludeHandler(IDxcUtils* utils) {
  IDxcIncludeHandler* include_handler = nullptr;
  auto hr = utils->CreateDefaultIncludeHandler(&include_handler);
  if (SUCCEEDED(hr)) return include_handler;
  logerror("CreateDefaultIncludeHandler failed. {}", hr);
  return nullptr;
}
IDxcCompiler3* CreateDxcShaderCompiler() {
  IDxcCompiler3* compiler = nullptr;
  auto hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
  if (SUCCEEDED(hr)) return compiler;
  logerror("CreateDxcShaderCompiler failed. {}", hr);
  return nullptr;
}
}
#include "doctest/doctest.h"
TEST_CASE("compile shader") { // NOLINT
  using namespace illuminate; // NOLINT
  auto utils = CreateDxcUtils();
  CHECK_NE(utils, nullptr); // NOLINT
  auto include_handler = CreateDxcIncludeHandler(utils);
  CHECK_NE(include_handler, nullptr); // NOLINT
  auto compiler = CreateDxcShaderCompiler();
  CHECK_NE(compiler, nullptr); // NOLINT
  CHECK_EQ(compiler->Release(), 0);
  CHECK_EQ(include_handler->Release(), 0);
  CHECK_EQ(utils->Release(), 0);
}
