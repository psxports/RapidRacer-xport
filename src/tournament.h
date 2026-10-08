#ifndef RR_TOURNAMENT_H
#define RR_TOURNAMENT_H

#include "input.h"

#include "psx.h"

sint32 tournament_draw_grid(void);
sint32 tournament_reset_grid(void);
sint32 tournament_calc_target_time(uint32 value, sint16 index);
sint32 tournament_refresh_grid(void);
sint32 tournament_update_grid_input(CONTROLLER_STATE *input);
sint32 tournament_sync_grid(void);
sint32 tournament_enter_race(void);
sint32 tournament_grid_complete(void);
sint32 tournament_refresh_player_name(void);
sint32 tournament_record_result(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 tournament_init_match_order(void);
sint32 tournament_init_round(void);
sint32 tournament_draw_match_highlight(void);
sint32 tournament_refresh_matches(void);
sint32 tournament_update_match_input(CONTROLLER_STATE *input);
sint32 tournament_sync_match(void);
sint32 tournament_record_match_result(void);
sint32 tournament_refresh_standings(void);
sint32 tournament_advance_round(CONTROLLER_STATE *input);
sint32 tournament_refresh_leader(void);
sint32 tournament_reset_result_rows(void);
sint32 tournament_init_profile(void);
sint32 tournament_refresh_table(void);
sint32 tournament_init_table(void);
sint32 tournament_update_mode_input(CONTROLLER_STATE *input);
sint32 tournament_reset_text(void);
sint32 tournament_record_champ_result(void);

#endif /* RR_TOURNAMENT_H */
