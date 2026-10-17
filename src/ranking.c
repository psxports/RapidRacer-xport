#include "menu_select.h"
#include "game.h"
#include "boat.h"
#include "input.h"
#include "disp.h"
#include "name.h"
#include "sprite.h"
#include "upgrades.h"
#include "ranking.h"
#include "utils.h"
#include "menu.h"
#include "profile.h"
#include "race.h"
#include "text.h"
#include "timer.h"
#include "xport_trace.h"
#include <stdlib.h>

typedef struct
{
    uint16 ticks;
    uint16 active;
    uint16 remaining;
    uint16 delta;
} RANKING_SCROLL;

static RANKING_SCROLL ranking_scroll;

static void ranking_format_name(const char *src, char *dst)
{
    sint32 idx = 0;

    while (src[idx] != 0)
    {
        dst[idx] = src[idx];
        ++idx;
    }
    while (idx < 12)
        dst[idx++] = ' ';
    dst[idx] = '\0';
}

// Place scores from MAIN.EXE 80098E08
static const uint8 ranking_points[16] = {10, 7, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

enum
{
    RANKING_CAPACITY = sizeof(race_selection.ranks) / sizeof(race_selection.ranks[0])
};

// Participant order from MAIN.EXE 800DCC10
static uint16 ranking_order[RANKING_CAPACITY];

// Keep the original signed observation within native participant capacity
static sint16 ranking_count(uint16 val)
{
    sint16 count = val;

    return count > RANKING_CAPACITY ? RANKING_CAPACITY : count;
}

uint8 ranking_place_points(uint32 place)
{
    return place < sizeof(ranking_points) / sizeof(ranking_points[0]) ? ranking_points[place] : 0u;
}

static sint32 ranking_write_time(char *output, uint16 minutes, uint16 seconds, uint16 fraction)
{
    uint32 remainder;
    uint32 tens;

    output[0] = 48;
    output[2u] = 48;
    output[3u] = 48;
    output[5u] = 48;
    output[6u] = 48;
    if (minutes != 0)
        output[0] = minutes + 48u;
    if (seconds != 0)
    {
        remainder = seconds % 100u;
        output[2u] = remainder / 10 + 48;
        output[3u] = remainder % 10 + 48;
    }
    if (fraction == 0)
        return fraction;
    tens = fraction / 10u % 100u / 10u;
    output[5u] = tens + 48;
    output[6u] = fraction / 10u % 10u + 48u;
    return 10 * tens;
}

static sint32 ranking_update_peer_times(BOAT_MENU *peer, TEXT_RECORD *menu, uint32 visible_base, uint32 empty_base, uint32 text_base, uint32 text_empty)
{
    sint32 idx;
    sint32 result = 0;

    for (idx = 0; idx < 3; ++idx)
    {
        TEXT_RECORD *visible = &menu[visible_base + idx];
        TEXT_RECORD *empty = &menu[empty_base + idx];
        if (idx < peer->lap_idx || peer->finish_state != 0)
        {
            char *src = (char *)boat_lap_time(peer, idx)->text;
            visible->visible = 1;
            empty->visible = 0;
            result = ranking_write_time(visible->text, text_parse_int(src), text_parse_int(src + 3u), 10 * text_parse_int(src + 6u));
        }
        else
        {
            visible->visible = 0;
            empty->visible = 1;
        }
    }
    if (peer->finish_state != 0)
    {
        menu[text_base].visible = 1;
        menu[text_empty].visible = 0;
        result = ranking_write_time(menu[text_base].text, text_parse_int((char *)peer->total_time.text), text_parse_int((char *)peer->total_time.text + 3u), 10 * text_parse_int((char *)peer->total_time.text + 6u));
    }
    else
    {
        menu[text_base].visible = 0;
        menu[text_empty].visible = 1;
        result = 1;
    }
    return result;
}

sint32 ranking_find_empty_slot(void)
{
    sint32 count = ranking_count(result_state.grid_count);
    sint32 idx;

    FUNCTION_MARKER(0x8005B6B8, "MAIN.EXE");
    if (count <= 0)
        return 15;
    for (idx = 0; idx < count; ++idx)
    {
        if (ranking_order[idx] == 0)
            return idx;
    }
    return 15;
}

uint8 ranking_player_is_leading(void)
{
    PROFILE_SERIES *series;
    sint32 count = ranking_count(result_state.grid_count);
    uint8 vals[RANKING_CAPACITY];
    sint16 order[RANKING_CAPACITY];
    sint32 idx;
    sint32 pass;

    FUNCTION_MARKER(0x8005B724, "MAIN.EXE");

    series = profile_get_series();
    if (count < 0)
        count = 0;
    for (idx = 0; idx < count; ++idx)
        vals[idx] = series->points[idx];
    if (game_selection.selection == -4 || game_selection.selection == -2)
    {
        for (idx = 0; idx < count; ++idx)
            vals[idx] = vals[idx] + ranking_place_points(race_selection.ranks[idx]);
    }
    else if (name_matches(0, name_code_winr) != 0 && count > 0)
        vals[0] = vals[0] + 15u;
    for (idx = 0; idx < count; ++idx)
        order[idx] = idx;
    for (pass = 0; pass < count; ++pass)
    {
        for (idx = 0; idx < count - 1; ++idx)
        {
            if (order[pass] != order[idx] && vals[order[pass]] >= vals[order[idx]])
            {
                sint16 tmp = order[pass];
                order[pass] = order[idx];
                order[idx] = tmp;
            }
        }
    }
    if (count == 0)
        return 1;
    return order[0] == 0;
}

sint32 ranking_refresh_championship(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    sint32 sel = profile_data->course - 1;
    sint32 count = ranking_count(result_state.grid_count);
    sint32 idx;

    FUNCTION_MARKER(0x8005B950, "MAIN.EXE");
    race_selection.boats[0] = series->grid_row;
    menu_boat_select.boat = race_selection.boats[0];
    for (idx = 0; idx < 6; ++idx)
    {
        uint8 active = idx == sel;
        menu[2 * idx + 1].visible = active;
        menu[2 * idx + 2].visible = active;
    }
    for (idx = 0; idx < 9; ++idx)
    {
        menu[3 * idx + 21].visible = 0;
        menu[3 * idx + 22].visible = 0;
        menu[3 * idx + 23].visible = 0;
    }
    if (game_selection.selection == -4 || game_selection.selection == -2)
    {
        for (idx = 0; idx < count; ++idx)
            series->points[idx] = series->points[idx] + ranking_place_points(race_selection.ranks[idx]);
    }
    else if (name_matches(0, name_code_winr) != 0)
        series->points[0] = series->points[0] + 15u;
    else
    {
        for (idx = 1; idx < count; ++idx)
            series->points[idx] = series->points[idx] + ranking_place_points(race_selection.ranks[idx]);
    }
    ranking_build_order();
    for (idx = 0; idx < count; ++idx)
    {
        const RACE_PARTICIPANT *participant = &race_participants[idx];
        uint32 pos = 3u * participant->boat + participant->variant;
        uint32 score = race_selection.ranks[idx];
        if (score < 8)
        {
            menu[pos + 21].visible = 1;
            menu[pos + 21].y = 12 * score + 78;
        }
    }
    if (game_selection.selection == -1 || game_selection.selection == -3)
    {
        for (idx = 0; idx < 11; ++idx)
            menu[idx + 49].visible = 0;
        return 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        profile_format_parts(race_lap_times[idx], menu[idx + 50].text);
    }

    {
        sint32 selection = sel + 6 * profile_data->level;
        PROFILE_SUMMARY *summary = &profile_summaries[selection];
        PROFILE_RECORD *rec = &profile_recs[selection];
        sint32 matched = 0;

        profile_format_parts(summary->race_time, menu[54].text);
        for (idx = 0; idx < 5; ++idx)
            menu[idx + 55].visible = 0;
        for (idx = 0; idx < 3; ++idx)
        {
            if (time_equal(race_lap_times[idx], rec->lap.time) != 0)
            {
                matched = 1;
                menu[idx + 55].visible = 1;
            }
        }
        if (time_equal(rec->times[0], summary->race_time) != 0)
            matched = 1;
        if (matched != 0)
            menu[59].visible = 1;
    }
    return 1;
}

static sint32 ranking_refresh_standard(void)
{
    PROFILE_SERIES *series;
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    sint32 count = ranking_count(boat_racer_count);
    sint32 idx;
    sint32 result = count;

    FUNCTION_MARKER(0x8005BEAC, "MAIN.EXE");

    series = profile_get_series();
    ranking_build_order();
    for (idx = 0; idx < 3; ++idx)
        recs[idx + 1].type = idx == menu_course_select.level;
    for (idx = 0; idx < 9; ++idx)
    {
        menu[3 * idx + 10].visible = 0;
        menu[3 * idx + 11].visible = 0;
        menu[3 * idx + 12].visible = 0;
    }
    for (idx = 0; idx < count; ++idx)
    {
        sint16 ranked = ranking_order[idx];
        const RACE_PARTICIPANT *participant = &race_participants[ranked];
        uint32 pos = 3u * participant->boat + participant->variant;
        uint32 score = series->points[ranked];
        char *text = menu[idx + 37].text;

        menu[pos + 10].visible = 1;
        menu[pos + 10].y = 14 * idx + 70;
        text[0] = score / 10 + 48;
        text[1u] = score % 10 + 48;
        menu[idx + 37].visible = 1;
        result = 14 * idx + 70;
        menu[idx + 37].y = result;
    }
    return result;
}

static sint32 ranking_build_order_desc(void)
{
    PROFILE_SERIES *series;
    sint16 count;
    sint32 outer;
    sint32 inner;

    FUNCTION_MARKER(0x8005DCD8, "MAIN.EXE");

    series = profile_get_series();
    count = ranking_count(result_state.count);
    for (outer = 0; outer < count; ++outer)
        ranking_order[outer] = outer;
    for (outer = 0; outer < count; ++outer)
    {
        for (inner = 0; inner < count - 1; ++inner)
        {
            sint16 outer_idx = ranking_order[outer];
            sint16 inner_idx = ranking_order[inner];
            if (outer_idx != inner_idx && series->points[outer_idx] >= series->points[inner_idx])
            {
                ranking_order[outer] = inner_idx;
                ranking_order[inner] = outer_idx;
            }
        }
    }
    return outer < count;
}

static sint32 ranking_refresh_reduced(void)
{
    PROFILE_SERIES *series;
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    sint16 count;
    sint32 idx;
    sint32 start;
    sint32 result;

    FUNCTION_MARKER(0x8005C0F4, "MAIN.EXE");

    series = profile_get_series();
    ranking_build_order_desc();
    for (idx = 0; idx < 3; ++idx)
        recs[idx + 1].type = idx == menu_course_select.level;
    for (idx = 0; idx < 9; ++idx)
    {
        menu[3 * idx + 10].visible = 0;
        menu[3 * idx + 11].visible = 0;
        menu[3 * idx + 12].visible = 0;
    }
    start = game_options.players_only != 0 ? 2 : 4;
    for (idx = start; idx < 8; ++idx)
    {
        menu[idx + 2].visible = 0;
        menu[idx + 37].visible = 0;
    }
    count = ranking_count(result_state.count);
    if (count >= 5)
        count = 4;
    result = count;
    for (idx = 0; idx < count; ++idx)
    {
        sint16 ranked = ranking_order[idx];
        const RACE_PARTICIPANT *participant = &race_participants[ranked];
        uint32 grid = 16u * (participant->variant + 3u * participant->boat);
        uint8 score = series->points[ranked];
        char *text = menu[idx + 37].text;
        menu[participant->variant + 3 * participant->boat + 10].visible = 1;
        menu[participant->variant + 3 * participant->boat + 10].y = 14 * idx + 70;
        text[0] = score != 0 ? (uint8)(score / 10u + 48u) : 48u;
        text[1u] = score != 0 ? (uint8)(score % 10u + 48u) : 48u;
        menu[idx + 37].visible = 1;
        result = 14 * idx + 70;
        menu[idx + 37].y = result;
    }
    return result;
}

sint32 ranking_refresh(void)
{
    FUNCTION_MARKER(0x8005C3BC, "MAIN.EXE");
    if (game_selection.menu_variant != 0)
        return ranking_refresh_reduced();
    return ranking_refresh_standard();
}

void ranking_refresh_selection(void)
{
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x8005C3F8, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
        recs[idx + 1].type = idx == menu_course_select.level;
}

sint32 ranking_wrap_selection(sint32 fallback)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    sint32 result = (sint16)fallback;

    FUNCTION_MARKER(0x8005C458, "MAIN.EXE");
    if (profile_data->course == 6)
    {
        profile_data->course = 0;
        series->phase = 0;
        if (game_selection.menu_variant != 0)
            result = 4;
        else
        {
            result = 3;
            if (ranking_order[0] == 0)
            {
                menu_tex.rebuild = 1;
                result = 23;
            }
        }
    }
    return result;
}

sint32 profile_confirm_mode(PAD *input)
{
    sint32 result;

    FUNCTION_MARKER(0x8005C4FC, "MAIN.EXE");
    menu_state.level_notice = 0;
    result = input->pressed & 0x40u;
    if (result != 0)
    {
        PLAYER_PROFILE *profile_data = profile_cur();
        result = 2;
        if (profile_data->level == 2)
        {
            result = 14;
            if (profile_unlock(6) != 0)
            {
                menu_state.phase = 14;
                menu_state.level_notice = 1;
                result = 1;
            }
        }
    }
    return result;
}

sint32 profile_init_mode_state(void)
{
    FUNCTION_MARKER(0x8005C564, "MAIN.EXE");
    if (menu_state.level_notice != 0)
    {
        menu_notice.val = 6;
        menu_notice.boat = 6;
        menu_notice.type = 2;
        return 2;
    }
    return 6;
}

sint32 ranking_build_order(void)
{
    PROFILE_SERIES *series;
    sint32 count = ranking_count(result_state.grid_count);
    sint32 i;
    sint32 j;

    FUNCTION_MARKER(0x8005C598, "MAIN.EXE");

    series = profile_get_series();
    for (i = 0; i < count; ++i)
        ranking_order[i] = i;
    for (i = 0; i < count; ++i)
    {
        for (j = 0; j < count - 1; ++j)
        {
            sint16 idx0 = ranking_order[i];
            sint16 idx1 = ranking_order[j];
            if (idx0 != idx1 && series->points[idx1] < series->points[idx0])
            {
                ranking_order[i] = idx1;
                ranking_order[j] = idx0;
            }
        }
    }
    return 0;
}

sint32 profile_reset_select_state(void)
{
    PROFILE_SERIES *series = profile_get_series();
    uint8 sel;

    FUNCTION_MARKER(0x8005C6C8, "MAIN.EXE");
    text_clear_boat_names();
    if (menu_state.phase != 3 && game_selection.menu_variant == 0 && game_selection.mode == 1)
    {
        sel = menu_boat_select.boat;
        race_selection.boats[0] = sel;
        series->grid_row = sel;
    }
    if (menu_course_select.level == 3)
        menu_course_select.level = 2;
    menu_course_select.phase = MENU_COURSE_IDLE;
    return 2;
}

uint32 ranking_init_scr(void)
{
    uint8 lane_next[9];
    PLAYER_PROFILE *profile_data;
    sint16 selection;
    PROFILE_RECORD *data;
    sint32 idx;

    FUNCTION_MARKER(0x8005C768, "MAIN.EXE");
    profile_data = profile_cur();
    for (idx = 0; idx < 6; ++idx)
    {
        uint8 active = idx == menu_course_select.course;
        TEXT_RECORD *menu = text_menu;

        menu[2 * idx].visible = active;
        menu = text_menu;
        menu[2 * idx + 1].visible = active;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        uint8 active = idx == menu_course_select.level;
        UI_RECORD *recs = spr_recs;

        recs[idx + 2].type = active;
    }
    selection = profile_data->level * 6u + menu_course_select.course;
    data = &profile_recs[selection];
    text_clear_boat_names();
    for (idx = 0; idx < 9; ++idx)
        lane_next[idx] = 10 * idx;
    for (idx = 0; idx < 10; ++idx)
    {
        sint16 y = 13 * idx + 83;
        sint32 half;
        uint8 visible = 0;

        for (half = 0; half < 2; ++half)
        {
            TEXT_RECORD *menu = text_menu;
            TEXT_RECORD *desc = &menu[2 * idx + half + 13];

            desc[0].y = y;
            visible = y > 74 && y < 144;
            desc[0].visible = visible;
            desc[0].style = 0;
        }
        {
            TEXT_RECORD *prims = text_hud;
            TEXT_RECORD *prim = &prims[idx + 3];
            sint16 lane_idx;
            uint8 lane;
            TEXT_RECORD *ranking;

            prim[0].x = 160;
            prim[0].centered = 1;
            lane_idx = data->boats[idx];
            lane = lane_next[lane_idx];
            ranking = &text_hud[lane + 31];
            prim[0].y = y;
            prim[0].font = 2;
            prim[0].visible = visible;
            prim[0].text = data->names[idx];
            prim[0].style = 0;
            ranking[0].x = 345;
            ranking[0].y = y;
            ranking[0].visible = visible;
            ranking[0].font = 2;
            ranking[0].style = 0;
            ++lane_next[lane_idx];
        }
    }
    for (idx = 0; idx < 10; ++idx)
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[2 * idx + 14].text;

        profile_format_parts(data->times[idx], output);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[37].text;

        menu[36].text = data->lap.name;
        profile_format_parts(data->lap.time, output);
    }
    ranking_format_name(text_boat_name(data->lap.boat), text_menu[38].text);
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[41].text;

        menu[40].text = data->trial.name;
        profile_format_parts(data->trial.time, output);
    }
    ranking_format_name(text_boat_name(data->trial.boat), text_menu[42].text);
    ranking_scroll.ticks = 0;
    ranking_scroll.active = 0;
    ranking_scroll.remaining = 0;
    return 1;
}

sint32 ranking_finalize_select(void)
{
    FUNCTION_MARKER(0x8005CCE4, "MAIN.EXE");
    menu_tex.rebuild = 1;
    text_clear_boat_names();
    text_clear_names();
    if (menu_state.phase == 21)
    {
        sint16 column = menu_course_select.course;
        sint16 row = menu_course_select.level;

        return menu_inc_grp_offset(column, row);
    }
    return 21;
}

sint32 ranking_update_scr_anim(void)
{
    uint8 lane_next[9];
    sint16 active;
    sint32 result = 1;

    FUNCTION_MARKER(0x8005CD44, "MAIN.EXE");
    active = ranking_scroll.active;
    if (active != 0)
    {
        PLAYER_PROFILE *profile_data = profile_cur();
        uint8 profile_row = profile_data->level;
        sint16 selection = profile_row * 6u + menu_course_select.course;
        PROFILE_RECORD *data = &profile_recs[selection];
        sint32 idx;

        for (idx = 0; idx < 9; ++idx)
            lane_next[idx] = 10 * idx;
        for (idx = 0; idx < 10; ++idx)
        {
            sint32 half;
            sint8 clip = 0;
            uint8 visible = 0;

            for (half = 0; half < 2; ++half)
            {
                TEXT_RECORD *menu = text_menu;
                TEXT_RECORD *desc = &menu[2 * idx + half + 13];
                uint16 old_y = desc[0].y;
                uint16 delta = ranking_scroll.delta;
                sint16 y = old_y - delta;

                desc[0].y = y;
                visible = y > 74 && y < 144;
                desc[0].visible = visible;
                if (y < 83)
                    clip = 83 - y;
                else if (y > 135)
                    clip = -121 - y;
                desc[0].style = clip;
            }
            {
                TEXT_RECORD *prims = text_hud;
                TEXT_RECORD *prim = &prims[idx + 3];
                uint16 old_y = prim[0].y;
                uint16 delta = ranking_scroll.delta;
                sint16 lane_idx;
                uint8 lane;
                TEXT_RECORD *ranking;

                prim[0].visible = visible;
                prim[0].style = clip;
                prim[0].y = old_y - delta;
                lane_idx = data->boats[idx];
                lane = lane_next[lane_idx];
                ranking = &text_hud[lane + 31];
                old_y = ranking[0].y;
                delta = ranking_scroll.delta;
                ranking[0].visible = visible;
                ranking[0].style = clip;
                ranking[0].y = old_y - delta;
                ++lane_next[lane_idx];
            }
        }
        {
            uint16 remaining = ranking_scroll.remaining;
            uint16 delta = ranking_scroll.delta;
            uint16 updated = remaining - delta;

            ranking_scroll.remaining = updated;
            result = (uint32)updated << 16;
        }
        if (result == 0)
            ranking_scroll.active = 0;
        return result;
    }
    uint16 counter = ranking_scroll.ticks + 1u;
    sint16 signed_counter = counter;

    ranking_scroll.ticks = counter;
    if (signed_counter == 250)
    {
        ranking_scroll.active = 1;
        ranking_scroll.remaining = 65;
        ranking_scroll.delta = 1;
    }
    if (signed_counter == 500)
    {
        ranking_scroll.active = 1;
        ranking_scroll.remaining = -65;
        ranking_scroll.delta = -1;
        ranking_scroll.ticks = 0;
        result = -1;
    }
    return result;
}

static sint32 profile_unlock_progress(uint32 slot)
{
    PLAYER_PROFILE *profile_data = profile_at(slot);
    sint32 grp;
    sint32 row;
    sint32 column;
    uint8 maximum = profile_data->max_level;

    FUNCTION_MARKER(0x8005D128, "MAIN.EXE");
    for (grp = 0; grp < 3; ++grp)
    {
        uint8 val = profile_at(grp)->max_level;
        if (maximum < val)
            maximum = val;
    }
    profile_data->max_level = maximum;
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[row][column].state = 5;
    }
    if (maximum != 0xFF)
    {
        for (row = 0; row <= maximum; ++row)
        {
            for (column = 0; column < 6; ++column)
                profile_data->progress.courses[row][column].state = 4;
        }
    }
    return maximum + 1u;
}

void profile_select_mode(void)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x8005D354, "MAIN.EXE");
    game_selection.menu_variant = 1;
    profile_data = profile_cur();
    selection = menu_query_grp_val(4, 0);
    if (selection == 0)
    {
        result = 1;
        menu_state.series_mode = 0;
        if (game_selection.prev_mode == 1)
            menu_select_restore_backup();
        game_selection.mode = 0;
    }
    else
    {
        PROFILE_SERIES *series;
        sint32 idx;

        profile_data->level = selection - 1;
        game_selection.mode = 1;
        menu_state.series_mode = 2;
        series = profile_get_series();
        if (series->phase != 0)
        {
            race_selection.boats[0] = series->grid_row;
            profile_data->course = series->completed;
        }
        else
        {
            result = (uint8)menu_boat_select.boat;
            series->completed = 0;
            series->phase = 1;
            race_selection.boats[0] = result;
            series->grid_row = result;
            for (idx = 0; idx < 16; ++idx)
                series->points[idx] = 0;
        }
    }
}

void profile_refresh_mode_visual(void)
{
    sint32 selection;
    sint32 rec_idx = 0;
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x8005D464, "MAIN.EXE");

    selection = menu_query_grp_val(4, 0);
    menu_state.saved_choice = selection - 1;
    if (selection == 1)
        rec_idx = 6;
    else if (selection == 2)
        rec_idx = 7;
    else if (selection == 3)
        rec_idx = 8;
    recs->type = 1;
    for (idx = 6; idx < 9; ++idx)
        recs[idx].type = 0;
    if (rec_idx != 0)
    {
        UI_RECORD *rec = recs + rec_idx;
        rec->type = 1;
        if (menu_state.selection != 0)
        {
            spr_apply_prim_vram_mask(rec);
            return;
        }
        spr_disable_semitransparency(rec);
        return;
    }
    recs[10].type = 0;
    recs[31].type = 0;
    recs[32].type = 0;
}

static sint32 profile_init_slot(sint16 profile_idx)
{
    PLAYER_PROFILE *profile_data = profile_at(3);
    sint32 row;
    sint32 idx;

    FUNCTION_MARKER(0x8005D604, "MAIN.EXE");
    profile_at(3)->reset_pending = 1;
    profile_copy_templates(profile_idx);
    profile_reset_series(3);
    profile_data->course = 0;
    profile_data->level = 0;
    profile_data->max_level = 0;
    profile_data->reward_flags = 0;
    for (row = 0; row < 3; ++row)
    {
        for (idx = 0; idx < 6; ++idx)
        {
            profile_data->progress.courses[row][idx].state = 4;
            profile_data->progress.courses[row][idx].result = 0;
        }
    }
    if (game_selection.mode != 0)
    {
        for (row = 0; row < 3; ++row)
        {
            uint8 val = profile_at(row)->max_level;
            if (profile_data->max_level < val)
                profile_data->max_level = val;
        }
    }
    else
        profile_unlock_progress(3);
    return profile_merge_unlocks(profile_at(3));
}

sint32 profile_init_select(void)
{
    FUNCTION_MARKER(0x8005D74C, "MAIN.EXE");
    game_selection.menu_variant = 1;
    game_selection.players = game_selection.players & 0xFF00u | 2;
    profile_selection.slot = 0;
    if (menu_state.from_scr == 1)
    {
        game_selection.mode = 0;
        game_selection.prev_mode = 0;
        menu_update_grp_desc_select(4, 0, 0);
    }
    else if (game_selection.mode == 1)
    {
        menu_update_grp_desc_select(4, 0, menu_state.saved_choice + 1);
    }
    profile_refresh_mode_visual();
    return profile_init_slot(0);
}

sint32 ranking_refresh_peer_times(void)
{
    TEXT_RECORD *menu = text_menu;

    FUNCTION_MARKER(0x8005D9FC, "MAIN.EXE");
    name_copy_player_name_bytes(result_state.winner, menu[1].text);
    name_copy_player_name_bytes(0, menu[4].text);
    ranking_update_peer_times(boat_player_menu(&race_views[0]), menu, 6, 18, 10, 21);
    name_copy_player_name_bytes(1, menu[11].text);
    return ranking_update_peer_times(boat_player_menu(&race_views[1]), menu, 13, 22, 17, 25);
}

sint32 ranking_refresh_results(void)
{
    PLAYER_PROFILE *profile = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    sint16 count = ranking_count(result_state.count);
    sint16 mode = game_selection.selection;
    sint32 idx;

    FUNCTION_MARKER(0x8005DE08, "MAIN.EXE");
    menu_boat_select.boat = race_selection.boats[0];
    text_clear_menu_range(1, 12);
    menu[2 * profile->course + -1].visible = 1;
    menu[2 * profile->course].visible = 1;
    if (count == 2)
    {
        menu[15].visible = 0;
        menu[16].visible = 0;
        text_clear_menu_range(17, 4);
    }
    if (mode == -1 || mode == -3)
    {
        if (count == 4)
        {
            if (mode == -1)
            {
                race_selection.ranks[0] = 3;
                race_selection.ranks[1] = 2;
                race_selection.ranks[2] = 1;
                race_selection.ranks[3] = 0;
                if (result_state.quit_player != 0)
                {
                    race_selection.ranks[0] = 2;
                    race_selection.ranks[1] = 3;
                    series->points[0] = series->points[0] + ranking_points[2];
                }
                else
                    series->points[1] = series->points[1] + ranking_points[2];
                series->points[2] = series->points[2] + ranking_points[1];
                series->points[3] = series->points[3] + ranking_points[0];
            }
            else
            {
                uint8 limit = race_selection.ranks[0] < race_selection.ranks[1] ? race_selection.ranks[0] : race_selection.ranks[1];
                for (idx = 2; idx < 4; ++idx)
                {
                    uint8 rank = race_selection.ranks[idx];
                    if (rank < limit)
                        series->points[idx] = series->points[idx] + ranking_place_points(rank);
                }
            }
        }
        else
        {
            race_selection.ranks[0] = 1;
            race_selection.ranks[1] = 0;
            if (mode == -1)
            {
                if (result_state.quit_player != 0)
                {
                    race_selection.ranks[0] = 0;
                    race_selection.ranks[1] = 1;
                    series->points[0] = series->points[0] + ranking_points[0];
                }
                else
                    series->points[1] = series->points[1] + ranking_points[0];
            }
        }
    }
    else
    {
        for (idx = 0; idx < count; ++idx)
        {
            uint8 rank = race_selection.ranks[idx];
            series->points[idx] = series->points[idx] + ranking_place_points(rank);
        }
    }
    ranking_build_order_desc();
    for (idx = 0; idx < count; ++idx)
    {
        uint8 rank = race_selection.ranks[idx];
        const RACE_PARTICIPANT *participant = &race_participants[idx];
        if (rank < count)
        {
            uint16 type = participant->mode;
            const char *src = type == 2 ? name_player_text(0) : type == 3 ? name_player_text(1) : text_boat_name(participant->boat);
            ranking_format_name(src, menu[idx + 17].text);
            menu[idx + 17].visible = 1;
            menu[idx + 17].y = 12u * rank + 96u;
        }
    }
    return count << 16;
}

sint32 ranking_leave_series_results(sint16 val)
{
    FUNCTION_MARKER(0x8005E320, "MAIN.EXE");

    return val;
}

sint32 results_enter_best_times_store(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    FUNCTION_MARKER(0x80063A18, "MAIN.EXE");
    profile_update_best_times(unused1, unused2, unused3, unused4);
    game_selection.event = 10;
    game_selection.event_arg = 0;
    return 10;
}
