#ifndef RR_EFFECTS_H
#define RR_EFFECTS_H

#include "psx.h"

struct RACE_VIEW;
#include "callbacks.h"

sint32 effects_advance_tex_cb(CB_NODE *node);

sint32 effects_update_transform_cb(CB_NODE *node);
uint32 sprite_packets_build2(uint32 *ot_entry);
sint32 effects_render_depth_quads(uint32 *ot);
uint32 effects_init_transform_object(const struct RACE_VIEW *state);
sint32 effects_update_particles(CB_NODE *node);
sint32 effects_init_particles(void);
sint32 effects_register_mode(void);
sint32 effects_capture_screen_region(const struct RACE_VIEW *state, sint32 buffer_index);
sint32 effects_init_cache(void);
sint32 effects_render_quad_proj_fade(const struct RACE_VIEW *state, sint32 buffer_index);
void effects_render(const struct RACE_VIEW *player, sint32 player_index);
uint32 effects_emit_filled_circle(uint32 *ot_entry, sint16 center_x, sint32 center_y, sint32 radius);

sint32 effects_run_expanding_circle_trans(void);

#endif /* RR_EFFECTS_H */
