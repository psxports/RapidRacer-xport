#ifndef HUD_H
#define HUD_H
#include "psx.h"
#include "input.h"
struct RACE_VIEW;

typedef struct
{
    char time[8];
    char lap[8];
    char rank[8];
    char laps[4][12];
    char best_lap[12];
    char rec_label[8];
    char best_label[8];
    char rec[12];
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
    SPRT spr;
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
    sint16 prim_count, desc_count, hud_count, menu_count, text_count;
} HUD;

enum
{
    HUD_PRIM_CAPACITY = 110
};

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

extern const HUD_SPRITE *hud_geom;

extern HUD hud_state;

sint32 hud_render(const struct RACE_VIEW *state, uint32 *ot, sint32 result);
void hud_init_context(sint32 mode);
sint32 hud_init_prims(HUD *hud, HUD_PRIM *prims, DR_MODE modes[2], const HUD_SPRITE *src, uint16 count, uint16 hud_count, uint16 menu_count, sint32 buf);
HUD_PRIM *hud_publish(const HUD *hud);
HUD_SPRITE *hud_sprs(HUD_LAYOUT layout);
sint32 hud_submit_active_recs(HUD_PRIM *prims, uint32 *ot);
#endif
