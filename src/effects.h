#ifndef RR_EFFECTS_H
#define RR_EFFECTS_H

#include "psx.h"

sint32 effects_advance_tex_cb(uint32 node);

uint32 effects_register_cb(uint32 argument);
sint32 effects_update_transform_cb(uint32 node);
uint32 sprite_packets_build2(uint32 ordering_entry);
sint32 effects_fn_80041104(uint32 ordering_table);
uint32 effects_init_transform_object(uint32 state);
uint32 effects_render_track_billboard(uint32 state, uint32 ordering_table);
sint32 effects_update_particles(uint32 node);
sint32 effects_fn_80041c8c(void);
sint32 effects_register_mode(void);
uint32 effects_relocate_anim_ptrs(uint32 structure);
uint32 effects_load_boat_model_chunk(uint32 archive, sint32 wanted);
uint32 effects_fn_80042258(uint32 archive, sint32 wanted);
sint32 effects_capture_screen_region(uint32 state, sint32 buffer_index);
sint32 effects_init_cache(void);
sint32 effects_render_quad_projected_fade(uint32 state, sint32 buffer_index);
void object_render_anim(uint32 player, sint32 player_index);
uint32 effects_emit_filled_circle(uint32 ordering_entry, sint16 center_x, sint32 center_y, sint32 radius);

sint32 effects_run_expanding_circle_trans(void);

#endif /* RR_EFFECTS_H */
