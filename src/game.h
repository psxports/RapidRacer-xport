#ifndef RR_GAME_H
#define RR_GAME_H

#include "psx.h"

typedef struct
{
    uint32 first;
    uint32 second;
    uint32 configuration;
    sint32 warmup;
} RR_RACE_LOOP_STATE;

void game_start(void);

sint32 race_loop(uint32 first, uint32 second, uint32 configuration);

sint32 game_render_frame(void);

sint32 race_init_runtime(uint32 first, uint32 second, uint32 context);

sint32 game_select_state(uint32 value);
sint32 game_init_defaults(void);
void guest_debug_append(const char *text, uint32 length);
sint32 race_loop_resume(uint32 first, uint32 second, uint32 configuration, sint32 warmup);
void race_begin_loop(RR_RACE_LOOP_STATE *state, uint32 first, uint32 second, uint32 configuration, sint32 warmup);
sint32 race_loop_before_frame(RR_RACE_LOOP_STATE *state);
void race_loop_after_frame(RR_RACE_LOOP_STATE *state);
sint32 race_finish_loop(RR_RACE_LOOP_STATE *state);

void game_main_loop_continue(sint32 menu_already_returned);

#endif /* RR_GAME_H */
