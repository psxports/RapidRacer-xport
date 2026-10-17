#ifndef MESH_H
#define MESH_H

#include "psx.h"
#include "utils.h"

struct RACE_VIEW;

#include <stddef.h>

// JETS archive identifiers in native little-endian word order
enum
{
    CHUNK_FORM = 0x4D524F46,
    CHUNK_JETS = 0x5354454A,
    CHUNK_BOAT = 0x54414F42,
    CHUNK_TIMA = 0x414D4954
};

typedef struct
{
    uint32 id, size_be;
} IFF_CHUNK;

typedef struct
{
    IFF_CHUNK chunk;
    uint32 type;
} IFF_HEADER;

struct BOAT;

typedef struct
{
    SVECTOR v[4], n[4];
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
} MESH_RENDER;

extern MESH_RENDER mesh_render_state;

typedef struct
{
    struct BOAT *boats[32];
    uint16 buckets[32];
    struct BOAT *carried;
    sint32 count;
} MESH_RACERS;

void mesh_prepare_racers(const struct RACE_VIEW *view, MESH_RACERS *dst, sint32 (*visible)(const struct RACE_VIEW *, const struct BOAT *));
void hd_mesh_prepare_racers(const struct RACE_VIEW *view, MESH_RACERS *dst, sint32 update, sint32 (*visible)(const struct RACE_VIEW *, const struct BOAT *), float (*depth)(const struct RACE_VIEW *, const struct BOAT *));
sint32 mesh_prop_offset(const struct BOAT *boat);
sint32 mesh_prepare_props(struct BOAT *boat, MATRIX dst[2]);

void mesh_clear_boat_models(MESH_BOAT_MODELS *models);
sint32 mesh_load_boat_models(const MESH_ARCHIVE *archive, uint8 boat, sint32 level, uint8 variant, uint32 flags, MESH_BOAT_MODELS *dst);

// Dst must be zero-initialized or own a decoded resource
sint32 mesh_decode_archive(const uint8 *src, uint32 size, MESH_ARCHIVE *dst);
void mesh_clear_archive(MESH_ARCHIVE *archive);
sint32 mesh_copy_model(const MESH_MODEL *src, MESH_MODEL *dst);

// Dst must be zero-initialized or own a previously decoded model
sint32 mesh_decode_model(const uint8 *src, uint32 size, MESH_MODEL *dst);
void mesh_clear_model(MESH_MODEL *model);

void mesh_draw_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model);

uint32 *mesh_prepare_player_ot_packet(const struct RACE_VIEW *state, sint32 player);

void mesh_render_racer_model(const struct RACE_VIEW *view, sint32 player);
sint32 mesh_init_tex_templates(void);

#endif /* MESH_H */
