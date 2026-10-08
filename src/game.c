#include "name.h"
#include "ai.h"
#include "psx_gpu.h"
#include "camera.h"
#include "display.h"
#include "effects.h"
#include "input.h"
#include "mc.h"
#include "mesh.h"
#include "object.h"
#include "pickup.h"
#include "perf.h"
#include "profile.h"
#include "race_events.h"
#include "route.h"
#include "scene.h"
#include "vehicle.h"
#include "motion.h"
#include "vehicle_select.h"
#include "cd.h"
#include "results.h"
#include "sound.h"
#include "arena.h"
#include "callbacks.h"
#include "intro.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include "runtime.h"
#include "global.h"
#include "render.h"
#include "game.h"
#include "xport_trace.h"

static void game_run_startup_constructors(void)
{
}

static void game_init_backup_unit(void)
{
}

static void race_debug_line(void)
{
    const BOAT *boat = vehicle_players[0];
    char line[64];
    int length = snprintf(line, sizeof(line), "depth %d mat %d\n", (sint32)((uint32)boat->contacts.points[3].height + (uint32)boat->contacts.points[4].height), boat->motion.transform.pose.t[1]);

    if (length >= 0 && (uint32)length < sizeof(line))
        guest_debug_append(line, (uint32)length);
}

void game_start(void)
{
    FUNCTION_MARKER(0x80010184u, "MAIN.EXE");
    game_run_startup_constructors();
    ResetCallback();
    ResetGraph(0);
    SetGraphDebug(0);
    global_fn_80068900(1u);
    InitGeom();
    input_init();
    mc_fn_80074d30(1u);
    mc_fn_80074d84();
    game_init_backup_unit();
    game_reset_checkpoint_stack();
    cd_init_drive();
    cb_init_vblank_timer();
    cb_init_pool();
    game_init_defaults();
    w_u32(0x800DE0F4u, 0u);
    w_u32(0x800DF09Cu, 1u);
    w_u32(0x800DE0F0u, 0u);
    w_u32(0x800DF098u, 0u);
    sound_init_audio();
#if 0
    perf_init(0);
#endif
    game_main_loop_continue(0);
}

sint32 race_loop(uint32 first, uint32 second, uint32 configuration)
{
    PSX_RECT clear_area;
    sint32 warmup = 2;

    FUNCTION_MARKER(0x800103C4u, "MAIN.EXE");
    clear_area.x = (sint16)r_u16(0x800B3DACu);
    clear_area.y = (sint16)r_u16(0x800B3DAEu);
    clear_area.w = (sint16)r_u16(0x800B3DB0u);
    clear_area.h = (sint16)r_u16(0x800B3DB2u);
    DrawSync(0);
    cb_wait_vblank();
    SetDispMask(0);
    ClearImage(&clear_area, 0u, 0u, 0u);
    DrawSync(0);
    cb_wait_vblank();
    if (r_u32(configuration) == 1u)
    {
        PutDispEnv((DISPENV *)psx_addr(first + 196u, sizeof(DISPENV)));
        PutDrawEnv((DRAWENV *)psx_addr(first + 104u, sizeof(DRAWENV)));
    }
    VSync(0);
    w_u16(0x800B3D94u, 0u);
    w_u32(0x800B3DA8u, 0u);
    if ((sint16)r_u16(0x800E05D6u) == 1)
        sound_queue_command(0x800DE0F0u, 11, 0, 0u);
    return race_loop_resume(first, second, configuration, warmup);
}

sint32 game_render_frame(void)
{
    sint32 players = (sint32)r_u32(0x80083478u);
    sint32 index;

    FUNCTION_MARKER(0x800107A4u, "MAIN.EXE");
#if 0
    perf_frame_end();
    {
        sint16 buffer = (sint16)r_u32(0x800B3DA8u);
        uint32 overlay_entry = 0x800DE238u + 1784u * (uint32)(sint32)buffer;

        perf_render_graph(overlay_entry, buffer);
        perf_render_current(overlay_entry, buffer);
    }
#endif
    if (players == 1 && r_u32(0x800B3D88u) == r_u32(0x800B69DCu))
    {
        uint32 state = 0x800DE0F0u;
        uint32 buffer_index;

        cb_sync_video_field();
        ResetGraph(1);
        buffer_index = r_u32(0x800B3DA8u);
        render_fn_8006c434(state + 1784u * (uint32)buffer_index + 1792u);
        if (r_u32(0x800B6C20u) != 0u)
        {
            buffer_index = r_u32(0x800B3DA8u);
            effects_capture_screen_region(state, buffer_index);
            buffer_index = r_u32(0x800B3DA8u);
            render_fn_8006c434(state + 1784u * (uint32)buffer_index + 1740u);
        }
    }
    else
    {
        uint32 buffer_index;

        VSync(2);
        ResetGraph(1);
        buffer_index = r_u32(0x800B3DA8u);
        PutDrawEnv((DRAWENV *)psx_addr(0x800DE158u + 1784u * (uint32)buffer_index, sizeof(DRAWENV)));
        buffer_index = r_u32(0x800B3DA8u);
        PutDispEnv((DISPENV *)psx_addr(0x800DE1B4u + 1784u * (uint32)buffer_index, sizeof(DISPENV)));
        for (index = 0; index < (sint32)r_u32(0x80083478u); ++index)
        {
            uint32 state = index == 0 ? 0x800DE0F0u : 0x800DF098u;

            buffer_index = r_u32(0x800B3DA8u);
            render_fn_8006c434(state + 1784u * (uint32)buffer_index + 1792u);
            if (r_u32(0x800B6C20u) != 0u)
            {
                buffer_index = r_u32(0x800B3DA8u);
                effects_capture_screen_region(state, buffer_index);
                buffer_index = r_u32(0x800B3DA8u);
                render_fn_8006c434(state + 1784u * (uint32)buffer_index + 1740u);
            }
        }
    }
    // Host display adapter after original 0x80010960 frame submission
    gpu_present();
    return 0;
}

sint32 race_init_runtime(uint32 first, uint32 second, uint32 context)
{
    uint32 first_state;
    uint32 first_output_state;
    uint32 second_state;
    uint32 second_output_state;
    uint32 call_argument;
    uint32 mode;
    sint32 font;
    sint32 half_period;
    uint8 parameter = 5u;

    FUNCTION_MARKER(0x80010DDCu, "MAIN.EXE");
    w_u32(0x800B69DCu, r_u32(0x80083478u) == 2u ? (uint32)((sint32)r_u32(0x800B3D88u) / 2) : r_u32(0x800B3D88u));
    w_u32(context + 708u, 0u);
    half_period = (sint32)r_u32(0x800B3D88u) / 2;
    w_u32(0x800B6A4Cu, 0u);
    motion_half_period = (sint32)((uint32)half_period);
    render_alloc_mode_bufs();
    render_build_tex_descs();
    if (r_u32(0x8008348Cu) != 2u)
        w_u32(0x800B6C20u, 1u);
    if (r_u32(0x80083484u) == 8u)
        race_events_fn_8003b508();
    else
    {
        render_select_tex_desc(context);
        mode = r_u32(0x80083484u);
        call_argument = r_u32(context + 40u);
        ai_config_template_desc((sint32)call_argument, mode == 5u ? 4u : r_u8(0x800E0596u));
        object_node_flags_update_linked(r_u32(0x800B6A98u));
        game_select_state(context);
        route_assign_topology_states();
    }
    vehicle_init_slots();
    first_state = vehicle_legacy(vehicle_player(first));
    first_output_state = vehicle_legacy(vehicle_player(first));
    vehicle_tracks(first)[1] = NULL;
    vehicle_tracks(first)[0] = NULL;
    w_u32(first + 100u, first_state + 1720u);
    w_u32(first_output_state + 2348u, 0x800B6B20u);
    call_argument = r_u32(context + 36u);
    menu_init_race(first, call_argument, 0);
    camera_init(camera_for_view(first));
    if (r_u32(0x80083478u) == 2u)
    {
        second_state = vehicle_legacy(vehicle_player(second));
        second_output_state = vehicle_legacy(vehicle_player(second));
        vehicle_tracks(second)[1] = NULL;
        vehicle_tracks(second)[0] = NULL;
        w_u32(second + 100u, second_state + 1720u);
        w_u32(second_output_state + 2348u, 0x800B6B22u);
        call_argument = r_u32(context + 36u);
        menu_init_race(second, call_argument, 1);
        camera_init(camera_for_view(second));
    }
    else
    {
        vehicle_players[1] = NULL;
        w_u32(second + 100u, 0u);
        vehicle_tracks(second)[1] = NULL;
        vehicle_tracks(second)[0] = NULL;
    }
    ai_sort_racers_by_rank(first, second);
    pickup_write_racer_indices(0x800E059Du);
    render_build_tex_desc_table((sint32)r_u32(context + 40u));
    render_init_prims(first + 104u, context);
    render_init_prims(first + 1888u, context);
    if (r_u32(context) == 2u)
    {
        render_init_prims(second + 104u, context);
        render_init_prims(second + 1888u, context);
    }
    if (r_u32(0x80083484u) != 8u)
        object_init_racer_object_groups(first, second, (sint32)r_u32(context));
    mode = r_u32(context);
    call_argument = r_u32(context + 16u);
    render_init_context((sint32)mode, call_argument);
    display_init_envs();
    render_init_ui_prims();
    runtime_load_font(960, 256);
    if (r_u32(context) == 1u)
        font = runtime_open_font(30, 200, 250, 230, 0, 512);
    else
        font = runtime_open_font(30, 32, 330, 50, 2, 512);
    runtime_set_font_dump(font);
    render_build_curve_scaled_lut();
    mesh_init_tex_templates();
    if (r_u32(0x80083484u) != 8u)
        ai_assign_ctrl_states(0x800DE0F0u, 0x800DF098u);
    w_u32(0x800B6B30u, 2u);
    w_u32(0x800B6B28u, 0u);
    CdControlB(14u, &parameter, 0);
    VSync(3);
    return (sint32)cd_set_ready_cb(0x80038950u);
}

sint32 game_select_state(uint32 value)
{
    sint32 result = r_u32(value + 0x14u) == 1u ? 5 : 3;

    FUNCTION_MARKER(0x80018C40u, "MAIN.EXE");
    w_u32(0x800B67A0u, (uint32)result);
    return result;
}

sint32 game_init_defaults(void)
{
    sint32 index;

    FUNCTION_MARKER(0x80053A60u, "MAIN.EXE");
    w_u8(0x800E058Au, 0u);
    w_u8(0x800E058Du, 0u);
    for (index = 0; index < 3; ++index)
    {
        profile_at((uint32)index)->reset_pending = 1u;
        profile_at((uint32)index)->reward_flags = 0u;
    }
    profile_at(3u)->reset_pending = 1u;
    profile_at(3u)->reward_flags = 0u;
    for (index = 0; index < 18; ++index)
    {
        uint32 record = 0x800E0BC0u + 14u * (uint32)index;
        w_u8(record, 50u);
        w_u8(record + 1u, 50u);
        w_u16(record + 2u, 0u);
        w_u16(record + 4u, 0u);
        w_u16(record + 6u, 0u);
        w_u16(record + 8u, 0u);
        w_u16(record + 10u, 0u);
        w_u16(record + 12u, 0u);
    }
    profile_generate_opponents();
    for (index = 0; index < 8; ++index)
        w_u16(0x800E05BCu + 2u * (uint32)index, r_u16(0x80098D1Cu + 2u * (uint32)index));
    w_u8(0x800E058Eu, 3u);
    w_u16(0x800E0590u, 5u);
    w_u16(0x800E0592u, 0u);
    name_reset_players();
    w_u16(0x800E05D0u, 40u);
    w_u16(0x80083490u, 40u);
    w_u16(0x800E05CEu, 63u);
    w_u16(0x80083494u, 63u);
    vehicle_racer_count = (uint32)(8u);
    vehicle_leader_count = (uint16)(8u);
    vehicle_trailer_count = (uint16)(0u);
    w_u16(0x800E1B86u, 0u);
    w_u8(0x800E1B88u, 0u);
    w_u16(0x800E1B84u, 0u);
    w_u8(0x800E1B8Au, 0u);
    input_calibrations[0].steer_center = (uint16)(0u);
    input_calibrations[0].steer_range = (uint16)(4000u);
    input_calibrations[0].accel_range = (uint16)(3500u);
    input_calibrations[0].brake_range = (uint16)(3500u);
    input_calibrations[1].steer_center = (uint16)(0u);
    input_calibrations[1].steer_range = (uint16)(4000u);
    input_calibrations[1].accel_range = (uint16)(3500u);
    input_calibrations[1].brake_range = (uint16)(3500u);
    input_calibrations[0].stick_center = (uint16)(0u);
    input_calibrations[0].stick_range = (uint16)(4000u);
    input_calibrations[1].stick_center = (uint16)(0u);
    input_calibrations[1].stick_range = (uint16)(4000u);
    for (index = 0; index < 5; ++index)
        vehicle_select_init_player_profile((uint32)index);
    w_u8(0x800E1B8Bu, 0u);
    name_reels.code = (uint32)game_random_next() & 0xFFFFFu;
    return (sint32)name_reels.code;
}

void guest_debug_append(const char *text, uint32 length)
{
    uint32 capacity = r_u32(0x800A0074u);
    uint32 output = r_u32(0x800A007Cu);
    uint32 used = r_u32(0x800A0080u);
    uint32 index;

    if (used > capacity)
        return;
    for (index = 0u; index < length; ++index)
    {
        w_u8(output + used, (uint8)text[index]);
        ++used;
        if (used > capacity)
        {
            w_u32(0x800A0080u, used);
            return;
        }
    }
    w_u8(output + used, 0u);
    w_u32(0x800A0080u, used);
}

sint32 race_loop_resume(uint32 first, uint32 second, uint32 configuration, sint32 warmup)
{
    RR_RACE_LOOP_STATE state;
    sint32 step;

    race_begin_loop(&state, first, second, configuration, warmup);
    while ((step = race_loop_before_frame(&state)) > 0)
        race_loop_after_frame(&state);
    return step < 0 ? 0 : race_finish_loop(&state);
}

void race_begin_loop(RR_RACE_LOOP_STATE *state, uint32 first, uint32 second, uint32 configuration, sint32 warmup)
{
    state->first = first;
    state->second = second;
    state->configuration = configuration;
    state->warmup = warmup;
}

sint32 race_loop_before_frame(RR_RACE_LOOP_STATE *state)
{
    sint32 buffer;
    sint32 dispatch_argument;
    uint32 first = state->first;
    uint32 second = state->second;
    uint32 configuration = state->configuration;

#if 0
    perf_begin_frame();
#endif
    race_debug_line();
    // Host quit boundary for original loop at 0x800104AC
    if (xport_isquit())
        return -1;
    if (r_u32(configuration) == 1u && r_u32(0x800B3D88u) == r_u32(0x800B69DCu))
    {
        w_u16(0x800B3D94u, (uint16)(r_u16(0x800B3D94u) + 2u));
        if (r_u16(0x800B3D94u) == 65022u)
            w_u16(0x800B3D94u, 0u);
        buffer = 1 - (sint32)r_u32(0x800B3DA8u);
        w_u32(0x800B3DA8u, (uint32)buffer);
        if (buffer != 0)
        {
            input_update_states();
            if (state->warmup == 0 && menu_run_pause(first) != 0)
                return 0;
            vehicle_input(vehicle_player(first), first);
        }
        dispatch_argument = buffer;
    }
    else
    {
        w_u16(0x800B3D94u, (uint16)(r_u16(0x800B3D94u) + 4u));
        if (r_u16(0x800B3D94u) == 65022u)
            w_u16(0x800B3D94u, 0u);
        buffer = 1 - (sint32)r_u32(0x800B3DA8u);
        w_u32(0x800B3DA8u, (uint32)buffer);
        input_update_states();
        if (state->warmup == 0 && (menu_run_pause(first) != 0 || menu_run_pause(second) != 0))
            return 0;
        vehicle_input(vehicle_player(first), first);
        vehicle_input(vehicle_player(second), second);
        race_update_racers(0);
        dispatch_argument = 1;
    }
    race_update_racers(dispatch_argument);
#if 0
    perf_mark_frame();
#endif
    if (r_u32(0x80083484u) == 8u)
        vehicle_update_route_segments(first);
    else if (r_u32(0x80083484u) == 5u && r_u16(0x800B3D94u) >= 0x0E10u)
        return 0;
    if ((sint16)r_u16(0x800E0580u) != 0)
        return 0;
    render_select_prim_pool(buffer);
    cb_dispatch_list();
    cb_traverse_list(0x800DE0F0u);
    cb_traverse_list(0x800DF098u);
#if 0
    perf_mark_frame();
#endif
    camera_update(camera_for_view(first), vehicle_player(first));
    scene_render_player_scene(first, buffer);
#if 0
    perf_mark_frame();
#endif
    if (r_u32(0x80083484u) == 5u)
        cd_update_audio(1);
    else
        vehicle_update_audio(first, 0u, 0u, 0u);
    if (r_u32(configuration) == 2u)
    {
        camera_update(camera_for_view(second), vehicle_player(second));
        scene_render_player_scene(second, buffer);
        if (r_u32(0x80083484u) != 5u)
            vehicle_update_audio(second, 0u, 0u, 0u);
    }
#if 0
    perf_mark_frame();
#endif
    return 1;
}

void race_loop_after_frame(RR_RACE_LOOP_STATE *state)
{
    game_render_frame();
    if (state->warmup != 0 && --state->warmup == 0)
        SetDispMask(1);
}

sint32 race_finish_loop(RR_RACE_LOOP_STATE *state)
{
    w_u32(0x800B3DA8u, 1u - r_u32(0x800B3DA8u));
    if (r_u32(0x8008373Cu) == 0u && r_u32(0x80083478u) == 1u)
        display_sync_swap_buf();
    pickup_write_racer_indices(0x800E059Du);
    return effects_run_expanding_circle_trans();
}

void game_main_loop_continue(sint32 menu_already_returned)
{
    for (;;)
    {
        if (xport_isquit())
            return;
        if (!menu_already_returned)
        {
            game_push_checkpoint();
            game_push_checkpoint();
            if ((sint16)r_u16(0x800E0580u) == -5)
                w_u16(0x800E0580u, 0u);
            else
                menu_update_select_mode();
        }
        menu_already_returned = 0;
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        game_pop_checkpoint();
        race_config_results();
        w_u32(0x800B6988u, 1u);
        w_u32(0x800B6C20u, 0u);
        cd_load_course_assets(0x80083478u);
        spu_init_cd_audio();
        race_init_runtime(0x800DE0F0u, 0x800DF098u, 0x80083478u);
        w_u32(0x800B6BB4u, guest_swap_stack_ptr(0x1F8003C0u));
        effects_register_mode();
        race_loop(0x800DE0F0u, 0x800DF098u, 0x80083478u);
        // Preserve the existing host quit boundary before scene cleanup
        if (xport_isquit())
            return;
        cb_clear_active_list();
        cb_clear_list(0x800DE0F0u);
        cb_clear_list(0x800DF098u);
        guest_swap_stack_ptr(r_u32(0x800B6BB4u));
        sound_config_reverb_depth(0u, 0u);
        SpuSetReverb(0);
        sound_reset_recs(0x800DE0F0u);
        if (r_u32(0x80083478u) == 2u)
            sound_reset_recs(0x800DF098u);
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        game_pop_checkpoint();
        menu_startup_tim_display();
    }
}
