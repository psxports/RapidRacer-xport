#include "render.h"
#include "game.h"
#include "input.h"
#include "tournament.h"
#include "display.h"
#include "global.h"
#include "menu.h"
#include "name.h"
#include "profile.h"
#include "ranking.h"
#include "results.h"
#include "text.h"
#include "xport_trace.h"

// Course target parts from MAIN.EXE 800996A0
static const uint16 tournament_course_times[6][2] = {
    {1u, 30u}, {1u, 40u}, {1u, 40u}, {2u, 20u}, {1u, 40u}, {2u, 20u},
};

// Round adjustments from MAIN.EXE 800996D0
static const uint16 tournament_time_adjustments[21] = {0u, 5u, 8u, 11u, 13u, 15u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u};

// Match chain and scores from MAIN.EXE 800E1A6C
// Runtime leader and match count from 800B6A96 and 800B6B46
TOURNAMENT_MATCH_STATE tournament_matches;

// Grid from MAIN.EXE 800E1916, extended for the byte round counter
TOURNAMENT_GRID_STATE tournament_grid;

// Tournament target parts and parameter from MAIN.EXE 800E1A44
TOURNAMENT_TIME tournament_times[5];

// Championship state from MAIN.EXE 800E1A8A
TOURNAMENT_CHAMP_STATE tournament_champ;

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
            tournament_grid.seconds[round][player] = 0u;
        }
    }
}

sint32 tournament_draw_grid(void)
{
    sint16 rows = (sint8)(uint8)game_selection.players;
    sint16 top = (sint16)(13 * (5 - rows) + 62);
    sint16 selected_row = (sint16)(top + 27 * tournament_grid.player);
    sint16 selected_column = tournament_grid.round >= 3u ? 2 : tournament_grid.round;
    sint32 index;

    FUNCTION_MARKER(0x80064814u, "MAIN.EXE");
    display_set_line_color((sint8)menu_pulse.brightness, 64, 64);
    display_queue_rect_outline((sint16)(90 * selected_column + 180), selected_row, (sint16)(90 * selected_column + 270), (sint16)(selected_row + 27));
    display_queue_rect_outline(60, selected_row, 143, (sint16)(selected_row + 23));
    display_set_line_color((sint8)192, (sint8)192, (sint8)192);
    for (index = 0; index < 4; ++index)
        display_queue_line_segment((sint16)(90 * index + 180), top, (sint16)(90 * index + 180), (sint16)(top + 27 * rows));
    if (rows != -1)
    {
        for (index = 0; index < rows + 1; ++index)
            display_queue_line_segment(180, (sint16)(top + 27 * index), 450, (sint16)(top + 27 * index));
    }
    display_set_line_color(32, 32, 32);
    for (index = 0; index < 4; ++index)
        display_queue_line_segment((sint16)(90 * index + 181), (sint16)(top + 1), (sint16)(90 * index + 181), (sint16)(top + 1 + 27 * rows));
    if (rows != -1)
    {
        for (index = 0; index < rows + 1; ++index)
            display_queue_line_segment(181, (sint16)(top + 1 + 27 * index), 451, (sint16)(top + 1 + 27 * index));
    }
    return top;
}

sint32 tournament_reset_grid(void)
{
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80064B24u, "MAIN.EXE");
    game_selection.menu_variant = 0u;
    tournament_grid.round = 0u;
    tournament_grid.player = 0u;
    for (row = 0; row < TOURNAMENT_GRID_ROUNDS; ++row)
    {
        for (column = 0; column < TOURNAMENT_GRID_PLAYERS; ++column)
        {
            tournament_grid.status[row][column] = 0u;
            tournament_grid.seconds[row][column] = 0u;
        }
    }
    return 0;
}

sint32 tournament_target_time(uint16 course, sint16 index)
{
    sint16 table_index = index;
    sint16 excess = 0;

    FUNCTION_MARKER(0x80064BC4u, "MAIN.EXE");
    if (index >= 21)
    {
        excess = (sint16)(index - 20);
        table_index = 20;
    }
    return (sint16)(60 * (sint16)tournament_course_times[course][0] + tournament_course_times[course][1] - ((sint16)tournament_time_adjustments[table_index] - excess));
}

sint32 tournament_refresh_grid(void)
{
    UI_RECORD *records = sprite_records;
    TEXT_RECORD *menu = text_menu;
    sint16 top;
    sint16 column_offset;
    sint16 record_index = 0;
    sint32 row;
    sint32 column;
    sint32 result = 0;

    FUNCTION_MARKER(0x80064C24u, "MAIN.EXE");
    menu_pulse.brightness = 128u;
    menu_pulse.falling = 0u;
    top = (sint16)tournament_draw_grid();
    for (row = 0; row < (uint8)game_selection.players; ++row)
        menu[row + 2].y = (uint16)(top + 27 * row + 5);
    for (row = 0; row < 3; ++row)
        menu[row + 7].y = (uint16)(top - 15);
    for (row = 0; row < 5; ++row)
        menu[row + 2].visible = 0u;
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        name_copy_player_name_bytes((uint16)row, menu[row + 2].text);
        menu[row + 2].visible = 1u;
    }
    column_offset = tournament_grid.round < 3u ? 0 : (sint16)tournament_grid.round - 2;
    for (row = 0; row < 15; ++row)
    {
        records[row + 3].type = 0u;
        records[row + 18].type = 0u;
        menu[row + 10].visible = 0u;
    }
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        sint16 y = (sint16)(top + 27 * row + 3);
        for (column = 0; column < 3; ++column, ++record_index)
        {
            sint32 source_column = column_offset + column;
            if (tournament_grid.status[source_column][row] == 1u)
            {
                UI_RECORD *record = (records + record_index + 3);
                TEXT_RECORD *text = &menu[record_index + 10];
                uint16 value = tournament_grid.seconds[source_column][row];
                char *output = text[0].text;
                sint32 remainder = value % 60u;
                record->type = 1u;
                record->x = (uint16)(90 * column + 191);
                record->y = (uint16)y;
                text[0].visible = 1u;
                text[0].x = (uint16)(record->x + 18u);
                text[0].y = (uint16)(record->y + 11u);
                output[0] = (uint8)(value / 60u + 48u);
                output[2u] = (uint8)(remainder / 10 + 48);
                output[3u] = (uint8)(remainder % 10 + 48);
            }
            else if (source_column < tournament_grid.round || (source_column == tournament_grid.round && row < tournament_grid.player))
            {
                UI_RECORD *record = (records + record_index + 18);
                record->type = 1u;
                record->x = (uint16)(90 * column + 200);
                record->y = (uint16)y;
            }
        }
    }
    {
        for (column = 0; column < 3; ++column)
        {
            sint16 value = (sint16)tournament_target_time(menu_course_select.course, (sint16)(column_offset + column));
            sint32 remainder = value % 60;
            char *output = menu[column + 7].text;
            output[0] = (uint8)(value / 60 + 48);
            output[2u] = (uint8)(remainder / 10 + 48);
            output[3u] = (uint8)(remainder % 10 + 48);
            result = 10 * (remainder / 10);
        }
    }
    return result;
}

sint32 tournament_update_grid_input(CONTROLLER_STATE *input)
{
    uint8 value;

    FUNCTION_MARKER(0x80065198u, "MAIN.EXE");
    if ((input->current & 0x40u) != 0u && menu_state.input_enabled != 0u)
        tournament_enter_race();
    tournament_draw_grid();
    if (menu_pulse.falling != 0u)
    {
        value = (uint8)(menu_pulse.brightness - 4u);
        menu_pulse.brightness = value;
        if (value < 0x80u)
            menu_pulse.falling = 0u;
        return value < 0x80u;
    }
    value = (uint8)(menu_pulse.brightness + 4u);
    menu_pulse.brightness = value;
    if (value >= 0xF1u)
        menu_pulse.falling = 1u;
    return 1;
}

sint32 tournament_sync_grid(void)
{
    FUNCTION_MARKER(0x80065274u, "MAIN.EXE");
    if ((sint16)menu_state.phase == 21)
        return menu_increment_group_offset((sint16)menu_course_select.course, (sint16)menu_course_select.level);
    return 21;
}

sint32 tournament_enter_race(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 mode = (sint16)game_selection.rules;
    TEXT_RECORD *record;

    FUNCTION_MARKER(0x800652B4u, "MAIN.EXE");
    menu_save_desc_payloads((sint16)menu_state.screen);
    profile_data = profile_current();
    profile_data->level_result = 0u;
    profile_data->course = (uint8)((uint8)menu_course_select.course);
    if (mode == 0)
    {
        game_selection.menu_variant = 0u;
        game_selection.event = 30u;
        name_copy_player_name_bytes(tournament_champ.player, hud_text.names[0]);
        profile_data->course = (uint8)(tournament_champ.course);
        profile_data->level = (uint8)((uint8)menu_course_select.level);
    }
    else if (mode == 1)
    {
        sint16 value;
        game_selection.event = 30u;
        name_copy_player_name_bytes(tournament_grid.player, hud_text.names[0]);
        value = (sint16)tournament_target_time(menu_course_select.course, tournament_grid.round);
        tournament_times[0].parts[0] = (uint16)(value / 60);
        tournament_times[0].parts[1] = (uint16)(value % 60);
    }
    else if (mode == 2)
    {
        sint32 row = tournament_matches.match;
        game_selection.event = 31u;
        name_copy_player_name_bytes(tournament_matches.winners[row], hud_text.names[0]);
        name_copy_player_name_bytes(tournament_matches.opponents[row], hud_text.names[1]);
    }
    game_selection.event_arg = 0u;
    w_u16(0x800B6AA0u, 5u);
    menu_state.sound = 1u;
    text_set_hud_visible(0, 1);
    menu_state.phase = 21u;
    menu_increment_group_offset((sint16)menu_course_select.course, (sint16)menu_course_select.level);
    menu_textures.rebuild = 1u;
    record = text_hud;
    record[0].x = 332u;
    record[0].y = 145u;
    return menu_build_player_mode_table();
}

sint32 tournament_grid_complete(void)
{
    sint16 counts[8] = {0};
    sint16 totals[8] = {0};
    sint16 best_count = -1;
    sint16 best_total = 1000;
    sint8 selected = -1;
    sint32 tied = 0;
    sint32 tied_total = 0;
    sint32 lower_total = 0;
    sint32 row;

    FUNCTION_MARKER(0x800654F4u, "MAIN.EXE");
    for (row = 0; row < (uint8)game_selection.players; ++row)
    {
        sint32 column = 0;
        if (tournament_grid.status[0][row] == 1u)
        {
            while (column < TOURNAMENT_GRID_ROUNDS && tournament_grid.status[column][row] == 1u)
            {
                ++counts[row];
                totals[row] = (sint16)(totals[row] + tournament_grid.seconds[column][row]);
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
            selected = (sint8)row;
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
                selected = (sint8)row;
                tied = 0;
            }
        }
    }
    game_selection.event = 49u;
    if (lower_total != 0)
    {
        w_u8(0x800B69E8u, (uint8)selected);
        w_u8(0x800B69BEu, 1u);
    }
    else
    {
        if (tied != 0)
        {
            if (tied_total == 0)
                game_selection.event = 1u;
        }
        else
            w_u8(0x800B69E8u, (uint8)selected);
        w_u8(0x800B69BEu, 0u);
    }
    game_selection.event_arg = 2u;
    return 2;
}

sint32 tournament_refresh_player_name(void)
{
    FUNCTION_MARKER(0x8006571Cu, "MAIN.EXE");
    return name_copy_player_name_bytes(r_u8(0x800B69E8u), text_menu[2].text);
}

sint32 tournament_record_result(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 state = (sint16)game_selection.selection;
    sint16 column = tournament_grid.round;
    uint8 completed = 0u;

    FUNCTION_MARKER(0x8006574Cu, "MAIN.EXE");
    game_selection.event = 30u;
    if (state == -4 || state == -2)
        tournament_grid.status[column][tournament_grid.player] = TOURNAMENT_GRID_PASSED;
    else if (state == -3 || state == -1)
        tournament_grid.status[column][tournament_grid.player] = TOURNAMENT_GRID_FAILED;
    profile_fn_800579d4(tournament_grid.player, unused2, unused3, unused4);
    tournament_grid.seconds[column][tournament_grid.player] = (uint16)(60u * race_lap_times[0][0] + race_lap_times[0][1]);
    for (;;)
    {
        sint32 first_open = 0;
        tournament_grid.player = (uint8)(tournament_grid.player + 1u);
        if (tournament_grid.player == (uint8)game_selection.players)
        {
            tournament_grid.player = 0u;
            tournament_grid.round = (uint8)(tournament_grid.round + 1u);
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

sint32 tournament_init_match_order(void)
{
    sint16 value = (sint16)tournament_matches.leader + 1;
    sint32 index;

    FUNCTION_MARKER(0x80065960u, "MAIN.EXE");
    tournament_matches.match = 0u;
    tournament_matches.winners[0] = (uint8)tournament_matches.leader;
    if (value == (uint8)game_selection.players)
        value = 0;
    for (index = 0; index < 4; ++index)
    {
        tournament_matches.opponents[index] = (uint8)value;
        ++value;
        if (value == (uint8)game_selection.players)
            value = 0;
    }
    return 0;
}

sint32 tournament_init_round(void)
{
    sint32 index;

    FUNCTION_MARKER(0x800659F0u, "MAIN.EXE");
    game_selection.menu_variant = 1u;
    tournament_matches.round = 0u;
    for (index = 0; index < 5; ++index)
        tournament_matches.points[index] = 0u;
    tournament_matches.leader = 0u;
    tournament_matches.match_count = (uint16)((uint8)game_selection.players - 1u);
    return tournament_init_match_order();
}

sint32 tournament_draw_match_highlight(void)
{
    sint16 row = tournament_matches.match;

    FUNCTION_MARKER(0x80065A6Cu, "MAIN.EXE");
    if (row < (sint16)tournament_matches.match_count)
    {
        sint16 top = (sint16)(30 * row + 74);
        display_set_line_color((sint8)menu_pulse.brightness, 64, 64);
        return display_queue_rect_outline(80, top, 430, (sint16)(top + 21));
    }
    return 0;
}

sint32 tournament_refresh_matches(void)
{
    TEXT_RECORD *menu = text_menu;
    UI_RECORD *records = sprite_records;
    sint32 index;

    FUNCTION_MARKER(0x80065AD8u, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        sint32 visible = index < (sint16)tournament_matches.match_count;
        sint16 y = (sint16)(30 * index + 80);
        char *output = menu[index + 8].text;
        sint32 character;
        menu[index + 4].visible = (uint8)visible;
        menu[index + 8].visible = (uint8)visible;
        menu[index + 12].visible = (uint8)visible;
        menu[index + 4].y = (uint16)y;
        menu[index + 8].y = (uint16)y;
        menu[index + 12].y = (uint16)y;
        if (tournament_matches.match < index)
        {
            for (character = 0; character < 5; ++character)
                output[(uint32)character] = r_u8(0x800B4274u + (uint32)character);
        }
        else
            name_copy_player_name_bytes(tournament_matches.winners[index], output);
        name_copy_player_name_bytes(tournament_matches.opponents[index], menu[index + 12].text);
        records[index + 2].type = index < tournament_matches.match;
        records[index + 6].type = index < tournament_matches.match;
        records[index + 2].y = (uint16)(y - 8);
        records[index + 6].y = (uint16)(y - 8);
        if (index < tournament_matches.match)
        {
            sint32 same = tournament_matches.winners[index + 1] == tournament_matches.winners[index];
            records[index + 2].x = same ? 105u : 350u;
            records[index + 6].x = same ? 350u : 105u;
        }
    }
    for (index = 0; index < 10; ++index)
        menu[index + 16].visible = index == tournament_matches.round;
    menu[3].visible = tournament_matches.match < (sint16)tournament_matches.match_count;
    menu[26].visible = menu[3].visible == 0u;
    return menu[26].visible;
}

sint32 tournament_update_match_input(CONTROLLER_STATE *input)
{
    uint8 value;

    FUNCTION_MARKER(0x80065D4Cu, "MAIN.EXE");
    if ((input->current & 0x40u) != 0u && menu_state.input_enabled != 0u)
    {
        if (tournament_matches.match >= (uint8)tournament_matches.match_count)
            menu_state.phase = 32u;
        else
            tournament_enter_race();
    }
    tournament_draw_match_highlight();
    if (menu_pulse.falling != 0u)
    {
        value = (uint8)(menu_pulse.brightness - 4u);
        menu_pulse.brightness = value;
        if (value < 0x80u)
            menu_pulse.falling = 0u;
        return value < 0x80u;
    }
    value = (uint8)(menu_pulse.brightness + 4u);
    menu_pulse.brightness = value;
    if (value >= 0xF1u)
        menu_pulse.falling = 1u;
    return 1;
}

sint32 tournament_sync_match(void)
{
    FUNCTION_MARKER(0x80065E54u, "MAIN.EXE");
    if ((sint16)menu_state.phase == 21)
        return menu_increment_group_offset((sint16)menu_course_select.course, (sint16)menu_course_select.level);
    return 21;
}

sint32 tournament_record_match_result(void)
{
    sint16 state = (sint16)game_selection.selection;
    sint32 row = tournament_matches.match;

    FUNCTION_MARKER(0x80065E94u, "MAIN.EXE");
    game_selection.event = 31u;
    if (state == -3)
        return state < -2;
    if (state == -1)
    {
        if (result_state.quit_player != 0u)
        {
            race_selection.ranks[0] = 0u;
            race_selection.ranks[1] = 1u;
        }
        else
            race_selection.ranks[0] = 1u;
    }
    {
        uint8 value = race_selection.ranks[0] >= race_selection.ranks[1] ? tournament_matches.opponents[row] : tournament_matches.winners[row];
        tournament_matches.points[value] = (uint8)(tournament_matches.points[value] + 1u);
        if (row + 1 >= (sint16)tournament_matches.match_count)
            tournament_matches.leader = value;
        tournament_matches.winners[row + 1] = value;
    }
    tournament_matches.match = (uint8)(row + 1);
    return row + 1;
}

sint32 tournament_refresh_standings(void)
{
    sint16 order[8];
    sint16 counts[8];
    TEXT_RECORD *menu = text_menu;
    sint32 first;
    sint32 second;

    FUNCTION_MARKER(0x80065F7Cu, "MAIN.EXE");
    for (first = 0; first < 5; ++first)
    {
        order[first] = (sint16)first;
        counts[first] = tournament_matches.points[first];
        menu[first + 2].visible = 0u;
        menu[first + 9].visible = 0u;
    }
    for (first = 0; first < (uint8)game_selection.players; ++first)
    {
        for (second = 0; second < (uint8)game_selection.players; ++second)
        {
            if (first != second && counts[second] < counts[first])
            {
                sint16 value = counts[first];
                sint16 index = order[first];
                counts[first] = counts[second];
                order[first] = order[second];
                counts[second] = value;
                order[second] = index;
            }
        }
    }
    for (first = 0; first < (uint8)game_selection.players; ++first)
    {
        char *output;
        menu[first + 2].visible = 1u;
        menu[first + 9].visible = 1u;
        name_copy_player_name_bytes((uint16)order[first], menu[first + 2].text);
        output = menu[first + 9].text;
        output[0] = (uint8)(counts[first] / 10 + 48);
        output[1u] = (uint8)(counts[first] % 10 + 48);
    }
    {
        char *output = menu[7].text;
        uint8 value = (uint8)(tournament_matches.round + 1u);
        output[0] = (uint8)(value / 10u + 48u);
        output[1u] = (uint8)(value % 10u + 48u);
        if (tournament_matches.round == 0u)
            output[8u] = 32u;
    }
    return 32;
}

sint32 tournament_advance_round(CONTROLLER_STATE *input)
{
    sint32 result = input->current & 0x40u;

    FUNCTION_MARKER(0x800662A4u, "MAIN.EXE");
    if (result != 0 && menu_state.input_enabled != 0u)
    {
        tournament_matches.round = (uint8)(tournament_matches.round + 1u);
        result = 33;
        if ((sint16)game_options.round_limit < tournament_matches.round)
            menu_state.phase = 33u;
        else
        {
            menu_state.phase = 31u;
            return tournament_init_round();
        }
    }
    return result;
}

sint32 tournament_refresh_leader(void)
{
    sint16 order[8];
    sint16 counts[8];
    sint32 first;
    sint32 second;
    TEXT_RECORD *menu = text_menu;

    FUNCTION_MARKER(0x80066338u, "MAIN.EXE");
    for (first = 0; first < 5; ++first)
    {
        order[first] = (sint16)first;
        counts[first] = tournament_matches.points[first];
    }
    for (first = 0; first < (uint8)game_selection.players; ++first)
    {
        for (second = 0; second < (uint8)game_selection.players; ++second)
        {
            if (first != second && counts[second] < counts[first])
            {
                sint16 value = counts[first];
                sint16 index = order[first];
                counts[first] = counts[second];
                order[first] = order[second];
                counts[second] = value;
                order[second] = index;
            }
        }
    }
    menu[0].visible = 1u;
    return name_copy_player_name_bytes((uint16)order[0], menu[0].text);
}

sint32 tournament_reset_result_rows(void)
{
    UI_RECORD *records = sprite_records;
    sint32 index;

    FUNCTION_MARKER(0x8006649Cu, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
        records[index + 2].type = 0u;
    return tournament_init_profile();
}

sint32 tournament_init_profile(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint16 row = (uint8)game_options.level;
    sint32 index;

    FUNCTION_MARKER(0x80066504u, "MAIN.EXE");
    profile_data->level = (uint8)((uint8)row);
    menu_course_select.level = (uint16)row;
    tournament_champ.course = 0u;
    tournament_champ.player = 0u;
    for (index = 0; index < 16; ++index)
    {
        tournament_champ.races[index] = 0u;
        tournament_champ.points[index] = 0u;
    }
    for (index = 0; index < 6; ++index)
        profile_data->progress.courses[(uint32)row][(uint32)index].state = (uint8)(5u);
    profile_data->progress.courses[(uint32)row][0u].state = (uint8)(4u);
    return 4;
}

sint32 tournament_refresh_table(void)
{
    UI_RECORD *records = sprite_records;
    TEXT_RECORD *menu = text_menu;
    sint32 rows = 0;
    sint32 index;
    sint16 top;

    FUNCTION_MARKER(0x80066600u, "MAIN.EXE");

    for (index = 0; index < 3; ++index)
        records[index + 1].type = index == (sint16)menu_course_select.level;
    if (tournament_champ.course < 6u)
    {
        records[5].type = 1u;
        records[6].type = 1u;
        records[7].type = 0u;
        menu[54].visible = 1u;
        menu[55].visible = 1u;
        menu[56].visible = 0u;
    }
    else
    {
        records[5].type = 0u;
        records[6].type = 0u;
        records[7].type = 1u;
        menu[54].visible = 0u;
        menu[55].visible = 0u;
        menu[56].visible = 1u;
    }
    for (index = 0; index < 16; ++index)
    {
        menu[index + 6].visible = 0u;
        menu[index + 22].visible = 0u;
        menu[index + 38].visible = 0u;
    }
    for (index = 0; index < 3; ++index)
        menu[index].visible = 0u;
    if ((sint16)r_u16(0x800B69CAu) == 1)
    {
        menu[0].visible = 1u;
        rows = 8;
    }
    else if ((sint16)r_u16(0x800B69CAu) == 2)
    {
        menu[1].visible = 1u;
        rows = 8;
    }
    else if ((sint16)r_u16(0x800B69CAu) == 3)
    {
        menu[2].visible = 1u;
        rows = (uint8)game_selection.players;
    }
    top = (sint16)(7 * (8 - rows) + 70);
    for (index = 0; index < rows; ++index)
    {
        sint16 source_index = (sint16)r_u16(0x800B6AF0u) + (sint16)index;
        sint16 y = (sint16)(top + 14 * index);
        TEXT_RECORD *left = &menu[source_index + 6];
        TEXT_RECORD *right = &menu[source_index + 38];
        TEXT_RECORD *value_text = &menu[source_index + 22];
        sint16 value = (sint16)r_u16(0x800DDCF8u + 2u * (uint32)source_index);
        left[0].visible = 1u;
        left[0].x = 110u;
        left[0].y = (uint16)y;
        right[0].visible = 1u;
        right[0].x = 330u;
        right[0].y = (uint16)y;
        value_text[0].visible = 1u;
        value_text[0].x = 390u;
        value_text[0].y = (uint16)y;
        if (value >= 0)
        {
            sint16 multiple = (sint16)(value - 9 * (value % 9));
            sint16 primitive_index = (value % 9 == 0 ? 13 : 22) + multiple;
            TEXT_RECORD *primitive = &text_hud[primitive_index];
            primitive[0].visible = 1u;
            primitive[0].x = 170u;
            primitive[0].y = (uint16)y;
        }
        else
        {
            sint16 peer = (sint16)~value;
            TEXT_RECORD *primitive = &text_hud[peer + 3];
            char *left_output = right[0].text;
            char *right_output = value_text[0].text;
            primitive[0].visible = 1u;
            primitive[0].x = 170u;
            primitive[0].y = (uint16)y;
            primitive[0].text = name_player_text((uint32)peer);
            left_output[0] = (uint8)(tournament_champ.races[peer] / 10u + 48u);
            left_output[1u] = (uint8)(tournament_champ.races[peer] % 10u + 48u);
            right_output[0] = (uint8)(tournament_champ.points[peer] / 10u + 48u);
            right_output[1u] = (uint8)(tournament_champ.points[peer] % 10u + 48u);
        }
    }
    menu[3].visible = 1u;
    menu[3].x = 110u;
    menu[3].y = (uint16)(top - 25);
    menu[4].visible = 1u;
    menu[4].x = 330u;
    menu[4].y = (uint16)(top - 25);
    menu[5].visible = 1u;
    menu[5].x = 390u;
    menu[5].y = (uint16)(top - 25);
    return 390;
}

sint32 tournament_init_table(void)
{
    sint16 order[8];
    sint16 identifiers[8];
    sint32 first;
    sint32 second;
    TEXT_RECORD *prims = text_hud;

    FUNCTION_MARKER(0x80066B50u, "MAIN.EXE");
    w_u16(0x800B6AF0u, 0u);
    for (first = 0; first < (uint8)game_selection.players; ++first)
    {
        TEXT_RECORD *primitive = &prims[first];
        primitive[3].font = 1u;
        primitive[3].x = 190u;
        primitive[3].text = name_player_text((uint32)first);
        identifiers[first] = (sint16)~first;
        order[first] = (sint16)first;
    }
    for (first = 0; first < 5; ++first)
    {
        for (second = 0; second < (uint8)game_selection.players; ++second)
        {
            sint32 other;
            for (other = 0; other < (uint8)game_selection.players; ++other)
            {
                if (tournament_champ.points[order[other]] < tournament_champ.points[order[second]])
                {
                    sint16 value = identifiers[second];
                    sint16 index = order[second];
                    identifiers[second] = identifiers[other];
                    order[second] = order[other];
                    identifiers[other] = value;
                    order[other] = index;
                }
            }
        }
    }
    for (first = 0; first < (uint8)game_selection.players; ++first)
        w_u16(0x800DDCF8u + 2u * (uint32)first, (uint16)identifiers[first]);
    return tournament_refresh_table();
}

sint32 tournament_update_mode_input(CONTROLLER_STATE *input)
{
    sint16 mode = (sint16)r_u16(0x800B69CAu);

    FUNCTION_MARKER(0x80066D58u, "MAIN.EXE");
    if (menu_state.input_enabled != 0u && mode == 3)
    {
        if (tournament_champ.course < 6u)
        {
            if ((input->current & 0x40u) != 0u)
            {
                menu_state.sound = 1u;
                tournament_enter_race();
            }
            if ((input->current & 0x10u) != 0u)
            {
                menu_state.sound = 2u;
                menu_state.phase = 1u;
            }
        }
        else if ((input->current & 0x40u) != 0u)
        {
            menu_state.sound = 1u;
            menu_state.phase = 1u;
        }
    }
    return tournament_refresh_table();
}

sint32 tournament_reset_text(void)
{
    FUNCTION_MARKER(0x80066E3Cu, "MAIN.EXE");
    return text_clear_hud();
}

sint32 tournament_record_champ_result(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint16 state = (sint16)game_selection.selection;
    sint16 source_index = 0;
    uint8 peer = tournament_champ.player;

    FUNCTION_MARKER(0x80066E5Cu, "MAIN.EXE");
    if (state == -3 || state == -1)
        source_index = 15;
    else if (state == -4 || state == -2)
        source_index = race_selection.ranks[0];
    tournament_champ.races[peer] = (uint8)(tournament_champ.races[peer] + 1u);
    tournament_champ.points[peer] = (uint8)(tournament_champ.points[peer] + ranking_place_points((uint32)source_index));
    tournament_champ.player = (uint8)(peer + 1u);
    if (tournament_champ.player == (uint8)game_selection.players)
    {
        tournament_champ.player = 0u;
        profile_data->progress.courses[menu_course_select.level][tournament_champ.course].state = (uint8)(6u);
        tournament_champ.course = (uint8)(tournament_champ.course + 1u);
        menu_course_select.course = tournament_champ.course;
        profile_data->course = (uint8)(tournament_champ.course);
        if (profile_data->course < 6u)
            profile_data->progress.courses[menu_course_select.level][tournament_champ.course].state = (uint8)(4u);
        else
        {
            profile_data->course = (uint8)(5u);
            game_selection.event = 1u;
            menu_course_select.course = 5u;
        }
    }
    game_selection.event = 38u;
    return 38;
}
