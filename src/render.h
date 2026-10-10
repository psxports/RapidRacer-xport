#include "object.h"
#ifndef RR_RENDER_H
#define RR_RENDER_H

#include "psx.h"
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

RENDER_LIGHTING *render_lighting(uint32 index);
void render_select_lighting(RENDER_LIGHT_SET set);
void render_load_lighting(const uint8 *src, size_t size);

const uint32 *render_palette(uint32 index);
void render_publish_palette(void);
void render_select_palette(sint32 mirror);
void render_load_palette(const uint8 *src, size_t size);
uint32 *render_begin_palette(uint32 segments);
void render_end_palette(uint32 words);

typedef struct
{
    uint8 u, v;
} RENDER_TEXCOORD;

typedef struct
{
    uint16 tpage, clut;
    RENDER_TEXCOORD uv[4];
    uint32 surface;
} RENDER_MATERIAL;

const RENDER_MATERIAL *render_material(uint32 index);

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

typedef struct
{
    char time[8];
    char lap[8];
    char rank[8];
    char laps[4][12];
    char best_lap[12];
    char record_label[8];
    char best_label[8];
    char record[12];
    char best[12];
    char markers[3][4];
    char trial_time[8];
    char names[2][16];
    char time_label[8];
    char extended[12];
    char wrong_way[12];
    char test[8];
    char delta[12];
} HUD_TEXT;

extern HUD_TEXT hud_text;

typedef struct HUD_FONT HUD_FONT;

typedef struct
{
    uint8 kind, state, reserved[2];
    SPRT sprite;
    DR_MODE mode;
} HUD_PRIM;

typedef struct
{
    sint16 font_id, x, y, align;
    uint8 visible;
    uint16 clut;
    char *text;
    const HUD_FONT *font;
    SPRT *shape;
    DR_MODE *mode;
} HUD_DESC;

typedef struct
{
    HUD_PRIM *prims;
    HUD_DESC *descs;
    uint16 prim_count, desc_count, hud_count, menu_count, text_count;
} HUD_STATE;

enum {HUD_PRIM_CAPACITY = 110};

typedef enum
{
    HUD_LAYOUT_SINGLE,
    HUD_LAYOUT_HORIZONTAL,
    HUD_LAYOUT_VERTICAL
} HUD_LAYOUT;

typedef struct
{
    sint16 x, y, tex_x, tex_y, w, h, clut_x, clut_y;
    uint8 state;
} HUD_SPRITE;

extern const HUD_SPRITE *render_hud_geom;
HUD_SPRITE *render_hud_sprites(HUD_LAYOUT layout);

extern HUD_STATE render_hud;
HUD_DESC *render_hud_desc_bank(HUD_LAYOUT layout, uint32 view);
HUD_STATE *render_frame_hud(RENDER_FRAME *frame);
HUD_PRIM *render_publish_hud(const HUD_STATE *hud);

uint32 *render_route_ot(RENDER_FRAME *frame);
uint32 *render_object_ot(RENDER_FRAME *frame);
uint32 *render_scene_ot(RENDER_FRAME *frame);
DR_MODE *render_ui_modes(RENDER_FRAME *frame);
RENDER_FRAME *render_frame(const CAMERA_STATE *view, sint32 frame);

sint32 render_init_prims(RENDER_FRAME *frame);
extern uint8 render_curve[16][512];
sint32 render_build_curve_scaled_lut(void);
sint32 render_init_materials(sint32 group);
sint32 render_init_ui_prims(void);
uint32 render_target_indicator(uint32 owner, uint32 *ot, sint32 alternate);

typedef struct
{
    // Ordering table at the GPU boundary
    uint32 *ot;
    uint32 depth_limit;
    sint32 bias;
    uint32 *route_ot;
} RENDER_ORDER;

extern RENDER_ORDER render_order;

void render_set_order_depth(uint32 *ot, uint32 depth_limit);
sint32 render_scale_proj_transform(uint32 shift);
void render_set_proj_dist(uint32 projection);
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
uint32 render_horizon(RENDER_FRAME *frame, const CAMERA_STATE *view);
sint32 render_dispatch_prim_groups(const OBJECT_GROUP_STATE *object, uint32 table, sint32 adjusted, uint32 *ot);
sint32 render_init_context(sint32 mode, uint32 unused);



sint32 render_submit_active_recs(HUD_PRIM *prims, uint32 *ot);

sint32 render_activate_recs(HUD_PRIM *prims);




void render_build_text_packets(void);

#endif /* RR_RENDER_H */
