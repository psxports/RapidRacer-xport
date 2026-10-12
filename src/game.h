#ifndef GAME_H
#define GAME_H

#include "psx.h"

typedef struct RACE_VIEW
{
    uint32 idx;
} RACE_VIEW;

extern const RACE_VIEW race_views[2];

typedef struct RACE_CFG
{
    uint32 players;
    uint32 boats[2];
    uint32 mode;
    uint32 layout;
    uint32 variant;
    uint32 course;
} RACE_CFG;

extern RACE_CFG race_cfg;
extern uint32 race_paused;

typedef struct
{
    sint16 selection;
    uint16 mode;
    uint16 rules;
    uint16 prev_mode;
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
    uint8 no_cur;
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

extern uint32 race_start_phase;
sint32 game_update_racer_modes(const RACE_VIEW *view);

void game_start(void);

sint32 game_render_frame(void);

uint32 xport_libsnd_sequence_call(uint32 target, uint32 argc, const uint32 *args);
void xport_main(void);
#endif /* GAME_H */
