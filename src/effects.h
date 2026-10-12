#ifndef EFFECTS_H
#define EFFECTS_H

#include "psx.h"

struct RACE_VIEW;
#include "callbacks.h"

uint32 effects_init_transform_object(const struct RACE_VIEW *state);

sint32 effects_register_mode(void);
sint32 effects_capture_scr_region(const struct RACE_VIEW *state, sint32 buffer_idx);

sint32 effects_render_quad_proj_fade(const struct RACE_VIEW *state, sint32 buffer_idx);
void effects_render(const struct RACE_VIEW *player, sint32 player_idx);

sint32 effects_run_expanding_circle_trans(void);

#endif /* EFFECTS_H */
