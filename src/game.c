#include "game.h"
#include "ai.h"
#include "arena.h"
#include "boat.h"
#include "callbacks.h"
#include "camera.h"
#include "cd.h"
#include "disp.h"
#include "effects.h"
#include "hd.h"
#ifdef _DEBUG
    #include "editor_vis.h"
#endif
#include "hud.h"
#include "input.h"
#include "menu.h"
#include "menu_pause.h"
#include "mesh.h"
#include "movie.h"
#include "name.h"
#include "object.h"
#include "perf.h"
#include "pickup.h"
#include "profile.h"
#include "psx_gpu.h"
#include "race.h"
#include "race_stats.h"
#include "render.h"
#include "river.h"
#include "route.h"
#include "save.h"
#include "sound.h"
#include "track.h"
#include "upgrades.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    const RACE_VIEW *view0;
    const RACE_VIEW *view1;
    const RACE_CFG *cfg;
    sint32 warmup;
    uint64 clock_idx;
    uint32 clock_reset;
    sint32 hd;
} RACE_LOOP;

// Timing from MAIN.EXE 800B3D88/800B69DC
GAME_TIMING game_timing = {50, 0};

uint64 game_frame_index;
float game_frame_fract;

const RACE_VIEW race_views[2] = {{0}, {1}};

RACE_CFG race_cfg = {1, {0, 1}, 3, 1, 1, 0};
uint32 race_paused;
uint32 race_start_phase;
static sint32 game_hd_frame;
static uint32 game_clock_reset;

GAME_SELECTION game_selection;

// Race choices from MAIN.EXE 800E059D/800E05AD, AI variant 800E0596 and format 800E05D6
RACE_SELECTION race_selection;

// Participant records from MAIN.EXE 800E0B40
RACE_PARTICIPANT race_participants[16];

// Race options from MAIN.EXE 800E0598..059C
// Additional options from MAIN.EXE 800E0590/94 and 800E05B2
GAME_OPTIONS game_options;

sint32 game_update_racer_modes(const RACE_VIEW *state)
{
    uint16 marker;
    uint32 val;
    sint32 participant_count;
    sint32 mode;
    sint32 idx;

    FUNCTION_MARKER(0x8002CFA8, "MAIN.EXE");
    marker = game_timing.ticks;
    val = track_start->event_tick;
    if (marker == val)
    {
        effects_init_transform_object(state);
        sound_queue_command(sound_for_context(&race_views[0]), 11, 0);
    }
    participant_count = boat_racer_count;
    if (participant_count > 0)
    {
        sint32 loop_mode = race_start_phase;

        idx = 0;
        do
        {
            BOAT *object = boat_racers[idx];
            object->race.phase = object->race.phase == 1 ? 2 : 1;
            if (loop_mode == 1)
                object->race.mode = 3;
            ++idx;
        } while (idx < boat_racer_count);
    }
    mode = race_start_phase;
    if (mode == 0)
        return 1;
    if (mode == 1)
    {
        camera_views[0].mode = 7;
        camera_attract.phase = 30;
    }
    else if (mode == 4)
    {
        BOAT_MENU *menu = boat_player_menu(&race_views[0]);

        menu->mode = mode;
        return sound_queue_command(sound_for_context(&race_views[0]), 10, 0);
    }
    mode = race_start_phase;
    if (mode <= 0)
    {
        return mode;
    }

    {
        BOAT *object = boat_player(state);

        if (object->race.phase == 2)
        {
            uint32 next;

            object->race.mode = 4;
            object = boat_player(state);
            boat_init_motion(object);
            object = boat_player(state);
            object->route.speed = 0;
            object = boat_player(state);
            val = race_start_phase;
            next = object->setup.throttle;
            val += 1;
            race_start_phase = val;
            object->control.throttle = next;
        }
    }
    {
        BOAT *object = boat_tracks(state)[0];

        if (object->race.phase == 2)
        {
            object->race.mode = 5;
            object = boat_tracks(state)[0];
            boat_init_motion(object);
            val = race_start_phase + 1;
            race_start_phase = val;
        }
    }
    {
        BOAT *object = boat_tracks(state)[1];

        if (object->race.phase == 2)
        {
            object->race.mode = 5;
            object = boat_tracks(state)[1];
            boat_init_motion(object);
            val = race_start_phase + 1;
            race_start_phase = val;
            return val;
        }
    }
    return 5;
}

static void race_debug_line(void)
{
    const BOAT *boat = boat_players[0];
    char line[64];
    int len = snprintf(line, sizeof(line), "depth %d mat %d\n", boat->contacts.points[3].height + boat->contacts.points[4].height, boat->pose.t[1]);

    if (len >= 0 && len < sizeof(line))
        FntPrint("%s", line);
}

static sint32 game_init_defaults(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x80053A60, "MAIN.EXE");
    game_selection.ready = 0;
    game_selection.menu_variant = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        profile_at(idx)->reset_pending = 1;
        profile_at(idx)->reward_flags = 0;
    }
    profile_at(3)->reset_pending = 1;
    profile_at(3)->reward_flags = 0;
    for (idx = 0; idx < 18; ++idx)
    {
        PROFILE_SUMMARY *rec = &profile_summaries[idx];
        rec->rank = 50;
        rec->profile = 50;
        rec->lap_time[0] = 0;
        rec->lap_time[1] = 0;
        rec->lap_time[2] = 0;
        rec->race_time[0] = 0;
        rec->race_time[1] = 0;
        rec->race_time[2] = 0;
    }
    profile_generate_opponents();
    for (idx = 0; idx < 8; ++idx)
        sound_options.tracks[idx] = sound_default_tracks[idx];
    game_selection.players = game_selection.players & 0xFF00u | 3;
    game_options.round_limit = 5;
    game_options.level = 0;
    name_reset_players();
    sound_options.effects = 40;
    sound_mix.effects = 40;
    sound_options.music = 63;
    sound_mix.music = 63;
    boat_racer_count = 8;
    boat_leader_count = 8;
    boat_trailer_count = 0;
    profile_selection.menu_slot = 0;
    profile_selection.mode_choice = 0;
    profile_selection.rules_slot = 0;
    profile_selection.pad = 0;
    input_calibrations[0].steer_center = 0;
    input_calibrations[0].steer_range = 4000;
    input_calibrations[0].accel_range = 3500;
    input_calibrations[0].brake_range = 3500;
    input_calibrations[1].steer_center = 0;
    input_calibrations[1].steer_range = 4000;
    input_calibrations[1].accel_range = 3500;
    input_calibrations[1].brake_range = 3500;
    input_calibrations[0].stick_center = 0;
    input_calibrations[0].stick_range = 4000;
    input_calibrations[1].stick_center = 0;
    input_calibrations[1].stick_range = 4000;
    for (idx = 0; idx < 5; ++idx)
        upgrades_init_player_profile(idx);
    game_selection.attract = 0;
    name_reels.code = random_next() & 0xFFFFF;
    return name_reels.code;
}

void game_sample_frame(void)
{
    uint64 elapsed = xport_timer_get();
    uint64 seconds = elapsed / 1000000 * FIELD_RATE_NUMERATOR;
    uint64 fraction = seconds % FIELD_RATE_DENOMINATOR * 1000000 + elapsed % 1000000 * FIELD_RATE_NUMERATOR;
    uint64 period = (uint64)1000000 * FIELD_RATE_DENOMINATOR;
    game_frame_index = seconds / FIELD_RATE_DENOMINATOR + fraction / period;
    game_frame_fract = (float)(fraction % period) / (float)period;
}

void game_sync_frame(void)
{
    ++game_clock_reset;
}

static void race_begin_loop(RACE_LOOP *state, const RACE_VIEW *view0, const RACE_VIEW *view1, const RACE_CFG *cfg, sint32 warmup)
{
#ifdef _DEBUG
    editor_vis_begin();
#endif
    state->view0 = view0;
    state->view1 = view1;
    state->cfg = cfg;
    state->warmup = warmup;
    state->clock_idx = 0;
    state->clock_reset = game_clock_reset;
    state->hd = 0;
}

static sint32 race_loop_before_frame(RACE_LOOP *state, sint32 hd)
{
    sint32 buffer;
    sint32 dispatch_argument;
    const RACE_VIEW *view0 = state->view0;
    const RACE_VIEW *view1 = state->view1;
    const RACE_CFG *cfg = state->cfg;

#if 0
    perf_begin_frame();
#endif
    race_debug_line();
    // Host quit boundary for original loop at 0x800104AC
    if (xport_isquit())
        return -1;
    game_hd_frame = hd;
    gpu_set_raster(!hd);
    game_timing.frame_rate = cfg->players == 2 && !hd ? game_timing.base_rate / 2 : game_timing.base_rate;
    if (game_timing.base_rate == game_timing.frame_rate)
    {
        game_timing.ticks = game_timing.ticks + 2u;
        if (game_timing.ticks == 65022)
            game_timing.ticks = 0;
        buffer = 1 - disp_state.scene_buffer;
        disp_state.scene_buffer = buffer;
        if (buffer != 0 || hd)
        {
            input_update_states();
#ifdef _DEBUG
            editor_vis_pad(&input_pads[0]);
#endif
            if (!race_stats.active && state->warmup == 0 && (menu_pause_run(view0) != 0 || (cfg->players == 2 && menu_pause_run(view1) != 0)))
                return 0;
            if (hd)
            {
                hd_boat_input(boat_player(view0), view0);
                if (cfg->players == 2)
                    hd_boat_input(boat_player(view1), view1);
            }
            else
            {
                boat_input(boat_player(view0), view0);
                if (cfg->players == 2)
                    boat_input(boat_player(view1), view1);
            }
        }
        dispatch_argument = buffer;
    }
    else
    {
        game_timing.ticks = game_timing.ticks + 4u;
        if (game_timing.ticks == 65022)
            game_timing.ticks = 0;
        buffer = 1 - disp_state.scene_buffer;
        disp_state.scene_buffer = buffer;
        input_update_states();
#ifdef _DEBUG
        editor_vis_pad(&input_pads[0]);
#endif
        if (state->warmup == 0 && (menu_pause_run(view0) != 0 || menu_pause_run(view1) != 0))
            return 0;
        boat_input(boat_player(view0), view0);
        boat_input(boat_player(view1), view1);
        race_update_racers(0);
        dispatch_argument = 1;
    }
    if (state->clock_reset != game_clock_reset)
    {
        game_sample_frame();
        state->clock_idx = game_frame_index;
        state->clock_reset = game_clock_reset;
        if (hd)
            hd_sync_proj();
    }
    input_update_vibration(view0);
    if (cfg->players == 2)
        input_update_vibration(view1);
    race_update_racers(dispatch_argument);
    race_stats_update();
#if 0
    perf_mark_frame();
#endif
    if (race_cfg.mode == 8)
        boat_update_route_segs(view0);
    else if (race_cfg.mode == 5 && game_timing.ticks >= 0x0E10u)
        return 0;
    if (game_selection.selection != 0)
        return 0;
    render_select_prim_pool(buffer);
    cb_dispatch_list();
    cb_traverse_list(&cb_players[0]);
    cb_traverse_list(&cb_players[1]);
#if 0
    perf_mark_frame();
#endif
    camera_update(camera_for_view(view0), boat_player(view0));
    if (game_hd_frame)
    {
        hd_update_player(view0);
    }
    else
        track_render_player(view0, buffer);
#if 0
    perf_mark_frame();
#endif
    if (race_cfg.mode == 5)
        cd_update_audio(1);
    else
        boat_update_audio(view0);
    if (cfg->players == 2)
    {
        camera_update(camera_for_view(view1), boat_player(view1));
        if (game_hd_frame)
        {
            hd_update_player(view1);
        }
        else
            track_render_player(view1, buffer);
        if (race_cfg.mode != 5)
            boat_update_audio(view1);
    }
    if (hd)
        hd_capture_frame();
#if 0
    perf_mark_frame();
#endif
    return 1;
}

static void race_loop_after_frame(RACE_LOOP *state)
{
    if (state->warmup != 0 && --state->warmup == 0)
        SetDispMask(1);
}

static sint32 race_finish_loop(RACE_LOOP *state)
{
    sint32 hd = xport_hd != 0;
    sint32 result;
    if (hd && !hd_ready())
        hd_seed();
    game_hd_frame = 0;
    disp_state.scene_buffer = 1 - disp_state.scene_buffer;
    if (!hd && race_paused == 0 && race_cfg.players == 1)
        disp_sync_swap_buf();
    pickup_write_racer_indices(race_selection.ranks);
    result = hd ? hd_effects_trans() : effects_run_expanding_circle_trans();
    hd_reset();
    return result;
}

static sint32 race_loop_resume(const RACE_VIEW *view0, const RACE_VIEW *view1, const RACE_CFG *cfg, sint32 warmup)
{
    RACE_LOOP state;
    sint32 step;

    race_begin_loop(&state, view0, view1, cfg, warmup);
    step = 1;
    while (step > 0)
    {
        if (xport_isquit())
        {
            step = -1;
            break;
        }
#ifdef _DEBUG
        editor_vis_update();
#endif
        sint32 hd = xport_hd != 0;
        if (hd != state.hd)
        {
            state.hd = hd;
            if (hd)
            {
                hd_seed();
                game_sample_frame();
                state.clock_idx = game_frame_index;
            }
        }
        if (!hd)
        {
            step = race_loop_before_frame(&state, 0);
            if (step > 0)
            {
                game_render_frame();
                race_loop_after_frame(&state);
            }
            continue;
        }
        // Poll native VBlank without coupling the render cadence to FIELD_RATE
        VSync(-1);
        game_sample_frame();
        while (step > 0 && state.clock_idx < game_frame_index && xport_hd)
        {
            ++state.clock_idx;
            step = race_loop_before_frame(&state, 1);
            if (step > 0)
                race_loop_after_frame(&state);
            VSync(-1);
            game_sample_frame();
        }
        if (step <= 0)
            break;
        if (!xport_hd)
            continue;
        hd_set_frame_fract(game_frame_fract);
        game_hd_frame = 1;
        game_render_frame();
        xport_render_wait();
    }
#ifdef _DEBUG
    editor_vis_end();
#endif
    return step < 0 ? 0 : race_finish_loop(&state);
}

static sint32 race_loop(const RACE_VIEW *view0, const RACE_VIEW *view1, const RACE_CFG *cfg)
{
    // Race clear area from MAIN.EXE 800B3DAC
    PSX_RECT clear_area = {0, 0, 512, 512};
    sint32 warmup = 2;

    FUNCTION_MARKER(0x800103C4, "MAIN.EXE");
    DrawSync(0);
    cb_wait_vblank();
    SetDispMask(0);
    ClearImage(&clear_area, 0, 0, 0);
    DrawSync(0);
    cb_wait_vblank();
    if (cfg->players == 1)
    {
        PutDispEnv(&disp_scene_frame(camera_for_view(view0), 0)->disp);
        PutDrawEnv(&disp_scene_frame(camera_for_view(view0), 0)->draw);
    }
    VSync(0);
    game_timing.ticks = 0;
    disp_state.scene_buffer = 0;
    if (race_selection.format == 1)
        sound_queue_command(sound_for_context(&race_views[0]), 11, 0);
    return race_loop_resume(view0, view1, cfg, warmup);
}

static void race_init_runtime(const RACE_VIEW *view0, const RACE_VIEW *view1, const RACE_CFG *cfg)
{
    uint32 call_argument;
    uint32 mode;
    sint32 font;
    sint32 half_period;
    uint8 parameter = 5;

    FUNCTION_MARKER(0x80010DDC, "MAIN.EXE");
    game_timing.frame_rate = cfg->players == 2 && !xport_hd ? game_timing.base_rate / 2 : game_timing.base_rate;
    race_paused = 0;
    half_period = (sint32)game_timing.base_rate / 2;
    menu_pause_reset();
    boat_half_period = half_period;
    render_init_packet_pools();
    render_init_billboards();
    if (race_cfg.variant != 2)
        render_capture_enabled = 1;
    if (race_cfg.mode == 8)
        river_init_showcase();
    else
    {
        track_select(cfg->course, cfg->variant);
        mode = race_cfg.mode;
        call_argument = cfg->course;
        ai_config_template_desc(call_argument, mode == 5 ? 4u : race_selection.ai_variant);
        object_node_flags_update_linked(route_resources.count);
        route_init_lanes(&route_resources, race_cfg.course);
    }
    boat_init_slots();
    boat_tracks(view0)[1] = NULL;
    boat_tracks(view0)[0] = NULL;
    call_argument = 0;
    race_init_player(view0, call_argument, 0);
    camera_init(camera_for_view(view0));
    if (race_cfg.players == 2)
    {
        boat_tracks(view1)[1] = NULL;
        boat_tracks(view1)[0] = NULL;
        call_argument = 0;
        race_init_player(view1, call_argument, 1);
        camera_init(camera_for_view(view1));
    }
    else
    {
        boat_players[1] = NULL;
        boat_tracks(view1)[1] = NULL;
        boat_tracks(view1)[0] = NULL;
    }
    ai_sort_racers_by_rank(view0, view1);
    pickup_write_racer_indices(race_selection.ranks);
    render_init_materials(cfg->course);
    render_init_prims(render_frame(&camera_views[0], 0));
    render_init_prims(render_frame(&camera_views[0], 1));
    if (cfg->players == 2)
    {
        render_init_prims(render_frame(&camera_views[1], 0));
        render_init_prims(render_frame(&camera_views[1], 1));
    }
    if (race_cfg.mode != 8)
        object_init_racer_object_grps(view0, view1, cfg->players);
    mode = cfg->players;
    hud_init_context(mode);
    disp_init_envs();
    render_init_ui_prims();
    FntLoad(960, 256);
    if (cfg->players == 1)
        font = FntOpen(30, 200, 250, 230, 0, 512);
    else
        font = FntOpen(30, 32, 330, 50, 2, 512);
    SetDumpFnt(font);
    render_build_curve_scaled_lut();
    mesh_init_tex_templates();
    if (race_cfg.mode != 8)
        ai_assign_ctrl_states(&race_views[0], &race_views[1]);
    cd_reset_audio();
    CdControlB(14, &parameter, 0);
    VSync(3);
    CdReadyCallback(cd_handle_ready_cb);
}

static void game_main_loop_continue(sint32 menu_already_returned)
{
    for (;;)
    {
        if (xport_isquit())
            return;
        if (!menu_already_returned)
        {
            arena_push();
            arena_push();
            if (game_selection.selection == -5)
                game_selection.selection = 0;
            else
                menu_update_select_mode();
        }
        menu_already_returned = 0;
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        arena_pop();
        race_config_results();
        mesh_render_state.enabled = 1;
        render_capture_enabled = 0;
        cd_load_course_assets(&race_cfg);
        sound_init_race_audio();
        race_init_runtime(&race_views[0], &race_views[1], &race_cfg);
        effects_register_mode();
        race_loop(&race_views[0], &race_views[1], &race_cfg);
        // Preserve the existing host quit boundary before scene cleanup
        if (xport_isquit())
            return;
        cb_clear_active_list();
        cb_clear_list(&cb_players[0]);
        cb_clear_list(&cb_players[1]);
        sound_config_reverb_depth(0, 0);
        SpuSetReverb(0);
        sound_reset_recs(sound_for_context(&race_views[0]));
        input_stop_vibration();
        if (race_cfg.players == 2)
            sound_reset_recs(sound_for_context(&race_views[1]));
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        arena_pop();
        menu_startup_tim_disp();
    }
}

void game_start(void)
{
    FUNCTION_MARKER(0x80010184, "MAIN.EXE");
    ResetCallback();
    ResetGraph(0);
    SetGraphDebug(0);
    SetVideoMode(MODE_PAL);
    InitGeom();
    input_init();
    arena_reset();
    cd_init_drive();
    cb_init_vblank_timer();
    cb_init_pool();
    game_init_defaults();
    save_load();
    xport_set_cleanup(hd_free);
    hd_init();
    sound_init_audio();
#if 0
    perf_init(0);
#endif
    game_main_loop_continue(0);
}

sint32 game_draw_og_frame(void)
{
    sint32 players = race_cfg.players;
    sint32 idx;

#if 0
    perf_frame_end();
    sint16 buffer = disp_state.scene_buffer;
    uint32 *overlay_entry = render_scene_ot(render_frame(&camera_views[0], buffer));

    perf_render_graph(overlay_entry, buffer);
    perf_render_cur(overlay_entry, buffer);
#endif
    ResetGraph(1);
    if (players == 1 && game_timing.base_rate == game_timing.frame_rate)
    {
        const RACE_VIEW *state = &race_views[0];
        uint32 buffer_idx;

        buffer_idx = disp_state.scene_buffer;
        DrawOTag(render_scene_ot(render_frame(camera_for_view(state), buffer_idx)) + 14);
        if (render_capture_enabled != 0)
        {
            buffer_idx = disp_state.scene_buffer;
            effects_capture_scr_region(state, buffer_idx);
            buffer_idx = disp_state.scene_buffer;
            DrawOTag(render_scene_ot(render_frame(camera_for_view(state), buffer_idx)) + 1);
        }
    }
    else
    {
        uint32 buffer_idx;

        buffer_idx = disp_state.scene_buffer;
        PutDrawEnv(&disp_scene_frame(&camera_views[0], buffer_idx)->draw);
        buffer_idx = disp_state.scene_buffer;
        PutDispEnv(&disp_scene_frame(&camera_views[0], buffer_idx)->disp);
        for (idx = 0; idx < (sint32)race_cfg.players; ++idx)
        {
            const RACE_VIEW *state = idx == 0 ? &race_views[0] : &race_views[1];

            buffer_idx = disp_state.scene_buffer;
            DrawOTag(render_scene_ot(render_frame(camera_for_view(state), buffer_idx)) + 14);
            if (render_capture_enabled != 0)
            {
                buffer_idx = disp_state.scene_buffer;
                effects_capture_scr_region(state, buffer_idx);
                buffer_idx = disp_state.scene_buffer;
                DrawOTag(render_scene_ot(render_frame(camera_for_view(state), buffer_idx)) + 1);
            }
        }
    }
    return 0;
}

sint32 game_frame_og(void)
{
    FUNCTION_MARKER(0x800107A4, "MAIN.EXE");
    gpu_frame_begin();
    if (race_cfg.players == 1 && game_timing.base_rate == game_timing.frame_rate)
        cb_sync_video_field();
    else
        VSync(2);
    game_draw_og_frame();
    gpu_frame_end();
    // Host display adapter after original 0x80010960 frame submission
    gpu_present();
    return 0;
}

static sint32 game_frame_hd(void)
{
    if (!hd_begin_frame())
        return 0;
    for (uint32 idx = 0; idx < race_cfg.players; ++idx)
        hd_render_player(&race_views[idx]);
#ifdef _DEBUG
    editor_vis_draw();
#endif
    return hd_end_frame();
}

sint32 game_render_frame(void)
{
    return game_hd_frame ? game_frame_hd() : game_frame_og();
}

uint32 xport_libsnd_sequence_call(uint32 target, uint32 argc, const uint32 *args)
{
#ifndef NDEBUG
    fprintf(stderr, "Rapid Racer has no LibSnd sequence callback binding: target=%08X argc=%u\n", target, argc);
#endif
    abort();
}

void xport_main(void)
{
    game_start();
}
