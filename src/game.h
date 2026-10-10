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

typedef struct
{
    uint16 selection;
    uint16 mode;
    uint16 rules;
    uint16 previous_mode;
    uint16 flags;
    uint8 ready;
    uint8 event;
    uint8 event_arg;
    uint8 menu_variant;
    uint16 players;
    // Attract sequence from MAIN.EXE 800E1B8B
    uint8 attract;
} GAME_SELECTION;

extern GAME_SELECTION game_selection;

typedef struct
{
    uint32 base_rate;
    uint32 frame_rate;
    // Shared race tick counter from MAIN.EXE 800B3D94
    uint16 ticks;
} GAME_TIMING;

extern GAME_TIMING game_timing;

typedef struct
{
    uint16 level;
    uint16 round_limit;
    uint8 unlock_courses;
    uint8 result_filter;
    uint8 no_current;
    uint8 split_layout;
    uint8 players_only;
    uint8 handicap[2];
} GAME_OPTIONS;

extern GAME_OPTIONS game_options;

typedef struct
{
    uint8 ranks[16];
    uint8 boats[5];
    uint8 ai_variant;
    uint16 format;
    // Special race flag and VRAM resource from MAIN.EXE 800E1B9C/800E1BA4
    uint16 special;
    uint16 resource;
} RACE_SELECTION;

extern RACE_SELECTION race_selection;

typedef struct
{
    uint8 boat;
    uint8 variant;
    uint8 reserved[2];
    uint32 mode;
} RACE_PARTICIPANT;

extern RACE_PARTICIPANT race_participants[16];

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
