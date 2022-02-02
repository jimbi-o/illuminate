#ifndef ILLUMINATE_D3D12_SHADER_COMPILER_H
#define ILLUMINATE_D3D12_SHADER_COMPILER_H
#include "d3d12_header_common.h"
namespace illuminate {
class ShaderCompiler {
 public:
  struct PsoDescVsPs {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_root_signature{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE};
    ID3D12RootSignature* root_signature{nullptr};
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode_vs{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS};
    D3D12_SHADER_BYTECODE shader_bytecode_vs{nullptr};
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type_shader_bytecode_ps{D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS};
    D3D12_SHADER_BYTECODE shader_bytecode_ps{nullptr};
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
        .StencilPassOp = D3D12_STENCIL_OP_REPLACE,
        .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
      },
      .DepthBoundsTestEnable = false,
    };
  };
  bool Init();
  void Term();
  bool CreateRootSignatureAndPsoCs(const char* const shadercode, const uint32_t shadercode_len, const uint32_t args_num, const wchar_t** args, D3d12Device* device, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso);
  bool CreateRootSignatureAndPsoVs(const char* const shadercode, const uint32_t shadercode_len, const uint32_t args_num, const wchar_t** args,
                                   D3d12Device* device, PsoDescVsPs* pso_desc, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso);
  bool CreateRootSignatureAndPsoVsPs(const char* const shadercode_vs, const uint32_t shadercode_vs_len, const uint32_t args_num_vs, const wchar_t** args_vs,
                                     const char* const shadercode_ps, const uint32_t shadercode_ps_len, const uint32_t args_num_ps, const wchar_t** args_ps,
                                     D3d12Device* device, PsoDescVsPs* pso_desc, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso);
 private:
  HMODULE library_{nullptr};
  IDxcCompiler3* compiler_{nullptr};
  IDxcUtils* utils_{nullptr};
  IDxcIncludeHandler* include_handler_{nullptr};
};
}
#endif
