#ifndef RR_PROFILE_H
#define RR_PROFILE_H

#include "input.h"

#include "psx.h"
#include <stddef.h>

typedef struct
{
    uint8 choice[4];
    uint8 limit[4];
    uint8 offset[4];
} PROFILE_GRID;

typedef struct
{
    uint16 completed;
    uint16 grid_row;
    uint16 phase;
    uint8 points[16];
} PROFILE_SERIES;

typedef struct
{
    uint8 attempts;
    uint8 result;
    uint8 state;
} PROFILE_COURSE;

typedef struct
{
    PROFILE_COURSE courses[4][6];
} PROFILE_PROGRESS;

typedef struct
{
    uint8 reset_pending;
    uint8 level_result;
    uint8 max_level;
    uint8 reward_flags;
    uint8 course;
    uint8 level;
    PROFILE_PROGRESS progress;
    uint8 availability[10];
    PROFILE_SERIES series[3];
    PROFILE_GRID grid[10];
} PLAYER_PROFILE;

sint32 profile_fn_80052f30(void);
sint32 profile_fn_800531c0(void);
sint32 profile_fn_80053458(void);
sint32 profile_fn_8005347c(void);
sint32 profile_fn_80053588(void);
sint32 profile_fn_800535b0(void);
sint32 profile_format_value_parts3(uint32 values, char *text);
sint32 time_fields3_compare(uint32 first, uint32 second);
uint32 time_fields_convert(uint32 value);
sint32 profile_reset_grid_flags(void);
sint32 profile_update_trans(void);
sint32 profile_update_mode_select_vis(void);
sint32 profile_update_carousel(sint16 direction, CONTROLLER_STATE *input);
sint32 vehicle_update_carousel(void);
sint32 vehicle_select_assign_palettes(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 vehicle_advance_carousel(sint16 action, CONTROLLER_STATE *input);
sint32 profile_populate_select_recs(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_sort_recs_desc(uint32 unused1, uint32 unused2);
sint32 profile_sort_recs_asc(uint32 unused1, uint32 unused2, uint32 unused3);
sint32 profile_fn_80055cfc(sint16 direction, sint16 index, uint32 state);
sint32 menu_start_dir_trans(sint16 direction);
sint32 profile_is_unlocked(sint16 index);
sint32 profile_level_is_available(sint32 requested, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_grid_cell_is_available(sint32 requested, sint32 column, uint32 unused3, uint32 unused4);
sint32 profile_get_grid_cell(sint32 requested, sint32 column, uint32 unused3, uint32 unused4);
sint32 profile_update_select_prims(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_init_menu(sint32 initialize, uint32 unused2, uint32 unused3, uint32 unused4);
void profile_select_course(void);
uint32 text_reset_rec_styles(void);
sint32 profile_fn_80056c98(void);
sint32 profile_handle_select_input(CONTROLLER_STATE *input, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_update_selection_vis_time(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_compact_rankings(void);
sint32 race_capture_time_summaries(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
uint32 profile_update_best_times(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_fn_800579d4(sint32 name_index, uint32 unused2, uint32 unused3, uint32 unused4);

uint32 profile_get_active(void);
PLAYER_PROFILE *profile_at(uint32 slot);
PLAYER_PROFILE *profile_current(void);
PROFILE_SERIES *profile_get_series(void);
void profile_reset_series(uint32 slot);
uint8 profile_save_read_byte(uint32 offset);
void profile_save_write_byte(uint32 offset, uint8 value);
sint32 profile_init_mode_rec(void);
sint32 profile_backup_selection(void);
sint32 profile_restore_backup(void);
sint32 profile_reset_recs(void);
sint32 profile_advance_menu_select(void);
sint32 profile_update_menu_trans(CONTROLLER_STATE *input);
sint32 profile_dispatch_menu_state(void);
sint32 profile_update_row_completion(void);
sint32 profile_row_is_complete(sint16 row);
sint32 profile_confirm_row_select(void);
sint32 profile_fn_8005aca8(void);
sint32 profile_refresh_menu_ack(void);

sint32 profile_fn_8005ad20(void);
sint32 profile_fn_8005afa0(void);
sint32 profile_fn_8005b350(CONTROLLER_STATE *input);
sint32 profile_fn_8005b570(void);
sint32 profile_restore_selection(sint16 selection);
sint32 profile_complete_selection(void);
sint32 profile_apply_action(sint16 action);
sint32 profile_reset_menu_state(void);
sint32 profile_dispatch_mode_result_handler(uint32 first, uint32 second, uint32 third, uint32 fourth);
sint32 menu_save_select_state(void);

sint32 profile_process_select_results(void);

sint32 results_handle_2p(void);
sint32 profile_commit_selection(void);
sint32 profile_update_limits(void);
sint32 profile_unlock(sint16 index);
sint32 profile_generate_opponents(void);
sint32 profile_merge_unlocks(PLAYER_PROFILE *profile);
sint32 profile_init_ranking_grid(void);

#endif /* RR_PROFILE_H */
