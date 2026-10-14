#include "upgrades.h"
#include "disp.h"
#include "game.h"
#include "input.h"
#include "menu.h"
#include "menu_select.h"
#include "movie.h"
#include "name.h"
#include "profile.h"
#include "race.h"
#include "save.h"
#include "sprite.h"
#include "text.h"
#include "track.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdlib.h>

static PROFILE_GRID boat_custom_grid;
static PROFILE_GRID boat_preview_grid;
static sint16 boat_grid_rows[3];

static const PROFILE_GRID profile_grid_defaults[10] = {
    {{2, 1, 1, 2}, {3, 2, 2, 2}, {0, 0, 0, 0}}, {{1, 1, 2, 2}, {2, 2, 3, 2}, {0, 0, 0, 0}}, {{1, 2, 1, 2}, {2, 3, 2, 2}, {0, 0, 0, 0}}, {{2, 2, 2, 4}, {5, 4, 4, 4}, {0, 0, 0, 0}}, {{2, 2, 2, 4}, {4, 4, 5, 4}, {0, 0, 0, 0}}, {{2, 2, 2, 4}, {4, 5, 4, 4}, {0, 0, 0, 0}}, {{5, 4, 4, 5}, {5, 5, 5, 5}, {2, 0, 0, 2}}, {{4, 4, 5, 4}, {5, 5, 5, 4}, {0, 0, 2, 2}}, {{5, 5, 5, 5}, {5, 5, 5, 5}, {0, 0, 0, 0}}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

static void profile_copy_grid_defaults(PLAYER_PROFILE *profile)
{
    uint32 row;

    for (row = 0; row < 10; ++row)
        profile->grid[row] = profile_grid_defaults[row];
}

static sint32 upgrades_pack_tpage(sint32 tp, sint32 abr, uint32 x, uint32 y)
{
    if (GetGraphType() == 1 || GetGraphType() == 2)
        return (tp & 3) << 9 | (abr & 3) << 7 | (y & 0x300) >> 3 | (x & 0x3FF) >> 6;
    return getTPage(tp, abr, x, y);
}

static sint32 upgrades_pack_clut(uint32 x, uint32 y)
{
    return getClut(x, y) & 0xFFFF;
}

static void upgrades_set_prim_alternate(POLY_FT4 *prim, sint32 alternate)
{
    if (alternate != 0)
        prim->rgbc.code |= 2;
    else
        prim->rgbc.code &= 0xFD;
}

typedef struct
{
    sint8 orient;
    sint16 recs[5];
    sint16 filled;
    sint16 empty;
} VEHICLE_GRID_COL;

typedef struct
{
    sint16 x;
    sint16 y;
} VEHICLE_GRID_POS;

// Grid columns and layouts from reviewed MAIN.EXE
static const VEHICLE_GRID_COL boat_grid_cols[4] = {
    {0, {0, 1, 2, 3, 4}, 23, 27},
    {1, {5, 6, 7, 8, 9}, 22, 26},
    {2, {10, 11, 12, 13, 14}, 21, 25},
    {3, {15, 16, 17, 18, 19}, 20, 24},
};

static const VEHICLE_GRID_POS boat_grid_pos[2][4][5] = {
    {
        {{40, 25}, {28, 19}, {20, 12}, {8, 6}, {0, 0}},
        {{51, 25}, {51, 19}, {51, 12}, {51, 6}, {51, 0}},
        {{40, 30}, {28, 30}, {20, 30}, {8, 30}, {0, 30}},
        {{51, 30}, {51, 30}, {51, 30}, {51, 30}, {51, 30}},
    },
    {
        {{15, 10}, {12, 8}, {8, 5}, {0, 2}, {0, 0}},
        {{24, 10}, {24, 8}, {24, 5}, {24, 2}, {24, 0}},
        {{15, 15}, {12, 15}, {8, 15}, {0, 15}, {0, 15}},
        {{24, 15}, {24, 15}, {24, 15}, {24, 15}, {24, 15}},
    },
};

static sint32 upgrades_build_grid(sint16 rec_grp, const PROFILE_GRID *profile_row, sint16 offset_x, sint16 offset_y, const VEHICLE_GRID_POS pos[4][5])
{
    UI_RECORD *recs;
    sint32 outer;
    sint32 inner;

    recs = spr_recs + rec_grp;
    for (outer = 0; outer < 4; ++outer)
        spr_apply_prim_vram_mask(recs + boat_grid_cols[outer].empty);
    for (outer = 0; outer < 4; ++outer)
    {
        sint8 orientation = boat_grid_cols[outer].orient;
        sint32 column = outer;
        for (inner = 0; inner < 5; ++inner)
        {
            UI_RECORD *target = recs + boat_grid_cols[outer].recs[inner];
            UI_RECORD *src_rec;
            sint32 alternate = 0;
            SPRITE_RENDER *src_prim;
            SPRITE_RENDER *target_prim;
            sint16 prim_idx;
            SPRITE_RENDER *prim_base;
            uint32 tpage_x;
            uint32 tpage_y;
            uint32 clut_x;
            uint32 clut_y;

            spr_set_orient(target, orientation);
            target->type = profile_row->limit[column] - 1 >= inner;
            target->x = offset_x + pos[outer][inner].x;
            target->y = offset_y + pos[outer][inner].y;
            if (inner < profile_row->choice[column])
                src_rec = recs + boat_grid_cols[outer].filled;
            else
            {
                src_rec = recs + boat_grid_cols[outer].empty;
                alternate = 1;
            }
            prim_idx = src_rec->val;
            prim_base = src_rec->data;
            src_prim = spr_render_at(prim_base, prim_idx);
            prim_idx = target->val;
            tpage_x = src_prim->tpage_x;
            tpage_y = src_prim->tpage_y;
            prim_base = target->data;
            target_prim = spr_render_at(prim_base, prim_idx);
            target_prim->quad.p[1].attr = upgrades_pack_tpage(0, 3, tpage_x, tpage_y);
            clut_x = src_prim->clut_x;
            clut_y = src_prim->clut_y;
            target_prim->quad.p[0].attr = upgrades_pack_clut(clut_x, clut_y);
            upgrades_set_prim_alternate(&target_prim->quad, alternate);
            setRGB0(&target_prim->quad, 208, 208, 208);
            DrawSync(0);
        }
    }
    return 0;
}

void upgrades_enable_prim(POLY_FT4 *prim, sint32 enabled)
{
    if (enabled != 0)
        prim->rgbc.code |= 1;
    else
        prim->rgbc.code &= 0xFE;
}

PROFILE_GRID *profile_grid_row(sint16 idx)
{
    FUNCTION_MARKER(0x8005E354, "MAIN.EXE");
    if (game_selection.mode == 7)
        return &boat_custom_grid;
    return &profile_cur()->grid[idx];
}

typedef struct
{
    sint16 recs[3];
    sint16 sel[3];
    sint16 initial[3];
    sint16 pending[3];
    uint16 disabled[3];
    sint16 column;
    sint16 focus;
    uint16 moving;
    sint16 limit;
    sint16 highlight;
    sint16 total;
    sint16 target;
} VEHICLE_GRID_EDITOR;

// Grid editor roots from MAIN.EXE 800B69B4/69D4/6A64/6A6A/6A8A
// Transition roots from 800B6B36/6B48/6B54/6B5A/6B70/6BD4/6BE0
static VEHICLE_GRID_EDITOR boat_grid_editor;

static sint32 profile_get_grid_byte(sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E3BC, "MAIN.EXE");
    return profile_grid_row(row)->choice[column];
}

sint32 profile_get_table_byte(const PLAYER_PROFILE *data, sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E448, "MAIN.EXE");
    if (game_selection.mode == 7)
        return boat_custom_grid.choice[column];
    return data->grid[row].choice[column];
}

static void upgrades_clear_grid(PROFILE_GRID *data)
{
    sint32 idx;

    FUNCTION_MARKER(0x8005E4A4, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
    {
        data->choice[idx] = 0;
        data->limit[idx] = 0;
        data->offset[idx] = 0;
    }
}

static sint32 profile_get_row_offset(sint16 row, sint16 column)
{
    FUNCTION_MARKER(0x8005E520, "MAIN.EXE");
    return profile_grid_row(row)->offset[column];
}

sint32 profile_copy_templates(sint16 profile_idx)
{
    PLAYER_PROFILE *dst;
    uint8 mode;

    FUNCTION_MARKER(0x8005E560, "MAIN.EXE");
    mode = game_selection.players;
    if (mode == 1)
        dst = profile_at(profile_idx);
    else if (mode == 2)
        dst = profile_at(3);
    else
        dst = profile_at(4);
    profile_copy_grid_defaults(dst);
    return 0;
}

sint32 upgrades_apply_race_result(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 result = 12;

    FUNCTION_MARKER(0x8005E624, "MAIN.EXE");
    game_selection.event_arg = 0;
    game_selection.mode = game_selection.prev_mode;
    if (result_state.status == 2)
    {
        PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
        sint16 column = result_state.column;
        state->result = 2;
        if (profile_data->course < 5u)
            profile_data->course++;
        game_selection.selection = -1;
        if (profile_get_row_offset(race_selection.boats[0], column) == 0)
        {
            menu_notice.val = 5 * column;
            result = profile_get_grid_byte(race_selection.boats[0], column);
            menu_notice.type = 1;
            game_selection.event = 13;
            menu_notice.val = menu_notice.val + result;
            return result;
        }
        menu_notice.boat = -1;
        if (column == 0)
            menu_notice.boat = 5;
        else if (column == 1)
            menu_notice.boat = 3;
        else if (column == 2)
            menu_notice.boat = 4;
        if (profile_unlock(menu_notice.boat) != 0)
        {
            menu_notice.type = 2;
            game_selection.event = 14;
            menu_notice.val = menu_notice.boat;
            return 14;
        }
        result = 10;
    }
    game_selection.event = result;
    return result;
}

sint32 profile_build_vehicle_grid(sint16 rec_grp, sint16 profile_row, sint16 offset_x, sint16 offset_y)
{
    const PROFILE_GRID *profile;
    PROFILE_GRID reward;

    FUNCTION_MARKER(0x8005E7DC, "MAIN.EXE");
    if (profile_row == -1)
    {
        PLAYER_PROFILE *native = profile_cur();
        uint32 column;

        // Fourth-column reward spans progression and availability
        reward.choice[0] = native->progress.courses[3][4].state;
        reward.choice[1] = native->progress.courses[3][5].state;
        reward.choice[2] = native->availability[0];
        reward.choice[3] = native->availability[1];
        for (column = 0; column < 4; ++column)
        {
            reward.limit[column] = native->availability[column + 2];
            reward.offset[column] = native->availability[column + 6];
        }
        profile = &reward;
    }
    else
        profile = &profile_cur()->grid[profile_row];
    return upgrades_build_grid(rec_grp, profile, offset_x, offset_y, boat_grid_pos[0]);
}

static sint32 upgrades_render(sint16 rec_grp, sint32 unused2, sint16 offset_x, sint16 offset_y, const PROFILE_GRID *profile_row)
{
    FUNCTION_MARKER(0x8005EB18, "MAIN.EXE");

    return upgrades_build_grid(rec_grp, profile_row, offset_x, offset_y, boat_grid_pos[1]);
}

static sint32 upgrades_draw_grid_lines(sint16 color, sint16 left, sint16 top, sint32 rows)
{
    sint16 right = left + 10;
    sint16 bottom = top + 30;
    sint16 row_count = rows + 1u;
    sint32 idx;
    sint32 line;

    FUNCTION_MARKER(0x8005EE38, "MAIN.EXE");
    if (color == 0)
        disp_set_line_color(216, 232, 56);
    else if (color == 1)
        disp_set_line_color(192, 0, 0);
    else if (color == 2)
        disp_set_line_color(192, 0, 192);
    for (idx = 0; idx < row_count; ++idx)
    {
        sint16 y = bottom - 6 * (idx + 1);
        for (line = 1; line < 6; ++line)
            disp_queue_line_seg(left + 1, y + line, right - 1, y + line);
    }
    for (idx = 0; idx < 4; ++idx)
    {
        sint16 y = top + 6 + 6 * idx;
        disp_set_line_color(192, 192, 192);
        disp_queue_line_seg(left, y, right, y);
        disp_set_line_color(32, 32, 32);
        disp_queue_line_seg(left, y, right, y);
    }
    return disp_queue_beveled_rect_outline(left, top, right, bottom);
}

static void upgrades_grid_column(const PROFILE_GRID *row, sint32 col)
{
    sint16 selection;
    sint16 count;

    upgrades_clear_grid(&boat_preview_grid);
    boat_preview_grid.choice[col] = row->choice[col];
    selection = menu_boat_select.boat;
    boat_preview_grid.limit[col] = row->limit[col];
    upgrades_render(37 + 28 * col, selection, 50 + 140 * col, 500, &boat_preview_grid);
    count = row->choice[col] - 1;
    boat_grid_rows[col] = count;
    count = boat_grid_rows[col];
    upgrades_draw_grid_lines(col, 65 + 140 * col, 135, count);
    spr_recs[count + 124 + 5 * col].type = 1;
    text_menu[count + 20 + 5 * col].visible = 1;
    text_menu[2 * count + 47 + 10 * col].visible = 1;
    text_menu[2 * count + 48 + 10 * col].visible = 1;
}

void upgrades_update_grid(void)
{
    sint16 track = menu_state.scr;
    sint16 selection;
    MENU_DESC *table;
    PROFILE_GRID *row;
    sint32 idx;
    sint32 available;

    FUNCTION_MARKER(0x8005F06C, "MAIN.EXE");
    selection = menu_boat_select.boat;
    table = menu_cfg[track].descs;
    table->rec = selection;
    selection = menu_boat_select.boat;
    row = &profile_cur()->grid[selection];
    for (idx = 0; idx < 9; ++idx)
    {
        spr_recs[idx].type = 0;
        text_menu[idx + 1].visible = 0;
    }
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 offset = idx;
        spr_recs[offset + 124].type = 0;
        spr_recs[offset + 129].type = 0;
        spr_recs[offset + 134].type = 0;
    }
    profile_build_vehicle_grid(9, menu_boat_select.boat, 364, 55);
    for (idx = 0; idx < 5; ++idx)
    {
        text_menu[idx + 10].visible = 0;
        text_menu[idx + 15].visible = 0;
        text_menu[idx + 35].visible = 0;
        text_menu[idx + 20].visible = 0;
        text_menu[idx + 25].visible = 0;
        text_menu[idx + 30].visible = 0;
        text_menu[2 * idx + 47].visible = 0;
        text_menu[2 * idx + 48].visible = 0;
        text_menu[2 * idx + 57].visible = 0;
        text_menu[2 * idx + 58].visible = 0;
        text_menu[2 * idx + 67].visible = 0;
        text_menu[2 * idx + 68].visible = 0;
    }
    for (idx = 0; idx < 3; ++idx)
        spr_recs[idx + 121].type = 0;
    selection = menu_boat_select.boat;
    text_menu[selection + 1].visible = 1;
    text_menu[row->choice[3] + 9].visible = 1;
    text_menu[row->choice[3] + 14].visible = 1;
    text_menu[row->choice[3] + 34].visible = 1;

    upgrades_grid_column(row, 0);

    upgrades_grid_column(row, 1);

    upgrades_grid_column(row, 2);

    available = profile_is_unlocked(menu_boat_select.boat) != 0;
    text_menu[42].visible = available;
    spr_recs[141].type = available;
    if (available != 0)
    {
        sint32 conflict = 0;
        if (boat_grid_rows[0] >= 2 || boat_grid_rows[1] >= 2 || boat_grid_rows[2] >= 2)
            conflict = 1;
        if (boat_grid_rows[0] > 0)
        {
            if (boat_grid_rows[1] > 0)
                conflict = 1;
            if (boat_grid_rows[2] > 0)
                conflict = 1;
        }
        if (boat_grid_rows[1] > 0 && boat_grid_rows[2] > 0)
            conflict = 1;
        if (conflict == 0)
            available = 0;
    }
    text_menu[43].visible = available;
    UI_RECORD *recs = spr_recs;
    recs[142].type = available;
}

sint32 upgrades_process_grid_input(PAD *input)
{
    sint16 suppress_grid = disp_state.line_count;
    sint32 result;

    FUNCTION_MARKER(0x8005F71C, "MAIN.EXE");
    if (suppress_grid == 0)
    {
        upgrades_draw_grid_lines(0, 65, 135, boat_grid_rows[0]);
        upgrades_draw_grid_lines(1, 205, 135, boat_grid_rows[1]);
        upgrades_draw_grid_lines(2, 345, 135, boat_grid_rows[2]);
    }
    result = menu_state.input_enabled;
    if (result != 0)
    {
        if (text_menu[43].visible == 1 && (input->cur & 0x40u) != 0)
        {
            menu_state.phase = 7;
            menu_state.sound = 1;
        }
        result = 1;
        if (text_menu[42].visible == 1)
        {
            result = 8;
            if ((input->cur & 0x20u) != 0)
            {
                menu_state.phase = 8;
                menu_state.sound = 1;
                result = 1;
            }
        }
    }
    return result;
}

sint32 upgrades_init_custom_grid(void)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *row = &profile_cur()->grid[menu_boat_select.boat];
    sint16 selections[3];
    sint32 idx;

    FUNCTION_MARKER(0x8005F82C, "MAIN.EXE");
    for (idx = 0; idx < 9; ++idx)
        recs[idx + 2].type = idx == menu_boat_select.boat;
    for (idx = 0; idx < 5; ++idx)
    {
        recs[idx + 11].type = 0;
        recs[idx + 16].type = 0;
        recs[idx + 21].type = 0;
        menu[2 * idx + 7].visible = 0;
        menu[2 * idx + 8].visible = 0;
        menu[2 * idx + 17].visible = 0;
        menu[2 * idx + 18].visible = 0;
        menu[2 * idx + 27].visible = 0;
        menu[2 * idx + 28].visible = 0;
    }
    for (idx = 0; idx < 3; ++idx)
        menu[idx + 37].visible = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        uint32 rec_base = idx * 5 + 11;
        uint32 menu_base = 125u + 160u * idx;
        selections[idx] = row->choice[idx] - 1;
        upgrades_draw_grid_lines(idx, 65 + 140 * idx, 88, selections[idx]);
        recs[rec_base + selections[idx]].type = 1;
        menu[2 * selections[idx] + 10 * idx + 7].visible = 1;
        menu[2 * selections[idx] + 10 * idx + 8].visible = 1;
    }
    boat_grid_editor.recs[0] = 11;
    boat_grid_editor.sel[0] = selections[0];
    boat_grid_editor.initial[0] = selections[0];
    boat_grid_editor.recs[1] = 16;
    boat_grid_editor.sel[1] = selections[1];
    boat_grid_editor.initial[1] = selections[1];
    boat_grid_editor.recs[2] = 21;
    boat_grid_editor.column = 1;
    boat_grid_editor.sel[2] = selections[2];
    boat_grid_editor.initial[2] = selections[2];
    boat_grid_editor.moving = 0;
    return menu_move_spr_rec(recs + (boat_grid_editor.recs[1] + boat_grid_editor.sel[1]), 1, 10, 8, 1, 1);
}

sint32 upgrades_update_custom_grid(PAD *input)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    sint16 original_column = boat_grid_editor.column;
    UI_RECORD *active_rec = recs + (boat_grid_editor.recs[original_column] + boat_grid_editor.sel[original_column]);
    sint32 idx;

    FUNCTION_MARKER(0x8005FC28, "MAIN.EXE");
    if ((active_rec->command | active_rec->returning << 8) == 257)
    {
        if (boat_grid_editor.moving == 0)
            active_rec->command = 2;
        goto draw_grids;
    }
    if (active_rec->command != 2 || menu_state.input_enabled == 0)
    {
        if (active_rec->command == 0)
        {
            if (boat_grid_editor.moving != 0)
            {
                sint16 column = boat_grid_editor.column;
                sint16 selection = boat_grid_editor.target;
                sint16 base = column == 0 ? 7 : column == 1 ? 17 : 27;
                active_rec->type = 0;
                boat_grid_editor.moving = 0;
                boat_grid_editor.sel[column] = selection;
                active_rec = recs + (boat_grid_editor.recs[column] + selection);
                active_rec->type = 1;
                for (idx = 0; idx < 5; ++idx)
                {
                    menu[base + 2 * idx].visible = 0;
                    menu[base + 2 * idx + 1].visible = 0;
                }
                menu[base + 2 * selection].visible = 1;
                menu[base + 2 * selection + 1].visible = 1;
                menu[column + 37].visible = selection != boat_grid_editor.initial[column];
            }
            menu_move_spr_rec(active_rec, 1, 10, 8, 1, 1);
        }
        goto draw_grids;
    }
    if ((input->cur & 0x40u) != 0)
    {
        for (idx = 0; idx < 3; ++idx)
            boat_custom_grid.choice[idx] = boat_grid_editor.sel[idx] + 1u;
        uint16 val = profile_get_grid_byte(menu_boat_select.boat, 3);
        // Original SB preserves the adjacent first limit
        boat_custom_grid.choice[3] = val;
        game_selection.mode = 7;
        race_selection.special = 1;
        race_selection.resource = 0;
        name_reels.code = name_reels.code & 0xFFFF0000 | 32168;
        race_selection.boats[0] = menu_boat_select.boat;
        return menu_select_commit();
    }
    {
        sint32 movement = 0;
        sint16 column = boat_grid_editor.column;
        sint16 selection;
        if ((input->cur & 0x8000u) != 0)
            movement = 1;
        if ((input->cur & 0x2000u) != 0)
            movement = 2;
        if (column == 1)
        {
            if (movement == 1)
            {
                menu_state.sound = 9;
                boat_grid_editor.column = 0;
            }
            if (movement == 2)
            {
                menu_state.sound = 9;
                boat_grid_editor.column = 2;
            }
        }
        else if (column == 2 && movement == 1 || column == 0 && movement == 2)
        {
            menu_state.sound = 9;
            boat_grid_editor.column = 1;
        }
        column = boat_grid_editor.column;
        selection = boat_grid_editor.sel[column];
        if ((input->cur & 0x4000u) != 0 && selection > 0 && boat_grid_editor.initial[column] == selection - 1)
        {
            menu_state.sound = 9;
            active_rec->command = 1;
            boat_grid_editor.moving = 1;
            boat_grid_editor.target = selection - 1;
            if (boat_grid_editor.target == -1)
                boat_grid_editor.target = 4;
        }
        if ((input->cur & 0x1000u) != 0 && selection < 4 && selection == boat_grid_editor.initial[column])
        {
            menu_state.sound = 9;
            active_rec->command = 1;
            boat_grid_editor.moving = 1;
            boat_grid_editor.target = selection + 1;
            if (boat_grid_editor.target == 5)
                boat_grid_editor.target = 0;
        }
    }
    if (original_column != boat_grid_editor.column)
    {
        sint16 column = boat_grid_editor.column;
        active_rec->command = 1;
        active_rec = recs + (boat_grid_editor.recs[column] + boat_grid_editor.sel[column]);
        menu_move_spr_rec(active_rec, 1, 10, 8, 1, 1);
    }

draw_grids:
    upgrades_draw_grid_lines(0, 65, 88, boat_grid_editor.sel[0]);
    upgrades_draw_grid_lines(1, 205, 88, boat_grid_editor.sel[1]);
    return upgrades_draw_grid_lines(2, 345, 88, boat_grid_editor.sel[2]);
}

sint32 upgrades_init_upgrade_grid(void)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *selection = &profile_cur()->grid[menu_boat_select.boat];
    sint32 idx;
    sint32 matches = 0;

    FUNCTION_MARKER(0x80060228, "MAIN.EXE");
    for (idx = 0; idx < 5; ++idx)
    {
        sint32 grp;
        for (grp = 0; grp < 6; ++grp)
            recs[grp * 5 + idx + 2].type = 0;
        menu[2 * idx + 7].visible = 0;
        menu[2 * idx + 8].visible = 0;
        menu[2 * idx + 17].visible = 0;
        menu[2 * idx + 18].visible = 0;
        menu[2 * idx + 27].visible = 0;
        menu[2 * idx + 28].visible = 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        boat_grid_editor.pending[idx] = 0;
        boat_grid_editor.disabled[idx] = 0;
        menu[idx + 37].visible = 0;
    }
    menu[47].visible = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        sint16 sel = selection->choice[idx] - 1;
        uint32 rec_offset = idx * 5 + 17;
        uint32 menu_offset = 125u + 144u * idx;
        recs[rec_offset + sel].type = 1;
        menu[2 * sel + 9 * idx + 7].visible = 1;
        menu[2 * sel + 9 * idx + 8].visible = 1;
        if (idx == 0)
        {
            boat_grid_editor.recs[0] = 17;
            boat_grid_editor.sel[0] = sel;
            boat_grid_editor.initial[0] = sel;
            if (sel + 1 == selection->limit[0])
                boat_grid_editor.disabled[0] = 1;
        }
        else if (idx == 1)
        {
            boat_grid_editor.recs[1] = 22;
            boat_grid_editor.sel[1] = sel;
            boat_grid_editor.initial[1] = sel;
            if (sel + 1 == selection->limit[1])
                boat_grid_editor.disabled[1] = 1;
        }
        else
        {
            boat_grid_editor.recs[2] = 27;
            boat_grid_editor.sel[2] = sel;
            boat_grid_editor.initial[2] = sel;
            if (sel + 1 == selection->limit[2])
                boat_grid_editor.disabled[2] = 1;
        }
    }
    boat_grid_editor.column = 1;
    boat_grid_editor.moving = 0;
    menu_move_spr_rec(recs + (boat_grid_editor.recs[1] + boat_grid_editor.sel[1]), 1, 10, 8, 1, 1);
    menu[1].visible = 0;
    menu[3].visible = 0;
    recs[34].type = 0;
    menu[41].visible = 0;
    for (idx = 0; idx < 30; ++idx)
        menu[idx + 47].visible = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        if (boat_grid_editor.initial[idx] + 1 == selection->limit[idx])
            ++matches;
    }
    if (matches == 3)
    {
        menu[42].visible = 0;
        menu[43].visible = 0;
        recs[33].type = 0;
    }
    return matches;
}

sint32 upgrades_update_upgrade_grid(PAD *input)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    PROFILE_GRID *row = &profile_cur()->grid[menu_boat_select.boat];
    sint16 original_column = boat_grid_editor.column;
    UI_RECORD *active_rec = recs + (boat_grid_editor.recs[original_column] + boat_grid_editor.sel[original_column]);
    sint32 total;
    sint32 idx;

    FUNCTION_MARKER(0x80060704, "MAIN.EXE");
    if ((active_rec->command | active_rec->returning << 8) == 257)
    {
        if (boat_grid_editor.moving == 0)
            active_rec->command = 2;
        goto update_summary;
    }
    if (active_rec->command != 2 || menu_state.input_enabled == 0)
    {
        if (active_rec->command == 0)
        {
            if (boat_grid_editor.moving != 0)
            {
                sint16 column = boat_grid_editor.column;
                sint16 selection = boat_grid_editor.target;
                sint16 base = column == 0 ? 7 : column == 1 ? 17 : 27;
                active_rec->type = 0;
                boat_grid_editor.moving = 0;
                boat_grid_editor.sel[column] = selection;
                active_rec = recs + (boat_grid_editor.recs[column] + selection);
                active_rec->type = 1;
                for (idx = 0; idx < 5; ++idx)
                {
                    menu[base + 2 * idx].visible = 0;
                    menu[base + 2 * idx + 1].visible = 0;
                }
                menu[base + 2 * selection].visible = 1;
                menu[base + 2 * selection + 1].visible = 1;
            }
            menu_move_spr_rec(active_rec, 1, 10, 8, 1, 1);
        }
        goto update_summary;
    }

    {
        sint32 movement = 0;
        sint16 column = boat_grid_editor.column;
        if ((input->cur & 0x8000u) != 0)
            movement = 1;
        if ((input->cur & 0x2000u) != 0)
            movement = 2;
        if (column == 1)
        {
            if (movement == 1 && boat_grid_editor.disabled[0] == 0)
            {
                boat_grid_editor.column = 0;
                menu_state.sound = 9;
            }
            else if (movement == 2 && boat_grid_editor.disabled[2] == 0)
            {
                boat_grid_editor.column = 2;
                menu_state.sound = 9;
            }
        }
        else if (column == 2 && movement == 1)
        {
            if (boat_grid_editor.disabled[1] == 0)
                boat_grid_editor.column = 1;
            else if (boat_grid_editor.disabled[0] == 0)
                boat_grid_editor.column = 0;
            if (boat_grid_editor.column != column)
                menu_state.sound = 9;
        }
        else if (column == 0 && movement == 2)
        {
            if (boat_grid_editor.disabled[1] == 0)
                boat_grid_editor.column = 1;
            else if (boat_grid_editor.disabled[2] == 0)
                boat_grid_editor.column = 2;
            if (boat_grid_editor.column != column)
                menu_state.sound = 9;
        }
    }

    total = boat_grid_editor.pending[0] + boat_grid_editor.pending[1] + boat_grid_editor.pending[2];
    if ((input->cur & 0x40u) != 0)
    {
        sint16 sel = boat_grid_editor.focus;
        sint32 other_count = 0;
        for (idx = 0; idx < 3; ++idx)
        {
            if (idx != sel && boat_grid_editor.pending[idx] != 0)
                ++other_count;
        }
        if (total == 0)
            menu_state.sound = 5;
        else
        {
            menu_state.sound = 4;
            if (other_count == 1)
            {
                if (row->limit[sel] >= (uint8)boat_grid_editor.limit)
                {
                    row->choice[sel] = boat_grid_editor.limit;
                    for (idx = 0; idx < 3; ++idx)
                    {
                        if (idx != sel && boat_grid_editor.pending[idx] != 0)
                        {
                            sint32 val = row->choice[idx] - 2;
                            row->choice[idx] = val > 0 ? val : 1;
                        }
                    }
                }
                else
                    menu_state.sound = 5;
            }
            else if (other_count == 2)
            {
                if (row->limit[sel] >= (uint8)boat_grid_editor.limit)
                {
                    row->choice[sel] = boat_grid_editor.limit;
                    for (idx = 0; idx < 3; ++idx)
                    {
                        if (idx != sel && boat_grid_editor.pending[idx] != 0)
                        {
                            sint32 val = row->choice[idx] - 1;
                            row->choice[idx] = val > 0 ? val : 1;
                        }
                    }
                }
                else
                    menu_state.sound = 5;
            }
            if (menu_boat_select.boat >= 3)
            {
                for (idx = 0; idx < 3; ++idx)
                    row->offset[idx] = row->choice[idx] == row->limit[idx] ? 2u : 0u;
            }
            menu_state.phase = 6;
        }
    }

    if ((input->cur & 0x20u) != 0)
    {
        sint32 matched = 0;
        sint16 column = boat_grid_editor.column;
        menu_state.sound = 4;
        for (idx = 0; idx < 3; ++idx)
        {
            if (boat_grid_editor.initial[idx] + 1 == row->limit[idx])
                ++matched;
        }
        if (matched != 3)
        {
            if (boat_grid_editor.pending[column] != 0)
            {
                boat_grid_editor.pending[column] = 0;
                if (boat_grid_editor.focus == column)
                {
                    boat_grid_editor.pending[0] = 0;
                    boat_grid_editor.pending[1] = 0;
                    boat_grid_editor.pending[2] = 0;
                    boat_grid_editor.disabled[2] = 0;
                    boat_grid_editor.disabled[1] = 0;
                    boat_grid_editor.disabled[0] = 0;
                    for (idx = 0; idx < 3; ++idx)
                    {
                        if (boat_grid_editor.initial[idx] + 1 == row->limit[idx])
                            boat_grid_editor.disabled[idx] = 1;
                    }
                }
            }
            else
            {
                boat_grid_editor.pending[column] = boat_grid_editor.sel[column] + 1;
                for (idx = 0; idx < 3; ++idx)
                {
                    if (boat_grid_editor.initial[idx] + 1 == row->limit[idx])
                        boat_grid_editor.disabled[idx] = 0;
                }
            }
            total = boat_grid_editor.pending[0] + boat_grid_editor.pending[1] + boat_grid_editor.pending[2];
            if (total == 0)
            {
                boat_grid_editor.focus = boat_grid_editor.column;
                boat_grid_editor.disabled[boat_grid_editor.column] = 0;
                for (idx = 0; idx < 3; ++idx)
                {
                    if (idx != boat_grid_editor.column && boat_grid_editor.initial[idx] == 0)
                        boat_grid_editor.disabled[idx] = 1;
                }
            }
        }
    }

    if (original_column != boat_grid_editor.column)
    {
        active_rec->command = 1;
        active_rec = recs + (boat_grid_editor.recs[boat_grid_editor.column] + boat_grid_editor.sel[boat_grid_editor.column]);
        menu_move_spr_rec(active_rec, 1, 10, 8, 1, 1);
    }
    for (idx = 0; idx < 3; ++idx)
    {
        menu[idx + 37].visible = 0;
        menu[idx + 44].visible = 0;
        if (boat_grid_editor.pending[idx] != 0)
        {
            if (idx == boat_grid_editor.focus)
                menu[idx + 44].visible = 1;
            else
                menu[idx + 37].visible = 1;
        }
    }

update_summary:
    total = boat_grid_editor.pending[0] + boat_grid_editor.pending[1] + boat_grid_editor.pending[2];
    if (total != 0)
    {
        menu[1].visible = 1;
        menu[42].visible = 0;
        menu[3].visible = 1;
        menu[43].visible = 0;
        menu[41].visible = 1;
        recs[34].type = 1;
    }
    else
    {
        menu[1].visible = 0;
        menu[42].visible = 1;
        menu[3].visible = 0;
        menu[43].visible = 1;
        menu[41].visible = 0;
        recs[34].type = 0;
    }
    {
        UI_RECORD *sel_rec = recs + boat_grid_editor.highlight;
        sint32 matched = 0;
        if (sel_rec->type == 1 && (sel_rec->command | sel_rec->returning << 8) == 257)
            sel_rec->command = 2;
        for (idx = 0; idx < 3; ++idx)
        {
            if (boat_grid_editor.initial[idx] + 1 == row->limit[idx])
                ++matched;
        }
        if (matched == 3)
        {
            menu[42].visible = 0;
            menu[43].visible = 0;
            recs[33].type = 0;
        }
    }
    if (total != boat_grid_editor.total)
    {
        boat_grid_editor.total = total;
        for (idx = 0; idx < 5; ++idx)
        {
            recs[idx + 2].type = 0;
            recs[idx + 7].type = 0;
            recs[idx + 12].type = 0;
        }
        for (idx = 0; idx < 30; ++idx)
            menu[idx + 47].visible = 0;
        if (total != 0)
        {
            sint32 other_total = 0;
            sint16 sel = boat_grid_editor.focus;
            sint16 maximum;
            for (idx = 0; idx < 3; ++idx)
            {
                if (idx != sel)
                    other_total += boat_grid_editor.pending[idx];
            }
            maximum = boat_grid_editor.pending[sel] + (other_total > 2 ? 1 : 0);
            if (maximum >= 6)
                maximum = 5;
            if (maximum > row->limit[sel])
                maximum = row->limit[sel];
            boat_grid_editor.limit = maximum;
            boat_grid_editor.highlight = 5 * sel + maximum + 1;
            recs[boat_grid_editor.highlight].type = 1;
            menu[10 * sel + 2 * maximum + 45].visible = 1;
            menu[10 * sel + 2 * maximum + 46].visible = 1;
        }
    }
    for (idx = 0; idx < 3; ++idx)
    {
        UI_RECORD *rec = recs + (boat_grid_editor.recs[idx] + boat_grid_editor.sel[idx]);
        SPRITE_RENDER *prim = spr_render_at(rec->data, rec->val);
        sint16 disabled = boat_grid_editor.disabled[idx];
        prim->quad.p[1].attr = upgrades_pack_tpage(1, 3, prim->tpage_x, prim->tpage_y);
        prim->quad.p[0].attr = upgrades_pack_clut(prim->clut_x, prim->clut_y);
        upgrades_set_prim_alternate(&prim->quad, disabled);
        setRGB0(&prim->quad, disabled != 0 ? 32u : 128u, disabled != 0 ? 32u : 128u, disabled != 0 ? 32u : 128u);
        DrawSync(0);
    }
    upgrades_draw_grid_lines(0, 65, 78, boat_grid_editor.sel[0]);
    upgrades_draw_grid_lines(1, 205, 78, boat_grid_editor.sel[1]);
    upgrades_draw_grid_lines(2, 345, 78, boat_grid_editor.sel[2]);
    if (total != 0)
        return upgrades_draw_grid_lines(boat_grid_editor.focus, 345, 160, boat_grid_editor.limit - 1);
    return total;
}

sint32 upgrades_init_retry(void)
{
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    MENU_DESC *table = menu_cfg[menu_state.scr].descs;
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_COURSE *row = &profile_data->progress.courses[profile_data->level][profile_data->course];
    uint8 state = row->attempts;
    uint8 visible = state != 0;
    sint32 idx;

    FUNCTION_MARKER(0x80061754, "MAIN.EXE");
    if (visible != 0)
    {
        menu_course_select.rec = 12;
        menu_state.sound = -1;
        menu_course_select.blocked = 1;
        recs[3].x = 159;
    }
    else
    {
        menu_course_select.blocked = 0;
        recs[3].x = 60;
    }
    recs[2].type = visible;
    recs[4].type = visible;
    menu[3].visible = visible;
    menu[4].visible = visible;
    for (idx = 0; idx < 3; ++idx)
        menu[idx + 5].visible = 0;
    if (state != 0)
        menu[state + 4].visible = 1;
    table->enabled = visible;
    spr_apply_inset(recs + 2);
    return spr_disable_semitransparency(recs + 2);
}

sint32 upgrades_update_retry(PAD *input)
{
    sint32 result = 0;

    FUNCTION_MARKER(0x800618D0, "MAIN.EXE");
    if (menu_course_select.blocked != 0)
    {
        UI_RECORD *rec = spr_recs + 2;
        if (rec->command == 0)
            menu_move_spr_rec(rec, 1, 8, 6, 1, 1);
        result = spr_disable_semitransparency(rec);
    }
    if (menu_state.input_enabled != 0 && menu_state.exit_wait == 0)
    {
        PLAYER_PROFILE *profile_data = profile_cur();
        PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
        if (state->attempts != 0)
        {
            if ((input->cur & 0x10u) != 0)
            {
                menu_state.sound = 2;
                menu_state.phase = 10;
            }
            if ((input->cur & 0x40u) != 0)
            {
                TEXT_RECORD *rec = text_hud;
                menu_state.sound = 1;
                rec[0].x = 339;
                rec[0].y = 195;
                return menu_select_commit();
            }
        }
        else if ((input->cur & 0x40u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 10;
            state->result = 0;
        }
        menu_tex.rebuild = 1;
        menu_cfg[menu_state.phase].selection = 0;
    }
    return result;
}

sint32 upgrades_refresh_unlocks(void)
{
    PROFILE_GRID *row = &profile_cur()->grid[race_selection.boats[0]];
    sint32 idx;
    sint32 result = race_selection.boats[0] < 3u;

    FUNCTION_MARKER(0x80061A90, "MAIN.EXE");
    if (race_selection.boats[0] < 3u)
    {
        for (idx = 0; idx < 3; ++idx)
        {
            sint16 availability_idx = idx == 0 ? 5 : idx == 1 ? 3 : 4;
            uint8 state = 0;
            if (row->limit[idx] == row->choice[idx])
                state = profile_is_unlocked(availability_idx) != 0 ? 2 : 1;
            row->offset[idx] = state;
            result = idx + 1 << 16;
        }
    }
    return result;
}

sint32 upgrades_apply_upgrade(void)
{
    sint16 column = result_state.column;
    sint16 profile_row = race_selection.boats[0];
    PROFILE_GRID *row = &profile_cur()->grid[profile_row];

    sint32 idx;

    FUNCTION_MARKER(0x80061BB4, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
        text_set_menu_visible(idx + 2, idx == column);
    for (idx = 6; idx < 10; ++idx)
        text_set_menu_visible(idx, 0);
    if (row->choice[column] != row->limit[column])
        row->choice[column] = row->choice[column] + 1u;
    if (row->choice[column] == row->limit[column])
        text_set_menu_visible(9, 1);
    else
        text_set_menu_visible(row->choice[column] + 4, 1);
    if (row->choice[column] == row->limit[column])
    {
        uint8 state = 2;
        if (profile_row < 3)
        {
            sint16 availability_idx = column == 0 ? 5 : column == 1 ? 3 : 4;
            if (profile_is_unlocked(availability_idx) == 0)
                state = 1;
        }
        row->offset[column] = state;
    }
    save_flush();
    return profile_build_vehicle_grid(2, profile_row, 383, 169);
}

sint32 upgrades_leave_upgrade(sint16 val)
{
    FUNCTION_MARKER(0x80061D7C, "MAIN.EXE");
    return val;
}

sint32 upgrades_apply_unlock(void)
{
    PROFILE_GRID *row;

    FUNCTION_MARKER(0x80061D88, "MAIN.EXE");
    row = profile_grid_row(race_selection.boats[0]);
    row->offset[result_state.column] = 2;
    save_flush();
    spr_recs[31].type = 0;
    return profile_build_vehicle_grid(2, menu_notice.boat, 383, 169);
}

sint32 upgrades_update_reward(PAD *input)
{
    sint32 result = 1;

    FUNCTION_MARKER(0x80061DE4, "MAIN.EXE");
    if ((input->pressed & 0x40u) != 0)
    {
        menu_state.sound = 1;
        result = 11;
        if (menu_state.reward_notice == 0)
        {
            result = 3;
            if (menu_state.level_notice == 0)
                result = 5;
        }
        menu_state.phase = result;
    }
    return result;
}

void upgrades_init_progress(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_GRID *row = &profile_cur()->grid[race_selection.boats[0]];
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    PROFILE_COURSE *state_row = &profile_data->progress.courses[profile_data->level][profile_data->course];
    sint16 incomplete = 0;
    sint32 visible;
    sint32 idx;

    FUNCTION_MARKER(0x80061E40, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
    {
        if (row->offset[idx] != 2)
            ++incomplete;
    }
    visible = state_row->attempts != 0 && state_row->result != 2 && incomplete != 0;
    if (visible != 0)
    {
        menu_course_select.rec = 12;
        menu_course_select.blocked = 1;
        recs[3].x = 136;
    }
    else
    {
        menu_course_select.blocked = 0;
        recs[3].x = 60;
    }
    recs[2].type = visible;
    recs[4].type = visible;
    text_set_menu_visible(4, visible);
    text_set_menu_visible(12, 1 - visible);
    text_set_menu_visible(3, visible);
    visible = game_selection.mode != 1 && profile_data->level_result != 0;
    menu[5].visible = 0;
    menu[6].visible = 0;
    menu[7].visible = 0;
    menu[8].visible = 0;
    menu[9].visible = 0;
    menu[11].visible = 0;
    if (visible != 0)
    {
        if (profile_data->max_level == 1)
        {
            menu[5].visible = 1;
            menu[8].visible = 1;
            menu[5].y = 130;
            menu[8].y = 150;
            menu[10].y = 170;
        }
        else if (profile_data->max_level == 2)
        {
            menu[6].visible = 1;
            menu[9].visible = 1;
            menu[6].y = 130;
            menu[9].y = 150;
            menu[10].y = 170;
        }
        else if (profile_data->max_level == 3)
        {
            menu[7].visible = 1;
            menu[11].visible = 1;
            menu[7].y = 130;
            menu[11].y = 150;
            menu[10].y = 170;
        }
    }
    menu[10].visible = visible;
}

sint32 upgrades_update_progress(PAD *input)
{
    sint32 result = menu_state.input_enabled;

    FUNCTION_MARKER(0x80062144, "MAIN.EXE");
    if (menu_course_select.blocked != 0)
    {
        UI_RECORD *rec = spr_recs + 2;
        if (rec->command == 0)
            menu_move_spr_rec(rec, 1, 8, 6, 1, 1);
    }
    if (menu_state.input_enabled != 0)
    {
        result = (sint16)menu_state.exit_wait;
        if (result == 0)
        {
            PLAYER_PROFILE *profile_data = profile_cur();
            PROFILE_COURSE *state = &profile_data->progress.courses[profile_data->level][profile_data->course];
            sint32 incomplete = 0;
            sint32 idx;
            sint32 enabled = state->attempts;
            if (state->result == 2)
                enabled = 0;
            for (idx = 0; idx < 3; ++idx)
            {
                if (profile_cur()->grid[race_selection.boats[0]].offset[idx] != 2)
                    ++incomplete;
            }
            if (enabled != 0)
            {
                if ((input->cur & 0x10u) != 0)
                {
                    menu_state.sound = 1;
                    menu_state.phase = 10;
                }
                if ((input->cur & 0x40u) != 0)
                {
                    menu_state.sound = 1;
                    if (incomplete != 0)
                    {
                        TEXT_RECORD *rec = text_hud;
                        rec[0].x = 319;
                        rec[0].y = 195;
                        return menu_select_commit();
                    }
                }
            }
            else if ((input->cur & 0x40u) != 0)
            {
                menu_state.sound = 1;
                if (state->result != 2)
                    state->result = 0;
            }
            menu_state.phase = 10;
            menu_tex.rebuild = 1;
            result = 24 * menu_state.phase;
            menu_cfg[menu_state.phase].selection = 0;
        }
    }
    return result;
}

sint32 upgrades_refresh_grid_limits(void)
{
    sint32 profile_idx;
    sint32 row_idx;
    sint32 column;
    sint32 result = 0;

    FUNCTION_MARKER(0x80062394, "MAIN.EXE");
    for (profile_idx = 0; profile_idx < 3; ++profile_idx)
    {
        for (row_idx = 0; row_idx < 10; ++row_idx)
        {
            if (row_idx >= 3 && row_idx < 9)
            {
                PROFILE_GRID *row = &profile_at(profile_idx)->grid[row_idx];
                for (column = 0; column < 4; ++column)
                {
                    if (row->choice[column] == row->limit[column])
                        row->offset[column] = 2;
                }
            }
        }
        result = profile_idx + 1 < 3;
    }
    return result;
}

sint32 profile_cell_dispatch_complete(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    uint8 row_idx = profile_data->level;
    uint8 column = profile_data->course;
    PROFILE_COURSE *row = &profile_data->progress.courses[row_idx][column];

    FUNCTION_MARKER(0x80062478, "MAIN.EXE");
    if (row->result == 2)
        return track_clear_grp_item_flags(3);
    return 2;
}

sint32 profile_idx_map(void)
{
    FUNCTION_MARKER(0x800624D0, "MAIN.EXE");
    if (profile_selection.slot == 0)
        return 4;
    if (profile_selection.slot == 1)
        return 2;
    if (profile_selection.slot == 2)
        return 1;
    return 0;
}

sint32 upgrades_init_player_profile(uint32 slot)
{
    PLAYER_PROFILE *native = profile_at(slot);
    sint32 row;
    sint32 idx;

    FUNCTION_MARKER(0x80062530, "MAIN.EXE");
    native->reward_flags = 0;
    native->max_level = 0;
    native->level = 0;
    native->course = 0;
    for (row = 0; row < 3; ++row)
    {
        for (idx = 0; idx < 3; ++idx)
            native->progress.courses[row][idx].state = 4;
        for (idx = 3; idx < 6; ++idx)
            native->progress.courses[row][idx].state = 5;
        for (idx = 0; idx < 6; ++idx)
        {
            native->progress.courses[row][idx].attempts = 0;
            native->progress.courses[row][idx].result = 0;
        }
    }
    for (idx = 0; idx < 10; ++idx)
        native->availability[idx] = 0;
    native->availability[0] = 1;
    native->availability[1] = 1;
    native->availability[2] = 1;
    profile_copy_grid_defaults(native);
    return 10 << 16;
}
