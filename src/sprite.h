#ifndef RR_SPRITE_H
#define RR_SPRITE_H

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

extern SPRITE_IMAGE sprite_labels[8];
extern SPRITE_IMAGE sprite_images[160];
extern SPRITE_IMAGE sprite_notices[10];
void sprite_decode_image(SPRITE_IMAGE *image, const uint8 *source);
void sprite_encode_image(const SPRITE_IMAGE *image, uint8 *target);
void sprite_set_image_path(SPRITE_IMAGE *image, const char *path);
void sprite_refresh_image_path(SPRITE_IMAGE *image);
sint32 sprite_pixel_scale(uint8 mode);

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
    uint16 clip_index;
    POLY_FT4 quad;
    SPRT simple;
    DR_TPAGE draw_mode;
} SPRITE_RENDER;

extern SPRITE_RENDER sprite_glyphs[52];
SPRITE_RENDER *sprite_render_at(SPRITE_RENDER *base, sint32 index);
void sprite_register_packets(void);

typedef struct
{
    SPRITE_RENDER *data;
    uint8 type;
    uint8 flags;
    uint16 value;
    uint16 x;
    uint16 y;
    uint8 command;
    uint8 returning;
    uint16 step_x;
    uint16 step_y;
    uint16 target_first;
    uint16 target_second;
    uint16 delta_x;
    uint16 delta_y;
    uint16 inset_x;
    uint16 inset_y;
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
    uint16 transition_base_first;
    uint16 transition_base_second;
    uint16 transition_target_first;
    uint16 transition_target_second;
    uint32 transition_first;
    uint32 transition_second;
    uint32 transition_step_first;
    uint32 transition_step_second;
    uint8 active;
    uint8 layer;
    uint8 unknown_52[2];
} UI_RECORD;

// UI array from MAIN.EXE 800B6A74
extern UI_RECORD sprite_records[180];
extern uint16 sprite_buffers_active;

sint32 sprite_flush_pending(sint32 ordering_index);
sint32 sprite_reset_labels(void);
sint32 sprite_animate_labels(void);
sint32 render_alloc_rec_bufs(void);
sint32 render_release_rec_bufs(void);
sint32 menu_build_sprite_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height);
sint32 sprite_build_draw_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height);
sint32 render_dispatch_rec(SPRITE_RENDER *output, sint16 source_index, sint16 kind);
UI_RECORD *sprite_init_anim_rec(sint16 index, SPRITE_RENDER *data, sint8 type, sint16 value, sint16 first, sint16 second, sint8 flags);
sint32 render_init_recs(void);
sint32 scene_release_render_bufs(void);
sint32 sprite_config_prim(SPRITE_RENDER *state, sint32 x, sint32 y);
SPRITE_RENDER *sprite_set_orient(UI_RECORD *owner, sint8 orientation);
sint32 sprite_config_uv_orientation(SPRITE_RENDER *state);
sint32 sprite_submit_anim(SPRITE_RENDER *state, sint32 x, sint32 y, sint32 ordering_index);
sint32 sprite_apply_prim_vram_mask(UI_RECORD *owner);
sint32 sprite_enable_semitransparency(UI_RECORD *owner);
sint32 sprite_disable_semitransparency(UI_RECORD *owner);
sint32 sprite_clear_anim_recs(void);
sint32 sprite_apply_inset(UI_RECORD *owner);
sint32 menu_move_sprite_rec(UI_RECORD *owner, sint8 command, sint16 first, sint16 second, sint32 step_x, sint32 step_y, sint32 unused7, sint32 unused8, sint16 unused9, sint16 unused10);
sint32 sprite_init_trans(UI_RECORD *state, sint32 target_x, sint16 target_y, sint16 target_first, sint16 target_second, sint16 duration);
sint32 sprite_copy_anim_rec_bytes(const UI_RECORD *source, UI_RECORD *destination);

sint32 sprite_update_anims(void);
sint32 sprite_submit_layered_recs(sint32 ordering_index);
sint32 sprite_submit_groups(sint32 ordering_index);

sint32 sprite_submit_pools(sint32 ordering_index);

sint32 sprite_text_buffer(void);
sint32 sprite_text_init_packets(void);

#endif /* RR_SPRITE_H */
