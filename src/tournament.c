#include "input.h"
#include "tournament.h"
#include "display.h"
#include "global.h"
#include "menu.h"
#include "name.h"
#include "profile.h"
#include "ranking.h"
#include "text.h"
#include "xport_trace.h"

sint32 tournament_draw_grid(void)
{
    sint16 rows = (sint8)r_u8(0x800E058Eu);
    sint16 top = (sint16)(13 * (5 - rows) + 62);
    sint16 selected_row = (sint16)(top + 27 * r_u8(0x800E1917u));
    sint16 selected_column = r_u8(0x800E1916u) >= 3u ? 2 : r_u8(0x800E1916u);
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
    w_u8(0x800E058Du, 0u);
    w_u8(0x800E1916u, 0u);
    w_u8(0x800E1917u, 0u);
    for (row = 0; row < 20; ++row)
    {
        for (column = 0; column < 5; ++column)
        {
            w_u8(0x800E1918u + 5u * (uint32)row + (uint32)column, 0u);
            w_u16(0x800E197Cu + 10u * (uint32)row + 2u * (uint32)column, 0u);
        }
    }
    return 0;
}

sint32 tournament_calc_target_time(uint32 value, sint16 index)
{
    sint16 table_index = index;
    sint16 excess = 0;

    FUNCTION_MARKER(0x80064BC4u, "MAIN.EXE");
    if (index >= 21)
    {
        excess = (sint16)(index - 20);
        table_index = 20;
    }
    return (sint16)(60 * (sint16)r_u16(value) + r_u16(value + 2u) - ((sint16)r_u16(0x800996D0u + 2u * (uint32)table_index) - excess));
}

sint32 tournament_refresh_grid(void)
{
    uint32 records = r_u32(0x800B6A74u);
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
    for (row = 0; row < r_u8(0x800E058Eu); ++row)
        menu[row + 2].y = (uint16)(top + 27 * row + 5);
    for (row = 0; row < 3; ++row)
        menu[row + 7].y = (uint16)(top - 15);
    for (row = 0; row < 5; ++row)
        menu[row + 2].visible = 0u;
    for (row = 0; row < r_u8(0x800E058Eu); ++row)
    {
        name_copy_player_name_bytes((uint16)row, menu[row + 2].text);
        menu[row + 2].visible = 1u;
    }
    column_offset = r_u8(0x800E1916u) < 3u ? 0 : (sint16)r_u8(0x800E1916u) - 2;
    for (row = 0; row < 15; ++row)
    {
        w_u8(records + 256u + 84u * (uint32)row, 0u);
        w_u8(records + 1516u + 84u * (uint32)row, 0u);
        menu[row + 10].visible = 0u;
    }
    for (row = 0; row < r_u8(0x800E058Eu); ++row)
    {
        sint16 y = (sint16)(top + 27 * row + 3);
        for (column = 0; column < 3; ++column, ++record_index)
        {
            sint32 source_column = column_offset + column;
            if (r_u8(0x800E1918u + 5u * (uint32)source_column + (uint32)row) == 1u)
            {
                uint32 record = records + 252u + 84u * (uint32)record_index;
                TEXT_RECORD *text = &menu[record_index + 10];
                uint16 value = r_u16(0x800E197Cu + 10u * (uint32)source_column + 2u * (uint32)row);
                char *output = text[0].text;
                sint32 remainder = value % 60u;
                w_u8(record + 4u, 1u);
                w_u16(record + 8u, (uint16)(90 * column + 191));
                w_u16(record + 10u, (uint16)y);
                text[0].visible = 1u;
                text[0].x = (uint16)(r_u16(record + 8u) + 18u);
                text[0].y = (uint16)(r_u16(record + 10u) + 11u);
                output[0] = (uint8)(value / 60u + 48u);
                output[2u] = (uint8)(remainder / 10 + 48);
                output[3u] = (uint8)(remainder % 10 + 48);
            }
            else if (source_column < r_u8(0x800E1916u) || (source_column == r_u8(0x800E1916u) && row < r_u8(0x800E1917u)))
            {
                uint32 record = records + 1512u + 84u * (uint32)record_index;
                w_u8(record + 4u, 1u);
                w_u16(record + 8u, (uint16)(90 * column + 200));
                w_u16(record + 10u, (uint16)y);
            }
        }
    }
    {
        uint32 values = 0x800996A0u + 8u * r_u16(0x800B4244u);
        for (column = 0; column < 3; ++column)
        {
            sint16 value = (sint16)tournament_calc_target_time(values, (sint16)(column_offset + column));
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
    if ((input->current & 0x40u) != 0u && r_u8(0x800B4138u) != 0u)
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
    if ((sint16)r_u16(0x800B413Eu) == 21)
        return menu_increment_group_offset((sint16)r_u16(0x800B4244u), (sint16)r_u16(0x800B4246u));
    return 21;
}

sint32 tournament_enter_race(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 mode = (sint16)r_u16(0x800E0584u);
    TEXT_RECORD *record;

    FUNCTION_MARKER(0x800652B4u, "MAIN.EXE");
    menu_save_desc_payloads((sint16)r_u16(0x800B413Cu));
    profile_data = profile_current();
    profile_data->level_result = 0u;
    profile_data->course = (uint8)((uint8)r_u16(0x800B4244u));
    if (mode == 0)
    {
        w_u8(0x800E058Du, 0u);
        w_u8(0x800E058Bu, 30u);
        name_copy_player_name_bytes(r_u8(0x800E1A8Bu), text_bind(0x80083994u));
        profile_data->course = (uint8)(r_u8(0x800E1A8Au));
        profile_data->level = (uint8)((uint8)r_u16(0x800B4246u));
    }
    else if (mode == 1)
    {
        sint16 value;
        w_u8(0x800E058Bu, 30u);
        name_copy_player_name_bytes(r_u8(0x800E1917u), text_bind(0x80083994u));
        value = (sint16)tournament_calc_target_time(0x800996A0u + 8u * r_u16(0x800B4244u), r_u8(0x800E1916u));
        w_u16(0x800E1A44u, (uint16)(value / 60));
        w_u16(0x800E1A46u, (uint16)(value % 60));
    }
    else if (mode == 2)
    {
        sint32 row = r_u8(0x800E1A6Du);
        w_u8(0x800E058Bu, 31u);
        name_copy_player_name_bytes(r_u8(0x800E1A6Eu + (uint32)row), text_bind(0x80083994u));
        name_copy_player_name_bytes(r_u8(0x800E1A73u + (uint32)row), text_bind(0x800839A4u));
    }
    w_u8(0x800E058Cu, 0u);
    w_u16(0x800B6AA0u, 5u);
    w_u16(0x800B6AFAu, 1u);
    text_set_hud_visible(0, 1);
    w_u16(0x800B413Eu, 21u);
    menu_increment_group_offset((sint16)r_u16(0x800B4244u), (sint16)r_u16(0x800B4246u));
    w_u16(0x800B4084u, 1u);
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
    for (row = 0; row < r_u8(0x800E058Eu); ++row)
    {
        sint32 column = 0;
        if (r_u8(0x800E1918u + (uint32)row) == 1u)
        {
            while (column < 20 && r_u8(0x800E1918u + 5u * (uint32)column + (uint32)row) == 1u)
            {
                ++counts[row];
                totals[row] = (sint16)(totals[row] + r_u16(0x800E197Cu + 10u * (uint32)column + 2u * (uint32)row));
                ++column;
            }
        }
    }
    for (row = 0; row < r_u8(0x800E058Eu); ++row)
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
    w_u8(0x800E058Bu, 49u);
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
                w_u8(0x800E058Bu, 1u);
        }
        else
            w_u8(0x800B69E8u, (uint8)selected);
        w_u8(0x800B69BEu, 0u);
    }
    w_u8(0x800E058Cu, 2u);
    return 2;
}

sint32 tournament_refresh_player_name(void)
{
    FUNCTION_MARKER(0x8006571Cu, "MAIN.EXE");
    return name_copy_player_name_bytes(r_u8(0x800B69E8u), text_menu[2].text);
}

sint32 tournament_record_result(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 state = (sint16)r_u16(0x800E0580u);
    sint16 column = r_u8(0x800E1916u);
    uint8 completed = 0u;

    FUNCTION_MARKER(0x8006574Cu, "MAIN.EXE");
    w_u8(0x800E058Bu, 30u);
    if (state == -4 || state == -2)
        w_u8(0x800E1918u + 5u * (uint32)column + r_u8(0x800E1917u), 1u);
    else if (state == -3 || state == -1)
        w_u8(0x800E1918u + 5u * (uint32)column + r_u8(0x800E1917u), 2u);
    profile_fn_800579d4(r_u8(0x800E1917u), unused2, unused3, unused4);
    w_u16(0x800E197Cu + 10u * (uint32)column + 2u * r_u8(0x800E1917u), (uint16)(60u * r_u16(0x800E18D4u) + r_u16(0x800E18D6u)));
    for (;;)
    {
        sint32 first_open = 0;
        w_u8(0x800E1917u, (uint8)(r_u8(0x800E1917u) + 1u));
        if (r_u8(0x800E1917u) == r_u8(0x800E058Eu))
        {
            w_u8(0x800E1917u, 0u);
            w_u8(0x800E1916u, (uint8)(r_u8(0x800E1916u) + 1u));
        }
        if (r_u8(0x800E1918u + r_u8(0x800E1917u)) == 1u)
        {
            do
                ++first_open;
            while (r_u8(0x800E1918u + 5u * (uint32)first_open + r_u8(0x800E1917u)) == 1u);
        }
        if (r_u8(0x800E1918u + 5u * (uint32)first_open + r_u8(0x800E1917u)) == 2u)
            ++completed;
        else
            break;
        if (completed >= r_u8(0x800E058Eu))
            return tournament_grid_complete();
    }
    return completed;
}

sint32 tournament_init_match_order(void)
{
    sint16 value = (sint16)r_u16(0x800B6A96u) + 1;
    sint32 index;

    FUNCTION_MARKER(0x80065960u, "MAIN.EXE");
    w_u8(0x800E1A6Du, 0u);
    w_u8(0x800E1A6Eu, (uint8)r_u16(0x800B6A96u));
    if (value == r_u8(0x800E058Eu))
        value = 0;
    for (index = 0; index < 4; ++index)
    {
        w_u8(0x800E1A73u + (uint32)index, (uint8)value);
        ++value;
        if (value == r_u8(0x800E058Eu))
            value = 0;
    }
    return 0;
}

sint32 tournament_init_round(void)
{
    sint32 index;

    FUNCTION_MARKER(0x800659F0u, "MAIN.EXE");
    w_u8(0x800E058Du, 1u);
    w_u8(0x800E1A6Cu, 0u);
    for (index = 0; index < 5; ++index)
        w_u8(0x800E1A78u + (uint32)index, 0u);
    w_u16(0x800B6A96u, 0u);
    w_u16(0x800B6B46u, (uint16)(r_u8(0x800E058Eu) - 1u));
    return tournament_init_match_order();
}

sint32 tournament_draw_match_highlight(void)
{
    sint16 row = r_u8(0x800E1A6Du);

    FUNCTION_MARKER(0x80065A6Cu, "MAIN.EXE");
    if (row < (sint16)r_u16(0x800B6B46u))
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
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x80065AD8u, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        sint32 visible = index < (sint16)r_u16(0x800B6B46u);
        sint16 y = (sint16)(30 * index + 80);
        char *output = menu[index + 8].text;
        sint32 character;
        menu[index + 4].visible = (uint8)visible;
        menu[index + 8].visible = (uint8)visible;
        menu[index + 12].visible = (uint8)visible;
        menu[index + 4].y = (uint16)y;
        menu[index + 8].y = (uint16)y;
        menu[index + 12].y = (uint16)y;
        if (r_u8(0x800E1A6Du) < index)
        {
            for (character = 0; character < 5; ++character)
                output[(uint32)character] = r_u8(0x800B4274u + (uint32)character);
        }
        else
            name_copy_player_name_bytes(r_u8(0x800E1A6Eu + (uint32)index), output);
        name_copy_player_name_bytes(r_u8(0x800E1A73u + (uint32)index), menu[index + 12].text);
        w_u8(records + 172u + 84u * (uint32)index, index < r_u8(0x800E1A6Du));
        w_u8(records + 508u + 84u * (uint32)index, index < r_u8(0x800E1A6Du));
        w_u16(records + 178u + 84u * (uint32)index, (uint16)(y - 8));
        w_u16(records + 514u + 84u * (uint32)index, (uint16)(y - 8));
        if (index < r_u8(0x800E1A6Du))
        {
            sint32 same = r_u8(0x800E1A6Fu + (uint32)index) == r_u8(0x800E1A6Eu + (uint32)index);
            w_u16(records + 176u + 84u * (uint32)index, same ? 105u : 350u);
            w_u16(records + 512u + 84u * (uint32)index, same ? 350u : 105u);
        }
    }
    for (index = 0; index < 10; ++index)
        menu[index + 16].visible = index == r_u8(0x800E1A6Cu);
    menu[3].visible = r_u8(0x800E1A6Du) < (sint16)r_u16(0x800B6B46u);
    menu[26].visible = menu[3].visible == 0u;
    return menu[26].visible;
}

sint32 tournament_update_match_input(CONTROLLER_STATE *input)
{
    uint8 value;

    FUNCTION_MARKER(0x80065D4Cu, "MAIN.EXE");
    if ((input->current & 0x40u) != 0u && r_u8(0x800B4138u) != 0u)
    {
        if (r_u8(0x800E1A6Du) >= r_u8(0x800B6B46u))
            w_u16(0x800B413Eu, 32u);
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
    if ((sint16)r_u16(0x800B413Eu) == 21)
        return menu_increment_group_offset((sint16)r_u16(0x800B4244u), (sint16)r_u16(0x800B4246u));
    return 21;
}

sint32 tournament_record_match_result(void)
{
    sint16 state = (sint16)r_u16(0x800E0580u);
    sint32 row = r_u8(0x800E1A6Du);

    FUNCTION_MARKER(0x80065E94u, "MAIN.EXE");
    w_u8(0x800E058Bu, 31u);
    if (state == -3)
        return state < -2;
    if (state == -1)
    {
        if (r_u16(0x800E1BB2u) != 0u)
        {
            w_u8(0x800E059Du, 0u);
            w_u8(0x800E059Eu, 1u);
        }
        else
            w_u8(0x800E059Du, 1u);
    }
    {
        uint8 value = r_u8(0x800E059Du) >= r_u8(0x800E059Eu) ? r_u8(0x800E1A73u + (uint32)row) : r_u8(0x800E1A6Eu + (uint32)row);
        w_u8(0x800E1A78u + value, (uint8)(r_u8(0x800E1A78u + value) + 1u));
        if (row + 1 >= (sint16)r_u16(0x800B6B46u))
            w_u16(0x800B6A96u, value);
        w_u8(0x800E1A6Fu + (uint32)row, value);
    }
    w_u8(0x800E1A6Du, (uint8)(row + 1));
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
        counts[first] = r_u8(0x800E1A78u + (uint32)first);
        menu[first + 2].visible = 0u;
        menu[first + 9].visible = 0u;
    }
    for (first = 0; first < r_u8(0x800E058Eu); ++first)
    {
        for (second = 0; second < r_u8(0x800E058Eu); ++second)
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
    for (first = 0; first < r_u8(0x800E058Eu); ++first)
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
        uint8 value = (uint8)(r_u8(0x800E1A6Cu) + 1u);
        output[0] = (uint8)(value / 10u + 48u);
        output[1u] = (uint8)(value % 10u + 48u);
        if (r_u8(0x800E1A6Cu) == 0u)
            output[8u] = 32u;
    }
    return 32;
}

sint32 tournament_advance_round(CONTROLLER_STATE *input)
{
    sint32 result = input->current & 0x40u;

    FUNCTION_MARKER(0x800662A4u, "MAIN.EXE");
    if (result != 0 && r_u8(0x800B4138u) != 0u)
    {
        w_u8(0x800E1A6Cu, (uint8)(r_u8(0x800E1A6Cu) + 1u));
        result = 33;
        if (r_u16(0x800E0590u) < r_u8(0x800E1A6Cu))
            w_u16(0x800B413Eu, 33u);
        else
        {
            w_u16(0x800B413Eu, 31u);
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
        counts[first] = r_u8(0x800E1A78u + (uint32)first);
    }
    for (first = 0; first < r_u8(0x800E058Eu); ++first)
    {
        for (second = 0; second < r_u8(0x800E058Eu); ++second)
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
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x8006649Cu, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
        w_u8(records + 172u + 84u * (uint32)index, 0u);
    return tournament_init_profile();
}

sint32 tournament_init_profile(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint16 row = r_u8(0x800E0592u);
    sint32 index;

    FUNCTION_MARKER(0x80066504u, "MAIN.EXE");
    profile_data->level = (uint8)((uint8)row);
    w_u16(0x800B4246u, (uint16)row);
    w_u8(0x800E1A8Au, 0u);
    w_u8(0x800E1A8Bu, 0u);
    for (index = 0; index < 16; ++index)
    {
        w_u8(0x800E1A8Cu + (uint32)index, 0u);
        w_u8(0x800E1A9Cu + (uint32)index, 0u);
    }
    for (index = 0; index < 6; ++index)
        profile_data->progress.courses[(uint32)row][(uint32)index].state = (uint8)(5u);
    profile_data->progress.courses[(uint32)row][0u].state = (uint8)(4u);
    return 4;
}

sint32 tournament_refresh_table(void)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    sint32 rows = 0;
    sint32 index;
    sint16 top;

    FUNCTION_MARKER(0x80066600u, "MAIN.EXE");

    for (index = 0; index < 3; ++index)
        w_u8(records + 88u + 84u * (uint32)index, index == (sint16)r_u16(0x800B4246u));
    if (r_u8(0x800E1A8Au) < 6u)
    {
        w_u8(records + 424u, 1u);
        w_u8(records + 508u, 1u);
        w_u8(records + 592u, 0u);
        menu[54].visible = 1u;
        menu[55].visible = 1u;
        menu[56].visible = 0u;
    }
    else
    {
        w_u8(records + 424u, 0u);
        w_u8(records + 508u, 0u);
        w_u8(records + 592u, 1u);
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
        rows = r_u8(0x800E058Eu);
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
            left_output[0] = (uint8)(r_u8(0x800E1A8Cu + (uint32)peer) / 10u + 48u);
            left_output[1u] = (uint8)(r_u8(0x800E1A8Cu + (uint32)peer) % 10u + 48u);
            right_output[0] = (uint8)(r_u8(0x800E1A9Cu + (uint32)peer) / 10u + 48u);
            right_output[1u] = (uint8)(r_u8(0x800E1A9Cu + (uint32)peer) % 10u + 48u);
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
    TEXT_RECORD *primitives = text_hud;

    FUNCTION_MARKER(0x80066B50u, "MAIN.EXE");
    w_u16(0x800B6AF0u, 0u);
    for (first = 0; first < r_u8(0x800E058Eu); ++first)
    {
        TEXT_RECORD *primitive = &primitives[first];
        primitive[3].font = 1u;
        primitive[3].x = 190u;
        primitive[3].text = name_player_text((uint32)first);
        identifiers[first] = (sint16)~first;
        order[first] = (sint16)first;
    }
    for (first = 0; first < 5; ++first)
    {
        for (second = 0; second < r_u8(0x800E058Eu); ++second)
        {
            sint32 other;
            for (other = 0; other < r_u8(0x800E058Eu); ++other)
            {
                if (r_u8(0x800E1A9Cu + (uint32)order[other]) < r_u8(0x800E1A9Cu + (uint32)order[second]))
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
    for (first = 0; first < r_u8(0x800E058Eu); ++first)
        w_u16(0x800DDCF8u + 2u * (uint32)first, (uint16)identifiers[first]);
    return tournament_refresh_table();
}

sint32 tournament_update_mode_input(CONTROLLER_STATE *input)
{
    sint16 mode = (sint16)r_u16(0x800B69CAu);

    FUNCTION_MARKER(0x80066D58u, "MAIN.EXE");
    if (r_u8(0x800B4138u) != 0u && mode == 3)
    {
        if (r_u8(0x800E1A8Au) < 6u)
        {
            if ((input->current & 0x40u) != 0u)
            {
                w_u16(0x800B6AFAu, 1u);
                tournament_enter_race();
            }
            if ((input->current & 0x10u) != 0u)
            {
                w_u16(0x800B6AFAu, 2u);
                w_u16(0x800B413Eu, 1u);
            }
        }
        else if ((input->current & 0x40u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 1u);
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
    sint16 state = (sint16)r_u16(0x800E0580u);
    sint16 source_index = 0;
    uint8 peer = r_u8(0x800E1A8Bu);

    FUNCTION_MARKER(0x80066E5Cu, "MAIN.EXE");
    if (state == -3 || state == -1)
        source_index = 15;
    else if (state == -4 || state == -2)
        source_index = r_u8(0x800E059Du);
    w_u8(0x800E1A8Cu + peer, (uint8)(r_u8(0x800E1A8Cu + peer) + 1u));
    w_u8(0x800E1A9Cu + peer, (uint8)(r_u8(0x800E1A9Cu + peer) + r_u8(0x80098E08u + (uint32)source_index)));
    w_u8(0x800E1A8Bu, (uint8)(peer + 1u));
    if (r_u8(0x800E1A8Bu) == r_u8(0x800E058Eu))
    {
        w_u8(0x800E1A8Bu, 0u);
        profile_data->progress.courses[r_u16(0x800B4246u)][r_u8(0x800E1A8Au)].state = (uint8)(6u);
        w_u8(0x800E1A8Au, (uint8)(r_u8(0x800E1A8Au) + 1u));
        w_u16(0x800B4244u, r_u8(0x800E1A8Au));
        profile_data->course = (uint8)(r_u8(0x800E1A8Au));
        if (profile_data->course < 6u)
            profile_data->progress.courses[r_u16(0x800B4246u)][r_u8(0x800E1A8Au)].state = (uint8)(4u);
        else
        {
            profile_data->course = (uint8)(5u);
            w_u8(0x800E058Bu, 1u);
            w_u16(0x800B4244u, 5u);
        }
    }
    w_u8(0x800E058Bu, 38u);
    return 38;
}
