#ifndef RANKING_H
#define RANKING_H

#include "input.h"

#include "psx.h"

uint8 ranking_place_points(uint32 place);

sint32 ranking_find_empty_slot(void);
uint8 ranking_player_is_leading(void);
sint32 ranking_refresh_championship(void);

sint32 ranking_refresh(void);
void ranking_refresh_selection(void);
sint32 ranking_wrap_selection(sint32 fallback);
sint32 profile_confirm_mode(PAD *input);
sint32 profile_init_mode_state(void);
sint32 ranking_build_order(void);
sint32 profile_reset_select_state(void);
uint32 ranking_init_scr(void);
sint32 ranking_finalize_select(void);
sint32 ranking_update_scr_anim(void);

void profile_select_mode(void);
void profile_refresh_mode_visual(void);

sint32 profile_init_select(void);
sint32 ranking_refresh_peer_times(void);

sint32 ranking_refresh_results(void);
sint32 ranking_leave_series_results(sint16 val);
sint32 results_enter_best_times_store(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
#endif /* RANKING_H */
