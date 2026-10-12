#ifndef RENDER_H
#define RENDER_H
#include "hud.h"
#include "object.h"
#include "psx.h"

struct RACE_VIEW;
#include "camera.h"

typedef struct
{
    sint16 vector[3];
    sint16 scale;
} RENDER_LIGHT;

typedef struct
{
    RENDER_LIGHT lights[3];
    MATRIX color;
} RENDER_LIGHTING;

typedef enum
{
    RENDER_LIGHT_NORMAL,
    RENDER_LIGHT_MIRROR,
    RENDER_LIGHT_SHOWCASE
} RENDER_LIGHT_SET;

RENDER_LIGHTING *render_lighting(uint32 idx);
void render_select_lighting(RENDER_LIGHT_SET set);
void render_load_lighting(const uint8 *src, uint32 size);

const uint32 *render_pal(uint32 idx);
void render_publish_pal(void);
void render_select_pal(sint32 mirror);
void render_load_pal(const uint8 *src, uint32 size);
uint32 *render_begin_pal(uint32 segs);
void render_end_pal(uint32 words);

typedef struct
{
    uint8 u, v;
} RENDER_TEXCOORD;

typedef struct
{
    uint16 tpage, clut;

    union
    {
        RENDER_TEXCOORD uv[4];
        uint32 uv_words[2];
    };

    uint32 surface;
} RENDER_MATERIAL;

const RENDER_MATERIAL *render_material(uint32 idx);

typedef struct
{
    sint16 x, y, z;
    uint16 type;
} RENDER_BILLBOARD;

typedef struct
{
    RENDER_BILLBOARD entries[64];
    uint32 count;
} RENDER_BILLBOARD_LIST;

extern RENDER_BILLBOARD_LIST render_billboards;
sint32 render_add_billboard(sint32 x, sint32 y, sint32 z, uint16 type);

typedef struct RENDER_FRAME RENDER_FRAME;

extern uint32 render_capture_enabled;
extern sint32 render_proj_speed[2];

HUD *render_frame_hud(RENDER_FRAME *frame);

uint32 *render_route_ot(RENDER_FRAME *frame);
uint32 *render_object_ot(RENDER_FRAME *frame);
uint32 *render_scene_ot(RENDER_FRAME *frame);
DR_MODE *render_ui_modes(RENDER_FRAME *frame);
RENDER_FRAME *render_frame(const CAMERA *view, sint32 frame);

sint32 render_init_prims(RENDER_FRAME *frame);
extern uint8 render_curve[16][512];
sint32 render_build_curve_scaled_lut(void);
sint32 render_init_materials(sint32 grp);
sint32 render_init_ui_prims(void);
uint32 render_target_indicator(const struct RACE_VIEW *owner, uint32 *ot, sint32 alternate);

typedef struct
{
    // Ordering table at the GPU boundary
    uint32 *ot;
    uint32 depth_limit;
    sint32 bias;
    uint32 *route_ot;
} RENDER_ORDER;

extern RENDER_ORDER render_order;

void render_set_ot(uint32 *ot, uint32 depth_limit);
sint32 render_scale_proj_transform(uint32 shift);
void render_set_proj_dist(uint32 proj);
void render_set_geom_offset(sint32 x, sint32 y);
void render_init_packet_pools(void);
void render_select_prim_pool(sint32 alternate);
uint32 render_packet_offset(void);
void *render_packet_at(uint32 offset, uint32 size);
void render_packet_publish(uint32 offset);
struct ROUTE_SEGMENT;
void render_generate_entries(const sint32 transform[3], const struct ROUTE_SEGMENT *seg, sint32 remaining);

sint32 render_init_billboards(void);
void render_billboard(uint32 *ot, const RENDER_BILLBOARD *entries, sint32 count, sint32 lower);

typedef struct
{
    POLY_FT4 *strips;
    POLY_F4 *edges;
    POLY_G4 *gradients;
    DR_TWIN *tex_windows;
} HORIZON_PRIMS;

HORIZON_PRIMS render_horizon_prims(RENDER_FRAME *frame);
sint32 render_dispatch_prim_grps(const OBJECT_GROUP *object, uint32 table, sint32 adjusted, uint32 *ot);

void render_init_hud_frame(RENDER_FRAME *frame, HUD_LAYOUT layout, sint32 buf);
#endif /* RENDER_H */
