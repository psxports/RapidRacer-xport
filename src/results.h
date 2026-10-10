#ifndef RR_RESULTS_H
#define RR_RESULTS_H

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
    uint32 record_ticks;
    uint32 checkpoint_max;
    uint32 lap_active;
    uint32 finished;
    uint16 hud_values[3];
    uint16 winner;
    uint16 lap_offset;
} RESULT_STATE;

extern RESULT_STATE result_state;
extern const uint32 results_grid_columns[3];

sint32 results_load_checkpoints(void);
sint32 results_layout_populate(const struct RACE_VIEW *state);
sint32 results_init_marker_recs(void);
sint32 race_config_results(void);
sint32 results_select_best_time_rec(struct BOAT_MENU *menu);

sint32 results_fn_8003cb14(const struct RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument);
sint32 results_advance_page(const struct RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument);
sint32 results_fn_8003cf10(const struct RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument);
sint32 race_process_lap_completion(const struct RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument);

void results_2p_complete_route(void);

#endif /* RR_RESULTS_H */
