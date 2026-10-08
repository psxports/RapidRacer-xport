#ifndef RR_SCENE_H
#define RR_SCENE_H

#include "psx.h"

sint32 scene_render_player_scene(uint32 context, sint32 player_index);
sint32 scene_clear_group_item_flags(sint16 group);

uint32 scene_render_player_sequence(uint32 sequence, uint32 state_base, sint32 state_index);
sint32 scene_process_published_rec_flags(void);

#endif /* RR_SCENE_H */
