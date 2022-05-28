#ifndef ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_BRDF_HLSLI
#define PI 3.14159265359f
#define RCP_PI 1.0f / PI
//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
float Power5(const float f) {
  float val = f * f; // f^2
  val *= val; // f^4
  return val * f;
}
float3 Fresnel(float3 f0, float vh) {
  return f0 + (1.0f - f0) * Power5(1.0f - vh);
}
float VisTrowbridgeReitz(float alpha_sq, float nl, float nv) { // aka GGX
  const float one_minus_alpha_sq = 1.0f - alpha_sq;
  const float v = nl * sqrt(nv * nv * one_minus_alpha_sq + alpha_sq);
  const float l = nv * sqrt(nl * nl * one_minus_alpha_sq + alpha_sq);
  const float sum = v + l;
  if (sum > 0.0f) {
    return 0.5f * rcp(sum);
  }
  return 0.0f;
}
float DistributionTrowbridgeReitz(const float alpha_sq, const float nh) { // aka GGX
  float f = nh * nh * (alpha_sq - 1.0f) + 1.0f;
  return alpha_sq * rcp(PI * f * f);
}
// clamp dot values before call
float3 BrdfDefault(float3 material_color, float metallic, float roughness, float vh, float nl, float nv, float nh) {
  const float3 diffuse_color = lerp(material_color, 0.0f, metallic);
  const float3 f0 = lerp(0.04f, material_color, metallic);
  const float  alpha = roughness * roughness;
  const float  alpha_sq = alpha * alpha;
  const float3 F = Fresnel(f0, vh);
  const float  V = VisTrowbridgeReitz(alpha_sq, nl, nv);
  const float  D = DistributionTrowbridgeReitz(alpha_sq, nh);
  const float3 diffuse = (1.0f - F) * RCP_PI * diffuse_color;
  const float3 specular = F * V * D;
  return diffuse + specular;
}
#endif
