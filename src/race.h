#ifndef RACE_H
#define RACE_H

#include "psx.h"

struct RACE_VIEW;

struct BOAT_MENU;

typedef struct
{
    uint8 status;
    uint8 column;
    uint8 pickups;
    // Grid and result counts from MAIN.EXE 800E1BAE/800E1BB0
    uint16 grid_count;
    uint16 count;
    // Pause menu player from MAIN.EXE 800E1BB2
    uint16 quit_player;
    uint32 bonus_timer;
    uint32 best_lap_ticks;
    uint32 rec_ticks;
    uint32 checkpoint_max;
    uint32 lap_active;
    uint32 finished;
    uint16 hud_vals[3];
    uint16 winner;
    uint16 lap_offset;
} RESULT;

extern RESULT result_state;
extern const uint32 race_grid_columns[3];

sint32 race_config_results(void);
void race_handle_2p(void);
sint32 race_init_markers(void);
sint32 race_load_checkpoints(void);
sint32 race_render_results(const struct RACE_VIEW *state, uint32 *ot);
sint32 race_compact_ranks(void);
sint32 race_capture_time_summaries(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 race_init_player(const struct RACE_VIEW *state, uint32 unused1, sint32 unused2);
#endif /* RACE_H */
