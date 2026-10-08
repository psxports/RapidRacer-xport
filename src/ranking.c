#include "vehicle.h"
#include "input.h"
#include "display.h"
#include "name.h"
#include "sprite.h"
#include "vehicle_select.h"
#include "ranking.h"
#include "global.h"
#include "menu.h"
#include "profile.h"
#include "results.h"
#include "text.h"
#include "timer.h"
#include "xport_trace.h"
#include <stdlib.h>

static sint32 ranking_write_time(char *output, uint16 first, uint16 second, uint16 third)
{
    uint32 remainder;
    uint32 tens;

    output[0] = 48u;
    output[2u] = 48u;
    output[3u] = 48u;
    output[5u] = 48u;
    output[6u] = 48u;
    if (first != 0u)
        output[0] = (uint8)(first + 48u);
    if (second != 0u)
    {
        remainder = second % 100u;
        output[2u] = (uint8)(remainder / 10u + 48u);
        output[3u] = (uint8)(remainder % 10u + 48u);
    }
    if (third == 0u)
        return third;
    tens = (third / 10u) % 100u / 10u;
    output[5u] = (uint8)(tens + 48u);
    output[6u] = (uint8)((third / 10u) % 10u + 48u);
    return (sint32)(10u * tens);
}

static sint32 ranking_update_peer_times(uint32 peer, TEXT_RECORD *menu, uint32 visible_base, uint32 empty_base, uint32 text_base, uint32 text_empty)
{
    sint32 index;
    sint32 result = 0;

    for (index = 0; index < 3; ++index)
    {
        TEXT_RECORD *visible = &menu[visible_base + index];
        TEXT_RECORD *empty = &menu[empty_base + index];
        if (index < r_u8(peer) || r_u32(peer + 624u) != 0u)
        {
            uint32 source = peer + 20u + 16u * (uint32)index;
            visible->visible = 1u;
            empty->visible = 0u;
            result = ranking_write_time(visible->text, (uint16)text_parse_signed_decimal(text_bind(source)), (uint16)text_parse_signed_decimal(text_bind(source + 3u)), (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u))));
        }
        else
        {
            visible->visible = 0u;
            empty->visible = 1u;
        }
    }
    if (r_u32(peer + 624u) != 0u)
    {
        menu[text_base].visible = 1u;
        menu[text_empty].visible = 0u;
        result = ranking_write_time(menu[text_base].text, (uint16)text_parse_signed_decimal(text_bind(peer + 516u)), (uint16)text_parse_signed_decimal(text_bind(peer + 519u)), (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 522u))));
    }
    else
    {
        menu[text_base].visible = 0u;
        menu[text_empty].visible = 1u;
        result = 1;
    }
    return result;
}

sint32 ranking_find_empty_slot(void)
{
    sint32 count = (sint16)r_u16(0x800E1BAEu);
    sint32 index;

    FUNCTION_MARKER(0x8005B6B8u, "MAIN.EXE");
    if (count <= 0)
        return 15;
    for (index = 0; index < count; ++index)
    {
        if ((sint16)r_u16(0x800DCC10u + 2u * (uint32)index) == 0)
            return index;
    }
    return 15;
}

sint32 ranking_player_is_leading(void)
{
    PROFILE_SERIES *series;
    sint32 count = (sint16)r_u16(0x800E1BAEu);
    uint8 values[18];
    sint16 order[18];
    sint32 index;
    sint32 pass;

    FUNCTION_MARKER(0x8005B724u, "MAIN.EXE");

    series = profile_get_series();
    if (count < 0)
        count = 0;
    if (count > 18)
        count = 18;
    for (index = 0; index < count; ++index)
        values[index] = series->points[(uint32)index];
    if ((sint16)r_u16(0x800E0580u) == -4 || (sint16)r_u16(0x800E0580u) == -2)
    {
        for (index = 0; index < count; ++index)
            values[index] = (uint8)(values[index] + r_u8(0x80098E08u + r_u8(0x800E059Du + (uint32)index)));
    }
    else if (name_player_name_matches(0, 0x800B425Cu) != 0 && count > 0)
        values[0] = (uint8)(values[0] + 15u);
    for (index = 0; index < count; ++index)
        order[index] = (sint16)index;
    for (pass = 0; pass < count; ++pass)
    {
        for (index = 0; index < count - 1; ++index)
        {
            if (order[pass] != order[index] && values[order[pass]] >= values[order[index]])
            {
                sint16 temporary = order[pass];
                order[pass] = order[index];
                order[index] = temporary;
            }
        }
    }
    if (count == 0)
        return 1;
    return order[0] == 0;
}

sint32 ranking_refresh_championship(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    sint32 selected = (sint32)profile_data->course - 1;
    sint32 count = (sint16)r_u16(0x800E1BAEu);
    sint32 index;

    FUNCTION_MARKER(0x8005B950u, "MAIN.EXE");
    w_u8(0x800E05ADu, (uint8)series->grid_row);
    w_u16(0x800B4238u, r_u8(0x800E05ADu));
    for (index = 0; index < 6; ++index)
    {
        uint8 active = (uint8)(index == selected);
        menu[(2 * (index)) + 1].visible = active;
        menu[(2 * (index)) + 2].visible = active;
    }
    for (index = 0; index < 9; ++index)
    {
        menu[(3 * (index)) + 21].visible = 0u;
        menu[(3 * (index)) + 22].visible = 0u;
        menu[(3 * (index)) + 23].visible = 0u;
    }
    if ((sint16)r_u16(0x800E0580u) == -4 || (sint16)r_u16(0x800E0580u) == -2)
    {
        for (index = 0; index < count; ++index)
            series->points[(uint32)index] = (uint8)(series->points[(uint32)index] + r_u8(0x80098E08u + r_u8(0x800E059Du + (uint32)index)));
    }
    else if (name_player_name_matches(0, 0x800B425Cu) != 0)
        series->points[0] = (uint8)(series->points[0] + 15u);
    else
    {
        for (index = 1; index < count; ++index)
            series->points[(uint32)index] = (uint8)(series->points[(uint32)index] + r_u8(0x80098E08u + r_u8(0x800E059Du + (uint32)index)));
    }
    ranking_build_order();
    for (index = 0; index < count; ++index)
    {
        uint32 descriptor = 0x800E0B40u + 8u * (uint32)index;
        uint32 position = 3u * r_u8(descriptor) + r_u8(descriptor + 1u);
        uint32 score = r_u8(0x800E059Du + (uint32)index);
        if (score < 8u)
        {
            menu[position + 21].visible = 1u;
            menu[position + 21].y = (uint16)(12u * score + 78u);
        }
    }
    if ((sint16)r_u16(0x800E0580u) == -1 || (sint16)r_u16(0x800E0580u) == -3)
    {
        for (index = 0; index < 11; ++index)
            menu[index + 49].visible = 0u;
        return 0;
    }
    for (index = 0; index < 3; ++index)
        profile_format_value_parts3(0x800E18D4u + 6u * (uint32)index, menu[index + 50].text);
    {
        sint32 selection = selected + 6 * profile_data->level;
        uint32 summary = 0x800E0BC0u + 14u * (uint32)selection;
        uint32 record = 0x800E0CBCu + 172u * (uint32)selection;
        sint32 matched = 0;

        profile_format_value_parts3(summary + 8u, menu[54].text);
        for (index = 0; index < 5; ++index)
            menu[index + 55].visible = 0u;
        for (index = 0; index < 3; ++index)
        {
            if (time_fields3_compare(record + 2u, 0x800E18D4u + 6u * (uint32)index) != 0)
            {
                matched = 1;
                menu[index + 55].visible = 1u;
            }
        }
        if (time_fields3_compare(record + 50u, summary + 8u) != 0)
            matched = 1;
        if (matched != 0)
            menu[59].visible = 1u;
    }
    return 1;
}

sint32 ranking_refresh_standard(void)
{
    PROFILE_SERIES *series;
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    sint32 count = (sint16)vehicle_racer_count;
    sint32 index;
    sint32 result = count;

    FUNCTION_MARKER(0x8005BEACu, "MAIN.EXE");

    series = profile_get_series();
    ranking_build_order();
    for (index = 0; index < 3; ++index)
        w_u8(records + 88u + 84u * (uint32)index, (uint8)(index == (sint16)r_u16(0x800B4246u)));
    for (index = 0; index < 9; ++index)
    {
        menu[(3 * (index)) + 10].visible = 0u;
        menu[(3 * (index)) + 11].visible = 0u;
        menu[(3 * (index)) + 12].visible = 0u;
    }
    for (index = 0; index < count; ++index)
    {
        sint32 ranked = (sint16)r_u16(0x800DCC10u + 2u * (uint32)index);
        uint32 descriptor = 0x800E0B40u + 8u * (uint32)ranked;
        uint32 position = 3u * r_u8(descriptor) + r_u8(descriptor + 1u);
        uint32 score = series->points[(uint32)ranked];
        char *text = menu[index + 37].text;

        menu[position + 10].visible = 1u;
        menu[position + 10].y = (uint16)(14 * index + 70);
        text[0] = (uint8)(score / 10u + 48u);
        text[1u] = (uint8)(score % 10u + 48u);
        menu[index + 37].visible = 1u;
        result = 14 * index + 70;
        menu[index + 37].y = (uint16)result;
    }
    return result;
}

sint32 ranking_refresh_reduced(void)
{
    PROFILE_SERIES *series;
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    sint16 count;
    sint32 index;
    sint32 start;
    sint32 result;

    FUNCTION_MARKER(0x8005C0F4u, "MAIN.EXE");

    series = profile_get_series();
    ranking_build_order_desc();
    for (index = 0; index < 3; ++index)
        w_u8(records + 88u + 84u * (uint32)index, index == (sint16)r_u16(0x800B4246u));
    for (index = 0; index < 9; ++index)
    {
        menu[(3 * (index)) + 10].visible = 0u;
        menu[(3 * (index)) + 11].visible = 0u;
        menu[(3 * (index)) + 12].visible = 0u;
    }
    start = r_u8(0x800E059Au) != 0u ? 2 : 4;
    for (index = start; index < 8; ++index)
    {
        menu[index + 2].visible = 0u;
        menu[index + 37].visible = 0u;
    }
    count = (sint16)r_u16(0x800E1BB0u);
    if (count >= 5)
        count = 4;
    result = count;
    for (index = 0; index < count; ++index)
    {
        sint16 ranked = (sint16)r_u16(0x800DCC10u + 2u * (uint32)index);
        uint32 descriptor = 0x800E0B40u + 8u * (uint32)ranked;
        uint32 grid = 16u * (r_u8(descriptor + 1u) + 3u * r_u8(descriptor));
        uint8 score = series->points[(sint32)ranked];
        char *text = menu[index + 37].text;
        menu[r_u8(descriptor + 1) + (3 * (r_u8(descriptor))) + 10].visible = 1u;
        menu[r_u8(descriptor + 1) + (3 * (r_u8(descriptor))) + 10].y = (uint16)(14 * index + 70);
        text[0] = score != 0u ? (uint8)(score / 10u + 48u) : 48u;
        text[1u] = score != 0u ? (uint8)(score % 10u + 48u) : 48u;
        menu[index + 37].visible = 1u;
        result = 14 * index + 70;
        menu[index + 37].y = (uint16)result;
    }
    return result;
}

sint32 ranking_refresh(void)
{
    FUNCTION_MARKER(0x8005C3BCu, "MAIN.EXE");
    if (r_u8(0x800E058Du) != 0u)
        return ranking_refresh_reduced();
    return ranking_refresh_standard();
}

sint32 ranking_refresh_selection(void)
{
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x8005C3F8u, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
        w_u8(records + 88u + 84u * (uint32)index, (uint8)(index == (sint16)r_u16(0x800B4246u)));
    return (sint32)(records + 256u);
}

sint32 ranking_wrap_selection(sint32 fallback)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    sint32 result = (sint16)fallback;

    FUNCTION_MARKER(0x8005C458u, "MAIN.EXE");
    if (profile_data->course == 6u)
    {
        profile_data->course = (uint8)(0u);
        series->phase = (uint16)(0u);
        if (r_u8(0x800E058Du) != 0u)
            result = 4;
        else
        {
            result = 3;
            if (r_u16(0x800DCC10u) == 0u)
            {
                w_u16(0x800B4084u, 1u);
                result = 23;
            }
        }
    }
    return result;
}

sint32 profile_confirm_mode(CONTROLLER_STATE *input)
{
    sint32 result;

    FUNCTION_MARKER(0x8005C4FCu, "MAIN.EXE");
    w_u16(0x800B6A80u, 0u);
    result = input->pressed & 0x40u;
    if (result != 0)
    {
        PLAYER_PROFILE *profile_data = profile_current();
        result = 2;
        if (profile_data->level == 2u)
        {
            result = 14;
            if (profile_unlock(6) != 0)
            {
                w_u16(0x800B413Eu, 14u);
                w_u16(0x800B6A80u, 1u);
                result = 1;
            }
        }
    }
    return result;
}

sint32 profile_init_mode_state(void)
{
    FUNCTION_MARKER(0x8005C564u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B6A80u) != 0)
    {
        w_u16(0x800B4264u, 6u);
        w_u16(0x800B4266u, 6u);
        w_u16(0x800B6ABEu, 2u);
        return 2;
    }
    return 6;
}

sint32 ranking_build_order(void)
{
    PROFILE_SERIES *series;
    sint32 count = (sint16)r_u16(0x800E1BAEu);
    sint32 first;
    sint32 second;

    FUNCTION_MARKER(0x8005C598u, "MAIN.EXE");

    series = profile_get_series();
    for (first = 0; first < count; ++first)
        w_u16(0x800DCC10u + 2u * (uint32)first, (uint16)first);
    for (first = 0; first < count; ++first)
    {
        for (second = 0; second < count - 1; ++second)
        {
            sint16 first_index = (sint16)r_u16(0x800DCC10u + 2u * (uint32)first);
            sint16 second_index = (sint16)r_u16(0x800DCC10u + 2u * (uint32)second);
            if (first_index != second_index && series->points[(uint32)second_index] < series->points[(uint32)first_index])
            {
                w_u16(0x800DCC10u + 2u * (uint32)first, (uint16)second_index);
                w_u16(0x800DCC10u + 2u * (uint32)second, (uint16)first_index);
            }
        }
    }
    return 0;
}

sint32 profile_reset_select_state(void)
{
    PROFILE_SERIES *series = profile_get_series();
    uint8 selected;

    FUNCTION_MARKER(0x8005C6C8u, "MAIN.EXE");
    text_clear_boat_names();
    if ((sint16)r_u16(0x800B413Eu) != 3 && r_u8(0x800E058Du) == 0u && (sint16)r_u16(0x800E0582u) == 1)
    {
        selected = r_u8(0x800B4238u);
        w_u8(0x800E05ADu, selected);
        series->grid_row = (uint16)(selected);
    }
    if ((sint16)r_u16(0x800B4246u) == 3)
        w_u16(0x800B4246u, 2u);
    w_u16(0x800B4248u, 0u);
    return 2;
}

uint32 ranking_init_screen(void)
{
    uint32 frame;
    PLAYER_PROFILE *profile_data;
    sint16 selection;
    uint32 data;
    char *result;
    uint8 profile_row;
    sint32 index;

    FUNCTION_MARKER(0x8005C768u, "MAIN.EXE");
    frame = guest_stack_push(0x30u);
    profile_data = profile_current();
    for (index = 0; index < 6; ++index)
    {
        uint8 active = (uint8)(index == (sint16)r_u16(0x800B4244u));
        TEXT_RECORD *menu = text_menu;

        menu[(2 * (index))].visible = active;
        menu = text_menu;
        menu[(2 * (index)) + 1].visible = active;
    }
    for (index = 0; index < 3; ++index)
    {
        uint8 active = (uint8)(index == (sint16)r_u16(0x800B4246u));
        uint32 records = r_u32(0x800B6A74u);

        w_u8(records + 172u + 84u * (uint32)index, active);
    }
    profile_row = profile_data->level;
    selection = (sint16)(profile_row * 6u + r_u16(0x800B4244u));
    data = 0x800E0CBCu + 172u * (uint32)(sint32)selection;
    text_clear_boat_names();
    for (index = 0; index < 9; ++index)
        w_u8(frame + 0x10u + (uint32)index, (uint8)(10 * index));
    for (index = 0; index < 10; ++index)
    {
        sint16 y = (sint16)(13 * index + 83);
        sint32 half;
        uint8 visible = 0u;

        for (half = 0; half < 2; ++half)
        {
            TEXT_RECORD *menu = text_menu;
            TEXT_RECORD *descriptor = &menu[(2 * (index)) + half + 13];

            descriptor[0].y = (uint16)y;
            visible = (uint8)(y > 74 && y < 144);
            descriptor[0].visible = visible;
            descriptor[0].style = 0u;
        }
        {
            TEXT_RECORD *primitives = text_hud;
            TEXT_RECORD *primitive = &primitives[index + 3];
            sint16 lane_index;
            uint32 lane_address;
            uint8 lane;
            TEXT_RECORD *ranking;

            primitive[0].x = 160u;
            primitive[0].centered = 1u;
            lane_index = (sint16)r_u16(data + 28u + 2u * (uint32)index);
            lane_address = frame + 0x10u + (uint32)(sint32)lane_index;
            lane = r_u8(lane_address);
            ranking = &text_hud[lane + 31];
            primitive[0].y = (uint16)y;
            primitive[0].font = 2u;
            primitive[0].visible = visible;
            primitive[0].text = text_bind(0x800E0D30u + 172u * (uint32)selection + 5u * (uint32)index);
            primitive[0].style = 0u;
            ranking[0].x = 345u;
            ranking[0].y = (uint16)y;
            ranking[0].visible = visible;
            ranking[0].font = 2u;
            ranking[0].style = 0u;
            w_u8(lane_address, (uint8)(r_u8(lane_address) + 1u));
        }
    }
    for (index = 0; index < 10; ++index)
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[(2 * (index)) + 14].text;

        profile_format_value_parts3(data + 50u + 6u * (uint32)index, output);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[37].text;

        menu[36].text = text_bind(data + 8u);
        profile_format_value_parts3(data + 2u, output);
    }
    {
        sint16 name_index = (sint16)r_u16(data);
        uint32 source = 0x80099720u + 14u * (uint32)name_index;
        TEXT_RECORD *menu = text_menu;
        uint8 character = r_u8(source);
        char *output = menu[38].text;
        sint32 offset = 0;

        if (character != 0u)
        {
            do
            {
                character = r_u8(source + (uint32)(sint16)offset);
                output[(uint32)(sint16)offset] = character;
                offset = (sint16)(offset + 1);
                character = r_u8(source + (uint32)(sint16)offset);
            } while (character != 0u);
        }
        while ((sint16)offset < 12)
        {
            output[(uint32)(sint16)offset] = 32u;
            offset = (sint16)(offset + 1);
        }
        output[(uint32)(sint16)offset] = 0u;
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *output = menu[41].text;

        menu[40].text = text_bind(data + 22u);
        profile_format_value_parts3(data + 16u, output);
    }
    {
        sint16 name_index = (sint16)r_u16(data + 14u);
        uint32 source = 0x80099720u + 14u * (uint32)name_index;
        TEXT_RECORD *menu = text_menu;
        uint8 character = r_u8(source);
        char *output = menu[42].text;
        sint32 offset = 0;

        if (character != 0u)
        {
            do
            {
                character = r_u8(source + (uint32)(sint16)offset);
                output[(uint32)(sint16)offset] = character;
                offset = (sint16)(offset + 1);
                character = r_u8(source + (uint32)(sint16)offset);
            } while (character != 0u);
        }
        while ((sint16)offset < 12)
        {
            output[(uint32)(sint16)offset] = 32u;
            offset = (sint16)(offset + 1);
        }
        result = output + (uint32)(sint16)offset;
        result[0] = 0u;
    }
    w_u16(0x800B69B0u, 0u);
    w_u16(0x800B6AF8u, 0u);
    w_u16(0x800B6AFEu, 0u);
    guest_stack_pop(0x30u);
    return 1;
}

sint32 ranking_finalize_select(void)
{
    FUNCTION_MARKER(0x8005CCE4u, "MAIN.EXE");
    w_u16(0x800B4084u, 1u);
    text_clear_boat_names();
    text_clear_names();
    if ((sint16)r_u16(0x800B413Eu) == 21)
    {
        sint16 column = (sint16)r_u16(0x800B4244u);
        sint16 row = (sint16)r_u16(0x800B4246u);

        return menu_increment_group_offset(column, row);
    }
    return 21;
}

sint32 ranking_update_screen_anim(void)
{
    uint32 frame;
    sint16 active;
    sint32 result = 1;

    FUNCTION_MARKER(0x8005CD44u, "MAIN.EXE");
    active = (sint16)r_u16(0x800B6AF8u);
    frame = guest_stack_push(0x28u);
    if (active != 0)
    {
        PLAYER_PROFILE *profile_data = profile_current();
        uint8 profile_row = profile_data->level;
        sint16 selection = (sint16)(profile_row * 6u + r_u16(0x800B4244u));
        uint32 data = 0x800E0CBCu + 172u * (uint32)(sint32)selection;
        sint32 index;

        for (index = 0; index < 9; ++index)
            w_u8(frame + 0x10u + (uint32)index, (uint8)(10 * index));
        for (index = 0; index < 10; ++index)
        {
            sint32 half;
            sint8 clip = 0;
            uint8 visible = 0u;

            for (half = 0; half < 2; ++half)
            {
                TEXT_RECORD *menu = text_menu;
                TEXT_RECORD *descriptor = &menu[(2 * (index)) + half + 13];
                uint16 old_y = descriptor[0].y;
                uint16 delta = r_u16(0x800B6A9Eu);
                sint16 y = (sint16)(uint16)(old_y - delta);

                descriptor[0].y = (uint16)y;
                visible = (uint8)(y > 74 && y < 144);
                descriptor[0].visible = visible;
                if (y < 83)
                    clip = (sint8)(83 - y);
                else if (y > 135)
                    clip = (sint8)(-121 - y);
                descriptor[0].style = (uint8)clip;
            }
            {
                TEXT_RECORD *primitives = text_hud;
                TEXT_RECORD *primitive = &primitives[index + 3];
                uint16 old_y = primitive[0].y;
                uint16 delta = r_u16(0x800B6A9Eu);
                sint16 lane_index;
                uint32 lane_address;
                uint8 lane;
                TEXT_RECORD *ranking;

                primitive[0].visible = visible;
                primitive[0].style = (uint8)clip;
                primitive[0].y = (uint16)(old_y - delta);
                lane_index = (sint16)r_u16(data + 28u + 2u * (uint32)index);
                lane_address = frame + 0x10u + (uint32)(sint32)lane_index;
                lane = r_u8(lane_address);
                ranking = &text_hud[lane + 31];
                old_y = ranking[0].y;
                delta = r_u16(0x800B6A9Eu);
                ranking[0].visible = visible;
                ranking[0].style = (uint8)clip;
                ranking[0].y = (uint16)(old_y - delta);
                w_u8(lane_address, (uint8)(r_u8(lane_address) + 1u));
            }
        }
        {
            uint16 remaining = r_u16(0x800B6AFEu);
            uint16 delta = r_u16(0x800B6A9Eu);
            uint16 updated = (uint16)(remaining - delta);

            w_u16(0x800B6AFEu, updated);
            result = (sint32)((uint32)updated << 16);
        }
        if (result == 0)
            w_u16(0x800B6AF8u, 0u);
        guest_stack_pop(0x28u);
        return result;
    }
    {
        uint16 counter = (uint16)(r_u16(0x800B69B0u) + 1u);
        sint16 signed_counter = (sint16)counter;

        w_u16(0x800B69B0u, counter);
        if (signed_counter == 250)
        {
            w_u16(0x800B6AF8u, 1u);
            w_u16(0x800B6AFEu, 65u);
            w_u16(0x800B6A9Eu, 1u);
        }
        if (signed_counter == 500)
        {
            w_u16(0x800B6AF8u, 1u);
            w_u16(0x800B6AFEu, (uint16)-65);
            w_u16(0x800B6A9Eu, (uint16)-1);
            w_u16(0x800B69B0u, 0u);
            result = -1;
        }
    }
    guest_stack_pop(0x28u);
    return result;
}

void ranking_noop(void)
{
    FUNCTION_MARKER(0x8005CFD8u, "MAIN.EXE");
}

sint32 profile_unlock_progress(uint32 slot)
{
    PLAYER_PROFILE *profile_data = profile_at(slot);
    sint32 group;
    sint32 row;
    sint32 column;
    uint8 maximum = profile_data->max_level;

    FUNCTION_MARKER(0x8005D128u, "MAIN.EXE");
    for (group = 0; group < 3; ++group)
    {
        uint8 value = profile_at((uint32)group)->max_level;
        if (maximum < value)
            maximum = value;
    }
    profile_data->max_level = maximum;
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(5u);
    }
    if (maximum != 0xFFu)
    {
        for (row = 0; row <= maximum; ++row)
        {
            for (column = 0; column < 6; ++column)
                profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(4u);
        }
    }
    return maximum + 1u;
}

sint32 profile_select_mode(void)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x8005D354u, "MAIN.EXE");
    w_u8(0x800E058Du, 1u);
    profile_data = profile_current();
    selection = (sint16)menu_query_group_value(4, 0);
    if (selection == 0)
    {
        result = 1;
        w_u16(0x800B69CAu, 0u);
        if ((sint16)r_u16(0x800E0586u) == 1)
            result = profile_restore_backup();
        w_u16(0x800E0582u, 0u);
    }
    else
    {
        PROFILE_SERIES *series;
        sint32 index;

        profile_data->level = (uint8)((uint8)(selection - 1));
        w_u16(0x800E0582u, 1u);
        w_u16(0x800B69CAu, 2u);
        series = profile_get_series();
        if (series->phase != 0u)
        {
            w_u8(0x800E05ADu, (uint8)series->grid_row);
            result = (uint8)series->completed;
            profile_data->course = (uint8)((uint8)result);
        }
        else
        {
            result = (uint8)r_u16(0x800B4238u);
            series->completed = (uint16)(0u);
            series->phase = (uint16)(1u);
            w_u8(0x800E05ADu, (uint8)result);
            series->grid_row = (uint16)result;
            for (index = 0; index < 16; ++index)
                series->points[(uint32)index] = (uint8)(0u);
        }
    }
    return result;
}

sint32 profile_refresh_mode_visual(void)
{
    sint32 selection;
    sint32 record_index = 0;
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x8005D464u, "MAIN.EXE");

    selection = (sint16)menu_query_group_value(4, 0);
    w_u16(0x800B6C04u, (uint16)(selection - 1));
    if (selection == 1)
        record_index = 6;
    else if (selection == 2)
        record_index = 7;
    else if (selection == 3)
        record_index = 8;
    w_u8(records + 4u, 1u);
    for (index = 6; index < 9; ++index)
        w_u8(records + 4u + 84u * (uint32)index, 0u);
    if (record_index != 0)
    {
        uint32 record = records + 84u * (uint32)record_index;
        w_u8(record + 4u, 1u);
        if ((sint16)r_u16(0x800B413Au) != 0)
            return sprite_apply_prim_vram_mask(record);
        return sprite_disable_semitransparency(record);
    }
    w_u8(records + 844u, 0u);
    w_u8(records + 2608u, 0u);
    w_u8(records + 2692u, 0u);
    return (sint32)records;
}

sint32 profile_init_slot(sint16 profile_index)
{
    PLAYER_PROFILE *profile_data = profile_at(3u);
    sint32 row;
    sint32 index;

    FUNCTION_MARKER(0x8005D604u, "MAIN.EXE");
    profile_at(3u)->reset_pending = 1u;
    profile_copy_templates(profile_index);
    profile_reset_series(3u);
    profile_data->course = 0u;
    profile_data->level = 0u;
    profile_data->max_level = 0u;
    profile_data->reward_flags = 0u;
    for (row = 0; row < 3; ++row)
    {
        for (index = 0; index < 6; ++index)
        {
            profile_data->progress.courses[(uint32)row][(uint32)index].state = (uint8)(4u);
            profile_data->progress.courses[(uint32)row][(uint32)index].result = (uint8)(0u);
        }
    }
    if (r_u16(0x800E0582u) != 0u)
    {
        for (row = 0; row < 3; ++row)
        {
            uint8 value = profile_at((uint32)row)->max_level;
            if (profile_data->max_level < value)
                profile_data->max_level = value;
        }
    }
    else
        profile_unlock_progress(3u);
    return profile_merge_unlocks(profile_at(3u));
}

sint32 profile_init_select(void)
{
    FUNCTION_MARKER(0x8005D74Cu, "MAIN.EXE");
    w_u8(0x800E058Du, 1u);
    w_u8(0x800E058Eu, 2u);
    w_u8(0x800E0595u, 0u);
    if ((sint16)r_u16(0x800B4142u) == 1)
    {
        w_u16(0x800E0582u, 0u);
        w_u16(0x800E0586u, 0u);
        menu_update_group_desc_select(4, 0, 0);
    }
    else if ((sint16)r_u16(0x800E0582u) == 1)
    {
        menu_update_group_desc_select(4, 0, (sint16)r_u16(0x800B6C04u) + 1);
    }
    profile_refresh_mode_visual();
    return profile_init_slot(0);
}

sint32 ranking_refresh_peer_times(void)
{
    TEXT_RECORD *menu = text_menu;
    sint32 result;

    FUNCTION_MARKER(0x8005D9FCu, "MAIN.EXE");
    name_copy_player_name_bytes(r_u16(0x800B6A62u), menu[1].text);
    name_copy_player_name_bytes(0u, menu[4].text);
    result = ranking_update_peer_times(r_u32(0x800DE154u), menu, 6u, 18u, 10u, 21u);
    name_copy_player_name_bytes(1u, menu[11].text);
    result = ranking_update_peer_times(r_u32(0x800DF0FCu), menu, 13u, 22u, 17u, 25u);
    return result;
}

sint32 ranking_build_order_desc(void)
{
    PROFILE_SERIES *series;
    sint16 count;
    sint32 outer;
    sint32 inner;

    FUNCTION_MARKER(0x8005DCD8u, "MAIN.EXE");

    series = profile_get_series();
    count = (sint16)r_u16(0x800E1BB0u);
    for (outer = 0; outer < count; ++outer)
        w_u16(0x800DCC10u + 2u * (uint32)outer, (uint16)outer);
    for (outer = 0; outer < count; ++outer)
    {
        for (inner = 0; inner < count - 1; ++inner)
        {
            sint16 outer_index = (sint16)r_u16(0x800DCC10u + 2u * (uint32)outer);
            sint16 inner_index = (sint16)r_u16(0x800DCC10u + 2u * (uint32)inner);
            if (outer_index != inner_index && series->points[(sint32)outer_index] >= series->points[(sint32)inner_index])
            {
                w_u16(0x800DCC10u + 2u * (uint32)outer, (uint16)inner_index);
                w_u16(0x800DCC10u + 2u * (uint32)inner, (uint16)outer_index);
            }
        }
    }
    return outer < count;
}

sint32 ranking_refresh_results(void)
{
    PLAYER_PROFILE *profile = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    sint16 count = (sint16)r_u16(0x800E1BB0u);
    sint16 mode = (sint16)r_u16(0x800E0580u);
    sint32 index;

    FUNCTION_MARKER(0x8005DE08u, "MAIN.EXE");
    w_u16(0x800B4238u, r_u8(0x800E05ADu));
    text_clear_menu_range(1, 12);
    menu[(2 * (profile->course)) + -1].visible = 1u;
    menu[(2 * (profile->course))].visible = 1u;
    if (count == 2)
    {
        menu[15].visible = 0u;
        menu[16].visible = 0u;
        text_clear_menu_range(17, 4);
    }
    if (mode == -1 || mode == -3)
    {
        if (count == 4)
        {
            if (mode == -1)
            {
                w_u8(0x800E059Du, 3u);
                w_u8(0x800E059Eu, 2u);
                w_u8(0x800E059Fu, 1u);
                w_u8(0x800E05A0u, 0u);
                if ((sint16)r_u16(0x800E1BB2u) != 0)
                {
                    w_u8(0x800E059Du, 2u);
                    w_u8(0x800E059Eu, 3u);
                    series->points[0] = (uint8)(series->points[0] + r_u8(0x80098E0Au));
                }
                else
                    series->points[1] = (uint8)(series->points[1] + r_u8(0x80098E0Au));
                series->points[2] = (uint8)(series->points[2] + r_u8(0x80098E09u));
                series->points[3] = (uint8)(series->points[3] + r_u8(0x80098E08u));
            }
            else
            {
                uint8 limit = r_u8(0x800E059Du) < r_u8(0x800E059Eu) ? r_u8(0x800E059Du) : r_u8(0x800E059Eu);
                for (index = 2; index < 4; ++index)
                {
                    uint8 rank = r_u8(0x800E059Du + (uint32)index);
                    if (rank < limit)
                        series->points[(uint32)index] = (uint8)(series->points[(uint32)index] + r_u8(0x80098E08u + rank));
                }
            }
        }
        else
        {
            w_u8(0x800E059Du, 1u);
            w_u8(0x800E059Eu, 0u);
            if (mode == -1)
            {
                if ((sint16)r_u16(0x800E1BB2u) != 0)
                {
                    w_u8(0x800E059Du, 0u);
                    w_u8(0x800E059Eu, 1u);
                    series->points[0] = (uint8)(series->points[0] + r_u8(0x80098E08u));
                }
                else
                    series->points[1] = (uint8)(series->points[1] + r_u8(0x80098E08u));
            }
        }
    }
    else
    {
        for (index = 0; index < count; ++index)
        {
            uint8 rank = r_u8(0x800E059Du + (uint32)index);
            series->points[(uint32)index] = (uint8)(series->points[(uint32)index] + r_u8(0x80098E08u + rank));
        }
    }
    ranking_build_order_desc();
    for (index = 0; index < count; ++index)
    {
        uint8 rank = r_u8(0x800E059Du + (uint32)index);
        uint32 descriptor = 0x800E0B40u + 8u * (uint32)index;
        if (rank < count)
        {
            uint16 type = r_u16(descriptor + 4u);
            const char *source = type == 2u ? name_player_text(0u) : type == 3u ? name_player_text(1u) : text_bind(0x800A9720u + 14u * r_u8(descriptor));
            char *text = menu[index + 17].text;
            sint32 character = 0;
            while ((uint8)source[character] != 0u)
            {
                text[(uint32)character] = (uint8)source[character];
                ++character;
            }
            while (character < 12)
                text[(uint32)character++] = 32u;
            text[(uint32)character] = 0u;
            menu[index + 17].visible = 1u;
            menu[index + 17].y = (uint16)(12u * rank + 96u);
        }
    }
    return count << 16;
}

sint32 ranking_fn_8005e320(sint16 value)
{
    FUNCTION_MARKER(0x8005E320u, "MAIN.EXE");

    profile_get_series();
    return value;
}

sint32 results_enter_best_times_store(uint32 first, uint32 second, uint32 third, uint32 fourth)
{
    FUNCTION_MARKER(0x80063A18u, "MAIN.EXE");
    profile_update_best_times(first, second, third, fourth);
    w_u8(0x800E058Bu, 10u);
    w_u8(0x800E058Cu, 0u);
    return 10;
}
