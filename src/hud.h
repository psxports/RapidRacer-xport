#ifndef HUD_H
#define HUD_H
#include "psx.h"
#include "input.h"
struct RACE_VIEW;

extern char *const hud_rec;
extern char *const hud_best;
extern char *const hud_names[2];

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
    char text[16];
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

typedef struct
{
    uint16 u, v, w, h, tpage;
} HUD_GLYPH;

extern const HUD_SPRITE *hud_geom;

extern HUD hud_state;

sint32 hud_render(const struct RACE_VIEW *state, uint32 *ot, sint32 result);
void hud_init_context(sint32 mode);
sint32 hud_init_prims(HUD *hud, HUD_PRIM *prims, DR_MODE modes[2], const HUD_SPRITE *src, uint16 count, uint16 hud_count, uint16 menu_count, sint32 buf);
HUD_PRIM *hud_publish(const HUD *hud);
HUD_SPRITE *hud_sprs(HUD_LAYOUT layout);
sint32 hud_submit_active_recs(HUD_PRIM *prims, uint32 *ot);
void hud_stats_glyph(uint8 ch, HUD_GLYPH *glyph);
void hud_stats_render(uint32 *ot);
void hud_stats_bars(uint32 *ot);
#endif
