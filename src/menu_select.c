#include "menu_select.h"
#include "boat.h"
#include "game.h"
#include "input.h"
#include "menu.h"
#include "name.h"
#include "profile.h"
#include "race.h"
#include "ranking.h"
#include "render.h"
#include "save.h"
#include "sound.h"
#include "sprite.h"
#include "text.h"
#include "timer.h"
#include "tournament.h"
#include "upgrades.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Course carousel text indices from MAIN.EXE 80082388
static const sint16 select_carousel_text[19] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 31, 30,
};

float hd_menu_course_step(void)
{
    if (menu_state.scr != 10 || !spr_recs[0].transition_active)
        return 0;
    switch (menu_course_select.phase)
    {
        case MENU_COURSE_NEXT_WAIT:
        case MENU_COURSE_NEXT_ENTER:
        case MENU_COURSE_PREV_WAIT:
        case MENU_COURSE_PREV_ENTER:
            return (sint16)(spr_recs[0].transition_step_x >> 16);
        default:
            return 0;
    }
}

// Special course label positions by language from MAIN.EXE 80098DB0
static const uint16 select_special_label_x[5] = {136, 136, 136, 136, 136};

typedef struct
{
    uint16 x;
    uint16 y;
    uint16 rec;
    uint16 rank;
} MENU_SELECT_COURSE_SLOT;

static MENU_SELECT_COURSE_SLOT select_course_slots[6];

typedef struct
{
    uint16 order;
    uint16 rec;
    uint16 label;
    uint16 x;
    uint16 y;
} MENU_SELECT_COURSE_UI;

const UI_RECORD *hd_menu_spr_parent(const UI_RECORD *rec)
{
    if (menu_state.scr == 10 && rec >= spr_recs + 49 && rec <= spr_recs + 54)
        return &spr_recs[select_course_slots[rec - spr_recs - 49].rec];
    return NULL;
}

// Original MAIN.EXE course maps at 80098D60/6C/8C/98/A4
static const MENU_SELECT_COURSE_UI select_course_ui[6] = {
    {0, 23, 4, 85, 107}, {1, 24, 5, 125, 83}, {2, 25, 6, 171, 71}, {3, 26, 7, 205, 65}, {4, 27, 8, 252, 60}, {5, 28, 9, 322, 56},
};

// Original MAIN.EXE course text map at 80098D3C
static const uint16 select_course_text[3][6] = {
    {0, 1, 1, 2, 2, 3},
    {3, 4, 4, 5, 5, 6},
    {7, 7, 8, 8, 9, 9},
};

// Original MAIN.EXE level records at 80098D84
static const uint16 select_level_recs[3] = {18, 19, 20};

typedef struct
{
    uint16 x;
    uint16 y;
    uint16 target_x;
    uint16 target_y;
} MENU_SELECT_CAROUSEL_SLOT;

static MENU_SELECT_CAROUSEL_SLOT boat_carousel[8];

// Original MAIN.EXE record order at 80098D2C
static const uint16 boat_carousel_order[8] = {3, 1, 0, 2, 4, 6, 7, 5};

typedef struct
{
    uint8 active;
    uint8 course;
    uint8 level;
    uint8 grid_row;
    PROFILE_PROGRESS progress;
} MENU_SELECT_SELECTION_BACKUP;

static MENU_SELECT_SELECTION_BACKUP select_selection_backups[3];

MENU_BOAT_SELECT menu_boat_select;

MENU_COURSE_SELECT menu_course_select;

uint8 *menu_select_save_backup(uint32 offset)
{
    uint32 relative;
    uint32 field;
    MENU_SELECT_SELECTION_BACKUP *backup;

    if (offset >= 1458 && offset < 1470)
    {
        relative = offset - 1458;
        backup = &select_selection_backups[relative / 4];
        switch (relative % 4)
        {
            case 0:
                return &backup->active;
            case 1:
                return &backup->course;
            case 2:
                return &backup->level;
            default:
                return &backup->grid_row;
        }
    }
    if (offset < 5420 || offset >= 5636)
        return NULL;
    relative = offset - 5420;
    backup = &select_selection_backups[relative / 72];
    field = relative % 24;
    if (relative % 72 < 24)
        return &backup->progress.courses[field / 6][field % 6].attempts;
    if (relative % 72 < 48)
        return &backup->progress.courses[field / 6][field % 6].result;
    return &backup->progress.courses[field / 6][field % 6].state;
}

static void menu_select_init_mode_rec(void)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x80059D8C, "MAIN.EXE");
    game_selection.menu_variant = 0;
    boat_racer_count = 8;
    profile_data = profile_cur();
    selection = menu_query_grp_val(3, 0);
    menu_state.series_mode = 0;
    result = selection < 2;
    if (selection == 1)
    {
        if (game_selection.prev_mode == 1)
            menu_select_restore_backup();
        game_selection.mode = 2;
        return;
    }
    if (selection >= 2)
    {
        uint32 level = selection - 2;
        result = level;
        if (selection < 5)
        {
            PROFILE_SERIES *series;
            sint32 idx;

            game_selection.mode = 1;
            menu_state.series_mode = 1;
            profile_data->level = level;
            profile_at(profile_selection.slot)->course = 0;
            series = profile_get_series();
            if (series->phase != 0)
            {
                race_selection.boats[0] = series->grid_row;
                result = (uint8)series->completed;
                profile_data->course = result;
            }
            else
            {
                result = (uint8)menu_boat_select.boat;
                series->completed = 0;
                series->phase = 1;
                race_selection.boats[0] = result;
                series->grid_row = result;
                for (idx = 15; idx >= 0; --idx)
                    series->points[idx] = 0;
            }
        }
        return;
    }
    if (selection == 0)
    {
        result = (sint16)game_selection.prev_mode;
        if (result == 1)
            menu_select_restore_backup();
        game_selection.mode = 0;
    }
}

void menu_select_apply_trans(void)
{
    PLAYER_PROFILE *profile = profile_cur();
    sint32 result;

    FUNCTION_MARKER(0x80053ECC, "MAIN.EXE");
    menu_save_desc_payloads(menu_state.scr);
    if (menu_state.scr == 3)
    {
        PROFILE_SERIES *series;

        menu_select_init_mode_rec();
        profile_reset_grid_flags();
        profile->reset_pending = 0;
        series = profile_get_series();
        if (menu_state.phase == 5)
        {
            result = (sint16)game_selection.mode;
            if (result != 0 && result != 2)
            {
                result = (sint16)series->phase;
                if (result == 2)
                {
                    menu_state.phase = 18;
                }
            }
        }
    }
    else if (menu_state.scr == 4)
    {
        profile_select_mode();
        profile_reset_grid_flags();
        profile->reset_pending = 0;
    }
}

static sint32 menu_select_update_mode_select_vis(void)
{
    sint16 track = menu_state.scr;
    MENU_DESC *table = menu_cfg[track].descs;
    sint32 idx;
    sint32 visible;
    sint16 selection;
    uint32 prim;
    UI_RECORD *recs;

    FUNCTION_MARKER(0x80053FA4, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
    {
        MENU_DESC *entry = table + idx;
        sint16 rec_idx = entry->rec;
        const MENU_VISIBILITY *list;
        sint32 child;
        visible = idx < 5;
        recs = spr_recs;
        recs[rec_idx].type = visible;
        list = entry->visibility;
        for (child = 0; child < list->count; ++child)
        {
            rec_idx = list->recs[child];
            recs = spr_recs;
            recs[rec_idx].type = visible;
        }
    }
    profile_build_vehicle_grid(8, menu_boat_select.boat, 344, 130);
    visible = profile_is_unlocked(menu_boat_select.boat) != 0;
    text_clear_boat_names();
    selection = menu_boat_select.boat;
    prim = 496u + 160u * selection;
    text_hud[31 + 10 * selection].visible = 1;
    text_hud[31 + 10 * selection].font = 1;
    recs = spr_recs;
    text_hud[31 + 10 * selection].x = 120;
    text_hud[31 + 10 * selection].y = 160;
    recs[40].type = visible;
    text_menu[4].visible = visible;
    return 1;
}

static sint32 menu_select_backup_selection(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    MENU_SELECT_SELECTION_BACKUP *backup = &select_selection_backups[profile_selection.slot];
    sint32 grp;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80059F38, "MAIN.EXE");
    backup->active = 1;
    profile_data->course = menu_course_select.course;
    backup->course = menu_course_select.course;
    backup->level = profile_data->level;
    race_selection.boats[0] = menu_boat_select.boat;
    backup->grid_row = menu_boat_select.boat;
    for (grp = 0; grp < 3; ++grp)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                PROFILE_COURSE *dst = &select_selection_backups[grp].progress.courses[row][column];
                const PROFILE_COURSE *src = &profile_at(grp)->progress.courses[row][column];
                dst->attempts = src->attempts;
                dst->result = src->result;
                dst->state = src->state;
            }
        }
    }
    return 0;
}

void menu_select_update_carousel(sint16 dir, PAD *input)
{
    sint32 idx;

    FUNCTION_MARKER(0x80054148, "MAIN.EXE");
    if (dir == 0)
    {
        sint32 result;
        if ((input->cur & 0x10u) != 0)
        {
            uint8 gate = menu_state.input_enabled;
            if (gate != 0)
            {
                sint16 state = menu_boat_select.phase;
                if (state == 0)
                {
                    uint8 pages = game_selection.players;
                    menu_state.sound = 2;
                    if (pages != 0)
                    {
                        sint16 page = menu_boat_select.player;
                        if (page != 0)
                        {
                            page--;
                            menu_state.phase = 5;
                            menu_boat_select.player = page;
                            menu_state.refresh = 1;
                            spr_clear_anim_recs();
                            menu_select_apply_trans();
                            page = menu_boat_select.player;
                            menu_boat_select.boat = race_selection.boats[page];
                        }
                        else
                        {
                            uint8 mode = game_selection.menu_variant;
                            menu_state.phase = 25;
                            if (mode == 0 && game_selection.mode == 0)
                                menu_select_backup_selection();
                        }
                    }
                    else
                    {
                        uint8 mode = game_selection.menu_variant;
                        menu_state.phase = 25;
                        if (mode == 0 && game_selection.mode == 0)
                            menu_select_backup_selection();
                    }
                }
            }
        }
        result = input->cur & 0x40u;
        if (result == 0)
            return;
        result = menu_state.input_enabled;
        if (result == 0)
            return;
        result = (sint16)menu_boat_select.phase;
        if (result != 0)
        {
            return;
        }

        {
            sint16 selection = menu_boat_select.boat;
            menu_state.sound = 1;
            result = profile_is_unlocked(selection);
            if (result == 0)
            {
                menu_state.sound = 5;
                return;
            }
            else
            {
                sint16 page = menu_boat_select.player;
                uint8 sel = menu_boat_select.boat;
                sint16 page_snapshot;
                uint8 pages;

                race_selection.boats[page] = sel;
                page_snapshot = menu_boat_select.player;
                pages = game_selection.players;
                if (page_snapshot + 1 == pages)
                {
                    sint16 mode = game_selection.mode;
                    if (mode == 0 || mode == 2 || page_snapshot != 0)
                        menu_state.phase = 10;
                    else
                        menu_state.phase = 18;
                }
                else
                {
                    TEXT_RECORD *menu;
                    char *output;

                    page_snapshot++;
                    menu = text_menu;
                    menu_state.refresh = 1;
                    output = menu[5].text;
                    menu_boat_select.player = page_snapshot;
                    menu_state.phase = 5;
                    name_copy_player_name_bytes(page_snapshot, output);
                    page_snapshot = menu_boat_select.player;
                    menu_boat_select.boat = race_selection.boats[page_snapshot];
                }
                spr_clear_anim_recs();
                menu_select_apply_trans();
                return;
            }
        }
    }
    if (dir == 1 || dir == 2)
    {
        sint32 table_idx = dir == 1 ? 7 : 1;
        for (idx = 0; idx < 8; ++idx)
        {
            UI_RECORD *rec;
            sint16 rec_idx;
            sint16 target_x;
            sint16 target_y;
            sint16 target0;
            sint16 target1;
            UI_RECORD *recs;
            if (table_idx == 8)
                table_idx = 0;
            rec_idx = boat_carousel_order[idx];
            const MENU_SELECT_CAROUSEL_SLOT *target = &boat_carousel[table_idx];
            target_x = target->x;
            target_y = target->y;
            target0 = target->target_x;
            target1 = target->target_y;
            recs = spr_recs;
            rec = recs + rec_idx;
            spr_init_trans(rec, target_x, target_y, target0, target1, 10);
            ++table_idx;
        }
        if (dir == 1)
        {
            sint16 selection = menu_boat_select.boat + 1u;
            menu_boat_select.boat = selection;
            if (selection == 8)
                menu_boat_select.boat = 0;
            menu_boat_select.rotation = 1;
        }
        else
        {
            sint16 selection = menu_boat_select.boat - 1u;
            menu_boat_select.boat = selection;
            if (selection < 0)
                menu_boat_select.boat = 7;
            menu_boat_select.rotation = 2;
        }
        menu_boat_select.phase = MENU_BOAT_MOVING;
        return;
    }
    if (dir == 3)
    {
        UI_RECORD saved_rec;
        UI_RECORD *saved = &saved_rec;
        sint16 idx0 = boat_carousel_order[0];
        UI_RECORD *recs = spr_recs;
        UI_RECORD *head = recs + idx0;
        uint8 state = head->transition_ticks;
        if (state == 5)
        {
            sint16 rotation = menu_boat_select.rotation;
            if (rotation == 1)
            {
                spr_copy_anim_rec_bytes(head, saved);
                for (idx = 0; idx < 7; ++idx)
                {
                    sint16 src_idx = boat_carousel_order[idx + 1];
                    sint16 dst_idx;
                    UI_RECORD *src;
                    UI_RECORD *dst;

                    recs = spr_recs;
                    dst_idx = boat_carousel_order[idx];
                    src = recs + src_idx;
                    dst = recs + dst_idx;
                    spr_copy_anim_rec_bytes(src, dst);
                }
                idx0 = boat_carousel_order[7];
                recs = spr_recs;
                spr_copy_anim_rec_bytes(saved, recs + idx0);
                idx0 = boat_carousel_order[0];
                recs = spr_recs;
                spr_apply_prim_vram_mask(recs + idx0);
                idx0 = boat_carousel_order[3];
                recs = spr_recs;
                spr_disable_semitransparency(recs + idx0);
            }
            rotation = menu_boat_select.rotation;
            if (rotation == 2)
            {
                UI_RECORD *last;

                idx0 = boat_carousel_order[7];
                recs = spr_recs;
                last = recs + idx0;
                spr_copy_anim_rec_bytes(last, saved);
                for (idx = 7; idx > 0; --idx)
                {
                    sint16 src_idx = boat_carousel_order[idx - 1];
                    sint16 dst_idx;
                    UI_RECORD *src;
                    UI_RECORD *dst;

                    recs = spr_recs;
                    dst_idx = boat_carousel_order[idx];
                    src = recs + src_idx;
                    dst = recs + dst_idx;
                    spr_copy_anim_rec_bytes(src, dst);
                }
                idx0 = boat_carousel_order[0];
                recs = spr_recs;
                spr_copy_anim_rec_bytes(saved, recs + idx0);
                idx0 = boat_carousel_order[4];
                recs = spr_recs;
                spr_apply_prim_vram_mask(recs + idx0);
                idx0 = boat_carousel_order[1];
                recs = spr_recs;
                spr_disable_semitransparency(recs + idx0);
            }
            {
                uint16 selection_bits = menu_boat_select.boat;
                sint32 selection = (sint16)(selection_bits - 2u);
                UI_RECORD *recs_snapshot;
                if (selection < 0)
                    selection = (sint16)(selection_bits + 6u);
                recs_snapshot = spr_recs;
                for (idx = 0; idx < 8; ++idx)
                {
                    sint16 rec_idx = boat_carousel_order[idx];
                    uint16 sel_rec = boat_carousel_order[selection];
                    UI_RECORD *rec = recs_snapshot + rec_idx;
                    rec->val = sel_rec;
                    selection = (sint16)(selection + 1);
                    if (selection == 8)
                        selection = 0;
                }
            }
            menu_select_update_mode_select_vis();
        }
        idx0 = boat_carousel_order[0];
        recs = spr_recs;
        head = recs + idx0;
        state = head->transition_ticks;
        if (state == 0)
        {
            sint16 rec_idx = boat_carousel_order[2];
            sint32 result;
            menu_boat_select.phase = MENU_BOAT_SETTLING;
            result = menu_move_spr_rec(recs + rec_idx, 1, 8, 6, 1, 1);
            return;
        }
        return;
    }
    if (dir == 4)
    {
        sint16 rec_idx = boat_carousel_order[2];
        UI_RECORD *recs = spr_recs;
        UI_RECORD *rec = recs + rec_idx;
        uint8 state = rec->command;
        if (state == 0)
        {
            menu_boat_select.phase = MENU_BOAT_IDLE;
            rec->command = 2;
        }
        return;
    }
}

sint32 menu_select_update_boat(void)
{
    sint16 track = menu_state.scr;
    UI_RECORD *recs = spr_recs;
    MENU_DESC *table = menu_cfg[track].descs;
    PLAYER_PROFILE *profile;
    sint32 selection;
    sint32 idx;
    sint32 result = 1;

    FUNCTION_MARKER(0x800549EC, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
    {
        sint16 rec_idx = boat_carousel_order[idx];
        UI_RECORD *rec = recs + rec_idx;
        const MENU_VISIBILITY *list;

        MENU_SELECT_CAROUSEL_SLOT *carousel_slot = boat_carousel + idx;
        carousel_slot->x = rec->x;
        carousel_slot->y = rec->y;
        list = table[idx].visibility;
        carousel_slot->target_x = list->x;
        carousel_slot->target_y = list->y;
    }
    selection = menu_boat_select.boat - 2;
    if (selection < 0)
        selection += 8;
    profile = profile_cur();
    for (idx = 0; idx < 8; ++idx)
    {
        UI_RECORD *rec;
        SPRITE_RENDER *prim;
        sint32 enabled;
        sint16 rec_idx;
        uint16 sel_rec;
        if (selection == 8)
            selection = 0;
        rec_idx = boat_carousel_order[idx];
        sel_rec = boat_carousel_order[selection];
        recs = spr_recs;
        rec = recs + rec_idx;
        rec->val = sel_rec;
        rec->type = 1;
        prim = spr_render_at(rec->data, rec->val);
        enabled = profile->availability[selection] != 0;
        setRGB0(&prim->quad, enabled != 0 ? 128u : 32u, enabled != 0 ? 128u : 32u, enabled != 0 ? 128u : 32u);
        upgrades_enable_prim(&prim->quad, enabled);
        ++selection;
    }
    menu_select_update_mode_select_vis();
    if ((uint8)game_selection.players == 1)
    {
        for (idx = 4; idx > 0; --idx)
            text_menu[5].visible = 0;
    }
    else
    {
        TEXT_RECORD *menu = text_menu;
        uint16 active = menu_boat_select.player;
        char *output = menu[5].text;
        name_copy_player_name_bytes(active, output);
    }
    if (game_selection.mode == 1 || (uint8)game_selection.players >= 2u)
    {
        TEXT_RECORD *menu = text_menu;
        UI_RECORD *recs_now;

        menu[3].visible = 0;
        menu = text_menu;
        recs_now = spr_recs;
        menu[4].x = menu[3].x;
        recs_now[41].type = 0;
        recs_now = spr_recs;
        result = recs_now[41].x;
        recs_now[40].x = result;
    }
    return result;
}

static sint32 menu_boat_assign_pals(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    MENU_DESC *table = menu_cfg[10].descs;
    PLAYER_PROFILE *profile_data = profile_cur();
    UI_RECORD *recs = spr_recs;
    sint32 grp = 0;
    sint32 idx;

    FUNCTION_MARKER(0x80054CB4, "MAIN.EXE");
    for (idx = 0; idx < 6; ++idx)
    {
        sint16 scr = menu_course_select.level;
        sint32 type = profile_data->progress.courses[scr][idx].state;
        sint16 selection;
        sint16 pal = 0;
        SPRITE_RENDER *output;
        MENU_DESC *entry;
        uint16 clut;

        if (game_selection.mode == 1)
        {
            selection = select_course_slots[idx].rec;
            sint16 split = menu_course_select.course;
            if (idx < split)
                type = 6;
            else
                type = idx == split ? 4 : 5;
        }
        else
        {
            selection = select_course_slots[idx].rec;
        }
        scr = menu_course_select.level;
        if (scr == 1)
        {
            grp = 23;
            if (type == 5)
                pal = selection != 0 ? 504 : 506;
            else if (type == 6)
                pal = selection != 0 ? 235 : 234;
            else
                pal = selection != 0 ? 507 : 505;
        }
        else if (scr == 0 || scr == 2)
        {
            if (type == 5)
                pal = selection != 0 ? 508 : 510;
            else if (type == 6)
                pal = selection != 0 ? 229 : 228;
            else
                pal = selection != 0 ? 511 : 509;
        }
        else if (scr == 3)
        {
            pal = idx + 229;
            grp = (uint16)(idx - 1) < 4u ? 36 : 0;
        }
        output = spr_render_at(recs->data, grp + idx);
        clut = getClut(768, pal);
        scr = menu_course_select.level;
        output->quad.p[0].attr = clut;
        output->orientation = scr == 2;
        entry = table + selection;
        scr = menu_course_select.level;
        entry->rec = grp + idx;
        if (scr == 3)
            entry->visible = grp != 0;
    }
    return idx << 16;
}

static sint32 menu_select_sort_recs_desc(uint32 unused1, uint32 unused2)
{
    sint16 split = menu_course_select.course;
    sint16 track = menu_state.scr;
    sint32 output_idx = 0;
    sint32 secondary = 1;
    MENU_DESC *table = menu_cfg[track].descs;

    FUNCTION_MARKER(0x800558EC, "MAIN.EXE");
    if (split >= 0)
    {
        UI_RECORD *recs = spr_recs;
        sint16 val = split;

        do
        {
            MENU_SELECT_COURSE_SLOT *slot = &select_course_slots[val];
            UI_RECORD *rec = recs + (sint16)output_idx;
            uint16 x = slot->x;
            uint16 y;
            MENU_DESC *entry;

            rec->x = x;
            y = slot->y;
            entry = table + (sint16)output_idx;
            rec->val = val;
            rec->y = y;
            entry->rec = val;
            val--;
            slot->rec = output_idx;
            slot->rank = output_idx;
            output_idx = (sint16)(output_idx + 1);
        } while (val >= 0);
    }
    split = menu_course_select.course;
    if (split < 5)
    {
        sint16 val = split + 1;

        if (val < 6)
        {
            UI_RECORD *recs = spr_recs;

            do
            {
                MENU_SELECT_COURSE_SLOT *slot = &select_course_slots[val];
                UI_RECORD *rec = recs + (sint16)output_idx;
                uint16 x = slot->x;
                uint16 y;
                MENU_DESC *entry;

                rec->x = x;
                y = slot->y;
                entry = table + (sint16)output_idx;
                rec->val = val;
                rec->y = y;
                entry->rec = val;
                val++;
                slot->rec = output_idx;
                output_idx = (sint16)(output_idx + 1);
                slot->rank = secondary;
                secondary = (sint16)(secondary + 1);
            } while (val < 6);
        }
    }
    return menu_select_populate_recs(0, 0, 0, 0);
}

static sint32 menu_select_sort_recs_asc(uint32 unused1, uint32 unused2, uint32 unused3)
{
    sint32 split = menu_course_select.course;
    sint16 track = menu_state.scr;
    MENU_DESC *table = menu_cfg[track].descs;
    sint32 val = split;
    sint32 output_idx = 0;
    sint32 secondary = 1;

    FUNCTION_MARKER(0x80055AA8, "MAIN.EXE");
    while (val < 6)
    {
        MENU_SELECT_COURSE_SLOT *slot = &select_course_slots[(sint16)val];
        UI_RECORD *rec = spr_recs + (sint16)output_idx;
        MENU_DESC *entry = table + output_idx;

        rec->x = slot->x;
        rec->val = val;
        rec->layer = 0;
        rec->y = slot->y;
        entry->rec = val;
        ++val;
        slot->rec = output_idx;
        slot->rank = output_idx;
        ++output_idx;
    }
    split = menu_course_select.course;
    if (split > 0)
    {
        val = (sint16)(split - 1);
        while ((sint16)val >= 0)
        {
            MENU_SELECT_COURSE_SLOT *slot = &select_course_slots[(sint16)val];
            UI_RECORD *rec = spr_recs + (sint16)output_idx;
            MENU_DESC *entry = table + output_idx;

            rec->x = slot->x;
            rec->val = val;
            rec->layer = 0;
            rec->y = slot->y;
            entry->rec = val;
            --val;
            slot->rec = output_idx;
            ++output_idx;
            slot->rank = secondary;
            ++secondary;
        }
    }
    return menu_select_populate_recs(0, 0, 0, 0);
}

static uint8 menu_select_grid_cell_is_available(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x8005600C, "MAIN.EXE");
    if (game_options.unlock_courses != 0)
        return 1;
    if (level == 3)
        return name_check_reels();
    if (profile_data->max_level < level)
        return 0;
    return profile_data->progress.courses[level][(uint8)column].state != 5;
}

static void menu_select_update_select_prims(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    UI_RECORD *recs;
    SPRITE_RENDER *row;
    uint32 availability;
    sint32 idx;

    FUNCTION_MARKER(0x80056178, "MAIN.EXE");
    for (idx = 0; idx < 6; ++idx)
    {
        sint16 mapped = select_course_ui[idx].label;
        TEXT_RECORD *desc = &text_menu[mapped];
        uint8 visible = 0;

        if (menu_course_select.course == idx && menu_course_select.level < 3)
            visible = 1;
        desc[0].visible = visible;
    }
    recs = spr_recs;
    {
        uint8 level = menu_course_select.level;
        sint16 prim_idx = recs[12].val;
        uint8 column = menu_course_select.course;

        row = spr_render_at(recs[12].data, prim_idx);
        availability = (uint8)menu_select_grid_cell(level, column, 0, 1);
    }
    if (availability == 1)
    {
        setRGB0(&row->quad, 0x80, 0x80, 0x80);
        row->quad.rgbc.code |= 1;
        recs[12].type = 1;
    }
    else if (availability == 2)
    {
        setRGB0(&row->quad, 0x80, 0x40, 0x40);
        row->quad.rgbc.code &= 0xFE;
        recs[12].type = 1;
    }
    else if (availability == 0)
    {
        setRGB0(&row->quad, 0x40, 0x40, 0x80);
        row->quad.rgbc.code &= 0xFE;
        recs[12].type = 0;
    }
    for (idx = 0; idx < 10; ++idx)
    {
        text_menu[idx + 11].visible = 0;
    }

    {
        sint16 level = menu_course_select.level;
        if (level < 3)
        {
            sint16 column = menu_course_select.course;
            sint16 mapped = select_course_text[level][column];
            TEXT_RECORD *desc = &text_menu[mapped];

            desc[11].visible = 1;
            return;
        }
        return;
    }
}

sint32 menu_select_advance_boat(sint16 action, PAD *input)
{
    sint32 apply_offset = 0;
    sint16 base_offset;
    sint32 idx;

    FUNCTION_MARKER(0x80054FC0, "MAIN.EXE");
    if (menu_course_select.blocked != 0)
    {
        sint16 rec_idx = menu_course_select.rec;
        UI_RECORD *rec = spr_recs + rec_idx;
        if (rec->command == 0)
            menu_move_spr_rec(rec, 1, 8, 6, 1, 1);
    }
    if (action == 0)
    {
        sint32 result = input->cur & 0x40u;
        sint32 enabled = 0;

        if (result != 0)
        {
            result = menu_state.input_enabled;
            if (result != 0)
            {
                result = menu_state.exit_wait == 0;
                enabled = result;
            }
        }
        if (enabled == 0)
        {
            return result;
        }

        {
            uint8 row = menu_course_select.level;
            uint8 column = menu_course_select.course;

            if (menu_select_grid_cell_is_available(row, column, 0, 0) != 0)
            {
                if (game_selection.mode == 6)
                {
                    sint16 mode = game_selection.rules;

                    menu_state.sound = 1;
                    if (mode == 1)
                    {
                        menu_state.phase = 30;
                        return tournament_reset_grid();
                    }
                    if (mode < 2)
                    {
                        if (mode == 0)
                            menu_state.phase = 38;
                        return 38;
                    }
                    if (mode == 2)
                    {
                        menu_state.phase = 31;
                        return tournament_init_round();
                    }
                    return 31;
                }
                voice_start_scaled((sint16)sound_banks.main, 1);
                SpuSetKey(SPU_OFF, 128u);
                return menu_select_commit();
            }
        }
        menu_state.sound = 5;
        return 5;
    }
    base_offset = spr_recs->transition_step_x >> 16;
    switch ((sint16)(action - 1))
    {
        case 0:
        case 1:
        {
            uint16 old_column;
            sint16 column;
            sint16 rec_idx;
            UI_RECORD *rec;
            sint16 target_x;
            sint16 target_y;

            if (action == 1)
            {
                menu_select_sort_recs_desc(0, 0);
                old_column = menu_course_select.course;
                menu_course_select.course = old_column + 1u;
                column = old_column + 1u;
                if (column >= 6)
                {
                    column = 5;
                    menu_course_select.course = 5;
                }
            }
            else
            {
                menu_select_sort_recs_asc(0, 0, 0);
                old_column = menu_course_select.course;
                menu_course_select.course = old_column - 1u;
                column = old_column - 1u;
                if (column < 0)
                {
                    column = 0;
                    menu_course_select.course = 0;
                }
            }
            column = menu_course_select.course;
            MENU_SELECT_COURSE_SLOT *course_slot = select_course_slots + column;
            rec_idx = course_slot->rec;
            rec = spr_recs + rec_idx;
            rec->x = course_slot->x;
            rec->y = course_slot->y;
            target_y = course_slot->y;
            target_x = select_course_slots[old_column].x + (action == 1 ? 134 : -134);
            spr_init_trans(rec, target_x, target_y, 0, 0, 6);
            menu_course_select.dir = action;
            menu_course_select.phase = MENU_COURSE_MOVING;
            break;
        }
        case 2:
        {
            sint16 column = menu_course_select.course;
            sint16 rec_idx = select_course_slots[column].rec;
            UI_RECORD *rec = spr_recs + rec_idx;
            if (rec->transition_ticks == 0)
            {
                sint16 x = rec->x;
                sint16 y = rec->y;
                UI_RECORD *recs;
                uint8 visible;
                sint16 target_x;
                sint16 target_y;
                uint8 row;
                uint8 sel_column;

                if (menu_course_select.dir == 1)
                    menu_select_sort_recs_desc(0, 0);
                else
                    menu_select_sort_recs_asc(0, 0, 0);
                column = menu_course_select.course;
                recs = spr_recs;
                recs->x = x;
                recs->y = y;
                target_x = select_course_slots[column].x;
                target_y = select_course_slots[column].y;
                spr_init_trans(recs, target_x, target_y, 0, 0, 6);
                row = menu_course_select.level;
                sel_column = menu_course_select.course;
                visible = menu_select_grid_cell_is_available(row, sel_column, 0, 0);
                spr_recs[8].type = visible;
                text_menu[3].visible = visible;
                menu_select_update_select_prims(0, 0, 0, 0);
                menu_boat_assign_pals(0, 0, 0, 0);
                menu_course_select.phase = MENU_COURSE_SETTLING;
            }
            break;
        }
        case 3:
            if (spr_recs->transition_ticks == 0)
                menu_course_select.phase = MENU_COURSE_IDLE;
            break;
        case 4:
            if (spr_recs->transition_ticks != 0)
            {
                apply_offset = 1;
                break;
            }
            menu_select_init(1, 0, 0, 0);
            menu_select_start_dir_trans(1);
            {
                TEXT_RECORD *menu = text_menu;
                UI_RECORD *recs;
                uint16 val;
                uint16 last_val;

                menu_course_select.phase = MENU_COURSE_NEXT_ENTER;
                for (idx = 0; idx < 19; ++idx)
                {
                    sint16 menu_idx = select_carousel_text[idx];
                    TEXT_RECORD *address = &menu[menu_idx];
                    address[0].x = address[0].x + 1029u;
                }
                recs = spr_recs;
                val = recs[18].x;
                last_val = recs[20].x;
                recs[18].x = val + 1029u;
                val = recs[19].x;
                recs[20].x = last_val + 1029u;
                recs[19].x = val + 1029u;
            }
            break;
        case 5:
        case 7:
            if (spr_recs->transition_ticks == 0)
                menu_course_select.phase = MENU_COURSE_IDLE;
            else
                apply_offset = 1;
            break;
        case 6:
            if (spr_recs->transition_ticks != 0)
            {
                apply_offset = 1;
                break;
            }
            menu_select_init(3, 0, 0, 0);
            menu_select_start_dir_trans(3);
            {
                TEXT_RECORD *menu = text_menu;
                UI_RECORD *recs;
                uint16 val;
                uint16 last_val;

                menu_course_select.phase = MENU_COURSE_PREV_ENTER;
                for (idx = 0; idx < 19; ++idx)
                {
                    sint16 menu_idx = select_carousel_text[idx];
                    TEXT_RECORD *address = &menu[menu_idx];
                    address[0].x = address[0].x - 980u;
                }
                recs = spr_recs;
                val = recs[18].x;
                last_val = recs[20].x;
                recs[18].x = val - 980u;
                val = recs[19].x;
                recs[20].x = last_val - 980u;
                recs[19].x = val - 980u;
            }
            break;
        default:
            break;
    }
    if (apply_offset != 0)
    {
        {
            TEXT_RECORD *menu = text_menu;
            UI_RECORD *recs;
            uint16 val;
            uint16 last_val;

            for (idx = 0; idx < 19; ++idx)
            {
                sint16 menu_idx = select_carousel_text[idx];
                TEXT_RECORD *address = &menu[menu_idx];
                address[0].x = address[0].x + base_offset;
            }
            recs = spr_recs;
            val = recs[18].x;
            last_val = recs[20].x;
            recs[18].x = val + base_offset;
            val = recs[19].x;
            recs[20].x = last_val + base_offset;
            recs[19].x = val + base_offset;
        }
    }
    return menu_boat_assign_pals(0, 0, 0, 0);
}

sint32 menu_select_populate_recs(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 idx;

    FUNCTION_MARKER(0x800557AC, "MAIN.EXE");
    for (idx = 0; idx < 6; ++idx)
    {
        sint16 offset = 2 * select_course_slots[idx].rank;
        sint16 src_idx = select_course_slots[idx].rec;
        UI_RECORD *src = spr_recs + src_idx;
        UI_RECORD *output;
        uint16 coord;
        sint16 scr;
        uint8 visible = 0;

        src->layer = offset + 1;
        output = spr_recs + idx + 49;
        output->x = src->x + 100u;
        coord = src->y;
        output->layer = offset;
        scr = menu_course_select.level;
        output->y = coord + 40u;
        if (profile_data->progress.courses[scr][idx].state == 6 && scr != 3)
        {
            if (game_selection.mode != 1 || idx < menu_course_select.course)
                visible = 1;
        }
        output->type = visible;
    }
    return idx << 16;
}

static sint32 menu_select_slide_course_rec(sint16 dir, sint16 idx, UI_RECORD *state)
{
    sint32 cur = state->x;
    sint32 target = 0;

    FUNCTION_MARKER(0x80055CFC, "MAIN.EXE");
    if (dir == 0)
        target = cur - 512;
    else if (dir == 1)
    {
        if (idx < 6)
        {
            target = cur;
            state->x = cur + 512;
        }
        else
        {
            target = cur + 512;
            state->x = cur + 1024;
        }
    }
    else if (dir == 2)
        target = cur + 512;
    else if (dir == 3)
    {
        if (idx < 6)
        {
            target = cur;
            state->x = cur - 512;
        }
        else
        {
            target = cur - 512;
            state->x = cur - 1024;
        }
    }
    return spr_init_trans(state, (sint16)target, state->y, 0, 0, 25);
}

sint32 menu_select_start_dir_trans(sint16 dir)
{
    UI_RECORD *recs = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x80055DE4, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
        menu_select_slide_course_rec(dir, idx, recs + idx);
    for (idx = 4; idx < 10; ++idx)
        menu_select_slide_course_rec(dir, 6, recs + idx + 37);
    return 0;
}

sint32 menu_select_init(sint32 initialize, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 track = menu_state.scr;
    MENU_CONFIGURATION *cfg = &menu_cfg[track];
    PLAYER_PROFILE *profile_data;
    sint32 initial_level;
    sint32 idx;

    FUNCTION_MARKER(0x80056368, "MAIN.EXE");
    profile_data = profile_cur();
    menu_course_select.course = profile_data->course;
    initial_level = profile_data->level;
    menu_course_select.level = initial_level;
    if (initial_level == 3)
    {
        if (game_selection.mode == 2)
        {
            menu_course_select.level = 0;
            menu_course_select.course = 0;
            profile_data->level = 0;
            profile_data->course = 0;
        }
        if (menu_course_select.level == initial_level && menu_course_select.course == 0)
            menu_course_select.course = 1;
    }
    if (game_selection.mode == 6 && game_selection.rules == 0)
        menu_course_select.level = game_options.level;
    for (idx = 0; idx < 6; ++idx)
    {
        const MENU_SELECT_COURSE_UI *course_ui = select_course_ui + idx;
        sint16 rec_idx = course_ui->rec;
        sint16 ordering;
        UI_RECORD *recs;

        spr_recs[rec_idx].type = 0;
        ordering = course_ui->order;
        recs = spr_recs;
        recs->val = ordering;
        if ((sint16)initialize == 0)
        {
            UI_RECORD *rec = recs + ordering;

            rec->x = course_ui->x;
            select_course_slots[idx].x = course_ui->x;
            rec->y = course_ui->y;
            select_course_slots[idx].y = course_ui->y;
        }
    }
    menu_select_sort_recs_asc(0, 0, 0);
    {
        UI_RECORD *recs = spr_recs;
        sint16 column = menu_course_select.course;

        cfg->selection = column;
        menu_state.selection = column;
        spr_set_orient(recs + 7, 1);
    }
    spr_recs[7].layer = 12;
    spr_recs[6].layer = 12;
    {
        uint8 visible = 0;
        if (menu_state.series_mode == 0)
            visible = menu_course_select.level != 0;
        spr_recs[7].type = visible;
    }
    {
        uint8 level = menu_course_select.level;
        uint8 visible = menu_select_level_available((uint8)(level + 1u), 0, 0, 0) != 0;
        spr_recs[6].type = visible;
    }
    {
        uint8 level = menu_course_select.level;
        uint8 column = menu_course_select.course;
        uint8 visible = menu_select_grid_cell_is_available(level, column, 0, 0);
        spr_recs[8].type = visible;
        text_menu[3].visible = visible;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        sint16 level = menu_course_select.level;
        sint16 rec_idx = select_level_recs[idx];
        spr_recs[rec_idx].type = level == idx;
    }
    spr_recs[13].type = 0;
    spr_recs[15].type = 0;
    spr_recs[14].type = 0;
    spr_recs[16].type = 0;
    spr_recs[22].type = 0;
    spr_recs[21].type = 0;
    spr_recs[34].type = 0;
    spr_recs[33].type = 0;
    spr_recs[36].type = 0;
    spr_recs[35].type = 0;
    menu_select_update_select_prims(0, 0, 0, 0);
    menu_boat_assign_pals(0, 0, 0, 0);
    spr_recs[17].type = 0;
    if (menu_course_select.level == 3)
    {
        TEXT_RECORD *first_menu;
        uint8 selector;
        uint16 coord;

        spr_recs[12].type = 0;
        first_menu = text_menu;
        menu_course_select.rec = 17;
        menu_course_select.blocked = 0;
        first_menu[10].visible = 0;
        text_menu[28].visible = 1;
        text_menu[29].visible = 1;
        text_menu[30].visible = 1;
        text_menu[31].visible = 1;
        spr_recs[47].type = 1;
        spr_recs[48].type = 1;
        text_menu[2].visible = 0;
        spr_recs[10].type = 0;
        selector = menu_state.language;
        {
            TEXT_RECORD *menu = text_menu;
            coord = select_special_label_x[selector];
            {
                UI_RECORD *recs = spr_recs;
                menu[3].x = coord + 15u;
                recs[8].x = coord;
            }
        }
        for (idx = 0; idx < 4; ++idx)
            spr_recs[idx + 37].type = 0;
        for (idx = 4; idx < 10; ++idx)
            spr_recs[idx + 37].type = 1;
        for (idx = 0; idx < 6; ++idx)
            spr_recs[idx + 49].type = 0;
        name_reset_reels();
    }
    else
    {
        MENU_DESC *descs = cfg->descs;
        TEXT_RECORD *state_menu;

        for (idx = 0; idx < 6; ++idx)
            descs[idx].visible = 1;
        text_menu[10].visible = 1;
        state_menu = text_menu;
        menu_course_select.rec = 12;
        menu_course_select.blocked = 0;
        state_menu[28].visible = 0;
        text_menu[29].visible = 0;
        text_menu[30].visible = 0;
        text_menu[31].visible = 0;
        spr_recs[47].type = 0;
        spr_recs[48].type = 0;
        text_menu[2].visible = 1;
        spr_recs[10].type = 1;
        text_menu[3].x = 227;
        spr_recs[8].x = 212;
        for (idx = 0; idx < 10; ++idx)
            spr_recs[idx + 37].type = 0;
        for (idx = 0; idx < 6; ++idx)
        {
            sint16 level = menu_course_select.level;
            UI_RECORD *recs = spr_recs;
            uint8 sel = profile_data->progress.courses[level][idx].state;

            recs[idx + 49].layer = 4;
            spr_recs[idx + 49].type = sel == 6;
        }
    }
    for (idx = 0; idx < 6; ++idx)
        text_menu[idx + 22].visible = 0;
    spr_recs[55].type = 0;
    text_menu[32].visible = 0;
    if (menu_state.series_mode != 0)
    {
        sint16 column;

        text_menu[0].visible = 0;
        text_menu[21].visible = 1;
        column = menu_course_select.course;
        text_menu[column + 22].visible = 1;
        if (menu_course_select.course != 0 && game_selection.mode != 6)
        {
            spr_recs[55].type = 1;
            text_menu[32].visible = 1;
        }
    }
    else
    {
        text_menu[0].visible = 1;
        text_menu[21].visible = 0;
    }
    return menu_select_populate_recs(1, 0, 0, 0);
}

void menu_select_course(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();

    FUNCTION_MARKER(0x80056C04, "MAIN.EXE");
    profile_data->course = menu_course_select.course;
}

uint32 menu_select_reset_rec_styles(void)
{
    FUNCTION_MARKER(0x80056C68, "MAIN.EXE");
    text_set_hud_visible(1, 0);
    return text_set_hud_visible(2, 0);
}

sint32 menu_select_leave_course_select(void)
{
    FUNCTION_MARKER(0x80056C98, "MAIN.EXE");
    return 10;
}

sint32 menu_select_input(PAD *input, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint32 result = menu_state.input_enabled;

    FUNCTION_MARKER(0x80056CA0, "MAIN.EXE");
    if (result != 0)
    {
        result = (sint16)menu_course_select.phase;
        if (result == 0)
        {
            if ((input->cur & 0x20u) != 0 && menu_course_select.level != 3)
            {
                menu_state.sound = 1;
                menu_state.phase = 21;
            }
            result = 1;
            if ((input->cur & 0x80u) != 0 && game_selection.mode == 1)
            {
                result = 1;
                if (menu_course_select.course != 0)
                {
                    menu_state.sound = 1;
                    result = 20;
                    menu_state.phase = 20;
                }
            }
        }
    }
    return result;
}

void menu_select_update_course_info(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    PROFILE_RECORD *src;
    PROFILE_SUMMARY *summary;
    uint32 enabled;
    sint32 idx;

    FUNCTION_MARKER(0x80056D6C, "MAIN.EXE");
    profile_data = profile_cur();
    for (idx = 0; idx < 12; ++idx)
        spr_recs[idx + 2].type = 0;
    for (idx = 0; idx < 6; ++idx)
    {
        uint8 active = idx == menu_course_select.course;
        uint32 offset = 32u * idx;

        text_menu[idx].visible = active;
        text_menu[idx + 1].visible = active;
    }
    {
        sint16 level = menu_course_select.level;

        if (level == 1)
        {
            sint16 column = menu_course_select.course;
            spr_recs[column + 8].type = 1;
        }
        else if (level == 2)
        {
            sint16 column = menu_course_select.course;
            UI_RECORD *owner = spr_recs + column + 2;

            owner->type = 1;
            spr_set_orient(owner, 1);
        }
        else if (level == 0)
        {
            sint16 column = menu_course_select.course;
            spr_recs[column + 2].type = 1;
        }
    }

    if (game_selection.mode == 4)
    {
        selection = (sint16)(menu_course_select.course + 6u * menu_course_select.level);
    }
    else
    {
        uint32 profile_level = profile_data->level;
        selection = (sint16)(menu_course_select.course + 6 * profile_level);
    }
    spr_recs[16].type = 0;
    spr_recs[17].type = 0;
    spr_recs[18].type = 0;
    spr_recs[19].type = 0;
    if (menu_state.exit_wait != 0 && game_selection.mode != 4)
    {
        if (selection == 0)
        {
            spr_recs[16].type = 1;
            spr_recs[19].type = 1;
        }
        else if (selection == 1)
            spr_recs[18].type = 1;
        else if (selection == 2 && game_selection.selection == 0 && (uint8)game_selection.players == 1)
            spr_recs[17].type = 1;
    }

    summary = &profile_summaries[(sint16)selection];
    enabled = summary->lap_time[0] != 0 || summary->lap_time[1] != 0 || summary->lap_time[2] != 0;
    for (idx = 0; idx < 16; ++idx)
    {
        uint8 sel = idx == summary->rank ? enabled : 0u;
        TEXT_RECORD *menu = text_menu;

        menu[idx + 24].visible = sel;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        uint8 sel = idx == summary->profile ? enabled : 0u;
        TEXT_RECORD *menu = text_menu;

        menu[idx + 40].visible = sel;
    }
    for (idx = 0; idx < 4; ++idx)
        text_menu[idx + 13].visible = enabled;
    for (idx = 0; idx < 2; ++idx)
    {
        uint32 offset = 16u * idx;

        text_menu[idx + 43].visible = 1 - enabled;
        text_menu[idx + 20].visible = enabled;
        text_menu[idx + 22].visible = enabled;
    }
    for (idx = 0; idx < 2; ++idx)
    {
        uint32 offset = 16u * idx;

        text_menu[idx + 18].visible = 1;
        text_menu[idx + 45].visible = 0;
        text_menu[idx + 22].visible = 1;
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *dst = menu[20].text;
        profile_format_parts(summary->lap_time, dst);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *dst = menu[21].text;
        profile_format_parts(summary->race_time, dst);
    }
    src = &profile_recs[(sint16)selection];
    {
        TEXT_RECORD *menu = text_menu;
        char *dst = menu[22].text;
        profile_format_parts(src->lap.time, dst);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *dst = menu[23].text;
        profile_format_parts(src->times[0], dst);
    }
    if (menu_state.exit_wait != 0)
    {
        TEXT_RECORD *menu;

        spr_recs[14].type = 0;
        text_menu[47].visible = 0;
        spr_recs[15].type = 0;
        menu = text_menu;
        menu[48].visible = 0;
        return;
    }
}

void menu_select_restore_backup(void)
{
    uint32 sel = profile_selection.slot;
    MENU_SELECT_SELECTION_BACKUP *backup = &select_selection_backups[sel];
    PLAYER_PROFILE *profile_data = profile_at(sel);
    sint32 grp;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8005A0C4, "MAIN.EXE");
    if (backup->active == 0)
        return;
    profile_data->course = backup->course;
    menu_course_select.course = backup->course;
    menu_state.saved_choice = profile_data->level;
    profile_data->level = backup->level;
    race_selection.boats[0] = backup->grid_row;
    menu_boat_select.boat = backup->grid_row;
    for (grp = 0; grp < 3; ++grp)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                PROFILE_COURSE *dst = &profile_at(grp)->progress.courses[row][column];
                const PROFILE_COURSE *src = &select_selection_backups[grp].progress.courses[row][column];
                dst->attempts = src->attempts;
                dst->result = src->result;
                dst->state = src->state;
            }
        }
    }
}

static sint32 menu_select_reset_recs(void)
{
    sint32 profile_idx;
    sint32 result = 0;

    FUNCTION_MARKER(0x8005A258, "MAIN.EXE");
    for (profile_idx = 0; profile_idx < 3; ++profile_idx)
    {
        PLAYER_PROFILE *profile_data = profile_at(profile_idx);
        if (profile_at(profile_idx)->reset_pending != 0)
        {
            sint32 grp;
            sint32 idx;

            profile_at(profile_idx)->reset_pending = 0;
            profile_copy_templates(profile_idx);
            profile_reset_series(profile_idx);
            select_selection_backups[profile_idx].active = 0;
            profile_data->course = 0;
            profile_data->level = 0;
            profile_data->max_level = 0;
            profile_data->reward_flags = 0;
            for (grp = 0; grp < 3; ++grp)
            {
                for (idx = 0; idx < 3; ++idx)
                    profile_data->progress.courses[grp][2u - idx].state = 4;
                for (idx = 0; idx < 3; ++idx)
                    profile_data->progress.courses[grp][3u + idx].state = 5;
            }
            for (grp = 0; grp < 3; ++grp)
            {
                for (idx = 0; idx < 6; ++idx)
                {
                    profile_data->progress.courses[grp][idx].result = 0;
                    profile_data->progress.courses[grp][idx].attempts = 0;
                }
            }
            for (idx = 9; idx >= 0; --idx)
                profile_at(profile_idx)->availability[idx] = 0;
            profile_at(profile_idx)->availability[0] = 1;
            profile_at(profile_idx)->availability[1] = 1;
            profile_at(profile_idx)->availability[2] = 1;
            save_flush();
        }
        result = profile_idx + 1 < 3;
    }
    return result;
}

sint32 menu_select_advance(void)
{
    PAD no_buttons = {0};

    FUNCTION_MARKER(0x8005A3EC, "MAIN.EXE");
    game_selection.players = game_selection.players & 0xFF00u | 1;
    if (menu_state.from_scr == 1)
    {
        game_selection.mode = 0;
        game_selection.prev_mode = 0;
    }
    else if (game_selection.mode == 1)
    {
        menu_update_grp_desc_select(3, 0, menu_state.saved_choice + 2u);
    }
    // Programmatic entry has no button input
    menu_select_update_trans(&no_buttons);
    return menu_select_reset_recs();
}

sint32 menu_select_update_trans(PAD *input)
{
    sint16 count;
    sint16 sel;
    sint16 next;
    sint16 rec_idx = 0;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x8005A480, "MAIN.EXE");
    if (menu_state.input_enabled != 0 && (input->cur & 0x40u) != 0)
    {
        menu_state.sound = 1;
        menu_state.phase = 25;
        spr_clear_anim_recs();
        menu_select_apply_trans();
    }
    count = menu_query_grp_val(3, 0);
    sel = menu_query_grp_val(3, 1);
    next = profile_at(sel)->max_level + 1u;
    profile_selection.slot = sel;
    menu_set_grp_desc_val(3, 0, next);
    if (next < count)
    {
        menu_update_grp_desc_select(3, 0, next);
        count = next;
    }
    menu_state.saved_choice = count - 2;
    if (count == 2)
        rec_idx = 10;
    else if (count == 3)
        rec_idx = 11;
    else if (count == 4)
        rec_idx = 12;
    spr_recs->type = 1;
    for (idx = 10; idx < 13; ++idx)
        spr_recs[idx].type = 0;
    if (rec_idx != 0)
    {
        UI_RECORD *rec = spr_recs + rec_idx;
        rec->type = 1;
        if (menu_state.selection != 0)
            spr_apply_prim_vram_mask(rec);
        else
            spr_disable_semitransparency(rec);
    }
    else
    {
        spr_recs[4].type = 0;
        spr_recs[8].type = 0;
        spr_recs[9].type = 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        text_menu[idx + 3].visible = 0;
        result = 1;
    }
    return result;
}

static void menu_select_apply_action(sint16 action)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 result;

    FUNCTION_MARKER(0x800637F0, "MAIN.EXE");
    if (action == 3)
    {
        if (menu_state.course_cleared != 0)
            menu_state.cutscene = 5;
        profile_data->reward_flags |= 0x88;
        action = profile_selection.slot == 0 ? 7 : profile_selection.slot == 1 ? 8 : 9;
    }
    switch (action)
    {
        case 1:
            profile_data->reward_flags |= 2;
            return;
        case 2:
            profile_data->reward_flags |= 4;
            return;
        case 4:
            profile_data->reward_flags |= 0x10;
            menu_state.cutscene = 2;
            menu_state.trophy = 1;
            return;
        case 5:
            profile_data->reward_flags |= 0x20;
            menu_state.cutscene = 3;
            menu_state.trophy = 2;
            return;
        case 6:
            profile_data->reward_flags |= 0x40;
            menu_state.cutscene = 4;
            menu_state.trophy = 3;
            return;
        case 7:
            profile_data->reward_flags |= 1;
            return;
        case 8:
        case 9:
            profile_data->reward_flags |= 1;
            if (profile_unlock(7) != 0)
            {
                menu_state.reward_notice = 1;
                menu_notice.val = 7;
                menu_notice.boat = 7;
                menu_notice.type = 2;
                game_selection.event = 14;
            }
            return;
        case 12:
            result = (profile_data->level_result & 7u) + 6;
            menu_state.cutscene = result;
            return;
        default:
            return;
    }
}

static void menu_select_update_row_completion(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    uint32 row = profile_data->level;
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005A8A0, "MAIN.EXE");
    profile_data->progress.courses[row][column].state = 6;
    menu_notice.type = 3;
    menu_notice.val = column + 1;
    if (row == 2)
        menu_notice.val = column + 7;
    game_selection.event = 19;
    game_selection.event_arg = 0;
    profile_data->course = column + 1;
    series->completed++;
    menu_course_select.course = profile_data->course;
    if (profile_data->course != 6)
        return;
    if (ranking_player_is_leading() == 0)
    {
        menu_select_apply_action(12);
        return;
    }
    profile_data->reward_flags |= 0x80;
    if (row == 0)
    {
        menu_select_apply_action(4);
        return;
    }
    if (row == 1)
    {
        menu_select_apply_action(5);
        return;
    }
    if (row == 2)
    {
        menu_select_apply_action(6);
        return;
    }
}

static sint32 menu_select_row_is_complete(sint16 row)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 column;

    FUNCTION_MARKER(0x8005A9F8, "MAIN.EXE");
    for (column = 0; column < 6; ++column)
    {
        if (profile_data->progress.courses[row][column].state != 6)
            return 0;
    }
    return 1;
}

static void menu_select_confirm_row_select(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    uint32 column = profile_data->course;
    uint32 row = profile_data->level;
    PROFILE_COURSE *row_data;
    PROFILE_COURSE *cell;
    sint32 result;

    FUNCTION_MARKER(0x8005AA88, "MAIN.EXE");
    profile_data->level_result = 0;
    game_selection.event = 11;
    if (row == 3)
    {
        profile_data->progress.courses[3u][column].attempts = 0;
        return;
    }
    row_data = profile_data->progress.courses[row];
    cell = &row_data[column];
    if (cell->state == 4)
    {
        cell->state = 6;
        menu_state.course_cleared = 1;
        if (row_data[3].state == 5)
            row_data[3].state = 4;
        else if (row_data[4].state == 5)
            row_data[4].state = 4;
        else if (row_data[5].state == 5)
            row_data[5].state = 4;
    }
    if (menu_select_row_is_complete(row) != 0)
    {
        if (profile_data->level <= 2u)
            menu_select_apply_action(profile_data->level + 1u);
        if (profile_data->level == profile_data->max_level)
        {
            profile_data->course = 0;
            profile_data->level_result = 1;
            profile_data->level++;
        }
        if (profile_data->max_level < profile_data->level)
            profile_data->max_level = profile_data->level;
    }
    result = 2 * row;
    if (result_state.pickups == 5)
    {
        result_state.pickups = 0;
        cell = &row_data[column];
        result = 2;
        if (cell->result != 2)
        {
            profile_data->course = column;
            profile_data->level = row;
            cell->result = 1;
            result = profile_idx_map();
            row_data[column].attempts = result;
        }
    }
}

static sint32 menu_select_select_series_event(void)
{
    FUNCTION_MARKER(0x8005ACA8, "MAIN.EXE");
    game_selection.event = 17;
    return 17;
}

static sint32 menu_select_refresh_menu_ack(void)
{
    uint32 state;

    FUNCTION_MARKER(0x8005ACBC, "MAIN.EXE");

    state = game_selection.menu_variant;
    game_selection.event = 10;
    if (state == 0)
        game_selection.event_arg = 0;
    else if (state == 1)
        game_selection.event_arg = 1;
    return 1;
}

static void menu_select_dispatch_menu_state(void)
{
    sint32 state;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x8005A714, "MAIN.EXE");
    result = game_selection.ready;
    if (result == 0)
        return;

    state = (sint16)game_selection.mode;
    if (state == 1)
    {
        result_state.grid_count = (uint16)boat_racer_count + boat_leader_count + boat_trailer_count;
        menu_select_update_row_completion();
        return;
    }
    if (state >= 2)
    {
        if (state == 3)
        {
            upgrades_apply_race_result();
            return;
        }
        if (state == 4)
        {
            game_selection.event = 1;
            game_selection.mode = 0;
            game_selection.event_arg = 0;
            game_selection.selection = -1;
            return;
        }
        return;
    }
    if (state != 0)
        return;
    selection = game_selection.selection;
    if (selection == -3)
    {
        game_selection.event = 48;
        game_selection.event_arg = 0;
        return;
    }
    if (selection == -4)
    {
        menu_select_confirm_row_select();
        return;
    }
    if (selection < -2)
        return;
    if (selection == -2)
    {
        menu_select_select_series_event();
        return;
    }
    if (selection == -1)
    {
        if (name_matches(0, name_code_winr) == 0)
        {
            menu_select_refresh_menu_ack();
            return;
        }
        menu_select_confirm_row_select();
        return;
    }
}

void menu_select_refresh_series_prompt(void)
{
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    UI_RECORD *recs = spr_recs;

    FUNCTION_MARKER(0x8005AD20, "MAIN.EXE");
    if (menu_state.series_confirm != 0)
    {
        menu[0].visible = 0;
        menu[29].visible = 0;
        menu[1].visible = 0;
        menu[2].visible = 0;
        recs[48].type = 0;
        recs[49].type = 0;
        recs[50].type = 0;
        recs[51].type = 1;
        recs[52].type = 1;
        menu[30].visible = 1;
        menu[31].visible = 1;
        menu[32].visible = 1;
        return;
    }
    menu[30].visible = 0;
    menu[31].visible = 0;
    menu[32].visible = 0;
    menu[0].visible = 1;
    recs[48].type = 1;
    recs[50].type = 1;
    recs[51].type = 0;
    if (series->phase == 1)
    {
        menu[29].visible = 1;
        menu[1].visible = 0;
        menu[2].visible = 0;
        recs[49].type = 0;
    }
    else
    {
        menu[29].visible = 0;
        menu[1].visible = 1;
        menu[2].visible = 1;
        recs[49].type = 1;
    }
    recs[51].type = 0;
    recs[52].type = 0;
    if (series->phase - 1u >= 2u)
    {
        sint16 pos = menu_state.language == 1 ? 262 : 242;
        recs[50].x = pos;
        return;
    }
    recs[50].x = 136;
}

void menu_select_init_series_menu(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *menu = text_menu;
    char *text;
    sint32 idx;
    sint32 val;

    FUNCTION_MARKER(0x8005AFA0, "MAIN.EXE");
    menu_course_select.course = series->completed;
    race_selection.boats[0] = series->grid_row;
    menu_boat_select.boat = race_selection.boats[0];
    for (idx = 0; idx < 6; ++idx)
    {
        uint8 sel = idx == menu_course_select.course;
        menu[2 * idx + 4].visible = sel;
        menu[2 * idx + 5].visible = sel;
        recs[idx + 39].type = sel;
    }
    for (idx = 0; idx < 9; ++idx)
    {
        uint8 sel = idx == menu_boat_select.boat;
        recs[idx + 30].type = sel;
        menu[idx + 16].visible = sel;
    }
    for (idx = 0; idx < 3; ++idx)
        recs[idx + 45].type = idx == profile_data->level;
    if (series->phase == 2)
    {
        recs[50].x = 136;
        recs[49].type = 0;
    }
    else
    {
        recs[50].x = menu_state.language == 1 ? 262u : 242u;
        recs[49].type = 1;
    }
    profile_build_vehicle_grid(1, menu_boat_select.boat, 388, 94);
    ranking_build_order();
    val = ranking_find_empty_slot() + 1;
    text = menu[26].text;
    if (series->completed != 0)
    {
        text[0] = val / 10 + 48;
        text[1u] = val % 10 + 48;
        text[2u] = 32;
        text[3u] = 32;
    }
    else
    {
        for (idx = 0; idx < 4; ++idx)
            text[idx] = 46;
    }
    text = menu[28].text;
    if (series->completed == 0)
    {
        for (idx = 0; idx < 4; ++idx)
            text[idx] = 46;
    }
    else if (series->points[0] == 0)
    {
        text[0] = 48;
        text[1u] = 48;
        text[2u] = 32;
        text[3u] = 32;
    }
    else
    {
        val = series->points[0];
        text[0] = val / 10 + 48;
        text[1u] = val % 10 + 48;
        text[2u] = 32;
        text[3u] = 32;
    }
    menu_select_refresh_series_prompt();
}

void menu_select_update_series_menu(PAD *input)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    sint32 idx;

    FUNCTION_MARKER(0x8005B350, "MAIN.EXE");
    if (menu_state.series_confirm == 0 && menu_state.input_enabled != 0)
    {
        if ((input->cur & 0x10u) != 0)
        {
            menu_state.sound = 2;
            menu_state.phase = 3;
            menu_select_restore_backup();
        }
        if ((input->cur & 0x40u) != 0)
        {
            if (series->phase == 1)
                series->phase = 2;
            menu_state.sound = 1;
            menu_state.phase = 10;
        }
    }
    if (series->phase - 2u < 2u && menu_state.input_enabled != 0)
    {
        if ((input->cur & 0x80u) != 0)
        {
            if (menu_state.series_confirm == 0 && spr_recs[49].type == 1 && (input->cur & 0x40u) == 0)
            {
                menu_state.sound = 4;
                menu_state.series_confirm = 1;
            }
            else if (menu_state.series_confirm != 0)
            {
                menu_state.sound = 1;
                menu_state.series_confirm = 0;
            }
        }
        if (menu_state.series_confirm != 0 && (input->cur & 0x20u) != 0)
        {
            menu_state.series_confirm = 0;
            series->phase = 1;
            menu_state.phase = 5;
            menu_state.sound = 1;
            series->completed = 0;
            profile_data->course = 0;
            for (idx = 0; idx < 16; ++idx)
                series->points[idx] = 0;
            menu_state.prev_scr = -1;
        }
    }
    menu_select_refresh_series_prompt();
}

sint32 menu_select_prepare_series_race(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    sint32 count;
    sint32 idx;

    FUNCTION_MARKER(0x8005B570, "MAIN.EXE");
    if (series->phase == 2)
    {
        series->completed = 0;
        profile_data->course = 0;
        series->phase = 3;
        for (idx = 0; idx < 16; ++idx)
            series->points[idx] = 0;
    }
    if (menu_state.phase != 10)
        return 10;
    count = (sint16)series->completed;
    for (idx = 0; idx < count; ++idx)
        profile_data->progress.courses[profile_data->level][idx].state = 6;
    for (idx = count; idx < 6; ++idx)
        profile_data->progress.courses[profile_data->level][idx].state = 4;
    return idx;
}

sint32 menu_select_restore(sint16 selection)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8005BE50, "MAIN.EXE");
    menu_clear_notice();
    profile_data = profile_cur();
    profile_get_series();
    menu_tex.sel = 0xFFFF;
    menu_course_select.course = profile_data->course;
    return selection;
}

void menu_select_complete(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    PROFILE_SERIES *series = profile_get_series();
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005D804, "MAIN.EXE");
    profile_data->progress.courses[profile_data->level][column].state = 6;
    menu_notice.type = 3;
    menu_notice.val = column + 1;
    if (profile_data->level == 2)
        menu_notice.val = column + 7;
    game_selection.event = 44;
    profile_data->course = column + 1;
    series->completed++;
    menu_course_select.course = profile_data->course;
    save_flush();
    if (profile_data->course == 6)
    {
        menu_select_apply_action(10);
        return;
    }
}

sint32 menu_select_reset(void)
{
    FUNCTION_MARKER(0x80063A4C, "MAIN.EXE");
    game_selection.mode = 0;
    game_selection.event = 8;
    game_selection.event_arg = 0;
    return 8;
}

sint32 menu_select_dispatch_results(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 mode;

    FUNCTION_MARKER(0x80063A70, "MAIN.EXE");
    race_capture_time_summaries(unused1, unused2, unused3, unused4);
    mode = game_selection.rules;
    if (mode == 0)
    {
        profile_update_best_times(unused1, unused2, unused3, unused4);
        return tournament_rec_champ_result();
    }
    if (mode == 1)
        return tournament_rec_result(unused1, unused2, unused3, unused4);
    if (mode == 2)
        return tournament_rec_match_result();
    return 2;
}

sint32 menu_select_restore_grid(void)
{
    FUNCTION_MARKER(0x80063B00, "MAIN.EXE");
    game_selection.event = 1;
    game_selection.mode = 0;
    game_selection.selection = -1;
    boat_racer_count = (sint16)profile_selection.saved.racers;
    boat_leader_count = profile_selection.saved.leaders;
    boat_trailer_count = profile_selection.saved.trailers;
    return -1;
}

void menu_select_process_results(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();

    FUNCTION_MARKER(0x80063B4C, "MAIN.EXE");
    race_compact_ranks();
    if (profile_data->level < 3u)
    {
        profile_update_best_times(0, 0, 0, 0);
        race_capture_time_summaries(0, 0, 0, 0);
    }
    menu_select_dispatch_menu_state();
    return;
}

sint32 menu_select_commit(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 initial_state;
    sint16 state;
    sint32 gate;

    FUNCTION_MARKER(0x80063D44, "MAIN.EXE");
    menu_save_desc_payloads(menu_state.scr);
    profile_data = profile_cur();
    profile_data->level_result = 0;
    profile_data->course = menu_course_select.course;
    game_selection.event = 10;
    initial_state = game_selection.mode;
    game_selection.event_arg = menu_course_select.course;
    if (initial_state == 4)
    {
        profile_selection.saved.racers = boat_racer_count;
        profile_selection.saved.leaders = boat_leader_count;
        profile_selection.saved.trailers = boat_trailer_count;
    }
    gate = (sint16)menu_course_select.blocked;
    result_state.status = 0;
    if (gate != 0)
    {
        game_selection.prev_mode = initial_state;
        if (menu_course_select.rec == 12)
        {
            sint16 row = menu_course_select.level;
            uint8 *val;

            game_selection.mode = 3;
            val = &profile_data->progress.courses[row][menu_course_select.course].attempts;
            if (*val != 0)
                (*val)--;
        }
    }
    if (menu_course_select.level == 3)
    {
        race_selection.special = 1;
        race_selection.resource = menu_course_select.course - 1u;
        profile_selection.saved.menu_slot = profile_selection.menu_slot;
    }
    menu_state.exit_wait = 5;
    text_set_hud_visible(0, 1);
    state = game_selection.mode;
    if (state != 3 && race_selection.special == 0)
    {
        TEXT_RECORD *rec;

        menu_state.phase = 21;
        menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
        rec = text_hud;
        rec[0].x = 332;
        rec[0].y = 145;
    }
    if (initial_state != 4)
        save_flush();
    if (state != 6 && (uint8)game_selection.players == 2)
    {
        name_copy_player_name_bytes(0, hud_text.names[0]);
        return name_copy_player_name_bytes(1, hud_text.names[1]);
    }
    return 2;
}

void menu_select_update_limits(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint16 maximum = 0;
    sint16 val;
    sint32 idx;

    FUNCTION_MARKER(0x80063F68, "MAIN.EXE");
    menu_set_grp_desc_val(3, 0, profile_data->max_level + 1);
    for (idx = 0; idx < 3; ++idx)
    {
        val = profile_at(idx)->max_level;
        if (val > maximum)
            maximum = val;
    }
    if (maximum >= 3)
        maximum = 2;
    menu_set_grp_desc_val(4, 0, maximum + 1);
    val = profile_at((sint16)profile_selection.rules_slot)->max_level;
    if (val >= 3)
        val = 2;
    menu_set_grp_desc_val(35, 0, val);
}

uint8 menu_select_level_available(sint32 requested, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    uint32 val = (uint8)requested;

    FUNCTION_MARKER(0x80055F0C, "MAIN.EXE");
    if (menu_state.series_mode != 0)
        return 0;
    if (game_selection.mode == 2)
    {
        if (val == 3)
            return 0;
    }
    else if (val == 3 && (uint8)game_selection.players != 1)
    {
        if (name_matches(0, name_code_frac) != 0 && (uint8)game_selection.players == 2)
            return game_selection.mode != 6;
        return 0;
    }
    if (game_options.unlock_courses != 0)
        return 1;
    return profile_data->max_level >= val;
}

sint32 menu_select_grid_cell(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x800560B8, "MAIN.EXE");
    if ((uint32)((sint16)game_selection.mode - 1) < 2 || level == 3)
        return 0;
    if (game_options.result_filter == 0)
        return profile_data->progress.courses[level][(uint8)column].result;
    if (profile_data->progress.courses[level][(uint8)column].result == 2)
        return profile_data->progress.courses[level][(uint8)column].result;
    return 1;
}
