#include "name.h"
#include "input.h"
#include "display.h"
#include "mdec.h"
#include "scene.h"
#include "vehicle_select.h"
#include "global.h"
#include "menu.h"
#include "profile.h"
#include "sprite.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>

static PROFILE_GRID vehicle_custom_grid;
static PROFILE_GRID vehicle_preview_grid;

static const PROFILE_GRID profile_grid_defaults[10] = {
    {{2u, 1u, 1u, 2u}, {3u, 2u, 2u, 2u}, {0u, 0u, 0u, 0u}}, {{1u, 1u, 2u, 2u}, {2u, 2u, 3u, 2u}, {0u, 0u, 0u, 0u}}, {{1u, 2u, 1u, 2u}, {2u, 3u, 2u, 2u}, {0u, 0u, 0u, 0u}}, {{2u, 2u, 2u, 4u}, {5u, 4u, 4u, 4u}, {0u, 0u, 0u, 0u}}, {{2u, 2u, 2u, 4u}, {4u, 4u, 5u, 4u}, {0u, 0u, 0u, 0u}}, {{2u, 2u, 2u, 4u}, {4u, 5u, 4u, 4u}, {0u, 0u, 0u, 0u}}, {{5u, 4u, 4u, 5u}, {5u, 5u, 5u, 5u}, {2u, 0u, 0u, 2u}}, {{4u, 4u, 5u, 4u}, {5u, 5u, 5u, 4u}, {0u, 0u, 2u, 2u}}, {{5u, 5u, 5u, 5u}, {5u, 5u, 5u, 5u}, {0u, 0u, 0u, 0u}}, {{0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}},
};

static void profile_copy_grid_defaults(PLAYER_PROFILE *profile)
{
    uint32 row;

    for (row = 0u; row < 10u; ++row)
        profile->grid[row] = profile_grid_defaults[row];
}

static sint32 vehicle_select_pack_tpage(sint32 first, sint32 second, uint32 x, uint32 y)
{
    if (r_u8(0x800A0C38u) == 1u || r_u8(0x800A0C38u) == 2u)
        return ((first & 3) << 9) | ((second & 3) << 7) | ((y & 0x300u) >> 3) | ((x & 0x3FFu) >> 6);
    return ((first & 3) << 7) | ((second & 3) << 5) | ((y & 0x100u) >> 4) | ((x & 0x3FFu) >> 6) | (4 * (y & 0x200u));
}

static sint32 vehicle_select_pack_clut(uint32 x, uint32 y)
{
    return (sint32)(((y << 6) & 0xFFFFu) | ((x >> 4) & 0x3Fu));
}

static void vehicle_select_set_prim_alternate(uint32 primitive, sint32 alternate)
{
    if (alternate != 0)
        xport_update_u8(primitive + 7u, XPORT_MEMORY_UPDATE_OR, 2u);
    else
        xport_update_u8(primitive + 7u, XPORT_MEMORY_UPDATE_AND, 0xFDu);
}

static sint32 vehicle_select_build_grid(sint16 record_group, const PROFILE_GRID *profile_row, sint16 offset_x, sint16 offset_y, uint32 coordinate_table)
{
    uint32 records;
    uint32 source_record_table = 0x80098EC8u;
    sint32 outer;
    sint32 inner;

    w_u16(0x800B40B4u, 3u);
    records = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_group;
    for (outer = 0; outer < 4; ++outer)
        sprite_apply_prim_vram_mask(records + 84u * (uint32)(sint16)r_u16(source_record_table + 2u * (uint32)outer));
    for (outer = 0; outer < 4; ++outer)
    {
        sint8 orientation = (sint8)r_u16(0x80098E90u + 12u * (uint32)outer);
        sint32 column = outer;
        for (inner = 0; inner < 5; ++inner)
        {
            uint32 target = records + 84u * (uint32)(sint16)r_u16(0x80098E92u + 12u * (uint32)outer + 2u * (uint32)inner);
            uint32 source_record;
            sint32 alternate = 0;
            uint32 source_primitive;
            uint32 target_primitive;
            sint16 primitive_index;
            uint32 primitive_base;
            uint32 tpage_x;
            uint32 tpage_y;
            uint32 clut_x;
            uint32 clut_y;

            sprite_set_orient(target, orientation);
            w_u8(target + 4u, (sint32)profile_row->limit[column] - 1 >= inner);
            w_u16(target + 8u, (uint16)(offset_x + (sint16)r_u16(coordinate_table + 4u * (uint32)(5 * outer + inner))));
            w_u16(target + 10u, (uint16)(offset_y + (sint16)r_u16(coordinate_table + 4u * (uint32)(5 * outer + inner) + 2u)));
            if (inner < profile_row->choice[column])
                source_record = records + 84u * (uint32)(sint16)r_u16(0x80098EC0u + 2u * (uint32)outer);
            else
            {
                source_record = records + 84u * (uint32)(sint16)r_u16(0x80098EC8u + 2u * (uint32)outer);
                alternate = 1;
            }
            primitive_index = (sint16)r_u16(source_record + 6u);
            primitive_base = r_u32(source_record);
            source_primitive = primitive_base + 120u * (uint32)(sint32)primitive_index;
            primitive_index = (sint16)r_u16(target + 6u);
            tpage_x = r_u32(source_primitive + 32u);
            tpage_y = r_u32(source_primitive + 36u);
            primitive_base = r_u32(target);
            target_primitive = primitive_base + 120u * (uint32)(sint32)primitive_index;
            w_u16(target_primitive + 74u, (uint16)vehicle_select_pack_tpage(0, 3, tpage_x, tpage_y));
            clut_x = r_u32(source_primitive + 24u);
            clut_y = r_u32(source_primitive + 28u);
            w_u16(target_primitive + 66u, (uint16)vehicle_select_pack_clut(clut_x, clut_y));
            vehicle_select_set_prim_alternate(target_primitive + 52u, alternate);
            w_u8(target_primitive + 57u, 208u);
            w_u8(target_primitive + 58u, 208u);
            w_u8(target_primitive + 56u, 208u);
            DrawSync(0);
        }
    }
    w_u16(0x800B40B4u, 0u);
    return 0;
}

void vehicle_select_enable_prim(uint32 primitive, sint32 enabled)
{
    if (enabled != 0)
        xport_update_u8(primitive + 7u, XPORT_MEMORY_UPDATE_OR, 1u);
    else
        xport_update_u8(primitive + 7u, XPORT_MEMORY_UPDATE_AND, 0xFEu);
}

PROFILE_GRID *profile_grid_row(sint16 index)
{
    FUNCTION_MARKER(0x8005E354u, "MAIN.EXE");
    if ((sint16)r_u16(0x800E0582u) == 7)
        return &vehicle_custom_grid;
    return &profile_current()->grid[index];
}

sint32 profile_get_grid_byte(sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E3BCu, "MAIN.EXE");
    return profile_grid_row(row)->choice[column];
}

sint32 profile_get_table_byte(const PLAYER_PROFILE *data, sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E448u, "MAIN.EXE");
    if ((sint16)r_u16(0x800E0582u) == 7)
        return vehicle_custom_grid.choice[column];
    return data->grid[row].choice[column];
}

void vehicle_select_clear_grid(PROFILE_GRID *data)
{
    sint32 index;

    FUNCTION_MARKER(0x8005E4A4u, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        data->choice[index] = 0u;
        data->limit[index] = 0u;
        data->offset[index] = 0u;
    }
}

sint32 profile_get_row_offset(sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E520u, "MAIN.EXE");
    return profile_grid_row(row)->offset[column];
}

sint32 profile_copy_templates(sint16 profile_index)
{
    PLAYER_PROFILE *destination;
    uint8 mode;

    FUNCTION_MARKER(0x8005E560u, "MAIN.EXE");
    mode = r_u8(0x800E058Eu);
    if (mode == 1u)
        destination = profile_at((uint32)profile_index);
    else if (mode == 2u)
        destination = profile_at(3u);
    else
        destination = profile_at(4u);
    profile_copy_grid_defaults(destination);
    return 0;
}

sint32 vehicle_select_fn_8005e624(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 result = 12;

    FUNCTION_MARKER(0x8005E624u, "MAIN.EXE");
    w_u8(0x800E058Cu, 0u);
    w_u16(0x800E0582u, r_u16(0x800E0586u));
    if (r_u8(0x800E05B3u) == 2u)
    {
        PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
        sint16 column = r_u8(0x800E05B4u);
        state->result = (uint8)(2u);
        if (profile_data->course < 5u)
            profile_data->course = (uint8)((uint8)(profile_data->course + 1u));
        w_u16(0x800E0580u, (uint16)-1);
        if (profile_get_row_offset(r_u8(0x800E05ADu), column) == 0)
        {
            w_u16(0x800B4264u, (uint16)(5 * column));
            result = profile_get_grid_byte(r_u8(0x800E05ADu), column);
            w_u16(0x800B6ABEu, 1u);
            w_u8(0x800E058Bu, 13u);
            w_u16(0x800B4264u, (uint16)((sint16)r_u16(0x800B4264u) + result));
            return result;
        }
        w_u16(0x800B4266u, (uint16)-1);
        if (column == 0)
            w_u16(0x800B4266u, 5u);
        else if (column == 1)
            w_u16(0x800B4266u, 3u);
        else if (column == 2)
            w_u16(0x800B4266u, 4u);
        if (profile_unlock((sint16)r_u16(0x800B4266u)) != 0)
        {
            w_u16(0x800B6ABEu, 2u);
            w_u8(0x800E058Bu, 14u);
            w_u16(0x800B4264u, r_u16(0x800B4266u));
            return 14;
        }
        result = 10;
    }
    w_u8(0x800E058Bu, (uint8)result);
    return result;
}

sint32 profile_build_vehicle_grid(sint16 record_group, sint16 profile_row, sint16 offset_x, sint16 offset_y)
{
    const PROFILE_GRID *profile;
    PROFILE_GRID reward;

    FUNCTION_MARKER(0x8005E7DCu, "MAIN.EXE");
    if (profile_row == -1)
    {
        PLAYER_PROFILE *native = profile_current();
        uint32 column;

        // Fourth-column reward spans progression and availability
        reward.choice[0] = native->progress.courses[3][4].state;
        reward.choice[1] = native->progress.courses[3][5].state;
        reward.choice[2] = native->availability[0];
        reward.choice[3] = native->availability[1];
        for (column = 0u; column < 4u; ++column)
        {
            reward.limit[column] = native->availability[column + 2u];
            reward.offset[column] = native->availability[column + 6u];
        }
        profile = &reward;
    }
    else
        profile = &profile_current()->grid[profile_row];
    return vehicle_select_build_grid(record_group, profile, offset_x, offset_y, 0x80098EE0u);
}

sint32 vehicle_select_render(sint16 record_group, sint32 unused2, sint16 offset_x, sint16 offset_y, const PROFILE_GRID *profile_row)
{
    FUNCTION_MARKER(0x8005EB18u, "MAIN.EXE");

    return vehicle_select_build_grid(record_group, profile_row, offset_x, offset_y, 0x80098F30u);
}

sint32 vehicle_draw_grid_lines(sint16 color, sint16 left, sint16 top, sint32 rows)
{
    sint16 right = (sint16)(left + 10);
    sint16 bottom = (sint16)(top + 30);
    sint16 row_count = (sint16)((uint32)rows + 1u);
    sint32 index;
    sint32 line;

    FUNCTION_MARKER(0x8005EE38u, "MAIN.EXE");
    if (color == 0)
        display_set_line_color((sint8)216, (sint8)232, 56);
    else if (color == 1)
        display_set_line_color((sint8)192, 0, 0);
    else if (color == 2)
        display_set_line_color((sint8)192, 0, (sint8)192);
    for (index = 0; index < row_count; ++index)
    {
        sint16 y = (sint16)(bottom - 6 * (index + 1));
        for (line = 1; line < 6; ++line)
            display_queue_line_segment((sint16)(left + 1), (sint16)(y + line), (sint16)(right - 1), (sint16)(y + line));
    }
    for (index = 0; index < 4; ++index)
    {
        sint16 y = (sint16)(top + 6 + 6 * index);
        display_set_line_color((sint8)192, (sint8)192, (sint8)192);
        display_queue_line_segment(left, y, right, y);
        display_set_line_color(32, 32, 32);
        display_queue_line_segment(left, y, right, y);
    }
    return display_queue_beveled_rect_outline(left, top, right, bottom);
}

sint32 vehicle_select_update_grid(void)
{
    sint16 track = (sint16)r_u16(0x800B413Cu);
    sint16 selection;
    uint32 table;
    PROFILE_GRID *row;
    sint32 index;
    sint32 available;
    sint16 count;

    FUNCTION_MARKER(0x8005F06Cu, "MAIN.EXE");
    selection = (sint16)r_u16(0x800B4238u);
    table = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)track);
    w_u16(table + 12u, r_u16(0x80098F80u + 2u * (uint32)selection));
    selection = (sint16)r_u16(0x800B4238u);
    row = &profile_current()->grid[selection];
    for (index = 0; index < 9; ++index)
    {
        sint16 mapped = (sint16)r_u16(0x80098F80u + 2u * (uint32)index);
        w_u8(r_u32(0x800B6A74u) + 4u + 84u * (uint32)(sint32)mapped, 0u);
        text_menu[index + 1].visible = 0u;
    }
    for (index = 0; index < 5; ++index)
    {
        uint32 offset = 84u * (uint32)index;
        w_u8(r_u32(0x800B6A74u) + 10420u + offset, 0u);
        w_u8(r_u32(0x800B6A74u) + 10840u + offset, 0u);
        w_u8(r_u32(0x800B6A74u) + 11260u + offset, 0u);
    }
    profile_build_vehicle_grid(9, (sint16)r_u16(0x800B4238u), 364, 55);
    for (index = 0; index < 5; ++index)
    {
        text_menu[index + 10].visible = 0u;
        text_menu[index + 15].visible = 0u;
        text_menu[index + 35].visible = 0u;
        text_menu[index + 20].visible = 0u;
        text_menu[index + 25].visible = 0u;
        text_menu[index + 30].visible = 0u;
        text_menu[(2 * (index)) + 47].visible = 0u;
        text_menu[(2 * (index)) + 48].visible = 0u;
        text_menu[(2 * (index)) + 57].visible = 0u;
        text_menu[(2 * (index)) + 58].visible = 0u;
        text_menu[(2 * (index)) + 67].visible = 0u;
        text_menu[(2 * (index)) + 68].visible = 0u;
    }
    for (index = 0; index < 3; ++index)
        w_u8(r_u32(0x800B6A74u) + 10168u + 84u * (uint32)index, 0u);
    selection = (sint16)r_u16(0x800B4238u);
    text_menu[selection + 1].visible = 1u;
    text_menu[row->choice[3] + 9].visible = 1u;
    text_menu[row->choice[3] + 14].visible = 1u;
    text_menu[row->choice[3] + 34].visible = 1u;

    vehicle_select_clear_grid(&vehicle_preview_grid);
    vehicle_preview_grid.choice[0] = (uint8)(row->choice[0]);
    selection = (sint16)r_u16(0x800B4238u);
    vehicle_preview_grid.limit[0] = (uint8)(row->limit[0]);
    vehicle_select_render(37, selection, 50, 500, &vehicle_preview_grid);
    count = (sint16)((sint32)row->choice[0] - 1);
    w_u16(0x800B6B60u, (uint16)count);
    count = (sint16)r_u16(0x800B6B60u);
    vehicle_draw_grid_lines(0, 65, 135, count);
    w_u8(r_u32(0x800B6A74u) + 10420u + 84u * (uint32)(sint32)count, 1u);
    text_menu[count + 20].visible = 1u;
    text_menu[(2 * (count)) + 47].visible = 1u;
    text_menu[(2 * (count)) + 48].visible = 1u;

    vehicle_select_clear_grid(&vehicle_preview_grid);
    vehicle_preview_grid.choice[1] = (uint8)(row->choice[1]);
    selection = (sint16)r_u16(0x800B4238u);
    vehicle_preview_grid.limit[1] = (uint8)(row->limit[1]);
    vehicle_select_render(65, selection, 190, 500, &vehicle_preview_grid);
    count = (sint16)((sint32)row->choice[1] - 1);
    w_u16(0x800B6B62u, (uint16)count);
    count = (sint16)r_u16(0x800B6B62u);
    vehicle_draw_grid_lines(1, 205, 135, count);
    w_u8(r_u32(0x800B6A74u) + 10840u + 84u * (uint32)(sint32)count, 1u);
    text_menu[count + 25].visible = 1u;
    text_menu[(2 * (count)) + 57].visible = 1u;
    text_menu[(2 * (count)) + 58].visible = 1u;

    vehicle_select_clear_grid(&vehicle_preview_grid);
    vehicle_preview_grid.choice[2] = (uint8)(row->choice[2]);
    selection = (sint16)r_u16(0x800B4238u);
    vehicle_preview_grid.limit[2] = (uint8)(row->limit[2]);
    vehicle_select_render(93, selection, 330, 500, &vehicle_preview_grid);
    count = (sint16)((sint32)row->choice[2] - 1);
    w_u16(0x800B6B64u, (uint16)count);
    count = (sint16)r_u16(0x800B6B64u);
    vehicle_draw_grid_lines(2, 345, 135, count);
    w_u8(r_u32(0x800B6A74u) + 11260u + 84u * (uint32)(sint32)count, 1u);
    text_menu[count + 30].visible = 1u;
    text_menu[(2 * (count)) + 67].visible = 1u;
    text_menu[(2 * (count)) + 68].visible = 1u;

    available = (uint8)profile_is_unlocked((sint16)((sint16)r_u16(0x800B4238u))) != 0u;
    text_menu[42].visible = (uint8)available;
    w_u8(r_u32(0x800B6A74u) + 11848u, (uint8)available);
    if (available != 0)
    {
        sint32 conflict = 0;
        if ((sint16)r_u16(0x800B6B60u) >= 2 || (sint16)r_u16(0x800B6B62u) >= 2 || (sint16)r_u16(0x800B6B64u) >= 2)
            conflict = 1;
        if ((sint16)r_u16(0x800B6B60u) > 0)
        {
            if ((sint16)r_u16(0x800B6B62u) > 0)
                conflict = 1;
            if ((sint16)r_u16(0x800B6B64u) > 0)
                conflict = 1;
        }
        if ((sint16)r_u16(0x800B6B62u) > 0 && (sint16)r_u16(0x800B6B64u) > 0)
            conflict = 1;
        if (conflict == 0)
            available = 0;
    }
    text_menu[43].visible = (uint8)available;
    {
        uint32 records = r_u32(0x800B6A74u);
        w_u8(records + 11932u, (uint8)available);
        return (sint32)records;
    }
}

sint32 vehicle_process_grid_input(CONTROLLER_STATE *input)
{
    sint16 suppress_grid = (sint16)r_u16(0x800B6B68u);
    sint32 result;

    FUNCTION_MARKER(0x8005F71Cu, "MAIN.EXE");
    if (suppress_grid == 0)
    {
        vehicle_draw_grid_lines(0, 65, 135, (sint16)r_u16(0x800B6B60u));
        vehicle_draw_grid_lines(1, 205, 135, (sint16)r_u16(0x800B6B62u));
        vehicle_draw_grid_lines(2, 345, 135, (sint16)r_u16(0x800B6B64u));
    }
    result = r_u8(0x800B4138u);
    if (result != 0)
    {
        if (text_menu[43].visible == 1u && (input->current & 0x40u) != 0u)
        {
            w_u16(0x800B413Eu, 7u);
            w_u16(0x800B6AFAu, 1u);
        }
        result = 1;
        if (text_menu[42].visible == 1u)
        {
            result = 8;
            if ((input->current & 0x20u) != 0u)
            {
                w_u16(0x800B413Eu, 8u);
                w_u16(0x800B6AFAu, 1u);
                result = 1;
            }
        }
    }
    return result;
}

sint32 vehicle_select_fn_8005f82c(void)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *row = &profile_current()->grid[r_u16(0x800B4238u)];
    sint16 selections[3];
    sint32 index;

    FUNCTION_MARKER(0x8005F82Cu, "MAIN.EXE");
    for (index = 0; index < 9; ++index)
        w_u8(records + 172u + 84u * (uint32)index, index == (sint16)r_u16(0x800B4238u));
    for (index = 0; index < 5; ++index)
    {
        w_u8(records + 928u + 84u * (uint32)index, 0u);
        w_u8(records + 1348u + 84u * (uint32)index, 0u);
        w_u8(records + 1768u + 84u * (uint32)index, 0u);
        menu[(2 * (index)) + 7].visible = 0u;
        menu[(2 * (index)) + 8].visible = 0u;
        menu[(2 * (index)) + 17].visible = 0u;
        menu[(2 * (index)) + 18].visible = 0u;
        menu[(2 * (index)) + 27].visible = 0u;
        menu[(2 * (index)) + 28].visible = 0u;
    }
    for (index = 0; index < 3; ++index)
        menu[index + 37].visible = 0u;
    for (index = 0; index < 3; ++index)
    {
        uint32 record_base = 928u + 420u * (uint32)index;
        uint32 menu_base = 125u + 160u * (uint32)index;
        selections[index] = (sint16)row->choice[(uint32)index] - 1;
        vehicle_draw_grid_lines((sint16)index, (sint16)(65 + 140 * index), 88, selections[index]);
        w_u8(records + record_base + 84u * (uint32)selections[index], 1u);
        menu[(2 * (selections[index])) + (10 * (index)) + 7].visible = 1u;
        menu[(2 * (selections[index])) + (10 * (index)) + 8].visible = 1u;
    }
    w_u16(0x800B69D4u, 11u);
    w_u16(0x800B6B70u, (uint16)selections[0]);
    w_u16(0x800B6A64u, (uint16)selections[0]);
    w_u16(0x800B69D6u, 16u);
    w_u16(0x800B6B72u, (uint16)selections[1]);
    w_u16(0x800B6A66u, (uint16)selections[1]);
    w_u16(0x800B69D8u, 21u);
    w_u16(0x800B6A8Au, 1u);
    w_u16(0x800B6B74u, (uint16)selections[2]);
    w_u16(0x800B6A68u, (uint16)selections[2]);
    w_u16(0x800B6B48u, 0u);
    return menu_move_sprite_rec(records + 84u * (uint32)((sint16)r_u16(0x800B69D6u) + (sint16)r_u16(0x800B6B72u)), 1, 10, 8, 1, 1, 0, 0, 0, 0);
}

sint32 vehicle_select_fn_8005fc28(CONTROLLER_STATE *input)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    sint16 original_column = (sint16)r_u16(0x800B6A8Au);
    uint32 active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)original_column) + (sint16)r_u16(0x800B6B70u + 2u * (uint32)original_column));
    sint32 index;

    FUNCTION_MARKER(0x8005FC28u, "MAIN.EXE");
    if (r_u16(active_record + 12u) == 257u)
    {
        if ((sint16)r_u16(0x800B6B48u) == 0)
            w_u8(active_record + 12u, 2u);
        goto draw_grids;
    }
    if (r_u8(active_record + 12u) != 2u || r_u8(0x800B4138u) == 0u)
    {
        if (r_u8(active_record + 12u) == 0u)
        {
            if ((sint16)r_u16(0x800B6B48u) != 0)
            {
                sint16 column = (sint16)r_u16(0x800B6A8Au);
                sint16 selection = (sint16)r_u16(0x800B6BD4u);
                sint16 base = column == 0 ? 7 : (column == 1 ? 17 : 27);
                w_u8(active_record + 4u, 0u);
                w_u16(0x800B6B48u, 0u);
                w_u16(0x800B6B70u + 2u * (uint32)column, (uint16)selection);
                active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)column) + selection);
                w_u8(active_record + 4u, 1u);
                for (index = 0; index < 5; ++index)
                {
                    menu[base + (2 * (index))].visible = 0u;
                    menu[base + (2 * (index)) + 1].visible = 0u;
                }
                menu[base + (2 * (selection))].visible = 1u;
                menu[base + (2 * (selection)) + 1].visible = 1u;
                menu[column + 37].visible = selection != (sint16)r_u16(0x800B6A64u + 2u * (uint32)column);
            }
            menu_move_sprite_rec(active_record, 1, 10, 8, 1, 1, 0, 0, 0, 0);
        }
        goto draw_grids;
    }
    if ((input->current & 0x40u) != 0u)
    {
        for (index = 0; index < 3; ++index)
            vehicle_custom_grid.choice[index] = (uint8)((uint8)(r_u16(0x800B6B70u + 2u * (uint32)index) + 1u));
        {
            uint16 value = (uint16)profile_get_grid_byte((sint16)r_u16(0x800B4238u), 3);
            // Original SB preserves the adjacent first limit
            vehicle_custom_grid.choice[3] = (uint8)value;
        }
        w_u16(0x800E0582u, 7u);
        w_u16(0x800E1B9Cu, 1u);
        w_u16(0x800E1BA4u, 0u);
        name_reels.code = (name_reels.code & 0xFFFF0000u) | (uint16)(32168u);
        w_u8(0x800E05ADu, (uint8)r_u16(0x800B4238u));
        return profile_commit_selection();
    }
    {
        sint32 movement = 0;
        sint16 column = (sint16)r_u16(0x800B6A8Au);
        sint16 selection;
        if ((input->current & 0x8000u) != 0u)
            movement = 1;
        if ((input->current & 0x2000u) != 0u)
            movement = 2;
        if (column == 1)
        {
            if (movement == 1)
            {
                w_u16(0x800B6AFAu, 9u);
                w_u16(0x800B6A8Au, 0u);
            }
            if (movement == 2)
            {
                w_u16(0x800B6AFAu, 9u);
                w_u16(0x800B6A8Au, 2u);
            }
        }
        else if ((column == 2 && movement == 1) || (column == 0 && movement == 2))
        {
            w_u16(0x800B6AFAu, 9u);
            w_u16(0x800B6A8Au, 1u);
        }
        column = (sint16)r_u16(0x800B6A8Au);
        selection = (sint16)r_u16(0x800B6B70u + 2u * (uint32)column);
        if ((input->current & 0x4000u) != 0u && selection > 0 && (sint16)r_u16(0x800B6A64u + 2u * (uint32)column) == selection - 1)
        {
            w_u16(0x800B6AFAu, 9u);
            w_u8(active_record + 12u, 1u);
            w_u16(0x800B6B48u, 1u);
            w_u16(0x800B6BD4u, (uint16)(selection - 1));
            if ((sint16)r_u16(0x800B6BD4u) == -1)
                w_u16(0x800B6BD4u, 4u);
        }
        if ((input->current & 0x1000u) != 0u && selection < 4 && selection == (sint16)r_u16(0x800B6A64u + 2u * (uint32)column))
        {
            w_u16(0x800B6AFAu, 9u);
            w_u8(active_record + 12u, 1u);
            w_u16(0x800B6B48u, 1u);
            w_u16(0x800B6BD4u, (uint16)(selection + 1));
            if ((sint16)r_u16(0x800B6BD4u) == 5)
                w_u16(0x800B6BD4u, 0u);
        }
    }
    if (original_column != (sint16)r_u16(0x800B6A8Au))
    {
        sint16 column = (sint16)r_u16(0x800B6A8Au);
        w_u8(active_record + 12u, 1u);
        active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)column) + (sint16)r_u16(0x800B6B70u + 2u * (uint32)column));
        menu_move_sprite_rec(active_record, 1, 10, 8, 1, 1, 0, 0, 0, 0);
    }

draw_grids:
    vehicle_draw_grid_lines(0, 65, 88, (sint16)r_u16(0x800B6B70u));
    vehicle_draw_grid_lines(1, 205, 88, (sint16)r_u16(0x800B6B72u));
    return vehicle_draw_grid_lines(2, 345, 88, (sint16)r_u16(0x800B6B74u));
}

sint32 vehicle_select_fn_80060228(void)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *selection = &profile_current()->grid[(uint32)(sint16)r_u16(0x800B4238u)];
    sint32 index;
    sint32 matches = 0;

    FUNCTION_MARKER(0x80060228u, "MAIN.EXE");
    for (index = 0; index < 5; ++index)
    {
        sint32 group;
        for (group = 0; group < 6; ++group)
            w_u8(records + 172u + 420u * (uint32)group + 84u * (uint32)index, 0u);
        menu[(2 * (index)) + 7].visible = 0u;
        menu[(2 * (index)) + 8].visible = 0u;
        menu[(2 * (index)) + 17].visible = 0u;
        menu[(2 * (index)) + 18].visible = 0u;
        menu[(2 * (index)) + 27].visible = 0u;
        menu[(2 * (index)) + 28].visible = 0u;
    }
    for (index = 0; index < 3; ++index)
    {
        w_u16(0x800B69B4u + 2u * (uint32)index, 0u);
        w_u16(0x800B6B54u + 2u * (uint32)index, 0u);
        menu[index + 37].visible = 0u;
    }
    menu[47].visible = 0u;
    for (index = 0; index < 3; ++index)
    {
        sint16 selected = (sint16)selection->choice[(uint32)index] - 1;
        uint32 record_offset = 1432u + 420u * (uint32)index;
        uint32 menu_offset = 125u + 144u * (uint32)index;
        w_u8(records + record_offset + 84u * (uint32)selected, 1u);
        menu[(2 * (selected)) + (9 * (index)) + 7].visible = 1u;
        menu[(2 * (selected)) + (9 * (index)) + 8].visible = 1u;
        if (index == 0)
        {
            w_u16(0x800B69D4u, 17u);
            w_u16(0x800B6B70u, (uint16)selected);
            w_u16(0x800B6A64u, (uint16)selected);
            if (selected + 1 == selection->limit[0])
                w_u16(0x800B6B54u, 1u);
        }
        else if (index == 1)
        {
            w_u16(0x800B69D6u, 22u);
            w_u16(0x800B6B72u, (uint16)selected);
            w_u16(0x800B6A66u, (uint16)selected);
            if (selected + 1 == selection->limit[1])
                w_u16(0x800B6B56u, 1u);
        }
        else
        {
            w_u16(0x800B69D8u, 27u);
            w_u16(0x800B6B74u, (uint16)selected);
            w_u16(0x800B6A68u, (uint16)selected);
            if (selected + 1 == selection->limit[2])
                w_u16(0x800B6B58u, 1u);
        }
    }
    w_u16(0x800B6A8Au, 1u);
    w_u16(0x800B6B48u, 0u);
    menu_move_sprite_rec(records + 84u * (uint32)((sint16)r_u16(0x800B69D6u) + (sint16)r_u16(0x800B6B72u)), 1, 10, 8, 1, 1, 0, 0, 0, 0);
    menu[1].visible = 0u;
    menu[3].visible = 0u;
    w_u8(records + 2860u, 0u);
    menu[41].visible = 0u;
    for (index = 0; index < 30; ++index)
        menu[index + 47].visible = 0u;
    for (index = 0; index < 3; ++index)
    {
        if ((sint16)r_u16(0x800B6A64u + 2u * (uint32)index) + 1 == selection->limit[(uint32)index])
            ++matches;
    }
    if (matches == 3)
    {
        menu[42].visible = 0u;
        menu[43].visible = 0u;
        w_u8(records + 2776u, 0u);
    }
    return matches;
}

sint32 vehicle_select_fn_80060704(CONTROLLER_STATE *input)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *row = &profile_current()->grid[r_u16(0x800B4238u)];
    sint16 original_column = (sint16)r_u16(0x800B6A8Au);
    uint32 active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)original_column) + (sint16)r_u16(0x800B6B70u + 2u * (uint32)original_column));
    sint32 total;
    sint32 index;

    FUNCTION_MARKER(0x80060704u, "MAIN.EXE");
    if (r_u16(active_record + 12u) == 257u)
    {
        if ((sint16)r_u16(0x800B6B48u) == 0)
            w_u8(active_record + 12u, 2u);
        goto update_summary;
    }
    if (r_u8(active_record + 12u) != 2u || r_u8(0x800B4138u) == 0u)
    {
        if (r_u8(active_record + 12u) == 0u)
        {
            if ((sint16)r_u16(0x800B6B48u) != 0)
            {
                sint16 column = (sint16)r_u16(0x800B6A8Au);
                sint16 selection = (sint16)r_u16(0x800B6BD4u);
                sint16 base = column == 0 ? 7 : (column == 1 ? 17 : 27);
                w_u8(active_record + 4u, 0u);
                w_u16(0x800B6B48u, 0u);
                w_u16(0x800B6B70u + 2u * (uint32)column, (uint16)selection);
                active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)column) + selection);
                w_u8(active_record + 4u, 1u);
                for (index = 0; index < 5; ++index)
                {
                    menu[base + (2 * (index))].visible = 0u;
                    menu[base + (2 * (index)) + 1].visible = 0u;
                }
                menu[base + (2 * (selection))].visible = 1u;
                menu[base + (2 * (selection)) + 1].visible = 1u;
            }
            menu_move_sprite_rec(active_record, 1, 10, 8, 1, 1, 0, 0, 0, 0);
        }
        goto update_summary;
    }

    {
        sint32 movement = 0;
        sint16 column = (sint16)r_u16(0x800B6A8Au);
        if ((input->current & 0x8000u) != 0u)
            movement = 1;
        if ((input->current & 0x2000u) != 0u)
            movement = 2;
        if (column == 1)
        {
            if (movement == 1 && (sint16)r_u16(0x800B6B54u) == 0)
            {
                w_u16(0x800B6A8Au, 0u);
                w_u16(0x800B6AFAu, 9u);
            }
            else if (movement == 2 && (sint16)r_u16(0x800B6B58u) == 0)
            {
                w_u16(0x800B6A8Au, 2u);
                w_u16(0x800B6AFAu, 9u);
            }
        }
        else if (column == 2 && movement == 1)
        {
            if ((sint16)r_u16(0x800B6B56u) == 0)
                w_u16(0x800B6A8Au, 1u);
            else if ((sint16)r_u16(0x800B6B54u) == 0)
                w_u16(0x800B6A8Au, 0u);
            if ((sint16)r_u16(0x800B6A8Au) != column)
                w_u16(0x800B6AFAu, 9u);
        }
        else if (column == 0 && movement == 2)
        {
            if ((sint16)r_u16(0x800B6B56u) == 0)
                w_u16(0x800B6A8Au, 1u);
            else if ((sint16)r_u16(0x800B6B58u) == 0)
                w_u16(0x800B6A8Au, 2u);
            if ((sint16)r_u16(0x800B6A8Au) != column)
                w_u16(0x800B6AFAu, 9u);
        }
    }

    total = (sint16)r_u16(0x800B69B4u) + (sint16)r_u16(0x800B69B6u) + (sint16)r_u16(0x800B69B8u);
    if ((input->current & 0x40u) != 0u)
    {
        sint16 selected = (sint16)r_u16(0x800B6B36u);
        sint32 other_count = 0;
        for (index = 0; index < 3; ++index)
        {
            if (index != selected && (sint16)r_u16(0x800B69B4u + 2u * (uint32)index) != 0)
                ++other_count;
        }
        if (total == 0)
            w_u16(0x800B6AFAu, 5u);
        else
        {
            w_u16(0x800B6AFAu, 4u);
            if (other_count == 1)
            {
                if (row->limit[selected] >= r_u8(0x800B6B5Au))
                {
                    row->choice[selected] = (uint8)(r_u8(0x800B6B5Au));
                    for (index = 0; index < 3; ++index)
                    {
                        if (index != selected && r_u16(0x800B69B4u + 2u * (uint32)index) != 0u)
                        {
                            sint32 value = (sint32)row->choice[(uint32)index] - 2;
                            row->choice[(uint32)index] = (uint8)((uint8)(value > 0 ? value : 1));
                        }
                    }
                }
                else
                    w_u16(0x800B6AFAu, 5u);
            }
            else if (other_count == 2)
            {
                if (row->limit[selected] >= r_u8(0x800B6B5Au))
                {
                    row->choice[selected] = (uint8)(r_u8(0x800B6B5Au));
                    for (index = 0; index < 3; ++index)
                    {
                        if (index != selected && r_u16(0x800B69B4u + 2u * (uint32)index) != 0u)
                        {
                            sint32 value = (sint32)row->choice[(uint32)index] - 1;
                            row->choice[(uint32)index] = (uint8)((uint8)(value > 0 ? value : 1));
                        }
                    }
                }
                else
                    w_u16(0x800B6AFAu, 5u);
            }
            if ((sint16)r_u16(0x800B4238u) >= 3)
            {
                for (index = 0; index < 3; ++index)
                    row->offset[(uint32)index] = (uint8)(row->choice[(uint32)index] == row->limit[(uint32)index] ? 2u : 0u);
            }
            w_u16(0x800B413Eu, 6u);
        }
    }

    if ((input->current & 0x20u) != 0u)
    {
        sint32 matched = 0;
        sint16 column = (sint16)r_u16(0x800B6A8Au);
        sint32 previous_total = total;
        w_u16(0x800B6AFAu, 4u);
        for (index = 0; index < 3; ++index)
        {
            if ((sint16)r_u16(0x800B6A64u + 2u * (uint32)index) + 1 == row->limit[(uint32)index])
                ++matched;
        }
        if (matched != 3)
        {
            if (r_u16(0x800B69B4u + 2u * (uint32)column) != 0u)
            {
                w_u16(0x800B69B4u + 2u * (uint32)column, 0u);
                if ((sint16)r_u16(0x800B6B36u) == column)
                {
                    w_u16(0x800B69B4u, 0u);
                    w_u16(0x800B69B6u, 0u);
                    w_u16(0x800B69B8u, 0u);
                    w_u16(0x800B6B58u, 0u);
                    w_u16(0x800B6B56u, 0u);
                    w_u16(0x800B6B54u, 0u);
                    for (index = 0; index < 3; ++index)
                    {
                        if ((sint16)r_u16(0x800B6A64u + 2u * (uint32)index) + 1 == row->limit[(uint32)index])
                            w_u16(0x800B6B54u + 2u * (uint32)index, 1u);
                    }
                }
            }
            else
            {
                w_u16(0x800B69B4u + 2u * (uint32)column, (uint16)((sint16)r_u16(0x800B6B70u + 2u * (uint32)column) + 1));
                for (index = 0; index < 3; ++index)
                {
                    if ((sint16)r_u16(0x800B6A64u + 2u * (uint32)index) + 1 == row->limit[(uint32)index])
                        w_u16(0x800B6B54u + 2u * (uint32)index, 0u);
                }
            }
            total = (sint16)r_u16(0x800B69B4u) + (sint16)r_u16(0x800B69B6u) + (sint16)r_u16(0x800B69B8u);
            if (total == 0)
            {
                w_u16(0x800B6B36u, r_u16(0x800B6A8Au));
                w_u16(0x800B6B54u + 2u * r_u16(0x800B6A8Au), 0u);
                for (index = 0; index < 3; ++index)
                {
                    if (index != (sint16)r_u16(0x800B6A8Au) && r_u16(0x800B6A64u + 2u * (uint32)index) == 0u)
                        w_u16(0x800B6B54u + 2u * (uint32)index, 1u);
                }
            }
            (void)previous_total;
        }
    }

    if (original_column != (sint16)r_u16(0x800B6A8Au))
    {
        w_u8(active_record + 12u, 1u);
        active_record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * r_u16(0x800B6A8Au)) + (sint16)r_u16(0x800B6B70u + 2u * r_u16(0x800B6A8Au)));
        menu_move_sprite_rec(active_record, 1, 10, 8, 1, 1, 0, 0, 0, 0);
    }
    for (index = 0; index < 3; ++index)
    {
        menu[index + 37].visible = 0u;
        menu[index + 44].visible = 0u;
        if (r_u16(0x800B69B4u + 2u * (uint32)index) != 0u)
        {
            if (index == (sint16)r_u16(0x800B6B36u))
                menu[index + 44].visible = 1u;
            else
                menu[index + 37].visible = 1u;
        }
    }

update_summary:
    total = (sint16)r_u16(0x800B69B4u) + (sint16)r_u16(0x800B69B6u) + (sint16)r_u16(0x800B69B8u);
    if (total != 0)
    {
        menu[1].visible = 1u;
        menu[42].visible = 0u;
        menu[3].visible = 1u;
        menu[43].visible = 0u;
        menu[41].visible = 1u;
        w_u8(records + 2860u, 1u);
    }
    else
    {
        menu[1].visible = 0u;
        menu[42].visible = 1u;
        menu[3].visible = 0u;
        menu[43].visible = 1u;
        menu[41].visible = 0u;
        w_u8(records + 2860u, 0u);
    }
    {
        uint32 selected_record = records + 84u * r_u16(0x800B6A6Au);
        sint32 matched = 0;
        if (r_u8(selected_record + 4u) == 1u && r_u16(selected_record + 12u) == 257u)
            w_u8(selected_record + 12u, 2u);
        for (index = 0; index < 3; ++index)
        {
            if ((sint16)r_u16(0x800B6A64u + 2u * (uint32)index) + 1 == row->limit[(uint32)index])
                ++matched;
        }
        if (matched == 3)
        {
            menu[42].visible = 0u;
            menu[43].visible = 0u;
            w_u8(records + 2776u, 0u);
        }
    }
    if (total != (sint16)r_u16(0x800B6BE0u))
    {
        w_u16(0x800B6BE0u, (uint16)total);
        for (index = 0; index < 5; ++index)
        {
            w_u8(records + 172u + 84u * (uint32)index, 0u);
            w_u8(records + 592u + 84u * (uint32)index, 0u);
            w_u8(records + 1012u + 84u * (uint32)index, 0u);
        }
        for (index = 0; index < 30; ++index)
            menu[index + 47].visible = 0u;
        if (total != 0)
        {
            sint32 other_total = 0;
            sint16 selected = (sint16)r_u16(0x800B6B36u);
            sint16 maximum;
            for (index = 0; index < 3; ++index)
            {
                if (index != selected)
                    other_total += (sint16)r_u16(0x800B69B4u + 2u * (uint32)index);
            }
            maximum = (sint16)(r_u16(0x800B69B4u + 2u * (uint32)selected) + (other_total > 2 ? 1 : 0));
            if (maximum >= 6)
                maximum = 5;
            if (maximum > row->limit[(uint32)selected])
                maximum = row->limit[(uint32)selected];
            w_u16(0x800B6B5Au, (uint16)maximum);
            w_u16(0x800B6A6Au, (uint16)(5 * selected + maximum + 1));
            w_u8(records + 4u + 84u * r_u16(0x800B6A6Au), 1u);
            menu[(10 * (selected)) + (2 * (maximum)) + 45].visible = 1u;
            menu[(10 * (selected)) + (2 * (maximum)) + 46].visible = 1u;
        }
    }
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + 84u * (uint32)((sint16)r_u16(0x800B69D4u + 2u * (uint32)index) + (sint16)r_u16(0x800B6B70u + 2u * (uint32)index));
        uint32 primitive = r_u32(record) + 120u * (uint32)(sint16)r_u16(record + 6u);
        sint32 disabled = (sint16)r_u16(0x800B6B54u + 2u * (uint32)index);
        w_u16(primitive + 74u, (uint16)vehicle_select_pack_tpage(1, 3, r_u32(primitive + 32u), r_u32(primitive + 36u)));
        w_u16(primitive + 66u, (uint16)vehicle_select_pack_clut(r_u32(primitive + 24u), r_u32(primitive + 28u)));
        vehicle_select_set_prim_alternate(primitive + 52u, disabled);
        w_u8(primitive + 56u, disabled != 0 ? 32u : 128u);
        w_u8(primitive + 57u, disabled != 0 ? 32u : 128u);
        w_u8(primitive + 58u, disabled != 0 ? 32u : 128u);
        DrawSync(0);
    }
    vehicle_draw_grid_lines(0, 65, 78, (sint16)r_u16(0x800B6B70u));
    vehicle_draw_grid_lines(1, 205, 78, (sint16)r_u16(0x800B6B72u));
    vehicle_draw_grid_lines(2, 345, 78, (sint16)r_u16(0x800B6B74u));
    if (total != 0)
        return vehicle_draw_grid_lines((sint16)r_u16(0x800B6B36u), 345, 160, (sint16)r_u16(0x800B6B5Au) - 1);
    return total;
}

sint32 vehicle_select_fn_80061754(void)
{
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    uint32 table = r_u32(0x80097D9Cu + 24u * (uint32)(sint16)r_u16(0x800B413Cu));
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_COURSE *row = &profile_data->progress.courses[profile_data->level][profile_data->course];
    uint8 state = row->attempts;
    uint8 visible = state != 0u;
    sint32 index;

    FUNCTION_MARKER(0x80061754u, "MAIN.EXE");
    if (visible != 0u)
    {
        w_u16(0x800B4242u, 12u);
        w_u16(0x800B6AFAu, (uint16)-1);
        w_u16(0x800B4240u, 1u);
        w_u16(records + 260u, 159u);
    }
    else
    {
        w_u16(0x800B4240u, 0u);
        w_u16(records + 260u, 60u);
    }
    w_u8(records + 172u, visible);
    w_u8(records + 340u, visible);
    menu[3].visible = visible;
    menu[4].visible = visible;
    for (index = 0; index < 3; ++index)
        menu[index + 5].visible = 0u;
    if (state != 0u)
        menu[state + 4].visible = 1u;
    w_u16(table + 4u, visible);
    sprite_apply_inset(records + 168u);
    return sprite_disable_semitransparency(records + 168u);
}

sint32 vehicle_select_fn_800618d0(CONTROLLER_STATE *input)
{
    sint32 result = 0;

    FUNCTION_MARKER(0x800618D0u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B4240u) != 0)
    {
        uint32 record = r_u32(0x800B6A74u) + 168u;
        if (r_u8(record + 12u) == 0u)
            menu_move_sprite_rec(record, 1, 8, 6, 1, 1, 0, 0, 0, 0);
        result = sprite_disable_semitransparency(record);
    }
    if (r_u8(0x800B4138u) != 0u && (sint16)r_u16(0x800B6AA0u) == 0)
    {
        PLAYER_PROFILE *profile_data = profile_current();
        PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
        if (state->attempts != 0u)
        {
            if ((input->current & 0x10u) != 0u)
            {
                w_u16(0x800B6AFAu, 2u);
                w_u16(0x800B413Eu, 10u);
            }
            if ((input->current & 0x40u) != 0u)
            {
                TEXT_RECORD *record = text_hud;
                w_u16(0x800B6AFAu, 1u);
                record[0].x = 339u;
                record[0].y = 195u;
                return profile_commit_selection();
            }
        }
        else if ((input->current & 0x40u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 10u);
            state->result = (uint8)(0u);
        }
        w_u16(0x800B4084u, 1u);
        w_u32(r_u32(0x80097D94u) + 24u * r_u16(0x800B413Eu), 0u);
    }
    return result;
}

sint32 vehicle_select_fn_80061a90(void)
{
    PROFILE_GRID *row = &profile_current()->grid[r_u8(0x800E05ADu)];
    sint32 index;
    sint32 result = r_u8(0x800E05ADu) < 3u;

    FUNCTION_MARKER(0x80061A90u, "MAIN.EXE");
    if (r_u8(0x800E05ADu) < 3u)
    {
        for (index = 0; index < 3; ++index)
        {
            sint16 availability_index = index == 0 ? 5 : (index == 1 ? 3 : 4);
            uint8 state = 0u;
            if (row->limit[(uint32)index] == row->choice[(uint32)index])
                state = (uint8)(profile_is_unlocked((sint16)(availability_index)) != 0 ? 2 : 1);
            row->offset[(uint32)index] = (uint8)(state);
            result = (index + 1) << 16;
        }
    }
    return result;
}

sint32 vehicle_select_fn_80061bb4(void)
{
    sint16 column = r_u8(0x800E05B4u);
    sint16 profile_row = r_u8(0x800E05ADu);
    PROFILE_GRID *row = &profile_current()->grid[(uint32)profile_row];

    sint32 index;

    FUNCTION_MARKER(0x80061BB4u, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
        text_set_menu_visible(index + 2, index == column);
    for (index = 6; index < 10; ++index)
        text_set_menu_visible(index, 0);
    if (row->choice[column] != row->limit[column])
        row->choice[column] = (uint8)((uint8)(row->choice[column] + 1u));
    if (row->choice[column] == row->limit[column])
        text_set_menu_visible(9, 1);
    else
        text_set_menu_visible((sint32)row->choice[column] + 4, 1);
    if (row->choice[column] == row->limit[column])
    {
        uint8 state = 2u;
        if (profile_row < 3)
        {
            sint16 availability_index = column == 0 ? 5 : (column == 1 ? 3 : 4);
            if (profile_is_unlocked((sint16)(availability_index)) == 0)
                state = 1u;
        }
        row->offset[column] = (uint8)(state);
    }
    return profile_build_vehicle_grid(2, profile_row, 383, 169);
}

sint32 vehicle_select_fn_80061d7c(sint16 value)
{
    FUNCTION_MARKER(0x80061D7Cu, "MAIN.EXE");
    return value;
}

sint32 vehicle_select_fn_80061d88(void)
{
    PROFILE_GRID *row;

    FUNCTION_MARKER(0x80061D88u, "MAIN.EXE");
    row = profile_grid_row(r_u8(0x800E05ADu));
    row->offset[r_u8(0x800E05B4u)] = (uint8)(2u);
    w_u8(r_u32(0x800B6A74u) + 2608u, 0u);
    return profile_build_vehicle_grid(2, (sint16)r_u16(0x800B4266u), 383, 169);
}

sint32 vehicle_select_fn_80061de4(CONTROLLER_STATE *input)
{
    sint32 result = 1;

    FUNCTION_MARKER(0x80061DE4u, "MAIN.EXE");
    if ((input->pressed & 0x40u) != 0u)
    {
        w_u16(0x800B6AFAu, 1u);
        result = 11;
        if ((sint16)r_u16(0x800B6A58u) == 0)
        {
            result = 3;
            if ((sint16)r_u16(0x800B6A80u) == 0)
                result = 5;
        }
        w_u16(0x800B413Eu, (uint16)result);
    }
    return result;
}

void vehicle_select_fn_80061e38(void)
{
    FUNCTION_MARKER(0x80061E38u, "MAIN.EXE");
}

sint32 vehicle_select_fn_80061e40(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_GRID *row = &profile_current()->grid[r_u8(0x800E05ADu)];
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    PROFILE_COURSE *state_row = &profile_data->progress.courses[profile_data->level][profile_data->course];
    sint16 incomplete = 0;
    sint32 visible;
    sint32 index;

    FUNCTION_MARKER(0x80061E40u, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
    {
        if (row->offset[(uint32)index] != 2u)
            ++incomplete;
    }
    visible = state_row->attempts != 0u && state_row->result != 2u && incomplete != 0;
    if (visible != 0)
    {
        w_u16(0x800B4242u, 12u);
        w_u16(0x800B4240u, 1u);
        w_u16(records + 260u, 136u);
    }
    else
    {
        w_u16(0x800B4240u, 0u);
        w_u16(records + 260u, 60u);
    }
    w_u8(records + 172u, (uint8)visible);
    w_u8(records + 340u, (uint8)visible);
    text_set_menu_visible(4, (sint8)visible);
    text_set_menu_visible(12, (sint8)(1 - visible));
    text_set_menu_visible(3, (sint8)visible);
    visible = (sint16)r_u16(0x800E0582u) != 1 && profile_data->level_result != 0u;
    menu[5].visible = 0u;
    menu[6].visible = 0u;
    menu[7].visible = 0u;
    menu[8].visible = 0u;
    menu[9].visible = 0u;
    menu[11].visible = 0u;
    if (visible != 0)
    {
        if (profile_data->max_level == 1u)
        {
            menu[5].visible = 1u;
            menu[8].visible = 1u;
            menu[5].y = 130u;
            menu[8].y = 150u;
            menu[10].y = 170u;
        }
        else if (profile_data->max_level == 2u)
        {
            menu[6].visible = 1u;
            menu[9].visible = 1u;
            menu[6].y = 130u;
            menu[9].y = 150u;
            menu[10].y = 170u;
        }
        else if (profile_data->max_level == 3u)
        {
            menu[7].visible = 1u;
            menu[11].visible = 1u;
            menu[7].y = 130u;
            menu[11].y = 150u;
            menu[10].y = 170u;
        }
    }
    menu[10].visible = (uint8)visible;
    return (sint32)menu;
}

sint32 vehicle_select_fn_80062144(CONTROLLER_STATE *input)
{
    sint32 result = r_u8(0x800B4138u);

    FUNCTION_MARKER(0x80062144u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B4240u) != 0)
    {
        uint32 record = r_u32(0x800B6A74u) + 168u;
        if (r_u8(record + 12u) == 0u)
            menu_move_sprite_rec(record, 1, 8, 6, 1, 1, 0, 0, 0, 0);
    }
    if (r_u8(0x800B4138u) != 0u)
    {
        result = (sint16)r_u16(0x800B6AA0u);
        if (result == 0)
        {
            PLAYER_PROFILE *profile_data = profile_current();
            PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
            sint32 incomplete = 0;
            sint32 index;
            sint32 enabled = state->attempts;
            if (state->result == 2u)
                enabled = 0;
            for (index = 0; index < 3; ++index)
            {
                if (profile_current()->grid[r_u8(0x800E05ADu)].offset[index] != 2u)
                    ++incomplete;
            }
            if (enabled != 0)
            {
                if ((input->current & 0x10u) != 0u)
                {
                    w_u16(0x800B6AFAu, 1u);
                    w_u16(0x800B413Eu, 10u);
                }
                if ((input->current & 0x40u) != 0u)
                {
                    w_u16(0x800B6AFAu, 1u);
                    if (incomplete != 0)
                    {
                        TEXT_RECORD *record = text_hud;
                        record[0].x = 319u;
                        record[0].y = 195u;
                        return profile_commit_selection();
                    }
                }
            }
            else if ((input->current & 0x40u) != 0u)
            {
                w_u16(0x800B6AFAu, 1u);
                if (state->result != 2u)
                    state->result = (uint8)(0u);
            }
            w_u16(0x800B413Eu, 10u);
            w_u16(0x800B4084u, 1u);
            result = 24 * (sint16)r_u16(0x800B413Eu);
            w_u32(r_u32(0x80097D94u) + (uint32)result, 0u);
        }
    }
    return result;
}

void vehicle_select_fn_8006238c(void)
{
    FUNCTION_MARKER(0x8006238Cu, "MAIN.EXE");
}

sint32 vehicle_select_fn_80062394(void)
{
    sint32 profile_index;
    sint32 row_index;
    sint32 column;
    sint32 result = 0;

    FUNCTION_MARKER(0x80062394u, "MAIN.EXE");
    for (profile_index = 0; profile_index < 3; ++profile_index)
    {
        for (row_index = 0; row_index < 10; ++row_index)
        {
            if (row_index >= 3 && row_index < 9)
            {
                PROFILE_GRID *row = &profile_at((uint32)profile_index)->grid[row_index];
                for (column = 0; column < 4; ++column)
                {
                    if (row->choice[(uint32)column] == row->limit[(uint32)column])
                        row->offset[(uint32)column] = (uint8)(2u);
                }
            }
        }
        result = profile_index + 1 < 3;
    }
    return result;
}

sint32 profile_cell_dispatch_complete(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint8 row_index = profile_data->level;
    uint8 column = profile_data->course;
    PROFILE_COURSE *row = &profile_data->progress.courses[row_index][column];

    FUNCTION_MARKER(0x80062478u, "MAIN.EXE");
    if (row->result == 2u)
        return scene_clear_group_item_flags(3);
    return 2;
}

sint32 profile_index_map(void)
{
    FUNCTION_MARKER(0x800624D0u, "MAIN.EXE");
    if (r_u8(0x800E0595u) == 0u)
        return 4;
    if (r_u8(0x800E0595u) == 1u)
        return 2;
    if (r_u8(0x800E0595u) == 2u)
        return 1;
    return 0;
}

sint32 vehicle_select_init_player_profile(uint32 slot)
{
    PLAYER_PROFILE *native = profile_at(slot);
    sint32 row;
    sint32 index;

    FUNCTION_MARKER(0x80062530u, "MAIN.EXE");
    native->reward_flags = (uint8)(0u);
    native->max_level = 0u;
    native->level = (uint8)(0u);
    native->course = (uint8)(0u);
    for (row = 0; row < 3; ++row)
    {
        for (index = 0; index < 3; ++index)
            native->progress.courses[(uint32)row][(uint32)index].state = (uint8)(4u);
        for (index = 3; index < 6; ++index)
            native->progress.courses[(uint32)row][(uint32)index].state = (uint8)(5u);
        for (index = 0; index < 6; ++index)
        {
            native->progress.courses[(uint32)row][(uint32)index].attempts = (uint8)(0u);
            native->progress.courses[(uint32)row][(uint32)index].result = (uint8)(0u);
        }
    }
    for (index = 0; index < 10; ++index)
        native->availability[index] = 0u;
    native->availability[0] = 1u;
    native->availability[1] = 1u;
    native->availability[2] = 1u;
    profile_copy_grid_defaults(native);
    return 10 << 16;
}

sint32 vehicle_select_fn_800630f8(void)
{
    PSX_RECT rectangle;
    sint32 result;

    FUNCTION_MARKER(0x800630F8u, "MAIN.EXE");
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.w = (sint16)mdec_scale_output_width(r_u16(0x800CD56Au));
    rectangle.h = 256;
    ClearImage(&rectangle, 0u, 0u, 0u);
    rectangle.y = 256;
    result = ClearImage(&rectangle, 0u, 0u, 0u);
    return result;
}
