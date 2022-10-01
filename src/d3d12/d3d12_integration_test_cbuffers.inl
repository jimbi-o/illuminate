namespace illuminate {
using namespace gfxminimath;
namespace {
auto MakeVec3(const float array[3]) {
  return vec3(array[0], array[1], array[2]);
}
auto MakeVec4(const float array[4]) {
  return vec4(array[0], array[1], array[2], array[3]);
}
auto MakeVec4(const float array[3], const float w) {
  return vec4(array[0], array[1], array[2], w);
}
auto CalcLightVectorVs(const float light_direction[3], const matrix& view_matrix) {
  auto light_direction_vs = vec3(mul(view_matrix, MakeVec4(light_direction, 0.0f)));
  return normalize_vector(light_direction_vs);
}
void GetLightOriginLocationInScreenSpace(const vec3& light_direction_vs, const matrix& projection_matrix, const uint32_t primarybuffer_width, const uint32_t primarybuffer_height, int32_t light_origin_location_in_screen_space[2]) {
  auto light_origin = mul(projection_matrix, append_w(light_direction_vs * 100000.0f, 1.0f));
  light_origin = (light_origin + 1.0f) * 0.5f;
  light_origin *= vec4(static_cast<float>(primarybuffer_width), static_cast<float>(primarybuffer_height), 0.0f, 0.0f);
  light_origin = round(light_origin);
  float tmp[4];
  light_origin.store(tmp);
  light_origin_location_in_screen_space[0] = static_cast<int32_t>(tmp[0]);
  light_origin_location_in_screen_space[1] = static_cast<int32_t>(tmp[1]);
}
auto GetAspectRatio(const Size2d& buffer_size) {
  return static_cast<float>(buffer_size.width) / buffer_size.height;
}
void GetCompactProjectionParam(const float fov_vertical_radian, const float aspect_ratio, const float z_near, const float z_far, float compact_projection_param[4]) {
  // https://shikihuiku.github.io/post/projection_matrix/
  // https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
  /*m11*/ compact_projection_param[0] = 1.0f / std::tan(fov_vertical_radian * 0.5f);
  /*m00*/ compact_projection_param[1] = compact_projection_param[0] / aspect_ratio;
  /*m22*/ compact_projection_param[2] = z_far / (z_far - z_near);
  /*m23*/ compact_projection_param[3] = -z_near * compact_projection_param[2];
}
auto GetProjectionMatrix(const float compact_projection_param[4]) {
  return matrix(compact_projection_param[1], 0.0f,  0.0f,  0.0f,
                0.0f, compact_projection_param[0],  0.0f,  0.0f,
                0.0f, 0.0f, compact_projection_param[2], compact_projection_param[1],
                0.0f,  0.0f,  1.0f, 0.0f);
}
auto CalcViewMatrix(const RenderPassConfigDynamicData& dynamic_data) {
  return lookat_lh(MakeVec3(dynamic_data.camera_pos), MakeVec3(dynamic_data.camera_focus));
}
struct Params {
  float view_matrix[16];
  float projection_matrix[16];
  float compact_projection_param[4];
  float light_direction_vs[3];
  float light_slope_zx;
  int32_t light_origin_location_in_screen_space[2];
  float far_div_near;
};
auto PrepareParams(const RenderPassConfigDynamicData& dynamic_data, const MainBufferSize& main_buffer_size) {
  auto params = AllocateFrame<Params>();
  const auto view_matrix = CalcViewMatrix(dynamic_data);
  GetCompactProjectionParam(dynamic_data.fov_vertical * gfxminimath::kDegreeToRadian, GetAspectRatio(main_buffer_size.primarybuffer), dynamic_data.near_z, dynamic_data.near_z, params->compact_projection_param);
  const auto projection_matrix = GetProjectionMatrix(params->compact_projection_param);
  const auto light_direction_vs = CalcLightVectorVs(dynamic_data.light_direction, view_matrix);
  GetLightOriginLocationInScreenSpace(light_direction_vs, projection_matrix, main_buffer_size.primarybuffer.width, main_buffer_size.primarybuffer.height, params->light_origin_location_in_screen_space);
  to_array(light_direction_vs, params->light_direction_vs);
  to_array_column_major(view_matrix, params->view_matrix);
  to_array_column_major(projection_matrix, params->projection_matrix);
  params->light_slope_zx = params->light_direction_vs[2] / params->light_direction_vs[0];
  params->far_div_near = dynamic_data.far_z / dynamic_data.near_z;
  return params;
}
const void* GetValuePtr(const Params& params, const StrHash name_hash) {
  switch (name_hash) {
    case SID("view_matrix"): {
      return params.view_matrix;
    }
    case SID("projection_matrix"): {
      return params.projection_matrix;
    }
    case SID("compact_projection_param"): {
      return params.compact_projection_param;
    }
    case SID("light vector"): {
      return params.light_direction_vs;
    }
    case SID("light_origin_location"): {
      return params.light_origin_location_in_screen_space;
    }
    case SID("far_div_near"): {
      return &params.far_div_near;
    }
    case SID("light_slope_zx"): {
      return &params.light_slope_zx;
    }
  }
  return nullptr;
}
auto GetCBufferParamSizeInBytes(const StrHash& cbuffer_param_name) {
  switch (cbuffer_param_name) {
    case SID("view_matrix"):
    case SID("projection_matrix"): {
      return GetUint32(sizeof(float)) * 16U;
    }
    case SID("compact_projection_param"): {
      return GetUint32(sizeof(float)) * 4U;
    }
    case SID("light vector"): {
      return GetUint32(sizeof(float)) * 3U;
    }
    case SID("light_origin_location"): {
      return GetUint32(sizeof(float)) * 2U;
    }
    case SID("far_div_near"):
    case SID("light_slope_zx"): {
      return GetUint32(sizeof(float));
    }
  }
  assert(false && "no valid cbuffer param found");
  return 0U;
}
auto FillShaderBoundCBuffers(const RenderPassConfigDynamicData& dynamic_data, const MainBufferSize& main_buffer_size, const ArrayOf<CBuffer>& cbuffer_list, void* const * cbuffer_src_list, const uint32_t* cbuffer_writable_size, void** cbuffer_dst_list) {
  auto params = PrepareParams(dynamic_data, main_buffer_size);
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    auto dst = cbuffer_src_list[i];
    for (uint32_t j = 0; j < cbuffer_list.array[i].params.size; j++) {
      auto& param = cbuffer_list.array[i].params.array[j];
      if (auto ptr = GetValuePtr(*params, param.name_hash)) {
        memcpy(dst, ptr, param.size_in_bytes);
      }
      dst = SucceedPtr(dst, param.size_in_bytes);
    }
    memcpy(cbuffer_dst_list[i], cbuffer_src_list[i], cbuffer_writable_size[i]);
  }
}
} // namespace
} // namespace illuminate
