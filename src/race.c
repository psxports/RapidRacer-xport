#include "effects.h"
#include "menu_select.h"
#include "hud.h"
#include "river.h"
#include <string.h>
#include "route.h"
#include "game.h"
#include "camera.h"
#include "boat.h"
#include "arena.h"
#include "mesh.h"
#include "name.h"
#include "pickup.h"
#include "render.h"
#include "track.h"
#include "replay.h"
#include "sound.h"
#include "text.h"
#include "profile.h"
#include "race.h"
#include "race_stats.h"
#include "hd.h"
#include "banner.h"
#include "tournament.h"
#include "utils.h"
#include "menu.h"
#include "timer.h"
#include "xport_trace.h"
#include "cd.h"
#include <stdlib.h>

// Checkpoint inputs from MAIN.EXE 800F5B68/800FF708/800DCC40
typedef struct
{
    uint16 parameters[30];
    uint16 laps[30];
    uint16 times[150][3];
} RESULTS_CHECKPOINTS;

static RESULTS_CHECKPOINTS race_checkpoints;

// Native course catalog from MAIN.EXE 80091B04/80091C6C/80091DD4/80091F3C/80091F8C
typedef struct
{
    uint16 course;
    uint16 counts[3];
    uint8 ai_variant;
    uint16 parameter;
    uint16 variant;
    uint16 laps;
    TOURNAMENT_TIME *times;
} RESULTS_COURSE;

// Shared banks preserve aliases between course variants
static TOURNAMENT_TIME race_times[33][5] = {
    {{{0, 30, 0}, 0}, {{0, 0, 0}, 2000}, {{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}}, {{{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}, {{0, 26, 0}, 0}, {{0, 30, 0}, 0}},   {{{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}, {{0, 26, 0}, 0}, {{0, 30, 0}, 0}},   {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},
    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}},
    {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},
    {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},
};

static RESULTS_COURSE race_courses[61] = {
    {1, {3, 5, 3}, 0, 3, 1, 3, race_times[8]},  {0, {3, 5, 3}, 0, 3, 1, 4, race_times[18]}, {2, {3, 5, 3}, 0, 3, 1, 4, race_times[13]}, {3, {3, 5, 3}, 0, 3, 1, 4, race_times[3]},  {4, {3, 5, 3}, 0, 3, 1, 4, race_times[23]}, {5, {3, 5, 3}, 0, 3, 1, 4, race_times[28]}, {1, {3, 5, 3}, 1, 3, 2, 3, race_times[9]},  {0, {3, 5, 3}, 1, 3, 2, 4, race_times[19]}, {2, {3, 5, 3}, 1, 3, 2, 4, race_times[14]}, {3, {3, 5, 3}, 1, 3, 2, 4, race_times[4]},  {4, {3, 5, 3}, 1, 3, 2, 4, race_times[24]}, {5, {3, 5, 3}, 1, 3, 2, 4, race_times[29]}, {1, {3, 5, 3}, 2, 3, 0, 3, race_times[10]}, {0, {3, 5, 3}, 2, 3, 0, 4, race_times[20]}, {2, {3, 5, 3}, 2, 3, 0, 4, race_times[15]}, {3, {3, 5, 3}, 2, 3, 0, 4, race_times[5]},  {4, {3, 5, 3}, 2, 3, 0, 4, race_times[25]}, {5, {3, 5, 3}, 2, 3, 0, 4, race_times[30]}, {1, {3, 5, 3}, 1, 3, 1, 3, race_times[9]},  {0, {3, 5, 3}, 1, 3, 1, 4, race_times[19]}, {2, {3, 5, 3}, 1, 3, 1, 4, race_times[14]},
    {3, {3, 5, 3}, 1, 3, 1, 4, race_times[4]},  {4, {3, 5, 3}, 1, 3, 1, 4, race_times[24]}, {5, {3, 5, 3}, 1, 3, 1, 4, race_times[29]}, {1, {3, 5, 3}, 2, 3, 2, 3, race_times[10]}, {0, {3, 5, 3}, 2, 3, 2, 4, race_times[20]}, {2, {3, 5, 3}, 2, 3, 2, 4, race_times[15]}, {3, {3, 5, 3}, 2, 3, 2, 4, race_times[5]},  {4, {3, 5, 3}, 2, 3, 2, 4, race_times[25]}, {5, {3, 5, 3}, 2, 3, 2, 4, race_times[30]}, {1, {3, 5, 3}, 3, 3, 0, 3, race_times[11]}, {0, {3, 5, 3}, 3, 3, 0, 4, race_times[21]}, {2, {3, 5, 3}, 3, 3, 0, 4, race_times[16]}, {3, {3, 5, 3}, 3, 3, 0, 4, race_times[6]},  {4, {3, 5, 3}, 3, 3, 0, 4, race_times[26]}, {5, {3, 5, 3}, 3, 3, 0, 4, race_times[31]}, {1, {3, 5, 3}, 2, 3, 1, 3, race_times[10]}, {0, {3, 5, 3}, 2, 3, 1, 4, race_times[20]}, {2, {3, 5, 3}, 2, 3, 1, 4, race_times[15]}, {3, {3, 5, 3}, 2, 3, 1, 4, race_times[5]},  {4, {3, 5, 3}, 2, 3, 1, 4, race_times[25]}, {5, {3, 5, 3}, 2, 3, 1, 4, race_times[30]},
    {1, {3, 5, 3}, 3, 3, 2, 3, race_times[11]}, {0, {3, 5, 3}, 3, 3, 2, 4, race_times[21]}, {2, {3, 5, 3}, 3, 3, 2, 4, race_times[16]}, {3, {3, 5, 3}, 3, 3, 2, 4, race_times[6]},  {4, {3, 5, 3}, 3, 3, 2, 4, race_times[26]}, {5, {3, 5, 3}, 3, 3, 2, 4, race_times[31]}, {1, {3, 5, 3}, 4, 3, 0, 3, race_times[12]}, {0, {3, 5, 3}, 4, 3, 0, 4, race_times[22]}, {2, {3, 5, 3}, 4, 3, 0, 4, race_times[17]}, {3, {3, 5, 3}, 4, 3, 0, 4, race_times[7]},  {4, {3, 5, 3}, 4, 3, 0, 4, race_times[27]}, {5, {3, 5, 3}, 4, 3, 0, 4, race_times[32]}, {6, {3, 5, 3}, 2, 3, 1, 3, race_times[1]},  {6, {3, 5, 3}, 3, 3, 1, 3, race_times[2]},  {6, {3, 5, 3}, 2, 3, 2, 3, race_times[1]},  {6, {3, 5, 3}, 3, 3, 2, 3, race_times[2]},  {4, {3, 5, 3}, 2, 3, 1, 4, race_times[25]}, {2, {3, 5, 3}, 3, 3, 1, 3, race_times[11]}, {1, {3, 5, 3}, 2, 3, 2, 4, race_times[15]},
};

// Result screen point banks from MAIN.EXE 80092010/80092028
static const sint16 race_points[2][5][2] = {
    {{40, 40}, {30, 34}, {16, 24}, {6, 10}, {0, 0}},
    {{38, 38}, {24, 29}, {9, 14}},
};

static sint32 race_build_checkpoints(void)
{
    sint32 grp;

    FUNCTION_MARKER(0x80048B30, "MAIN.EXE");
    for (grp = 0; grp < 3u; ++grp)
    {
        sint32 outer;
        uint32 desc_idx = 0;
        for (outer = grp; outer < grp + 3; ++outer)
        {
            sint32 row;
            for (row = 0; row < 6; ++row, ++desc_idx)
            {
                RESULTS_COURSE *desc = &race_courses[grp * 18u + desc_idx];
                TOURNAMENT_TIME *output = desc->times;
                sint32 column;
                desc->laps = race_checkpoints.laps[outer + 5 * row] + 1u;
                for (column = 0; column < 5; ++column)
                {
                    const uint16 *src = race_checkpoints.times[25 * row + 5 * outer + column];
                    output->parts[0] = src[0];
                    output->parts[1] = src[1];
                    output->parts[2] = src[2];
                    output->parameter = race_checkpoints.parameters[5 * row + column];
                    ++output;
                }
            }
        }
    }
    return 0;
}

// ASCII checkpoint file separators
static sint32 race_checkpoint_space(uint8 chr)
{
    if (chr > 127u)
        abort();
    return chr == ' ' || chr >= 9u && chr <= 13u;
}

sint32 race_load_checkpoints(void)
{
    const uint8 *cursor;
    const uint8 *end;
    uint32 size;
    sint16 section = 0;
    sint16 idx = 0;
    sint32 stop = 0;

    FUNCTION_MARKER(0x800487F0, "MAIN.EXE");
    arena_push();
    strcpy(cd_dir, "\\MENU");
    cursor = cd_load_file_bytes("\\CHKPOINT.DAT", &size);
    if (!cursor || size == 0)
        abort();
    end = cursor + size;
    do
    {
        const uint8 *token;
        sint16 len = 0;
        sint32 val;
        uint8 chr;

        while (cursor < end && race_checkpoint_space(*cursor))
            ++cursor;
        if (cursor == end)
            abort();
        if (*cursor == ';')
        {
            ++cursor;
            while (cursor < end && *cursor++ != 10)
            {
            }
            if (cursor == end)
                abort();
        }
        token = cursor;
        while (cursor < end && !race_checkpoint_space(*cursor))
        {
            if (len == INT16_MAX)
                abort();
            ++cursor;
            ++len;
        }
        if (cursor == end)
            abort();
        chr = len == 0 ? 0u : *token;
        if (chr == '!')
        {
            ++section;
            idx = 0;
        }
        else if (chr == '*')
            stop = 1;
        else if (chr != ';')
        {
            ++cursor;
            val = text_parse_digits(token, len);
            if (section == 0)
            {
                if (idx < 0 || idx >= 30)
                    abort();
                race_checkpoints.parameters[idx] = val;
            }
            else if (section == 1)
            {
                if (idx < 0 || idx >= 30)
                    abort();
                race_checkpoints.laps[idx] = val;
            }
            else if (section == 2)
            {
                if (idx < 0 || idx >= 450)
                    abort();
                race_checkpoints.times[idx / 3][idx % 3] = val;
            }
            ++idx;
        }
    } while (stop == 0 && section < 9);
    arena_pop();
    return race_build_checkpoints();
}

// Result bytes from MAIN.EXE 800E05B3/05B4/05B8
RESULT result_state;

// Grid columns from MAIN.EXE 80091480
const uint32 race_grid_columns[3] = {2, 1, 0};

// Lap age palettes from reviewed MAIN.EXE

sint32 race_init_markers(void)
{
    static RIVER_REC recs[3];
    sint32 angle = 768;
    sint32 idx;
    const PROFILE_GRID *table = &profile_at(profile_selection.slot)->grid[race_cfg.boats[0]];

    FUNCTION_MARKER(0x8003B9C4, "MAIN.EXE");
    river_markers.recs = recs;
    river_markers.count = 3;
    river_markers.idx = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        RIVER_REC *rec = &recs[idx];
        sint32 s = rsin(angle);
        sint32 c = rcos(angle);
        sint32 type = idx;
        rec->pos[0] = route_showcase_seg.origin[0] + 1000 * c / 4096 + 2000 * c / 4096;
        rec->pos[1] = track_horizon->ground;
        rec->pos[2] = route_showcase_seg.origin[2] + 1000 * s / 4096 + 2000 * s / 4096;
        if (table->offset[race_grid_columns[idx]] == 2)
            type = 3;
        rec->angle = type << 12 | angle & 0x0FFF;
        rec->kind = 0;
        angle += 256;
    }
    return 0;
}

static const RESULTS_COURSE *race_select_course(const RESULTS_COURSE *fallback)
{
    const PLAYER_PROFILE *profile_data;
    uint32 idx;
    uint32 profile;

    FUNCTION_MARKER(0x8003BC14, "MAIN.EXE");
    if (race_selection.special != 0)
    {
        sound_options.track_slot = 6;
        return &race_courses[54];
    }
    if (game_selection.mode == 4)
    {
        if (game_selection.attract >= 3u)
            abort();
        return &race_courses[58u + game_selection.attract];
    }
    profile_data = profile_cur();
    profile = profile_selection.slot;
    idx = profile_data->course + 6 * profile_data->level;
    if (profile < 3)
    {
        idx += profile * 18;
        if (idx >= 61)
            abort();
        fallback = &race_courses[idx];
    }
    sound_options.track_slot = profile_data->course;
    if (game_selection.mode == 3)
        sound_options.track_slot = 7;
    return fallback;
}

sint32 race_config_results(void)
{
    const RESULTS_COURSE *desc;
    TRACK_RACE_CFG *cfg;
    const TOURNAMENT_TIME *src;
    const TOURNAMENT_TIME *times = NULL;
    sint32 mode;
    sint32 selector;
    sint32 publish_selector = 1;
    sint32 vertical = 220;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x8003BEB4, "MAIN.EXE");

    time_init(&race_freeze_time);
    result_state.bonus_timer = 0;
    desc = race_select_course(NULL);
    for (idx = 0; idx < 4; ++idx)
        pickup_hud[idx].limit = 0;

    race_cfg.course = (sint16)desc->course;
    race_cfg.variant = (sint16)desc->variant;
    track_select(race_cfg.course, race_cfg.variant);
    uint8 desc_mode = desc->ai_variant;

    cfg = track_race;
    race_selection.ai_variant = desc_mode;
    cfg->laps = desc->parameter;
    cfg->checkpoint_count = desc->laps + 1u;
    mode = (sint16)game_selection.mode;
    src = desc->times;

    switch (mode)
    {
        case 0:
        case 1:
            selector = game_selection.menu_variant != 0 ? 1 : 2;
            break;
        case 2:
        case 3:
            selector = 1;
            break;
        case 4:
            selector = 4;
            break;
        case 6:
        {
            sint16 submode = game_selection.rules;

            if (submode == 1)
            {
                cfg->laps = 1;
                for (idx = 0; idx < 5; ++idx)
                    tournament_times[idx].parameter = src[idx].parameter;
                times = tournament_times;
                selector = 2;
            }
            else if (submode == 2)
                selector = 1;
            else if (submode == 0)
                selector = 2;
            else
                publish_selector = 0;
            break;
        }
        case 7:
            src = race_times[0];
            selector = 1;
            break;
        default:
            publish_selector = 0;
            break;
    }
    if (publish_selector)
        race_selection.format = selector;
    if (race_selection.special != 0)
        race_selection.format = 1;

    for (idx = 0; idx < 5; ++idx)
    {
        const TOURNAMENT_TIME *input = &src[idx];
        TRACK_CHECKPOINT *dst = &cfg->checkpoints[idx];
        sint32 minutes = times != NULL ? (sint16)times[idx].parts[0] : (sint16)input->parts[0];
        sint32 fraction = times != NULL ? (sint16)times[idx].parts[2] : (sint16)input->parts[2];
        sint32 seconds = times != NULL ? (sint16)times[idx].parts[1] : (sint16)input->parts[1];
        sint32 val = 6000u * minutes + 100u * seconds + fraction / 10;

        time_format(&dst->time, val);
        dst->seg = times != NULL ? times[idx].parameter : input->parameter;
    }
    result_state.lap_active = race_selection.format != 1;
    mode = race_cfg.mode;
    result_state.finished = 0;
    if (mode == 8)
    {
        uint8 profile = profile_selection.slot;
        sint32 val = profile == 1 ? 3000 : profile == 2 ? 2500 : 3500;

        time_format(&race_time, val);
    }
    else
    {
        time_copy(&race_time, &cfg->checkpoints[0].time);
        time_add(&race_time, &cfg->checkpoints[1].time);
    }
    mode = race_cfg.players;
    result_state.checkpoint_max = 0;

    if (mode == 1)
        vertical = 420;
    pickup_hud[2].limit = desc->counts[1];
    pickup_hud[2].center_x = 76;
    {
        uint16 count = desc->counts[0];

        pickup_hud[1].center_x = 192;
        pickup_hud[1].limit = count;
    }
    pickup_hud[3].limit = desc->counts[2];
    pickup_hud[3].center_x = 308;
    {
        sint16 count = pickup_hud[2].limit;
        const sint16(*points)[2];
        if (count < 0 || count >= 6)
            abort();
        points = race_points[0];

        for (idx = 0; idx < pickup_hud[2].limit; ++idx)
        {
            pickup_hud[2].x[idx] = points[0][0] + 20;
            pickup_hud[2].y[idx] = vertical + points[0][1] - 38;
            ++points;
        }
    }
    {
        sint16 count = pickup_hud[1].limit;
        sint32 span = 18 * count + 10;
        sint32 half = (span - (span < 0)) / 2;
        uint32 left = pickup_hud[1].center_x - half;

        for (idx = 0; idx < count; ++idx)
        {
            pickup_hud[1].y[idx] = vertical + 200;
            pickup_hud[1].x[idx] = left + 18u * idx;
        }
    }
    result = 4 * (sint16)pickup_hud[3].limit;
    {
        sint16 count = pickup_hud[3].limit;
        const sint16(*points)[2];
        if (count < 0 || count >= 6)
            abort();
        points = race_points[1];

        for (idx = 0; idx < pickup_hud[3].limit; ++idx)
        {
            pickup_hud[3].x[idx] = 384 - (points[0][0] + 48);
            pickup_hud[3].y[idx] = vertical + points[0][1] - 38;
            ++points;
            result = (sint16)pickup_hud[3].limit;
        }
    }
    result_state.lap_offset = 0;
    return result;
}

static sint32 race_select_best_time_rec(BOAT_MENU *menu)
{
    uint32 best = UINT32_MAX;
    sint32 count = menu->lap_idx;
    sint32 idx;

    FUNCTION_MARKER(0x8003CA84, "MAIN.EXE");
    for (idx = 0; idx < count; ++idx)
    {
        uint32 val = boat_lap_time(menu, idx)->ticks;
        if (val < best)
        {
            best = val;
            menu->best_lap_idx = idx;
        }
    }
    return idx < count;
}

static sint32 race_finish_race(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 choice = boat_player_menu(state)->racer_num;
    sint32 distance;

    FUNCTION_MARKER(0x8003CB14, "MAIN.EXE");
    menu->finish_state = 7;
    if (!(game_selection.mode == 6 && game_selection.rules == 1))
    {
        if (race_cfg.players == 1)
        {
            if (choice == 1)
                sound_queue_command(sound_for_context(state), 2, 0);
            else if (choice == 2)
                sound_queue_command(sound_for_context(state), 3, 0);
            else
                sound_queue_command(sound_for_context(state), choice == 3 ? 13 : 4, 0);
        }
        else if (choice == 1)
            sound_queue_command(sound_for_context(state), 2, 0);
        else if (choice == 2)
        {
            BOAT_MENU *peer = state == &race_views[0] ? boat_player_menu(&race_views[1]) : boat_player_menu(&race_views[0]);
            distance = boat_player_menu(state)->total_time.ticks - peer->total_time.ticks;
            if (distance >= 1000)
                sound_queue_command(sound_for_context(state), 13, 0);
            else if (distance >= 500)
                sound_queue_command(sound_for_context(state), 7, 7);
            else if (distance >= 301)
                sound_queue_command(sound_for_context(state), 3, 0);
        }
        sound_queue_command(sound_for_context(state), 0, 5);
    }
    result_state.finished = 1;
    if (race_cfg.players == 1 && game_selection.mode < 2)
        menu->mode = 8;
    else
    {
        menu->mode = 5;
        camera_for_view(state)->mode = 5;
    }
    result_state.bonus_timer = 0;
    race_select_best_time_rec(menu);
    --menu->lap_idx;
    time_format(&race_end_time, 500);
    result_state.pickups = menu->pickups[2].count;
    if (menu->mode == 8)
        race_stats_begin();
    return pickup_write_racer_indices(race_selection.ranks);
}

static sint32 race_advance_page(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 cur;
    uint32 prev;
    uint32 val;
    sint32 idx;
    sint32 maximum;

    FUNCTION_MARKER(0x8003CCEC, "MAIN.EXE");
    cur = menu->lap_idx;
    if (state == &race_views[0] && cur < 3)
        race_stats.lap_pos[cur] = menu->rank + boat_leader_count + 1;
    menu->lap_idx = cur + 1u;
    if (game_selection.mode == 2)
    {
        cur = menu->lap_idx;
        if (cur == 4)
        {
            for (idx = 0; idx < 3; ++idx)
                time_copy(boat_lap_time(menu, idx), boat_lap_time(menu, idx + 1u));
            time_init(boat_lap_time(menu, 3));
            menu->lap_idx--;
            result_state.lap_offset++;
        }
        cur = menu->lap_idx;
        TIME_REC *entry = boat_lap_time(menu, cur - 1u);

        val = entry->ticks;
        cur = menu->lap_idx;
        if (cur == 1)
        {
            replay_finish(&boat_players[0]->contacts.points[0], 1);
            time_copy_chars7((char *)entry->text, hud_best);
            prev = result_state.rec_ticks;
            result_state.best_lap_ticks = val;
        }
        else
        {
            prev = result_state.best_lap_ticks;
            if (val > prev)
            {
                replay_finish(&boat_players[0]->contacts.points[0], 0);
                menu->checkpoint_idx = 1;
                return 0;
            }
            replay_finish(&boat_players[0]->contacts.points[0], 1);
            result_state.best_lap_ticks = val;
            time_copy_chars7((char *)entry->text, hud_best);
            prev = result_state.rec_ticks;
        }
        if (val < prev)
        {
            sound_queue_command(sound_for_context(state), 1, 0);
            result_state.rec_ticks = val;
            time_copy_chars7((char *)entry->text, hud_rec);
            time_copy(&menu->best_time, entry);
        }
        menu->checkpoint_idx = 1;
        return 0;
    }

    menu->checkpoint_idx = 1;
    cur = menu->lap_idx;
    maximum = track_race->laps;
    if (cur == maximum)
    {
        race_finish_race(state, menu, unused, argument);
        return 1;
    }
    if (cur == maximum - 1)
        sound_queue_command(sound_for_context(state), 0, 0);
    else
    {
        if ((sint32)race_time.ticks >= 200)
            return 0;
        sound_queue_command(sound_for_context(state), 9, 0);
    }
    return 0;
}

static sint32 race_finish_challenge(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8003CF10, "MAIN.EXE");
    if (menu->pickups[2].count >= 5u)
        result_state.status = 2;
    if (result_state.status == 0)
        return 5;
    if (menu->mode == 5 || menu->mode == 7)
        return 7;
    profile_data = profile_at(profile_selection.slot);
    if (result_state.status == 2)
    {
        sound_queue_command(sound_for_context(state), 2, 0);
        profile_data->progress.courses[profile_data->level][profile_data->course].result = 2;
    }
    else if (result_state.status == 1)
        sound_queue_command(sound_for_context(state), 4, 0);
    sound_queue_command(sound_for_context(state), 0, 5);
    menu->mode = 5;
    time_format(&race_end_time, 300);
    return 1;
}

static sint32 race_process_lap_completion(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 phase;
    sint32 completed = 0;

    FUNCTION_MARKER(0x8003D04C, "MAIN.EXE");
    phase = menu->mode;
    if (phase == 4 || phase == 6)
    {
        sint32 enabled = 1;

        if (game_selection.mode == 6 && game_selection.rules == 1 && menu->mode != 4)
            enabled = 0;
        if (enabled != 0 && menu->checkpoint_idx == 0)
        {
            result_state.lap_active = 1;
            menu->checkpoint_idx = 1;
            if (race_cfg.mode == 4)
                replay_begin(&boat_players[0]->contacts.points[0]);
        }
        else if (enabled != 0)
        {
            sint32 count;

            ++menu->checkpoint_idx;
            {
                uint32 checkpoint = menu->checkpoint_idx;
                sint32 checkpoint_count = track_race->checkpoint_count;

                if (checkpoint == checkpoint_count)
                {
                    completed = race_advance_page(state, menu, unused, argument);
                    if (race_cfg.mode == 4)
                        sound_queue_command(sound_for_context(state), 8, 1);
                }
            }
            {
                uint32 lap = menu->lap_idx;
                sint32 src = menu->total_time.ticks;
                uint32 checkpoint = menu->checkpoint_idx;

                time_format(boat_checkpoint_time(menu, lap, checkpoint), src);
            }
            count = menu->checkpoint_total + 1;
            menu->checkpoint_total = count;
            if ((sint32)result_state.checkpoint_max < count)
            {
                result_state.checkpoint_max = count;
                if ((uint32)completed << 16 == 0 && (game_selection.mode != 6 || game_selection.rules != 1) && race_cfg.mode != 4)
                {
                    phase = menu->mode;
                    if (phase == 6 && result_state.lap_active != 0)
                    {
                        TIME_REC scratch;
                        BOAT_MENU *peer;

                        sound_queue_command(sound_for_context(state), 9, 0);
                        time_format(&scratch, 300);
                        time_sub(&scratch, &race_end_time);
                        time_sub(&race_time, &scratch);
                        peer = boat_player_menu(&race_views[0]);
                        if (peer->mode == 6)
                        {
                            uint32 player_idx;

                            time_add(&boat_player_menu(&race_views[0])->total_time, &scratch);
                            peer = boat_player_menu(&race_views[0]);
                            player_idx = peer->lap_idx;
                            peer = boat_player_menu(&race_views[0]);
                            time_add(boat_lap_time(peer, player_idx), &scratch);
                            boat_player_menu(&race_views[0])->mode = 4;
                        }
                        if (race_cfg.players == 2)
                        {
                            peer = boat_player_menu(&race_views[1]);
                            if (peer->mode == 6)
                            {
                                uint32 player_idx;

                                time_add(&boat_player_menu(&race_views[1])->total_time, &scratch);
                                peer = boat_player_menu(&race_views[1]);
                                player_idx = peer->lap_idx;
                                peer = boat_player_menu(&race_views[1]);
                                time_add(boat_lap_time(peer, player_idx), &scratch);
                                boat_player_menu(&race_views[1])->mode = 4;
                            }
                        }
                        time_format(&race_end_time, 300);
                    }
                    {
                        sint32 lap_count = track_race->laps;
                        uint32 lap = menu->lap_idx;

                        if (lap == lap_count - 1)
                        {
                            sint32 checkpoint_count = track_race->checkpoint_count;
                            uint32 checkpoint = menu->checkpoint_idx;

                            if (checkpoint == checkpoint_count - 1)
                                banner_show_finish();
                        }
                    }
                    {
                        uint32 mode = race_cfg.mode;

                        if (mode != 9 && mode != 5)
                            sound_queue_command(sound_for_context(state), 8, 1);
                    }
                    {
                        uint32 checkpoint = menu->checkpoint_idx;
                        const TIME_REC *rec;

                        result_state.bonus_timer = 120;
                        rec = &track_checkpoint(checkpoint)->time;
                        time_add(&race_time, rec);
                        time_copy(&race_bonus_time, rec);
                    }
                }
            }
            else
            {
                BOAT_MENU *peer;

                menu->delta_timer = 240;
                peer = state == &race_views[0] ? boat_player_menu(&race_views[1]) : boat_player_menu(&race_views[0]);
                peer->delta_timer = 240;
                sound_queue_command(sound_for_context(state), 8, 1);
            }
        }
    }
    return (sint16)menu->mode;
}

static void race_complete_2p(void)
{
    sint16 state = game_selection.mode;
    sint16 selection = game_selection.selection;

    FUNCTION_MARKER(0x8005D8E8, "MAIN.EXE");
    game_selection.event = 10;
    game_selection.event_arg = 0;
    if (state == 0)
    {
        if (selection == -3)
        {
            game_selection.event = 48;
            return;
        }
        if (selection != -4 && selection != -2)
            return;
        game_selection.event = 51;
        result_state.winner = (sint32)boat_player_menu(&race_views[0])->total_time.ticks >= (sint32)boat_player_menu(&race_views[1])->total_time.ticks;
        return;
    }
    if (state == 1)
    {
        result_state.count = boat_racer_count + boat_leader_count + boat_trailer_count;
        menu_select_complete();
        return;
    }
}

static sint32 race_update_progress(const RACE_VIEW *state, struct BOAT_MENU *menu)
{
    sint32 mode;
    sint32 result = 0;
    sint32 timer;

    FUNCTION_MARKER(0x8003C640, "MAIN.EXE");
    if (state == &race_views[0])
    {
        timer = result_state.bonus_timer;
        if (timer > 0)
            result_state.bonus_timer = timer - 1u;
    }
    timer = menu->delta_timer;
    if (timer != 0)
    {
        menu->delta_timer = timer - (game_timing.frame_rate == game_timing.base_rate ? 1u : 2u);
    }
    if (menu_state.race_trans_paused != 0)
        return 0;
    mode = menu->mode;
    if (mode == 1)
    {
        if (time_dec(&menu->start_time) != 0)
        {
            mode = 4;
            sound_queue_command(sound_for_context(state), 10, 0);
            result_state.best_lap_ticks = UINT32_MAX;
        }
        if (menu->start_ready == 0 && (sint32)menu->start_time.ticks < 301)
        {
            effects_init_transform_object(state);
            sound_queue_command(sound_for_context(state), 11, 0);
            menu->start_ready = 1;
        }
    }
    else if (mode == 4)
    {
        sint32 pos = (uint8)(menu->rank + (uint8)boat_leader_count);
        uint8 prev_pos;

        if (state == &race_views[0])
            race_selection.ranks[0] = pos;
        else
            race_selection.ranks[1] = pos;
        if (result_state.lap_active != 0)
        {
            time_inc(boat_lap_time(menu, menu->lap_idx));
            time_inc(&menu->total_time);
        }
        if (race_cfg.mode == 4)
            return 0;
        prev_pos = menu->prev_rank;
        if (prev_pos != (uint8)pos)
        {
            sound_queue_command(sound_for_context(state), prev_pos < (uint8)pos ? -30 : 30, 2);
            menu->prev_rank = pos;
        }
        if (state == &race_views[0] || race_cfg.players == 2 && boat_player_menu(&race_views[0])->mode != 4)
        {
            if (race_freeze_time.ticks != 0)
                time_dec(&race_freeze_time);
            else if (time_dec(&race_time) != 0)
            {
                time_format(&race_end_time, 300);
                mode = 6;
            }
        }
        else if (race_time.ticks == 0)
            mode = 6;
    }
    else if (mode == 5)
        camera_for_view(state)->mode = 5;
    else if (mode == 6)
    {
        sint32 waiting = 1;
        if (boat_player_menu(&race_views[0])->mode == 6 && (race_cfg.players != 2 || boat_player_menu(&race_views[1])->mode == 6))
            waiting = 0;
        if ((state == &race_views[0] || waiting != 0) && time_dec(&race_end_time) != 0)
        {
            uint32 game_mode = race_cfg.mode;
            sint32 play_sound = 0;

            if (game_mode == 8)
            {
                uint32 profile = profile_selection.slot;
                const PLAYER_PROFILE *profile_data = profile_at(profile);
                uint32 row = profile_data->level;
                uint32 column = profile_data->course;

                play_sound = profile_data->progress.courses[row][column].attempts != 0;
            }
            else if (game_mode != 9)
                play_sound = game_selection.mode != 6;
            if (play_sound != 0)
                sound_queue_command(sound_for_context(state), 4, 0);
            camera_for_view(state)->mode = 5;
            mode = 5;
            if (race_cfg.players == 2 && waiting == 0)
            {
                BOAT_MENU *peer = boat_player_menu(&race_views[1]);

                camera_views[1].mode = 5;
                peer->mode = 5;
            }
        }
    }
    else if (mode == 7)
        result = 1;
    menu->mode = mode;
    return result;
}

static sint32 race_update_player(const RACE_VIEW *state, sint32 argument)
{
    uint32 gate;
    BOAT_MENU *menu;
    sint32 result;

    FUNCTION_MARKER(0x80020B60, "MAIN.EXE");
    gate = race_paused;
    menu = boat_player_menu(state);
    result = (sint16)argument;
    if (gate == 0)
    {
        BOAT *boat;
        const ROUTE_SEGMENT *seg;
        sint32 pos;
        sint32 limit;
        uint32 base_rate;
        uint32 frame_rate;
        uint32 menu_type;

        base_rate = game_timing.base_rate;
        frame_rate = game_timing.frame_rate;
        if (base_rate != frame_rate)
            race_update_progress(state, menu);
        result = race_update_progress(state, menu);
        boat = boat_player(state);
        seg = boat->contacts.points[0].seg;
        pos = seg->next.idx - 1;
        menu_type = menu->checkpoint_idx;
        limit = track_checkpoint(menu_type)->seg;
        if (pos < 0)
            pos = seg->prev.idx + 1;
        if (pos >= limit && pos - boat->race.progress_step < limit)
        {
            race_process_lap_completion(state, menu, 0, 0);
            pickup_restore();
        }
        menu_type = race_cfg.mode;
        if (menu_type == 8)
            race_finish_challenge(state, menu, 0, 0);
    }
    return (sint16)result;
}

static sint32 race_render_progress(const RACE_VIEW *state, uint32 *ot)
{
    sint16 result;

    FUNCTION_MARKER(0x80020F7C, "MAIN.EXE");
    result = race_update_player(state, 0);
    if (race_stats_hide_hud())
    {
        hud_stats_render(ot);
        return 0;
    }
    return (sint16)hud_render(state, ot, result);
}

static sint32 race_finish_results(void)
{
    sint32 result;
    sint32 complete = 0;
    uint32 mode;

    mode = race_cfg.players;
    result = 2;
    if (mode == 1)
    {
        BOAT_MENU *peer = boat_player_menu(&race_views[0]);

        if (peer->mode == 7)
            complete = 1;
        else
            mode = race_cfg.players;
    }
    if (complete == 0 && mode == 2)
    {
        BOAT_MENU *peer = boat_player_menu(&race_views[0]);

        result = 7;
        if (peer->mode == 7)
        {
            result = boat_player_menu(&race_views[1])->mode ^ 7;
            complete = result == 0;
        }
    }
    if (complete != 0)
    {
        if (result_state.finished != 0)
        {
            result = -2;
            if (race_selection.ranks[0] == 0)
                result = -4;
            game_selection.selection = result;
        }
        else
        {
            game_selection.selection = -3;
            return pickup_write_racer_indices(race_selection.ranks);
        }
    }
    return result;
}

sint32 race_render_results(const RACE_VIEW *state, uint32 *ot)
{
    FUNCTION_MARKER(0x80020FD0, "MAIN.EXE");
    race_render_progress(state, ot);
    return race_finish_results();
}

void hd_race_update(const RACE_VIEW *view)
{
    race_update_player(view, 0);
    race_finish_results();
}

void hd_race_results(const RACE_VIEW *view)
{
    hd_hud_render(view);
}

void race_handle_2p(void)
{
    FUNCTION_MARKER(0x80063BA4, "MAIN.EXE");
    race_complete_2p();
    return;
}

sint32 race_compact_ranks(void)
{
    sint16 state = game_selection.selection;
    uint32 total;
    uint32 player_rank;
    sint16 limit;
    sint16 idx;

    FUNCTION_MARKER(0x8005737C, "MAIN.EXE");
    if (state != -1 && state != -3)
        return -3;
    total = (uint16)boat_racer_count + boat_leader_count + boat_trailer_count;
    limit = total;
    player_rank = race_selection.ranks[0];
    if (limit > 1)
    {
        for (idx = 1; idx < limit; ++idx)
        {
            uint32 val = race_selection.ranks[idx];
            if (player_rank < val)
                race_selection.ranks[idx] = val - 1;
        }
    }
    race_selection.ranks[0] = total - 1;
    return total - 1;
}

sint32 race_capture_time_summaries(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    uint32 profile_level;
    uint32 profile_row;
    BOAT_MENU *peer;
    PROFILE_SUMMARY *rec;
    uint32 peer_idx;
    sint32 idx;
    sint32 result = -1;
    sint32 selection;

    FUNCTION_MARKER(0x80057448, "MAIN.EXE");
    if (game_selection.menu_variant != 0)
        return result;
    result = -3;
    selection = game_selection.selection;
    if (selection == -1 || selection == -3)
        return result;
    result = 3;
    if (game_selection.mode == 3)
        return result;
    profile_data = profile_cur();
    profile_level = profile_data->level;
    profile_row = profile_data->course;
    peer = boat_player_menu(&race_views[0]);
    rec = &profile_summaries[profile_row + 6 * profile_level];
    rec->rank = race_selection.ranks[0];
    rec->profile = profile_selection.slot;
    peer_idx = peer->best_lap_idx;
    rec->race_time[0] = text_parse_int((char *)peer->total_time.text);
    rec->race_time[1] = text_parse_int((char *)peer->total_time.text + 3u);
    rec->race_time[2] = 10 * text_parse_int((char *)peer->total_time.text + 6u);
    if (peer_idx < 10)
    {
        char *src = (char *)boat_lap_time(peer, peer_idx)->text;
        rec->lap_time[0] = text_parse_int(src);
        rec->lap_time[1] = text_parse_int(src + 3u);
        rec->lap_time[2] = 10 * text_parse_int(src + 6u);
    }
    for (idx = 0; idx < 3; ++idx)
    {
        char *src = (char *)boat_lap_time(peer, idx)->text;
        uint16 *output = race_lap_times[idx];
        output[0] = text_parse_int(src);
        output[1] = text_parse_int(src + 3u);
        result = text_parse_int(src + 6u);
        output[2] = 10 * result;
    }
    return result;
}

sint32 race_init_player(const RACE_VIEW *state, uint32 unused1, sint32 unused2)
{
    BOAT_MENU *menu = boat_player_menu(state);
    sint16 mode;
    sint32 idx;

    FUNCTION_MARKER(0x8003C458, "MAIN.EXE");
    if (state == &race_views[0])
        race_stats_reset();
    menu->lap_idx = 0;
    menu->best_lap_idx = 20;
    menu->finish_state = 0;
    time_init(&menu->best_time);
    mode = race_selection.format;
    if (mode == 1)
    {
        menu->checkpoint_idx = 0;
        menu->start_ready = 0;
    }
    else
    {
        menu->checkpoint_idx = 1;
        menu->start_ready = 1;
    }
    menu->checkpoint_total = 0;
    menu->rank = 1;
    time_init(&menu->total_time);
    for (idx = 0; idx < 5; ++idx)
        time_init(boat_checkpoint_time(menu, 0, idx));
    menu->delta_timer = 0;
    mode = race_selection.format;
    menu->mode = mode;
    if (mode == 1)
    {
        uint8 val0 = game_options.handicap[0];
        uint8 val1 = game_options.handicap[1];
        sint32 handicap0 = val0 == 6 ? 10 : val0;
        sint32 handicap1 = val1 == 6 ? 10 : val1;
        sint32 minimum = handicap0 < handicap1 ? handicap0 : handicap1;
        sint32 val = race_cfg.players == 1 ? 300 : 100 * (state == &race_views[0] ? handicap0 - minimum + 3 : handicap1 - minimum + 3);
        time_format(&menu->start_time, val);
    }
    sound_queue_command(sound_for_context(state), 0, 4);
    for (idx = 0; idx < 6; ++idx)
    {
        menu->pickups[idx] = (PICKUP_SLOT){0};
    }
    for (idx = 0; idx < 5; ++idx)
        time_format(boat_lap_time(menu, idx), 0);
    return 0;
}
