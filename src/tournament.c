#include "menu_select.h"
#include "hud.h"
#include "render.h"
#include "game.h"
#include "input.h"
#include "tournament.h"
#include "disp.h"
#include "utils.h"
#include "menu.h"
#include "name.h"
#include "profile.h"
#include "ranking.h"
#include "race.h"
#include "text.h"
#include "xport_trace.h"

// Course target parts from MAIN.EXE 800996A0
static const uint16 tournament_course_times[6][2] = {
    {1, 30}, {1, 40}, {1, 40}, {2, 20}, {1, 40}, {2, 20},
};

// Round adjustments from MAIN.EXE 800996D0
static const uint16 tournament_time_adjusts[21] = {0, 5, 8, 11, 13, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

// Match chain and scores from MAIN.EXE 800E1A6C
// Runtime leader and match count from 800B6A96 and 800B6B46
TOURNAMENT_MATCH tournament_matches;

// Grid from MAIN.EXE 800E1916, extended for the byte round counter
TOURNAMENT_GRID tournament_grid;

// Tournament target parts and parameter from MAIN.EXE 800E1A44
TOURNAMENT_TIME tournament_times[5];

// Championship state from MAIN.EXE 800E1A8A
TOURNAMENT_CHAMP tournament_champ;

typedef struct
{
    sint16 ids[16];
    uint16 offset;
} TOURNAMENT_TABLE;

static TOURNAMENT_TABLE tournament_table;

typedef struct
{
    uint8 player;
    uint8 lower_total;
} TOURNAMENT_RESULT;

static TOURNAMENT_RESULT tournament_result;
static const char tournament_pending_name[5] = "....";

// Extra native rounds have no representation in the legacy save file
void tournament_clear_extra_rounds(void)
{
    sint32 round;
    sint32 player;

    for (round = 20; round < TOURNAMENT_GRID_ROUNDS; ++round)
    {
        for (player = 0; player < TOURNAMENT_GRID_PLAYERS; ++player)
        {
            tournament_grid.status[round][player] = TOURNAMENT_GRID_EMPTY;
            tournament_grid.seconds[round][player] = 0;
        }
    }
}

static sint32 tournament_draw_grid(void)
{
    sint16 rows = (sint8)game_selection.players;
    sint16 top = 13 * (5 - rows) + 62;
    sint16 sel_row = top + 27 * tournament_grid.player;
    sint16 sel_column = tournament_grid.round >= 3u ? 2 : tournament_grid.round;
    sint32 idx;

    FUNCTION_MARKER(0x80064814, "MAIN.EXE");
    disp_set_line_color(menu_pulse.brightness, 64, 64);
    disp_queue_rect_outline(90 * sel_column + 180, sel_row, 90 * sel_column + 270, sel_row + 27);
    disp_queue_rect_outline(60, sel_row, 143, sel_row + 23);
    disp_set_line_color(192, 192, 192);
    for (idx = 0; idx < 4; ++idx)
        disp_queue_line_seg(90 * idx + 180, top, 90 * idx + 180, top + 27 * rows);
    if (rows != -1)
    {
        for (idx = 0; idx < rows + 1; ++idx)
            disp_queue_line_seg(180, top + 27 * idx, 450, top + 27 * idx);
    }
    disp_set_line_color(32, 32, 32);
    for (idx = 0; idx < 4; ++idx)
        disp_queue_line_seg(90 * idx + 181, top + 1, 90 * idx + 181, top + 1 + 27 * rows);
    if (rows != -1)
    {
        for (idx = 0; idx < rows + 1; ++idx)
            disp_queue_line_seg(181, top + 1 + 27 * idx, 451, top + 1 + 27 * idx);
    }
    return top;
}

sint32 tournament_reset_grid(void)
{
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80064B24, "MAIN.EXE");
    game_selection.menu_variant = 0;
    tournament_grid.round = 0;
    tournament_grid.player = 0;
    for (row = 0; row < TOURNAMENT_GRID_ROUNDS; ++row)
    {
        for (column = 0; column < TOURNAMENT_GRID_PLAYERS; ++column)
        {
            tournament_grid.status[row][column] = 0;
            tournament_grid.seconds[row][column] = 0;
        }
    }
    return 0;
}

static sint32 tournament_target_time(uint16 course, sint16 idx)
{
    sint16 table_idx = idx;
    sint16 excess = 0;

    FUNCTION_MARKER(0x80064BC4, "MAIN.EXE");
    if (idx >= 21)
    {
        excess = idx - 20;
        table_idx = 20;
    }
    return (sint16)(60 * (sint16)tournament_course_times[course][0] + tournament_course_times[course][1] - ((sint16)tournament_time_adjusts[table_idx] - excess));
}

sint32 tournament_refresh_grid(void)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    sint16 top;
    sint16 column_offset;
    sint16 rec_idx = 0;
    sint32 row;
    sint32 column;
    sint32 result = 0;

    FUNCTION_MARKER(0x80064C24, "MAIN.EXE");
    menu_pulse.brightness = 128;
    menu_pulse.falling = 0;
    top = tournament_draw_grid();
    for (row = 0; row < (uint8)game_selection.players; ++row)
        menu[row + 2].y = top + 27 * row + 5;
    for (row = 0; row < 3; ++row)
        menu[row + 7].y = top - 15;
    for (row = 0; row < 5; ++row)
        menu[row + 2].visible = 0;
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        name_copy_player_name_bytes(row, menu[row + 2].text);
        menu[row + 2].visible = 1;
    }
    column_offset = tournament_grid.round < 3u ? 0 : tournament_grid.round - 2;
    for (row = 0; row < 15; ++row)
    {
        recs[row + 3].type = 0;
        recs[row + 18].type = 0;
        menu[row + 10].visible = 0;
    }
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        sint16 y = top + 27 * row + 3;
        for (column = 0; column < 3; ++column, ++rec_idx)
        {
            sint32 src_column = column_offset + column;
            if (tournament_grid.status[src_column][row] == 1)
            {
                UI_RECORD *rec = recs + rec_idx + 3;
                TEXT_RECORD *text = &menu[rec_idx + 10];
                uint16 val = tournament_grid.seconds[src_column][row];
                char *output = text[0].text;
                sint32 remainder = val % 60u;
                rec->type = 1;
                rec->x = 90 * column + 191;
                rec->y = y;
                text[0].visible = 1;
                text[0].x = rec->x + 18u;
                text[0].y = rec->y + 11u;
                output[0] = val / 60u + 48u;
                output[2u] = remainder / 10 + 48;
                output[3u] = remainder % 10 + 48;
            }
            else if (src_column < tournament_grid.round || src_column == tournament_grid.round && row < tournament_grid.player)
            {
                UI_RECORD *rec = recs + rec_idx + 18;
                rec->type = 1;
                rec->x = 90 * column + 200;
                rec->y = y;
            }
        }
    }
    {
        for (column = 0; column < 3; ++column)
        {
            sint16 val = tournament_target_time(menu_course_select.course, column_offset + column);
            sint32 remainder = val % 60;
            char *output = menu[column + 7].text;
            output[0] = val / 60 + 48;
            output[2u] = remainder / 10 + 48;
            output[3u] = remainder % 10 + 48;
            result = 10 * (remainder / 10);
        }
    }
    return result;
}

static sint32 tournament_enter_race(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 mode = game_selection.rules;
    TEXT_RECORD *rec;

    FUNCTION_MARKER(0x800652B4, "MAIN.EXE");
    menu_save_desc_payloads(menu_state.scr);
    profile_data = profile_cur();
    profile_data->level_result = 0;
    profile_data->course = menu_course_select.course;
    if (mode == 0)
    {
        game_selection.menu_variant = 0;
        game_selection.event = 30;
        name_copy_player_name_bytes(tournament_champ.player, hud_text.names[0]);
        profile_data->course = tournament_champ.course;
        profile_data->level = menu_course_select.level;
    }
    else if (mode == 1)
    {
        sint16 val;
        game_selection.event = 30;
        name_copy_player_name_bytes(tournament_grid.player, hud_text.names[0]);
        val = tournament_target_time(menu_course_select.course, tournament_grid.round);
        tournament_times[0].parts[0] = val / 60;
        tournament_times[0].parts[1] = val % 60;
    }
    else if (mode == 2)
    {
        sint32 row = tournament_matches.match;
        game_selection.event = 31;
        name_copy_player_name_bytes(tournament_matches.winners[row], hud_text.names[0]);
        name_copy_player_name_bytes(tournament_matches.opponents[row], hud_text.names[1]);
    }
    game_selection.event_arg = 0;
    menu_state.exit_wait = 5;
    menu_state.sound = 1;
    text_set_hud_visible(0, 1);
    menu_state.phase = 21;
    menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
    menu_tex.rebuild = 1;
    rec = text_hud;
    rec[0].x = 332;
    rec[0].y = 145;
    return menu_build_player_mode_table();
}

sint32 tournament_update_grid_input(PAD *input)
{
    uint8 val;

    FUNCTION_MARKER(0x80065198, "MAIN.EXE");
    if ((input->cur & 0x40u) != 0 && menu_state.input_enabled != 0)
        tournament_enter_race();
    tournament_draw_grid();
    if (menu_pulse.falling != 0)
    {
        val = menu_pulse.brightness - 4u;
        menu_pulse.brightness = val;
        if (val < 0x80u)
            menu_pulse.falling = 0;
        return val < 0x80u;
    }
    val = menu_pulse.brightness + 4u;
    menu_pulse.brightness = val;
    if (val >= 0xF1u)
        menu_pulse.falling = 1;
    return 1;
}

sint32 tournament_sync_grid(void)
{
    FUNCTION_MARKER(0x80065274, "MAIN.EXE");
    if (menu_state.phase == 21)
        return menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
    return 21;
}

static sint32 tournament_grid_complete(void)
{
    sint16 counts[8] = {0};
    sint16 totals[8] = {0};
    sint16 best_count = -1;
    sint16 best_total = 1000;
    sint8 sel = -1;
    sint32 tied = 0;
    sint32 tied_total = 0;
    sint32 lower_total = 0;
    sint32 row;

    FUNCTION_MARKER(0x800654F4, "MAIN.EXE");
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        sint32 column = 0;
        if (tournament_grid.status[0][row] == 1)
        {
            while (column < TOURNAMENT_GRID_ROUNDS && tournament_grid.status[column][row] == 1)
            {
                ++counts[row];
                totals[row] = totals[row] + tournament_grid.seconds[column][row];
                ++column;
            }
        }
    }
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        if (best_count < counts[row])
        {
            best_count = counts[row];
            best_total = totals[row];
            sel = row;
            tied = 0;
            lower_total = 0;
        }
        else if (counts[row] == best_count && best_total >= totals[row])
        {
            lower_total = 1;
            if (totals[row] >= best_total)
            {
                lower_total = 0;
                ++tied;
                tied_total += totals[row];
            }
            else
            {
                best_count = counts[row];
                best_total = totals[row];
                sel = row;
                tied = 0;
            }
        }
    }
    game_selection.event = 49;
    if (lower_total != 0)
    {
        tournament_result.player = sel;
        tournament_result.lower_total = 1;
    }
    else
    {
        if (tied != 0)
        {
            if (tied_total == 0)
                game_selection.event = 1;
        }
        else
            tournament_result.player = sel;
        tournament_result.lower_total = 0;
    }
    game_selection.event_arg = 2;
    return 2;
}

sint32 tournament_refresh_player_name(void)
{
    FUNCTION_MARKER(0x8006571C, "MAIN.EXE");
    return name_copy_player_name_bytes(tournament_result.player, text_menu[2].text);
}

sint32 tournament_rec_result(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 state = game_selection.selection;
    sint16 column = tournament_grid.round;
    uint8 completed = 0;

    FUNCTION_MARKER(0x8006574C, "MAIN.EXE");
    game_selection.event = 30;
    if (state == -4 || state == -2)
        tournament_grid.status[column][tournament_grid.player] = TOURNAMENT_GRID_PASSED;
    else if (state == -3 || state == -1)
        tournament_grid.status[column][tournament_grid.player] = TOURNAMENT_GRID_FAILED;
    profile_store_best_lap(tournament_grid.player, unused2, unused3, unused4);
    tournament_grid.seconds[column][tournament_grid.player] = 60u * race_lap_times[0][0] + race_lap_times[0][1];
    for (;;)
    {
        sint32 first_open = 0;
        tournament_grid.player++;
        if (tournament_grid.player == (uint8)game_selection.players)
        {
            tournament_grid.player = 0;
            tournament_grid.round++;
        }
        while (first_open < TOURNAMENT_GRID_ROUNDS && tournament_grid.status[first_open][tournament_grid.player] == TOURNAMENT_GRID_PASSED)
            ++first_open;
        if (first_open == TOURNAMENT_GRID_ROUNDS || tournament_grid.status[first_open][tournament_grid.player] == TOURNAMENT_GRID_FAILED)
            ++completed;
        else
            break;
        if (completed >= (uint8)game_selection.players)
            return tournament_grid_complete();
    }
    return completed;
}

static sint32 tournament_init_match_order(void)
{
    sint16 val = tournament_matches.leader + 1;
    sint32 idx;

    FUNCTION_MARKER(0x80065960, "MAIN.EXE");
    tournament_matches.match = 0;
    tournament_matches.winners[0] = tournament_matches.leader;
    if (val == (uint8)game_selection.players)
        val = 0;
    for (idx = 0; idx < 4; ++idx)
    {
        tournament_matches.opponents[idx] = val;
        ++val;
        if (val == (uint8)game_selection.players)
            val = 0;
    }
    return 0;
}

sint32 tournament_init_round(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x800659F0, "MAIN.EXE");
    game_selection.menu_variant = 1;
    tournament_matches.round = 0;
    for (idx = 0; idx < 5; ++idx)
        tournament_matches.points[idx] = 0;
    tournament_matches.leader = 0;
    tournament_matches.match_count = (uint8)game_selection.players - 1u;
    return tournament_init_match_order();
}

static sint32 tournament_draw_match_highlight(void)
{
    sint16 row = tournament_matches.match;

    FUNCTION_MARKER(0x80065A6C, "MAIN.EXE");
    if (row < (sint16)tournament_matches.match_count)
    {
        sint16 top = 30 * row + 74;
        disp_set_line_color(menu_pulse.brightness, 64, 64);
        return disp_queue_rect_outline(80, top, 430, top + 21);
    }
    return 0;
}

sint32 tournament_refresh_matches(void)
{
    TEXT_RECORD *menu = text_menu;
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x80065AD8, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
    {
        sint32 visible = idx < (sint16)tournament_matches.match_count;
        sint16 y = 30 * idx + 80;
        char *output = menu[idx + 8].text;
        sint32 character;
        menu[idx + 4].visible = visible;
        menu[idx + 8].visible = visible;
        menu[idx + 12].visible = visible;
        menu[idx + 4].y = y;
        menu[idx + 8].y = y;
        menu[idx + 12].y = y;
        if (tournament_matches.match < idx)
        {
            for (character = 0; character < 5; ++character)
                output[character] = tournament_pending_name[character];
        }
        else
            name_copy_player_name_bytes(tournament_matches.winners[idx], output);
        name_copy_player_name_bytes(tournament_matches.opponents[idx], menu[idx + 12].text);
        recs[idx + 2].type = idx < tournament_matches.match;
        recs[idx + 6].type = idx < tournament_matches.match;
        recs[idx + 2].y = y - 8;
        recs[idx + 6].y = y - 8;
        if (idx < tournament_matches.match)
        {
            sint32 same = tournament_matches.winners[idx + 1] == tournament_matches.winners[idx];
            recs[idx + 2].x = same ? 105u : 350u;
            recs[idx + 6].x = same ? 350u : 105u;
        }
    }
    for (idx = 0; idx < 10; ++idx)
        menu[idx + 16].visible = idx == tournament_matches.round;
    menu[3].visible = tournament_matches.match < (sint16)tournament_matches.match_count;
    menu[26].visible = menu[3].visible == 0;
    return menu[26].visible;
}

sint32 tournament_update_match_input(PAD *input)
{
    uint8 val;

    FUNCTION_MARKER(0x80065D4C, "MAIN.EXE");
    if ((input->cur & 0x40u) != 0 && menu_state.input_enabled != 0)
    {
        if (tournament_matches.match >= (uint8)tournament_matches.match_count)
            menu_state.phase = 32;
        else
            tournament_enter_race();
    }
    tournament_draw_match_highlight();
    if (menu_pulse.falling != 0)
    {
        val = menu_pulse.brightness - 4u;
        menu_pulse.brightness = val;
        if (val < 0x80u)
            menu_pulse.falling = 0;
        return val < 0x80u;
    }
    val = menu_pulse.brightness + 4u;
    menu_pulse.brightness = val;
    if (val >= 0xF1u)
        menu_pulse.falling = 1;
    return 1;
}

sint32 tournament_sync_match(void)
{
    FUNCTION_MARKER(0x80065E54, "MAIN.EXE");
    if (menu_state.phase == 21)
        return menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
    return 21;
}

sint32 tournament_rec_match_result(void)
{
    sint16 state = game_selection.selection;
    sint32 row = tournament_matches.match;

    FUNCTION_MARKER(0x80065E94, "MAIN.EXE");
    game_selection.event = 31;
    if (state == -3)
        return state < -2;
    if (state == -1)
    {
        if (result_state.quit_player != 0)
        {
            race_selection.ranks[0] = 0;
            race_selection.ranks[1] = 1;
        }
        else
            race_selection.ranks[0] = 1;
    }
    {
        uint8 val = race_selection.ranks[0] >= race_selection.ranks[1] ? tournament_matches.opponents[row] : tournament_matches.winners[row];
        tournament_matches.points[val] = tournament_matches.points[val] + 1u;
        if (row + 1 >= (sint16)tournament_matches.match_count)
            tournament_matches.leader = val;
        tournament_matches.winners[row + 1] = val;
    }
    tournament_matches.match = row + 1;
    return row + 1;
}

sint32 tournament_refresh_standings(void)
{
    sint16 order[8];
    sint16 counts[8];
    TEXT_RECORD *menu = text_menu;
    sint32 i;
    sint32 j;

    FUNCTION_MARKER(0x80065F7C, "MAIN.EXE");
    for (i = 0; i < 5; ++i)
    {
        order[i] = i;
        counts[i] = tournament_matches.points[i];
        menu[i + 2].visible = 0;
        menu[i + 9].visible = 0;
    }
    for (i = 0; i < (uint8)game_selection.players; ++i)
    {
        for (j = 0; j < (uint8)game_selection.players; ++j)
        {
            if (i != j && counts[j] < counts[i])
            {
                sint16 val = counts[i];
                sint16 idx = order[i];
                counts[i] = counts[j];
                order[i] = order[j];
                counts[j] = val;
                order[j] = idx;
            }
        }
    }
    for (i = 0; i < (uint8)game_selection.players; ++i)
    {
        char *output;
        menu[i + 2].visible = 1;
        menu[i + 9].visible = 1;
        name_copy_player_name_bytes(order[i], menu[i + 2].text);
        output = menu[i + 9].text;
        output[0] = counts[i] / 10 + 48;
        output[1u] = counts[i] % 10 + 48;
    }
    {
        char *output = menu[7].text;
        uint8 val = tournament_matches.round + 1u;
        output[0] = val / 10u + 48u;
        output[1u] = val % 10u + 48u;
        if (tournament_matches.round == 0)
            output[8u] = 32;
    }
    return 32;
}

sint32 tournament_advance_round(PAD *input)
{
    sint32 result = input->cur & 0x40u;

    FUNCTION_MARKER(0x800662A4, "MAIN.EXE");
    if (result != 0 && menu_state.input_enabled != 0)
    {
        tournament_matches.round++;
        result = 33;
        if ((sint16)game_options.round_limit < tournament_matches.round)
            menu_state.phase = 33;
        else
        {
            menu_state.phase = 31;
            return tournament_init_round();
        }
    }
    return result;
}

sint32 tournament_refresh_leader(void)
{
    sint16 order[8];
    sint16 counts[8];
    sint32 i;
    sint32 j;
    TEXT_RECORD *menu = text_menu;

    FUNCTION_MARKER(0x80066338, "MAIN.EXE");
    for (i = 0; i < 5; ++i)
    {
        order[i] = i;
        counts[i] = tournament_matches.points[i];
    }
    for (i = 0; i < (uint8)game_selection.players; ++i)
    {
        for (j = 0; j < (uint8)game_selection.players; ++j)
        {
            if (i != j && counts[j] < counts[i])
            {
                sint16 val = counts[i];
                sint16 idx = order[i];
                counts[i] = counts[j];
                order[i] = order[j];
                counts[j] = val;
                order[j] = idx;
            }
        }
    }
    menu[0].visible = 1;
    return name_copy_player_name_bytes(order[0], menu[0].text);
}

static sint32 tournament_init_profile(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint16 row = (uint8)game_options.level;
    sint32 idx;

    FUNCTION_MARKER(0x80066504, "MAIN.EXE");
    profile_data->level = row;
    menu_course_select.level = row;
    tournament_champ.course = 0;
    tournament_champ.player = 0;
    for (idx = 0; idx < 16; ++idx)
    {
        tournament_champ.races[idx] = 0;
        tournament_champ.points[idx] = 0;
    }
    for (idx = 0; idx < 6; ++idx)
        profile_data->progress.courses[row][idx].state = 5;
    profile_data->progress.courses[row][0u].state = 4;
    return 4;
}

sint32 tournament_reset_result_rows(void)
{
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x8006649C, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
        recs[idx + 2].type = 0;
    return tournament_init_profile();
}

static sint32 tournament_refresh_table(void)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    sint32 rows = 0;
    sint32 idx;
    sint16 top;

    FUNCTION_MARKER(0x80066600, "MAIN.EXE");

    for (idx = 0; idx < 3; ++idx)
        recs[idx + 1].type = idx == menu_course_select.level;
    if (tournament_champ.course < 6u)
    {
        recs[5].type = 1;
        recs[6].type = 1;
        recs[7].type = 0;
        menu[54].visible = 1;
        menu[55].visible = 1;
        menu[56].visible = 0;
    }
    else
    {
        recs[5].type = 0;
        recs[6].type = 0;
        recs[7].type = 1;
        menu[54].visible = 0;
        menu[55].visible = 0;
        menu[56].visible = 1;
    }
    for (idx = 0; idx < 16; ++idx)
    {
        menu[idx + 6].visible = 0;
        menu[idx + 22].visible = 0;
        menu[idx + 38].visible = 0;
    }
    for (idx = 0; idx < 3; ++idx)
        menu[idx].visible = 0;
    if (menu_state.series_mode == 1)
    {
        menu[0].visible = 1;
        rows = 8;
    }
    else if (menu_state.series_mode == 2)
    {
        menu[1].visible = 1;
        rows = 8;
    }
    else if (menu_state.series_mode == 3)
    {
        menu[2].visible = 1;
        rows = (uint8)game_selection.players;
    }
    top = 7 * (8 - rows) + 70;
    for (idx = 0; idx < rows; ++idx)
    {
        sint16 src_idx = tournament_table.offset + idx;
        sint16 y = top + 14 * idx;
        TEXT_RECORD *left = &menu[src_idx + 6];
        TEXT_RECORD *right = &menu[src_idx + 38];
        TEXT_RECORD *val_text = &menu[src_idx + 22];
        sint16 val = tournament_table.ids[src_idx];
        left[0].visible = 1;
        left[0].x = 110;
        left[0].y = y;
        right[0].visible = 1;
        right[0].x = 330;
        right[0].y = y;
        val_text[0].visible = 1;
        val_text[0].x = 390;
        val_text[0].y = y;
        if (val >= 0)
        {
            sint16 multiple = val - 9 * (val % 9);
            sint16 prim_idx = (val % 9 == 0 ? 13 : 22) + multiple;
            TEXT_RECORD *prim = &text_hud[prim_idx];
            prim[0].visible = 1;
            prim[0].x = 170;
            prim[0].y = y;
        }
        else
        {
            sint16 peer = ~val;
            TEXT_RECORD *prim = &text_hud[peer + 3];
            char *left_output = right[0].text;
            char *right_output = val_text[0].text;
            prim[0].visible = 1;
            prim[0].x = 170;
            prim[0].y = y;
            prim[0].text = name_player_text(peer);
            left_output[0] = tournament_champ.races[peer] / 10u + 48u;
            left_output[1u] = tournament_champ.races[peer] % 10u + 48u;
            right_output[0] = tournament_champ.points[peer] / 10u + 48u;
            right_output[1u] = tournament_champ.points[peer] % 10u + 48u;
        }
    }
    menu[3].visible = 1;
    menu[3].x = 110;
    menu[3].y = top - 25;
    menu[4].visible = 1;
    menu[4].x = 330;
    menu[4].y = top - 25;
    menu[5].visible = 1;
    menu[5].x = 390;
    menu[5].y = top - 25;
    return 390;
}

sint32 tournament_init_table(void)
{
    sint16 order[8];
    sint16 identifiers[8];
    sint32 i;
    sint32 j;
    TEXT_RECORD *prims = text_hud;

    FUNCTION_MARKER(0x80066B50, "MAIN.EXE");
    tournament_table.offset = 0;
    for (i = 0; i < (uint8)game_selection.players; ++i)
    {
        TEXT_RECORD *prim = &prims[i];
        prim[3].font = 1;
        prim[3].x = 190;
        prim[3].text = name_player_text(i);
        identifiers[i] = ~i;
        order[i] = i;
    }
    for (i = 0; i < 5; ++i)
    {
        for (j = 0; j < (uint8)game_selection.players; ++j)
        {
            sint32 other;
            for (other = 0; other < (uint8)game_selection.players; ++other)
            {
                if (tournament_champ.points[order[other]] < tournament_champ.points[order[j]])
                {
                    sint16 val = identifiers[j];
                    sint16 idx = order[j];
                    identifiers[j] = identifiers[other];
                    order[j] = order[other];
                    identifiers[other] = val;
                    order[other] = idx;
                }
            }
        }
    }
    for (i = 0; i < (uint8)game_selection.players; ++i)
        tournament_table.ids[i] = identifiers[i];
    return tournament_refresh_table();
}

sint32 tournament_update_mode_input(PAD *input)
{
    sint16 mode = menu_state.series_mode;

    FUNCTION_MARKER(0x80066D58, "MAIN.EXE");
    if (menu_state.input_enabled != 0 && mode == 3)
    {
        if (tournament_champ.course < 6u)
        {
            if ((input->cur & 0x40u) != 0)
            {
                menu_state.sound = 1;
                tournament_enter_race();
            }
            if ((input->cur & 0x10u) != 0)
            {
                menu_state.sound = 2;
                menu_state.phase = 1;
            }
        }
        else if ((input->cur & 0x40u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 1;
        }
    }
    return tournament_refresh_table();
}

sint32 tournament_reset_text(void)
{
    FUNCTION_MARKER(0x80066E3C, "MAIN.EXE");
    return text_clear_hud();
}

sint32 tournament_rec_champ_result(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint16 state = game_selection.selection;
    sint16 src_idx = 0;
    uint8 peer = tournament_champ.player;

    FUNCTION_MARKER(0x80066E5C, "MAIN.EXE");
    if (state == -3 || state == -1)
        src_idx = 15;
    else if (state == -4 || state == -2)
        src_idx = race_selection.ranks[0];
    tournament_champ.races[peer] = tournament_champ.races[peer] + 1u;
    tournament_champ.points[peer] = tournament_champ.points[peer] + ranking_place_points(src_idx);
    tournament_champ.player = peer + 1u;
    if (tournament_champ.player == (uint8)game_selection.players)
    {
        tournament_champ.player = 0;
        profile_data->progress.courses[menu_course_select.level][tournament_champ.course].state = 6;
        tournament_champ.course++;
        menu_course_select.course = tournament_champ.course;
        profile_data->course = tournament_champ.course;
        if (profile_data->course < 6u)
            profile_data->progress.courses[menu_course_select.level][tournament_champ.course].state = 4;
        else
        {
            profile_data->course = 5;
            game_selection.event = 1;
            menu_course_select.course = 5;
        }
    }
    game_selection.event = 38;
    return 38;
}
