#include "vehicle.h"
#include "input.h"
#include "tournament.h"
#include "display.h"
#include "sound.h"
#include "ranking.h"
#include "name.h"
#include "profile.h"
#include "vehicle_select.h"
#include "arena.h"
#include "global.h"
#include "menu.h"
#include "results.h"
#include "sprite.h"
#include "text.h"
#include "timer.h"
#include "xport_trace.h"
#include <stdlib.h>

static PLAYER_PROFILE player_profiles[5];

typedef struct
{
    uint8 active;
    uint8 course;
    uint8 level;
    uint8 grid_row;
    PROFILE_PROGRESS progress;
} PROFILE_SELECTION_BACKUP;

static PROFILE_SELECTION_BACKUP profile_selection_backups[3];

static uint32 profile_active_slot(void)
{
    uint8 mode = r_u8(0x800E058Eu);

    if (mode == 1u)
        return r_u8(0x800E0595u);
    return mode == 2u ? 3u : 4u;
}

PLAYER_PROFILE *profile_at(uint32 slot)
{
    return &player_profiles[slot];
}

PLAYER_PROFILE *profile_current(void)
{
    return profile_at(profile_active_slot());
}

void profile_reset_series(uint32 slot)
{
    uint32 row;
    uint32 racer;

    for (row = 0u; row < 3u; ++row)
    {
        PROFILE_SERIES *series = &player_profiles[slot].series[row];

        series->completed = 0u;
        series->phase = 0u;
        for (racer = 0u; racer < 16u; ++racer)
            series->points[racer] = 0u;
    }
}

// Decode only the series region of the compatible save format
static PROFILE_SERIES *profile_save_series(uint32 offset, uint32 *field)
{
    uint32 relative;
    uint32 slot;
    uint32 within;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    within = relative % 274u;
    if (slot >= 5u || within < 208u)
        return NULL;
    within -= 208u;
    *field = within % 22u;
    return &player_profiles[slot].series[within / 22u];
}

static uint8 *profile_save_availability(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    field = relative % 274u;
    if (slot >= 5u || field < 78u || field >= 88u)
        return NULL;
    return &player_profiles[slot].availability[field - 78u];
}

// Decode the grid region at the compatible file boundary
static uint8 *profile_save_grid(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;
    PROFILE_GRID *row;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    field = relative % 274u;
    if (slot >= 5u || field < 88u || field >= 208u)
        return NULL;
    field -= 88u;
    row = &player_profiles[slot].grid[field / 12u];
    field %= 12u;
    if (field < 4u)
        return &row->choice[field];
    if (field < 8u)
        return &row->limit[field - 4u];
    return &row->offset[field - 8u];
}

// Locate native header fields in the compatible file format
static uint8 *profile_save_header(uint32 offset)
{
    uint32 relative;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    if (relative / 274u >= 5u)
        return NULL;
    switch (relative % 274u)
    {
        case 0u:
            return &player_profiles[relative / 274u].reset_pending;
        case 1u:
            return &player_profiles[relative / 274u].course;
        case 2u:
            return &player_profiles[relative / 274u].level;
        case 3u:
            return &player_profiles[relative / 274u].max_level;
        case 4u:
            return &player_profiles[relative / 274u].level_result;
        case 5u:
            return &player_profiles[relative / 274u].reward_flags;
        default:
            return NULL;
    }
}

// Map selection backups only at the compatible file boundary
static uint8 *profile_save_backup(uint32 offset)
{
    uint32 relative;
    uint32 field;
    PROFILE_SELECTION_BACKUP *backup;

    if (offset >= 1458u && offset < 1470u)
    {
        relative = offset - 1458u;
        backup = &profile_selection_backups[relative / 4u];
        switch (relative % 4u)
        {
            case 0u:
                return &backup->active;
            case 1u:
                return &backup->course;
            case 2u:
                return &backup->level;
            default:
                return &backup->grid_row;
        }
    }
    if (offset < 5420u || offset >= 5636u)
        return NULL;
    relative = offset - 5420u;
    backup = &profile_selection_backups[relative / 72u];
    field = relative % 24u;
    if (relative % 72u < 24u)
        return &backup->progress.courses[field / 6u][field % 6u].attempts;
    if (relative % 72u < 48u)
        return &backup->progress.courses[field / 6u][field % 6u].result;
    return &backup->progress.courses[field / 6u][field % 6u].state;
}

// Map profile progression only at the compatible file boundary
static uint8 *profile_save_progress(uint32 offset)
{
    uint32 relative;
    uint32 field;
    PROFILE_COURSE *course;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    field = relative % 274u;
    if (relative / 274u >= 5u || field < 6u || field >= 78u)
        return NULL;
    field -= 6u;
    course = &player_profiles[relative / 274u].progress.courses[(field % 24u) / 6u][field % 6u];
    if (field < 24u)
        return &course->attempts;
    if (field < 48u)
        return &course->result;
    return &course->state;
}

// Native save encoder used by mc_save_segments_gather (MAIN.EXE 0x8004B644)
uint8 profile_save_read_byte(uint32 offset)
{
    uint8 *name = name_save_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = profile_save_backup(offset);
    uint8 *header = profile_save_header(offset);
    uint8 *grid = profile_save_grid(offset);
    uint8 *availability = profile_save_availability(offset);
    uint32 field;
    PROFILE_SERIES *series = profile_save_series(offset, &field);
    uint16 value;

    if (offset >= 5664u && offset < 5668u)
        return (uint8)(name_reels.code >> (8u * (offset - 5664u)));
    if (name != NULL)
        return *name;
    if (progress != NULL)
        return *progress;
    if (backup != NULL)
        return *backup;
    if (header != NULL)
        return *header;
    if (grid != NULL)
        return *grid;
    if (availability != NULL)
        return *availability;
    if (series == NULL)
        return r_u8(0x800E0580u + offset);
    if (field >= 6u)
        return series->points[field - 6u];
    value = field < 2u ? series->completed : field < 4u ? series->grid_row : series->phase;
    return (uint8)(value >> (8u * (field & 1u)));
}

// Native save decoder used by mc_save_segments_scatter (MAIN.EXE 0x8004B6BC)
void profile_save_write_byte(uint32 offset, uint8 value)
{
    uint8 *name = name_save_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = profile_save_backup(offset);
    uint8 *header = profile_save_header(offset);
    uint8 *grid = profile_save_grid(offset);
    uint8 *availability = profile_save_availability(offset);
    uint32 field;
    PROFILE_SERIES *series = profile_save_series(offset, &field);
    uint16 *target;
    uint32 shift;

    if (offset >= 5664u && offset < 5668u)
    {
        uint32 shift = 8u * (offset - 5664u);

        name_reels.code = (name_reels.code & ~(0xFFu << shift)) | ((uint32)value << shift);
        return;
    }
    if (name != NULL)
    {
        *name = value;
        return;
    }
    if (progress != NULL)
    {
        *progress = value;
        return;
    }
    if (backup != NULL)
    {
        *backup = value;
        return;
    }
    if (header != NULL)
    {
        *header = value;
        return;
    }
    if (grid != NULL)
    {
        *grid = value;
        return;
    }
    if (availability != NULL)
    {
        *availability = value;
        return;
    }
    if (series == NULL)
    {
        w_u8(0x800E0580u + offset, value);
        return;
    }
    if (field >= 6u)
    {
        series->points[field - 6u] = value;
        return;
    }
    target = field < 2u ? &series->completed : field < 4u ? &series->grid_row : &series->phase;
    shift = 8u * (field & 1u);
    *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
}

sint32 profile_fn_80052f30(void)
{
    FUNCTION_MARKER(0x80052F30u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return menu_fn_80052f58();
}

sint32 profile_fn_800531c0(void)
{
    FUNCTION_MARKER(0x800531C0u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return menu_fn_800531e8();
}

sint32 profile_fn_80053458(void)
{
    FUNCTION_MARKER(0x80053458u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    return profile_fn_8005347c();
}

sint32 profile_fn_8005347c(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x8005347Cu, "MAIN.EXE");
    if ((sint16)controller->type == 7)
    {
        uint32 records = r_u32(0x800B6A74u);
        sint32 delta = 127 - (sint32)controller->packet[6];
        w_u16(records + 260u, (uint16)(252 - delta / 2));
        display_set_line_color(192, 192, 192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        input_calibrations[selected].stick_center = (uint16)((uint16)delta);
        result = 1;
        if ((controller->pressed & 0x800u) == 0u)
            return result;
        w_u16(0x800B6AFAu, 1u);
        result = 46;
    }
    w_u16(0x800B413Eu, (uint16)result);
    return result;
}

sint32 profile_fn_80053588(void)
{
    FUNCTION_MARKER(0x80053588u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return profile_fn_800535b0();
}

sint32 profile_fn_800535b0(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x800535B0u, "MAIN.EXE");
    if ((sint16)controller->type == 7)
    {
        uint32 records;
        uint32 sprite;
        char *text;
        uint32 raw;
        sint32 value;
        sint32 percent;
        sint32 scale;
        sint16 record_index;

        display_set_line_color(192, 192, 192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        raw = controller->packet[6];
        value = (127 - (sint32)raw) / 2;
        records = r_u32(0x800B6A74u);
        record_index = (sint16)r_u16(records + 426u);
        w_u16(records + 260u, (uint16)(252 - value));
        sprite = r_u32(records + 420u) + 120u * (uint32)(sint32)record_index;
        if (value < 0)
            value = -value;
        if (400 * (sint16)value / 252 < 9)
        {
            value = (sint16)input_calibrations[selected].stick_range >> 6;
            if (value < 0)
                value = -value;
            w_u16(0x800B4234u, 0u);
        }
        else if ((sint16)r_u16(0x800B4234u) >= (sint16)value)
            value = (sint16)r_u16(0x800B4234u);
        else
        {
            sint32 magnitude = 127 - (sint32)raw;
            w_u16(0x800B4234u, (uint16)value);
            if (magnitude < 0)
                magnitude = -magnitude;
            input_calibrations[selected].stick_range = (uint16)((uint16)(32 * magnitude));
        }
        percent = 400 * (sint16)value / 252;
        scale = 2 * (sint16)value + 1;
        if (percent >= 101)
            percent = 100;
        if (scale >= 129)
            scale = 128;
        w_u32(sprite + 16u, (uint32)scale);
        w_u32(sprite, (uint32)(1024 - scale));
        text = text_menu[3].text;
        text[0] = (uint8)(percent / 100 + 48);
        text[1u] = (uint8)(percent % 100 / 10 + 48);
        text[2u] = (uint8)(percent % 10 + 48);
        if ((controller->pressed & 0x800u) != 0u)
        {
            w_u16(0x800B413Eu, 2u);
            result = 1;
            w_u16(0x800B6AFAu, 1u);
        }
    }
    else
        w_u16(0x800B413Eu, 2u);
    return result;
}

sint32 profile_format_value_parts3(uint32 values, char *text)
{
    uint32 middle = r_u16(values + 2u);
    uint32 last = r_u16(values + 4u);
    uint32 first = r_u16(values);
    uint32 remainder;
    sint32 result = (sint32)last;

    FUNCTION_MARKER(0x80053898u, "MAIN.EXE");
    text[0] = 48u;
    text[2u] = 48u;
    text[3u] = 48u;
    text[5u] = 48u;
    text[6u] = 48u;
    if (first != 0u)
        text[0] = (uint8)(first + 48u);
    if (middle != 0u)
    {
        remainder = middle % 100u;
        text[2u] = (uint8)(remainder / 10u + 48u);
        text[3u] = (uint8)(remainder % 10u + 48u);
    }
    if (last != 0u)
    {
        uint32 tens = (last / 10u) % 100u / 10u;
        text[5u] = (uint8)(tens + 48u);
        result = (sint32)(10u * tens);
        text[6u] = (uint8)((last / 10u) % 100u % 10u + 48u);
    }
    return result;
}

sint32 time_fields3_compare(uint32 first, uint32 second)
{
    FUNCTION_MARKER(0x800539BCu, "MAIN.EXE");
    if (r_u16(first + 4u) != r_u16(second + 4u))
        return 0;
    if (r_u16(first + 2u) != r_u16(second + 2u))
        return 0;
    return r_u16(first) == r_u16(second);
}

uint32 time_fields_convert(uint32 value)
{
    uint32 first = r_u16(value);
    uint32 second = r_u16(value + 2u);
    uint32 third = r_u16(value + 4u);

    FUNCTION_MARKER(0x80053A00u, "MAIN.EXE");
    return 60000u * first + 100u * second + third / 10u;
}

sint32 profile_reset_grid_flags(void)
{
    uint32 profile = profile_get_active();
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 result = (sint32)profile;
    sint32 row;
    sint32 index;

    FUNCTION_MARKER(0x80053E14u, "MAIN.EXE");
    if ((r_u16(0x800E0588u) & 2u) != 0u)
    {
        uint8 flags = profile_data->reward_flags;

        profile_data->max_level = 3u;
        profile_data->reward_flags = (uint8)(flags | 0x80u);
        for (row = 0; row < 3; ++row)
        {
            for (index = 5; index >= 0; --index)
                profile_data->progress.courses[(uint32)row][(uint32)index].state = (uint8)(6u);
            for (index = 5; index >= 0; --index)
                profile_data->progress.courses[(uint32)row][(uint32)index].result = (uint8)(1u);
        }
        for (index = 9; index >= 0; --index)
            profile_current()->availability[index] = 1u;
        result = (sint32)(profile - 1u);
    }
    return result;
}

sint32 profile_update_trans(void)
{
    PLAYER_PROFILE *profile = profile_current();
    sint32 result;

    FUNCTION_MARKER(0x80053ECCu, "MAIN.EXE");
    menu_save_desc_payloads((sint16)r_u16(0x800B413Cu));
    result = 4;
    if ((sint16)r_u16(0x800B413Cu) == 3)
    {
        PROFILE_SERIES *series;

        profile_init_mode_rec();
        profile_reset_grid_flags();
        profile->reset_pending = 0u;
        series = profile_get_series();
        result = 5;
        if ((sint16)r_u16(0x800B413Eu) == 5)
        {
            result = (sint16)r_u16(0x800E0582u);
            if (result != 0 && result != 2)
            {
                result = (sint16)series->phase;
                if (result == 2)
                {
                    result = 18;
                    w_u16(0x800B413Eu, 18u);
                }
            }
        }
    }
    else if ((sint16)r_u16(0x800B413Cu) == 4)
    {
        profile_select_mode();
        result = profile_reset_grid_flags();
        profile->reset_pending = 0u;
    }
    return result;
}

sint32 profile_update_mode_select_vis(void)
{
    sint16 track = (sint16)r_u16(0x800B413Cu);
    uint32 table = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)track);
    sint32 index;
    sint32 visible;
    sint16 selection;
    uint32 primitive;
    uint32 records;

    FUNCTION_MARKER(0x80053FA4u, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        uint32 entry = table + 20u * (uint32)index;
        sint16 record_index = (sint16)r_u16(entry + 12u);
        uint32 list;
        sint32 child;
        visible = index < 5;
        records = r_u32(0x800B6A74u);
        w_u8(records + 4u + 84u * (uint32)(sint32)record_index, (uint8)visible);
        list = r_u32(entry + 16u);
        for (child = 0; child < (sint16)r_u16(list + 6u); ++child)
        {
            record_index = (sint16)r_u16(list + 8u + 2u * (uint32)child);
            records = r_u32(0x800B6A74u);
            w_u8(records + 4u + 84u * (uint32)(sint32)record_index, (uint8)visible);
        }
    }
    profile_build_vehicle_grid(8, (sint16)r_u16(0x800B4238u), 344, 130);
    visible = (uint8)profile_is_unlocked((sint16)((sint16)r_u16(0x800B4238u))) != 0u;
    text_clear_boat_names();
    selection = (sint16)r_u16(0x800B4238u);
    primitive = 496u + 160u * (uint32)(sint32)selection;
    text_hud[31 + 10 * selection].visible = 1;
    text_hud[31 + 10 * selection].font = 1;
    records = r_u32(0x800B6A74u);
    text_hud[31 + 10 * selection].x = 120;
    text_hud[31 + 10 * selection].y = 160;
    w_u8(records + 3364u, (uint8)visible);
    text_menu[4].visible = (uint8)visible;
    return 1;
}

sint32 profile_update_carousel(sint16 direction, CONTROLLER_STATE *input)
{
    sint32 index;

    FUNCTION_MARKER(0x80054148u, "MAIN.EXE");
    if (direction == 0)
    {
        sint32 result;
        if ((input->current & 0x10u) != 0u)
        {
            uint8 gate = r_u8(0x800B4138u);
            if (gate != 0u)
            {
                sint16 state = (sint16)r_u16(0x800B423Cu);
                if (state == 0)
                {
                    uint8 pages = r_u8(0x800E058Eu);
                    w_u16(0x800B6AFAu, 2u);
                    if (pages != 0u)
                    {
                        sint16 page = (sint16)r_u16(0x800B423Au);
                        if (page != 0)
                        {
                            page = (sint16)(page - 1);
                            w_u16(0x800B413Eu, 5u);
                            w_u16(0x800B423Au, (uint16)page);
                            w_u8(0x800B4139u, 1u);
                            sprite_clear_anim_recs();
                            profile_update_trans();
                            page = (sint16)r_u16(0x800B423Au);
                            w_u16(0x800B4238u, r_u8(0x800E05ADu + (uint32)(sint32)page));
                        }
                        else
                        {
                            uint8 mode = r_u8(0x800E058Du);
                            w_u16(0x800B413Eu, 25u);
                            if (mode == 0u && (sint16)r_u16(0x800E0582u) == 0)
                                profile_backup_selection();
                        }
                    }
                    else
                    {
                        uint8 mode = r_u8(0x800E058Du);
                        w_u16(0x800B413Eu, 25u);
                        if (mode == 0u && (sint16)r_u16(0x800E0582u) == 0)
                            profile_backup_selection();
                    }
                }
            }
        }
        result = input->current & 0x40u;
        if (result == 0)
            return result;
        result = r_u8(0x800B4138u);
        if (result == 0)
            return result;
        result = (sint16)r_u16(0x800B423Cu);
        if (result != 0)
            return 1;
        {
            sint16 selection = (sint16)r_u16(0x800B4238u);
            w_u16(0x800B6AFAu, 1u);
            result = (uint8)profile_is_unlocked((sint16)(selection));
            if (result == 0)
            {
                w_u16(0x800B6AFAu, 5u);
                return 5;
            }
            else
            {
                sint16 page = (sint16)r_u16(0x800B423Au);
                uint8 selected = r_u8(0x800B4238u);
                sint16 page_snapshot;
                uint8 pages;

                w_u8(0x800E05ADu + (uint32)(sint32)page, selected);
                page_snapshot = (sint16)r_u16(0x800B423Au);
                pages = r_u8(0x800E058Eu);
                if (page_snapshot + 1 == pages)
                {
                    sint16 mode = (sint16)r_u16(0x800E0582u);
                    if (mode == 0 || mode == 2 || page_snapshot != 0)
                        w_u16(0x800B413Eu, 10u);
                    else
                        w_u16(0x800B413Eu, 18u);
                }
                else
                {
                    TEXT_RECORD *menu;
                    char *output;

                    page_snapshot = (sint16)(page_snapshot + 1);
                    menu = text_menu;
                    w_u8(0x800B4139u, 1u);
                    output = menu[5].text;
                    w_u16(0x800B423Au, (uint16)page_snapshot);
                    w_u16(0x800B413Eu, 5u);
                    name_copy_player_name_bytes((uint16)page_snapshot, output);
                    page_snapshot = (sint16)r_u16(0x800B423Au);
                    w_u16(0x800B4238u, r_u8(0x800E05ADu + (uint32)(sint32)page_snapshot));
                }
                sprite_clear_anim_recs();
                return profile_update_trans();
            }
        }
    }
    if (direction == 1 || direction == 2)
    {
        sint32 table_index = direction == 1 ? 7 : 1;
        for (index = 0; index < 8; ++index)
        {
            uint32 record;
            sint16 record_index;
            sint16 target_x;
            sint16 target_y;
            sint16 target_first;
            sint16 target_second;
            uint32 records;
            if (table_index == 8)
                table_index = 0;
            record_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
            target_x = (sint16)r_u16(0x800DD350u + 2u * (uint32)table_index);
            target_y = (sint16)r_u16(0x800DD380u + 2u * (uint32)table_index);
            target_first = (sint16)r_u16(0x800E9010u + 2u * (uint32)table_index);
            target_second = (sint16)r_u16(0x800E9020u + 2u * (uint32)table_index);
            records = r_u32(0x800B6A74u);
            record = records + 84u * (uint32)(sint32)record_index;
            sprite_init_trans(record, target_x, target_y, target_first, target_second, 10);
            ++table_index;
        }
        if (direction == 1)
        {
            sint16 selection = (sint16)(r_u16(0x800B4238u) + 1u);
            w_u16(0x800B4238u, (uint16)selection);
            if (selection == 8)
                w_u16(0x800B4238u, 0u);
            w_u16(0x800B423Eu, 1u);
        }
        else
        {
            sint16 selection = (sint16)(uint16)(r_u16(0x800B4238u) - 1u);
            w_u16(0x800B4238u, (uint16)selection);
            if (selection < 0)
                w_u16(0x800B4238u, 7u);
            w_u16(0x800B423Eu, 2u);
        }
        w_u16(0x800B423Cu, 3u);
        return 3;
    }
    if (direction == 3)
    {
        uint32 frame = guest_stack_push(0x90u);
        uint32 saved = frame + 0x18u;
        sint16 first_index = (sint16)r_u16(0x80098D2Cu);
        uint32 records = r_u32(0x800B6A74u);
        uint32 first = records + 84u * (uint32)(sint32)first_index;
        uint8 state = r_u8(first + 31u);
        if (state == 5u)
        {
            sint16 rotation = (sint16)r_u16(0x800B423Eu);
            if (rotation == 1)
            {
                sprite_copy_anim_rec_bytes(first, saved);
                for (index = 0; index < 7; ++index)
                {
                    sint16 source_index = (sint16)r_u16(0x80098D2Eu + 2u * (uint32)index);
                    sint16 destination_index;
                    uint32 source;
                    uint32 destination;

                    records = r_u32(0x800B6A74u);
                    destination_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
                    source = records + 84u * (uint32)(sint32)source_index;
                    destination = records + 84u * (uint32)(sint32)destination_index;
                    sprite_copy_anim_rec_bytes(source, destination);
                }
                first_index = (sint16)r_u16(0x80098D3Au);
                records = r_u32(0x800B6A74u);
                sprite_copy_anim_rec_bytes(saved, records + 84u * (uint32)(sint32)first_index);
                first_index = (sint16)r_u16(0x80098D2Cu);
                records = r_u32(0x800B6A74u);
                sprite_apply_prim_vram_mask(records + 84u * (uint32)(sint32)first_index);
                first_index = (sint16)r_u16(0x80098D32u);
                records = r_u32(0x800B6A74u);
                sprite_disable_semitransparency(records + 84u * (uint32)(sint32)first_index);
            }
            rotation = (sint16)r_u16(0x800B423Eu);
            if (rotation == 2)
            {
                uint32 last;

                first_index = (sint16)r_u16(0x80098D3Au);
                records = r_u32(0x800B6A74u);
                last = records + 84u * (uint32)(sint32)first_index;
                sprite_copy_anim_rec_bytes(last, saved);
                for (index = 7; index > 0; --index)
                {
                    sint16 source_index = (sint16)r_u16(0x80098D2Au + 2u * (uint32)index);
                    sint16 destination_index;
                    uint32 source;
                    uint32 destination;

                    records = r_u32(0x800B6A74u);
                    destination_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
                    source = records + 84u * (uint32)(sint32)source_index;
                    destination = records + 84u * (uint32)(sint32)destination_index;
                    sprite_copy_anim_rec_bytes(source, destination);
                }
                first_index = (sint16)r_u16(0x80098D2Cu);
                records = r_u32(0x800B6A74u);
                sprite_copy_anim_rec_bytes(saved, records + 84u * (uint32)(sint32)first_index);
                first_index = (sint16)r_u16(0x80098D34u);
                records = r_u32(0x800B6A74u);
                sprite_apply_prim_vram_mask(records + 84u * (uint32)(sint32)first_index);
                first_index = (sint16)r_u16(0x80098D2Eu);
                records = r_u32(0x800B6A74u);
                sprite_disable_semitransparency(records + 84u * (uint32)(sint32)first_index);
            }
            {
                uint16 selection_bits = r_u16(0x800B4238u);
                sint32 selection = (sint16)(uint16)(selection_bits - 2u);
                uint32 records_snapshot;
                if (selection < 0)
                    selection = (sint16)(uint16)(selection_bits + 6u);
                records_snapshot = r_u32(0x800B6A74u);
                for (index = 0; index < 8; ++index)
                {
                    sint16 record_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
                    uint16 selected_record = r_u16(0x80098D2Cu + 2u * (uint32)selection);
                    uint32 record = records_snapshot + 84u * (uint32)(sint32)record_index;
                    w_u16(record + 6u, selected_record);
                    selection = (sint16)(uint16)(selection + 1);
                    if (selection == 8)
                        selection = 0;
                }
            }
            profile_update_mode_select_vis();
        }
        first_index = (sint16)r_u16(0x80098D2Cu);
        records = r_u32(0x800B6A74u);
        first = records + 84u * (uint32)(sint32)first_index;
        state = r_u8(first + 31u);
        if (state == 0u)
        {
            sint16 record_index = (sint16)r_u16(0x80098D30u);
            sint32 result;
            w_u16(0x800B423Cu, 4u);
            result = menu_move_sprite_rec(records + 84u * (uint32)(sint32)record_index, 1, 8, 6, 1, 1, 0, 0, 0, 0);
            guest_stack_pop(0x90u);
            return result;
        }
        guest_stack_pop(0x90u);
        return state;
    }
    if (direction == 4)
    {
        sint16 record_index = (sint16)r_u16(0x80098D30u);
        uint32 records = r_u32(0x800B6A74u);
        uint32 record = records + 84u * (uint32)(sint32)record_index;
        uint8 state = r_u8(record + 12u);
        if (state == 0u)
        {
            w_u16(0x800B423Cu, 0u);
            w_u8(record + 12u, 2u);
        }
        return 2;
    }
    return 4;
}

sint32 vehicle_update_carousel(void)
{
    sint16 track = (sint16)r_u16(0x800B413Cu);
    uint32 records = r_u32(0x800B6A74u);
    uint32 table = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)track);
    PLAYER_PROFILE *profile;
    sint32 selection;
    sint32 index;
    sint32 result = 1;

    FUNCTION_MARKER(0x800549ECu, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        sint16 record_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
        uint32 record = records + 84u * (uint32)(sint32)record_index;
        uint16 value = r_u16(record + 8u);
        uint32 list;

        w_u16(0x800DD350u + 2u * (uint32)index, value);
        value = r_u16(record + 10u);
        w_u16(0x800DD380u + 2u * (uint32)index, value);
        list = r_u32(table + 16u + 20u * (uint32)index);
        value = r_u16(list);
        w_u16(0x800E9010u + 2u * (uint32)index, value);
        value = r_u16(list + 2u);
        w_u16(0x800E9020u + 2u * (uint32)index, value);
    }
    selection = (sint16)r_u16(0x800B4238u) - 2;
    if (selection < 0)
        selection += 8;
    profile = profile_current();
    for (index = 0; index < 8; ++index)
    {
        uint32 record;
        uint32 primitive;
        sint32 enabled;
        sint16 record_index;
        uint16 selected_record;
        if (selection == 8)
            selection = 0;
        record_index = (sint16)r_u16(0x80098D2Cu + 2u * (uint32)index);
        selected_record = r_u16(0x80098D2Cu + 2u * (uint32)selection);
        records = r_u32(0x800B6A74u);
        record = records + 84u * (uint32)(sint32)record_index;
        w_u16(record + 6u, selected_record);
        w_u8(record + 4u, 1u);
        primitive = r_u32(record) + 120u * (uint32)(sint16)r_u16(record + 6u);
        enabled = profile->availability[selection] != 0u;
        w_u8(primitive + 58u, enabled != 0 ? 128u : 32u);
        w_u8(primitive + 57u, enabled != 0 ? 128u : 32u);
        w_u8(primitive + 56u, enabled != 0 ? 128u : 32u);
        vehicle_select_enable_prim(primitive + 52u, enabled);
        ++selection;
    }
    profile_update_mode_select_vis();
    if (r_u8(0x800E058Eu) == 1u)
    {
        for (index = 4; index > 0; --index)
            text_menu[5].visible = 0u;
    }
    else
    {
        TEXT_RECORD *menu = text_menu;
        uint16 active = r_u16(0x800B423Au);
        char *output = menu[5].text;
        name_copy_player_name_bytes(active, output);
    }
    if ((sint16)r_u16(0x800E0582u) == 1 || r_u8(0x800E058Eu) >= 2u)
    {
        TEXT_RECORD *menu = text_menu;
        uint32 records_now;
        uint16 value;

        menu[3].visible = 0u;
        menu = text_menu;
        records_now = r_u32(0x800B6A74u);
        value = menu[3].x;
        menu[4].x = value;
        w_u8(records_now + 3448u, 0u);
        records_now = r_u32(0x800B6A74u);
        result = r_u16(records_now + 3452u);
        w_u16(records_now + 3368u, (uint16)result);
    }
    return result;
}

sint32 vehicle_select_assign_palettes(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    uint32 table = r_u32(0x80097E8Cu);
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 records = r_u32(0x800B6A74u);
    sint32 group = 0;
    sint32 index;

    FUNCTION_MARKER(0x80054CB4u, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 screen = (sint16)r_u16(0x800B4246u);
        sint32 type = profile_data->progress.courses[(uint32)(sint32)screen][(uint32)index].state;
        sint16 selection;
        sint16 palette = 0;
        uint32 output;
        uint32 entry;
        uint16 clut;

        if ((sint16)r_u16(0x800E0582u) == 1)
        {
            selection = (sint16)r_u16(0x800E8FF0u + (uint32)index * 2u);
            sint32 split = (sint16)r_u16(0x800B4244u);
            if (index < split)
                type = 6;
            else
                type = index == split ? 4 : 5;
        }
        else
        {
            selection = (sint16)r_u16(0x800E8FF0u + (uint32)index * 2u);
        }
        screen = (sint16)r_u16(0x800B4246u);
        if (screen == 1)
        {
            group = 23;
            if (type == 5)
                palette = selection != 0 ? 504 : 506;
            else if (type == 6)
                palette = selection != 0 ? 235 : 234;
            else
                palette = selection != 0 ? 507 : 505;
        }
        else if (screen == 0 || screen == 2)
        {
            if (type == 5)
                palette = selection != 0 ? 508 : 510;
            else if (type == 6)
                palette = selection != 0 ? 229 : 228;
            else
                palette = selection != 0 ? 511 : 509;
        }
        else if (screen == 3)
        {
            palette = (sint16)(index + 229);
            group = (uint16)(index - 1) < 4u ? 36 : 0;
        }
        output = r_u32(records) + 120u * (uint32)(group + index);
        clut = getClut(768, palette);
        screen = (sint16)r_u16(0x800B4246u);
        w_u16(output + 66u, clut);
        w_u8(output + 46u, (uint8)(screen == 2));
        entry = table + 20u * (uint32)(sint32)selection;
        screen = (sint16)r_u16(0x800B4246u);
        w_u16(entry + 12u, (uint16)(group + index));
        if (screen == 3)
            w_u16(entry + 8u, group != 0);
    }
    return index << 16;
}

sint32 vehicle_advance_carousel(sint16 action, CONTROLLER_STATE *input)
{
    uint32 menu_words[9];
    uint16 menu_last;
    sint32 apply_offset = 0;
    sint16 base_offset;
    sint32 index;

    FUNCTION_MARKER(0x80054FC0u, "MAIN.EXE");
    for (index = 0; index < 9; ++index)
        menu_words[index] = r_u32(0x80082388u + 4u * (uint32)index);
    menu_last = r_u16(0x800823ACu);
    if ((sint16)r_u16(0x800B4240u) != 0)
    {
        sint16 record_index = (sint16)r_u16(0x800B4242u);
        uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
        if (r_u8(record + 12u) == 0u)
            menu_move_sprite_rec(record, 1, 8, 6, 1, 1, (sint32)menu_words[0], (sint32)menu_words[1], (sint16)menu_words[2], (sint16)menu_words[3]);
    }
    if (action == 0)
    {
        sint32 result = input->current & 0x40u;
        sint32 enabled = 0;

        if (result != 0)
        {
            result = r_u8(0x800B4138u);
            if (result != 0)
            {
                result = (sint16)r_u16(0x800B6AA0u) == 0;
                enabled = result;
            }
        }
        if (enabled == 0)
            return result;
        {
            uint8 row = r_u8(0x800B4246u);
            uint8 column = r_u8(0x800B4244u);

            if ((uint8)profile_grid_cell_is_available(row, column, 0u, 0u) != 0u)
            {
                if ((sint16)r_u16(0x800E0582u) == 6)
                {
                    sint16 mode = (sint16)r_u16(0x800E0584u);

                    w_u16(0x800B6AFAu, 1u);
                    if (mode == 1)
                    {
                        w_u16(0x800B413Eu, 30u);
                        return tournament_reset_grid();
                    }
                    if (mode < 2)
                    {
                        if (mode == 0)
                            w_u16(0x800B413Eu, 38u);
                        return 38;
                    }
                    if (mode == 2)
                    {
                        w_u16(0x800B413Eu, 31u);
                        return tournament_init_round();
                    }
                    return 31;
                }
                voice_start_scaled((sint16)r_u16(0x800834B4u), 1);
                sound_fn_8007741c(0, 128u);
                return profile_commit_selection();
            }
        }
        w_u16(0x800B6AFAu, 5u);
        return 5;
    }
    base_offset = (sint16)r_u16(r_u32(0x800B6A74u) + 50u);
    switch ((sint16)(action - 1))
    {
        case 0:
        case 1:
        {
            uint16 old_column;
            sint16 column;
            sint16 record_index;
            uint32 record;
            uint32 target_offset;
            sint16 target_x;
            sint16 target_y;

            if (action == 1)
            {
                profile_sort_recs_desc(0u, 0u);
                old_column = r_u16(0x800B4244u);
                w_u16(0x800B4244u, (uint16)(old_column + 1u));
                column = (sint16)(uint16)(old_column + 1u);
                w_u16(0x800B6BECu, old_column);
                if (column >= 6)
                {
                    column = 5;
                    w_u16(0x800B4244u, 5u);
                }
            }
            else
            {
                profile_sort_recs_asc(0u, 0u, 0u);
                old_column = r_u16(0x800B4244u);
                w_u16(0x800B4244u, (uint16)(old_column - 1u));
                w_u16(0x800B6BECu, old_column);
                column = (sint16)(uint16)(old_column - 1u);
                if (column < 0)
                {
                    column = 0;
                    w_u16(0x800B4244u, 0u);
                }
            }
            column = (sint16)r_u16(0x800B4244u);
            record_index = (sint16)r_u16(0x800E8FF0u + 2u * (uint32)column);
            record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
            w_u16(record + 8u, r_u16(0x800E35A0u + 2u * (uint32)column));
            w_u16(record + 10u, r_u16(0x800E35B0u + 2u * (uint32)column));
            target_offset = 2u * (uint32)(sint32)(sint16)old_column;
            target_y = (sint16)r_u16(0x800E35B0u + 2u * (uint32)column);
            target_x = (sint16)((sint16)r_u16(0x800E35A0u + target_offset) + (action == 1 ? 134 : -134));
            sprite_init_trans(record, target_x, target_y, 0, 0, 6);
            w_u16(0x800B424Au, (uint16)action);
            w_u16(0x800B4248u, 3u);
            break;
        }
        case 2:
        {
            sint16 column = (sint16)r_u16(0x800B4244u);
            sint16 record_index = (sint16)r_u16(0x800E8FF0u + 2u * (uint32)column);
            uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
            if (r_u8(record + 31u) == 0u)
            {
                sint16 x = (sint16)r_u16(record + 8u);
                sint16 y = (sint16)r_u16(record + 10u);
                uint32 records;
                uint8 visible;
                sint16 target_x;
                sint16 target_y;
                uint8 row;
                uint8 selected_column;

                if ((sint16)r_u16(0x800B424Au) == 1)
                    profile_sort_recs_desc(0u, 0u);
                else
                    profile_sort_recs_asc(0u, 0u, 0u);
                column = (sint16)r_u16(0x800B4244u);
                records = r_u32(0x800B6A74u);
                w_u16(records + 8u, (uint16)x);
                w_u16(records + 10u, (uint16)y);
                target_x = (sint16)r_u16(0x800E35A0u + 2u * (uint32)column);
                target_y = (sint16)r_u16(0x800E35B0u + 2u * (uint32)column);
                sprite_init_trans(records, target_x, target_y, 0, 0, 6);
                row = r_u8(0x800B4246u);
                selected_column = r_u8(0x800B4244u);
                visible = (uint8)profile_grid_cell_is_available(row, selected_column, 0u, 0u);
                w_u8(r_u32(0x800B6A74u) + 676u, visible);
                text_menu[3].visible = visible;
                profile_update_select_prims(0u, 0u, 0u, 0u);
                vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
                w_u16(0x800B411Au, 2u);
                w_u16(0x800B4248u, 4u);
            }
            break;
        }
        case 3:
            if (r_u8(r_u32(0x800B6A74u) + 31u) == 0u)
                w_u16(0x800B4248u, 0u);
            break;
        case 4:
            if (r_u8(r_u32(0x800B6A74u) + 31u) != 0u)
            {
                apply_offset = 1;
                break;
            }
            profile_init_menu(1, 0u, 0u, 0u);
            menu_start_dir_trans(1);
            {
                TEXT_RECORD *menu = text_menu;
                uint32 records;
                uint16 value;
                uint16 last_value;

                w_u16(0x800B4248u, 6u);
                for (index = 0; index < 19; ++index)
                {
                    sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                    TEXT_RECORD *address = &menu[menu_index];
                    address[0].x = (uint16)(address[0].x + 1029u);
                }
                records = r_u32(0x800B6A74u);
                value = r_u16(records + 1520u);
                last_value = r_u16(records + 1688u);
                w_u16(records + 1520u, (uint16)(value + 1029u));
                value = r_u16(records + 1604u);
                w_u16(records + 1688u, (uint16)(last_value + 1029u));
                w_u16(records + 1604u, (uint16)(value + 1029u));
            }
            break;
        case 5:
        case 7:
            if (r_u8(r_u32(0x800B6A74u) + 31u) == 0u)
                w_u16(0x800B4248u, 0u);
            else
                apply_offset = 1;
            break;
        case 6:
            if (r_u8(r_u32(0x800B6A74u) + 31u) != 0u)
            {
                apply_offset = 1;
                break;
            }
            profile_init_menu(3, 0u, 0u, 0u);
            menu_start_dir_trans(3);
            {
                TEXT_RECORD *menu = text_menu;
                uint32 records;
                uint16 value;
                uint16 last_value;

                w_u16(0x800B4248u, 8u);
                for (index = 0; index < 19; ++index)
                {
                    sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                    TEXT_RECORD *address = &menu[menu_index];
                    address[0].x = (uint16)(address[0].x - 980u);
                }
                records = r_u32(0x800B6A74u);
                value = r_u16(records + 1520u);
                last_value = r_u16(records + 1688u);
                w_u16(records + 1520u, (uint16)(value - 980u));
                value = r_u16(records + 1604u);
                w_u16(records + 1688u, (uint16)(last_value - 980u));
                w_u16(records + 1604u, (uint16)(value - 980u));
            }
            break;
        default:
            break;
    }
    if (apply_offset != 0)
    {
        {
            TEXT_RECORD *menu = text_menu;
            uint32 records;
            uint16 value;
            uint16 last_value;

            for (index = 0; index < 19; ++index)
            {
                sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                TEXT_RECORD *address = &menu[menu_index];
                address[0].x = (uint16)(address[0].x + (uint32)(sint32)base_offset);
            }
            records = r_u32(0x800B6A74u);
            value = r_u16(records + 1520u);
            last_value = r_u16(records + 1688u);
            w_u16(records + 1520u, (uint16)(value + (uint32)(sint32)base_offset));
            value = r_u16(records + 1604u);
            w_u16(records + 1688u, (uint16)(last_value + (uint32)(sint32)base_offset));
            w_u16(records + 1604u, (uint16)(value + (uint32)(sint32)base_offset));
        }
    }
    return vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
}

sint32 profile_populate_select_recs(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 index;

    FUNCTION_MARKER(0x800557ACu, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 offset = (sint16)(2 * (sint16)r_u16(0x800E8D40u + (uint32)index * 2u));
        sint16 source_index = (sint16)r_u16(0x800E8FF0u + (uint32)index * 2u);
        uint32 source = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)source_index;
        uint32 output;
        uint16 coordinate;
        sint16 screen;
        uint8 visible = 0u;

        w_u8(source + 81u, (uint8)(offset + 1));
        output = r_u32(0x800B6A74u) + 4116u + 84u * (uint32)index;
        coordinate = r_u16(source + 8u);
        w_u16(output + 8u, (uint16)(coordinate + 100u));
        coordinate = r_u16(source + 10u);
        w_u8(output + 81u, (uint8)offset);
        screen = (sint16)r_u16(0x800B4246u);
        w_u16(output + 10u, (uint16)(coordinate + 40u));
        if (profile_data->progress.courses[(uint32)(sint32)screen][(uint32)index].state == 6u && screen != 3)
        {
            if ((sint16)r_u16(0x800E0582u) != 1 || index < (sint16)r_u16(0x800B4244u))
                visible = 1u;
        }
        w_u8(output + 4u, visible);
    }
    return index << 16;
}

sint32 profile_sort_recs_desc(uint32 unused1, uint32 unused2)
{
    sint16 split = (sint16)r_u16(0x800B4244u);
    sint16 track = (sint16)r_u16(0x800B413Cu);
    sint32 output_index = 0;
    sint32 secondary = 1;
    uint32 table = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)track);

    FUNCTION_MARKER(0x800558ECu, "MAIN.EXE");
    if (split >= 0)
    {
        uint32 records = r_u32(0x800B6A74u);
        sint16 value = split;

        do
        {
            uint32 value_offset = 2u * (uint32)(sint32)value;
            uint32 record = records + 84u * (uint32)(sint16)output_index;
            uint16 x = r_u16(0x800E35A0u + value_offset);
            uint16 y;
            uint32 entry;

            w_u16(record + 8u, x);
            y = r_u16(0x800E35B0u + value_offset);
            entry = table + 20u * (uint32)(sint16)output_index;
            w_u16(record + 6u, (uint16)value);
            w_u16(record + 10u, y);
            w_u16(entry + 12u, (uint16)value);
            value = (sint16)(value - 1);
            w_u16(0x800E8FF0u + value_offset, (uint16)output_index);
            w_u16(0x800E8D40u + value_offset, (uint16)output_index);
            output_index = (sint16)(output_index + 1);
        } while (value >= 0);
    }
    split = (sint16)r_u16(0x800B4244u);
    if (split < 5)
    {
        sint16 value = (sint16)(split + 1);

        if (value < 6)
        {
            uint32 records = r_u32(0x800B6A74u);

            do
            {
                uint32 value_offset = 2u * (uint32)(sint32)value;
                uint32 record = records + 84u * (uint32)(sint16)output_index;
                uint16 x = r_u16(0x800E35A0u + value_offset);
                uint16 y;
                uint32 entry;

                w_u16(record + 8u, x);
                y = r_u16(0x800E35B0u + value_offset);
                entry = table + 20u * (uint32)(sint16)output_index;
                w_u16(record + 6u, (uint16)value);
                w_u16(record + 10u, y);
                w_u16(entry + 12u, (uint16)value);
                value = (sint16)(value + 1);
                w_u16(0x800E8FF0u + value_offset, (uint16)output_index);
                output_index = (sint16)(output_index + 1);
                w_u16(0x800E8D40u + value_offset, (uint16)secondary);
                secondary = (sint16)(secondary + 1);
            } while (value < 6);
        }
    }
    return profile_populate_select_recs(0u, 0u, 0u, 0u);
}

sint32 profile_sort_recs_asc(uint32 unused1, uint32 unused2, uint32 unused3)
{
    sint32 split = (sint16)r_u16(0x800B4244u);
    sint16 track = (sint16)r_u16(0x800B413Cu);
    uint32 table = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)track);
    sint32 value = split;
    sint32 output_index = 0;
    sint32 secondary = 1;

    FUNCTION_MARKER(0x80055AA8u, "MAIN.EXE");
    while (value < 6)
    {
        uint32 value_offset = 2u * (uint32)(sint32)(sint16)value;
        uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint16)output_index;
        uint32 entry = table + 20u * (uint32)output_index;
        uint16 first = r_u16(0x800E35A0u + value_offset);
        uint16 second;

        w_u16(record + 8u, first);
        second = r_u16(0x800E35B0u + value_offset);
        w_u16(record + 6u, (uint16)value);
        w_u8(record + 81u, 0u);
        w_u16(record + 10u, second);
        w_u16(entry + 12u, (uint16)value);
        ++value;
        w_u16(0x800E8FF0u + value_offset, (uint16)output_index);
        w_u16(0x800E8D40u + value_offset, (uint16)output_index);
        ++output_index;
    }
    split = (sint16)r_u16(0x800B4244u);
    if (split > 0)
    {
        value = (sint16)(split - 1);
        while ((sint16)value >= 0)
        {
            uint32 value_offset = 2u * (uint32)(sint32)(sint16)value;
            uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint16)output_index;
            uint32 entry = table + 20u * (uint32)output_index;
            uint16 first = r_u16(0x800E35A0u + value_offset);
            uint16 second;

            w_u16(record + 8u, first);
            second = r_u16(0x800E35B0u + value_offset);
            w_u16(record + 6u, (uint16)value);
            w_u8(record + 81u, 0u);
            w_u16(record + 10u, second);
            w_u16(entry + 12u, (uint16)value);
            --value;
            w_u16(0x800E8FF0u + value_offset, (uint16)output_index);
            ++output_index;
            w_u16(0x800E8D40u + value_offset, (uint16)secondary);
            ++secondary;
        }
    }
    return profile_populate_select_recs(0u, 0u, 0u, 0u);
}

sint32 profile_fn_80055cfc(sint16 direction, sint16 index, uint32 state)
{
    sint32 current = r_u16(state + 8u);
    sint32 target = 0;

    FUNCTION_MARKER(0x80055CFCu, "MAIN.EXE");
    if (direction == 0)
        target = current - 512;
    else if (direction == 1)
    {
        if (index < 6)
        {
            target = current;
            w_u16(state + 8u, (uint16)(current + 512));
        }
        else
        {
            target = current + 512;
            w_u16(state + 8u, (uint16)(current + 1024));
        }
    }
    else if (direction == 2)
        target = current + 512;
    else if (direction == 3)
    {
        if (index < 6)
        {
            target = current;
            w_u16(state + 8u, (uint16)(current - 512));
        }
        else
        {
            target = current - 512;
            w_u16(state + 8u, (uint16)(current - 1024));
        }
    }
    return sprite_init_trans(state, (sint16)target, (sint16)r_u16(state + 10u), 0, 0, 25);
}

sint32 menu_start_dir_trans(sint16 direction)
{
    uint32 records = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x80055DE4u, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
        profile_fn_80055cfc(direction, (sint16)index, records + 84u * (uint32)index);
    for (index = 4; index < 10; ++index)
        profile_fn_80055cfc(direction, 6, records + 3108u + 84u * (uint32)index);
    return 0;
}

sint32 profile_is_unlocked(sint16 index)
{
    FUNCTION_MARKER(0x80055EC8u, "MAIN.EXE");
    return profile_current()->availability[index] == 1u;
}

sint32 profile_level_is_available(sint32 requested, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 value = (uint8)requested;

    FUNCTION_MARKER(0x80055F0Cu, "MAIN.EXE");
    if ((sint16)r_u16(0x800B69CAu) != 0)
        return 0;
    if ((sint16)r_u16(0x800E0582u) == 2)
    {
        if (value == 3u)
            return 0;
    }
    else if (value == 3u && r_u8(0x800E058Eu) != 1u)
    {
        if (name_player_name_matches(0, 0x800B4254u) != 0 && r_u8(0x800E058Eu) == 2u)
            return (sint16)r_u16(0x800E0582u) != 6;
        return 0;
    }
    if (r_u8(0x800E0594u) != 0u)
        return 1;
    return profile_data->max_level >= value;
}

sint32 profile_grid_cell_is_available(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x8005600Cu, "MAIN.EXE");
    if (r_u8(0x800E0594u) != 0u)
        return 1;
    if (level == 3u)
        return (uint8)name_check_reels();
    if (profile_data->max_level < level)
        return 0;
    return profile_data->progress.courses[level][(uint8)column].state != 5u;
}

sint32 profile_get_grid_cell(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x800560B8u, "MAIN.EXE");
    if ((uint32)((sint16)r_u16(0x800E0582u) - 1) < 2u || level == 3u)
        return 0;
    if (r_u8(0x800E05B2u) == 0u)
        return profile_data->progress.courses[level][(uint8)column].result;
    if (profile_data->progress.courses[level][(uint8)column].result == 2u)
        return profile_data->progress.courses[level][(uint8)column].result;
    return 1;
}

sint32 profile_update_select_prims(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    uint32 records;
    uint32 row;
    uint32 availability;
    sint32 index;

    FUNCTION_MARKER(0x80056178u, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 mapped = (sint16)r_u16(0x80098D8Cu + 2u * (uint32)index);
        TEXT_RECORD *descriptor = &text_menu[mapped];
        uint8 visible = 0u;

        if ((sint16)r_u16(0x800B4244u) == index && (sint16)r_u16(0x800B4246u) < 3)
            visible = 1u;
        descriptor[0].visible = visible;
    }
    records = r_u32(0x800B6A74u);
    {
        uint8 level = r_u8(0x800B4246u);
        sint16 primitive_index = (sint16)r_u16(records + 1014u);
        uint8 column = r_u8(0x800B4244u);

        row = r_u32(records + 1008u) + 120u * (uint32)(sint32)primitive_index;
        availability = (uint8)profile_get_grid_cell(level, column, 0u, 1u);
    }
    if (availability == 1u)
    {
        w_u8(row + 58u, 0x80u);
        w_u8(row + 57u, 0x80u);
        w_u8(row + 56u, 0x80u);
        xport_update_u8(row + 59u, XPORT_MEMORY_UPDATE_OR, 1u);
        w_u8(records + 1012u, 1u);
    }
    else if (availability == 2u)
    {
        w_u8(row + 56u, 0x80u);
        w_u8(row + 58u, 0x40u);
        w_u8(row + 57u, 0x40u);
        xport_update_u8(row + 59u, XPORT_MEMORY_UPDATE_AND, 0xFEu);
        w_u8(records + 1012u, 1u);
    }
    else if (availability == 0u)
    {
        w_u8(row + 57u, 0x40u);
        w_u8(row + 56u, 0x40u);
        w_u8(row + 58u, 0x80u);
        xport_update_u8(row + 59u, XPORT_MEMORY_UPDATE_AND, 0xFEu);
        w_u8(records + 1012u, 0u);
    }
    for (index = 0; index < 10; ++index)
        text_menu[index + 11].visible = 0u;
    {
        sint16 level = (sint16)r_u16(0x800B4246u);
        if (level < 3)
        {
            sint16 column = (sint16)r_u16(0x800B4244u);
            sint16 mapped = (sint16)r_u16(0x80098D3Cu + 2u * (uint32)(6 * (sint32)level + column));
            TEXT_RECORD *descriptor = &text_menu[mapped];

            descriptor[11].visible = 1u;
            return (sint32)(descriptor + 189u);
        }
        return 2 * level;
    }
}

sint32 profile_init_menu(sint32 initialize, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 track = (sint16)r_u16(0x800B413Cu);
    uint32 table = 0x80097D94u + 24u * (uint32)(sint32)track;
    PLAYER_PROFILE *profile_data;
    sint32 initial_level;
    sint32 index;

    FUNCTION_MARKER(0x80056368u, "MAIN.EXE");
    profile_data = profile_current();
    w_u16(0x800B4244u, profile_data->course);
    initial_level = profile_data->level;
    w_u16(0x800B4246u, (uint16)initial_level);
    if (initial_level == 3)
    {
        if ((sint16)r_u16(0x800E0582u) == 2)
        {
            w_u16(0x800B4246u, 0u);
            w_u16(0x800B4244u, 0u);
            profile_data->level = (uint8)(0u);
            profile_data->course = (uint8)(0u);
        }
        if ((sint16)r_u16(0x800B4246u) == initial_level && (sint16)r_u16(0x800B4244u) == 0)
            w_u16(0x800B4244u, 1u);
    }
    if ((sint16)r_u16(0x800E0582u) == 6 && (sint16)r_u16(0x800E0584u) == 0)
        w_u16(0x800B4246u, r_u16(0x800E0592u));
    for (index = 0; index < 6; ++index)
    {
        sint32 record_index = (sint16)r_u16(0x80098D6Cu + 2u * (uint32)index);
        sint16 ordering;
        uint32 records;

        w_u8(r_u32(0x800B6A74u) + 4u + 84u * (uint32)record_index, 0u);
        ordering = (sint16)r_u16(0x80098D60u + 2u * (uint32)index);
        records = r_u32(0x800B6A74u);
        w_u16(records + 6u, (uint16)ordering);
        if ((sint16)initialize == 0)
        {
            uint16 first = r_u16(0x80098D98u + 2u * (uint32)index);
            uint32 record = records + 84u * (uint32)(sint32)ordering;
            uint16 second;

            w_u16(record + 8u, first);
            second = r_u16(0x80098DA4u + 2u * (uint32)index);
            w_u16(0x800E35A0u + 2u * (uint32)index, first);
            w_u16(record + 10u, second);
            w_u16(0x800E35B0u + 2u * (uint32)index, second);
        }
    }
    profile_sort_recs_asc(0u, 0u, 0u);
    {
        uint32 records = r_u32(0x800B6A74u);
        sint16 column = (sint16)r_u16(0x800B4244u);

        w_u32(table, (uint32)(sint32)column);
        w_u16(0x800B413Au, (uint16)column);
        sprite_set_orient(records + 588u, 1);
    }
    w_u8(r_u32(0x800B6A74u) + 669u, 12u);
    w_u8(r_u32(0x800B6A74u) + 585u, 12u);
    {
        uint8 visible = 0u;
        if ((sint16)r_u16(0x800B69CAu) == 0)
            visible = (uint8)((sint16)r_u16(0x800B4246u) != 0);
        w_u8(r_u32(0x800B6A74u) + 592u, visible);
    }
    {
        uint8 level = r_u8(0x800B4246u);
        uint8 visible = (uint8)profile_level_is_available((uint8)(level + 1u), 0u, 0u, 0u) != 0u;
        w_u8(r_u32(0x800B6A74u) + 508u, visible);
    }
    {
        uint8 level = r_u8(0x800B4246u);
        uint8 column = r_u8(0x800B4244u);
        uint8 visible = (uint8)profile_grid_cell_is_available(level, column, 0u, 0u);
        w_u8(r_u32(0x800B6A74u) + 676u, visible);
        text_menu[3].visible = visible;
    }
    w_u16(0x800B411Au, 2u);
    for (index = 0; index < 3; ++index)
    {
        sint16 level = (sint16)r_u16(0x800B4246u);
        sint32 record_index = (sint16)r_u16(0x80098D84u + 2u * (uint32)index);
        w_u8(r_u32(0x800B6A74u) + 4u + 84u * (uint32)record_index, (uint8)(level == index));
    }
    w_u8(r_u32(0x800B6A74u) + 1096u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1264u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1180u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1348u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1852u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1768u, 0u);
    w_u8(r_u32(0x800B6A74u) + 2860u, 0u);
    w_u8(r_u32(0x800B6A74u) + 2776u, 0u);
    w_u8(r_u32(0x800B6A74u) + 3028u, 0u);
    w_u8(r_u32(0x800B6A74u) + 2944u, 0u);
    profile_update_select_prims(0u, 0u, 0u, 0u);
    vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1432u, 0u);
    if ((sint16)r_u16(0x800B4246u) == 3)
    {
        TEXT_RECORD *first_menu;
        uint8 selector;
        uint16 coordinate;

        w_u8(r_u32(0x800B6A74u) + 1012u, 0u);
        first_menu = text_menu;
        w_u16(0x800B4242u, 17u);
        w_u16(0x800B4240u, 0u);
        first_menu[10].visible = 0u;
        text_menu[28].visible = 1u;
        text_menu[29].visible = 1u;
        text_menu[30].visible = 1u;
        text_menu[31].visible = 1u;
        w_u8(r_u32(0x800B6A74u) + 3952u, 1u);
        w_u8(r_u32(0x800B6A74u) + 4036u, 1u);
        text_menu[2].visible = 0u;
        w_u8(r_u32(0x800B6A74u) + 844u, 0u);
        selector = r_u8(0x800E05B7u);
        {
            TEXT_RECORD *menu = text_menu;
            coordinate = r_u16(0x80098DB0u + 2u * selector);
            {
                uint32 records = r_u32(0x800B6A74u);
                menu[3].x = (uint16)(coordinate + 15u);
                w_u16(records + 680u, coordinate);
            }
        }
        for (index = 0; index < 4; ++index)
            w_u8(r_u32(0x800B6A74u) + 3112u + 84u * (uint32)index, 0u);
        for (index = 4; index < 10; ++index)
            w_u8(r_u32(0x800B6A74u) + 3112u + 84u * (uint32)index, 1u);
        for (index = 0; index < 6; ++index)
            w_u8(r_u32(0x800B6A74u) + 4120u + 84u * (uint32)index, 0u);
        name_reset_reels();
    }
    else
    {
        uint32 descriptors = r_u32(table + 8u);
        TEXT_RECORD *state_menu;

        for (index = 0; index < 6; ++index)
            w_u16(descriptors + 8u + 20u * (uint32)index, 1u);
        text_menu[10].visible = 1u;
        state_menu = text_menu;
        w_u16(0x800B4242u, 12u);
        w_u16(0x800B4240u, 0u);
        state_menu[28].visible = 0u;
        text_menu[29].visible = 0u;
        text_menu[30].visible = 0u;
        text_menu[31].visible = 0u;
        w_u8(r_u32(0x800B6A74u) + 3952u, 0u);
        w_u8(r_u32(0x800B6A74u) + 4036u, 0u);
        text_menu[2].visible = 1u;
        w_u8(r_u32(0x800B6A74u) + 844u, 1u);
        text_menu[3].x = 227u;
        w_u16(r_u32(0x800B6A74u) + 680u, 212u);
        for (index = 0; index < 10; ++index)
            w_u8(r_u32(0x800B6A74u) + 3112u + 84u * (uint32)index, 0u);
        for (index = 0; index < 6; ++index)
        {
            sint16 level = (sint16)r_u16(0x800B4246u);
            uint32 records = r_u32(0x800B6A74u);
            uint8 selected = profile_data->progress.courses[(uint32)(sint32)level][(uint32)index].state;

            w_u8(records + 4197u + 84u * (uint32)index, 4u);
            w_u8(r_u32(0x800B6A74u) + 4120u + 84u * (uint32)index, (uint8)(selected == 6u));
        }
    }
    for (index = 0; index < 6; ++index)
        text_menu[index + 22].visible = 0u;
    w_u8(r_u32(0x800B6A74u) + 4624u, 0u);
    text_menu[32].visible = 0u;
    if ((sint16)r_u16(0x800B69CAu) != 0)
    {
        sint16 column;

        text_menu[0].visible = 0u;
        text_menu[21].visible = 1u;
        column = (sint16)r_u16(0x800B4244u);
        text_menu[column + 22].visible = 1u;
        if ((sint16)r_u16(0x800B4244u) != 0 && (sint16)r_u16(0x800E0582u) != 6)
        {
            w_u8(r_u32(0x800B6A74u) + 4624u, 1u);
            text_menu[32].visible = 1u;
        }
    }
    else
    {
        text_menu[0].visible = 1u;
        text_menu[21].visible = 0u;
    }
    return profile_populate_select_recs(1u, 0u, 0u, 0u);
}

void profile_select_course(void)
{
    PLAYER_PROFILE *profile_data = profile_current();

    FUNCTION_MARKER(0x80056C04u, "MAIN.EXE");
    profile_data->course = (uint8)(r_u8(0x800B4244u));
}

uint32 text_reset_rec_styles(void)
{
    FUNCTION_MARKER(0x80056C68u, "MAIN.EXE");
    text_set_hud_visible(1, 0);
    return text_set_hud_visible(2, 0);
}

sint32 profile_fn_80056c98(void)
{
    FUNCTION_MARKER(0x80056C98u, "MAIN.EXE");
    return 10;
}

sint32 profile_handle_select_input(CONTROLLER_STATE *input, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint32 result = r_u8(0x800B4138u);

    FUNCTION_MARKER(0x80056CA0u, "MAIN.EXE");
    if (result != 0)
    {
        result = (sint16)r_u16(0x800B4248u);
        if (result == 0)
        {
            if ((input->current & 0x20u) != 0u && (sint16)r_u16(0x800B4246u) != 3)
            {
                w_u16(0x800B6AFAu, 1u);
                w_u16(0x800B413Eu, 21u);
            }
            result = 1;
            if ((input->current & 0x80u) != 0u && (sint16)r_u16(0x800E0582u) == 1)
            {
                result = 1;
                if ((sint16)r_u16(0x800B4244u) != 0)
                {
                    w_u16(0x800B6AFAu, 1u);
                    result = 20;
                    w_u16(0x800B413Eu, 20u);
                }
            }
        }
    }
    return result;
}

sint32 profile_update_selection_vis_time(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    uint32 source;
    uint32 enabled;
    sint32 index;

    FUNCTION_MARKER(0x80056D6Cu, "MAIN.EXE");
    profile_data = profile_current();
    for (index = 0; index < 12; ++index)
        w_u8(r_u32(0x800B6A74u) + 172u + 84u * (uint32)index, 0u);
    for (index = 0; index < 6; ++index)
    {
        uint8 active = (uint8)(index == r_s16(0x800B4244u));
        uint32 offset = 32u * (uint32)index;

        text_menu[index].visible = active;
        text_menu[index + 1].visible = active;
    }
    {
        sint32 level = r_s16(0x800B4246u);

        if (level == 1)
        {
            sint32 column = r_s16(0x800B4244u);
            w_u8(r_u32(0x800B6A74u) + 676u + 84u * (uint32)column, 1u);
        }
        else if (level == 2)
        {
            sint32 column = r_s16(0x800B4244u);
            uint32 owner = r_u32(0x800B6A74u) + 168u + 84u * (uint32)column;

            w_u8(owner + 4u, 1u);
            sprite_set_orient(owner, 1);
        }
        else if (level == 0)
        {
            sint32 column = r_s16(0x800B4244u);
            w_u8(r_u32(0x800B6A74u) + 172u + 84u * (uint32)column, 1u);
        }
    }

    if (r_s16(0x800E0582u) == 4)
    {
        sint32 level = r_s16(0x800B4246u);
        selection = (sint16)(r_u16(0x800B4244u) + 6u * (uint32)level);
    }
    else
    {
        uint32 profile_level = profile_data->level;
        selection = (sint16)(r_u16(0x800B4244u) + 6u * profile_level);
    }
    w_u8(r_u32(0x800B6A74u) + 1348u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1432u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1516u, 0u);
    w_u8(r_u32(0x800B6A74u) + 1600u, 0u);
    if (r_s16(0x800B6AA0u) != 0 && r_s16(0x800E0582u) != 4)
    {
        if (selection == 0)
        {
            w_u8(r_u32(0x800B6A74u) + 1348u, 1u);
            w_u8(r_u32(0x800B6A74u) + 1600u, 1u);
        }
        else if (selection == 1)
            w_u8(r_u32(0x800B6A74u) + 1516u, 1u);
        else if (selection == 2 && r_s16(0x800E0580u) == 0 && r_u8(0x800E058Eu) == 1u)
            w_u8(r_u32(0x800B6A74u) + 1432u, 1u);
    }

    source = 0x800E0BC0u + 14u * (uint32)(sint16)selection;
    enabled = r_u16(source + 2u) != 0u || r_u16(source + 4u) != 0u || r_u16(source + 6u) != 0u;
    for (index = 0; index < 16; ++index)
    {
        uint8 selected = (uint8)(index == r_u8(source) ? enabled : 0u);
        TEXT_RECORD *menu = text_menu;

        menu[index + 24].visible = selected;
    }
    for (index = 0; index < 3; ++index)
    {
        uint8 selected = (uint8)(index == r_u8(source + 1u) ? enabled : 0u);
        TEXT_RECORD *menu = text_menu;

        menu[index + 40].visible = selected;
    }
    for (index = 0; index < 4; ++index)
        text_menu[index + 13].visible = (uint8)enabled;
    for (index = 0; index < 2; ++index)
    {
        uint32 offset = 16u * (uint32)index;

        text_menu[index + 43].visible = (uint8)(1u - enabled);
        text_menu[index + 20].visible = (uint8)enabled;
        text_menu[index + 22].visible = (uint8)enabled;
    }
    for (index = 0; index < 2; ++index)
    {
        uint32 offset = 16u * (uint32)index;

        text_menu[index + 18].visible = 1u;
        text_menu[index + 45].visible = 0u;
        text_menu[index + 22].visible = 1u;
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[20].text;
        profile_format_value_parts3(source + 2u, destination);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[21].text;
        profile_format_value_parts3(source + 8u, destination);
    }
    source = 0x800E0CBCu + 172u * (uint32)(sint16)selection;
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[22].text;
        profile_format_value_parts3(source + 2u, destination);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[23].text;
        profile_format_value_parts3(source + 50u, destination);
    }
    if (r_s16(0x800B6AA0u) != 0)
    {
        TEXT_RECORD *menu;

        w_u8(r_u32(0x800B6A74u) + 1180u, 0u);
        text_menu[47].visible = 0u;
        w_u8(r_u32(0x800B6A74u) + 1264u, 0u);
        menu = text_menu;
        menu[48].visible = 0u;
        return (sint32)menu;
    }
    return 0;
}

sint32 profile_compact_rankings(void)
{
    sint16 state = (sint16)r_u16(0x800E0580u);
    uint32 total;
    uint32 first;
    sint16 limit;
    sint16 index;

    FUNCTION_MARKER(0x8005737Cu, "MAIN.EXE");
    if (state != -1 && state != -3)
        return -3;
    total = (uint32)(uint16)vehicle_racer_count + (uint32)vehicle_leader_count + (uint32)vehicle_trailer_count;
    limit = (sint16)total;
    first = r_u8(0x800E059Du);
    if (limit > 1)
    {
        for (index = 1; index < limit; ++index)
        {
            uint32 value = r_u8(0x800E059Du + (uint32)(sint32)index);
            if (first < value)
                w_u8(0x800E059Du + (uint32)(sint32)index, (uint8)(value - 1u));
        }
    }
    w_u8(0x800E059Du, (uint8)(total - 1u));
    return (sint32)(total - 1u);
}

sint32 race_capture_time_summaries(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    uint32 profile_level;
    uint32 profile_row;
    uint32 peer;
    uint32 record;
    uint32 peer_index;
    sint32 index;
    sint32 result = -1;
    sint32 selection;

    FUNCTION_MARKER(0x80057448u, "MAIN.EXE");
    if (r_u8(0x800E058Du) != 0u)
        return result;
    result = -3;
    selection = (sint16)r_u16(0x800E0580u);
    if (selection == -1 || selection == -3)
        return result;
    result = 3;
    if ((sint16)r_u16(0x800E0582u) == 3)
        return result;
    profile_data = profile_current();
    profile_level = profile_data->level;
    profile_row = profile_data->course;
    peer = r_u32(0x800DE154u);
    record = 0x800E0BC0u + 14u * (profile_row + 6u * profile_level);
    w_u8(record, r_u8(0x800E059Du));
    w_u8(record + 1u, r_u8(0x800E0595u));
    peer_index = r_u8(peer + 1u);
    w_u16(record + 8u, (uint16)text_parse_signed_decimal(text_bind(peer + 516u)));
    w_u16(record + 10u, (uint16)text_parse_signed_decimal(text_bind(peer + 519u)));
    w_u16(record + 12u, (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 522u))));
    if (peer_index < 10u)
    {
        uint32 source = peer + 20u + 16u * peer_index;
        w_u16(record + 2u, (uint16)text_parse_signed_decimal(text_bind(source)));
        w_u16(record + 4u, (uint16)text_parse_signed_decimal(text_bind(source + 3u)));
        w_u16(record + 6u, (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u))));
    }
    for (index = 0; index < 3; ++index)
    {
        uint32 source = peer + 20u + 16u * (uint32)index;
        uint32 output = 0x800E18D4u + 6u * (uint32)index;
        w_u16(output, (uint16)text_parse_signed_decimal(text_bind(source)));
        w_u16(output + 2u, (uint16)text_parse_signed_decimal(text_bind(source + 3u)));
        result = text_parse_signed_decimal(text_bind(source + 6u));
        w_u16(output + 4u, (uint16)(10 * result));
    }
    return result;
}

uint32 profile_update_best_times(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    uint32 frame;
    uint32 result;
    sint32 mode;
    sint32 selection;
    PLAYER_PROFILE *profile_data;
    uint32 profile_level;
    uint32 profile_row;
    uint32 peer;
    uint32 record;
    uint32 source;
    uint32 candidate;
    uint32 current;
    sint32 checked;
    sint32 index;

    FUNCTION_MARKER(0x80057610u, "MAIN.EXE");
    frame = guest_stack_push(0x30u);
    result = r_u8(0x800E058Du);
    if (result != 0u)
        goto done;
    mode = (sint16)r_u16(0x800E0582u);
    selection = (sint16)r_u16(0x800E0580u);
    if (mode != 2 && (selection == -1 || selection == -3))
    {
        result = (uint32)-3;
        goto done;
    }
    result = 3u;
    if (mode == 3)
        goto done;
    profile_data = profile_current();
    profile_level = profile_data->level;
    peer = r_u32(0x800DE154u);
    profile_row = profile_data->course;
    record = 0x800E0CBCu + 172u * (profile_row + 6u * profile_level);
    if (mode == 2)
    {
        w_u16(frame + 0x10u, (uint16)text_parse_signed_decimal(text_bind(peer + 532u)));
        w_u16(frame + 0x12u, (uint16)text_parse_signed_decimal(text_bind(peer + 535u)));
        w_u16(frame + 0x14u, (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 538u))));
        candidate = time_fields_convert(frame + 0x10u);
        current = time_fields_convert(record + 16u);
        if (candidate != 0u && (candidate < current || current == 0u))
        {
            w_u16(record + 16u, r_u16(frame + 0x10u));
            w_u16(record + 18u, r_u16(frame + 0x12u));
            w_u16(record + 20u, r_u16(frame + 0x14u));
            w_u16(record + 14u, r_u8(0x800E0B40u));
            result = (uint32)name_copy_player_name_bytes(0u, text_bind(record + 22u));
            goto done;
        }
        result = current;
        goto done;
    }

    source = peer + 20u + 16u * r_u8(peer + 1u);
    w_u16(frame + 0x10u, (uint16)text_parse_signed_decimal(text_bind(source)));
    w_u16(frame + 0x12u, (uint16)text_parse_signed_decimal(text_bind(source + 3u)));
    w_u16(frame + 0x14u, (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u))));
    candidate = time_fields_convert(frame + 0x10u);
    current = time_fields_convert(record + 2u);
    if (candidate < current || current == 0u)
    {
        w_u16(record + 2u, r_u16(frame + 0x10u));
        w_u16(record + 4u, r_u16(frame + 0x12u));
        w_u16(record + 6u, r_u16(frame + 0x14u));
        w_u16(record, r_u8(0x800E0B40u));
        name_copy_player_name_bytes(0u, text_bind(record + 8u));
    }

    w_u16(frame + 0x10u, (uint16)text_parse_signed_decimal(text_bind(peer + 516u)));
    w_u16(frame + 0x12u, (uint16)text_parse_signed_decimal(text_bind(peer + 519u)));
    w_u16(frame + 0x14u, (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 522u))));
    candidate = time_fields_convert(frame + 0x10u);
    checked = 0;
    do
    {
        current = time_fields_convert(record + 50u + 6u * (uint32)checked);
        ++checked;
        if (current >= candidate || current == 0u)
            break;
    } while ((sint16)checked < 10);
    if ((sint16)checked == 10)
    {
        result = 9u;
        goto done;
    }
    index = (sint16)(checked - 1);
    for (current = 10u; (sint32)current > index; --current)
    {
        uint32 destination = record + 50u + 6u * current;
        uint32 previous = destination - 6u;
        w_u16(destination, r_u16(previous));
        w_u16(destination + 2u, r_u16(previous + 2u));
        w_u16(destination + 4u, r_u16(previous + 4u));
        w_u16(record + 28u + 2u * current, r_u16(record + 26u + 2u * current));
        guest_copy_bytes_forward(record + 116u + 5u * current, record + 111u + 5u * current, 4u);
    }
    w_u16(record + 50u + 6u * (uint32)index, r_u16(frame + 0x10u));
    w_u16(record + 52u + 6u * (uint32)index, r_u16(frame + 0x12u));
    w_u16(record + 54u + 6u * (uint32)index, r_u16(frame + 0x14u));
    w_u16(record + 28u + 2u * (uint32)index, r_u8(0x800E0B40u));
    result = (uint32)name_copy_player_name_bytes(0u, text_bind(record + 116u + 5u * (uint32)index));

done:
    guest_stack_pop(0x30u);
    return result;
}

sint32 profile_fn_800579d4(sint32 name_index, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    uint32 peer;
    uint32 source;
    uint32 record;
    uint32 first;
    uint32 second;
    uint32 third;
    uint32 candidate;
    uint32 current;

    FUNCTION_MARKER(0x800579D4u, "MAIN.EXE");
    if (r_u8(0x800E058Du) != 0u)
        return r_u8(0x800E058Du);
    if ((sint16)r_u16(0x800E0582u) != 2 && ((sint16)r_u16(0x800E0580u) == -1 || (sint16)r_u16(0x800E0580u) == -3))
        return -3;
    if ((sint16)r_u16(0x800E0582u) == 3)
        return 3;
    profile_data = profile_current();
    peer = r_u32(0x800DE154u);
    source = peer + 20u + 16u * r_u8(peer + 1u);
    record = 0x800E0CBCu + 172u * (profile_data->course + 6u * profile_data->level);
    first = (uint16)text_parse_signed_decimal(text_bind(source));
    second = (uint16)text_parse_signed_decimal(text_bind(source + 3u));
    third = (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u)));
    candidate = 60000u * first + 100u * second + third / 10u;
    current = time_fields_convert(record + 2u);
    if (candidate < current || current == 0u)
    {
        w_u16(record + 2u, (uint16)first);
        w_u16(record + 4u, (uint16)second);
        w_u16(record + 6u, (uint16)third);
        w_u16(record, r_u8(0x800E0B40u));
    }
    return name_copy_player_name_bytes((uint16)name_index, text_bind(record + 8u));
}

uint32 profile_get_active(void)
{
    uint32 mode = r_u8(0x800E058Eu);

    FUNCTION_MARKER(0x80059CD0u, "MAIN.EXE");
    if (mode == 1u)
        return 0x800E05D8u + 274u * r_u8(0x800E0595u);
    if (mode == 2u)
        return 0x800E090Eu;
    return 0x800E0A20u;
}

PROFILE_SERIES *profile_get_series(void)
{
    PLAYER_PROFILE *profile = profile_current();

    FUNCTION_MARKER(0x80059D28u, "MAIN.EXE");
    return &profile->series[profile->level];
}

sint32 profile_init_mode_rec(void)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x80059D8Cu, "MAIN.EXE");
    w_u8(0x800E058Du, 0u);
    vehicle_racer_count = (uint32)(8u);
    profile_data = profile_current();
    selection = (sint16)menu_query_group_value(3, 0);
    w_u16(0x800B69CAu, 0u);
    result = selection < 2;
    if (selection == 1)
    {
        if ((sint16)r_u16(0x800E0586u) == 1)
            profile_restore_backup();
        w_u16(0x800E0582u, 2u);
        return 2;
    }
    if (selection >= 2)
    {
        uint32 level = (uint32)(selection - 2);
        result = (sint32)level;
        if (selection < 5)
        {
            PROFILE_SERIES *series;
            sint32 index;

            w_u16(0x800E0582u, 1u);
            w_u16(0x800B69CAu, 1u);
            profile_data->level = (uint8)((uint8)level);
            profile_at(r_u8(0x800E0595u))->course = 0u;
            series = profile_get_series();
            if (series->phase != 0u)
            {
                w_u8(0x800E05ADu, (uint8)series->grid_row);
                result = (uint8)series->completed;
                profile_data->course = (uint8)((uint8)result);
            }
            else
            {
                result = r_u8(0x800B4238u);
                series->completed = (uint16)(0u);
                series->phase = (uint16)(1u);
                w_u8(0x800E05ADu, (uint8)result);
                series->grid_row = (uint16)result;
                for (index = 15; index >= 0; --index)
                    series->points[(uint32)index] = (uint8)(0u);
            }
        }
        return result;
    }
    if (selection == 0)
    {
        result = (sint16)r_u16(0x800E0586u);
        if (result == 1)
            result = profile_restore_backup();
        w_u16(0x800E0582u, 0u);
    }
    return result;
}

sint32 profile_backup_selection(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SELECTION_BACKUP *backup = &profile_selection_backups[r_u8(0x800E0595u)];
    sint32 group;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80059F38u, "MAIN.EXE");
    backup->active = 1u;
    profile_data->course = (uint8)((uint8)r_u16(0x800B4244u));
    backup->course = (uint8)r_u16(0x800B4244u);
    backup->level = profile_data->level;
    w_u8(0x800E05ADu, (uint8)r_u16(0x800B4238u));
    backup->grid_row = (uint8)r_u16(0x800B4238u);
    for (group = 0; group < 3; ++group)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                profile_selection_backups[group].progress.courses[row][column].attempts = profile_at((uint32)group)->progress.courses[row][column].attempts;
                profile_selection_backups[group].progress.courses[row][column].result = profile_at((uint32)group)->progress.courses[row][column].result;
                profile_selection_backups[group].progress.courses[row][column].state = profile_at((uint32)group)->progress.courses[row][column].state;
            }
        }
    }
    return 0;
}

sint32 profile_restore_backup(void)
{
    uint32 selected = r_u8(0x800E0595u);
    PROFILE_SELECTION_BACKUP *backup = &profile_selection_backups[selected];
    PLAYER_PROFILE *profile_data = profile_at(selected);
    sint32 group;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8005A0C4u, "MAIN.EXE");
    if (backup->active == 0u)
        return (sint32)(274u * selected);
    profile_data->course = (uint8)(backup->course);
    w_u16(0x800B4244u, backup->course);
    w_u16(0x800B6C04u, profile_data->level);
    profile_data->level = (uint8)(backup->level);
    w_u8(0x800E05ADu, backup->grid_row);
    w_u16(0x800B4238u, backup->grid_row);
    for (group = 0; group < 3; ++group)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                profile_at((uint32)group)->progress.courses[row][column].attempts = (uint8)(profile_selection_backups[group].progress.courses[row][column].attempts);
                profile_at((uint32)group)->progress.courses[row][column].result = (uint8)(profile_selection_backups[group].progress.courses[row][column].result);
                profile_at((uint32)group)->progress.courses[row][column].state = (uint8)(profile_selection_backups[group].progress.courses[row][column].state);
            }
        }
    }
    return 0;
}

sint32 profile_reset_recs(void)
{
    sint32 profile_index;
    sint32 result = 0;

    FUNCTION_MARKER(0x8005A258u, "MAIN.EXE");
    for (profile_index = 0; profile_index < 3; ++profile_index)
    {
        PLAYER_PROFILE *profile_data = profile_at((uint32)profile_index);
        if (profile_at((uint32)profile_index)->reset_pending != 0u)
        {
            sint32 group;
            sint32 index;

            profile_at((uint32)profile_index)->reset_pending = 0u;
            profile_copy_templates((sint16)profile_index);
            profile_reset_series((uint32)profile_index);
            profile_selection_backups[profile_index].active = 0u;
            profile_data->course = (uint8)(0u);
            profile_data->level = (uint8)(0u);
            profile_data->max_level = 0u;
            profile_data->reward_flags = (uint8)(0u);
            for (group = 0; group < 3; ++group)
            {
                for (index = 0; index < 3; ++index)
                    profile_data->progress.courses[(uint32)group][2u - (uint32)index].state = (uint8)(4u);
                for (index = 0; index < 3; ++index)
                    profile_data->progress.courses[(uint32)group][3u + (uint32)index].state = (uint8)(5u);
            }
            for (group = 0; group < 3; ++group)
            {
                for (index = 0; index < 6; ++index)
                {
                    profile_data->progress.courses[(uint32)group][(uint32)index].result = (uint8)(0u);
                    profile_data->progress.courses[(uint32)group][(uint32)index].attempts = (uint8)(0u);
                }
            }
            for (index = 9; index >= 0; --index)
                profile_at((uint32)profile_index)->availability[index] = 0u;
            profile_at((uint32)profile_index)->availability[0] = 1u;
            profile_at((uint32)profile_index)->availability[1] = 1u;
            profile_at((uint32)profile_index)->availability[2] = 1u;
        }
        result = profile_index + 1 < 3;
    }
    return result;
}

sint32 profile_advance_menu_select(void)
{
    uint32 frame;
    sint32 result;

    FUNCTION_MARKER(0x8005A3ECu, "MAIN.EXE");
    frame = guest_stack_push(0x28u);
    w_u8(0x800E058Eu, 1u);
    if ((sint16)r_u16(0x800B4142u) == 1)
    {
        w_u16(0x800E0582u, 0u);
        w_u16(0x800E0586u, 0u);
    }
    else if ((sint16)r_u16(0x800E0582u) == 1)
    {
        menu_update_group_desc_select(3, 0, (sint16)(uint16)(r_u16(0x800B6C04u) + 2u));
    }
    {
        CONTROLLER_STATE no_buttons = {0};
        no_buttons.current = r_u16(frame + 0x10u);
        profile_update_menu_trans(&no_buttons);
    }
    result = profile_reset_recs();
    guest_stack_pop(0x28u);
    return result;
}

sint32 profile_update_menu_trans(CONTROLLER_STATE *input)
{
    sint16 count;
    sint16 selected;
    sint16 next;
    sint16 record_index = 0;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8005A480u, "MAIN.EXE");
    if (r_u8(0x800B4138u) != 0u && (input->current & 0x40u) != 0u)
    {
        w_u16(0x800B6AFAu, 1u);
        w_u16(0x800B413Eu, 25u);
        ranking_noop();
        sprite_clear_anim_recs();
        profile_update_trans();
    }
    count = (sint16)menu_query_group_value(3, 0);
    selected = (sint16)menu_query_group_value(3, 1);
    next = (sint16)(profile_at((uint32)selected)->max_level + 1u);
    w_u8(0x800E0595u, (uint8)selected);
    menu_set_group_desc_value(3, 0, next);
    if (next < count)
    {
        menu_update_group_desc_select(3, 0, next);
        count = next;
    }
    w_u16(0x800B6C04u, (uint16)(count - 2));
    if (count == 2)
        record_index = 10;
    else if (count == 3)
        record_index = 11;
    else if (count == 4)
        record_index = 12;
    w_u8(r_u32(0x800B6A74u) + 4u, 1u);
    for (index = 10; index < 13; ++index)
        w_u8(r_u32(0x800B6A74u) + 4u + 84u * (uint32)index, 0u);
    if (record_index != 0)
    {
        uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
        w_u8(record + 4u, 1u);
        if ((sint16)r_u16(0x800B413Au) != 0)
            sprite_apply_prim_vram_mask(record);
        else
            sprite_disable_semitransparency(record);
    }
    else
    {
        w_u8(r_u32(0x800B6A74u) + 340u, 0u);
        w_u8(r_u32(0x800B6A74u) + 676u, 0u);
        w_u8(r_u32(0x800B6A74u) + 760u, 0u);
    }
    for (index = 0; index < 3; ++index)
    {
        text_menu[index + 3].visible = 0;
        result = 1;
    }
    return result;
}

sint32 profile_dispatch_menu_state(void)
{
    sint32 state;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x8005A714u, "MAIN.EXE");
    result = r_u8(0x800E058Au);
    if (result == 0)
        return result;

    state = (sint16)r_u16(0x800E0582u);
    if (state == 1)
    {
        uint32 first = (uint16)vehicle_racer_count;
        uint32 second = vehicle_leader_count;
        uint32 third = vehicle_trailer_count;

        w_u16(0x800E1BAEu, (uint16)(first + second + third));
        return profile_update_row_completion();
    }
    if (state >= 2)
    {
        if (state == 3)
            return vehicle_select_fn_8005e624();
        if (state == 4)
        {
            w_u8(0x800E058Bu, 1u);
            w_u16(0x800E0582u, 0u);
            w_u8(0x800E058Cu, 0u);
            w_u16(0x800E0580u, (uint16)-1);
            return -1;
        }
        return 1;
    }
    if (state != 0)
        return -3;
    selection = (sint16)r_u16(0x800E0580u);
    if (selection == -3)
    {
        w_u8(0x800E058Bu, 48u);
        w_u8(0x800E058Cu, 0u);
        return 48;
    }
    if (selection == -4)
        return profile_confirm_row_select();
    if (selection < -2)
        return -4;
    if (selection == -2)
        return profile_fn_8005aca8();
    if (selection == -1)
    {
        if (name_player_name_matches(0, 0x800B425Cu) == 0)
            return profile_refresh_menu_ack();
        return profile_confirm_row_select();
    }
    return -1;
}

sint32 profile_update_row_completion(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    uint32 row = profile_data->level;
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005A8A0u, "MAIN.EXE");
    profile_data->progress.courses[row][column].state = (uint8)(6u);
    w_u16(0x800B6ABEu, 3u);
    w_u16(0x800B4264u, (uint16)(column + 1u));
    if (row == 2u)
        w_u16(0x800B4264u, (uint16)(column + 7u));
    w_u8(0x800E058Bu, 19u);
    w_u8(0x800E058Cu, 0u);
    profile_data->course = (uint8)((uint8)(column + 1u));
    series->completed = (uint16)(series->completed + 1u);
    w_u16(0x800B4244u, profile_data->course);
    if (profile_data->course != 6u)
        return 6;
    if ((uint16)ranking_player_is_leading() == 0u)
        return profile_apply_action(12);
    profile_data->reward_flags |= 0x80u;
    if (row == 0u)
        return profile_apply_action(4);
    if (row == 1u)
        return profile_apply_action(5);
    if (row == 2u)
        return profile_apply_action(6);
    return 0;
}

sint32 profile_row_is_complete(sint16 row)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 column;

    FUNCTION_MARKER(0x8005A9F8u, "MAIN.EXE");
    for (column = 0; column < 6; ++column)
    {
        if (profile_data->progress.courses[(uint32)(sint32)row][(uint32)column].state != 6u)
            return 0;
    }
    return 1;
}

sint32 profile_confirm_row_select(void)
{
    uint32 profile = profile_get_active();
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 column = profile_data->course;
    uint32 row = profile_data->level;
    PROFILE_COURSE *row_data;
    PROFILE_COURSE *cell;
    sint32 result;

    FUNCTION_MARKER(0x8005AA88u, "MAIN.EXE");
    profile_data->level_result = 0u;
    w_u8(0x800E058Bu, 11u);
    w_u8(0x800B6C24u, 0u);
    if (row == 3u)
    {
        profile_data->progress.courses[3u][column].attempts = (uint8)(0u);
        return (sint32)(profile + 24u + column);
    }
    row_data = profile_data->progress.courses[row];
    cell = &row_data[column];
    if (cell->state == 4u)
    {
        cell->state = 6u;
        w_u16(0x800B69E0u, 1u);
        if (row_data[3].state == 5u)
            row_data[3].state = 4u;
        else if (row_data[4].state == 5u)
            row_data[4].state = 4u;
        else if (row_data[5].state == 5u)
            row_data[5].state = 4u;
    }
    if (profile_row_is_complete((sint16)row) != 0)
    {
        if (profile_data->level <= 2u)
            profile_apply_action((sint16)(profile_data->level + 1u));
        if (profile_data->level == profile_data->max_level)
        {
            profile_data->course = (uint8)(0u);
            profile_data->level_result = 1u;
            profile_data->level = (uint8)((uint8)(profile_data->level + 1u));
        }
        if (profile_data->max_level < profile_data->level)
            profile_data->max_level = profile_data->level;
    }
    result = (sint32)(2u * row);
    if (r_u8(0x800E05B8u) == 5u)
    {
        w_u8(0x800E05B8u, 0u);
        cell = &row_data[column];
        result = 2;
        if (cell->result != 2u)
        {
            profile_data->course = (uint8)((uint8)column);
            profile_data->level = (uint8)((uint8)row);
            cell->result = 1u;
            result = profile_index_map();
            row_data[column].attempts = (uint8)result;
            w_u8(0x800B6C24u, 1u);
        }
    }
    return result;
}

sint32 profile_fn_8005aca8(void)
{
    FUNCTION_MARKER(0x8005ACA8u, "MAIN.EXE");
    w_u8(0x800E058Bu, 17u);
    return 17;
}

sint32 profile_refresh_menu_ack(void)
{
    uint32 state;

    FUNCTION_MARKER(0x8005ACBCu, "MAIN.EXE");

    state = r_u8(0x800E058Du);
    w_u8(0x800E058Bu, 10u);
    if (state == 0u)
        w_u8(0x800E058Cu, 0u);
    else if (state == 1u)
        w_u8(0x800E058Cu, 1u);
    return 1;
}

sint32 profile_fn_8005ad20(void)
{
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    uint32 records = r_u32(0x800B6A74u);

    FUNCTION_MARKER(0x8005AD20u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B6B0Cu) != 0)
    {
        menu[0].visible = 0u;
        menu[29].visible = 0u;
        menu[1].visible = 0u;
        menu[2].visible = 0u;
        w_u8(records + 4036u, 0u);
        w_u8(records + 4120u, 0u);
        w_u8(records + 4204u, 0u);
        w_u8(records + 4288u, 1u);
        w_u8(records + 4372u, 1u);
        menu[30].visible = 1u;
        menu[31].visible = 1u;
        menu[32].visible = 1u;
        return (sint32)menu;
    }
    menu[30].visible = 0u;
    menu[31].visible = 0u;
    menu[32].visible = 0u;
    menu[0].visible = 1u;
    w_u8(records + 4036u, 1u);
    w_u8(records + 4204u, 1u);
    w_u8(records + 4288u, 0u);
    if (series->phase == 1u)
    {
        menu[29].visible = 1u;
        menu[1].visible = 0u;
        menu[2].visible = 0u;
        w_u8(records + 4120u, 0u);
    }
    else
    {
        menu[29].visible = 0u;
        menu[1].visible = 1u;
        menu[2].visible = 1u;
        w_u8(records + 4120u, 1u);
    }
    w_u8(records + 4288u, 0u);
    w_u8(records + 4372u, 0u);
    if ((uint32)series->phase - 1u >= 2u)
    {
        sint16 position = r_u8(0x800E05B7u) == 1u ? 262 : 242;
        w_u16(records + 4208u, (uint16)position);
        return (sint32)records;
    }
    w_u16(records + 4208u, 136u);
    return 136;
}

sint32 profile_fn_8005afa0(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *menu = text_menu;
    char *text;
    sint32 index;
    sint32 value;

    FUNCTION_MARKER(0x8005AFA0u, "MAIN.EXE");
    w_u16(0x800B4244u, series->completed);
    w_u8(0x800E05ADu, (uint8)series->grid_row);
    w_u16(0x800B4238u, r_u8(0x800E05ADu));
    for (index = 0; index < 6; ++index)
    {
        uint8 selected = index == (sint16)r_u16(0x800B4244u);
        menu[(2 * (index)) + 4].visible = selected;
        menu[(2 * (index)) + 5].visible = selected;
        w_u8(records + 3280u + 84u * (uint32)index, selected);
    }
    for (index = 0; index < 9; ++index)
    {
        uint8 selected = index == (sint16)r_u16(0x800B4238u);
        w_u8(records + 2524u + 84u * (uint32)index, selected);
        menu[index + 16].visible = selected;
    }
    for (index = 0; index < 3; ++index)
        w_u8(records + 3784u + 84u * (uint32)index, index == profile_data->level);
    if (series->phase == 2u)
    {
        w_u16(records + 4208u, 136u);
        w_u8(records + 4120u, 0u);
    }
    else
    {
        w_u16(records + 4208u, r_u8(0x800E05B7u) == 1u ? 262u : 242u);
        w_u8(records + 4120u, 1u);
    }
    profile_build_vehicle_grid(1, (sint16)r_u16(0x800B4238u), 388, 94);
    ranking_build_order();
    value = ranking_find_empty_slot() + 1;
    text = menu[26].text;
    if (series->completed != 0u)
    {
        text[0] = (uint8)(value / 10 + 48);
        text[1u] = (uint8)(value % 10 + 48);
        text[2u] = 32u;
        text[3u] = 32u;
    }
    else
    {
        for (index = 0; index < 4; ++index)
            text[(uint32)index] = 46u;
    }
    text = menu[28].text;
    if (series->completed == 0u)
    {
        for (index = 0; index < 4; ++index)
            text[(uint32)index] = 46u;
    }
    else if (series->points[0] == 0u)
    {
        text[0] = 48u;
        text[1u] = 48u;
        text[2u] = 32u;
        text[3u] = 32u;
    }
    else
    {
        value = series->points[0];
        text[0] = (uint8)(value / 10 + 48);
        text[1u] = (uint8)(value % 10 + 48);
        text[2u] = 32u;
        text[3u] = 32u;
    }
    return profile_fn_8005ad20();
}

sint32 profile_fn_8005b350(CONTROLLER_STATE *input)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    sint32 index;

    FUNCTION_MARKER(0x8005B350u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B6B0Cu) == 0 && r_u8(0x800B4138u) != 0u)
    {
        if ((input->current & 0x10u) != 0u)
        {
            w_u16(0x800B6AFAu, 2u);
            w_u16(0x800B413Eu, 3u);
            profile_restore_backup();
        }
        if ((input->current & 0x40u) != 0u)
        {
            if (series->phase == 1u)
                series->phase = (uint16)(2u);
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 10u);
        }
    }
    if ((uint32)series->phase - 2u < 2u && r_u8(0x800B4138u) != 0u)
    {
        if ((input->current & 0x80u) != 0u)
        {
            if ((sint16)r_u16(0x800B6B0Cu) == 0 && r_u8(r_u32(0x800B6A74u) + 4120u) == 1u && (input->current & 0x40u) == 0u)
            {
                w_u16(0x800B6AFAu, 4u);
                w_u16(0x800B6B0Cu, 1u);
            }
            else if ((sint16)r_u16(0x800B6B0Cu) != 0)
            {
                w_u16(0x800B6AFAu, 1u);
                w_u16(0x800B6B0Cu, 0u);
            }
        }
        if ((sint16)r_u16(0x800B6B0Cu) != 0 && (input->current & 0x20u) != 0u)
        {
            w_u16(0x800B6B0Cu, 0u);
            series->phase = (uint16)(1u);
            w_u16(0x800B413Eu, 5u);
            w_u16(0x800B6AFAu, 1u);
            series->completed = (uint16)(0u);
            profile_data->course = (uint8)(0u);
            for (index = 0; index < 16; ++index)
                series->points[(uint32)index] = (uint8)(0u);
            w_u16(0x800B4140u, (uint16)-1);
        }
    }
    return profile_fn_8005ad20();
}

sint32 profile_fn_8005b570(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    sint32 count;
    sint32 index;

    FUNCTION_MARKER(0x8005B570u, "MAIN.EXE");
    if (series->phase == 2u)
    {
        series->completed = (uint16)(0u);
        profile_data->course = (uint8)(0u);
        series->phase = (uint16)(3u);
        for (index = 0; index < 16; ++index)
            series->points[(uint32)index] = (uint8)(0u);
    }
    if ((sint16)r_u16(0x800B413Eu) != 10)
        return 10;
    count = (sint16)series->completed;
    for (index = 0; index < count; ++index)
        profile_data->progress.courses[profile_data->level][(uint32)index].state = (uint8)(6u);
    for (index = count; index < 6; ++index)
        profile_data->progress.courses[profile_data->level][(uint32)index].state = (uint8)(4u);
    return index;
}

sint32 profile_restore_selection(sint16 selection)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8005BE50u, "MAIN.EXE");
    menu_cleanup_tex();
    profile_data = profile_current();
    profile_get_series();
    w_u16(0x800B4096u, UINT16_C(0xFFFF));
    w_u16(0x800B4244u, profile_data->course);
    return selection;
}

sint32 profile_complete_selection(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005D804u, "MAIN.EXE");
    profile_data->progress.courses[profile_data->level][column].state = (uint8)(6u);
    w_u16(0x800B6ABEu, 3u);
    w_u16(0x800B4264u, (uint16)(column + 1u));
    if (profile_data->level == 2u)
        w_u16(0x800B4264u, (uint16)(column + 7u));
    w_u8(0x800E058Bu, 44u);
    profile_data->course = (uint8)((uint8)(column + 1u));
    series->completed = (uint16)(series->completed + 1u);
    w_u16(0x800B4244u, profile_data->course);
    if (profile_data->course == 6u)
        return profile_apply_action(10);
    return 6;
}

sint32 profile_apply_action(sint16 action)
{
    uint32 state = profile_get_active();
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 result = (sint32)state;

    FUNCTION_MARKER(0x800637F0u, "MAIN.EXE");
    if (action == 3)
    {
        if ((sint16)r_u16(0x800B69E0u) != 0)
            w_u16(0x800B6BF0u, 5u);
        profile_data->reward_flags |= 0x88u;
        action = r_u8(0x800E0595u) == 0u ? 7 : (r_u8(0x800E0595u) == 1u ? 8 : 9);
    }
    switch (action)
    {
        case 1:
            profile_data->reward_flags |= 2u;
            return profile_data->reward_flags;
        case 2:
            profile_data->reward_flags |= 4u;
            return profile_data->reward_flags;
        case 4:
            profile_data->reward_flags |= 0x10u;
            w_u16(0x800B6BF0u, 2u);
            w_u32(0x800B6AE4u, 1u);
            return 1;
        case 5:
            profile_data->reward_flags |= 0x20u;
            w_u16(0x800B6BF0u, 3u);
            w_u32(0x800B6AE4u, 2u);
            return 2;
        case 6:
            profile_data->reward_flags |= 0x40u;
            w_u16(0x800B6BF0u, 4u);
            w_u32(0x800B6AE4u, 3u);
            return 3;
        case 7:
            profile_data->reward_flags |= 1u;
            return profile_data->reward_flags;
        case 8:
        case 9:
            profile_data->reward_flags |= 1u;
            result = 1;
            if (profile_unlock(7) != 0)
            {
                w_u32(0x800B6A58u, 1u);
                w_u16(0x800B4264u, 7u);
                w_u16(0x800B4266u, 7u);
                w_u16(0x800B6ABEu, 2u);
                w_u8(0x800E058Bu, 14u);
                result = 14;
            }
            return result;
        case 12:
            result = (profile_data->level_result & 7u) + 6;
            w_u16(0x800B6BF0u, (uint16)result);
            return result;
        default:
            return result;
    }
}

sint32 profile_reset_menu_state(void)
{
    FUNCTION_MARKER(0x80063A4Cu, "MAIN.EXE");
    w_u16(0x800E0582u, 0u);
    w_u8(0x800E058Bu, 8u);
    w_u8(0x800E058Cu, 0u);
    return 8;
}

sint32 profile_dispatch_mode_result_handler(uint32 first, uint32 second, uint32 third, uint32 fourth)
{
    sint16 mode;

    FUNCTION_MARKER(0x80063A70u, "MAIN.EXE");
    race_capture_time_summaries(first, second, third, fourth);
    mode = (sint16)r_u16(0x800E0584u);
    if (mode == 0)
    {
        profile_update_best_times(first, second, third, fourth);
        return tournament_record_champ_result();
    }
    if (mode == 1)
        return tournament_record_result(first, second, third, fourth);
    if (mode == 2)
        return tournament_record_match_result();
    return 2;
}

sint32 menu_save_select_state(void)
{
    sint32 first = (sint16)r_u16(0x800B6AD6u);
    uint16 second = r_u16(0x800B6BF2u);
    uint16 third = r_u16(0x800B6B34u);

    FUNCTION_MARKER(0x80063B00u, "MAIN.EXE");
    w_u8(0x800E058Bu, 1u);
    w_u16(0x800E0582u, 0u);
    w_u16(0x800E0580u, (uint16)-1);
    vehicle_racer_count = (uint32)((uint32)first);
    vehicle_leader_count = (uint16)(second);
    vehicle_trailer_count = (uint16)(third);
    return -1;
}

sint32 profile_process_select_results(void)
{
    PLAYER_PROFILE *profile_data = profile_current();

    FUNCTION_MARKER(0x80063B4Cu, "MAIN.EXE");
    profile_compact_rankings();
    if (profile_data->level < 3u)
    {
        profile_update_best_times(0u, 0u, 0u, 0u);
        race_capture_time_summaries(0u, 0u, 0u, 0u);
    }
    return profile_dispatch_menu_state();
}

sint32 results_handle_2p(void)
{
    FUNCTION_MARKER(0x80063BA4u, "MAIN.EXE");
    return results_2p_complete_route();
}

sint32 profile_commit_selection(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 initial_state;
    sint16 state;
    sint32 gate;

    FUNCTION_MARKER(0x80063D44u, "MAIN.EXE");
    menu_save_desc_payloads((sint16)r_u16(0x800B413Cu));
    profile_data = profile_current();
    profile_data->level_result = 0u;
    profile_data->course = (uint8)(r_u8(0x800B4244u));
    w_u8(0x800E058Bu, 10u);
    initial_state = (sint16)r_u16(0x800E0582u);
    w_u8(0x800E058Cu, r_u8(0x800B4244u));
    if (initial_state == 4)
    {
        uint16 first = (uint16)vehicle_racer_count;
        uint16 second = vehicle_leader_count;
        uint16 third = vehicle_trailer_count;

        w_u16(0x800B6AD6u, first);
        w_u16(0x800B6BF2u, second);
        w_u16(0x800B6B34u, third);
    }
    gate = (sint16)r_u16(0x800B4240u);
    w_u8(0x800E05B3u, 0u);
    if (gate != 0)
    {
        w_u16(0x800E0586u, (uint16)initial_state);
        if ((sint16)r_u16(0x800B4242u) == 12)
        {
            sint32 row = (sint16)r_u16(0x800B4246u);
            uint8 *value;

            w_u16(0x800E0582u, 3u);
            value = &profile_data->progress.courses[(uint32)row][(uint32)(sint16)r_u16(0x800B4244u)].attempts;
            if (*value != 0u)
                *value = (uint8)(*value - 1u);
        }
    }
    if ((sint16)r_u16(0x800B4246u) == 3)
    {
        uint16 selection;
        uint16 saved;

        w_u16(0x800E1B9Cu, 1u);
        selection = r_u16(0x800B4244u);
        saved = r_u16(0x800E1B86u);
        w_u16(0x800E1BA4u, (uint16)(selection - 1u));
        w_u16(0x800B6BF6u, saved);
    }
    w_u16(0x800B6AA0u, 5u);
    text_set_hud_visible(0, 1);
    state = (sint16)r_u16(0x800E0582u);
    if (state != 3 && (sint16)r_u16(0x800E1B9Cu) == 0)
    {
        TEXT_RECORD *record;
        sint32 first = (sint16)r_u16(0x800B4244u);
        sint32 second = (sint16)r_u16(0x800B4246u);

        w_u16(0x800B413Eu, 21u);
        menu_increment_group_offset(first, second);
        record = text_hud;
        record[0].x = 332u;
        record[0].y = 145u;
    }
    if (state != 6 && r_u8(0x800E058Eu) == 2u)
    {
        name_copy_player_name_bytes(0u, text_bind(0x80083994u));
        return name_copy_player_name_bytes(1u, text_bind(0x800839A4u));
    }
    return 2;
}

sint32 profile_update_limits(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint16 maximum = 0;
    sint16 value;
    sint32 index;

    FUNCTION_MARKER(0x80063F68u, "MAIN.EXE");
    menu_set_group_desc_value(3, 0, (sint16)profile_data->max_level + 1);
    for (index = 0; index < 3; ++index)
    {
        value = profile_at((uint32)index)->max_level;
        if (value > maximum)
            maximum = value;
    }
    if (maximum >= 3)
        maximum = 2;
    menu_set_group_desc_value(4, 0, maximum + 1);
    value = profile_at((uint32)(sint32)(sint16)r_u16(0x800E1B84u))->max_level;
    if (value >= 3)
        value = 2;
    return menu_set_group_desc_value(35, 0, value);
}

sint32 profile_unlock(sint16 index)
{
    PLAYER_PROFILE *profile = profile_current();

    FUNCTION_MARKER(0x80064064u, "MAIN.EXE");
    // Fourth-column completion unlocks the final reserved course state
    if (index == -1)
    {
        uint8 *tail = &profile->progress.courses[3][5].state;

        if (*tail != 0u)
            return 0;
        *tail = 1u;
        return 1;
    }
    if (profile->availability[index] == 0u)
    {
        profile->availability[index] = 1u;
        return 1;
    }
    return 0;
}

sint32 profile_generate_opponents(void)
{
    sint32 record_index;

    FUNCTION_MARKER(0x8006414Cu, "MAIN.EXE");
    game_random_seed(0u);
    for (record_index = 0; record_index < 18; ++record_index)
    {
        uint32 record = 0x800E0CBCu + 172u * (uint32)record_index;
        sint32 base_time = (sint32)r_u32(0x800995F8u + 8u * (uint32)record_index);
        sint32 generated[10];
        sint32 minutes = base_time / 6000;
        sint32 seconds = (base_time % 6000) / 100;
        sint32 remainder = (base_time - 6000 * minutes - 100 * seconds) / 2;
        sint32 random_value;
        sint32 value;
        sint32 index;

        w_u16(record + 2u, (uint16)minutes);
        w_u16(record + 4u, (uint16)seconds);
        w_u16(record + 6u, (uint16)(20 * remainder));
        random_value = game_random_next();
        value = 3 * base_time + 194 + random_value % 500;
        for (index = 0; index < 10; ++index)
        {
            generated[index] = value;
            random_value = game_random_next();
            value += 20 + random_value % 105;
        }
        random_value = game_random_next();
        value = base_time - 44 - random_value % 120;
        minutes = value / 6000;
        seconds = (value % 6000) / 100;
        w_u16(record + 16u, (uint16)minutes);
        w_u16(record + 18u, (uint16)seconds);
        w_u16(record + 20u, (uint16)(20 * ((value - 6000 * minutes - 100 * seconds) / 2)));
        for (index = 0; index < 5; ++index)
        {
            w_u8(record + 8u + (uint32)index, r_u8(0x800991C6u + 60u * (uint32)record_index + (uint32)index));
            w_u8(record + 22u + (uint32)index, r_u8(0x800991C0u + 60u * (uint32)record_index + (uint32)index));
        }
        for (index = 0; index < 10; ++index)
        {
            sint32 generated_value = generated[index];
            sint32 generated_minutes = generated_value / 6000;
            sint32 generated_seconds = (generated_value % 6000) / 100;
            sint32 character;
            w_u16(record + 50u + 6u * (uint32)index, (uint16)generated_minutes);
            w_u16(record + 52u + 6u * (uint32)index, (uint16)generated_seconds);
            w_u16(record + 54u + 6u * (uint32)index, (uint16)(20 * ((generated_value - 6000 * generated_minutes - 100 * generated_seconds) / 2)));
            for (character = 0; character < 5; ++character)
                w_u8(record + 116u + 5u * (uint32)index + (uint32)character, r_u8(0x800991C0u + 60u * (uint32)record_index + 6u * (uint32)index + (uint32)character));
        }
    }
    return 0;
}

sint32 profile_merge_unlocks(PLAYER_PROFILE *profile)
{
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80064584u, "MAIN.EXE");
    for (column = 0; column < 10; ++column)
        profile->availability[column] = 0u;
    for (row = 0; row < 3; ++row)
    {
        PLAYER_PROFILE *source = profile_at((uint32)row);
        for (column = 0; column < 10; ++column)
        {
            if (source->availability[column] == 1u)
                profile->availability[column] = 1u;
        }
    }
    return 0;
}

sint32 profile_init_ranking_grid(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8006464Cu, "MAIN.EXE");
    profile_data->course = (uint8)(0u);
    profile_data->level = (uint8)(0u);
    profile_data->reward_flags = (uint8)(0u);
    profile_data->max_level = profile_at(r_u16(0x800E1B84u))->max_level;
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(5u);
    }
    if (profile_data->max_level != 0xFFu)
    {
        for (row = 0; row < (sint32)profile_data->max_level + 1; ++row)
        {
            for (column = 0; column < 6; ++column)
                profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(4u);
        }
    }
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[(uint32)row][(uint32)column].result = (uint8)(0u);
    }
    return profile_merge_unlocks(profile_current());
}
