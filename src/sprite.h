#ifndef SPRITE_H
#define SPRITE_H

#include "psx.h"
#include <stddef.h>

// Compatible image descriptors are decoded at the file boundary
typedef struct
{
    char path[61];
    uint8 mode;
    uint8 reserved;
    uint16 u, v;
    uint16 width, height;
    uint16 tpage_x, tpage_y;
    uint16 clut_x, clut_y;
    uint16 x, y;
    uint16 tpage, clut;
} SPRITE_IMAGE;

extern SPRITE_IMAGE spr_labels[8];
extern SPRITE_IMAGE spr_images[160];
extern SPRITE_IMAGE spr_notices[10];
void spr_decode_image(SPRITE_IMAGE *image, const uint8 *src);

void spr_set_image_path(SPRITE_IMAGE *image, const char *path);
void spr_refresh_image_path(SPRITE_IMAGE *image);
sint32 spr_pixel_scale(uint8 mode);

// Sprite runtime owners from MAIN.EXE 800B6A04/800B6A84/800B6BCC/800D7578
typedef struct
{
    uint32 u, v;
    uint32 offset_x, offset_y;
    uint32 width, height;
    uint32 clut_x, clut_y;
    uint32 tpage_x, tpage_y;
    uint32 kind;
    uint8 mode, orientation, last_orientation;
    uint16 clip_idx;
    POLY_FT4 quad;
    SPRT simple;
    DR_TPAGE draw_mode;
} SPRITE_RENDER;

extern SPRITE_RENDER spr_glyphs[52];
SPRITE_RENDER *spr_render_at(SPRITE_RENDER *base, sint32 idx);
void spr_register_packets(void);

typedef struct
{
    SPRITE_RENDER *data;
    uint8 type;
    uint8 flags;
    sint16 val;
    sint16 x;
    sint16 y;
    uint8 command;
    uint8 returning;
    uint16 step_x;
    uint16 step_y;
    sint16 target0;
    sint16 target1;
    sint16 delta_x;
    sint16 delta_y;
    sint16 inset_x;
    sint16 inset_y;
    uint8 transition_active;
    uint8 transition_ticks;
    uint16 transition_base_x;
    uint16 transition_base_y;
    uint16 transition_target_x;
    uint16 transition_target_y;
    uint32 transition_x;
    uint32 transition_y;
    uint32 transition_step_x;
    uint32 transition_step_y;
    uint16 transition_base0;
    uint16 transition_base1;
    uint16 transition_target0;
    uint16 transition_target1;
    uint32 transition0;
    uint32 transition1;
    uint32 transition_step0;
    uint32 transition_step1;
    uint8 active;
    uint8 layer;
    uint8 unknown_52[2];
} UI_RECORD;

// UI array from MAIN.EXE 800B6A74
extern UI_RECORD spr_recs[180];
extern uint16 spr_buffers_active;

sint32 spr_flush_pending(sint32 ordering_idx);
sint32 spr_reset_labels(void);
sint32 spr_animate_labels(void);

sint32 menu_build_spr_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *src, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height);

UI_RECORD *spr_init_anim_rec(sint16 idx, SPRITE_RENDER *data, sint8 type, sint16 val, sint16 x, sint16 y, sint8 flags);
sint32 render_init_recs(void);
sint32 track_release_render_bufs(void);

SPRITE_RENDER *spr_set_orient(UI_RECORD *owner, sint8 orientation);

sint32 spr_apply_prim_vram_mask(UI_RECORD *owner);
sint32 spr_enable_semitransparency(UI_RECORD *owner);
sint32 spr_disable_semitransparency(UI_RECORD *owner);
sint32 spr_clear_anim_recs(void);
sint32 spr_apply_inset(UI_RECORD *owner);
sint32 menu_move_spr_rec(UI_RECORD *owner, sint8 command, sint16 inset_x, sint16 inset_y, sint32 step_x, sint32 step_y);
sint32 spr_init_trans(UI_RECORD *state, sint32 target_x, sint16 target_y, sint16 target0, sint16 target1, sint16 duration);
sint32 spr_copy_anim_rec_bytes(const UI_RECORD *src, UI_RECORD *dst);

sint32 spr_update_anims(void);
sint32 spr_submit_layered_recs(sint32 ordering_idx);
sint32 spr_submit_grps(sint32 ordering_idx);

sint32 spr_submit_pools(sint32 ordering_idx);

sint32 spr_text_buffer(void);
sint32 spr_text_init_packets(void);

#endif /* SPRITE_H */
