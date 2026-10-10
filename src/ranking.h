#ifndef RR_RANKING_H
#define RR_RANKING_H

#include "input.h"

#include "psx.h"

extern const uint8 ranking_points[16];
uint8 ranking_place_points(uint32 place);

sint32 ranking_find_empty_slot(void);
sint32 ranking_player_is_leading(void);
sint32 ranking_refresh_championship(void);
sint32 ranking_refresh_standard(void);
sint32 ranking_refresh_reduced(void);
sint32 ranking_refresh(void);
sint32 ranking_refresh_selection(void);
sint32 ranking_wrap_selection(sint32 fallback);
sint32 profile_confirm_mode(CONTROLLER_STATE *input);
sint32 profile_init_mode_state(void);
sint32 ranking_build_order(void);
sint32 profile_reset_select_state(void);
uint32 ranking_init_screen(void);
sint32 ranking_finalize_select(void);
sint32 ranking_update_screen_anim(void);
void ranking_noop(void);
sint32 profile_unlock_progress(uint32 slot);
void profile_select_mode(void);
sint32 profile_refresh_mode_visual(void);
sint32 profile_init_slot(sint16 profile_index);
sint32 profile_init_select(void);
sint32 ranking_refresh_peer_times(void);

sint32 ranking_build_order_desc(void);
sint32 ranking_refresh_results(void);
sint32 ranking_fn_8005e320(sint16 value);
sint32 results_enter_best_times_store(uint32 first, uint32 second, uint32 third, uint32 fourth);
#endif /* RR_RANKING_H */
