#ifndef UPGRADES_H
#define UPGRADES_H

#include "input.h"
#include "profile.h"

#include "psx.h"

void upgrades_enable_prim(POLY_FT4 *prim, sint32 enabled);

PROFILE_GRID *profile_grid_row(sint16 idx);

sint32 profile_get_table_byte(const PLAYER_PROFILE *data, sint16 row, sint16 column);

sint32 profile_copy_templates(sint16 profile_idx);
sint32 upgrades_apply_race_result(void);
sint32 profile_build_vehicle_grid(sint16 rec_grp, sint16 profile_row, sint16 offset_x, sint16 offset_y);

void upgrades_update_grid(void);
sint32 upgrades_process_grid_input(PAD *input);
sint32 upgrades_init_custom_grid(void);
sint32 upgrades_update_custom_grid(PAD *input);
sint32 upgrades_init_upgrade_grid(void);

sint32 upgrades_update_upgrade_grid(PAD *input);
sint32 upgrades_init_retry(void);
sint32 upgrades_update_retry(PAD *input);
sint32 upgrades_refresh_unlocks(void);
sint32 upgrades_apply_upgrade(void);
sint32 upgrades_leave_upgrade(sint16 val);
sint32 upgrades_apply_unlock(void);
sint32 upgrades_update_reward(PAD *input);
void upgrades_init_progress(void);
sint32 upgrades_update_progress(PAD *input);
sint32 upgrades_refresh_grid_limits(void);
sint32 profile_cell_dispatch_complete(void);
sint32 profile_idx_map(void);
sint32 upgrades_init_player_profile(uint32 slot);

#endif /* UPGRADES_H */
