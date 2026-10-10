#ifndef RR_VEHICLE_SELECT_H
#define RR_VEHICLE_SELECT_H

#include "input.h"
#include "profile.h"

#include "psx.h"

void vehicle_select_enable_prim(POLY_FT4 *primitive, sint32 enabled);

PROFILE_GRID *profile_grid_row(sint16 index);
sint32 profile_get_grid_byte(sint16 row, sint16 column);
sint32 profile_get_table_byte(const PLAYER_PROFILE *data, sint16 row, sint16 column);
void vehicle_select_clear_grid(PROFILE_GRID *data);
sint32 profile_get_row_offset(sint16 row, sint16 column);
sint32 profile_copy_templates(sint16 profile_index);
sint32 vehicle_select_fn_8005e624(void);
sint32 profile_build_vehicle_grid(sint16 record_group, sint16 profile_row, sint16 offset_x, sint16 offset_y);
sint32 vehicle_select_render(sint16 record_group, sint32 unused2, sint16 offset_x, sint16 offset_y, const PROFILE_GRID *profile_row);
sint32 vehicle_draw_grid_lines(sint16 color, sint16 left, sint16 top, sint32 rows);
sint32 vehicle_select_update_grid(void);
sint32 vehicle_process_grid_input(CONTROLLER_STATE *input);
sint32 vehicle_select_fn_8005f82c(void);
sint32 vehicle_select_fn_8005fc28(CONTROLLER_STATE *input);
sint32 vehicle_select_fn_80060228(void);

sint32 vehicle_select_fn_80060704(CONTROLLER_STATE *input);
sint32 vehicle_select_fn_80061754(void);
sint32 vehicle_select_fn_800618d0(CONTROLLER_STATE *input);
sint32 vehicle_select_fn_80061a90(void);
sint32 vehicle_select_fn_80061bb4(void);
sint32 vehicle_select_fn_80061d7c(sint16 value);
sint32 vehicle_select_fn_80061d88(void);
sint32 vehicle_select_fn_80061de4(CONTROLLER_STATE *input);
void vehicle_select_fn_80061e38(void);
sint32 vehicle_select_fn_80061e40(void);
sint32 vehicle_select_fn_80062144(CONTROLLER_STATE *input);
void vehicle_select_fn_8006238c(void);
sint32 vehicle_select_fn_80062394(void);
sint32 profile_cell_dispatch_complete(void);
sint32 profile_index_map(void);
sint32 vehicle_select_init_player_profile(uint32 slot);
sint32 vehicle_select_fn_800630f8(void);

#endif /* RR_VEHICLE_SELECT_H */
