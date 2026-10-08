#ifndef RR_MENU_H
#define RR_MENU_H

#include "input.h"

#include "psx.h"

typedef struct
{
    uint32 frame;
} MENU_STATE;

typedef struct
{
    uint8 brightness;
    uint8 falling;
} MENU_PULSE;

extern MENU_PULSE menu_pulse;

extern MENU_STATE menu_state;

sint32 menu_run_pause(uint32 context);
sint32 menu_init_layout(void);
sint32 menu_fn_80020058(uint32 state);
sint32 menu_dispatch_layout(uint32 state);
sint32 race_update_segment_bufs(uint32 state, sint32 argument);
sint32 menu_submit_recs_refresh(uint32 unused, uint32 ordering_table);
sint32 menu_dispatch_state_render_mode(uint32 state, uint32 ordering_table, sint32 result);
sint32 race_render_segment_update(uint32 state, uint32 ordering_table);
sint32 menu_render_multiplayer_result(uint32 state, uint32 ordering_table);

sint32 rec_transition_states(uint32 records);
sint32 menu_update_select_recs(uint32 records);
sint32 menu_update_selected_prims(sint16 selected);
sint32 menu_render_select(sint32 selected, uint32 state, CONTROLLER_STATE *inherited_menu_input);
sint32 menu_select_result_clut(uint32 state);
sint32 menu_update_status_clut(uint32 state);
sint32 menu_set_group_rec_state(sint16 group, uint8 state);
sint32 menu_fn_80021e20(uint32 state);
sint32 menu_update_result_rec_palettes(uint32 state);

sint32 menu_update_select_mode(void);
sint32 menu_init_race(uint32 state, uint32 unused1, sint32 unused2);
sint32 menu_update_trans(uint32 state, uint32 menu, sint32 unused, uint32 argument);
sint32 menu_outer_loop(void);
sint32 menu_reset_display(void);
sint32 menu_config_display(sint16 width, sint16 height);
sint32 menu_select_special_screen_assets(void);
sint32 menu_format_request_labels(void);
sint32 menu_set_resource_prefix(void);
sint32 rec_stop_voices(uint32 prefix);
void menu_fn_80044c18(void);
void menu_fn_80044c28(void);
sint32 menu_prepare_assets(uint32 prefix);
void menu_reset_display_flags(void);
sint32 menu_select_upload_images(void);
sint32 menu_startup_tim_display(void);
sint32 menu_load_tex(uint32 state, uint32 prefix);
sint32 menu_render_frame(void);
sint32 menu_load_course_tex(sint16 requested);
sint32 menu_update_tex_select(void);
sint32 menu_cleanup_tex(void);
uint32 menu_clear_rec_bytes(void);
sint32 menu_fn_8004c28c(sint16 mode);
uint32 menu_propagate_vis(void);
uint32 menu_fn_8004c614(void);
void menu_fn_8004c71c(void);
uint32 menu_update_flag_index(sint32 index, sint8 value);
sint32 menu_fn_8004c740(CONTROLLER_STATE *input);
sint32 mc_state_is_three(void);
void menu_init_runtime(void);
sint32 menu_checkpoints_pop2(void);
sint32 menu_frame_loop(void);
sint32 menu_loop(CONTROLLER_STATE *input);
sint32 menu_alloc_workspace(void);
sint32 menu_pop_active_workspace(void);
sint32 menu_load_localized_data(void);
sint32 menu_prepare_resource_tables(void);
sint32 menu_init(void);
sint32 menu_alloc_work_bufs(void);
sint32 menu_fn_8004dbec(void);
sint32 menu_start_audio_bank(void);
sint32 menu_tex_refresh_if_needed(void);
sint32 menu_stop_voice(void);
uint32 menu_alloc_runtime_buf(void);
sint32 menu_fn_8004dd10(void);
sint32 guest_copy_bytes_forward(uint32 destination, uint32 source, uint16 size);
sint32 menu_rebuild_config(sint16 selection);
sint32 menu_apply_state_trans(void);
sint32 menu_update_desc_select(void);
sint32 menu_is_halfword_zero(CONTROLLER_STATE *value);
sint32 menu_lookup_key_mapping(sint32 index);
sint32 menu_restore(sint16 configuration_index);
sint32 menu_save_desc_payloads(sint16 configuration_index);
sint32 menu_process_input_commands(CONTROLLER_STATE *input);

sint32 sprite_deactivate_rec_tree(uint32 descriptor);
sint32 sprite_activate_rec_tree(uint32 descriptor);
sint32 menu_prepare_rec_groups(void);
sint32 menu_config(void);
sint32 menu_dispatch_state(void);
sint32 menu_dispatch_state_jump_table(sint32 value);
sint32 menu_update_state(CONTROLLER_STATE *input);
sint32 menu_update_group_desc_select(sint16 configuration_index, sint16 descriptor_index, sint16 value);

sint32 menu_set_group_desc_value(sint16 configuration_index, sint16 descriptor_index, sint16 value);
sint32 menu_refresh_language_labels(void);
sint32 menu_update_language_labels(void);
sint32 menu_fn_80050094(sint32 refresh);
sint32 menu_fn_80050198(void);

sint32 menu_fn_800501b8(void);
sint32 menu_fn_800501d8(void);
void menu_fn_800501f8(void);
sint32 menu_scale_sprite_recs(void);
sint32 menu_update_audio_volume(CONTROLLER_STATE *input);
sint32 menu_fn_80050440(sint16 configuration_index, sint16 selection);
sint32 menu_increment_group_offset(sint16 index, sint16 group);
sint32 menu_query_group_value(sint16 configuration_index, sint16 descriptor_index);
sint32 menu_has_special_ctrl(void);
sint32 menu_update_ctrl_layout(void);
sint32 menu_refresh_ctrl_layout(void);
sint32 menu_init_render(void);
sint32 menu_select_next_ctrl_state(void);
sint32 menu_fn_80052830(CONTROLLER_STATE *input);
sint32 menu_fn_80052aa4(void);
sint32 menu_fn_80052ac8(void);
sint32 menu_fn_80052c00(void);

sint32 menu_fn_80052c28(void);
sint32 menu_fn_80052f58(void);
sint32 menu_fn_800531e8(void);
sint32 menu_enable_text_entries(void);
sint32 menu_reset_tex_state(void);
sint32 menu_init_mode(void);
sint32 menu_dispatch_frame(uint32 unused1, uint32 second, uint32 third, uint32 fourth);
sint32 menu_build_player_mode_table(void);
sint32 menu_config_text_ptrs(void);
void menu_fn_8006776c(void);
int menu_run_iteration(CONTROLLER_STATE *input, uint8 *warmup);

sint32 menu_after_loop(void);
sint32 menu_after_entry(void);
sint32 menu_after_outer(void);
void menu_after_swap(uint8 *warmup);

#endif /* RR_MENU_H */
