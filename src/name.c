#include "input.h"
#include "display.h"
#include "name.h"
#include "global.h"
#include "menu.h"
#include "profile.h"
#include "sprite.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>

NAME_EDITOR name_editor;
NAME_REELS name_reels;

typedef struct
{
    uint32 angle;
    uint16 glyph;
    uint16 radial;
    uint16 depth;
    uint16 x;
    uint16 y;
} NAME_WHEEL_SLOT;

typedef struct
{
    uint16 width;
    uint16 height;
} NAME_WHEEL_METRIC;

typedef struct
{
    NAME_WHEEL_SLOT slots[15];
    uint16 order[15];
    uint16 sorted_glyphs[15];
    NAME_WHEEL_METRIC glyphs[37];
} NAME_WHEEL;

static NAME_WHEEL name_wheel;
static const char name_alphabet[] = "abcdefghijklmnopqrstuvwxyz 0123456789abcdefghijklmno";

typedef struct
{
    char text[5];
    uint8 reserved;
} PLAYER_NAME;

static PLAYER_NAME player_names[5];

char *name_player_text(uint32 slot)
{
    return player_names[slot].text;
}

// Native initialization extracted from MAIN.EXE game_defaults at 0x80053A60
void name_reset_players(void)
{
    static const char defaults[5][4] = {{'p', 'l', ' ', '1'}, {'p', 'l', ' ', '2'}, {'p', 'l', ' ', '3'}, {'p', 'l', ' ', '4'}, {'p', 'l', ' ', '5'}};
    uint32 slot;
    uint32 character;

    for (slot = 0u; slot < 5u; ++slot)
    {
        for (character = 0u; character < 4u; ++character)
            player_names[slot].text[character] = defaults[slot][character];
        player_names[slot].text[4] = 0;
    }
}

// Native name codec used by MAIN.EXE save gather/scatter at 0x8004B644/0x8004B6BC
uint8 *name_save_byte(uint32 offset)
{
    uint32 field;
    PLAYER_NAME *name;

    if (offset < 4984u || offset >= 5014u)
        return NULL;
    field = offset - 4984u;
    name = &player_names[field / 6u];
    field %= 6u;
    return field < 5u ? (uint8 *)&name->text[field] : &name->reserved;
}

// Native ring helper used by name_update_wheel (MAIN.EXE 0x800581E8)
static void name_fill_ring(sint32 start)
{
    sint32 index;
    sint32 value;

    for (index = 0; index < 14; ++index)
    {
        value = (sint16)(start + index);
        name_wheel.slots[index].glyph = (uint16)value;
        if (value >= 37)
            name_wheel.slots[index].glyph = (uint16)(value - 37);
    }
    value = (sint16)(start - 1);
    name_wheel.slots[14].glyph = (uint16)value;
    if (value < 0)
        name_wheel.slots[14].glyph = (uint16)(value + 37);
    value = (sint16)name_wheel.slots[14].glyph;
    if (value >= 37)
        name_wheel.slots[14].glyph = (uint16)(value - 37);
}

sint32 name_apply_player_name_cheats(void)
{
    static const uint32 mode_strings[3][6] = {{0x800B4150u, 0x800B4158u, 0x800B4160u, 0x800B4168u, 0x800B4170u, 0x800B4178u}, {0x800B4180u, 0x800B4188u, 0x800B4190u, 0x800B4198u, 0x800B41A0u, 0x800B41A8u}, {0x800B41B0u, 0x800B41B8u, 0x800B41C0u, 0x800B41C8u, 0x800B41D0u, 0x800B41D8u}};
    sint32 result = (sint16)r_u16(0x800E0582u);

    FUNCTION_MARKER(0x8005056Cu, "MAIN.EXE");
    if (result != 0)
        return result;
    result = 1;
    if (r_u8(0x800E058Eu) == 1u)
    {
        PLAYER_PROFILE *profile_data = profile_current();
        sint32 selected_mode = -1;
        sint32 selected_count = 0;
        sint32 mode;
        sint32 count;

        for (mode = 0; mode < 3; ++mode)
            for (count = 0; count < 6; ++count)
                if (name_player_name_matches(0, mode_strings[mode][count]) != 0)
                {
                    selected_mode = mode;
                    selected_count = count + 1;
                }
        if (selected_mode != -1)
        {
            w_u16(0x800B6AFAu, 3u);
            profile_data->course = (uint8)((uint8)(selected_count - 1));
            profile_data->level = (uint8)((uint8)selected_mode);
            profile_data->max_level = (uint8)selected_mode;
            for (count = 0; count < selected_count - 1; ++count)
                profile_data->progress.courses[(uint32)selected_mode][(uint32)count].state = (uint8)(6u);
            profile_data->progress.courses[(uint32)selected_mode][(uint32)count].state = (uint8)(4u);
        }
        if (name_player_name_matches(0, 0x800B41E0u) != 0)
        {
            w_u16(0x800B6AFAu, 3u);
            profile_data->course = (uint8)(5u);
            profile_data->level = (uint8)(0u);
            profile_data->max_level = 0u;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[0u][(uint32)count].state = (uint8)(6u);
        }
        if (name_player_name_matches(0, 0x800B41E8u) != 0)
        {
            w_u16(0x800B6AFAu, 3u);
            profile_data->course = (uint8)(5u);
            profile_data->level = (uint8)(1u);
            profile_data->max_level = 1u;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[1u][(uint32)count].state = (uint8)(6u);
        }
        if (name_player_name_matches(0, 0x800B41F0u) != 0)
        {
            w_u16(0x800B6AFAu, 3u);
            profile_data->course = (uint8)(5u);
            profile_data->level = (uint8)(2u);
            profile_data->max_level = 2u;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[2u][(uint32)count].state = (uint8)(6u);
        }
        if (name_player_name_matches(0, 0x800B41F8u) != 0)
        {
            w_u16(0x800B6AFAu, 3u);
            profile_data->course = (uint8)(1u);
            profile_data->level = (uint8)(3u);
            profile_data->max_level = 3u;
        }
        if (name_player_name_matches(0, 0x800B4200u) != 0)
        {
            w_u16(0x800B6AFAu, 3u);
            for (count = 0; count < 10; ++count)
                profile_current()->availability[count] = 1u;
        }
        {
            uint16 flags = r_u16(0x800E0588u);
            if ((flags & 4u) != 0u)
                w_u16(0x800E0588u, (uint16)(flags - 4u));
        }
        if (name_player_name_matches(0, 0x800B4208u) != 0)
        {
            uint16 flags = r_u16(0x800E0588u);
            w_u16(0x800B6AFAu, 3u);
            w_u16(0x800E0588u, (uint16)(flags + 4u));
        }
        {
            uint16 flags = r_u16(0x800E0588u);
            if ((flags & 16u) != 0u)
                w_u16(0x800E0588u, (uint16)(flags - 16u));
        }
        if (name_player_name_matches(0, 0x800B4210u) != 0)
        {
            uint16 flags;

            w_u16(0x800B6AFAu, 3u);
            flags = r_u16(0x800E0588u);
            w_u16(0x800B6B86u, 0u);
            w_u16(0x800B6AA0u, 5u);
            w_u16(0x800E0588u, (uint16)(flags + 16u));
            text_set_hud_visible(0, 1);
            w_u16(0x800B413Eu, 25u);
        }
        {
            uint16 flags = r_u16(0x800E0588u);
            if ((flags & 8u) != 0u)
                w_u16(0x800E0588u, (uint16)(flags - 8u));
        }
        if (name_player_name_matches(0, 0x800B4218u) != 0)
        {
            uint16 flags = r_u16(0x800E0588u);
            w_u16(0x800B6AFAu, 3u);
            if ((flags & 0x20u) != 0u)
                w_u16(0x800E0588u, (uint16)(flags + 8u));
        }
        if (name_player_name_matches(0, 0x800B4220u) != 0)
            w_u16(0x800B6AFAu, 3u);
        {
            uint16 flags = r_u16(0x800E0588u);
            if ((flags & 64u) != 0u)
                w_u16(0x800E0588u, (uint16)(flags - 64u));
        }
        if (name_player_name_matches(0, 0x800B4228u) != 0)
        {
            uint16 flags = r_u16(0x800E0588u);
            w_u16(0x800B6AFAu, 3u);
            w_u16(0x800E0588u, (uint16)(flags + 64u));
        }
        return profile_reset_grid_flags();
    }
    return result;
}

sint32 name_refresh_menu_text(void)
{
    sint32 index;
    uint32 value = 0u;

    FUNCTION_MARKER(0x80057B60u, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        TEXT_RECORD *records = text_menu;
        sint32 selection = (sint16)name_editor.player_slot;
        char *destination = records[index + 5].text;

        value = (uint8)name_player_text((uint32)selection)[index];
        destination[0] = (uint8)value;
    }
    return (sint32)value;
}

sint32 name_player_name_matches(sint16 index, uint32 value)
{
    const char *source = name_player_text((uint32)(sint32)index);
    sint32 offset;

    FUNCTION_MARKER(0x80057BC8u, "MAIN.EXE");
    for (offset = 0; offset < 4; ++offset)
    {
        if ((uint8)source[offset] != r_u8(value + (uint32)offset))
            return 0;
    }
    return 1;
}

sint32 name_copy_player_name_bytes(uint16 index, char *output)
{
    const char *source;
    sint32 offset;
    uint32 value = 0u;

    FUNCTION_MARKER(0x80057C4Cu, "MAIN.EXE");
    if (index >= 5u)
        abort();
    source = name_player_text(index);
    for (offset = 0; offset < 4; ++offset)
    {
        value = (uint8)source[offset];
        output[(uint32)offset] = (uint8)value;
    }
    return (sint32)value;
}

void name_commit_chars(void)
{
    sint32 index;

    FUNCTION_MARKER(0x80057CC0u, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        TEXT_RECORD *records = text_menu;
        sint32 selection = (sint16)name_editor.player_slot;
        char *source = records[index + 5].text;
        uint8 value = (uint8)source[0];

        name_player_text((uint32)selection)[index] = (char)value;
    }
}

sint32 name_reset_wheel(void)
{
    uint16 configuration = r_u16(0x800B413Cu);
    uint32 records;
    sint32 index;

    FUNCTION_MARKER(0x80057D28u, "MAIN.EXE");
    name_editor.player_slot = (uint16)(0u);
    name_editor.phase = (uint16)(NAME_EDITOR_IDLE);
    name_editor.glyph = (uint16)(0u);
    menu_pulse.brightness = 0u;
    menu_pulse.falling = 0u;
    name_editor.pending_glyph = (uint16)((uint16)-1);
    w_u16(0x800B4142u, configuration);
    name_build_glyphs();
    records = r_u32(0x800B6A74u);
    for (index = 0; index < 37; ++index)
    {
        uint32 record = records + 504u + 84u * (uint32)index;
        sint32 sprite_index = (sint16)r_u16(record + 6u);
        uint32 sprite = r_u32(record) + 120u * (uint32)sprite_index;

        name_wheel.glyphs[index].width = r_u16(sprite + 16u);
        sprite_index = (sint16)r_u16(record + 6u);
        sprite = r_u32(record) + 120u * (uint32)sprite_index;
        name_wheel.glyphs[index].height = r_u16(sprite + 20u);
    }
    configuration = name_editor.glyph;
    for (index = 0; index < 15; ++index)
    {
        sint32 value = (sint16)(configuration + index - 2);

        name_wheel.order[index] = (uint16)index;
        name_wheel.slots[index].glyph = (uint16)value;
        if (value < 0)
            name_wheel.slots[index].glyph = (uint16)(value + 37);
        value = (sint16)name_wheel.slots[index].glyph;
        if (value >= 37)
            name_wheel.slots[index].glyph = (uint16)(value - 37);
    }
    w_u16(0x800B6A60u, 34u);
    for (index = 0; index < 37; ++index)
        w_u8(r_u32(0x800B6A74u) + 508u + 84u * (uint32)index, 0u);
    name_refresh_menu_text();
    return name_update_wheel(&input_controllers[0], 0u, 0u, 0u);
}

sint32 name_find_glyph(sint8 value)
{
    sint32 index;

    FUNCTION_MARKER(0x80057EF4u, "MAIN.EXE");
    for (index = 0; index < 37; ++index)
    {
        if ((uint8)name_alphabet[index] == (uint8)value)
            return index;
    }
    return 37;
}

sint32 name_layout_wheel(sint16 offset_x, sint16 offset_y)
{
    sint32 index;

    FUNCTION_MARKER(0x80057F40u, "MAIN.EXE");
    for (index = 0; index < 15; ++index)
    {
        NAME_WHEEL_SLOT *slot = &name_wheel.slots[index];
        sint32 phase = (36 * index + (sint16)name_editor.wheel_angle - 90) * 4096 / 540;
        sint32 sine;
        sint32 cosine;
        sint32 radial;
        sint32 x;
        sint32 y;

        slot->angle = (uint32)phase;
        if (phase < 0)
            slot->angle = (uint32)(phase + 4096);
        phase = (sint32)slot->angle;
        if (phase >= 4096)
            slot->angle = (uint32)(phase - 4096);
        phase = (sint32)slot->angle;
        /* PsyQ rsin uses the original integer lookup table */
        sine = rsin(phase);
        radial = (sint16)(uint16)(((uint32)(35 * sine)) >> 11);
        slot->radial = (uint16)radial;
        cosine = rcos(phase);
        name_wheel.slots[index].depth = (uint16)((7 * cosine) >> 12);
        sine = rsin(508);
        radial = (sint16)slot->radial;
        x = ((radial + 70) * sine) >> 12;
        slot->x = (uint16)x;
        cosine = rcos(508);
        radial = (sint16)slot->radial;
        y = ((radial + 70) * cosine) >> 12;
        slot->y = (uint16)y;
        x = slot->x;
        slot->x = (uint16)(x + offset_x);
        y = slot->y;
        slot->y = (uint16)(y + offset_y);
    }
    return 0;
}

sint32 name_scroll_char(void)
{
    sint16 selected = (sint16)name_editor.selected_char;
    TEXT_RECORD *records = text_menu;
    char *text = records[selected + 5].text;
    sint16 current;
    sint16 found;
    sint16 lower;
    sint16 upper;
    sint16 difference;
    sint32 result;

    FUNCTION_MARKER(0x800580F8u, "MAIN.EXE");
    name_editor.wheel_step = 0u;
    found = (sint16)name_find_glyph((sint8)(uint8)text[0]);
    current = (sint16)name_editor.glyph;
    lower = (sint16)(current - 18);
    upper = current;
    if (lower < 0)
    {
        lower = (sint16)(current + 19);
        upper = (sint16)(current + 37);
    }
    difference = (sint16)(found - current);
    result = difference;
    if (found != current)
    {
        uint16 magnitude = difference < 0 ? (uint16)(0u - (uint32)(uint16)difference) : (uint16)difference;
        if (magnitude < 18)
            result = difference < 0 ? 4 : 6;
        else if (lower >= found)
            result = 6;
        else
            result = upper >= found ? 4 : 6;
        name_editor.phase = (uint16)((uint16)result);
    }
    return result;
}

sint32 name_update_wheel(CONTROLLER_STATE *input, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint32 state;
    sint32 index;
    char *selected_text;

    FUNCTION_MARKER(0x800581E8u, "MAIN.EXE");

    for (index = 0; index < 5; ++index)
    {
        sint32 selection = (sint16)name_editor.player_slot;
        TEXT_RECORD *menu = text_menu;

        menu[index].visible = (uint8)(selection == index);
    }

    state = (sint16)name_editor.phase;
    if (state == NAME_EDITOR_STEP_PREV || state == NAME_EDITOR_STEP_NEXT)
    {
        sint32 angle = (sint16)name_editor.wheel_angle;
        sint32 old_step = (sint16)name_editor.wheel_step;
        sint32 step = old_step + 6;

        if (state == NAME_EDITOR_STEP_PREV)
        {
            angle -= old_step;
            name_editor.previous_angle = (uint16)angle;
            angle += step;
        }
        else
        {
            angle += old_step;
            name_editor.previous_angle = (uint16)angle;
            angle -= step;
        }
        if (angle < 0)
            angle += 540;
        if (angle >= 540)
            angle -= 540;
        name_editor.wheel_step = (uint16)step;
        name_editor.wheel_angle = (uint16)angle;
        if (step >= 36)
            name_editor.phase = (uint16)((uint16)(state == NAME_EDITOR_STEP_PREV ? NAME_EDITOR_COMMIT_STEP : NAME_EDITOR_COMMIT_STEP));
    }
    else if (state == NAME_EDITOR_SEEK_PREV || state == NAME_EDITOR_SEEK_NEXT)
    {
        sint32 angle = (sint16)name_editor.wheel_angle;
        sint32 old_step = (sint16)name_editor.wheel_step;
        sint32 step = old_step + 12;

        if (state == NAME_EDITOR_SEEK_PREV)
        {
            angle -= old_step;
            name_editor.previous_angle = (uint16)angle;
            angle += step;
            if (angle >= 540)
                angle -= 540;
        }
        else
        {
            angle += old_step;
            name_editor.previous_angle = (uint16)angle;
            angle -= step;
            if (angle < 0)
                angle += 540;
        }
        name_editor.wheel_step = (uint16)step;
        name_editor.wheel_angle = (uint16)angle;
        if (step >= 36)
            name_editor.phase = (uint16)((uint16)(state == NAME_EDITOR_SEEK_PREV ? NAME_EDITOR_COMMIT_PREV : NAME_EDITOR_COMMIT_NEXT));
        if (r_u8(0x800B4138u) != 0u)
        {
            if ((input->current & 0x8000u) != 0u)
            {
                sint32 selected = (sint16)name_editor.target_char;

                if (selected != 0)
                    name_editor.target_char = (uint16)(selected - 1);
            }
            if ((input->current & 0x2000u) != 0u)
            {
                sint32 selected = (sint16)name_editor.target_char;

                if (selected < 3)
                    name_editor.target_char = (uint16)(selected + 1);
            }
        }
    }

    state = (sint16)name_editor.phase;
    if (state == NAME_EDITOR_COMMIT_STEP)
    {
        sint32 selected = name_editor.pending_glyph;
        sint32 start;

        name_editor.wheel_step = 0u;
        name_editor.wheel_angle = 0u;
        name_editor.phase = (uint16)(NAME_EDITOR_IDLE);
        name_editor.glyph = (uint16)((uint16)selected);
        start = (sint16)selected - 2;
        if (start < 0)
            start += 37;
        name_fill_ring(start);
    }
    else if (state == NAME_EDITOR_COMMIT_PREV)
    {
        sint32 current = (sint16)name_editor.glyph - 1;
        sint32 selected;
        sint32 desired;
        TEXT_RECORD *menu;
        uint8 character;

        name_editor.wheel_step = 0u;
        name_editor.wheel_angle = 0u;
        name_editor.glyph = (uint16)((uint16)current);
        if ((sint16)current < 0)
            name_editor.glyph = (uint16)((uint16)(current + 37));
        current = (sint16)name_editor.glyph;
        {
            sint32 start = current - 2;

            if (start < 0)
                start += 37;
            name_fill_ring(start);
        }
        selected = (sint16)name_editor.selected_char;
        menu = text_menu;
        selected_text = menu[selected + 5].text;
        desired = (sint16)name_editor.target_char;
        character = (uint8)name_alphabet[current];
        if (desired != selected)
        {
            name_editor.selected_char = (uint16)desired;
            name_scroll_char();
        }
        else if (character != (uint8)selected_text[0])
            name_editor.phase = (uint16)(NAME_EDITOR_SEEK_PREV);
        else
        {
            name_editor.phase = (uint16)(NAME_EDITOR_IDLE);
            w_u16(0x800B6AFAu, 0u);
        }
    }
    else if (state == NAME_EDITOR_COMMIT_NEXT)
    {
        sint32 old = (sint16)name_editor.glyph;
        sint32 current = old + 1;
        sint32 start = old - 1;
        sint32 selected;
        sint32 desired;
        TEXT_RECORD *menu;
        uint8 character;

        name_editor.wheel_step = 0u;
        name_editor.wheel_angle = 0u;
        name_editor.glyph = (uint16)((uint16)current);
        if ((sint16)current >= 37)
            name_editor.glyph = (uint16)(0u);
        current = (sint16)name_editor.glyph;
        if (start < 0)
            start += 37;
        name_fill_ring(start);
        selected = (sint16)name_editor.selected_char;
        menu = text_menu;
        selected_text = menu[selected + 5].text;
        desired = (sint16)name_editor.target_char;
        character = (uint8)name_alphabet[current];
        if (desired != selected)
        {
            name_editor.selected_char = (uint16)desired;
            name_scroll_char();
        }
        else if (character != (uint8)selected_text[0])
            name_editor.phase = (uint16)(NAME_EDITOR_SEEK_NEXT);
        else
        {
            name_editor.phase = (uint16)(NAME_EDITOR_IDLE);
            w_u16(0x800B6AFAu, 0u);
        }
    }

    name_layout_wheel(355, 70);
    for (index = 0; index < 15; ++index)
    {
        sint32 other;

        for (other = 0; other < 15; ++other)
        {
            sint16 first_index = (sint16)name_wheel.order[index];
            sint16 second_index = (sint16)name_wheel.order[other];
            sint16 first_x = (sint16)name_wheel.slots[first_index].depth;
            sint16 second_x = (sint16)name_wheel.slots[second_index].depth;
            sint16 first_secondary = 0;
            sint16 second_secondary = 0;

            if (first_x == second_x && index != other)
            {
                first_secondary = (sint16)name_wheel.slots[first_index].y;
                second_secondary = (sint16)name_wheel.slots[second_index].y;
            }
            if (index != other && (first_x > second_x || (first_x == second_x && second_secondary < first_secondary)))
            {
                name_wheel.order[other] = (uint16)first_index;
                name_wheel.order[index] = (uint16)second_index;
            }
        }
    }
    for (index = 0; index < 15; ++index)
    {
        sint16 sorted = (sint16)name_wheel.order[index];
        uint16 glyph = name_wheel.slots[sorted].glyph;

        name_wheel.sorted_glyphs[index] = glyph;
    }
    {
        uint32 records = r_u32(0x800B6A74u);

        for (index = 0; index < 15; ++index)
        {
            uint32 record = records + 3612u + 84u * (uint32)index;
            sint16 sorted = (sint16)name_wheel.order[index];
            sint16 glyph = (sint16)name_wheel.sorted_glyphs[index];
            uint32 sprite_base;
            uint32 sprite;
            sint16 vertical;
            sint16 width;
            sint16 height;
            sint16 coordinate;

            w_u16(record + 6u, (uint16)glyph);
            sprite_base = r_u32(record);
            vertical = (sint16)name_wheel.slots[sorted].depth;
            height = (sint16)name_wheel.glyphs[glyph].height;
            sprite = sprite_base + 120u * (uint32)(sint32)glyph;
            width = (sint16)name_wheel.glyphs[glyph].width;
            w_u8(record + 4u, (uint8)(vertical >= -2));
            coordinate = (sint16)name_wheel.slots[sorted].x;
            vertical = (sint16)name_wheel.slots[sorted].depth;
            w_u16(record + 8u, (uint16)(coordinate - (width >> 1) - vertical));
            coordinate = (sint16)name_wheel.slots[sorted].y;
            vertical = (sint16)name_wheel.slots[sorted].depth;
            w_u16(record + 10u, (uint16)(coordinate - vertical));
            vertical = (sint16)name_wheel.slots[sorted].depth;
            w_u32(sprite + 16u, (uint32)(width + 2 * vertical));
            vertical = (sint16)name_wheel.slots[sorted].depth;
            w_u32(sprite + 20u, (uint32)(height + 2 * vertical));
        }
    }

    if ((sint16)name_editor.phase == NAME_EDITOR_IDLE)
    {
        if ((input->current & 0x1000u) != 0u)
        {
            sint32 next = (sint16)name_editor.glyph - 1;
            sint32 selected;
            TEXT_RECORD *menu;
            uint8 character;

            w_u16(0x800B6AFAu, 0u);
            name_editor.wheel_step = 0u;
            name_editor.phase = (uint16)(NAME_EDITOR_STEP_PREV);
            name_editor.pending_glyph = (uint16)((uint16)next);
            if ((sint16)next < 0)
                name_editor.pending_glyph = (uint16)((uint16)(next + 37));
            selected = (sint16)name_editor.selected_char;
            menu = text_menu;
            next = (sint16)name_editor.pending_glyph;
            selected_text = menu[selected + 5].text;
            character = (uint8)name_alphabet[next];
            selected_text[0] = character;
        }
        if ((input->current & 0x4000u) != 0u)
        {
            sint32 next = (sint16)name_editor.glyph + 1;
            sint32 selected;
            TEXT_RECORD *menu;
            uint8 character;

            w_u16(0x800B6AFAu, 0u);
            name_editor.wheel_step = 0u;
            name_editor.phase = (uint16)(NAME_EDITOR_STEP_NEXT);
            name_editor.pending_glyph = (uint16)((uint16)next);
            if ((sint16)next >= 37)
                name_editor.pending_glyph = (uint16)(0u);
            selected = (sint16)name_editor.selected_char;
            menu = text_menu;
            next = (sint16)name_editor.pending_glyph;
            selected_text = menu[selected + 5].text;
            character = (uint8)name_alphabet[next];
            selected_text[0] = character;
        }
        if (r_u8(0x800B4138u) != 0u)
        {
            if ((input->current & 0x10u) != 0u)
            {
                sint32 page = (sint16)name_editor.player_slot;

                w_u16(0x800B6AFAu, 2u);
                if (page != 0)
                {
                    name_editor.player_slot = (uint16)((uint16)(page - 1));
                    name_refresh_menu_text();
                    name_editor.selected_char = 0u;
                    name_editor.glyph = (uint16)(0u);
                    name_editor.pending_glyph = (uint16)((uint16)-1);
                }
                else
                {
                    uint8 mode = r_u8(0x800E058Eu);

                    if (mode == 1u)
                        w_u16(0x800B413Eu, 3u);
                    else if (mode == 2u)
                        w_u16(0x800B413Eu, 4u);
                    else
                    {
                        sint32 profile_mode = (sint16)r_u16(0x800E0584u);

                        if (profile_mode == 1)
                            w_u16(0x800B413Eu, 27u);
                        else if (profile_mode == 2)
                            w_u16(0x800B413Eu, 37u);
                        else if (profile_mode == 0)
                            w_u16(0x800B413Eu, 35u);
                    }
                }
            }
            {
                uint16 buttons = input->current;
                sint16 previous = (sint16)name_editor.selected_char;
                sint16 selected;

                if ((buttons & 0x8000u) != 0u && previous != 0)
                {
                    name_editor.selected_char = (uint16)(previous - 1);
                    w_u16(0x800B6AFAu, 0u);
                }
                if ((input->current & 0x2000u) != 0u)
                {
                    sint32 current = (sint16)name_editor.selected_char;

                    if (current < 3)
                    {
                        name_editor.selected_char = (uint16)(current + 1);
                        w_u16(0x800B6AFAu, 0u);
                    }
                }
                selected = (sint16)name_editor.selected_char;
                if (previous != selected)
                {
                    name_editor.target_char = (uint16)selected;
                    name_scroll_char();
                }
            }
            if ((input->current & 0x40u) != 0u)
            {
                sint32 page;
                uint8 page_count;

                w_u16(0x800B6AFAu, 1u);
                name_commit_chars();
                page = (sint16)name_editor.player_slot;
                page_count = r_u8(0x800E058Eu);
                if (page + 1 == page_count)
                {
                    sint32 gate = (sint16)r_u16(0x800B69CAu);

                    w_u16(0x800B413Eu, 5u);
                    if (gate == 1)
                    {
                        PROFILE_SERIES *series = profile_get_series();

                        if ((sint16)series->phase != gate)
                            w_u16(0x800B413Eu, 18u);
                    }
                }
                else
                {
                    name_editor.player_slot = (uint16)((uint16)(page + 1));
                    name_refresh_menu_text();
                    name_editor.selected_char = 0u;
                    name_editor.pending_glyph = (uint16)((uint16)-1);
                }
            }
            {
                sint32 selected = (sint16)name_editor.selected_char;
                TEXT_RECORD *menu = text_menu;
                uint8 character;
                uint8 current;

                selected_text = menu[selected + 5].text;
                character = (uint8)name_alphabet[(sint16)name_editor.glyph];
                current = (uint8)selected_text[0];
                if (character == current)
                    name_editor.phase = (uint16)(NAME_EDITOR_IDLE);
            }
        }
    }
    return name_draw_entry();
}

sint32 name_draw_entry(void)
{
    uint32 frame = guest_stack_push(0x30u);
    TEXT_RECORD *records;
    sint16 selected;
    sint32 total = 6;
    sint32 x;
    sint16 left = 0;
    sint16 right = 0;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80058D68u, "MAIN.EXE");
    display_queue_beveled_rect_outline(184, 102, 340, 130);
    if ((sint16)name_editor.phase == NAME_EDITOR_IDLE)
        display_queue_beveled_rect_outline(360, 96, 420, 136);
    selected = (sint16)name_editor.selected_char;
    result = selected < 4;
    if (selected >= 4)
    {
        guest_stack_pop(0x30u);
        return result;
    }
    records = text_menu;
    for (index = 0; index < 4; ++index)
    {
        char *text = records[index + 5].text;
        uint8 sentinel = r_u8(0x800984B0u);
        uint32 character = (uint8)text[0];
        sint32 alphabet_index = 0;

        if (sentinel != 64u)
        {
            while (r_u8(0x800984B0u + (uint32)alphabet_index) != character && r_u8(0x800984B0u + (uint32)(alphabet_index + 1)) != 64u)
                ++alphabet_index;
        }
        w_u16(frame + 16u + 2u * (uint32)index, r_u16(0x800985B0u + 8u * (uint32)alphabet_index));
        total += r_u16(0x800985B0u + 8u * (uint32)alphabet_index);
    }
    x = 266 - (sint16)total / 2;
    records = text_menu;
    selected = (sint16)name_editor.selected_char;
    for (index = 0; index < 4; ++index)
    {
        records[index + 5].x = (uint16)x;
        if (index == selected)
        {
            left = (sint16)x;
            right = (sint16)(x + r_u16(frame + 16u + 2u * (uint32)index) - 2);
        }
        x = (sint16)(x + r_u16(frame + 16u + 2u * (uint32)index) - 2);
    }
    {
        uint8 color = menu_pulse.brightness;

        display_set_line_color((sint8)color, 0, (sint8)color);
    }
    display_queue_line_segment(left, 127, right, 127);
    display_queue_line_segment(left, 128, right, 128);
    if (menu_pulse.falling != 0u)
    {
        uint8 color = menu_pulse.brightness;

        menu_pulse.brightness = (uint8)(color - 4u);
        result = menu_pulse.brightness < 8u;
        if (result != 0)
            menu_pulse.falling = 0u;
    }
    else
    {
        uint8 color = menu_pulse.brightness;

        menu_pulse.brightness = (uint8)(color + 4u);
        result = 1;
        if (menu_pulse.brightness >= 225u)
            menu_pulse.falling = 1u;
    }
    guest_stack_pop(0x30u);
    return result;
}

sint32 name_build_glyphs(void)
{
    uint32 frame = guest_stack_push(0x88u);
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80058FD0u, "MAIN.EXE");
    for (index = 0; index < 52; ++index)
    {
        uint8 character = (uint8)name_alphabet[index];
        sint16 alphabet_index = text_fonts[0].map[character];
        const TEXT_METRIC *glyph;
        uint32 output = 0x800D7578u + 120u * (uint32)index;
        uint16 x;
        uint16 y;
        uint16 width;
        uint16 height;
        uint16 tpage;
        uint16 clut;

        if (alphabet_index < 0 || (size_t)alphabet_index >= text_fonts[0].count)
            abort();
        glyph = &text_fonts[0].metrics[alphabet_index];
        w_u16(frame + 78u, 450u);
        w_u8(frame + 62u, 0u);
        w_u16(frame + 76u, 960u);
        w_u16(frame + 72u, 960u);
        w_u16(frame + 74u, 256u);
        x = glyph->u;
        y = glyph->v;
        w_u16(frame + 80u, 256u);
        w_u16(frame + 82u, 80u);
        w_u16(frame + 64u, x);
        w_u16(frame + 66u, y);
        width = glyph->width;
        w_u16(frame + 68u, width);
        height = glyph->height;
        w_u16(frame + 70u, height);
        tpage = GetTPage(0, 0, 960, 256);
        w_u16(frame + 84u, tpage);
        clut = GetClut(960, 450);
        w_u16(frame + 86u, clut);
        width = glyph->width;
        height = glyph->height;
        menu_build_sprite_packet(output, frame + 32u, (sint16)x, (sint16)y, 0, 0, 0, 0, (sint16)width, (sint16)height);
        sprite_init_anim_rec((sint16)(index + 6), 0x800D7578u, 0, (sint16)index, (sint16)(10 * index), (sint16)(5 * index), 1);
    }
    result = 52 << 16;
    guest_stack_pop(0x88u);
    return result;
}

void name_fn_800591bc(void)
{
    FUNCTION_MARKER(0x800591BCu, "MAIN.EXE");
}

sint32 name_update_reels(CONTROLLER_STATE *input)
{
    uint32 records = r_u32(0x800B6A74u);
    uint32 buttons = input->current;
    sint32 index;
    sint32 active_count = 0;
    uint32 encoded = 0u;

    FUNCTION_MARKER(0x800591C4u, "MAIN.EXE");
    for (index = 0; index < 5; ++index)
    {
        uint32 record = records + 3444u + 84u * (uint32)index;
        uint32 sprite = r_u32(record) + 120u * (uint32)(sint32)(sint16)r_u16(record + 6u);
        NAME_REEL *reel = &name_reels.slots[index];
        sint16 mode = (sint16)reel->mode;
        sint32 shade = (sint32)r_u32(sprite + 36u) + reel->counter + 8 * (sint16)reel->selection;

        w_u32(sprite + 20u, 8u);
        w_u8(sprite + 89u, (uint8)(shade + 8));
        w_u8(sprite + 73u, (uint8)(shade + 8));
        w_u8(sprite + 81u, (uint8)shade);
        w_u8(sprite + 65u, (uint8)shade);
        if (mode == NAME_REEL_SPIN || mode == NAME_REEL_STEP)
        {
            uint16 counter = (uint16)(reel->counter + reel->step);
            reel->counter = counter;
            if (counter >= 9u)
            {
                sint16 step = (sint16)reel->step;
                sint16 selection;
                reel->counter = 0u;
                if (step > 0)
                {
                    selection = (sint16)(reel->selection + step);
                    if (selection < 0)
                        selection = 15;
                    if (selection >= 16)
                        selection = 0;
                    reel->selection = (uint16)selection;
                }
                if (mode == NAME_REEL_STEP && (sint16)reel->selection == (sint16)reel->previous)
                    reel->mode = NAME_REEL_SELECTED;
            }
        }
        if ((sint16)reel->mode == NAME_REEL_STOPPING)
        {
            uint16 counter = (uint16)(reel->counter + reel->step);
            reel->counter = counter;
            if (counter >= 9u)
            {
                sint16 step = (sint16)reel->step;
                reel->counter = 0u;
                if (step > 0)
                {
                    sint16 selection = (sint16)(reel->selection + step);
                    if (selection < 0)
                        selection = (sint16)(selection + 16);
                    if (selection >= 16)
                        selection = (sint16)(selection - 16);
                    reel->selection = (uint16)selection;
                }
                if ((sint16)reel->selection == (sint16)reel->target)
                {
                    sint32 may_stop = index == 0 || name_reels.slots[index - 1].mode == NAME_REEL_IDLE;
                    if (may_stop != 0)
                    {
                        w_u16(0x800B6AFAu, 4u);
                        reel->mode = NAME_REEL_IDLE;
                    }
                }
                if (reel->mode != NAME_REEL_IDLE)
                {
                    sint32 selection = reel->selection;
                    reel->step = (uint16)-2;
                    reel->counter = 6u;
                    reel->previous = (uint16)(selection - 1);
                    if ((sint16)reel->previous < 0)
                        reel->previous = (uint16)(selection + 15);
                    reel->selection = (uint16)(selection - 1);
                    if ((sint16)reel->selection < 0)
                        reel->selection = (uint16)(selection + 15);
                }
            }
        }
        display_set_line_color((sint8)224, (sint8)128, 0);
        mode = (sint16)reel->mode;
        if (mode == NAME_REEL_SELECTED || mode == NAME_REEL_STEP)
            display_queue_rect_outline((sint16)(18 * index + 326), 173, (sint16)(18 * index + 344), 182);
    }

    for (index = 0; index < 5; ++index)
    {
        NAME_REEL *reel = &name_reels.slots[index];
        if ((sint16)reel->mode != NAME_REEL_SELECTED)
            continue;
        ++active_count;
        if ((buttons & 0x1000u) != 0u)
        {
            sint16 selection;
            reel->step = (uint16)-1;
            reel->mode = NAME_REEL_STEP;
            reel->counter = 7u;
            w_u16(0x800B6AFAu, 0u);
            selection = (sint16)reel->selection - 1;
            if (selection < 0)
                selection = 15;
            reel->previous = (uint16)selection;
            selection = (sint16)reel->selection - 1;
            if (selection < 0)
                selection = 15;
            reel->selection = (uint16)selection;
        }
        else if ((buttons & 0x4000u) != 0u)
        {
            sint16 previous;
            reel->step = 1u;
            reel->mode = NAME_REEL_STEP;
            w_u16(0x800B6AFAu, 0u);
            previous = (sint16)reel->selection + 1;
            if (previous >= 16)
                previous = 0;
            reel->previous = (uint16)previous;
        }
        else if ((buttons & 0x8000u) != 0u && index > 0 && (uint8)name_reels.input_gate != 0u)
        {
            w_u16(0x800B6AFAu, 9u);
            name_reels.input_gate = (uint16)((name_reels.input_gate & 0xFF00u) | (uint8)(0u));
            reel->mode = NAME_REEL_IDLE;
            name_reels.slots[index - 1].mode = NAME_REEL_SELECTED;
        }
        else if ((buttons & 0x2000u) != 0u && index < 4 && (uint8)name_reels.input_gate != 0u)
        {
            w_u16(0x800B6AFAu, 9u);
            name_reels.input_gate = (uint16)((name_reels.input_gate & 0xFF00u) | (uint8)(0u));
            reel->mode = NAME_REEL_IDLE;
            name_reels.slots[index + 1].mode = NAME_REEL_SELECTED;
        }
        else if ((buttons & 0x80u) != 0u && (uint8)name_reels.input_gate != 0u)
        {
            reel->mode = NAME_REEL_IDLE;
            name_reels.input_gate = (uint16)((name_reels.input_gate & 0xFF00u) | (uint8)(0u));
            name_reels.input_gate = 0u;
            w_u16(0x800B6AFAu, 4u);
        }
    }

    if ((uint8)name_reels.input_gate != 0u && (buttons & 0x20u) != 0u)
    {
        for (index = 0; index < 5; ++index)
        {
            if ((sint16)name_reels.slots[index].mode == NAME_REEL_STOPPING)
                return index + 1;
        }
        for (index = 0; index < 5; ++index)
        {
            NAME_REEL *reel = &name_reels.slots[index];
            sint32 selection = reel->selection;
            reel->step = (uint16)-2;
            reel->mode = NAME_REEL_STOPPING;
            reel->counter = 6u;
            reel->previous = (uint16)(selection - 1);
            name_reels.slots[index].target = (uint16)(global_fn_8006e9d8() & 15);
            if ((sint16)reel->previous < 0)
                reel->previous = (uint16)(selection + 15);
            reel->selection = (uint16)(selection - 1);
            if ((sint16)reel->selection < 0)
                reel->selection = (uint16)(selection + 15);
        }
    }

    if ((uint8)name_reels.input_gate != 0u && (buttons & 0x80u) != 0u && active_count == 0)
    {
        for (index = 0; index < 5; ++index)
        {
            name_reels.slots[index].mode = NAME_REEL_IDLE;
            name_reels.slots[index].counter = 0u;
        }
        name_reels.slots[0].mode = NAME_REEL_SELECTED;
        name_reels.input_gate = 1u;
        w_u16(0x800B6AFAu, 4u);
    }

    for (index = 0; index < 5; ++index)
    {
        sint16 selection = (sint16)name_reels.slots[index].selection;
        uint32 digit = selection < 0 ? 15u : (uint32)(uint16)selection;
        encoded += digit << (4 * (4 - index));
    }
    name_reels.code = encoded;
    return 0;
}

sint32 name_check_reels(void)
{
    sint32 index;

    FUNCTION_MARKER(0x80059A34u, "MAIN.EXE");
    for (index = 0; index < 5; ++index)
    {
        sint16 mode = (sint16)name_reels.slots[index].mode;
        if (mode != NAME_REEL_IDLE && mode != NAME_REEL_SELECTED)
            return 0;
    }
    return 1;
}

sint32 name_reset_reels(void)
{
    uint32 encoded = name_reels.code;
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x80059A98u, "MAIN.EXE");
    name_reels.input_gate = 0u;
    for (index = 0; index < 5; ++index)
    {
        uint16 value = (uint16)((encoded >> (4 * (4 - index))) & 15u);
        name_reels.slots[index].previous = value;
        name_reels.slots[index].selection = value;
    }
    for (index = 0; index < 5; ++index)
    {
        uint32 sprite = r_u32(records + 3444u) + 120u * (uint32)(sint32)(sint16)r_u16(records + 3450u);
        name_reels.slots[index].step = 0u;
        name_reels.slots[index].counter = 0u;
        name_reels.slots[index].mode = NAME_REEL_IDLE;
        w_u32(sprite + 20u, 8u);
    }
    return name_update_reels(&input_controllers[0]);
}

sint32 time_copy_chars7(char *source, char *destination)
{
    sint32 index;
    uint32 result;

    FUNCTION_MARKER(0x80059BB4u, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        result = (uint8)source[(uint32)index + 1u];
        destination[(uint32)index] = (uint8)result;
    }
    result = (uint8)source[7u];
    destination[7u] = 0u;
    destination[6u] = (uint8)result;
    return (sint32)result;
}

sint32 name_bytes_copy8(char *source, char *destination)
{
    sint32 index;
    uint32 result = 0u;

    FUNCTION_MARKER(0x80059C10u, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        result = (uint8)source[(uint32)index];
        destination[(uint32)index] = (uint8)result;
    }
    return (sint32)result;
}

uint32 time_parts_to_tenths(uint32 value)
{
    uint32 third = r_u16(value + 4u);
    uint32 first = r_u16(value);
    uint32 second = r_u16(value + 2u);

    FUNCTION_MARKER(0x80059C70u, "MAIN.EXE");
    return 6000u * first + 100u * second + third / 10u;
}
