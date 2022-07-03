#ifndef ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#define ILLUMINATE_D3D12_RENDER_GRAPH_JSON_PARSER_H
#include "d3d12_json_parser.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_render_graph.h"
#include "d3d12_scene.h"
#include "d3d12_src_common.h"
#include "illuminate/util/hash_map.h"
#include <nlohmann/json.hpp>
namespace illuminate {
void ParseRenderGraphJson(const nlohmann::json& j, const uint32_t material_num, StrHash* material_hash_list, RenderGraph* graph);
}
#endif
