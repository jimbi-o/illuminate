/* Provide Declarations */
#include <stdarg.h>
#include <setjmp.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#ifndef __cplusplus
typedef unsigned char bool;
#endif

/* get a declaration for alloca */
#if defined(__CYGWIN__) || defined(__MINGW32__)
#define  alloca(x) __builtin_alloca((x))
#define _alloca(x) __builtin_alloca((x))
#elif defined(__APPLE__)
extern void *__builtin_alloca(unsigned long);
#define alloca(x) __builtin_alloca(x)
#define longjmp _longjmp
#define setjmp _setjmp
#elif defined(__sun__)
#if defined(__sparcv9)
extern void *__builtin_alloca(unsigned long);
#else
extern void *__builtin_alloca(unsigned int);
#endif
#define alloca(x) __builtin_alloca(x)
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__arm__)
#define alloca(x) __builtin_alloca(x)
#elif defined(_MSC_VER)
#define alloca(x) _alloca(x)
#else
#include <alloca.h>
#endif

#ifndef _MSC_VER
#define __forceinline __attribute__((always_inline)) inline
#endif

#if defined(__GNUC__)
#define  __ATTRIBUTELIST__(x) __attribute__(x)
#else
#define  __ATTRIBUTELIST__(x)  
#endif

#ifdef _MSC_VER  /* Can only support "linkonce" vars with GCC */
#define __attribute__(X)
#define __asm__(X)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define __MSALIGN__(X) __declspec(align(X))
#else
#define __MSALIGN__(X)
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4101)
#pragma warning(disable: 4133)
#pragma warning(disable: 4146)
#endif//_MSC_VER


/* Global Declarations */

/* Types Declarations */
struct l_unnamed_1;
struct l_struct____rpsl_node_info_struct;
struct l_struct____rpsl_entry_desc_struct;
struct l_struct____rpsl_type_info_struct;
struct l_struct____rpsl_params_info_struct;
struct l_struct____rpsl_shader_ref_struct;
struct l_struct____rpsl_pipeline_info_struct;
struct l_struct____rpsl_pipeline_field_info_struct;
struct l_struct____rpsl_pipeline_res_binding_info_struct;
struct l_struct____rpsl_module_info_struct;
struct l_struct_struct_OC_RpsTypeInfo;
struct l_struct_struct_OC_RpsParameterDesc;
struct l_struct_struct_OC_RpsNodeDesc;
struct l_struct_struct_OC_RpslEntry;
struct SubresourceRange;
struct texture;
struct ResourceDesc;
struct Args;
struct buffer;
struct RpsViewport;

/* Function definitions */
typedef void l_fptr_1(uint32_t, uint8_t**, uint32_t);


/* Types Definitions */

/** RPS Built-In Types Begin **/

typedef enum _RpsFormat {
    _RPS_FORMAT_UNKNOWN = 0,
    _RPS_FORMAT_R32G32B32A32_TYPELESS,
    _RPS_FORMAT_R32G32B32A32_FLOAT,
    _RPS_FORMAT_R32G32B32A32_UINT,
    _RPS_FORMAT_R32G32B32A32_SINT,
    _RPS_FORMAT_R32G32B32_TYPELESS,
    _RPS_FORMAT_R32G32B32_FLOAT,
    _RPS_FORMAT_R32G32B32_UINT,
    _RPS_FORMAT_R32G32B32_SINT,
    _RPS_FORMAT_R16G16B16A16_TYPELESS,
    _RPS_FORMAT_R16G16B16A16_FLOAT,
    _RPS_FORMAT_R16G16B16A16_UNORM,
    _RPS_FORMAT_R16G16B16A16_UINT,
    _RPS_FORMAT_R16G16B16A16_SNORM,
    _RPS_FORMAT_R16G16B16A16_SINT,
    _RPS_FORMAT_R32G32_TYPELESS,
    _RPS_FORMAT_R32G32_FLOAT,
    _RPS_FORMAT_R32G32_UINT,
    _RPS_FORMAT_R32G32_SINT,
    _RPS_FORMAT_R32G8X24_TYPELESS,
    _RPS_FORMAT_D32_FLOAT_S8X24_UINT,
    _RPS_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    _RPS_FORMAT_X32_TYPELESS_G8X24_UINT,
    _RPS_FORMAT_R10G10B10A2_TYPELESS,
    _RPS_FORMAT_R10G10B10A2_UNORM,
    _RPS_FORMAT_R10G10B10A2_UINT,
    _RPS_FORMAT_R11G11B10_FLOAT,
    _RPS_FORMAT_R8G8B8A8_TYPELESS,
    _RPS_FORMAT_R8G8B8A8_UNORM,
    _RPS_FORMAT_R8G8B8A8_UNORM_SRGB,
    _RPS_FORMAT_R8G8B8A8_UINT,
    _RPS_FORMAT_R8G8B8A8_SNORM,
    _RPS_FORMAT_R8G8B8A8_SINT,
    _RPS_FORMAT_R16G16_TYPELESS,
    _RPS_FORMAT_R16G16_FLOAT,
    _RPS_FORMAT_R16G16_UNORM,
    _RPS_FORMAT_R16G16_UINT,
    _RPS_FORMAT_R16G16_SNORM,
    _RPS_FORMAT_R16G16_SINT,
    _RPS_FORMAT_R32_TYPELESS,
    _RPS_FORMAT_D32_FLOAT,
    _RPS_FORMAT_R32_FLOAT,
    _RPS_FORMAT_R32_UINT,
    _RPS_FORMAT_R32_SINT,
    _RPS_FORMAT_R24G8_TYPELESS,
    _RPS_FORMAT_D24_UNORM_S8_UINT,
    _RPS_FORMAT_R24_UNORM_X8_TYPELESS,
    _RPS_FORMAT_X24_TYPELESS_G8_UINT,
    _RPS_FORMAT_R8G8_TYPELESS,
    _RPS_FORMAT_R8G8_UNORM,
    _RPS_FORMAT_R8G8_UINT,
    _RPS_FORMAT_R8G8_SNORM,
    _RPS_FORMAT_R8G8_SINT,
    _RPS_FORMAT_R16_TYPELESS,
    _RPS_FORMAT_R16_FLOAT,
    _RPS_FORMAT_D16_UNORM,
    _RPS_FORMAT_R16_UNORM,
    _RPS_FORMAT_R16_UINT,
    _RPS_FORMAT_R16_SNORM,
    _RPS_FORMAT_R16_SINT,
    _RPS_FORMAT_R8_TYPELESS,
    _RPS_FORMAT_R8_UNORM,
    _RPS_FORMAT_R8_UINT,
    _RPS_FORMAT_R8_SNORM,
    _RPS_FORMAT_R8_SINT,
    _RPS_FORMAT_A8_UNORM,
    _RPS_FORMAT_R1_UNORM,
    _RPS_FORMAT_R9G9B9E5_SHAREDEXP,
    _RPS_FORMAT_R8G8_B8G8_UNORM,
    _RPS_FORMAT_G8R8_G8B8_UNORM,
    _RPS_FORMAT_BC1_TYPELESS,
    _RPS_FORMAT_BC1_UNORM,
    _RPS_FORMAT_BC1_UNORM_SRGB,
    _RPS_FORMAT_BC2_TYPELESS,
    _RPS_FORMAT_BC2_UNORM,
    _RPS_FORMAT_BC2_UNORM_SRGB,
    _RPS_FORMAT_BC3_TYPELESS,
    _RPS_FORMAT_BC3_UNORM,
    _RPS_FORMAT_BC3_UNORM_SRGB,
    _RPS_FORMAT_BC4_TYPELESS,
    _RPS_FORMAT_BC4_UNORM,
    _RPS_FORMAT_BC4_SNORM,
    _RPS_FORMAT_BC5_TYPELESS,
    _RPS_FORMAT_BC5_UNORM,
    _RPS_FORMAT_BC5_SNORM,
    _RPS_FORMAT_B5G6R5_UNORM,
    _RPS_FORMAT_B5G5R5A1_UNORM,
    _RPS_FORMAT_B8G8R8A8_UNORM,
    _RPS_FORMAT_B8G8R8X8_UNORM,
    _RPS_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    _RPS_FORMAT_B8G8R8A8_TYPELESS,
    _RPS_FORMAT_B8G8R8A8_UNORM_SRGB,
    _RPS_FORMAT_B8G8R8X8_TYPELESS,
    _RPS_FORMAT_B8G8R8X8_UNORM_SRGB,
    _RPS_FORMAT_BC6H_TYPELESS,
    _RPS_FORMAT_BC6H_UF16,
    _RPS_FORMAT_BC6H_SF16,
    _RPS_FORMAT_BC7_TYPELESS,
    _RPS_FORMAT_BC7_UNORM,
    _RPS_FORMAT_BC7_UNORM_SRGB,
    _RPS_FORMAT_AYUV,
    _RPS_FORMAT_Y410,
    _RPS_FORMAT_Y416,
    _RPS_FORMAT_NV12,
    _RPS_FORMAT_P010,
    _RPS_FORMAT_P016,
    _RPS_FORMAT_420_OPAQUE,
    _RPS_FORMAT_YUY2,
    _RPS_FORMAT_Y210,
    _RPS_FORMAT_Y216,
    _RPS_FORMAT_NV11,
    _RPS_FORMAT_AI44,
    _RPS_FORMAT_IA44,
    _RPS_FORMAT_P8,
    _RPS_FORMAT_A8P8,
    _RPS_FORMAT_B4G4R4A4_UNORM,

    _RPS_FORMAT_COUNT,

    _RPS_FORMAT_FORCE_INT32 = 0x7FFFFFFF,
} _RpsFormat;

typedef enum _RpsResourceType
{
    _RPS_RESOURCE_TYPE_BUFFER = 0,
    _RPS_RESOURCE_TYPE_IMAGE_1D,
    _RPS_RESOURCE_TYPE_IMAGE_2D,
    _RPS_RESOURCE_TYPE_IMAGE_3D,
    _RPS_RESOURCE_TYPE_UNKNOWN,

    _RPS_RESOURCE_TYPE_FORCE_INT32 = 0x7FFFFFFF,
} _RpsResourceType;

typedef enum _RpsResourceFlags
{
    RPS_RESOURCE_FLAG_NONE                        = 0,
    RPS_RESOURCE_CUBEMAP_COMPATIBLE_BIT           = (1 << 1),
    RPS_RESOURCE_ROWMAJOR_IMAGE_BIT               = (1 << 2),
    RPS_RESOURCE_PREFER_GPU_LOCAL_CPU_VISIBLE_BIT = (1 << 3),
    RPS_RESOURCE_PREFER_DEDICATED_BIT             = (1 << 4),
    RPS_RESOURCE_PERSISTENT_BIT                   = (1 << 15),
} _RpsResourceFlags;


typedef struct SubresourceRange {
  uint16_t base_mip_level;
  uint16_t mip_level_count;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} SubresourceRange;

typedef struct texture {
  uint32_t Resource;
  _RpsFormat Format;
  uint32_t TemporalLayer;
  uint32_t Flags;
  SubresourceRange SubresourceRange;
  float MinLodClamp;
  uint32_t ComponentMapping;
} texture;

typedef struct buffer {
  uint32_t Resource;
  _RpsFormat Format;
  uint32_t TemporalLayer;
  uint32_t Flags;
  uint64_t Offset;
  uint64_t SizeInBytes;
  uint32_t StructureByteStride;
} buffer;

typedef struct ResourceDesc {
  _RpsResourceType Type;
  uint32_t TemporalLayers;
  _RpsResourceFlags Flags;
  uint32_t Width;
  uint32_t Height;
  uint32_t DepthOrArraySize;
  uint32_t MipLevels;
  _RpsFormat Format;
  uint32_t SampleCount;
} ResourceDesc;

typedef struct RpsViewport {
  float x;
  float y;
  float width;
  float height;
  float minZ;
  float maxZ;
} RpsViewport;

typedef struct ShaderModule {
  uint32_t h;
} ShaderModule;

typedef struct Pipeline {
  uint32_t h;
} Pipeline;
/** RPS Built-In Types End **/

struct l_unnamed_1 {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
};
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_node_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
  uint32_t field4;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_entry_desc_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
  uint8_t* field4;
  uint8_t* field5;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_type_info_struct {
  uint8_t field0;
  uint8_t field1;
  uint8_t field2;
  uint8_t field3;
  uint32_t field4;
  uint32_t field5;
  uint32_t field6;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_params_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
  uint32_t field4;
  uint16_t field5;
  uint16_t field6;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_shader_ref_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_pipeline_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_pipeline_field_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
  uint32_t field4;
  uint32_t field5;
  uint32_t field6;
  uint32_t field7;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_pipeline_res_binding_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
struct l_array_179_uint8_t {
  uint8_t array[179];
};
struct l_array_5_struct_AC_l_struct____rpsl_node_info_struct {
  struct l_struct____rpsl_node_info_struct array[5];
};
struct l_array_7_struct_AC_l_struct____rpsl_type_info_struct {
  struct l_struct____rpsl_type_info_struct array[7];
};
struct l_array_15_struct_AC_l_struct____rpsl_params_info_struct {
  struct l_struct____rpsl_params_info_struct array[15];
};
struct l_array_2_struct_AC_l_struct____rpsl_entry_desc_struct {
  struct l_struct____rpsl_entry_desc_struct array[2];
};
struct l_array_1_struct_AC_l_struct____rpsl_shader_ref_struct {
  struct l_struct____rpsl_shader_ref_struct array[1];
};
struct l_array_1_struct_AC_l_struct____rpsl_pipeline_info_struct {
  struct l_struct____rpsl_pipeline_info_struct array[1];
};
struct l_array_1_struct_AC_l_struct____rpsl_pipeline_field_info_struct {
  struct l_struct____rpsl_pipeline_field_info_struct array[1];
};
struct l_array_1_struct_AC_l_struct____rpsl_pipeline_res_binding_info_struct {
  struct l_struct____rpsl_pipeline_res_binding_info_struct array[1];
};
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct l_struct____rpsl_module_info_struct {
  uint32_t field0;
  uint32_t field1;
  uint32_t field2;
  uint32_t field3;
  uint32_t field4;
  uint32_t field5;
  uint32_t field6;
  uint32_t field7;
  uint32_t field8;
  uint32_t field9;
  uint32_t field10;
  uint32_t field11;
  uint32_t field12;
  struct l_array_179_uint8_t* field13;
  struct l_array_5_struct_AC_l_struct____rpsl_node_info_struct* field14;
  struct l_array_7_struct_AC_l_struct____rpsl_type_info_struct* field15;
  struct l_array_15_struct_AC_l_struct____rpsl_params_info_struct* field16;
  struct l_array_2_struct_AC_l_struct____rpsl_entry_desc_struct* field17;
  struct l_array_1_struct_AC_l_struct____rpsl_shader_ref_struct* field18;
  struct l_array_1_struct_AC_l_struct____rpsl_pipeline_info_struct* field19;
  struct l_array_1_struct_AC_l_struct____rpsl_pipeline_field_info_struct* field20;
  struct l_array_1_struct_AC_l_struct____rpsl_pipeline_res_binding_info_struct* field21;
  uint32_t field22;
} __attribute__ ((packed));
#ifdef _MSC_VER
#pragma pack(pop)
#endif
struct l_struct_struct_OC_RpsTypeInfo {
  uint16_t field0;
  uint16_t field1;
};
struct l_struct_struct_OC_RpsParameterDesc {
  struct l_struct_struct_OC_RpsTypeInfo field0;
  uint32_t field1;
  struct l_unnamed_1* field2;
  uint8_t* field3;
  uint32_t field4;
};
struct l_struct_struct_OC_RpsNodeDesc {
  uint32_t field0;
  uint32_t field1;
  struct l_struct_struct_OC_RpsParameterDesc* field2;
  uint8_t* field3;
};
struct l_struct_struct_OC_RpslEntry {
  uint8_t* field0;
  l_fptr_1* field1;
  struct l_struct_struct_OC_RpsParameterDesc* field2;
  struct l_struct_struct_OC_RpsNodeDesc* field3;
  uint32_t field4;
  uint32_t field5;
};
struct Args {
  uint32_t frame_buffer_num;
  uint32_t camera_buffer_size;
};
struct l_array_14_uint8_t {
  uint8_t array[14];
};
struct l_array_18_uint8_t {
  uint8_t array[18];
};
struct l_array_16_uint8_t {
  uint8_t array[16];
};
struct l_array_2_struct_AC_l_struct_struct_OC_RpsParameterDesc {
  struct l_struct_struct_OC_RpsParameterDesc array[2];
};
struct l_array_20_uint8_t {
  uint8_t array[20];
};
struct l_array_2_uint8_t {
  uint8_t array[2];
};
struct l_array_7_uint8_t {
  uint8_t array[7];
};
struct l_array_4_struct_AC_l_struct_struct_OC_RpsParameterDesc {
  struct l_struct_struct_OC_RpsParameterDesc array[4];
};
struct l_array_10_uint8_t {
  uint8_t array[10];
};
struct l_array_3_uint8_t {
  uint8_t array[3];
};
struct l_array_4_uint8_t {
  uint8_t array[4];
};
struct l_array_13_uint8_t {
  uint8_t array[13];
};
struct l_array_6_uint8_t {
  uint8_t array[6];
};
struct l_array_4_struct_AC_l_struct_struct_OC_RpsNodeDesc {
  struct l_struct_struct_OC_RpsNodeDesc array[4];
};
struct l_array_9_uint8_t {
  uint8_t array[9];
};
struct l_array_11_uint8_t {
  uint8_t array[11];
};
struct l_array_5_uint8_t {
  uint8_t array[5];
};
struct l_array_2_uint8_t_KC_ {
  uint8_t* array[2];
};
struct l_array_4_uint8_t_KC_ {
  uint8_t* array[4];
};

/* External Global Variable Declarations */

/* Function Declarations */
static void _BA__PD_make_default_texture_view_from_desc_AE__AE_YA_PD_AUtexture_AE__AE_IUResourceDesc_AE__AE__AE_Z(struct texture*, uint32_t, struct ResourceDesc*) __ATTRIBUTELIST__((nothrow)) __asm__ ("?make_default_texture_view_from_desc@@YA?AUtexture@@IUResourceDesc@@@Z");
void rpsl_M_rps_main_Fn_rps_main(struct texture*, struct Args*) __ATTRIBUTELIST__((nothrow));
void ___rpsl_abort(uint32_t);
uint32_t ___rpsl_node_call(uint32_t, uint32_t, uint8_t**, uint32_t, uint32_t);
void ___rpsl_block_marker(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void ___rpsl_describe_handle(uint8_t*, uint32_t, uint32_t*, uint32_t);
uint32_t ___rpsl_create_resource(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void ___rpsl_name_resource(uint32_t, uint8_t*, uint32_t);
uint32_t ___rpsl_dxop_binary_i32(uint32_t, uint32_t, uint32_t);
void rpsl_M_rps_main_Fn_rps_main_wrapper(uint32_t, uint8_t**, uint32_t) __ATTRIBUTELIST__((noinline, nothrow));


/* Global Variable Definitions and Initialization */
static struct l_array_14_uint8_t _AE__AE_rps_Str0 = { "camera_buffer" };
static __MSALIGN__(4) struct l_array_5_struct_AC_l_struct____rpsl_node_info_struct ___rpsl_nodedefs_default_rendergraph __attribute__((aligned(4))) = { { { 0, 100, 0, 2, 1 }, { 1, 118, 2, 4, 1 }, { 2, 138, 6, 4, 1 }, { 3, 148, 10, 2, 1 }, { 0, 0, 0, 0, 0 } } };
static __MSALIGN__(4) struct l_array_2_struct_AC_l_struct____rpsl_entry_desc_struct ___rpsl_entries_default_rendergraph __attribute__((aligned(4))) = { { { 0, 154, 12, 2, ((uint8_t*)rpsl_M_rps_main_Fn_rps_main), ((uint8_t*)rpsl_M_rps_main_Fn_rps_main_wrapper) }, { 0, 0, 0, 0, ((uint8_t*)/*NULL*/0), ((uint8_t*)/*NULL*/0) } } };
static __MSALIGN__(4) struct l_array_7_struct_AC_l_struct____rpsl_type_info_struct ___rpsl_types_metadata_default_rendergraph __attribute__((aligned(4))) = { { { 6, 0, 0, 0, 0, 40, 8 }, { 6, 0, 0, 0, 0, 36, 4 }, { 3, 32, 0, 0, 0, 4, 4 }, { 4, 32, 0, 0, 0, 4, 4 }, { 5, 0, 0, 0, 0, 24, 4 }, { 5, 0, 0, 0, 0, 8, 4 }, { 0, 0, 0, 0, 0, 0, 0 } } };
static __MSALIGN__(4) struct l_array_15_struct_AC_l_struct____rpsl_params_info_struct ___rpsl_params_metadata_default_rendergraph __attribute__((aligned(4))) = { { { 37, 0, 538968064, -1, 0, 40, 0 }, { 53, 0, 8, -1, 0, 40, 40 }, { 67, 1, 805308928, -1, 0, 36, 0 }, { 69, 2, 0, -1, 0, 4, 36 }, { 76, 3, 0, -1, 0, 4, 40 }, { 78, 2, 0, -1, 0, 4, 44 }, { 80, 1, 128, -1, 0, 36, 0 }, { 84, 1, 2560, -1, 0, 36, 36 }, { 87, 4, 0, -1, 0, 24, 72 }, { 53, 0, 8, -1, 0, 40, 96 }, { 80, 1, 128, -1, 0, 36, 0 }, { 87, 4, 0, -1, 0, 24, 36 }, { 163, 1, 524288, -1, 0, 36, 0 }, { 174, 5, 0, -1, 0, 8, 36 }, { 0, 0, 0, 0, 0, 0, 0 } } };
static __MSALIGN__(4) struct l_array_1_struct_AC_l_struct____rpsl_shader_ref_struct ___rpsl_shader_refs_default_rendergraph __attribute__((aligned(4)));
static __MSALIGN__(4) struct l_array_1_struct_AC_l_struct____rpsl_pipeline_info_struct ___rpsl_pipelines_default_rendergraph __attribute__((aligned(4)));
static __MSALIGN__(4) struct l_array_1_struct_AC_l_struct____rpsl_pipeline_field_info_struct ___rpsl_pipeline_fields_default_rendergraph __attribute__((aligned(4)));
static __MSALIGN__(4) struct l_array_1_struct_AC_l_struct____rpsl_pipeline_res_binding_info_struct ___rpsl_pipeline_res_bindings_default_rendergraph __attribute__((aligned(4)));
__MSALIGN__(4) struct l_array_179_uint8_t ___rpsl_string_table_default_rendergraph __attribute__((aligned(4))) = { { 99u, 97u, 109u, 101u, 114u, 97u, 95u, 98u, 117u, 102u, 102u, 101u, 114u, 0, 100u, 115u, 0, 100u, 101u, 102u, 97u, 117u, 108u, 116u, 95u, 114u, 101u, 110u, 100u, 101u, 114u, 103u, 114u, 97u, 112u, 104u, 0, 99u, 97u, 109u, 101u, 114u, 97u, 95u, 114u, 101u, 115u, 111u, 117u, 114u, 99u, 101u, 0, 99u, 97u, 109u, 101u, 114u, 97u, 95u, 104u, 97u, 110u, 100u, 108u, 101u, 0, 116u, 0, 111u, 112u, 116u, 105u, 111u, 110u, 0, 100u, 0, 115u, 0, 100u, 115u, 116u, 0, 100u, 115u, 0, 100u, 115u, 116u, 95u, 118u, 105u, 101u, 119u, 112u, 111u, 114u, 116u, 0, 117u, 112u, 100u, 97u, 116u, 101u, 95u, 115u, 99u, 101u, 110u, 101u, 95u, 100u, 97u, 116u, 97u, 0, 99u, 108u, 101u, 97u, 114u, 95u, 100u, 101u, 112u, 116u, 104u, 95u, 115u, 116u, 101u, 110u, 99u, 105u, 108u, 0, 112u, 114u, 101u, 122u, 95u, 112u, 97u, 115u, 115u, 0, 105u, 109u, 103u, 117u, 105u, 0, 114u, 112u, 115u, 95u, 109u, 97u, 105u, 110u, 0, 98u, 97u, 99u, 107u, 98u, 117u, 102u, 102u, 101u, 114u, 0, 97u, 114u, 103u, 115u, 0 } };
__declspec(dllexport) __MSALIGN__(4) struct l_struct____rpsl_module_info_struct ___rpsl_module_info_default_rendergraph __attribute__((aligned(4))) = { 1297305682u, 3, 9, 17, 179, 4, 6, 14, 1, 0, 0, 0, 0, (&___rpsl_string_table_default_rendergraph), (&___rpsl_nodedefs_default_rendergraph), (&___rpsl_types_metadata_default_rendergraph), (&___rpsl_params_metadata_default_rendergraph), (&___rpsl_entries_default_rendergraph), (&___rpsl_shader_refs_default_rendergraph), (&___rpsl_pipelines_default_rendergraph), (&___rpsl_pipeline_fields_default_rendergraph), (&___rpsl_pipeline_res_bindings_default_rendergraph), 1297305682u };
static struct l_array_18_uint8_t _AE__AE_rps_Str2 = { "update_scene_data" };
static struct l_array_16_uint8_t _AE__AE_rps_Str3 = { "camera_resource" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr4 = { 538968064, 0, 0, 0 };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr6 = { 8, 1, 0, 0 };
static struct l_array_20_uint8_t _AE__AE_rps_Str8 = { "clear_depth_stencil" };
static struct l_array_2_uint8_t _AE__AE_rps_Str9 = { "t" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr10 = { 805308928, 0, 0, 0 };
static struct l_array_7_uint8_t _AE__AE_rps_Str11 = { "option" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr12;
static struct l_array_2_uint8_t _AE__AE_rps_Str13 = { "d" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr14 = { 0, 0, 28, 0 };
static struct l_array_2_uint8_t _AE__AE_rps_Str15 = { "s" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr16 = { 0, 0, 29, 0 };
static struct l_array_4_struct_AC_l_struct_struct_OC_RpsParameterDesc _AE__AE_rps_ParamDescArray17 = { { { { 36, 64 }, 0, (&_AE__AE_rps_ParamAttr10), ((&_AE__AE_rps_Str9.array[((int32_t)0)])), 4 }, { { 4, 0 }, 0, (&_AE__AE_rps_ParamAttr12), ((&_AE__AE_rps_Str11.array[((int32_t)0)])), 0 }, { { 4, 0 }, 0, (&_AE__AE_rps_ParamAttr14), ((&_AE__AE_rps_Str13.array[((int32_t)0)])), 0 }, { { 4, 0 }, 0, (&_AE__AE_rps_ParamAttr16), ((&_AE__AE_rps_Str15.array[((int32_t)0)])), 0 } } };
static struct l_array_10_uint8_t _AE__AE_rps_Str18 = { "prez_pass" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr20 = { 128, 0, 35, 0 };
static struct l_array_3_uint8_t _AE__AE_rps_Str21 = { "ds" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr22 = { 2560, 0, 36, 0 };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr24 = { 0, 0, 17, 0 };
static struct l_array_14_uint8_t _AE__AE_rps_Str25 = { "camera_handle" };
static struct l_array_2_struct_AC_l_struct_struct_OC_RpsParameterDesc _AE__AE_rps_ParamDescArray7 = { { { { 40, 65 }, 0, (&_AE__AE_rps_ParamAttr4), ((&_AE__AE_rps_Str3.array[((int32_t)0)])), 4 }, { { 40, 65 }, 0, (&_AE__AE_rps_ParamAttr6), ((&_AE__AE_rps_Str25.array[((int32_t)0)])), 4 } } };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr26 = { 8, 1, 0, 0 };
static struct l_array_6_uint8_t _AE__AE_rps_Str28 = { "imgui" };
static struct l_array_4_uint8_t _AE__AE_rps_Str29 = { "dst" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr30 = { 128, 0, 35, 0 };
static struct l_array_13_uint8_t _AE__AE_rps_Str31 = { "dst_viewport" };
static struct l_array_4_struct_AC_l_struct_struct_OC_RpsParameterDesc _AE__AE_rps_ParamDescArray27 = { { { { 36, 64 }, 0, (&_AE__AE_rps_ParamAttr20), ((&_AE__AE_rps_Str29.array[((int32_t)0)])), 4 }, { { 36, 64 }, 0, (&_AE__AE_rps_ParamAttr22), ((&_AE__AE_rps_Str21.array[((int32_t)0)])), 4 }, { { 24, 0 }, 0, (&_AE__AE_rps_ParamAttr24), ((&_AE__AE_rps_Str31.array[((int32_t)0)])), 0 }, { { 40, 65 }, 0, (&_AE__AE_rps_ParamAttr26), ((&_AE__AE_rps_Str25.array[((int32_t)0)])), 4 } } };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr32 = { 0, 0, 17, 0 };
static struct l_array_2_struct_AC_l_struct_struct_OC_RpsParameterDesc _AE__AE_rps_ParamDescArray33 = { { { { 36, 64 }, 0, (&_AE__AE_rps_ParamAttr30), ((&_AE__AE_rps_Str29.array[((int32_t)0)])), 4 }, { { 24, 0 }, 0, (&_AE__AE_rps_ParamAttr32), ((&_AE__AE_rps_Str31.array[((int32_t)0)])), 0 } } };
__declspec(dllexport) struct l_array_4_struct_AC_l_struct_struct_OC_RpsNodeDesc NodeDecls_default_rendergraph = { { { 1, 2, ((&_AE__AE_rps_ParamDescArray7.array[((int32_t)0)])), ((&_AE__AE_rps_Str2.array[((int32_t)0)])) }, { 1, 4, ((&_AE__AE_rps_ParamDescArray17.array[((int32_t)0)])), ((&_AE__AE_rps_Str8.array[((int32_t)0)])) }, { 1, 4, ((&_AE__AE_rps_ParamDescArray27.array[((int32_t)0)])), ((&_AE__AE_rps_Str18.array[((int32_t)0)])) }, { 1, 2, ((&_AE__AE_rps_ParamDescArray33.array[((int32_t)0)])), ((&_AE__AE_rps_Str28.array[((int32_t)0)])) } } };
static struct l_array_9_uint8_t _AE__AE_rps_Str34 = { "rps_main" };
static struct l_array_11_uint8_t _AE__AE_rps_Str35 = { "backbuffer" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr36 = { 524288, 0, 0, 0 };
static struct l_array_5_uint8_t _AE__AE_rps_Str37 = { "args" };
static struct l_unnamed_1 _AE__AE_rps_ParamAttr38;
static struct l_array_2_struct_AC_l_struct_struct_OC_RpsParameterDesc _AE__AE_rps_ParamDescArray39 = { { { { 36, 64 }, 0, (&_AE__AE_rps_ParamAttr36), ((&_AE__AE_rps_Str35.array[((int32_t)0)])), 4 }, { { 8, 0 }, 0, (&_AE__AE_rps_ParamAttr38), ((&_AE__AE_rps_Str37.array[((int32_t)0)])), 0 } } };
struct l_struct_struct_OC_RpslEntry rpsl_M_default_rendergraph_E_rps_main_AE_value = { ((&_AE__AE_rps_Str34.array[((int32_t)0)])), rpsl_M_rps_main_Fn_rps_main_wrapper, ((&_AE__AE_rps_ParamDescArray39.array[((int32_t)0)])), ((&NodeDecls_default_rendergraph.array[((int32_t)0)])), 2, 4 };
__declspec(dllexport) struct l_struct_struct_OC_RpslEntry* rpsl_M_default_rendergraph_E_rps_main = (&rpsl_M_default_rendergraph_E_rps_main_AE_value);
__declspec(dllexport) struct l_struct_struct_OC_RpslEntry** rpsl_M_default_rendergraph_E_rps_main_pp = (&rpsl_M_default_rendergraph_E_rps_main);


/* LLVM Intrinsic Builtin Function Bodies */
static __forceinline uint32_t llvm_add_u32(uint32_t a, uint32_t b) {
  uint32_t r = a + b;
  return r;
}
static __forceinline uint32_t llvm_lshr_u32(uint32_t a, uint32_t b) {
  uint32_t r = a >> b;
  return r;
}
static __forceinline struct texture llvm_ctor_struct_struct_OC_texture(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3, struct SubresourceRange x4, float x5, uint32_t x6) {
  struct texture r;
  r.Resource = x0;
  r.Format = x1;
  r.TemporalLayer = x2;
  r.Flags = x3;
  r.SubresourceRange = x4;
  r.MinLodClamp = x5;
  r.ComponentMapping = x6;
  return r;
}
static __forceinline struct SubresourceRange llvm_ctor_struct_struct_OC_SubresourceRange(uint16_t x0, uint16_t x1, uint32_t x2, uint32_t x3) {
  struct SubresourceRange r;
  r.base_mip_level = x0;
  r.mip_level_count = x1;
  r.base_array_layer = x2;
  r.array_layer_count = x3;
  return r;
}


/* Function Bodies */

#line 151 "./___rpsl_builtin_header_.rpsl"
static void _BA__PD_make_default_texture_view_from_desc_AE__AE_YA_PD_AUtexture_AE__AE_IUResourceDesc_AE__AE__AE_Z(struct texture* agg_2e_result, uint32_t resourceHdl, struct ResourceDesc* desc) {

  struct {
    uint32_t _1;
    uint32_t _2;
    uint32_t _3;
  } _llvm_cbe_tmps;

  struct {
    uint32_t _4;
    uint32_t _4__PHI_TEMPORARY;
  } _llvm_cbe_phi_tmps = {0};

  #line 151 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._1 = *((&desc->MipLevels));
#line 151 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._2 = *((&desc->Type));
#line 151 "./___rpsl_builtin_header_.rpsl"
#line 151 "./___rpsl_builtin_header_.rpsl"
  if ((((_llvm_cbe_tmps._2 == 4u)&1))) {
#line 151 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps._4__PHI_TEMPORARY = 1;   /* for PHI node */
#line 151 "./___rpsl_builtin_header_.rpsl"
    goto llvm_cbe_temp__5;
#line 151 "./___rpsl_builtin_header_.rpsl"
  } else {
#line 151 "./___rpsl_builtin_header_.rpsl"
    goto llvm_cbe_temp__6;
#line 151 "./___rpsl_builtin_header_.rpsl"
  }

llvm_cbe_temp__6:
#line 151 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._3 = *((&desc->DepthOrArraySize));
#line 151 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps._4__PHI_TEMPORARY = _llvm_cbe_tmps._3;   /* for PHI node */
#line 151 "./___rpsl_builtin_header_.rpsl"
  goto llvm_cbe_temp__5;
#line 151 "./___rpsl_builtin_header_.rpsl"

llvm_cbe_temp__5:
#line 151 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps._4 = _llvm_cbe_phi_tmps._4__PHI_TEMPORARY;
  #line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->Resource)) = resourceHdl;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->Format)) = 0;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->TemporalLayer)) = 0;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->Flags)) = 0;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->SubresourceRange.base_mip_level)) = 0;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->SubresourceRange.mip_level_count)) = (((uint16_t)_llvm_cbe_tmps._1));
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->SubresourceRange.base_array_layer)) = 0;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->SubresourceRange.array_layer_count)) = _llvm_cbe_phi_tmps._4;
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->MinLodClamp)) = ((float)(0.000000e+00));
#line 153 "./___rpsl_builtin_header_.rpsl"
  *((&agg_2e_result->ComponentMapping)) = 50462976;
#line 153 "./___rpsl_builtin_header_.rpsl"
}


#line 6 "default_rendergraph.rpsl"
void rpsl_M_rps_main_Fn_rps_main(struct texture* backbuffer, struct Args* args) {
    struct buffer camera_buffer;    /* Address-exposed local */
    struct RpsViewport dst_viewport;    /* Address-exposed local */
    __MSALIGN__(8) struct texture ds __attribute__((aligned(8)));    /* Address-exposed local */

  struct {
    __MSALIGN__(8) struct texture _7 __attribute__((aligned(8)));    /* Address-exposed local */
    __MSALIGN__(8) struct texture _8 __attribute__((aligned(8)));    /* Address-exposed local */
    uint32_t* _9;
    uint32_t _10;
    uint32_t _11;
    uint32_t _12;
    uint32_t _13;
    uint16_t _14;
    uint16_t _15;
    uint32_t _16;
    uint32_t _17;
    float _18;
    uint32_t _19;
    __MSALIGN__(8) struct ResourceDesc _20 __attribute__((aligned(8)));    /* Address-exposed local */
    uint32_t _21;
    uint32_t _22;
    uint32_t _23;
    uint32_t _24;
    uint32_t _25;
    struct l_array_2_uint8_t_KC_ _26;    /* Address-exposed local */
    uint8_t** _2e_sub;
    uint32_t _27;
    uint32_t _28;
    uint32_t _29;
    uint32_t _30;
    uint32_t _31;
    uint32_t UMin;
    uint32_t _32;
    uint32_t* _33;
    uint32_t* _34;
    uint32_t* _35;
    uint32_t* _36;
    uint16_t* _37;
    uint16_t* _38;
    uint32_t* _39;
    uint32_t* _40;
    float* _41;
    uint32_t* _42;
    struct l_array_4_uint8_t_KC_* _43;
    uint8_t** _2e_sub_2e_2;
    uint32_t* _44;
    float* _45;
    uint32_t* _46;
    uint32_t _47;
    struct texture* _2e_tvd_2e_0;
    uint32_t _48;
    struct texture _49;
    uint32_t _50;
    struct texture _51;
    uint32_t _52;
    struct texture _53;
    uint32_t _54;
    struct texture _55;
    uint16_t _56;
    struct texture _57;
    uint16_t _58;
    struct texture _59;
    uint32_t _60;
    struct texture _61;
    uint32_t _62;
    struct texture _63;
    float _64;
    struct texture _65;
    uint32_t _66;
    struct texture _67;
    uint32_t _68;
    uint32_t _69;
    uint32_t _70;
    uint16_t _71;
    uint16_t _72;
    uint32_t _73;
    uint32_t _74;
    float _75;
    uint32_t _76;
    struct l_array_4_uint8_t_KC_* _77;
    uint8_t** _2e_sub_2e_3;
    uint32_t _78;
    struct l_array_2_uint8_t_KC_* _79;
    uint8_t** _2e_sub_2e_4;
    uint32_t _80;
  } _llvm_cbe_tmps;

  struct {
    uint32_t mips_2e_i_2e_i_2e_041;
    uint32_t mips_2e_i_2e_i_2e_041__PHI_TEMPORARY;
    uint32_t d_2e_i_2e_i_2e_040;
    uint32_t d_2e_i_2e_i_2e_040__PHI_TEMPORARY;
    uint32_t h_2e_i_2e_i_2e_039;
    uint32_t h_2e_i_2e_i_2e_039__PHI_TEMPORARY;
    uint32_t w_2e_i_2e_i_2e_038;
    uint32_t w_2e_i_2e_i_2e_038__PHI_TEMPORARY;
    uint32_t mips_2e_i_2e_i_2e_0_2e_lcssa;
    uint32_t mips_2e_i_2e_i_2e_0_2e_lcssa__PHI_TEMPORARY;
  } _llvm_cbe_phi_tmps = {0};

#line 6 "default_rendergraph.rpsl"
  ___rpsl_block_marker(0, 0, 2, 4, -1, 0, -1);
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._9 = (&_llvm_cbe_tmps._7.Resource);
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._10 = *((&backbuffer->Resource));
#line 6 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._9 = _llvm_cbe_tmps._10;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._11 = *((&backbuffer->Format));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.Format)) = _llvm_cbe_tmps._11;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._12 = *((&backbuffer->TemporalLayer));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.TemporalLayer)) = _llvm_cbe_tmps._12;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._13 = *((&backbuffer->Flags));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.Flags)) = _llvm_cbe_tmps._13;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._14 = *((&backbuffer->SubresourceRange.base_mip_level));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.SubresourceRange.base_mip_level)) = _llvm_cbe_tmps._14;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._15 = *((&backbuffer->SubresourceRange.mip_level_count));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.SubresourceRange.mip_level_count)) = _llvm_cbe_tmps._15;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._16 = *((&backbuffer->SubresourceRange.base_array_layer));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.SubresourceRange.base_array_layer)) = _llvm_cbe_tmps._16;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._17 = *((&backbuffer->SubresourceRange.array_layer_count));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.SubresourceRange.array_layer_count)) = _llvm_cbe_tmps._17;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._18 = *((&backbuffer->MinLodClamp));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.MinLodClamp)) = _llvm_cbe_tmps._18;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._19 = *((&backbuffer->ComponentMapping));
#line 6 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._7.ComponentMapping)) = _llvm_cbe_tmps._19;
  #line 8 "default_rendergraph.rpsl"
  ___rpsl_describe_handle((((uint8_t*)(&_llvm_cbe_tmps._20))), 36, _llvm_cbe_tmps._9, 1);
#line 8 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._21 = *((&_llvm_cbe_tmps._20.Width));
#line 8 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._22 = *((&_llvm_cbe_tmps._20.Height));
#line 9 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._23 = *((&args->frame_buffer_num));
#line 9 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._24 = *((&args->camera_buffer_size));
#line 9 "default_rendergraph.rpsl"
  #line 9 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._25 = ___rpsl_create_resource(1, 8, 0, _llvm_cbe_tmps._24, 0, 1, 1, 1, 0, _llvm_cbe_tmps._23, 0);
  #line 9 "default_rendergraph.rpsl"
  ___rpsl_name_resource(_llvm_cbe_tmps._25, ((&_AE__AE_rps_Str0.array[((int32_t)0)])), 13);
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.Resource)) = _llvm_cbe_tmps._25;
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.Format)) = 0;
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.TemporalLayer)) = 0;
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.Flags)) = 0;
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.Offset)) = UINT64_C(0);
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.SizeInBytes)) = (((uint64_t)(uint32_t)_llvm_cbe_tmps._24));
#line 9 "default_rendergraph.rpsl"
  *((&camera_buffer.StructureByteStride)) = 0;
#line 10 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._2e_sub = (&_llvm_cbe_tmps._26.array[((int32_t)0)]);
#line 10 "default_rendergraph.rpsl"
  *(((struct buffer**)_llvm_cbe_tmps._2e_sub)) = (&camera_buffer);
#line 10 "default_rendergraph.rpsl"
  *(((struct buffer**)((&_llvm_cbe_tmps._26.array[((int32_t)1)])))) = (&camera_buffer);
#line 10 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._27 = ___rpsl_node_call(0, 2, _llvm_cbe_tmps._2e_sub, 0, 0);
#line 11 "default_rendergraph.rpsl"
  #line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.x)) = ((float)(0.000000e+00));
#line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.y)) = ((float)(0.000000e+00));
#line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.width)) = (((float)(uint32_t)_llvm_cbe_tmps._21));
#line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.height)) = (((float)(uint32_t)_llvm_cbe_tmps._22));
#line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.minZ)) = ((float)(0.000000e+00));
#line 11 "default_rendergraph.rpsl"
  *((&dst_viewport.maxZ)) = ((float)(1.000000e+00));
  #line 228 "./___rpsl_builtin_header_.rpsl"
#line 228 "./___rpsl_builtin_header_.rpsl"
  if ((((((uint32_t)(_llvm_cbe_tmps._22 | _llvm_cbe_tmps._21)) > ((uint32_t)1u))&1))) {
#line 228 "./___rpsl_builtin_header_.rpsl"
    goto _2e_lr_2e_ph_2e_preheader;
#line 228 "./___rpsl_builtin_header_.rpsl"
  } else {
#line 228 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_0_2e_lcssa__PHI_TEMPORARY = 1;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
    goto _1__3f_create_tex2d_40__40_YA_3f_AUtexture_40__40_IIIIIIIII_40_Z_2e_exit;
#line 228 "./___rpsl_builtin_header_.rpsl"
  }

_2e_lr_2e_ph_2e_preheader:
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_041__PHI_TEMPORARY = 1;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.d_2e_i_2e_i_2e_040__PHI_TEMPORARY = 1;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.h_2e_i_2e_i_2e_039__PHI_TEMPORARY = _llvm_cbe_tmps._22;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.w_2e_i_2e_i_2e_038__PHI_TEMPORARY = _llvm_cbe_tmps._21;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
  goto _2e_lr_2e_ph;
#line 228 "./___rpsl_builtin_header_.rpsl"

#line 228 "./___rpsl_builtin_header_.rpsl"
  do {     /* Syntactic loop '.lr.ph' to make GCC happy */
_2e_lr_2e_ph:
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_041 = _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_041__PHI_TEMPORARY;
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.d_2e_i_2e_i_2e_040 = _llvm_cbe_phi_tmps.d_2e_i_2e_i_2e_040__PHI_TEMPORARY;
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.h_2e_i_2e_i_2e_039 = _llvm_cbe_phi_tmps.h_2e_i_2e_i_2e_039__PHI_TEMPORARY;
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.w_2e_i_2e_i_2e_038 = _llvm_cbe_phi_tmps.w_2e_i_2e_i_2e_038__PHI_TEMPORARY;
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._28 = llvm_add_u32(_llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_041, 1);
  #line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._29 = llvm_lshr_u32(_llvm_cbe_phi_tmps.w_2e_i_2e_i_2e_038, 1);
  #line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._30 = llvm_lshr_u32(_llvm_cbe_phi_tmps.h_2e_i_2e_i_2e_039, 1);
  #line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps._31 = llvm_lshr_u32(_llvm_cbe_phi_tmps.d_2e_i_2e_i_2e_040, 1);
  #line 228 "./___rpsl_builtin_header_.rpsl"
#line 228 "./___rpsl_builtin_header_.rpsl"
  if ((((((uint32_t)((_llvm_cbe_tmps._30 | _llvm_cbe_tmps._29) | _llvm_cbe_tmps._31)) > ((uint32_t)1u))&1))) {
#line 228 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_041__PHI_TEMPORARY = _llvm_cbe_tmps._28;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps.d_2e_i_2e_i_2e_040__PHI_TEMPORARY = _llvm_cbe_tmps._31;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps.h_2e_i_2e_i_2e_039__PHI_TEMPORARY = _llvm_cbe_tmps._30;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
    _llvm_cbe_phi_tmps.w_2e_i_2e_i_2e_038__PHI_TEMPORARY = _llvm_cbe_tmps._29;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
    goto _2e_lr_2e_ph;
#line 228 "./___rpsl_builtin_header_.rpsl"
  } else {
#line 228 "./___rpsl_builtin_header_.rpsl"
    goto _1__3f_create_tex2d_40__40_YA_3f_AUtexture_40__40_IIIIIIIII_40_Z_2e_exit_2e_loopexit;
#line 228 "./___rpsl_builtin_header_.rpsl"
  }

  } while (1); /* end of syntactic loop '.lr.ph' */
_1__3f_create_tex2d_40__40_YA_3f_AUtexture_40__40_IIIIIIIII_40_Z_2e_exit_2e_loopexit:
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_0_2e_lcssa__PHI_TEMPORARY = _llvm_cbe_tmps._28;   /* for PHI node */
#line 228 "./___rpsl_builtin_header_.rpsl"
  goto _1__3f_create_tex2d_40__40_YA_3f_AUtexture_40__40_IIIIIIIII_40_Z_2e_exit;
#line 228 "./___rpsl_builtin_header_.rpsl"

_1__3f_create_tex2d_40__40_YA_3f_AUtexture_40__40_IIIIIIIII_40_Z_2e_exit:
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_0_2e_lcssa = _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_0_2e_lcssa__PHI_TEMPORARY;
#line 228 "./___rpsl_builtin_header_.rpsl"
  _llvm_cbe_tmps.UMin = ___rpsl_dxop_binary_i32(40, 1, _llvm_cbe_phi_tmps.mips_2e_i_2e_i_2e_0_2e_lcssa);
  #line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._32 = ___rpsl_create_resource(3, 0, 44, _llvm_cbe_tmps._21, _llvm_cbe_tmps._22, 1, _llvm_cbe_tmps.UMin, 1, 0, 1, 1);
  #line 232 "./___rpsl_builtin_header_.rpsl"
  ___rpsl_name_resource(_llvm_cbe_tmps._32, ((&_AE__AE_rps_Str21.array[((int32_t)0)])), 2);
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._33 = (&ds.Resource);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._33 = _llvm_cbe_tmps._32;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._34 = (&ds.Format);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._34 = 0;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._35 = (&ds.TemporalLayer);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._35 = 0;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._36 = (&ds.Flags);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._36 = 0;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._37 = (&ds.SubresourceRange.base_mip_level);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._37 = 0;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._38 = (&ds.SubresourceRange.mip_level_count);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._38 = (((uint16_t)_llvm_cbe_tmps.UMin));
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._39 = (&ds.SubresourceRange.base_array_layer);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._39 = 0;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._40 = (&ds.SubresourceRange.array_layer_count);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._40 = 1;
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._41 = (&ds.MinLodClamp);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._41 = ((float)(0.000000e+00));
#line 12 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._42 = (&ds.ComponentMapping);
#line 12 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._42 = 50462976;
  #line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._43 = (struct l_array_4_uint8_t_KC_*) alloca(sizeof(struct l_array_4_uint8_t_KC_));
#line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._2e_sub_2e_2 = (&(*_llvm_cbe_tmps._43).array[((int32_t)0)]);
#line 13 "default_rendergraph.rpsl"
  *(((struct texture**)_llvm_cbe_tmps._2e_sub_2e_2)) = (&ds);
#line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._44 = (uint32_t*) alloca(sizeof(uint32_t));
#line 13 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._44 = 6;
#line 13 "default_rendergraph.rpsl"
  *(((uint32_t**)((&(*_llvm_cbe_tmps._43).array[((int32_t)1)])))) = _llvm_cbe_tmps._44;
#line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._45 = (float*) alloca(sizeof(float));
#line 13 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._45 = ((float)(1.000000e+00));
#line 13 "default_rendergraph.rpsl"
  *(((float**)((&(*_llvm_cbe_tmps._43).array[((int32_t)2)])))) = _llvm_cbe_tmps._45;
#line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._46 = (uint32_t*) alloca(sizeof(uint32_t));
#line 13 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._46 = 0;
#line 13 "default_rendergraph.rpsl"
  *(((uint32_t**)((&(*_llvm_cbe_tmps._43).array[((int32_t)3)])))) = _llvm_cbe_tmps._46;
#line 13 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._47 = ___rpsl_node_call(1, 4, _llvm_cbe_tmps._2e_sub_2e_2, 0, 1);
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._2e_tvd_2e_0 = (struct texture*) alloca(sizeof(struct texture));
  #line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._48 = *_llvm_cbe_tmps._33;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._49 = llvm_ctor_struct_struct_OC_texture(0, 0, 0, 0, llvm_ctor_struct_struct_OC_SubresourceRange(0, 0, 0, 0), ((float)(0.000000e+00)), 0);
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._49.Resource = _llvm_cbe_tmps._48;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._50 = *_llvm_cbe_tmps._34;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._51 = _llvm_cbe_tmps._49;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._51.Format = _llvm_cbe_tmps._50;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._52 = *_llvm_cbe_tmps._35;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._53 = _llvm_cbe_tmps._51;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._53.TemporalLayer = _llvm_cbe_tmps._52;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._54 = *_llvm_cbe_tmps._36;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._55 = _llvm_cbe_tmps._53;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._55.Flags = _llvm_cbe_tmps._54;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._56 = *_llvm_cbe_tmps._37;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._57 = _llvm_cbe_tmps._55;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._57.SubresourceRange.base_mip_level = _llvm_cbe_tmps._56;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._58 = *_llvm_cbe_tmps._38;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._59 = _llvm_cbe_tmps._57;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._59.SubresourceRange.mip_level_count = _llvm_cbe_tmps._58;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._60 = *_llvm_cbe_tmps._39;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._61 = _llvm_cbe_tmps._59;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._61.SubresourceRange.base_array_layer = _llvm_cbe_tmps._60;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._62 = *_llvm_cbe_tmps._40;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._63 = _llvm_cbe_tmps._61;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._63.SubresourceRange.array_layer_count = _llvm_cbe_tmps._62;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._64 = *_llvm_cbe_tmps._41;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._65 = _llvm_cbe_tmps._63;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._65.MinLodClamp = _llvm_cbe_tmps._64;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._66 = *_llvm_cbe_tmps._42;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._67 = _llvm_cbe_tmps._65;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._67.ComponentMapping = _llvm_cbe_tmps._66;
#line 14 "default_rendergraph.rpsl"
  *_llvm_cbe_tmps._2e_tvd_2e_0 = _llvm_cbe_tmps._67;
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._2e_tvd_2e_0->Format)) = 45;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._68 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->Resource));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.Resource)) = _llvm_cbe_tmps._68;
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.Format)) = 45;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._69 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->TemporalLayer));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.TemporalLayer)) = _llvm_cbe_tmps._69;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._70 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->Flags));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.Flags)) = _llvm_cbe_tmps._70;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._71 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->SubresourceRange.base_mip_level));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.SubresourceRange.base_mip_level)) = _llvm_cbe_tmps._71;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._72 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->SubresourceRange.mip_level_count));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.SubresourceRange.mip_level_count)) = _llvm_cbe_tmps._72;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._73 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->SubresourceRange.base_array_layer));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.SubresourceRange.base_array_layer)) = _llvm_cbe_tmps._73;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._74 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->SubresourceRange.array_layer_count));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.SubresourceRange.array_layer_count)) = _llvm_cbe_tmps._74;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._75 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->MinLodClamp));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.MinLodClamp)) = _llvm_cbe_tmps._75;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._76 = *((&_llvm_cbe_tmps._2e_tvd_2e_0->ComponentMapping));
#line 14 "default_rendergraph.rpsl"
  *((&_llvm_cbe_tmps._8.ComponentMapping)) = _llvm_cbe_tmps._76;
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._77 = (struct l_array_4_uint8_t_KC_*) alloca(sizeof(struct l_array_4_uint8_t_KC_));
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._2e_sub_2e_3 = (&(*_llvm_cbe_tmps._77).array[((int32_t)0)]);
#line 14 "default_rendergraph.rpsl"
  *(((struct texture**)_llvm_cbe_tmps._2e_sub_2e_3)) = (&_llvm_cbe_tmps._7);
#line 14 "default_rendergraph.rpsl"
  *(((struct texture**)((&(*_llvm_cbe_tmps._77).array[((int32_t)1)])))) = (&_llvm_cbe_tmps._8);
#line 14 "default_rendergraph.rpsl"
  *(((struct RpsViewport**)((&(*_llvm_cbe_tmps._77).array[((int32_t)2)])))) = (&dst_viewport);
#line 14 "default_rendergraph.rpsl"
  *(((struct buffer**)((&(*_llvm_cbe_tmps._77).array[((int32_t)3)])))) = (&camera_buffer);
#line 14 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._78 = ___rpsl_node_call(2, 4, _llvm_cbe_tmps._2e_sub_2e_3, 0, 2);
#line 15 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._79 = (struct l_array_2_uint8_t_KC_*) alloca(sizeof(struct l_array_2_uint8_t_KC_));
#line 15 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._2e_sub_2e_4 = (&(*_llvm_cbe_tmps._79).array[((int32_t)0)]);
#line 15 "default_rendergraph.rpsl"
  *(((struct texture**)_llvm_cbe_tmps._2e_sub_2e_4)) = (&_llvm_cbe_tmps._7);
#line 15 "default_rendergraph.rpsl"
  *(((struct RpsViewport**)((&(*_llvm_cbe_tmps._79).array[((int32_t)1)])))) = (&dst_viewport);
#line 15 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._80 = ___rpsl_node_call(3, 2, _llvm_cbe_tmps._2e_sub_2e_4, 0, 3);
#line 16 "default_rendergraph.rpsl"
}


#line 6 "default_rendergraph.rpsl"
void rpsl_M_rps_main_Fn_rps_main_wrapper(uint32_t llvm_cbe_temp__81, uint8_t** llvm_cbe_temp__82, uint32_t llvm_cbe_temp__83) {

  struct {
    uint8_t* _84;
    struct texture* _85;
    struct texture* _86;
    uint8_t* _87;
  } _llvm_cbe_tmps;

  struct {
    struct texture* _88;
    struct texture* _88__PHI_TEMPORARY;
  } _llvm_cbe_phi_tmps = {0};

#line 6 "default_rendergraph.rpsl"
#line 6 "default_rendergraph.rpsl"
  if ((((llvm_cbe_temp__81 == 2u)&1))) {
#line 6 "default_rendergraph.rpsl"
    goto trunk;
#line 6 "default_rendergraph.rpsl"
  } else {
#line 6 "default_rendergraph.rpsl"
    goto err;
#line 6 "default_rendergraph.rpsl"
  }

trunk:
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._84 = *llvm_cbe_temp__82;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._85 = (struct texture*) alloca(sizeof(struct texture));
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._86 = ((struct texture*)_llvm_cbe_tmps._84);
#line 6 "default_rendergraph.rpsl"
  if (((((llvm_cbe_temp__83 & 1) == 0u)&1))) {
#line 6 "default_rendergraph.rpsl"
    goto _2e_preheader;
#line 6 "default_rendergraph.rpsl"
  } else {
#line 6 "default_rendergraph.rpsl"
    _llvm_cbe_phi_tmps._88__PHI_TEMPORARY = _llvm_cbe_tmps._86;   /* for PHI node */
#line 6 "default_rendergraph.rpsl"
    goto _2e_loopexit;
#line 6 "default_rendergraph.rpsl"
  }

err:
#line 6 "default_rendergraph.rpsl"
  ___rpsl_abort(-3);
#line 6 "default_rendergraph.rpsl"
  return;
_2e_preheader:
#line 6 "default_rendergraph.rpsl"
  _BA__PD_make_default_texture_view_from_desc_AE__AE_YA_PD_AUtexture_AE__AE_IUResourceDesc_AE__AE__AE_Z(_llvm_cbe_tmps._85, 0, (((struct ResourceDesc*)_llvm_cbe_tmps._84)));
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_phi_tmps._88__PHI_TEMPORARY = _llvm_cbe_tmps._85;   /* for PHI node */
#line 6 "default_rendergraph.rpsl"
  goto _2e_loopexit;
#line 6 "default_rendergraph.rpsl"

_2e_loopexit:
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_phi_tmps._88 = _llvm_cbe_phi_tmps._88__PHI_TEMPORARY;
#line 6 "default_rendergraph.rpsl"
  _llvm_cbe_tmps._87 = *((&llvm_cbe_temp__82[((int32_t)1)]));
#line 6 "default_rendergraph.rpsl"
  rpsl_M_rps_main_Fn_rps_main(_llvm_cbe_phi_tmps._88, (((struct Args*)_llvm_cbe_tmps._87)));
#line 6 "default_rendergraph.rpsl"
}

