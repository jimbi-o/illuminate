struct VsInput {
  float3 position : POSITION;
};
#define PrezRootsig "RootFlags("                                    \
                         "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | "    \
                         "DENY_VERTEX_SHADER_ROOT_ACCESS | "        \
                         "DENY_HULL_SHADER_ROOT_ACCESS | "          \
                         "DENY_DOMAIN_SHADER_ROOT_ACCESS | "        \
                         "DENY_GEOMETRY_SHADER_ROOT_ACCESS | "      \
                         "DENY_PIXEL_SHADER_ROOT_ACCESS | "         \
                         "DENY_AMPLIFICATION_SHADER_ROOT_ACCESS | " \
                         "DENY_MESH_SHADER_ROOT_ACCESS"             \
                         "), "
[RootSignature(PrezRootsig)]
float4 main(const VsInput input) : SV_Position {
  float4 output = float4(input, 1.0f);
  return output;
}
