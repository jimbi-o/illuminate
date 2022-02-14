#include "doctest/doctest.h"
#include <nlohmann/json.hpp>
#include "tiny_gltf.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_descriptors.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_scene.h"
#include "d3d12_shader_compiler.h"
#include "d3d12_swapchain.h"
#include "d3d12_view_util.h"
#include "d3d12_win32_window.h"
#include "render_pass/d3d12_render_pass_cs_dispatch.h"
#include "render_pass/d3d12_render_pass_imgui.h"
#include "render_pass/d3d12_render_pass_postprocess.h"
#include "render_pass/d3d12_render_pass_prez.h"
#include "render_pass/d3d12_render_pass_copy_resource.h"
namespace illuminate {
namespace {
auto GetTestJson() {
  return R"(
{
  "frame_buffer_num": 2,
  "frame_loop_num": 5,
  "primarybuffer_width": 300,
  "primarybuffer_height": 400,
  "primarybuffer_format": "R8G8B8A8_UNORM",
  "window": {
    "title": "integration test",
    "width": 500,
    "height" : 300
  },
  "command_queue": [
    {
      "name": "queue_graphics",
      "type": "3d",
      "priority": "normal",
      "command_list_num": 1
    },
    {
      "name": "queue_compute",
      "type": "compute",
      "priority": "normal",
      "command_list_num": 1
    },
    {
      "name": "queue_copy",
      "type": "copy",
      "priority": "normal",
      "command_list_num": 1
    }
  ],
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
  },
  "buffer": [
    {
      "name": "uav",
      "format": "R8G8B8A8_UNORM",
      "heap_type": "default",
      "dimension": "texture2d",
      "size_type": "swapchain_relative",
      "width": 1.0,
      "height": 1.0,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "uav"
    },
    {
      "name": "pingpong",
      "pingpong": true,
      "format": "R8G8B8A8_UNORM",
      "heap_type": "default",
      "dimension": "texture2d",
      "size_type": "primary_relative",
      "width": 1.0,
      "height": 1.0,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "rtv"
    },
    {
      "name": "cbv-a",
      "need_name_cache": true,
      "format": "UNKNOWN",
      "heap_type": "upload",
      "dimension": "buffer",
      "size_type": "absolute",
      "width": 256,
      "height": 1,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "row_major",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "common"
    },
    {
      "name": "cbv-b",
      "need_name_cache": true,
      "format": "UNKNOWN",
      "heap_type": "upload",
      "dimension": "buffer",
      "size_type": "absolute",
      "width": 256,
      "height": 1,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "row_major",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "common"
    },
    {
      "name": "cbv-c",
      "need_name_cache": true,
      "format": "UNKNOWN",
      "heap_type": "upload",
      "dimension": "buffer",
      "size_type": "absolute",
      "width": 256,
      "height": 1,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "row_major",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "common"
    },
    {
      "name": "dsv",
      "format": "D24_UNORM_S8_UINT",
      "heap_type": "default",
      "dimension": "texture2d",
      "size_type": "primary_relative",
      "width": 1.0,
      "height": 1.0,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "dsv_write",
      "clear_depth": 1,
      "clear_stencil": 0
    },
    {
      "name": "swapchain",
      "need_name_cache": true,
      "descriptor_only": true
    },
    {
      "name": "imgui_font",
      "need_name_cache": true,
      "descriptor_only": true
    }
  ],
  "sampler": [
    {
      "name": "bilinear",
      "filter_min": "linear",
      "filter_mag": "linear",
      "filter_mip": "point",
      "address_mode": ["clamp", "clamp"],
      "mip_lod_bias": 0,
      "max_anisotropy": 0,
      "comparison_func": "never",
      "min_lod": 0,
      "max_lod": 65535
    },
    {
      "name": "trilinear",
      "filter_min": "linear",
      "filter_mag": "linear",
      "filter_mip": "linear",
      "address_mode": ["clamp", "clamp", "clamp"],
      "mip_lod_bias": 0,
      "max_anisotropy": 0,
      "comparison_func": "never",
      "border_color": [0,0,0,0],
      "min_lod": 0,
      "max_lod": 65535
    }
  ],
  "gpu_handle_num_view": 16,
  "gpu_handle_num_sampler": 8,
  "material": {
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
    "compile_unit": [
      {
        "name": "compile unit dispatch cs",
        "filename": "dispatch cs",
        "target": "cs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit output to swapchain vs",
        "filename": "output to swapchain",
        "target": "vs",
        "entry": "MainVs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit output to swapchain ps",
        "filename": "output to swapchain",
        "target": "ps",
        "entry": "MainPs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit pingpong-a vs",
        "filename": "pingpong-a",
        "target": "vs",
        "entry": "MainVs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit pingpong-a ps",
        "filename": "pingpong-a",
        "target": "ps",
        "entry": "MainPs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit pingpong-bc vs",
        "filename": "pingpong-bc",
        "target": "vs",
        "entry": "MainVs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      },
      {
        "name": "compile unit pingpong-bc ps",
        "filename": "pingpong-bc",
        "target": "ps",
        "entry": "MainPs",
        "args": ["-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      }
    ],
    "rootsig": [
      {
        "name": "rootsig dispatch cs",
        "unit_name": "compile unit dispatch cs"
      },
      {
        "name": "rootsig output to swapchain",
        "unit_name": "compile unit output to swapchain ps"
      },
      {
        "name": "rootsig pingpong-a",
        "unit_name": "compile unit pingpong-a ps"
      },
      {
        "name": "rootsig pingpong-bc",
        "unit_name": "compile unit pingpong-bc ps"
      }
    ],
    "pso": [
      {
        "name": "pso dispatch cs",
        "rootsig": "rootsig dispatch cs",
        "unit_list": ["compile unit dispatch cs"]
      },
      {
        "name": "pso output to swapchain",
        "rootsig": "rootsig output to swapchain",
        "unit_list": ["compile unit output to swapchain vs", "compile unit output to swapchain ps"],
        "render_target_formats": ["R8G8B8A8_UNORM"],
        "depth_stencil": {
          "depth_enable": false
        }
      },
      {
        "name": "pso pingpong-a",
        "rootsig": "rootsig pingpong-a",
        "unit_list": ["compile unit pingpong-a vs", "compile unit pingpong-a ps"],
        "render_target_formats": ["R8G8B8A8_UNORM"],
        "depth_stencil": {
          "depth_enable": false
        }
      },
      {
        "name": "pso pingpong-bc",
        "rootsig": "rootsig pingpong-bc",
        "unit_list": ["compile unit pingpong-bc vs", "compile unit pingpong-bc ps"],
        "render_target_formats": ["R8G8B8A8_UNORM"],
        "depth_stencil": {
          "depth_enable": false
        }
      }
    ]
  },
  "render_pass": [
    {
      "name": "copy resource",
      "command_queue": "queue_copy",
      "execute": true
    },
    {
      "name": "dispatch cs",
      "command_queue": "queue_compute",
      "wait_pass": ["output to swapchain"],
      "execute": true,
      "buffer_list": [
        {
          "name": "uav",
          "state": "uav"
        }
      ],
      "pass_vars": {
        "thread_group_count_x": 32,
        "thread_group_count_y": 32,
        "shader": "test.cs.hlsl",
        "shader_compile_args":["-T", "cs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"]
      }
    },
    {
      "name": "prez",
      "command_queue": "queue_graphics",
      "wait_pass": ["copy resource"],
      "execute": true,
      "buffer_list": [
        {
          "name": "dsv",
          "state": "dsv_write"
        }
      ],
      "pass_vars": {
        "shader": "prez.vs.hlsl",
        "shader_compile_args":["-T", "vs_6_6", "-E", "main", "-Zi", "-Zpr", "-Qstrip_debug", "-Qstrip_reflect", "-Qstrip_rootsignature"],
        "stencil_val": 255
      }
    },
    {
      "name": "pingpong-a",
      "command_queue": "queue_graphics",
      "buffer_list": [
        {
          "name": "cbv-a",
          "state": "cbv"
        },
        {
          "name": "pingpong",
          "state": "rtv"
        }
      ],
      "sampler": ["bilinear"],
      "flip_pingpong": ["pingpong"],
      "postpass_barrier": [
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["srv_ps"]
        }
      ],
      "pass_vars": {
        "material": "pso pingpong-a",
        "rtv_index": 1,
        "use_sampler": false,
        "cbv": "cbv-a"
      }
    },
    {
      "name": "pingpong-b",
      "command_queue": "queue_graphics",
      "buffer_list": [
        {
          "name": "cbv-b",
          "state": "cbv"
        },
        {
          "name": "pingpong",
          "state": "srv_ps"
        },
        {
          "name": "pingpong",
          "state": "rtv"
        }
      ],
      "sampler": ["bilinear"],
      "flip_pingpong": ["pingpong"],
      "postpass_barrier": [
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["srv_ps"]
        },
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["rtv"]
        }
      ],
      "pass_vars": {
        "material": "pso pingpong-bc",
        "rtv_index": 2,
        "cbv": "cbv-b"
      }
    },
    {
      "name": "pingpong-c",
      "command_queue": "queue_graphics",
      "buffer_list": [
        {
          "name": "cbv-c",
          "state": "cbv"
        },
        {
          "name": "pingpong",
          "state": "srv_ps"
        },
        {
          "name": "pingpong",
          "state": "rtv"
        }
      ],
      "sampler": ["bilinear"],
      "flip_pingpong": ["pingpong"],
      "postpass_barrier": [
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["srv_ps"]
        },
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["rtv"]
        }
      ],
      "pass_vars": {
        "material": "pso pingpong-bc",
        "rtv_index": 2,
        "cbv": "cbv-c"
      }
    },
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "wait_pass": ["dispatch cs"],
      "execute": true,
      "buffer_list": [
        {
          "name": "uav",
          "state": "srv_ps"
        },
        {
          "name": "dsv",
          "state": "srv_ps"
        },
        {
          "name": "swapchain",
          "state": "rtv"
        },
        {
          "name": "pingpong",
          "state": "srv_ps"
        }
      ],
      "sampler": ["bilinear"],
      "flip_pingpong": ["pingpong"],
      "prepass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": ["present"],
          "state_after": ["rtv"]
        },
        {
          "buffer_name": "uav",
          "type": "transition",
          "split_type": "none",
          "state_after": ["srv_ps"]
        },
        {
          "buffer_name": "dsv",
          "type": "transition",
          "split_type": "none",
          "state_after": ["srv_ps"]
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "uav",
          "type": "transition",
          "split_type": "none",
          "state_after": ["uav"]
        },
        {
          "buffer_name": "dsv",
          "type": "transition",
          "split_type": "none",
          "state_after": ["dsv_write"]
        },
        {
          "buffer_name": "pingpong",
          "type": "transition",
          "split_type": "none",
          "state_after": ["rtv"]
        }
      ],
      "pass_vars": {
        "material": "pso output to swapchain",
        "size_type": "swapchain_relative",
        "rtv_index": 2,
        "use_views": true,
        "use_sampler": true
      }
    },
    {
      "name": "imgui",
      "command_queue": "queue_graphics",
      "execute": true,
      "buffer_list": [
        {
          "name": "imgui_font",
          "state": "srv_ps"
        },
        {
          "name": "swapchain",
          "state": "rtv"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_after": ["present"]
        }
      ]
    }
  ]
}
)"_json;
}
auto GetTestTinyGltf() {
  return R"(
{
  "scene": 0,
  "scenes" : [
    {
      "nodes" : [ 0 ]
    }
  ],
  "nodes" : [
    {
      "mesh" : 0
    }
  ],
  "meshes" : [
    {
      "primitives" : [ {
        "attributes" : {
          "POSITION" : 1
        },
        "indices" : 0
      } ]
    }
  ],
  "buffers" : [
    {
      "uri" : "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=",
      "byteLength" : 44
    }
  ],
  "bufferViews" : [
    {
      "buffer" : 0,
      "byteOffset" : 0,
      "byteLength" : 6,
      "target" : 34963
    },
    {
      "buffer" : 0,
      "byteOffset" : 8,
      "byteLength" : 36,
      "target" : 34962
    }
  ],
  "accessors" : [
    {
      "bufferView" : 0,
      "byteOffset" : 0,
      "componentType" : 5123,
      "count" : 3,
      "type" : "SCALAR",
      "max" : [ 2 ],
      "min" : [ 0 ]
    },
    {
      "bufferView" : 1,
      "byteOffset" : 0,
      "componentType" : 5126,
      "count" : 3,
      "type" : "VEC3",
      "max" : [ 1.0, 1.0, 0.0 ],
      "min" : [ 0.0, 0.0, 0.0 ]
    }
  ],
  "asset" : {
    "version" : "2.0"
  }
}
)";
}
auto PrepareRenderPassResources(const nlohmann::json& src_render_pass_list, MemoryAllocationJanitor* allocator, RenderPassFuncArgsInit* args) {
  ShaderCompiler shader_compiler;
  if (!shader_compiler.Init()) {
    logerror("shader_compiler.Init failed");
    assert(false && "shader_compiler.Init failed");
  }
  args->shader_compiler = &shader_compiler;
  const auto render_pass_num = static_cast<uint32_t>(src_render_pass_list.size());
  auto render_pass_vars = AllocateArray<void*>(allocator, render_pass_num);
  {
    auto shader_code_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("dispatch cs"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_cs;
    render_pass_vars[index] = RenderPassCsDispatch::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    auto shader_code_vs = R"(
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
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("prez"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_vs;
    render_pass_vars[index] = RenderPassPrez::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    auto shader_code_vs_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
Texture2D src : register(t0);
Texture2D src1;
Texture2D pingpong;
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  color.r = src1.Sample(tex_sampler, input.texcoord).r;
  color.g = pingpong.Sample(tex_sampler, input.texcoord).b;
  return color;
}
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("output to swapchain"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_vs_ps;
    render_pass_vars[index] = RenderPassPostprocess::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    const auto index = FindIndex(src_render_pass_list, "name", SID("imgui"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = nullptr;
    render_pass_vars[index] = RenderPassImgui::Init(args);
    CHECK_EQ(render_pass_vars[index], nullptr);
  }
  {
    const auto index = FindIndex(src_render_pass_list, "name", SID("copy resource"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = nullptr;
    render_pass_vars[index] = RenderPassCopyResource::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    auto shader_code_vs_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
float4 cbv_color : register(b0);
#define CopyFullscreenRootsig "\
DescriptorTable(CBV(b0), visibility=SHADER_VISIBILITY_PIXEL),    \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  return cbv_color;
}
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("pingpong-a"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_vs_ps;
    render_pass_vars[index] = RenderPassPostprocess::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    auto shader_code_vs_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
float4 cbv_color : register(b0);
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(CBV(b0),                                                \
                SRV(t0, numDescriptors=1),                              \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return color * cbv_color;
}
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("pingpong-b"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_vs_ps;
    render_pass_vars[index] = RenderPassPostprocess::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  {
    auto shader_code_vs_ps = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
float4 cbv_color : register(b0);
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(CBV(b0),                                                \
                SRV(t0, numDescriptors=1),                              \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return color * cbv_color;
}
)";
    const auto index = FindIndex(src_render_pass_list, "name", SID("pingpong-c"));
    args->json = src_render_pass_list[index].contains("pass_vars") ? &src_render_pass_list[index].at("pass_vars") : nullptr;
    args->shader_code = shader_code_vs_ps;
    render_pass_vars[index] = RenderPassPostprocess::Init(args);
    CHECK_NE(render_pass_vars[index], nullptr);
  }
  shader_compiler.Term();
  return render_pass_vars;
}
auto FindIndex(const uint32_t render_pass_num, const RenderPass* render_pass_list, const StrHash& name) {
  for (uint32_t i = 0; i < render_pass_num; i++) {
    if (render_pass_list[i].name == name) { return i; }
  }
  logerror("render pass not found {} {}", render_pass_num, name);
  assert(false && "render pass not found");
  return ~0U;
}
void ReleaseRenderPassResources(const uint32_t render_pass_num, const RenderPass* render_pass_list, void** render_pass_vars) {
  RenderPassCsDispatch::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("dispatch cs"))]);
  RenderPassPrez::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("prez"))]);
  RenderPassPostprocess::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("output to swapchain"))]);
  RenderPassImgui::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("imgui"))]);
  RenderPassCopyResource::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("copy resource"))]);
  RenderPassPostprocess::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("pingpong-a"))]);
  RenderPassPostprocess::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("pingpong-b"))]);
  RenderPassPostprocess::Term(render_pass_vars[FindIndex(render_pass_num, render_pass_list, SID("pingpong-c"))]);
}
void RenderPassUpdate(const RenderPass& render_pass, void** render_pass_vars, SceneData* scene_data, const uint32_t frame_index) {
  RenderPassFuncArgsUpdate args{
    .pass_vars_ptr = render_pass_vars[render_pass.index],
    .scene_data = scene_data,
    .frame_index = frame_index,
  };
  switch (render_pass.name) {
    case SID("dispatch cs"): {
      RenderPassCsDispatch::Update(&args);
      return;
    }
    case SID("prez"): {
      RenderPassPrez::Update(&args);
      return;
    }
    case SID("output to swapchain"): {
      RenderPassPostprocess::Update(&args);
      return;
    }
    case SID("imgui"): {
      RenderPassImgui::Update(&args);
      return;
    }
    case SID("copy resource"): {
      RenderPassCopyResource::Update(&args);
      return;
    }
    case SID("pingpong-a"): {
      float c[4]{0.0f,1.0f,1.0f,1.0f};
      args.ptr = c;
      RenderPassPostprocess::Update(&args);
      return;
    }
    case SID("pingpong-b"): {
      float c[4]{1.0f,0.0f,1.0f,1.0f};
      args.ptr = c;
      RenderPassPostprocess::Update(&args);
      return;
    }
    case SID("pingpong-c"): {
      float c[4]{1.0f,1.0f,1.0f,1.0f};
      args.ptr = c;
      RenderPassPostprocess::Update(&args);
      return;
    }
  }
  logerror("Update not registered {}", render_pass.name);
  assert(false && "Update not registered");
}
auto IsRenderNeeded(const RenderPass& render_pass, void** render_pass_vars) {
  auto args = render_pass_vars[render_pass.index];
  switch (render_pass.name) {
    case SID("dispatch cs"): {
      return RenderPassCsDispatch::IsRenderNeeded(args);
    }
    case SID("prez"): {
      return RenderPassPrez::IsRenderNeeded(args);
    }
    case SID("output to swapchain"): {
      return RenderPassPostprocess::IsRenderNeeded(args);
    }
    case SID("imgui"): {
      return RenderPassImgui::IsRenderNeeded(args);
    }
    case SID("copy resource"): {
      return RenderPassCopyResource::IsRenderNeeded(args);
    }
    case SID("pingpong-a"): {
      return RenderPassPostprocess::IsRenderNeeded(&args);
    }
    case SID("pingpong-b"): {
      return RenderPassPostprocess::IsRenderNeeded(&args);
    }
    case SID("pingpong-c"): {
      return RenderPassPostprocess::IsRenderNeeded(&args);
     }
  }
  logerror("IsRenderNeeded not registered {}", render_pass.name);
  assert(false && "IsRenderNeeded not registered");
  return false;
}
auto PrepareResourceCpuHandleList(const RenderPass& render_pass, DescriptorCpu* descriptor_cpu, const BufferList& buffer_list, MemoryAllocationJanitor* allocator) {
  auto resource_list = AllocateArray<ID3D12Resource*>(allocator, render_pass.buffer_num);
  auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(allocator, render_pass.buffer_num);
  for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
    auto& buffer = render_pass.buffer_list[b];
    const auto pingpong_rw = GetPingPongBufferReadWriteType(buffer.state);
    resource_list[b] = GetResource(buffer_list, buffer.buffer_index, pingpong_rw);
    const auto descriptor_type = ConvertToDescriptorType(buffer.state);
    if (descriptor_type != DescriptorType::kNum) {
      cpu_handle_list[b].ptr = descriptor_cpu->GetHandle(GetBufferAllocationIndex(buffer_list, buffer.buffer_index, pingpong_rw), descriptor_type).ptr;
    } else {
      cpu_handle_list[b].ptr = 0;
    }
  }
  return std::make_tuple(resource_list, cpu_handle_list);
}
auto RenderPassRender(const RenderPass& render_pass, const MainBufferSize& main_buffer_size, void** render_pass_vars, D3d12CommandList* command_list, ID3D12Resource** resource_list, D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle_list, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle_list, SceneData* scene_data, const uint32_t frame_index) {
  RenderPassFuncArgsRender args{
    .command_list = command_list,
    .main_buffer_size = &main_buffer_size,
    .pass_vars_ptr = render_pass_vars[render_pass.index],
    .gpu_handles = gpu_handle_list,
    .cpu_handles = cpu_handle_list,
    .resources = resource_list,
    .scene_data = scene_data,
    .frame_index = frame_index,
  };
  switch (render_pass.name) {
    case SID("dispatch cs"): {
      RenderPassCsDispatch::Render(&args);
      return;
    }
    case SID("prez"): {
      RenderPassPrez::Render(&args);
      return;
    }
    case SID("output to swapchain"): {
      RenderPassPostprocess::Render(&args);
      return;
    }
    case SID("imgui"): {
      RenderPassImgui::Render(&args);
      return;
    }
    case SID("copy resource"): {
      RenderPassCopyResource::Render(&args);
      return;
    }
    case SID("pingpong-a"): {
      RenderPassPostprocess::Render(&args);
      return;
    }
    case SID("pingpong-b"): {
      RenderPassPostprocess::Render(&args);
      return;
    }
    case SID("pingpong-c"): {
      RenderPassPostprocess::Render(&args);
      return;
    }
  }
  logerror("Render not registered {}", render_pass.name);
  assert(false && "Render not registered");
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    switch (barrier.Type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource   = resource[i];
        barrier.Transition.StateBefore = config.state_before;
        barrier.Transition.StateAfter  = config.state_after;
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        assert(false && "aliasing barrier not implemented yet");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        barrier.UAV.pResource = resource[i];
        break;
      }
    }
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
auto ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, const BufferList& buffer_list) {
  if (barrier_num == 0) { return; }
  auto allocator = GetTemporalMemoryAllocator();
  auto resource_list = AllocateArray<ID3D12Resource*>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    resource_list[i] = GetResource(buffer_list, barrier_config[i].buffer_index, GetPingPongBufferReadWriteTypeFromD3d12ResourceState(barrier_config[i].state_after));
  }
  ExecuteBarrier(command_list, barrier_num, barrier_config, resource_list);
}
auto PrepareGpuHandleList(D3d12Device* device, const RenderPass& render_pass, const BufferList& buffer_list, const DescriptorCpu& descriptor_cpu, DescriptorGpu* const descriptor_gpu, MemoryAllocationJanitor* allocator) {
  auto gpu_handle_list = AllocateArray<D3D12_GPU_DESCRIPTOR_HANDLE>(allocator, 2); // 0:view 1:sampler
  {
    // view
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto buffer_id_list = AllocateArray<uint32_t>(&tmp_allocator, render_pass.buffer_num);
    auto descriptor_type_list = AllocateArray<DescriptorType>(&tmp_allocator, render_pass.buffer_num);
    for (uint32_t j = 0; j < render_pass.buffer_num; j++) {
      buffer_id_list[j] = GetBufferAllocationIndex(buffer_list, render_pass.buffer_list[j].buffer_index, GetPingPongBufferReadWriteType(render_pass.buffer_list[j].state));
      descriptor_type_list[j] = ConvertToDescriptorType(render_pass.buffer_list[j].state);
    }
    gpu_handle_list[0] = descriptor_gpu->CopyViewDescriptors(device, render_pass.buffer_num, buffer_id_list, descriptor_type_list, descriptor_cpu);
  }
  {
    // sampler
    gpu_handle_list[1] = descriptor_gpu->CopySamplerDescriptors(device, render_pass.sampler_num, render_pass.sampler_index_list, descriptor_cpu);
  }
  return gpu_handle_list;
}
auto CreateCpuHandleWithViewImpl(const BufferConfig& buffer_config, const uint32_t buffer_id, const DescriptorType& descriptor_type, ID3D12Resource* resource, DescriptorCpu* descriptor_cpu, D3d12Device* device) {
  auto cpu_handle = descriptor_cpu->CreateHandle(buffer_id, descriptor_type);
  if (cpu_handle.ptr == 0) {
    logwarn("no cpu_handle {} {}", buffer_config.buffer_index, descriptor_type);
    return false;
  }
  if (resource == nullptr) { return true; }
  return CreateView(device, descriptor_type, buffer_config, resource, cpu_handle);
}
auto CreateCpuHandleWithView(const BufferConfig& buffer_config, const uint32_t buffer_id, ID3D12Resource* resource, DescriptorCpu* descriptor_cpu, D3d12Device* device) {
  bool ret = true;
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagCbv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kCbv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(cbv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagSrv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kSrv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(srv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagUav) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kUav, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(uav) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagRtv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kRtv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(rtv) failed.");
    }
  }
  if (buffer_config.descriptor_type_flags & kDescriptorTypeFlagDsv) {
    if (!CreateCpuHandleWithViewImpl(buffer_config, buffer_id, DescriptorType::kDsv, resource, descriptor_cpu, device)) {
      ret = false;
      logerror("CreateCpuHandleWithViewImpl(dsv) failed.");
    }
  }
  return ret;
}
} // namespace anonymous
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  MainBufferSize main_buffer_size{};
  MainBufferFormat main_buffer_format{};
  Window window;
  CommandListSet command_list_set;
  Swapchain swapchain;
  CommandQueueSignals command_queue_signals;
  DescriptorCpu descriptor_cpu;
  BufferList buffer_list;
  DescriptorGpu descriptor_gpu;
  PsoRootsigManager pso_rootsig_manager;
  RenderGraph render_graph;
  void** render_pass_vars{nullptr};
  HashMap<uint32_t, MemoryAllocationJanitor> named_buffer_config_index(&allocator);
  HashMap<uint32_t, MemoryAllocationJanitor> named_buffer_allocator_index(&allocator);
  {
    auto json = GetTestJson();
    ParseRenderGraphJson(json, &allocator, &render_graph);
    CHECK_UNARY(command_list_set.Init(device.Get(),
                                      render_graph.command_queue_num,
                                      render_graph.command_queue_type,
                                      render_graph.command_queue_priority,
                                      render_graph.command_list_num_per_queue,
                                      render_graph.frame_buffer_num,
                                      render_graph.command_allocator_num_per_queue_type));
    CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
    CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, render_graph.frame_buffer_num + 1, render_graph.frame_buffer_num, render_graph.swapchain_usage)); // NOLINT
    main_buffer_size = MainBufferSize{
      .swapchain = {
        .width = swapchain.GetWidth(),
        .height = swapchain.GetHeight(),
      },
      .primarybuffer = {
        render_graph.primarybuffer_width,
        render_graph.primarybuffer_height,
      },
    };
    main_buffer_format = MainBufferFormat{
      .swapchain = render_graph.swapchain_format,
      .primarybuffer = render_graph.primarybuffer_format,
    };
    buffer_list = CreateBuffers(render_graph.buffer_num, render_graph.buffer_list, main_buffer_size, buffer_allocator, &allocator);
    CHECK_UNARY(descriptor_cpu.Init(device.Get(), buffer_list.buffer_allocation_num, render_graph.sampler_num, render_graph.descriptor_handle_num_per_type, &allocator));
    command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
    auto& json_buffer_list = json.at("buffer");
    for (uint32_t i = 0; i < buffer_list.buffer_allocation_num; i++) {
      const auto buffer_config_index = buffer_list.buffer_config_index[i];
      auto& buffer_config = render_graph.buffer_list[buffer_config_index];
      CHECK_UNARY(CreateCpuHandleWithView(buffer_config, i, buffer_list.resource_list[i], &descriptor_cpu, device.Get()));
      auto str = json_buffer_list[buffer_config_index].at("name").get<std::string>();
      if (buffer_config.pingpong) {
        if (buffer_list.is_sub[i]) {
          SetD3d12Name(buffer_list.resource_list[i], str + "_B");
        } else {
          SetD3d12Name(buffer_list.resource_list[i], str + "_A");
        }
      } else {
        SetD3d12Name(buffer_list.resource_list[i], str);
      }
      if (buffer_config.need_name_cache) {
        auto index = buffer_config_index;
        CHECK_UNARY(named_buffer_config_index.Insert(CalcEntityStrHash(json_buffer_list[buffer_config_index], "name"), std::move(index)));
        index = i;
        CHECK_UNARY(named_buffer_allocator_index.Insert(CalcEntityStrHash(json_buffer_list[buffer_config_index], "name"), std::move(index)));
      }
    }
    for (uint32_t i = 0; i < render_graph.sampler_num; i++) {
      auto cpu_handler = descriptor_cpu.CreateHandle(i, DescriptorType::kSampler);
      CHECK_NE(cpu_handler.ptr, 0);
      device.Get()->CreateSampler(&render_graph.sampler_list[i], cpu_handler);
    }
    CHECK_UNARY(descriptor_gpu.Init(device.Get(), render_graph.gpu_handle_num_view, render_graph.gpu_handle_num_sampler));
    {
      auto shader_code_dispatch_cs = R"(
RWTexture2D<float4> uav : register(u0);
#define FillScreenCsRootsig "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL) "
[RootSignature(FillScreenCsRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uav[thread_id.xy] = float4(group_thread_id * rcp(32.0f), 1.0f);
}
)";
      auto shader_code_output_to_swapchain = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
Texture2D src : register(t0);
Texture2D src1;
Texture2D pingpong;
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  color.r = src1.Sample(tex_sampler, input.texcoord).r;
  color.g = pingpong.Sample(tex_sampler, input.texcoord).b;
  return color;
}
)";
      auto shader_code_pingpong_a = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
float4 cbv_color : register(b0);
#define CopyFullscreenRootsig "\
DescriptorTable(CBV(b0), visibility=SHADER_VISIBILITY_PIXEL),    \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  return cbv_color;
}
)";
      auto shader_code_pingpong_bc = R"(
struct FullscreenTriangleVSOutput {
  float4 position : SV_POSITION;
  float2 texcoord : TEXCOORD0;
};
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
float4 cbv_color : register(b0);
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(CBV(b0),                                                \
                SRV(t0, numDescriptors=1),                              \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return color * cbv_color;
}
)";
      const char* shader_code_list[] = {
        shader_code_dispatch_cs,
        shader_code_output_to_swapchain,
        shader_code_pingpong_a,
        shader_code_pingpong_bc,
      };
      CHECK_UNARY(pso_rootsig_manager.Init(json.at("material"), shader_code_list, device.Get(), &allocator));
    }
    RenderPassFuncArgsInit render_pass_func_args_init{
      .json = nullptr,
      .shader_code = nullptr,
      .shader_compiler = nullptr,
      .descriptor_cpu = &descriptor_cpu,
      .device = device.Get(),
      .main_buffer_format = main_buffer_format,
      .hwnd = window.GetHwnd(),
      .frame_buffer_num = render_graph.frame_buffer_num,
      .allocator = &allocator,
      .named_buffer_allocator_index = &named_buffer_allocator_index,
      .named_buffer_config_index = &named_buffer_config_index,
      .buffer_list = &buffer_list,
      .buffer_config_list = render_graph.buffer_list,
      .pso_rootsig_manager = &pso_rootsig_manager,
    };
    render_pass_vars = PrepareRenderPassResources(json.at("render_pass"), &allocator, &render_pass_func_args_init);
  }
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_start = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_end = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
    gpu_descriptor_offset_start[i] = ~0u;
    gpu_descriptor_offset_end[i] = ~0u;
  }
  auto render_pass_signal = AllocateArray<uint64_t>(&allocator, render_graph.render_pass_num);
  for (uint32_t i = 0; i < render_graph.render_pass_num; i++) {
    render_pass_signal[i] = 0UL;
  }
  auto scene_data = GetSceneFromTinyGltfText(GetTestTinyGltf(), "", buffer_allocator, &allocator);
  auto prev_command_list = AllocateArray<D3d12CommandList*>(&allocator, render_graph.command_queue_num);
  for (uint32_t i = 0; i < render_graph.command_queue_num; i++) {
    prev_command_list[i] = nullptr;
  }
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    const auto frame_index = i % render_graph.frame_buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    RegisterResource(*named_buffer_config_index.Get(SID("swapchain")), swapchain.GetResource(), &buffer_list);
    descriptor_cpu.RegisterExternalHandle(*named_buffer_allocator_index.Get(SID("swapchain")), DescriptorType::kRtv, swapchain.GetRtvHandle());
    gpu_descriptor_offset_start[frame_index] = descriptor_gpu.GetViewHandleCount();
    if (gpu_descriptor_offset_start[frame_index] == descriptor_gpu.GetViewHandleTotal()) {
      gpu_descriptor_offset_start[frame_index] = 0;
    }
    if (i > 0) {
      for (uint32_t j = 0; j < render_graph.frame_buffer_num; j++) {
        if (frame_index == j) { continue; }
        if (gpu_descriptor_offset_start[j] == ~0u) { continue; }
        if (gpu_descriptor_offset_start[j] <= gpu_descriptor_offset_end[j]) {
          if (gpu_descriptor_offset_start[frame_index] <= gpu_descriptor_offset_end[frame_index]) {
            assert(gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] || gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && "increase RenderGraph::gpu_handle_num_view");
            continue;
          }
          assert(gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] && "increase RenderGraph::gpu_handle_num_view");
          continue;
        }
        if (gpu_descriptor_offset_start[frame_index] <= gpu_descriptor_offset_end[frame_index]) {
          assert(gpu_descriptor_offset_end[j] <= gpu_descriptor_offset_start[frame_index] && gpu_descriptor_offset_end[frame_index] <= gpu_descriptor_offset_start[j] && "increase RenderGraph::gpu_handle_num_view");
          continue;
        }
        assert(false  && "increase RenderGraph::gpu_handle_num_view");
      }
    }
    // update
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      RenderPassUpdate(render_graph.render_pass_list[k], render_pass_vars, &scene_data, frame_index);
    }
    // render
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      auto render_pass_allocator = GetTemporalMemoryAllocator();
      for (uint32_t l = 0; l < render_pass.wait_pass_num; l++) {
        CHECK_UNARY(command_queue_signals.RegisterWaitOnCommandQueue(render_pass.signal_queue_index[l], render_pass.command_queue_index, render_pass_signal[render_pass.signal_pass_index[l]]));
      }
      if (render_pass.prepass_barrier_num == 0 && render_pass.postpass_barrier_num == 0 && !IsRenderNeeded(render_pass, render_pass_vars)) { continue; }
      auto command_list = prev_command_list[render_pass.command_queue_index];
      if (command_list == nullptr) {
        command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
        prev_command_list[render_pass.command_queue_index] = command_list;
      }
      if (command_list && render_graph.command_queue_type[render_pass.command_queue_index] != D3D12_COMMAND_LIST_TYPE_COPY) {
        descriptor_gpu.SetDescriptorHeapsToCommandList(1, &command_list);
      }
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, buffer_list);
      auto [resource_list, cpu_handle_list] = PrepareResourceCpuHandleList(render_pass, &descriptor_cpu, buffer_list, &render_pass_allocator);
      auto gpu_handle_list = PrepareGpuHandleList(device.Get(), render_pass, buffer_list, descriptor_cpu, &descriptor_gpu, &render_pass_allocator);
      RenderPassRender(render_pass, main_buffer_size, render_pass_vars, command_list, resource_list, cpu_handle_list, gpu_handle_list, &scene_data, frame_index);
      FlipPingPongBuffer(&buffer_list, render_pass.flip_pingpong_num, render_pass.flip_pingpong_index_list);
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, buffer_list);
      if (render_pass.execute) {
        command_list_set.ExecuteCommandList(render_pass.command_queue_index);
        prev_command_list[render_pass.command_queue_index] = nullptr;
      }
      if (render_pass.sends_signal) {
        assert(render_pass.execute);
        render_pass_signal[k] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
        frame_signals[frame_index][render_pass.command_queue_index] = render_pass_signal[k];
      }
    } // render pass
    gpu_descriptor_offset_end[frame_index] = descriptor_gpu.GetViewHandleCount();
    swapchain.Present();
  }
  command_queue_signals.WaitAll(device.Get());
  ReleaseSceneData(&scene_data);
  ReleaseRenderPassResources(render_graph.render_pass_num, render_graph.render_pass_list, render_pass_vars);
  swapchain.Term();
  pso_rootsig_manager.Term();
  descriptor_gpu.Term();
  descriptor_cpu.Term();
  RegisterResource(*named_buffer_config_index.Get(SID("swapchain")), nullptr, &buffer_list);
  ReleaseBuffers(&buffer_list);
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
