#include "name.h"
#include "disp.h"
#include "game.h"
#include "hd.h"
#include "input.h"
#include "menu.h"
#include "movie.h"
#include "profile.h"
#include "save.h"
#include "sprite.h"
#include "text.h"
#include "utils.h"
#include "xport_trace.h"
#include <math.h>
#include <stdlib.h>

NAME_EDITOR name_editor;
NAME_REELS name_reels;

const char name_code_frac[5] = "frac";
const char name_code_winr[5] = "winr";
const char name_code_hurr[5] = "hurr";

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

    for (slot = 0; slot < 5; ++slot)
    {
        for (character = 0; character < 4; ++character)
            player_names[slot].text[character] = defaults[slot][character];
        player_names[slot].text[4] = 0;
    }
}

// Native name codec used by MAIN.EXE save gather/scatter at 0x8004B644/0x8004B6BC
uint8 *name_save_byte(uint32 offset)
{
    uint32 field;
    PLAYER_NAME *name;

    if (offset < 4984 || offset >= 5014)
        return NULL;
    field = offset - 4984;
    name = &player_names[field / 6];
    field %= 6;
    return field < 5 ? (uint8 *)&name->text[field] : &name->reserved;
}

// Keep the hidden seam opposite the visible side of the wheel
static void name_fill_ring(sint32 start)
{
    for (sint32 idx = 0; idx < 15; ++idx)
    {
        sint32 val = start + idx - (idx >= 8 ? 15 : 0);
        if (val < 0)
            val += 37;
        if (val >= 37)
            val -= 37;
        name_wheel.slots[idx].glyph = val;
    }
}

void name_apply_player_name_cheats(void)
{
    static const char mode_codes[3][6][5] = {{"d  1", "d  2", "d  3", "d  4", "d  5", "d  6"}, {"n  1", "n  2", "n  3", "n  4", "n  5", "n  6"}, {"m  1", "m  2", "m  3", "m  4", "m  5", "m  6"}};
    sint32 result = (sint16)game_selection.mode;

    FUNCTION_MARKER(0x8005056C, "MAIN.EXE");
    if (result != 0)
        return;
    result = 1;
    if ((uint8)game_selection.players == 1)
    {
        PLAYER_PROFILE *profile_data = profile_cur();
        sint32 sel_mode = -1;
        sint32 sel_count = 0;
        sint32 mode;
        sint32 count;

        for (mode = 0; mode < 3; ++mode)
            for (count = 0; count < 6; ++count)
                if (name_matches(0, mode_codes[mode][count]) != 0)
                {
                    sel_mode = mode;
                    sel_count = count + 1;
                }
        if (sel_mode != -1)
        {
            menu_state.sound = 3;
            profile_data->course = sel_count - 1;
            profile_data->level = sel_mode;
            profile_data->max_level = sel_mode;
            for (count = 0; count < sel_count - 1; ++count)
                profile_data->progress.courses[sel_mode][count].state = 6;
            profile_data->progress.courses[sel_mode][count].state = 4;
        }
        if (name_matches(0, " day") != 0)
        {
            menu_state.sound = 3;
            profile_data->course = 5;
            profile_data->level = 0;
            profile_data->max_level = 0;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[0u][count].state = 6;
        }
        if (name_matches(0, " nit") != 0)
        {
            menu_state.sound = 3;
            profile_data->course = 5;
            profile_data->level = 1;
            profile_data->max_level = 1;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[1u][count].state = 6;
        }
        if (name_matches(0, "rrim") != 0)
        {
            menu_state.sound = 3;
            profile_data->course = 5;
            profile_data->level = 2;
            profile_data->max_level = 2;
            for (count = 0; count < 6; ++count)
                profile_data->progress.courses[2u][count].state = 6;
        }
        if (name_matches(0, name_code_frac) != 0)
        {
            menu_state.sound = 3;
            profile_data->course = 1;
            profile_data->level = 3;
            profile_data->max_level = 3;
        }
        if (name_matches(0, " boa") != 0)
        {
            menu_state.sound = 3;
            for (count = 0; count < 10; ++count)
                profile_cur()->availability[count] = 1;
        }
        {
            uint16 flags = game_selection.flags;
            if ((flags & 4u) != 0)
                game_selection.flags = flags - 4u;
        }
        if (name_matches(0, " qak") != 0)
        {
            uint16 flags = game_selection.flags;
            menu_state.sound = 3;
            game_selection.flags = flags + 4u;
        }
        {
            uint16 flags = game_selection.flags;
            if ((flags & 16u) != 0)
                game_selection.flags = flags - 16u;
        }
        if (name_matches(0, " str") != 0)
        {
            uint16 flags;

            menu_state.sound = 3;
            flags = game_selection.flags;
            menu_state.movie_idx = 0;
            menu_state.exit_wait = 5;
            game_selection.flags = flags + 16u;
            text_set_hud_visible(0, 1);
            menu_state.phase = 25;
        }
        {
            uint16 flags = game_selection.flags;
            if ((flags & 8u) != 0)
                game_selection.flags = flags - 8u;
        }
        if (name_matches(0, "bxtr") != 0)
        {
            menu_state.sound = 3;
            game_selection.flags |= 0x28;
        }
        if (name_matches(0, name_code_hurr) != 0)
        {
            menu_state.sound = 3;
        }

        {
            uint16 flags = game_selection.flags;
            if ((flags & 64u) != 0)
                game_selection.flags = flags - 64u;
        }
        if (name_matches(0, "rush") != 0)
        {
            uint16 flags = game_selection.flags;
            menu_state.sound = 3;
            game_selection.flags = flags + 64u;
        }
        profile_reset_grid_flags();
        save_flush();
        return;
    }
}

static sint32 name_refresh_menu_text(void)
{
    sint32 idx;
    uint32 val = 0;

    FUNCTION_MARKER(0x80057B60, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
    {
        TEXT_RECORD *recs = text_menu;
        sint16 selection = name_editor.player_slot;
        char *dst = recs[idx + 5].text;

        val = (uint8)name_player_text(selection)[idx];
        dst[0] = val;
    }
    return val;
}

sint32 name_matches(sint16 idx, const char *code)
{
    const char *src = name_player_text(idx);
    sint32 i;

    FUNCTION_MARKER(0x80057BC8, "MAIN.EXE");
    for (i = 0; i < 4; ++i)
    {
        if ((uint8)src[i] != (uint8)code[i])
            return 0;
    }
    return 1;
}

sint32 name_copy_player_name_bytes(uint16 idx, char *output)
{
    const char *src;
    sint32 offset;
    uint32 val = 0;

    FUNCTION_MARKER(0x80057C4C, "MAIN.EXE");
    if (idx >= 5u)
        abort();
    src = name_player_text(idx);
    for (offset = 0; offset < 4; ++offset)
    {
        val = (uint8)src[offset];
        output[offset] = val;
    }
    return val;
}

static void name_commit_chars(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x80057CC0, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
    {
        TEXT_RECORD *recs = text_menu;
        sint16 selection = name_editor.player_slot;
        char *src = recs[idx + 5].text;
        uint8 val = src[0];

        name_player_text(selection)[idx] = (char)val;
    }
    save_flush();
}

static sint32 name_build_glyphs(void)
{
    SPRITE_IMAGE image = {0};
    sint32 idx;

    FUNCTION_MARKER(0x80058FD0, "MAIN.EXE");
    spr_register_packets();
    for (idx = 0; idx < 52; ++idx)
    {
        uint8 character = name_alphabet[idx];
        sint16 alphabet_idx = text_fonts[0].map[character];
        const TEXT_METRIC *glyph;
        SPRITE_RENDER *output = spr_render_at(spr_glyphs, idx);
        uint16 x;
        uint16 y;
        uint16 width;
        uint16 height;
        uint16 tpage;
        uint16 clut;

        if (alphabet_idx < 0 || (uint32)alphabet_idx >= text_fonts[0].count)
            abort();
        glyph = &text_fonts[0].metrics[alphabet_idx];
        image.clut_y = 450;
        image.mode = 0;
        image.clut_x = 960;
        image.tpage_x = 960;
        image.tpage_y = 256;
        x = glyph->u;
        y = glyph->v;
        image.x = 256;
        image.y = 80;
        image.u = x;
        image.v = y;
        image.width = glyph->width;
        image.height = glyph->height;
        tpage = GetTPage(0, 0, 960, 256);
        image.tpage = tpage;
        clut = GetClut(960, 450);
        image.clut = clut;
        width = glyph->width;
        height = glyph->height;
        menu_build_spr_packet(output, &image, (sint16)x, (sint16)y, 0, 0, 0, 0, (sint16)width, (sint16)height);
        spr_init_anim_rec(idx + 6, spr_glyphs, 0, idx, 10 * idx, 5 * idx, 1);
    }
    return 52 << 16;
}

static sint32 name_find_glyph(sint8 val)
{
    sint32 idx;

    FUNCTION_MARKER(0x80057EF4, "MAIN.EXE");
    for (idx = 0; idx < 37; ++idx)
    {
        if ((uint8)name_alphabet[idx] == (uint8)val)
            return idx;
    }
    return 37;
}

sint32 name_reset_wheel(void)
{
    uint16 cfg = menu_state.scr;
    UI_RECORD *recs;
    sint32 idx;

    FUNCTION_MARKER(0x80057D28, "MAIN.EXE");
    name_editor.player_slot = 0;
    name_editor.phase = NAME_EDITOR_IDLE;
    name_editor.sel_char = 0;
    name_editor.target_char = 0;
    name_editor.wheel_angle = 0;
    name_editor.wheel_step = 0;
    menu_pulse.brightness = 0;
    menu_pulse.falling = 0;
    name_editor.pending_glyph = -1;
    menu_state.from_scr = cfg;
    name_build_glyphs();
    recs = spr_recs;
    for (idx = 0; idx < 37; ++idx)
    {
        UI_RECORD *rec = recs + idx + 6;
        sint32 spr_idx = rec->val;
        SPRITE_RENDER *spr = spr_render_at(rec->data, spr_idx);

        name_wheel.glyphs[idx].width = spr->width;
        spr_idx = rec->val;
        spr = spr_render_at(rec->data, spr_idx);
        name_wheel.glyphs[idx].height = spr->height;
    }
    for (idx = 0; idx < 15; ++idx)
        name_wheel.order[idx] = idx;
    for (idx = 0; idx < 37; ++idx)
        spr_recs[idx + 6].type = 0;
    name_refresh_menu_text();
    name_editor.glyph = name_find_glyph(text_menu[name_editor.sel_char + 5].text[0]);
    sint32 start = name_editor.glyph - 2;
    if (start < 0)
        start += 37;
    name_fill_ring(start);
    return name_update_wheel(&input_pads[0]);
}

static sint32 name_layout_wheel(sint16 offset_x, sint16 offset_y)
{
    sint32 idx;

    FUNCTION_MARKER(0x80057F40, "MAIN.EXE");
    for (idx = 0; idx < 15; ++idx)
    {
        NAME_WHEEL_SLOT *slot = &name_wheel.slots[idx];
        sint32 phase = (36 * idx + (sint16)name_editor.wheel_angle - 90) * 4096 / 540;
        sint32 s;
        sint32 c;
        sint32 radial;
        sint32 x;
        sint32 y;

        slot->angle = phase;
        if (phase < 0)
            slot->angle = phase + 4096;
        phase = slot->angle;
        if (phase >= 4096)
            slot->angle = phase - 4096;
        phase = slot->angle;
        /* PsyQ rsin uses the original integer lookup table */
        s = rsin(phase);
        slot->radial = (sint16)(35 * s >> 11);
        c = rcos(phase);
        slot->depth = 7 * c >> 12;
        s = rsin(508);
        x = ((sint16)slot->radial + 70) * s >> 12;
        slot->x = x;
        c = rcos(508);
        radial = (sint16)slot->radial;
        y = (radial + 70) * c >> 12;
        slot->y = y;
        slot->x += offset_x;
        slot->y += offset_y;
    }
    return 0;
}

void hd_name_wheel(float fract)
{
    float angle = (sint16)name_editor.wheel_angle;
    switch (name_editor.phase)
    {
        case NAME_EDITOR_STEP_PREV:
            angle += 6 * fract;
            break;
        case NAME_EDITOR_STEP_NEXT:
            angle -= 6 * fract;
            break;
        case NAME_EDITOR_SEEK_PREV:
            angle += 12 * fract;
            break;
        case NAME_EDITOR_SEEK_NEXT:
            angle -= 12 * fract;
            break;
        default:
            break;
    }

    struct
    {
        float x, y, depth;
        uint32 slot;
    } samples[15];

    uint32 count = 0;
    for (uint32 idx = 0; idx < 15; ++idx)
    {
        float phase = (36 * idx + angle - 90) * (4096.0f / 540) * RAD12;
        float radial = 70 * sinf(phase);
        float depth = 7 * cosf(phase);
        if (depth < -2)
            continue;
        uint32 dst = count++;
        float x = (radial + 70) * sinf(508 * RAD12) + 355;
        float y = (radial + 70) * cosf(508 * RAD12) + 70;
        while (dst && (samples[dst - 1].depth > depth || (samples[dst - 1].depth == depth && samples[dst - 1].y > y)))
        {
            samples[dst] = samples[dst - 1];
            --dst;
        }
        samples[dst].x = x;
        samples[dst].y = y;
        samples[dst].depth = depth;
        samples[dst].slot = idx;
    }
    for (uint32 idx = 0; idx < count; ++idx)
    {
        uint32 glyph = name_wheel.slots[samples[idx].slot].glyph;
        const NAME_WHEEL_METRIC *metric = &name_wheel.glyphs[glyph];
        const SPRITE_RENDER *spr = &spr_glyphs[glyph];
        float depth = samples[idx].depth;
        float x = samples[idx].x - (metric->width >> 1) - depth;
        float y = samples[idx].y - depth;
        float width = metric->width + 2 * depth;
        float height = metric->height + 2 * depth;
        float pos[4][2], uv[4][2];
        for (uint32 corner = 0; corner < 4; ++corner)
        {
            pos[corner][0] = x + (corner & 2 ? width : 0);
            pos[corner][1] = y + (corner & 1 ? height : 0);
            uv[corner][0] = spr->quad.p[corner].uv.u;
            uv[corner][1] = spr->quad.p[corner].uv.v;
        }
        hd_draw_ui_quad(pos, uv, spr->quad.p[1].attr, spr->quad.p[0].attr, spr->quad.rgbc.rgbc, 256, 1, (spr->quad.rgbc.code & 2) != 0);
    }
}

static sint32 name_scroll_char(void)
{
    sint16 sel = name_editor.sel_char;
    TEXT_RECORD *recs = text_menu;
    char *text = recs[sel + 5].text;
    sint16 cur;
    sint16 found;
    sint16 lower;
    sint16 upper;
    sint16 difference;
    sint32 result;

    FUNCTION_MARKER(0x800580F8, "MAIN.EXE");
    name_editor.wheel_step = 0;
    found = name_find_glyph(text[0]);
    cur = name_editor.glyph;
    lower = cur - 18;
    upper = cur;
    if (lower < 0)
    {
        lower = cur + 19;
        upper = cur + 37;
    }
    difference = found - cur;
    result = difference;
    if (found != cur)
    {
        uint16 mag = abs(difference);
        if (mag < 18)
            result = difference < 0 ? 4 : 6;
        else if (lower >= found)
            result = 6;
        else
            result = upper >= found ? 4 : 6;
        name_editor.phase = result;
    }
    return result;
}

static sint32 name_draw_entry(void)
{
    uint16 widths[4];
    TEXT_RECORD *recs;
    sint16 sel;
    sint32 total = 6;
    sint32 x;
    sint16 left = 0;
    sint16 right = 0;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x80058D68, "MAIN.EXE");
    disp_queue_beveled_rect_outline(184, 102, 340, 130);
    if (name_editor.phase == NAME_EDITOR_IDLE)
        disp_queue_beveled_rect_outline(360, 96, 420, 136);
    sel = name_editor.sel_char;
    result = sel < 4;
    if (sel >= 4)
    {
        return result;
    }
    recs = text_menu;
    for (idx = 0; idx < 4; ++idx)
    {
        widths[idx] = text_char_width(0, recs[idx + 5].text[0]);
        total += widths[idx];
    }
    x = 266 - (sint16)total / 2;
    recs = text_menu;
    sel = name_editor.sel_char;
    for (idx = 0; idx < 4; ++idx)
    {
        recs[idx + 5].x = x;
        if (idx == sel)
        {
            left = x;
            right = x + widths[idx] - 2;
        }
        x = (sint16)(x + widths[idx] - 2);
    }
    {
        uint8 color = menu_pulse.brightness;

        disp_set_line_color(color, 0, color);
    }
    disp_queue_line_seg(left, 127, right, 127);
    disp_queue_line_seg(left, 128, right, 128);
    if (menu_pulse.falling != 0)
    {
        menu_pulse.brightness -= 4;
        result = menu_pulse.brightness < 8u;
        if (result != 0)
            menu_pulse.falling = 0;
    }
    else
    {
        menu_pulse.brightness += 4;
        result = 1;
        if (menu_pulse.brightness >= 225u)
            menu_pulse.falling = 1;
    }
    return result;
}

sint32 name_update_wheel(PAD *input)
{
    sint32 state;
    sint32 idx;
    char *sel_text;

    FUNCTION_MARKER(0x800581E8, "MAIN.EXE");

    for (idx = 0; idx < 5; ++idx)
    {
        sint16 selection = name_editor.player_slot;
        TEXT_RECORD *menu = text_menu;

        menu[idx].visible = selection == idx;
    }

    state = name_editor.phase;
    if (state == NAME_EDITOR_STEP_PREV || state == NAME_EDITOR_STEP_NEXT)
    {
        sint32 angle = (sint16)name_editor.wheel_angle;
        sint32 step = name_editor.wheel_step + 6;

        angle += state == NAME_EDITOR_STEP_PREV ? 6 : -6;
        if (angle < 0)
            angle += 540;
        if (angle >= 540)
            angle -= 540;
        name_editor.wheel_step = step;
        name_editor.wheel_angle = angle;
        if (step >= 36)
            name_editor.phase = NAME_EDITOR_COMMIT_STEP;
    }
    else if (state == NAME_EDITOR_SEEK_PREV || state == NAME_EDITOR_SEEK_NEXT)
    {
        sint32 angle = (sint16)name_editor.wheel_angle;
        sint32 step = name_editor.wheel_step + 12;

        angle += state == NAME_EDITOR_SEEK_PREV ? 12 : -12;
        if (angle < 0)
            angle += 540;
        if (angle >= 540)
            angle -= 540;
        name_editor.wheel_step = step;
        name_editor.wheel_angle = angle;
        if (step >= 36)
            name_editor.phase = state == NAME_EDITOR_SEEK_PREV ? NAME_EDITOR_COMMIT_PREV : NAME_EDITOR_COMMIT_NEXT;
        if (menu_state.input_enabled != 0)
        {
            if ((input->cur & 0x8000u) != 0)
            {
                sint16 sel = name_editor.target_char;

                if (sel != 0)
                    name_editor.target_char = sel - 1;
            }
            if ((input->cur & 0x2000u) != 0)
            {
                sint16 sel = name_editor.target_char;

                if (sel < 3)
                    name_editor.target_char = sel + 1;
            }
        }
    }

    state = name_editor.phase;
    if (state == NAME_EDITOR_COMMIT_STEP)
    {
        sint32 sel = name_editor.pending_glyph;
        sint32 start;

        name_editor.wheel_step = 0;
        name_editor.wheel_angle = 0;
        name_editor.phase = NAME_EDITOR_IDLE;
        name_editor.glyph = sel;
        start = (sint16)sel - 2;
        if (start < 0)
            start += 37;
        name_fill_ring(start);
    }
    else if (state == NAME_EDITOR_COMMIT_PREV)
    {
        sint32 cur = name_editor.glyph - 1;
        sint32 sel;
        sint32 desired;
        TEXT_RECORD *menu;
        uint8 character;

        name_editor.wheel_step = 0;
        name_editor.wheel_angle = 0;
        name_editor.glyph = cur;
        if ((sint16)cur < 0)
            name_editor.glyph = cur + 37;
        cur = name_editor.glyph;
        {
            sint32 start = cur - 2;

            if (start < 0)
                start += 37;
            name_fill_ring(start);
        }
        sel = name_editor.sel_char;
        menu = text_menu;
        sel_text = menu[sel + 5].text;
        desired = name_editor.target_char;
        character = name_alphabet[cur];
        if (desired != sel)
        {
            name_editor.sel_char = desired;
            name_scroll_char();
        }
        else if (character != (uint8)sel_text[0])
            name_editor.phase = NAME_EDITOR_SEEK_PREV;
        else
        {
            name_editor.phase = NAME_EDITOR_IDLE;
            menu_state.sound = 0;
        }
    }
    else if (state == NAME_EDITOR_COMMIT_NEXT)
    {
        sint16 old = name_editor.glyph;
        sint32 cur = old + 1;
        sint32 start = old - 1;
        sint32 sel;
        sint32 desired;
        TEXT_RECORD *menu;
        uint8 character;

        name_editor.wheel_step = 0;
        name_editor.wheel_angle = 0;
        name_editor.glyph = cur;
        if ((sint16)cur >= 37)
            name_editor.glyph = 0;
        cur = name_editor.glyph;
        if (start < 0)
            start += 37;
        name_fill_ring(start);
        sel = name_editor.sel_char;
        menu = text_menu;
        sel_text = menu[sel + 5].text;
        desired = name_editor.target_char;
        character = name_alphabet[cur];
        if (desired != sel)
        {
            name_editor.sel_char = desired;
            name_scroll_char();
        }
        else if (character != (uint8)sel_text[0])
            name_editor.phase = NAME_EDITOR_SEEK_NEXT;
        else
        {
            name_editor.phase = NAME_EDITOR_IDLE;
            menu_state.sound = 0;
        }
    }

    name_layout_wheel(355, 70);
    for (idx = 0; idx < 15; ++idx)
    {
        sint32 other;

        for (other = 0; other < 15; ++other)
        {
            sint16 idx0 = name_wheel.order[idx];
            sint16 idx1 = name_wheel.order[other];
            sint16 x0 = name_wheel.slots[idx0].depth;
            sint16 x1 = name_wheel.slots[idx1].depth;
            sint16 secondary0 = 0;
            sint16 secondary1 = 0;

            if (x0 == x1 && idx != other)
            {
                secondary0 = name_wheel.slots[idx0].y;
                secondary1 = name_wheel.slots[idx1].y;
            }
            if (idx != other && (x0 > x1 || x0 == x1 && secondary1 < secondary0))
            {
                name_wheel.order[other] = idx0;
                name_wheel.order[idx] = idx1;
            }
        }
    }
    for (idx = 0; idx < 15; ++idx)
    {
        sint16 sorted = name_wheel.order[idx];
        name_wheel.sorted_glyphs[idx] = name_wheel.slots[sorted].glyph;
    }
    {
        UI_RECORD *recs = spr_recs;

        for (idx = 0; idx < 15; ++idx)
        {
            UI_RECORD *rec = recs + idx + 43;
            sint16 sorted = name_wheel.order[idx];
            sint16 glyph = name_wheel.sorted_glyphs[idx];
            SPRITE_RENDER *spr_base;
            SPRITE_RENDER *spr;
            sint16 vertical;
            sint16 width;
            sint16 height;
            sint16 coord;

            rec->val = glyph;
            spr_base = rec->data;
            NAME_WHEEL_SLOT *wheel_slot = name_wheel.slots + sorted;
            vertical = wheel_slot->depth;
            height = name_wheel.glyphs[glyph].height;
            spr = spr_render_at(spr_base, glyph);
            width = name_wheel.glyphs[glyph].width;
            rec->type = vertical >= -2;
            coord = wheel_slot->x;
            vertical = wheel_slot->depth;
            rec->x = coord - (width >> 1) - vertical;
            coord = wheel_slot->y;
            vertical = wheel_slot->depth;
            rec->y = coord - vertical;
            vertical = wheel_slot->depth;
            spr->width = width + 2 * vertical;
            vertical = wheel_slot->depth;
            spr->height = height + 2 * vertical;
        }
    }

    if (name_editor.phase == NAME_EDITOR_IDLE)
    {
        if ((input->cur & 0x1000u) != 0)
        {
            sint32 next = name_editor.glyph - 1;
            sint32 sel;
            TEXT_RECORD *menu;
            uint8 character;

            menu_state.sound = 0;
            name_editor.wheel_step = 0;
            name_editor.phase = NAME_EDITOR_STEP_PREV;
            name_editor.pending_glyph = next;
            if ((sint16)next < 0)
                name_editor.pending_glyph = next + 37;
            sel = name_editor.sel_char;
            menu = text_menu;
            next = name_editor.pending_glyph;
            sel_text = menu[sel + 5].text;
            character = name_alphabet[next];
            sel_text[0] = character;
        }
        if ((input->cur & 0x4000u) != 0)
        {
            sint32 next = name_editor.glyph + 1;
            sint32 sel;
            TEXT_RECORD *menu;
            uint8 character;

            menu_state.sound = 0;
            name_editor.wheel_step = 0;
            name_editor.phase = NAME_EDITOR_STEP_NEXT;
            name_editor.pending_glyph = next;
            if ((sint16)next >= 37)
                name_editor.pending_glyph = 0;
            sel = name_editor.sel_char;
            menu = text_menu;
            next = name_editor.pending_glyph;
            sel_text = menu[sel + 5].text;
            character = name_alphabet[next];
            sel_text[0] = character;
        }
        if (menu_state.input_enabled != 0)
        {
            if ((input->cur & 0x10u) != 0)
            {
                sint16 page = name_editor.player_slot;

                menu_state.sound = 2;
                if (page != 0)
                {
                    name_editor.player_slot = page - 1;
                    name_refresh_menu_text();
                    name_editor.sel_char = 0;
                    name_editor.glyph = 0;
                    name_editor.pending_glyph = -1;
                }
                else
                {
                    uint8 mode = game_selection.players;

                    if (mode == 1)
                        menu_state.phase = 3;
                    else if (mode == 2)
                        menu_state.phase = 4;
                    else
                    {
                        sint16 profile_mode = game_selection.rules;

                        if (profile_mode == 1)
                            menu_state.phase = 27;
                        else if (profile_mode == 2)
                            menu_state.phase = 37;
                        else if (profile_mode == 0)
                            menu_state.phase = 35;
                    }
                }
            }
            {
                uint16 buttons = input->cur;
                sint16 prev = name_editor.sel_char;
                sint16 sel;

                if ((buttons & 0x8000u) != 0 && prev != 0)
                {
                    name_editor.sel_char = prev - 1;
                    menu_state.sound = 0;
                }
                if ((input->cur & 0x2000u) != 0)
                {
                    sint16 cur = name_editor.sel_char;

                    if (cur < 3)
                    {
                        name_editor.sel_char = cur + 1;
                        menu_state.sound = 0;
                    }
                }
                sel = name_editor.sel_char;
                if (prev != sel)
                {
                    name_editor.target_char = sel;
                    name_scroll_char();
                }
            }
            if ((input->cur & 0x40u) != 0)
            {
                sint32 page;
                uint8 page_count;

                menu_state.sound = 1;
                name_commit_chars();
                page = name_editor.player_slot;
                page_count = game_selection.players;
                if (page + 1 == page_count)
                {
                    sint16 gate = menu_state.series_mode;

                    menu_state.phase = 5;
                    if (gate == 1)
                    {
                        PROFILE_SERIES *series = profile_get_series();

                        if ((sint16)series->phase != gate)
                            menu_state.phase = 18;
                    }
                }
                else
                {
                    name_editor.player_slot = page + 1;
                    name_refresh_menu_text();
                    name_editor.sel_char = 0;
                    name_editor.pending_glyph = -1;
                }
            }
            {
                sint16 sel = name_editor.sel_char;
                TEXT_RECORD *menu = text_menu;
                uint8 character;
                uint8 cur;

                sel_text = menu[sel + 5].text;
                character = name_alphabet[name_editor.glyph];
                cur = sel_text[0];
                if (character == cur)
                    name_editor.phase = NAME_EDITOR_IDLE;
            }
        }
    }
    return name_draw_entry();
}

sint32 name_update_reels(PAD *input)
{
    UI_RECORD *recs = spr_recs;
    uint32 buttons = input->cur;
    sint32 idx;
    sint32 active_count = 0;
    uint32 encoded = 0;

    FUNCTION_MARKER(0x800591C4, "MAIN.EXE");
    for (idx = 0; idx < 5; ++idx)
    {
        UI_RECORD *rec = recs + idx + 41;
        SPRITE_RENDER *spr = spr_render_at(rec->data, rec->val);
        NAME_REEL *reel = &name_reels.slots[idx];
        sint16 mode = reel->mode;
        sint32 shade = spr->tpage_y + reel->counter + 8 * reel->selection;

        spr->height = 8;
        spr->quad.p[3].uv.v = shade + 8;
        spr->quad.p[1].uv.v = shade + 8;
        spr->quad.p[2].uv.v = shade;
        spr->quad.p[0].uv.v = shade;
        if (mode == NAME_REEL_SPIN || mode == NAME_REEL_STEP)
        {
            uint16 counter = reel->counter + reel->step;
            reel->counter = counter;
            if (counter >= 9u)
            {
                sint16 step = reel->step;
                sint16 selection;
                reel->counter = 0;
                if (step > 0)
                {
                    selection = reel->selection + step;
                    if (selection < 0)
                        selection = 15;
                    if (selection >= 16)
                        selection = 0;
                    reel->selection = selection;
                }
                if (mode == NAME_REEL_STEP && reel->selection == reel->prev)
                    reel->mode = NAME_REEL_SELECTED;
            }
        }
        if (reel->mode == NAME_REEL_STOPPING)
        {
            uint16 counter = reel->counter + reel->step;
            reel->counter = counter;
            if (counter >= 9u)
            {
                sint16 step = reel->step;
                reel->counter = 0;
                if (step > 0)
                {
                    sint16 selection = reel->selection + step;
                    if (selection < 0)
                        selection = selection + 16;
                    if (selection >= 16)
                        selection = selection - 16;
                    reel->selection = selection;
                }
                if (reel->selection == reel->target)
                {
                    sint32 may_stop = idx == 0 || name_reels.slots[idx - 1].mode == NAME_REEL_IDLE;
                    if (may_stop != 0)
                    {
                        menu_state.sound = 4;
                        reel->mode = NAME_REEL_IDLE;
                    }
                }
                if (reel->mode != NAME_REEL_IDLE)
                {
                    sint32 selection = reel->selection;
                    reel->step = -2;
                    reel->counter = 6;
                    reel->prev = selection - 1;
                    if (reel->prev < 0)
                        reel->prev = selection + 15;
                    reel->selection = selection - 1;
                    if (reel->selection < 0)
                        reel->selection = selection + 15;
                }
            }
        }
        disp_set_line_color(224, 128, 0);
        mode = reel->mode;
        if (mode == NAME_REEL_SELECTED || mode == NAME_REEL_STEP)
            disp_queue_rect_outline(18 * idx + 326, 173, 18 * idx + 344, 182);
    }

    for (idx = 0; idx < 5; ++idx)
    {
        NAME_REEL *reel = &name_reels.slots[idx];
        if (reel->mode != NAME_REEL_SELECTED)
            continue;
        ++active_count;
        if ((buttons & 0x1000) != 0)
        {
            sint16 selection;
            reel->step = -1;
            reel->mode = NAME_REEL_STEP;
            reel->counter = 7;
            menu_state.sound = 0;
            selection = reel->selection - 1;
            if (selection < 0)
                selection = 15;
            reel->prev = selection;
            selection = reel->selection - 1;
            if (selection < 0)
                selection = 15;
            reel->selection = selection;
        }
        else if ((buttons & 0x4000) != 0)
        {
            sint16 prev;
            reel->step = 1;
            reel->mode = NAME_REEL_STEP;
            menu_state.sound = 0;
            prev = reel->selection + 1;
            if (prev >= 16)
                prev = 0;
            reel->prev = prev;
        }
        else if ((buttons & 0x8000) != 0 && idx > 0 && (uint8)name_reels.input_gate != 0)
        {
            menu_state.sound = 9;
            name_reels.input_gate = name_reels.input_gate & 0xFF00u | 0;
            reel->mode = NAME_REEL_IDLE;
            name_reels.slots[idx - 1].mode = NAME_REEL_SELECTED;
        }
        else if ((buttons & 0x2000) != 0 && idx < 4 && (uint8)name_reels.input_gate != 0)
        {
            menu_state.sound = 9;
            name_reels.input_gate = name_reels.input_gate & 0xFF00u | 0;
            reel->mode = NAME_REEL_IDLE;
            name_reels.slots[idx + 1].mode = NAME_REEL_SELECTED;
        }
        else if ((buttons & 0x80) != 0 && (uint8)name_reels.input_gate != 0)
        {
            reel->mode = NAME_REEL_IDLE;
            name_reels.input_gate = name_reels.input_gate & 0xFF00u | 0;
            name_reels.input_gate = 0;
            menu_state.sound = 4;
        }
    }

    if ((uint8)name_reels.input_gate != 0 && (buttons & 0x20) != 0)
    {
        for (idx = 0; idx < 5; ++idx)
        {
            if (name_reels.slots[idx].mode == NAME_REEL_STOPPING)
                return idx + 1;
        }
        for (idx = 0; idx < 5; ++idx)
        {
            NAME_REEL *reel = &name_reels.slots[idx];
            sint32 selection = reel->selection;
            reel->step = -2;
            reel->mode = NAME_REEL_STOPPING;
            reel->counter = 6;
            reel->prev = selection - 1;
            name_reels.slots[idx].target = random_next() & 15;
            if (reel->prev < 0)
                reel->prev = selection + 15;
            reel->selection = selection - 1;
            if (reel->selection < 0)
                reel->selection = selection + 15;
        }
    }

    if ((uint8)name_reels.input_gate != 0 && (buttons & 0x80) != 0 && active_count == 0)
    {
        for (idx = 0; idx < 5; ++idx)
        {
            name_reels.slots[idx].mode = NAME_REEL_IDLE;
            name_reels.slots[idx].counter = 0;
        }
        name_reels.slots[0].mode = NAME_REEL_SELECTED;
        name_reels.input_gate = 1;
        menu_state.sound = 4;
    }

    for (idx = 0; idx < 5; ++idx)
    {
        sint16 selection = name_reels.slots[idx].selection;
        uint32 digit = selection < 0 ? 15u : (uint16)selection;
        encoded += digit << 4 * (4 - idx);
    }
    name_reels.code = encoded;
    return 0;
}

uint8 name_check_reels(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x80059A34, "MAIN.EXE");
    for (idx = 0; idx < 5; ++idx)
    {
        sint16 mode = name_reels.slots[idx].mode;
        if (mode != NAME_REEL_IDLE && mode != NAME_REEL_SELECTED)
            return 0;
    }
    return 1;
}

sint32 name_reset_reels(void)
{
    uint32 encoded = name_reels.code;
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x80059A98, "MAIN.EXE");
    name_reels.input_gate = 0;
    for (idx = 0; idx < 5; ++idx)
    {
        uint16 val = encoded >> 4 * (4 - idx) & 15;
        name_reels.slots[idx].prev = val;
        name_reels.slots[idx].selection = val;
    }
    for (idx = 0; idx < 5; ++idx)
    {
        SPRITE_RENDER *spr = spr_render_at(recs[41].data, recs[41].val);
        NAME_REEL *reel = name_reels.slots + idx;
        reel->step = 0;
        reel->counter = 0;
        reel->mode = NAME_REEL_IDLE;
        spr->height = 8;
    }
    return name_update_reels(&input_pads[0]);
}

sint32 time_copy_chars7(char *src, char *dst)
{
    sint32 idx;
    uint32 result;

    FUNCTION_MARKER(0x80059BB4, "MAIN.EXE");
    for (idx = 0; idx < 6; ++idx)
    {
        result = (uint8)src[idx + 1u];
        dst[idx] = result;
    }
    result = (uint8)src[7u];
    dst[7u] = 0;
    dst[6u] = result;
    return result;
}

sint32 name_bytes_copy8(char *src, char *dst)
{
    sint32 idx;
    uint32 result = 0;

    FUNCTION_MARKER(0x80059C10, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
    {
        result = (uint8)src[idx];
        dst[idx] = result;
    }
    return result;
}
