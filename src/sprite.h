#ifndef RR_SPRITE_H
#define RR_SPRITE_H

#include "psx.h"
#include <stddef.h>

typedef struct
{
    uint32 data;
    uint8 type;
    uint8 flags;
    uint16 value;
    uint16 x;
    uint16 y;
    uint8 command;
    uint8 field_0d;
    uint16 step_x;
    uint16 step_y;
    uint16 target_first;
    uint16 target_second;
    uint16 field_16;
    uint16 field_18;
    uint16 inset_x;
    uint16 inset_y;
    uint8 field_1e;
    uint8 unknown_1f[49];
    uint8 field_50;
    uint8 field_51;
    uint8 unknown_52[2];
} UI_RECORD;

typedef char UI_RECORD_SIZE_CHECK[(sizeof(UI_RECORD) == 84u) ? 1 : -1];
typedef char UI_RECORD_DATA_OFFSET_CHECK[(offsetof(UI_RECORD, data) == 0u) ? 1 : -1];
typedef char UI_RECORD_TYPE_OFFSET_CHECK[(offsetof(UI_RECORD, type) == 4u) ? 1 : -1];
typedef char UI_RECORD_FLAGS_OFFSET_CHECK[(offsetof(UI_RECORD, flags) == 5u) ? 1 : -1];
typedef char UI_RECORD_VALUE_OFFSET_CHECK[(offsetof(UI_RECORD, value) == 6u) ? 1 : -1];
typedef char UI_RECORD_X_OFFSET_CHECK[(offsetof(UI_RECORD, x) == 8u) ? 1 : -1];
typedef char UI_RECORD_Y_OFFSET_CHECK[(offsetof(UI_RECORD, y) == 10u) ? 1 : -1];
typedef char UI_RECORD_COMMAND_OFFSET_CHECK[(offsetof(UI_RECORD, command) == 12u) ? 1 : -1];
typedef char UI_RECORD_FIELD_0D_OFFSET_CHECK[(offsetof(UI_RECORD, field_0d) == 13u) ? 1 : -1];
typedef char UI_RECORD_STEP_X_OFFSET_CHECK[(offsetof(UI_RECORD, step_x) == 14u) ? 1 : -1];
typedef char UI_RECORD_STEP_Y_OFFSET_CHECK[(offsetof(UI_RECORD, step_y) == 16u) ? 1 : -1];
typedef char UI_RECORD_TARGET_FIRST_OFFSET_CHECK[(offsetof(UI_RECORD, target_first) == 18u) ? 1 : -1];
typedef char UI_RECORD_TARGET_SECOND_OFFSET_CHECK[(offsetof(UI_RECORD, target_second) == 20u) ? 1 : -1];
typedef char UI_RECORD_FIELD_16_OFFSET_CHECK[(offsetof(UI_RECORD, field_16) == 22u) ? 1 : -1];
typedef char UI_RECORD_FIELD_18_OFFSET_CHECK[(offsetof(UI_RECORD, field_18) == 24u) ? 1 : -1];
typedef char UI_RECORD_INSET_X_OFFSET_CHECK[(offsetof(UI_RECORD, inset_x) == 26u) ? 1 : -1];
typedef char UI_RECORD_INSET_Y_OFFSET_CHECK[(offsetof(UI_RECORD, inset_y) == 28u) ? 1 : -1];
typedef char UI_RECORD_FIELD_1E_OFFSET_CHECK[(offsetof(UI_RECORD, field_1e) == 30u) ? 1 : -1];
typedef char UI_RECORD_FIELD_50_OFFSET_CHECK[(offsetof(UI_RECORD, field_50) == 80u) ? 1 : -1];
typedef char UI_RECORD_FIELD_51_OFFSET_CHECK[(offsetof(UI_RECORD, field_51) == 81u) ? 1 : -1];

sint32 sprite_flush_pending(sint32 ordering_index);
sint32 sprite_reset_rotating_state(void);
sint32 sprite_update_rotating_recs(void);
sint32 render_alloc_rec_bufs(void);
sint32 render_release_rec_bufs(void);
sint32 menu_build_sprite_packet(uint32 output, uint32 source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height);
sint32 sprite_build_draw_packet(uint32 output, uint32 source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height);
sint32 render_dispatch_rec(uint32 output, sint16 source_index, sint16 kind);
uint32 sprite_init_anim_rec(sint16 index, uint32 data, sint8 type, sint16 value, sint16 first, sint16 second, sint8 flags);
sint32 render_init_recs(void);
sint32 scene_release_render_bufs(void);
sint32 sprite_config_prim(uint32 state, sint32 x, sint32 y);
uint32 sprite_set_orient(uint32 owner, sint8 orientation);
sint32 sprite_config_uv_orientation(uint32 state);
sint32 sprite_submit_anim(uint32 state, sint32 x, sint32 y, sint32 ordering_index);
sint32 sprite_apply_prim_vram_mask(uint32 owner);
sint32 sprite_enable_semitransparency(uint32 owner);
sint32 sprite_disable_semitransparency(uint32 owner);
sint32 sprite_clear_anim_recs(void);
sint32 sprite_apply_inset(uint32 owner);
sint32 menu_move_sprite_rec(uint32 owner, sint8 command, sint16 first, sint16 second, sint32 step_x, sint32 step_y, sint32 unused7, sint32 unused8, sint16 unused9, sint16 unused10);
sint32 sprite_init_trans(uint32 state, sint32 target_x, sint16 target_y, sint16 target_first, sint16 target_second, sint16 duration);
sint32 sprite_copy_anim_rec_bytes(uint32 source, uint32 destination);

sint32 sprite_update_anims(void);
sint32 sprite_submit_layered_recs(sint32 ordering_index);
sint32 sprite_submit_groups(sint32 ordering_index);
sint32 sprite_build_tex_desc_grid(void);
sint32 sprite_submit_pools(sint32 ordering_index);

sint32 sprite_text_buffer(void);
sint32 sprite_text_init_packets(void);

#endif /* RR_SPRITE_H */
