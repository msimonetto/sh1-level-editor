#pragma once
#include <cstdint>

#pragma pack(push, 1)

// ---------------------------------------------------------------------------
// IPD_FILE_HEADER -- 84 bytes (0x54)
// ---------------------------------------------------------------------------
struct IPD_FILE_HEADER {
    uint8_t id;
    uint8_t flag;
    int8_t x_pos;
    int8_t y_pos;
    int32_t plm_offset;
    uint8_t obj_num;
    uint8_t pos_num;
    uint8_t unk1_num;
    uint8_t unk2;
    char pad1[8];
    int32_t obj_name_offset;
    int32_t obj_data_offset;
    char unk1_data[52];
    int32_t unkdata_offset;
};

// ---------------------------------------------------------------------------
// IPD_COLL_HEADER -- 308 bytes (0x134)
// ---------------------------------------------------------------------------
struct IPD_COLL_HEADER {
    int32_t positionX;
    int32_t positionZ;
    uint8_t splitVertexCount;
    uint8_t surfaceCount;
    uint8_t subcellCount;
    uint8_t unkBlock3Count;
    int32_t ptr_splitVertices;
    int32_t ptr_surfaces;
    int32_t ptr_subcells;
    int32_t ptr_unkBlock3;
    uint16_t gridScale;
    uint8_t gridWidth;
    uint8_t gridHeight;
    int32_t ptr_grid;
    uint16_t block5Count;
    uint16_t block6Count;
    int32_t ptr_block5;
    int32_t ptr_block6;
    int32_t ptr_unk7;
    uint8_t subcellCheckIdx[256];
};

// ---------------------------------------------------------------------------
// IPD_COLL_SVECTOR -- 6 bytes
// ---------------------------------------------------------------------------
struct IPD_COLL_SVECTOR {
    int16_t x;
    int16_t y;
    int16_t z;
};

// ---------------------------------------------------------------------------
// IPD_COLL_SURFACE -- 12 bytes
// ---------------------------------------------------------------------------
struct IPD_COLL_SURFACE {
    int16_t field_0;
    int16_t baseGroundHeight;
    int16_t field_4;
    uint16_t tilt_flags;
    int16_t tiltAngleX;
    int16_t tiltAngleZ;
};

// ---------------------------------------------------------------------------
// IPD_COLL_SUBCELL -- 10 bytes
// ---------------------------------------------------------------------------
struct IPD_COLL_SUBCELL {
    int16_t field_0;
    int16_t field_2;
    int16_t field_4;
    uint8_t splitVertexIdx0;
    uint8_t splitVertexIdx1;
    uint8_t surfaceIdx0;
    uint8_t surfaceIdx1;
};

// ---------------------------------------------------------------------------
// IPD_COLL_UNK3 -- 10 bytes
// ---------------------------------------------------------------------------
struct IPD_COLL_UNK3 {
    uint16_t flags;
    int16_t offsetX;
    int16_t offsetY;
    int16_t offsetZ;
    int16_t field_8;
};

// ---------------------------------------------------------------------------
// IPD_COLL_UNK5 -- 1 byte
// ---------------------------------------------------------------------------
struct IPD_COLL_UNK5 {
    uint8_t data;
};

// ---------------------------------------------------------------------------
// IPD_COLL_UNK6 -- 1 byte
// ---------------------------------------------------------------------------
struct IPD_COLL_UNK6 {
    uint8_t data;
};

// ---------------------------------------------------------------------------
// IPD_OBJNAME_DATA -- 16 bytes
// ---------------------------------------------------------------------------
struct IPD_OBJNAME_DATA {
    int32_t flag;
    char name[8];
    int32_t unk;
};

// ---------------------------------------------------------------------------
// IPD_POS_HEADER -- 24 bytes
// ---------------------------------------------------------------------------
struct IPD_POS_HEADER {
    uint8_t obj_num;
    uint8_t unk1_num;
    uint8_t unk2_num;
    uint8_t unk3_num;
    int32_t unk2;
    int32_t unk3;
    int32_t data_offset;
    int32_t unk1_offset;
    int32_t unk2_offset;
};

// ---------------------------------------------------------------------------
// IPD_OBJ_DATA -- 36 bytes
// ---------------------------------------------------------------------------
struct IPD_OBJ_DATA {
    int32_t obj_id;
    int16_t rt11;
    int16_t rt12;
    int16_t rt13;
    int16_t rt21;
    int16_t rt22;
    int16_t rt23;
    int16_t rt31;
    int16_t rt32;
    int16_t rt33;
    int16_t pad;
    int32_t tx;
    int32_t ty;
    int32_t tz;
};

// ---------------------------------------------------------------------------
// PLM_FILE_HEADER -- 20 bytes
// ---------------------------------------------------------------------------
struct PLM_FILE_HEADER {
    uint16_t id;
    uint8_t flag;
    uint8_t tex_num;
    int32_t tex_name_offset;
    int32_t obj_num;
    int32_t obj_start_offset;
    int32_t unk_data_offset;
};

// ---------------------------------------------------------------------------
// PLM_OBJ_HEADER -- 16 bytes
// ---------------------------------------------------------------------------
struct PLM_OBJ_HEADER {
    char name[8];
    uint8_t mesh_num;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    int32_t data_offset;
};

// ---------------------------------------------------------------------------
// PLM_DATA_HEADER -- 24 bytes
// ---------------------------------------------------------------------------
struct PLM_DATA_HEADER {
    uint8_t pack_num;
    uint8_t vert_num;
    uint8_t num_c;
    uint8_t num_d;
    int32_t pack_offset;
    int32_t vert_xy_offset;
    int32_t vert_z_offset;
    int32_t normal_offset;
    int32_t end_offset;
};

// ---------------------------------------------------------------------------
// PLM_PACK_HEADER -- 20 bytes
// ---------------------------------------------------------------------------
struct PLM_PACK_HEADER {
    uint8_t u0;
    uint8_t v0;
    uint16_t cba;
    uint8_t u1;
    uint8_t v1;
    uint8_t unk1;
    uint8_t tex_num_and_unk2_byte;
    uint8_t u2;
    uint8_t v2;
    uint8_t u3;
    uint8_t v3;
    uint8_t faces_0;
    uint8_t faces_1;
    uint8_t faces_2;
    uint8_t faces_3;
    uint8_t normals_0;
    uint8_t normals_1;
    uint8_t normals_2;
    uint8_t normals_3;
};

// ---------------------------------------------------------------------------
// TIM_FILE_HEADER -- 8 bytes
// ---------------------------------------------------------------------------
struct TIM_FILE_HEADER {
    uint8_t id;
    uint8_t ver;
    uint8_t pad1_0;
    uint8_t pad1_1;
    uint8_t bpp_and_flags;
    uint8_t pad2_0;
    uint8_t pad2_1;
    uint8_t pad2_2;
};

// ---------------------------------------------------------------------------
// TIM_CLUT_HEADER -- 12 bytes
// ---------------------------------------------------------------------------
struct TIM_CLUT_HEADER {
    int32_t clut_length;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

// ---------------------------------------------------------------------------
// TIM_IMG_HEADER -- 12 bytes
// ---------------------------------------------------------------------------
struct TIM_IMG_HEADER {
    int32_t img_length;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

// ---------------------------------------------------------------------------
// TGA_FILE_HEADER -- 18 bytes
// ---------------------------------------------------------------------------
struct TGA_FILE_HEADER {
    uint8_t id_len;
    uint8_t color_map_type;
    uint8_t image_type;
    uint16_t cm_first_entry;
    uint16_t cm_length;
    uint8_t map_entry_size;
    uint16_t h_origin;
    uint16_t v_origin;
    uint16_t width;
    uint16_t height;
    uint8_t pixel_depth;
    uint8_t image_desc;
};

// ---------------------------------------------------------------------------
// MapPoint2d -- 12 bytes
// ---------------------------------------------------------------------------
struct MapPoint2d {
    int32_t  positionX;       // Q19.12 world X
    uint32_t paperMapIdx    : 5;
    uint32_t field_4_5      : 4;
    uint32_t loadingScreenId: 3;
    uint32_t unused_4_12    : 4;
    uint32_t triggerParam0  : 8;  // Q8 arrival angle / trigger param
    uint32_t triggerParam1  : 8;
    int32_t  positionZ;       // Q19.12 world Z
};

// ---------------------------------------------------------------------------
// EventData -- 12 bytes
// ---------------------------------------------------------------------------
struct EventData {
    int16_t  requiredEventFlag;
    int16_t  disabledEventFlag;
    uint8_t  triggerType    : 4;  // e_TriggerType
    uint8_t  activationType : 4;  // e_TriggerActivationType
    uint8_t  pointOfInterestIdx;  // index into mapPoints[]
    uint8_t  requiredItemId;
    uint8_t  pad_7;
    uint32_t sysState       : 5;  // e_SysState
    uint32_t eventParam     : 8;  // mapEventFuncs index OR mapPoints index
    uint32_t flags_8_13     : 6;
    uint32_t sfxPairIdx     : 5;  // e_SfxPairIdx
    uint32_t field_8_24     : 1;  // "Is on camera rail?"
    uint32_t mapIdx         : 6;  // destination map (e_MapIdx, 0-42)
    uint32_t field_8_31     : 1;
};


#pragma pack(pop)

// Compile-time verification of #pragma pack(1) alignment
static_assert(sizeof(IPD_FILE_HEADER) == 84, "IPD_FILE_HEADER size mismatch");
static_assert(sizeof(IPD_COLL_HEADER) == 308, "IPD_COLL_HEADER size mismatch");
static_assert(sizeof(IPD_COLL_SVECTOR) == 6, "IPD_COLL_SVECTOR size mismatch");
static_assert(sizeof(IPD_COLL_SURFACE) == 12, "IPD_COLL_SURFACE size mismatch");
static_assert(sizeof(IPD_COLL_SUBCELL) == 10, "IPD_COLL_SUBCELL size mismatch");
static_assert(sizeof(IPD_COLL_UNK3) == 10, "IPD_COLL_UNK3 size mismatch");
static_assert(sizeof(IPD_COLL_UNK5) == 1, "IPD_COLL_UNK5 size mismatch");
static_assert(sizeof(IPD_COLL_UNK6) == 1, "IPD_COLL_UNK6 size mismatch");
static_assert(sizeof(IPD_OBJNAME_DATA) == 16, "IPD_OBJNAME_DATA size mismatch");
static_assert(sizeof(IPD_POS_HEADER) == 24, "IPD_POS_HEADER size mismatch");
static_assert(sizeof(IPD_OBJ_DATA) == 36, "IPD_OBJ_DATA size mismatch");
static_assert(sizeof(PLM_FILE_HEADER) == 20, "PLM_FILE_HEADER size mismatch");
static_assert(sizeof(PLM_OBJ_HEADER) == 16, "PLM_OBJ_HEADER size mismatch");
static_assert(sizeof(PLM_DATA_HEADER) == 24, "PLM_DATA_HEADER size mismatch");
static_assert(sizeof(PLM_PACK_HEADER) == 20, "PLM_PACK_HEADER size mismatch");
static_assert(sizeof(TIM_FILE_HEADER) == 8, "TIM_FILE_HEADER size mismatch");
static_assert(sizeof(TIM_CLUT_HEADER) == 12, "TIM_CLUT_HEADER size mismatch");
static_assert(sizeof(TIM_IMG_HEADER) == 12, "TIM_IMG_HEADER size mismatch");
static_assert(sizeof(TGA_FILE_HEADER) == 18, "TGA_FILE_HEADER size mismatch");
static_assert(sizeof(MapPoint2d) == 12, "MapPoint2d size mismatch");
static_assert(sizeof(EventData) == 12, "EventData size mismatch");

