#ifndef RR_ARENA_H
#define RR_ARENA_H

#include "psx.h"

uint32 game_reset_checkpoint_stack(void);
uint32 game_push_checkpoint(void);
sint32 game_pop_checkpoint(void);
uint32 game_alloc_arena_bytes(sint32 size);
uint32 arena_activate_scope(void);
uint32 arena_release_aligned(sint32 size);

uint32 arena_alloc_aligned(uint32 size);

#endif /* RR_ARENA_H */
