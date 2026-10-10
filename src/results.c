#include <string.h>
#include "route.h"
#include "game.h"
#include "camera.h"
#include "vehicle.h"
#include "arena.h"
#include "mesh.h"
#include "name.h"
#include "pickup.h"
#include "render.h"
#include "scene.h"
#include "replay.h"
#include "sound.h"
#include "text.h"
#include "profile.h"
#include "results.h"
#include "race_events.h"
#include "title.h"
#include "tournament.h"
#include "global.h"
#include "menu.h"
#include "timer.h"
#include "xport_trace.h"
#include "cd.h"
#include "runtime.h"
#include "game_main.h"
#include <stdlib.h>

// Checkpoint inputs from MAIN.EXE 800F5B68/800FF708/800DCC40
typedef struct
{
    uint16 parameters[30];
    uint16 laps[30];
    uint16 times[150][3];
} RESULTS_CHECKPOINTS;

static RESULTS_CHECKPOINTS results_checkpoints;

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
static TOURNAMENT_TIME results_times[33][5] = {
    {{{0, 30, 0}, 0}, {{0, 0, 0}, 2000}, {{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}}, {{{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}, {{0, 26, 0}, 0}, {{0, 30, 0}, 0}},   {{{0, 30, 0}, 0}, {{0, 30, 0}, 200}, {{0, 32, 0}, 400}, {{0, 26, 0}, 0}, {{0, 30, 0}, 0}},   {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 210}, {{0, 50, 0}, 422}, {{0, 50, 0}, 634}, {{0, 50, 0}, 0}},  {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},
    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 130}, {{0, 50, 0}, 260}, {{0, 50, 0}, 0}, {{0, 0, 0}, 0}},    {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 240}, {{0, 50, 0}, 480}, {{0, 50, 0}, 720}, {{0, 50, 0}, 0}},
    {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{0, 50, 0}, 0}, {{0, 50, 0}, 260}, {{0, 50, 0}, 520}, {{0, 50, 0}, 780}, {{0, 50, 0}, 0}}, {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},
    {{{1, 0, 0}, 0}, {{0, 50, 0}, 250}, {{0, 50, 0}, 500}, {{0, 50, 0}, 750}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},  {{{1, 0, 0}, 0}, {{0, 50, 0}, 245}, {{0, 50, 0}, 490}, {{0, 50, 0}, 735}, {{0, 50, 0}, 0}},
};

static RESULTS_COURSE results_courses[61] = {
    {1, {3, 5, 3}, 0, 3, 1, 3, results_times[8]},  {0, {3, 5, 3}, 0, 3, 1, 4, results_times[18]}, {2, {3, 5, 3}, 0, 3, 1, 4, results_times[13]}, {3, {3, 5, 3}, 0, 3, 1, 4, results_times[3]},  {4, {3, 5, 3}, 0, 3, 1, 4, results_times[23]}, {5, {3, 5, 3}, 0, 3, 1, 4, results_times[28]}, {1, {3, 5, 3}, 1, 3, 2, 3, results_times[9]},  {0, {3, 5, 3}, 1, 3, 2, 4, results_times[19]}, {2, {3, 5, 3}, 1, 3, 2, 4, results_times[14]}, {3, {3, 5, 3}, 1, 3, 2, 4, results_times[4]},  {4, {3, 5, 3}, 1, 3, 2, 4, results_times[24]}, {5, {3, 5, 3}, 1, 3, 2, 4, results_times[29]}, {1, {3, 5, 3}, 2, 3, 0, 3, results_times[10]}, {0, {3, 5, 3}, 2, 3, 0, 4, results_times[20]}, {2, {3, 5, 3}, 2, 3, 0, 4, results_times[15]}, {3, {3, 5, 3}, 2, 3, 0, 4, results_times[5]},  {4, {3, 5, 3}, 2, 3, 0, 4, results_times[25]}, {5, {3, 5, 3}, 2, 3, 0, 4, results_times[30]}, {1, {3, 5, 3}, 1, 3, 1, 3, results_times[9]},  {0, {3, 5, 3}, 1, 3, 1, 4, results_times[19]}, {2, {3, 5, 3}, 1, 3, 1, 4, results_times[14]},
    {3, {3, 5, 3}, 1, 3, 1, 4, results_times[4]},  {4, {3, 5, 3}, 1, 3, 1, 4, results_times[24]}, {5, {3, 5, 3}, 1, 3, 1, 4, results_times[29]}, {1, {3, 5, 3}, 2, 3, 2, 3, results_times[10]}, {0, {3, 5, 3}, 2, 3, 2, 4, results_times[20]}, {2, {3, 5, 3}, 2, 3, 2, 4, results_times[15]}, {3, {3, 5, 3}, 2, 3, 2, 4, results_times[5]},  {4, {3, 5, 3}, 2, 3, 2, 4, results_times[25]}, {5, {3, 5, 3}, 2, 3, 2, 4, results_times[30]}, {1, {3, 5, 3}, 3, 3, 0, 3, results_times[11]}, {0, {3, 5, 3}, 3, 3, 0, 4, results_times[21]}, {2, {3, 5, 3}, 3, 3, 0, 4, results_times[16]}, {3, {3, 5, 3}, 3, 3, 0, 4, results_times[6]},  {4, {3, 5, 3}, 3, 3, 0, 4, results_times[26]}, {5, {3, 5, 3}, 3, 3, 0, 4, results_times[31]}, {1, {3, 5, 3}, 2, 3, 1, 3, results_times[10]}, {0, {3, 5, 3}, 2, 3, 1, 4, results_times[20]}, {2, {3, 5, 3}, 2, 3, 1, 4, results_times[15]}, {3, {3, 5, 3}, 2, 3, 1, 4, results_times[5]},  {4, {3, 5, 3}, 2, 3, 1, 4, results_times[25]}, {5, {3, 5, 3}, 2, 3, 1, 4, results_times[30]},
    {1, {3, 5, 3}, 3, 3, 2, 3, results_times[11]}, {0, {3, 5, 3}, 3, 3, 2, 4, results_times[21]}, {2, {3, 5, 3}, 3, 3, 2, 4, results_times[16]}, {3, {3, 5, 3}, 3, 3, 2, 4, results_times[6]},  {4, {3, 5, 3}, 3, 3, 2, 4, results_times[26]}, {5, {3, 5, 3}, 3, 3, 2, 4, results_times[31]}, {1, {3, 5, 3}, 4, 3, 0, 3, results_times[12]}, {0, {3, 5, 3}, 4, 3, 0, 4, results_times[22]}, {2, {3, 5, 3}, 4, 3, 0, 4, results_times[17]}, {3, {3, 5, 3}, 4, 3, 0, 4, results_times[7]},  {4, {3, 5, 3}, 4, 3, 0, 4, results_times[27]}, {5, {3, 5, 3}, 4, 3, 0, 4, results_times[32]}, {6, {3, 5, 3}, 2, 3, 1, 3, results_times[1]},  {6, {3, 5, 3}, 3, 3, 1, 3, results_times[2]},  {6, {3, 5, 3}, 2, 3, 2, 3, results_times[1]},  {6, {3, 5, 3}, 3, 3, 2, 3, results_times[2]},  {4, {3, 5, 3}, 2, 3, 1, 4, results_times[25]}, {2, {3, 5, 3}, 3, 3, 1, 3, results_times[11]}, {1, {3, 5, 3}, 2, 3, 2, 4, results_times[15]},
};

// Result screen point banks from MAIN.EXE 80092010/80092028
static const sint16 results_points[2][5][2] = {
    {{40, 40}, {30, 34}, {16, 24}, {6, 10}, {0, 0}},
    {{38, 38}, {24, 29}, {9, 14}},
};

static sint32 results_build_checkpoints(void)
{
    uint32 group;

    FUNCTION_MARKER(0x80048B30u, "MAIN.EXE");
    for (group = 0u; group < 3u; ++group)
    {
        sint32 outer;
        uint32 desc_index = 0u;
        for (outer = (sint32)group; outer < (sint32)group + 3; ++outer)
        {
            sint32 row;
            for (row = 0; row < 6; ++row, ++desc_index)
            {
                RESULTS_COURSE *desc = &results_courses[group * 18u + desc_index];
                TOURNAMENT_TIME *output = desc->times;
                sint32 column;
                desc->laps = (uint16)(results_checkpoints.laps[outer + 5 * row] + 1u);
                for (column = 0; column < 5; ++column)
                {
                    const uint16 *source = results_checkpoints.times[25 * row + 5 * outer + column];
                    output->parts[0] = source[0];
                    output->parts[1] = source[1];
                    output->parts[2] = source[2];
                    output->parameter = results_checkpoints.parameters[5 * row + column];
                    ++output;
                }
            }
        }
    }
    return 0;
}

// ASCII checkpoint file separators
static sint32 results_checkpoint_space(uint8 chr)
{
    if (chr > 127u)
        abort();
    return chr == ' ' || (chr >= 9u && chr <= 13u);
}

sint32 results_load_checkpoints(void)
{
    const uint8 *cursor;
    const uint8 *end;
    size_t size;
    sint16 section = 0;
    sint16 index = 0;
    sint32 stop = 0;

    FUNCTION_MARKER(0x800487F0u, "MAIN.EXE");
    game_push_checkpoint();
    strcpy(cd_dir, "\\MENU");
    cursor = cd_load_file_bytes("\\CHKPOINT.DAT", &size);
    if (!cursor || size == 0u)
        abort();
    end = cursor + size;
    do
    {
        const uint8 *token;
        sint16 length = 0;
        sint32 value;
        uint8 chr;

        while (cursor < end && results_checkpoint_space(*cursor))
            ++cursor;
        if (cursor == end)
            abort();
        if (*cursor == ';')
        {
            ++cursor;
            while (cursor < end && *cursor++ != 10u)
            {
            }
            if (cursor == end)
                abort();
        }
        token = cursor;
        while (cursor < end && !results_checkpoint_space(*cursor))
        {
            if (length == INT16_MAX)
                abort();
            ++cursor;
            ++length;
        }
        if (cursor == end)
            abort();
        chr = length == 0 ? 0u : *token;
        if (chr == '!')
        {
            ++section;
            index = 0;
        }
        else if (chr == '*')
            stop = 1;
        else if (chr != ';')
        {
            ++cursor;
            value = runtime_parse_decimal(token, length);
            if (section == 0)
            {
                if (index < 0 || index >= 30)
                    abort();
                results_checkpoints.parameters[index] = (uint16)value;
            }
            else if (section == 1)
            {
                if (index < 0 || index >= 30)
                    abort();
                results_checkpoints.laps[index] = (uint16)value;
            }
            else if (section == 2)
            {
                if (index < 0 || index >= 450)
                    abort();
                results_checkpoints.times[index / 3][index % 3] = (uint16)value;
            }
            ++index;
        }
    } while (stop == 0 && section < 9);
    game_pop_checkpoint();
    return results_build_checkpoints();
}

// Result bytes from MAIN.EXE 800E05B3/05B4/05B8
RESULT_STATE result_state;

// Grid columns from MAIN.EXE 80091480
const uint32 results_grid_columns[3] = {2u, 1u, 0u};

sint32 results_layout_populate(const RACE_VIEW *state)
{
    BOAT_MENU *src;
    HUD_DESC *descs;
    HUD_DESC *desc;
    char *text;
    const TIME_REC *time_rec;
    uint32 racer_count;
    uint32 leader_count;
    uint32 trailer_count;
    sint32 result;
    sint32 idx;
    sint32 alternate;
    sint32 position;
    sint32 language_is_six;
    sint32 total;

    FUNCTION_MARKER(0x8001FA38u, "MAIN.EXE");
    src = vehicle_player_menu(state);
    menu_select_result_clut(state);
    if (race_cfg.mode == 9u)
    {
        menu_update_result_rec_palettes(state);
        descs = render_hud.descs;
        result = time_copy_chars7((char *)race_time.text, descs[0].text);
        hud_text.time[4] = 0;
        return result;
    }

    descs = render_hud.descs;
    descs[16].visible = 0u;
    descs = render_hud.descs;
    desc = descs;
    for (idx = 0; idx < 8; ++idx)
    {
        desc->visible = 1u;
        ++desc;
    }
    descs = render_hud.descs;
    desc = descs + 17;
    for (idx = 0; idx < 4; ++idx)
    {
        desc->visible = 0u;
        ++desc;
    }
    language_is_six = (sint16)game_selection.mode == 6;
    descs = render_hud.descs;
    descs[11].visible = (uint8)language_is_six;

    alternate = (sint32)result_state.bonus_timer > 0 && src->mode == 4u;
    time_rec = alternate ? &race_bonus_time : &race_time;
    if ((sint32)race_freeze_time.ticks > 0 && !alternate)
    {
        descs = render_hud.descs;
        descs[0].clut = 27508u;
    }
    else
    {
        sint32 row = (sint32)time_rec->ticks / 500;
        uint16 clut;

        if ((sint16)row >= 10)
            row = 9;
        clut = getClut(832, 449 - (sint16)row);
        descs = render_hud.descs;
        descs[0].clut = clut;
    }

    alternate = (sint32)result_state.bonus_timer > 0 && src->mode == 4u;
    descs = render_hud.descs;
    descs[8].visible = (uint8)alternate;
    descs = render_hud.descs;
    descs[9].visible = (uint8)alternate;
    descs = render_hud.descs;
    time_rec = alternate ? &race_bonus_time : &race_time;
    position = alternate ? (sint32)((result_state.bonus_timer >> 3) & 1u) : 1;
    descs[0].visible = (uint8)position;
    time_copy_chars7((char *)time_rec->text, descs[0].text);
    hud_text.time[4] = 0;

    position = alternate;
    if (!alternate)
    {
        position = 1;
        if ((sint16)camera_for_view(state)->direction == 2)
            position = src->mode != 4u;
    }
    descs = render_hud.descs;
    descs[10].visible = (uint8)(1 - position);

    position = src->lap_idx;
    descs = render_hud.descs;
    desc = descs + 3;
    for (idx = 0; idx < 4; ++idx)
    {
        desc->visible = position >= idx;
        if (position - idx >= 0)
        {
            char *lap_text = (char *)vehicle_lap_time(src, (uint32)idx)->text;

            if (lap_text[0] == '0')
                time_copy_chars7(lap_text, desc->text);
            else
                name_bytes_copy8(lap_text, desc->text);
            desc->clut = r_u16(0x800B3E28u + (uint32)(position - idx) * 2u);
        }
        ++desc;
    }

    if (race_cfg.mode == 8u)
    {
        descs = render_hud.descs;
        desc = descs;
        for (idx = 0; idx < 17; ++idx)
        {
            desc->visible = 0u;
            ++desc;
        }
        descs = render_hud.descs;
        text = descs[17].text;
        descs[17].visible = 1u;
        time_copy_chars7((char *)race_time.text, text);
        descs = render_hud.descs;
        desc = descs + 18;
        for (idx = 0; idx < 3; ++idx)
        {
            sint16 value = (sint16)result_state.hud_values[idx];

            if (value == -1)
                desc->visible = 0u;
            else
            {
                desc->visible = 1u;
                value = (sint16)result_state.hud_values[idx];
                text_format_decimal_digits(value, desc->text, 2);
            }
            ++desc;
        }
        return render_activate_recs(render_hud.prims);
    }

    racer_count = (uint16)vehicle_racer_count;
    leader_count = vehicle_leader_count;
    trailer_count = vehicle_trailer_count;
    desc->visible = (uint8)language_is_six;
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[12].visible = (uint8)language_is_six;
    alternate = src->racer_num;
    total = (sint32)(racer_count + leader_count);
    descs = render_hud.descs;
    text = descs[1].text;
    total = (sint16)(total + (sint32)trailer_count);
    text_format_decimal_digits(alternate, text, 2);
    text[2u] = (uint8)('/');
    text_format_decimal_digits(total, text + 3u, 2);
    position = src->lap_idx;
    position = (sint16)(position + (sint32)result_state.lap_offset + 1);
    if ((sint16)position >= 100)
        position = 99;
    {
        sint16 course_total;

        descs = render_hud.descs;
        course_total = scene_race->laps;
        alternate = (sint16)position < 10;
        text = descs[2].text;

        if (alternate)
        {
            text_format_decimal_digits(position, text, 1);
            text[1u] = (uint8)('/');
            text_format_decimal_digits(course_total, text + 2u, 1);
        }
        else
        {
            text_format_decimal_digits(position, text, 2);
            text[2u] = (uint8)(0u);
        }
    }
    descs = render_hud.descs;
    alternate = src->total_time.text[0];
    text = descs[7].text;
    if (alternate == '0')
        time_copy_chars7((char *)src->total_time.text, text);
    else
        name_bytes_copy8((char *)src->total_time.text, text);
    menu_update_result_rec_palettes(state);

    alternate = race_cfg.mode == 4u;
    if (alternate)
        render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[0].visible = 0u;
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[1].visible = (uint8)(1 - alternate);
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[7].visible = (uint8)(1 - alternate);
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[13].visible = (uint8)alternate;
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[12].visible = (uint8)alternate;
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[15].visible = (uint8)alternate;
    render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[14].visible = (uint8)alternate;
    if (alternate)
    {
        if ((sint16)position >= 10)
            hud_text.lap[2] = 0;
        else
            hud_text.lap[1] = 0;
        render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[8].visible = 0u;
        render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[9].visible = 0u;
        result = (sint16)position < 10;
    }
    else
    {
        hud_text.lap[1] = '/';
        result = '/';
    }
    return result;
}

sint32 results_init_marker_recs(void)
{
    static EVENT_REC recs[3];
    sint32 angle = 768;
    sint32 index;
    const PROFILE_GRID *table = &profile_at(profile_selection.slot)->grid[race_cfg.boats[0]];

    FUNCTION_MARKER(0x8003B9C4u, "MAIN.EXE");
    race_event_markers.recs = recs;
    race_event_markers.count = 3;
    race_event_markers.idx = 0;
    for (index = 0; index < 3; ++index)
    {
        EVENT_REC *rec = &recs[index];
        sint32 sine = rsin(angle);
        sint32 cosine = rcos(angle);
        sint32 type = index;
        rec->pos[0] = (uint16)((uint16)route_showcase_seg.origin[0] + 1000 * cosine / 4096 + 2000 * cosine / 4096);
        rec->pos[1] = (uint16)scene_horizon->ground;
        rec->pos[2] = (uint16)((uint16)route_showcase_seg.origin[2] + 1000 * sine / 4096 + 2000 * sine / 4096);
        if (table->offset[results_grid_columns[index]] == 2u)
            type = 3;
        rec->angle = (uint16)((type << 12) | (angle & 0x0FFF));
        rec->kind = 0u;
        angle += 256;
    }
    return 0;
}

static const RESULTS_COURSE *results_select_course(const RESULTS_COURSE *fallback)
{
    const PLAYER_PROFILE *profile_data;
    uint32 index;
    uint32 profile;
    uint32 second;
    uint32 first;

    FUNCTION_MARKER(0x8003BC14u, "MAIN.EXE");
    if (race_selection.special != 0u)
    {
        sound_options.track_slot = 6u;
        return &results_courses[54];
    }
    if ((sint16)game_selection.mode == 4)
    {
        if (game_selection.attract >= 3u)
            abort();
        return &results_courses[58u + game_selection.attract];
    }
    profile_data = profile_current();
    second = profile_data->level;
    profile = profile_selection.slot;
    first = profile_data->course;
    index = first + 6u * second;
    if (profile < 3u)
    {
        index += profile * 18u;
        if (index >= 61u)
            abort();
        fallback = &results_courses[index];
    }
    sound_options.track_slot = profile_data->course;
    if ((sint16)game_selection.mode == 3)
        sound_options.track_slot = 7u;
    return fallback;
}

sint32 race_config_results(void)
{
    const RESULTS_COURSE *desc;
    SCENE_RACE_CFG *cfg;
    const TOURNAMENT_TIME *source;
    const TOURNAMENT_TIME *times = NULL;
    sint32 mode;
    sint32 selector;
    sint32 publish_selector = 1;
    sint32 vertical = 220;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8003BEB4u, "MAIN.EXE");

    time_init(&race_freeze_time);
    result_state.bonus_timer = 0u;
    desc = results_select_course(NULL);
    for (index = 0; index < 4; ++index)
        pickup_hud[index].limit = 0u;

    race_cfg.course = (uint32)((uint32)(sint32)(sint16)desc->course);
    race_cfg.variant = (uint32)((uint32)(sint32)(sint16)desc->variant);
    scene_select(race_cfg.course, race_cfg.variant);
    {
        uint8 desc_mode = desc->ai_variant;

        cfg = scene_race;
        race_selection.ai_variant = desc_mode;
    }
    cfg->laps = (sint16)desc->parameter;
    cfg->checkpoint_count = (sint16)(uint16)(desc->laps + 1u);
    mode = (sint16)game_selection.mode;
    source = desc->times;

    switch (mode)
    {
        case 0:
        case 1:
            selector = game_selection.menu_variant != 0u ? 1 : 2;
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
            sint32 submode = (sint16)game_selection.rules;

            if (submode == 1)
            {
                cfg->laps = 1;
                for (index = 0; index < 5; ++index)
                    tournament_times[index].parameter = source[index].parameter;
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
            source = results_times[0];
            selector = 1;
            break;
        default:
            publish_selector = 0;
            break;
    }
    if (publish_selector)
        race_selection.format = (uint16)selector;
    if ((sint16)race_selection.special != 0)
        race_selection.format = 1u;

    for (index = 0; index < 5; ++index)
    {
        const TOURNAMENT_TIME *input = &source[index];
        SCENE_CHECKPOINT *dst = &cfg->checkpoints[index];
        sint32 first = times != NULL ? (sint16)times[index].parts[0] : (sint16)input->parts[0];
        sint32 third = times != NULL ? (sint16)times[index].parts[2] : (sint16)input->parts[2];
        sint32 second = times != NULL ? (sint16)times[index].parts[1] : (sint16)input->parts[1];
        sint32 value = (sint32)(6000u * (uint32)first + 100u * (uint32)second + (uint32)(third / 10));

        time_format(&dst->time, value);
        dst->seg = (sint16)(times != NULL ? times[index].parameter : input->parameter);
    }
    result_state.lap_active = (sint16)race_selection.format != 1;
    mode = (sint32)race_cfg.mode;
    result_state.finished = 0u;
    if (mode == 8)
    {
        uint8 profile = profile_selection.slot;
        sint32 value = profile == 1u ? 3000 : (profile == 2u ? 2500 : 3500);

        time_format(&race_time, value);
    }
    else
    {
        time_copy(&race_time, &cfg->checkpoints[0].time);
        time_add(&race_time, &cfg->checkpoints[1].time);
    }
    mode = (sint32)race_cfg.players;
    result_state.checkpoint_max = 0u;

    if (mode == 1)
        vertical = 420;
    pickup_hud[2].limit = desc->counts[1];
    pickup_hud[2].center_x = 76u;
    {
        uint16 count = desc->counts[0];

        pickup_hud[1].center_x = 192u;
        pickup_hud[1].limit = count;
    }
    pickup_hud[3].limit = desc->counts[2];
    pickup_hud[3].center_x = 308u;
    {
        sint32 count = (sint16)pickup_hud[2].limit;
        const sint16(*points)[2];
        if (count < 0 || count >= 6)
            abort();
        points = results_points[0];

        for (index = 0; index < (sint16)pickup_hud[2].limit; ++index)
        {
            pickup_hud[2].x[index] = (uint16)(points[0][0] + 20);
            pickup_hud[2].y[index] = (uint16)(vertical + points[0][1] - 38);
            ++points;
        }
    }
    {
        sint32 count = (sint16)pickup_hud[1].limit;
        sint32 span = 18 * count + 10;
        sint32 half = (span - (span < 0)) / 2;
        uint32 left = pickup_hud[1].center_x - (uint32)half;

        for (index = 0; index < count; ++index)
        {
            pickup_hud[1].y[index] = (uint16)(vertical + 200);
            pickup_hud[1].x[index] = (uint16)(left + 18u * (uint32)index);
        }
    }
    result = 4 * (sint16)pickup_hud[3].limit;
    {
        sint32 count = (sint16)pickup_hud[3].limit;
        const sint16(*points)[2];
        if (count < 0 || count >= 6)
            abort();
        points = results_points[1];

        for (index = 0; index < (sint16)pickup_hud[3].limit; ++index)
        {
            pickup_hud[3].x[index] = (uint16)(384 - (points[0][0] + 48));
            pickup_hud[3].y[index] = (uint16)(vertical + points[0][1] - 38);
            ++points;
            result = (sint16)pickup_hud[3].limit;
        }
    }
    result_state.lap_offset = 0u;
    return result;
}

sint32 results_select_best_time_rec(BOAT_MENU *menu)
{
    uint32 best = UINT32_MAX;
    sint32 count = menu->lap_idx;
    sint32 index;

    FUNCTION_MARKER(0x8003CA84u, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        uint32 value = vehicle_lap_time(menu, (uint32)index)->ticks;
        if (value < best)
        {
            best = value;
            menu->best_lap_idx = (uint8)index;
        }
    }
    return index < count;
}

sint32 results_fn_8003cb14(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 choice = vehicle_player_menu(state)->racer_num;
    sint32 distance;

    FUNCTION_MARKER(0x8003CB14u, "MAIN.EXE");
    menu->finish_state = 7u;
    if (!(game_selection.mode == 6u && game_selection.rules == 1u))
    {
        if (race_cfg.players == 1u)
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
            BOAT_MENU *peer = state == &race_views[0] ? vehicle_player_menu(&race_views[1]) : vehicle_player_menu(&race_views[0]);
            distance = (sint32)(vehicle_player_menu(state)->total_time.ticks - peer->total_time.ticks);
            if (distance >= 1000)
                sound_queue_command(sound_for_context(state), 13, 0);
            else if (distance >= 500)
                sound_queue_command(sound_for_context(state), 7, 7);
            else if (distance >= 301)
                sound_queue_command(sound_for_context(state), 3, 0);
        }
        sound_queue_command(sound_for_context(state), 0, 5);
    }
    result_state.finished = 1u;
    menu->mode = 5u;
    camera_for_view(state)->mode = 5u;
    result_state.bonus_timer = 0u;
    results_select_best_time_rec(menu);
    --menu->lap_idx;
    time_format(&race_end_time, 500);
    w_u16(0x800B6ACCu, 1u);
    result_state.pickups = (uint8)(menu->pickups[2].count);
    return pickup_write_racer_indices(race_selection.ranks);
}

sint32 results_advance_page(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    uint32 current;
    uint32 previous;
    uint32 value;
    sint32 index;
    sint32 maximum;

    FUNCTION_MARKER(0x8003CCECu, "MAIN.EXE");
    current = menu->lap_idx;
    menu->lap_idx = (uint8)(current + 1u);
    if ((sint16)game_selection.mode == 2)
    {
        current = menu->lap_idx;
        if (current == 4u)
        {
            for (index = 0; index < 3; ++index)
                time_copy(vehicle_lap_time(menu, (uint32)index), vehicle_lap_time(menu, (uint32)index + 1u));
            time_init(vehicle_lap_time(menu, 3u));
            current = menu->lap_idx;
            menu->lap_idx = (uint8)(current - 1u);
            current = result_state.lap_offset;
            result_state.lap_offset = (uint16)(current + 1u);
        }
        current = menu->lap_idx;
        {
            TIME_REC *entry = vehicle_lap_time(menu, current - 1u);

            value = entry->ticks;
            current = menu->lap_idx;
            if (current == 1u)
            {
                replay_finish(&vehicle_players[0]->contacts.points[0], 1);
                time_copy_chars7((char *)entry->text, hud_text.best);
                previous = result_state.record_ticks;
                result_state.best_lap_ticks = value;
            }
            else
            {
                previous = result_state.best_lap_ticks;
                if (value > previous)
                {
                    replay_finish(&vehicle_players[0]->contacts.points[0], 0);
                    menu->checkpoint_idx = 1u;
                    return 0;
                }
                replay_finish(&vehicle_players[0]->contacts.points[0], 1);
                result_state.best_lap_ticks = value;
                time_copy_chars7((char *)entry->text, hud_text.best);
                previous = result_state.record_ticks;
            }
            if (value < previous)
            {
                sound_queue_command(sound_for_context(state), 1, 0);
                result_state.record_ticks = value;
                time_copy_chars7((char *)entry->text, hud_text.record);
                time_copy(&menu->best_time, entry);
            }
        }
        menu->checkpoint_idx = 1u;
        return 0;
    }

    menu->checkpoint_idx = 1u;
    current = menu->lap_idx;
    maximum = scene_race->laps;
    if ((sint32)current == maximum)
    {
        results_fn_8003cb14(state, menu, unused, argument);
        return 1;
    }
    if ((sint32)current == maximum - 1)
        sound_queue_command(sound_for_context(state), 0, 0);
    else
    {
        if ((sint32)race_time.ticks >= 200)
            return 0;
        sound_queue_command(sound_for_context(state), 9, 0);
    }
    return 0;
}

sint32 results_fn_8003cf10(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8003CF10u, "MAIN.EXE");
    if (menu->pickups[2].count >= 5u)
        result_state.status = (uint8)(2u);
    if (result_state.status == 0u)
        return 5;
    if (menu->mode == 5u || menu->mode == 7u)
        return 7;
    profile_data = profile_at(profile_selection.slot);
    if (result_state.status == 2u)
    {
        sound_queue_command(sound_for_context(state), 2, 0);
        profile_data->progress.courses[profile_data->level][profile_data->course].result = 2u;
    }
    else if (result_state.status == 1u)
        sound_queue_command(sound_for_context(state), 4, 0);
    sound_queue_command(sound_for_context(state), 0, 5);
    menu->mode = 5u;
    time_format(&race_end_time, 300);
    w_u32(0x800B6ACCu, 1u);
    return 1;
}

sint32 race_process_lap_completion(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 phase;
    sint32 completed = 0;
    sint32 result;

    FUNCTION_MARKER(0x8003D04Cu, "MAIN.EXE");
    phase = (sint32)menu->mode;
    if (phase == 4 || phase == 6)
    {
        sint32 enabled = 1;

        if ((sint16)game_selection.mode == 6 && (sint16)game_selection.rules == 1 && (sint32)menu->mode != 4)
            enabled = 0;
        if (enabled != 0 && menu->checkpoint_idx == 0u)
        {
            result_state.lap_active = 1u;
            menu->checkpoint_idx = 1u;
            if (race_cfg.mode == 4u)
                replay_begin(&vehicle_players[0]->contacts.points[0]);
        }
        else if (enabled != 0)
        {
            uint32 count;

            ++menu->checkpoint_idx;
            {
                uint32 checkpoint = menu->checkpoint_idx;
                sint32 checkpoint_count = scene_race->checkpoint_count;

                if (checkpoint == (uint32)checkpoint_count)
                {
                    completed = results_advance_page(state, menu, unused, argument);
                    if (race_cfg.mode == 4u)
                        sound_queue_command(sound_for_context(state), 8, 1);
                }
            }
            {
                uint32 lap = menu->lap_idx;
                sint32 source = (sint32)menu->total_time.ticks;
                uint32 checkpoint = menu->checkpoint_idx;

                time_format(vehicle_checkpoint_time(menu, lap, checkpoint), source);
            }
            count = menu->checkpoint_total + 1u;
            menu->checkpoint_total = count;
            if ((sint32)result_state.checkpoint_max < (sint32)count)
            {
                result_state.checkpoint_max = count;
                if (((uint32)completed << 16) == 0u && ((sint16)game_selection.mode != 6 || (sint16)game_selection.rules != 1) && race_cfg.mode != 4u)
                {
                    phase = (sint32)menu->mode;
                    if (phase == 6 && result_state.lap_active != 0u)
                    {
                        TIME_REC scratch;
                        BOAT_MENU *peer;

                        sound_queue_command(sound_for_context(state), 9, 0);
                        time_format(&scratch, 300);
                        time_sub(&scratch, &race_end_time);
                        time_sub(&race_time, &scratch);
                        peer = vehicle_player_menu(&race_views[0]);
                        if (peer->mode == 6u)
                        {
                            uint32 player_index;

                            time_add(&vehicle_player_menu(&race_views[0])->total_time, &scratch);
                            peer = vehicle_player_menu(&race_views[0]);
                            player_index = peer->lap_idx;
                            peer = vehicle_player_menu(&race_views[0]);
                            time_add(vehicle_lap_time(peer, player_index), &scratch);
                            vehicle_player_menu(&race_views[0])->mode = 4u;
                        }
                        if (race_cfg.players == 2u)
                        {
                            peer = vehicle_player_menu(&race_views[1]);
                            if (peer->mode == 6u)
                            {
                                uint32 player_index;

                                time_add(&vehicle_player_menu(&race_views[1])->total_time, &scratch);
                                peer = vehicle_player_menu(&race_views[1]);
                                player_index = peer->lap_idx;
                                peer = vehicle_player_menu(&race_views[1]);
                                time_add(vehicle_lap_time(peer, player_index), &scratch);
                                vehicle_player_menu(&race_views[1])->mode = 4u;
                            }
                        }
                        time_format(&race_end_time, 300);
                    }
                    {
                        sint32 lap_count = scene_race->laps;
                        uint32 lap = menu->lap_idx;

                        if (lap == (uint32)(lap_count - 1))
                        {
                            sint32 checkpoint_count = scene_race->checkpoint_count;
                            uint32 checkpoint = menu->checkpoint_idx;

                            if (checkpoint == (uint32)(checkpoint_count - 1))
                                title_show_finish();
                        }
                    }
                    {
                        uint32 mode = race_cfg.mode;

                        if (mode != 9u && mode != 5u)
                            sound_queue_command(sound_for_context(state), 8, 1);
                    }
                    {
                        uint32 checkpoint = menu->checkpoint_idx;
                        const TIME_REC *rec;

                        result_state.bonus_timer = 120u;
                        rec = &scene_checkpoint(checkpoint)->time;
                        time_add(&race_time, rec);
                        time_copy(&race_bonus_time, rec);
                    }
                }
            }
            else
            {
                BOAT_MENU *peer;

                menu->delta_timer = 240u;
                peer = state == &race_views[0] ? vehicle_player_menu(&race_views[1]) : vehicle_player_menu(&race_views[0]);
                peer->delta_timer = 240u;
                sound_queue_command(sound_for_context(state), 8, 1);
            }
        }
    }
    result = (sint16)(uint16)menu->mode;
    return result;
}

void results_2p_complete_route(void)
{
    sint32 state = (sint16)game_selection.mode;
    sint32 selection = (sint16)game_selection.selection;

    FUNCTION_MARKER(0x8005D8E8u, "MAIN.EXE");
    game_selection.event = 10u;
    game_selection.event_arg = 0u;
    if (state == 0)
    {
        if (selection == -3)
        {
            game_selection.event = 48u;
            return;
        }
        if (selection != -4 && selection != -2)
            return;
        game_selection.event = 51u;
        result_state.winner = (uint16)((sint32)vehicle_player_menu(&race_views[0])->total_time.ticks >= (sint32)vehicle_player_menu(&race_views[1])->total_time.ticks);
        return;
    }
    if (state == 1)
    {
        result_state.count = (uint16)((uint16)vehicle_racer_count + vehicle_leader_count + vehicle_trailer_count);
        {
            profile_complete_selection();
            return;
        }
    }
}
