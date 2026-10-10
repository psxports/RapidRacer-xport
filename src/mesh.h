#ifndef RR_MESH_H
#define RR_MESH_H

#include "psx.h"

#include <stddef.h>

struct BOAT;

typedef struct
{
    SVECTOR v[4], normals[4];
    uint32 colors[4];
    uint16 tpage, clut;
} MESH_FACE;

typedef struct
{
    uint32 count;
    MESH_FACE *faces;
} MESH_FACE_SET;

typedef struct
{
    MESH_FACE_SET sets[8];
} MESH_MODEL;

typedef struct
{
    uint32 count;
    MESH_MODEL *models;
} MESH_ARCHIVE;

typedef struct
{
    MESH_MODEL main, lod, prop[2], prop_lod;
} MESH_BOAT_MODELS;

extern MESH_BOAT_MODELS mesh_boat_models[2][16];
extern MESH_MODEL mesh_event_models[4];

typedef struct
{
    uint32 enabled, rand_bit, rand_pair;
} MESH_RENDER_STATE;

extern MESH_RENDER_STATE mesh_render_state;

void mesh_clear_boat_models(MESH_BOAT_MODELS *models);
sint32 mesh_load_boat_models(const MESH_ARCHIVE *archive, uint8 boat, sint32 level, uint8 variant, uint32 flags, MESH_BOAT_MODELS *dst);

// Dst must be zero-initialized or own a decoded resource
sint32 mesh_decode_archive(const uint8 *src, size_t size, MESH_ARCHIVE *dst);
void mesh_clear_archive(MESH_ARCHIVE *archive);
sint32 mesh_copy_model(const MESH_MODEL *src, MESH_MODEL *dst);

// Dst must be zero-initialized or own a previously decoded model
sint32 mesh_decode_model(const uint8 *src, size_t size, MESH_MODEL *dst);
void mesh_clear_model(MESH_MODEL *model);
sint32 mesh_project_model(MESH_MODEL *model);
void mesh_draw_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model);
void mesh_outline_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model);
void mesh_draw_lit_quads(uint32 *ot, const MESH_MODEL *model, const SVECTOR *normal);


uint32 *mesh_prepare_player_ot_packet(uint32 state, sint32 player);
sint32 mesh_vis_vert_project(const struct BOAT *boat);
uint32 *mesh_prepare_racer_ot_packet(uint32 state, uint32 bucket, sint32 player);
sint32 route_segment_is_in_window(uint32 entry, sint32 offset);
sint32 mesh_order_bucket(uint32 view, const sint32 position[3], sint16 *depth, sint32 *visible);
void mesh_render_racer_model(uint32 view, sint32 player);
sint32 mesh_init_tex_templates(void);
void mesh_fn_80034180(void);

#endif /* RR_MESH_H */
