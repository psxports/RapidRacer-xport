#ifndef RR_RESULTS_H
#define RR_RESULTS_H

#include "psx.h"

sint32 results_layout_populate(uint32 state);
sint32 results_init_marker_recs(void);
uint32 results_select_table(uint32 unused, uint32 fallback);
sint32 race_config_results(void);
sint32 results_select_best_time_rec(uint32 state);

sint32 results_fn_8003cb14(uint32 state, uint32 menu, sint32 unused, uint32 argument);
sint32 results_advance_page(uint32 state, uint32 menu, sint32 unused, uint32 argument);
sint32 results_fn_8003cf10(uint32 state, uint32 menu, sint32 unused, uint32 argument);
sint32 race_process_lap_completion(uint32 state, uint32 menu, sint32 unused, uint32 argument);

sint32 results_2p_complete_route(void);

#endif /* RR_RESULTS_H */
