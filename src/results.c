#include "camera.h"
#include "vehicle.h"
#include "arena.h"
#include "mesh.h"
#include "name.h"
#include "pickup.h"
#include "render.h"
#include "replay.h"
#include "sound.h"
#include "text.h"
#include "profile.h"
#include "results.h"
#include "global.h"
#include "menu.h"
#include "timer.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 results_layout_populate(uint32 state)
{
    uint32 source;
    uint32 ui;
    uint32 record;
    uint32 text;
    uint32 first;
    uint32 second;
    uint32 third;
    sint32 result;
    sint32 index;
    sint32 alternate;
    sint32 position;
    sint32 language_is_six;
    sint32 total;

    FUNCTION_MARKER(0x8001FA38u, "MAIN.EXE");
    source = r_u32(state + 100u);
    menu_select_result_clut(state);
    if (r_u32(0x80083484u) == 9u)
    {
        menu_update_result_rec_palettes(state);
        ui = r_u32(0x800B69F4u);
        result = time_copy_chars7(text_bind(0x800D6B48u), text_bind(r_u32(ui + 24u)));
        w_u8(0x80083908u, 0u);
        return result;
    }

    ui = r_u32(0x800B69F4u);
    w_u8(ui + 652u, 0u);
    ui = r_u32(0x800B69F4u);
    record = ui;
    for (index = 0; index < 8; ++index)
    {
        w_u8(record + 12u, 1u);
        record += 40u;
    }
    ui = r_u32(0x800B69F4u);
    record = ui + 680u;
    for (index = 0; index < 4; ++index)
    {
        w_u8(record + 12u, 0u);
        record += 40u;
    }
    language_is_six = (sint16)r_u16(0x800E0582u) == 6;
    ui = r_u32(0x800B69F4u);
    w_u8(ui + 452u, (uint8)language_is_six);

    alternate = (sint32)r_u32(0x800B69ECu) > 0 && vehicle_menu(source)->mode == 4u;
    text = alternate ? 0x800D6948u : 0x800D6B48u;
    if ((sint32)r_u32(0x800DCFD4u) > 0 && !alternate)
    {
        ui = r_u32(0x800B69F4u);
        w_u16(ui + 28u, 27508u);
    }
    else
    {
        sint32 row = (sint32)r_u32(text + 12u) / 500;
        uint16 clut;

        if ((sint16)row >= 10)
            row = 9;
        clut = getClut(832, 449 - (sint16)row);
        ui = r_u32(0x800B69F4u);
        w_u16(ui + 28u, clut);
    }

    alternate = (sint32)r_u32(0x800B69ECu) > 0 && vehicle_menu(source)->mode == 4u;
    ui = r_u32(0x800B69F4u);
    w_u8(ui + 332u, (uint8)alternate);
    ui = r_u32(0x800B69F4u);
    w_u8(ui + 372u, (uint8)alternate);
    ui = r_u32(0x800B69F4u);
    text = alternate ? 0x800D6948u : 0x800D6B48u;
    position = alternate ? (sint32)((r_u32(0x800B69ECu) >> 3) & 1u) : 1;
    w_u8(ui + 12u, (uint8)position);
    time_copy_chars7(text_bind(text), text_bind(r_u32(ui + 24u)));
    w_u8(0x80083908u, 0u);

    position = alternate;
    if (!alternate)
    {
        position = 1;
        if ((sint16)r_u16(state + 76u) == 2)
            position = vehicle_menu(source)->mode != 4u;
    }
    ui = r_u32(0x800B69F4u);
    w_u8(ui + 412u, (uint8)(1 - position));

    position = r_u8(source);
    ui = r_u32(0x800B69F4u);
    record = ui + 120u;
    for (index = 0; index < 4; ++index)
    {
        w_u8(record + 12u, position >= index);
        if (position - index >= 0)
        {
            uint32 name = source + 20u + (uint32)index * 16u;

            if (r_u8(name) == '0')
                time_copy_chars7(text_bind(name), text_bind(r_u32(record + 24u)));
            else
                name_bytes_copy8(text_bind(name), text_bind(r_u32(record + 24u)));
            w_u16(record + 28u, r_u16(0x800B3E28u + (uint32)(position - index) * 2u));
        }
        record += 40u;
    }

    if (r_u32(0x80083484u) == 8u)
    {
        ui = r_u32(0x800B69F4u);
        record = ui;
        for (index = 0; index < 17; ++index)
        {
            w_u8(record + 12u, 0u);
            record += 40u;
        }
        ui = r_u32(0x800B69F4u);
        text = r_u32(ui + 704u);
        w_u8(ui + 692u, 1u);
        time_copy_chars7(text_bind(0x800D6B48u), text_bind(text));
        ui = r_u32(0x800B69F4u);
        record = ui + 720u;
        for (index = 0; index < 3; ++index)
        {
            sint16 value = (sint16)r_u16(0x800B69FCu + (uint32)index * 2u);

            if (value == -1)
                w_u8(record + 12u, 0u);
            else
            {
                w_u8(record + 12u, 1u);
                value = (sint16)r_u16(0x800B69FCu + (uint32)index * 2u);
                text_format_decimal_digits(value, text_bind(r_u32(record + 24u)), 2);
            }
            record += 40u;
        }
        return render_activate_recs(r_u32(0x800B69A0u));
    }

    first = (uint16)vehicle_racer_count;
    second = vehicle_leader_count;
    third = vehicle_trailer_count;
    w_u8(record + 12u, (uint8)language_is_six);
    w_u8(0x80083BD4u, (uint8)language_is_six);
    alternate = vehicle_menu(source)->racer_num;
    total = (sint32)(first + second);
    ui = r_u32(0x800B69F4u);
    text = r_u32(ui + 64u);
    total = (sint16)(total + (sint32)third);
    text_format_decimal_digits(alternate, text_bind(text), 2);
    w_u8(text + 2u, '/');
    text_format_decimal_digits(total, text_bind(text + 3u), 2);
    position = r_u8(source);
    position = (sint16)(position + (sint32)r_u16(0x800B699Cu) + 1);
    if ((sint16)position >= 100)
        position = 99;
    {
        uint32 course = r_u32(0x8008349Cu);
        sint16 course_total;

        ui = r_u32(0x800B69F4u);
        course_total = (sint16)r_u16(course + 28u);
        alternate = (sint16)position < 10;
        text = r_u32(ui + 104u);

        if (alternate)
        {
            text_format_decimal_digits(position, text_bind(text), 1);
            w_u8(text + 1u, '/');
            text_format_decimal_digits(course_total, text_bind(text + 2u), 1);
        }
        else
        {
            text_format_decimal_digits(position, text_bind(text), 2);
            w_u8(text + 2u, 0u);
        }
    }
    ui = r_u32(0x800B69F4u);
    alternate = r_u8(source + 516u);
    text = r_u32(ui + 304u);
    if (alternate == '0')
        time_copy_chars7(text_bind(source + 516u), text_bind(text));
    else
        name_bytes_copy8(text_bind(source + 516u), text_bind(text));
    menu_update_result_rec_palettes(state);

    alternate = r_u32(0x80083484u) == 4u;
    if (alternate)
        w_u8(0x800839F4u, 0u);
    w_u8(0x80083A1Cu, (uint8)(1 - alternate));
    w_u8(0x80083B0Cu, (uint8)(1 - alternate));
    w_u8(0x80083BFCu, (uint8)alternate);
    w_u8(0x80083BD4u, (uint8)alternate);
    w_u8(0x80083C4Cu, (uint8)alternate);
    w_u8(0x80083C24u, (uint8)alternate);
    if (alternate)
    {
        if ((sint16)position >= 10)
            w_u8(0x8008390Eu, 0u);
        else
            w_u8(0x8008390Du, 0u);
        w_u8(0x80083B34u, 0u);
        w_u8(0x80083B5Cu, 0u);
        result = (sint16)position < 10;
    }
    else
    {
        w_u8(0x8008390Du, '/');
        result = '/';
    }
    return result;
}

sint32 results_init_marker_recs(void)
{
    uint32 records = game_alloc_arena_bytes(60);
    sint32 angle = 768;
    sint32 index;
    const PROFILE_GRID *table = &profile_at(r_u8(0x800E0595u))->grid[r_u32(0x8008347Cu)];

    FUNCTION_MARKER(0x8003B9C4u, "MAIN.EXE");
    w_u16(0x800F3FACu, 3u);
    w_u32(0x800F3FB4u, UINT32_MAX);
    w_u32(0x800F3FA8u, records);
    w_u32(0x800F3FB0u, 0u);
    w_u32(0x800F3FC0u, 0u);
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)index * 20u;
        sint32 sine = rsin(angle);
        sint32 cosine = rcos(angle);
        sint32 type = index;
        w_u16(record, (uint16)(r_u16(0x800CB070u) + 1000 * cosine / 4096 + cosine * (sint32)r_u32(0x800914A0u) / 4096));
        w_u16(record + 2u, r_u16(r_u32(0x8008349Cu) + 4u));
        w_u16(record + 4u, (uint16)(r_u16(0x800CB074u) + 1000 * sine / 4096 + sine * (sint32)r_u32(0x800914A0u) / 4096));
        if (table->offset[r_u32(0x80091480u + (uint32)index * 4u)] == 2u)
            type = 3;
        w_u16(record + 6u, (uint16)((type << 12) | (angle & 0x0FFF)));
        w_u16(record + 14u, 0u);
        angle += 256;
    }
    return 0;
}

uint32 results_select_table(uint32 unused, uint32 fallback)
{
    const PLAYER_PROFILE *profile_data;
    uint32 index;
    uint32 profile;
    uint32 second;
    uint32 first;

    FUNCTION_MARKER(0x8003BC14u, "MAIN.EXE");
    if (r_u16(0x800E1B9Cu) != 0u)
    {
        w_u32(0x80083740u, 6u);
        return 0x80091F3Cu;
    }
    if (r_s16(0x800E0582u) == 4)
        return 0x80091F8Cu + 20u * r_u8(0x800E1B8Bu);
    profile_data = profile_current();
    second = profile_data->level;
    profile = r_u8(0x800E0595u);
    first = profile_data->course;
    index = first + 6u * second;
    if (profile == 0u)
        fallback = 0x80091B04u + index * 20u;
    else if (profile == 1u)
        fallback = 0x80091C6Cu + index * 20u;
    else if (profile == 2u)
        fallback = 0x80091DD4u + index * 20u;
    w_u32(0x80083740u, profile_data->course);
    if (r_s16(0x800E0582u) == 3)
        w_u32(0x80083740u, 7u);
    return fallback;
}

sint32 race_config_results(void)
{
    uint32 descriptor;
    uint32 config;
    uint32 source;
    sint32 mode;
    sint32 selector;
    sint32 publish_selector = 1;
    sint32 vertical = 220;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8003BEB4u, "MAIN.EXE");

    race_format_time(0x800CB390u, 300);
    time_init_rec_zero(0x800DCFC8u);
    w_u32(0x800B69ECu, 0u);
    descriptor = results_select_table(0u, 0u);
    for (index = 0; index < 4; ++index)
        w_u16(0x800D69D8u + (uint32)index * 28u, 0u);

    w_u32(0x800834A0u, (uint32)(sint32)(sint16)r_u16(descriptor));
    w_u32(0x8008348Cu, (uint32)(sint32)(sint16)r_u16(descriptor + 12u));
    render_select_tex_desc(0x80083478u);
    {
        uint8 descriptor_mode = r_u8(descriptor + 8u);

        config = r_u32(0x8008349Cu);
        w_u8(0x800E0596u, descriptor_mode);
    }
    w_u16(config + 28u, r_u16(descriptor + 10u));
    w_u16(config + 30u, (uint16)(r_u16(descriptor + 14u) + 1u));
    mode = r_s16(0x800E0582u);
    source = r_u32(descriptor + 16u);

    switch (mode)
    {
        case 0:
        case 1:
            selector = r_u8(0x800E058Du) != 0u ? 1 : 2;
            break;
        case 2:
        case 3:
            selector = 1;
            break;
        case 4:
            selector = 4;
            break;
        case 6:
        {
            sint32 submode = r_s16(0x800E0584u);

            if (submode == 1)
            {
                w_u16(config + 28u, 1u);
                for (index = 0; index < 5; ++index)
                    w_u16(0x800E1A4Au + (uint32)index * 8u, r_u16(source + 6u + (uint32)index * 8u));
                source = 0x800E1A44u;
                selector = 2;
            }
            else if (submode == 2)
                selector = 1;
            else if (submode == 0)
                selector = 2;
            else
                publish_selector = 0;
            break;
        }
        case 7:
            source = 0x8009157Cu;
            selector = 1;
            break;
        default:
            publish_selector = 0;
            break;
    }
    if (publish_selector)
        w_u16(0x800E05D6u, (uint16)selector);
    if (r_s16(0x800E1B9Cu) != 0)
        w_u16(0x800E05D6u, 1u);

    for (index = 0; index < 5; ++index)
    {
        uint32 input = source + (uint32)index * 8u;
        uint32 output = config + 36u + (uint32)index * 20u;
        sint32 first = r_s16(input);
        sint32 third = r_s16(input + 4u);
        sint32 second = r_s16(input + 2u);
        sint32 value = (sint32)(6000u * (uint32)first + 100u * (uint32)second + (uint32)(third / 10));

        race_format_time(output, value);
        w_u16(output + 16u, r_u16(input + 6u));
    }
    w_u32(0x800B6AC0u, r_s16(0x800E05D6u) != 1);
    mode = (sint32)r_u32(0x80083484u);
    w_u32(0x800B6B1Cu, 0u);
    if (mode == 8)
    {
        uint8 profile = r_u8(0x800E0595u);
        sint32 value = profile == 1u ? 3000 : (profile == 2u ? 2500 : 3500);

        race_format_time(0x800D6B48u, value);
    }
    else
    {
        time_copy_rec(0x800D6B48u, config + 36u);
        time_packed_add(0x800D6B48u, config + 56u);
    }
    mode = (sint32)r_u32(0x80083478u);
    w_u32(0x800B6AF4u, 0u);

    if (mode == 1)
        vertical = 420;
    w_u16(0x800D6A10u, r_u16(descriptor + 4u));
    w_u16(0x800D6A2Au, 76u);
    {
        uint16 count = r_u16(descriptor + 2u);

        w_u16(0x800D6A0Eu, 192u);
        w_u16(0x800D69F4u, count);
    }
    w_u16(0x800D6A2Cu, r_u16(descriptor + 6u));
    w_u16(0x800D6A46u, 308u);
    {
        sint32 count = r_s16(0x800D6A10u);
        uint32 points = r_u32(0x80092010u + 4u * (uint32)count);

        for (index = 0; index < r_s16(0x800D6A10u); ++index)
        {
            w_u16(0x800D6A12u + (uint32)index * 2u, (uint16)((sint16)r_u16(points) + 20));
            w_u16(0x800D6A1Eu + (uint32)index * 2u, (uint16)(vertical + (sint16)r_u16(points + 2u) - 38));
            points += 4u;
        }
    }
    {
        sint32 count = (sint16)r_u16(0x800D69F4u);
        sint32 span = 18 * count + 10;
        sint32 half = (span - (span < 0)) / 2;
        uint32 left = r_u16(0x800D6A0Eu) - (uint32)half;

        for (index = 0; index < count; ++index)
        {
            w_u16(0x800D6A02u + (uint32)index * 2u, (uint16)(vertical + 200));
            w_u16(0x800D69F6u + (uint32)index * 2u, (uint16)(left + 18u * (uint32)index));
        }
    }
    result = 4 * (sint16)r_u16(0x800D6A2Cu);
    {
        uint32 points = r_u32(0x80092028u + (uint32)result);

        for (index = 0; index < (sint16)r_u16(0x800D6A2Cu); ++index)
        {
            w_u16(0x800D6A2Eu + (uint32)index * 2u, (uint16)(384 - ((sint16)r_u16(points) + 48)));
            w_u16(0x800D6A3Au + (uint32)index * 2u, (uint16)(vertical + (sint16)r_u16(points + 2u) - 38));
            points += 4u;
            result = r_s16(0x800D6A2Cu);
        }
    }
    w_u16(0x800B699Cu, 0u);
    return result;
}

sint32 results_select_best_time_rec(uint32 state)
{
    uint32 best = UINT32_MAX;
    sint32 count = r_u8(state);
    sint32 index;

    FUNCTION_MARKER(0x8003CA84u, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        uint32 value = (uint32)time_get_rec_ticks(state + 20u + (uint32)index * 16u);
        if (value < best)
        {
            best = value;
            w_u8(state + 1u, (uint8)index);
        }
    }
    return index < count;
}

sint32 results_fn_8003cb14(uint32 state, uint32 menu, sint32 unused, uint32 argument)
{
    sint32 choice = vehicle_menu(r_u32(state + 100u))->racer_num;
    sint32 distance;

    FUNCTION_MARKER(0x8003CB14u, "MAIN.EXE");
    w_u32(menu + 624u, 7u);
    if (!(r_u16(0x800E0582u) == 6u && r_u16(0x800E0584u) == 1u))
    {
        if (r_u32(0x80083478u) == 1u)
        {
            if (choice == 1)
                sound_queue_command(state, 2, 0, argument);
            else if (choice == 2)
                sound_queue_command(state, 3, 0, argument);
            else
                sound_queue_command(state, choice == 3 ? 13 : 4, 0, argument);
        }
        else if (choice == 1)
            sound_queue_command(state, 2, 0, argument);
        else if (choice == 2)
        {
            uint32 peer = state == 0x800DE0F0u ? r_u32(0x800DF0FCu) : r_u32(0x800DE154u);
            distance = (sint32)r_u32(r_u32(state + 100u) + 528u) - (sint32)r_u32(peer + 528u);
            if (distance >= 1000)
                sound_queue_command(state, 13, 0, argument);
            else if (distance >= 500)
                sound_queue_command(state, 7, 7, argument);
            else if (distance >= 301)
                sound_queue_command(state, 3, 0, argument);
        }
        else
            sound_queue_command(state, 13, 0, argument);
        sound_queue_command(state, 0, 5, argument);
    }
    w_u32(0x800B6B1Cu, 1u);
    vehicle_menu(menu)->mode = 5u;
    camera_for_view(state)->mode = 5u;
    w_u32(0x800B69ECu, 0u);
    results_select_best_time_rec(menu);
    xport_update_u8(menu, XPORT_MEMORY_UPDATE_SUBTRACT, 1u);
    race_format_time(0x800F2578u, 500);
    w_u16(0x800B6ACCu, 1u);
    w_u8(0x800E05B8u, r_u8(menu + 582u));
    return pickup_write_racer_indices(0x800E059Du);
}

sint32 results_advance_page(uint32 state, uint32 menu, sint32 unused, uint32 argument)
{
    uint32 current;
    uint32 previous;
    uint32 settings;
    uint32 value;
    sint32 index;
    sint32 maximum;

    FUNCTION_MARKER(0x8003CCECu, "MAIN.EXE");
    current = r_u8(menu);
    w_u8(menu, current + 1u);
    if ((sint16)r_u16(0x800E0582u) == 2)
    {
        current = r_u8(menu);
        if (current == 4u)
        {
            for (index = 0; index < 3; ++index)
                time_copy_rec(menu + 20u + (uint32)index * 16u, menu + 36u + (uint32)index * 16u);
            time_init_rec_zero(menu + 68u);
            current = r_u8(menu);
            w_u8(menu, current - 1u);
            current = r_u16(0x800B699Cu);
            w_u16(0x800B699Cu, current + 1u);
        }
        current = r_u8(menu);
        {
            uint32 entry = menu + (current << 4) + 4u;

            value = (uint32)time_get_rec_ticks(entry);
            current = r_u8(menu);
            if (current == 1u)
            {
                replay_finish(&vehicle_players[0]->contacts.points[0], 1);
                time_copy_chars7(text_bind(entry), text_bind(0x80083974u));
                previous = r_u32(0x800B6A54u);
                w_u32(0x800B6BC0u, value);
            }
            else
            {
                previous = r_u32(0x800B6BC0u);
                if (value > previous)
                {
                    replay_finish(&vehicle_players[0]->contacts.points[0], 0);
                    w_u8(menu + 2u, 1u);
                    return 0;
                }
                replay_finish(&vehicle_players[0]->contacts.points[0], 1);
                w_u32(0x800B6BC0u, value);
                time_copy_chars7(text_bind(entry), text_bind(0x80083974u));
                previous = r_u32(0x800B6A54u);
            }
            if (value < previous)
            {
                sound_queue_command(state, 1, 0, 0u);
                w_u32(0x800B6A54u, value);
                time_copy_chars7(text_bind(entry), text_bind(0x80083968u));
                time_copy_rec(menu + 532u, entry);
            }
        }
        w_u8(menu + 2u, 1u);
        return 0;
    }

    w_u8(menu + 2u, 1u);
    settings = r_u32(0x8008349Cu);
    current = r_u8(menu);
    maximum = (sint16)r_u16(settings + 28u);
    if ((sint32)current == maximum)
    {
        results_fn_8003cb14(state, menu, unused, argument);
        return 1;
    }
    if ((sint32)current == maximum - 1)
        sound_queue_command(state, 0, 0, 0u);
    else
    {
        if ((sint32)r_u32(0x800D6B54u) >= 200)
            return 0;
        sound_queue_command(state, 9, 0, 0u);
    }
    return 0;
}

sint32 results_fn_8003cf10(uint32 state, uint32 menu, sint32 unused, uint32 argument)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8003CF10u, "MAIN.EXE");
    if (r_u8(menu + 582u) >= 5u)
        w_u8(0x800E05B3u, 2u);
    if (r_u8(0x800E05B3u) == 0u)
        return 5;
    if (vehicle_menu(menu)->mode == 5u || vehicle_menu(menu)->mode == 7u)
        return 7;
    profile_data = profile_at(r_u8(0x800E0595u));
    if (r_u8(0x800E05B3u) == 2u)
    {
        sound_queue_command(state, 2, 0, argument);
        profile_data->progress.courses[profile_data->level][profile_data->course].result = 2u;
    }
    else if (r_u8(0x800E05B3u) == 1u)
        sound_queue_command(state, 4, 0, argument);
    sound_queue_command(state, 0, 5, argument);
    vehicle_menu(menu)->mode = 5u;
    race_format_time(0x800F2578u, 300);
    w_u32(0x800B6ACCu, 1u);
    return 1;
}

sint32 race_process_lap_completion(uint32 state, uint32 menu, sint32 unused, uint32 argument)
{
    uint32 frame;
    sint32 phase;
    sint32 completed = 0;
    sint32 result;

    FUNCTION_MARKER(0x8003D04Cu, "MAIN.EXE");
    frame = guest_stack_push(0x38u);
    phase = (sint32)vehicle_menu(menu)->mode;
    if (phase == 4 || phase == 6)
    {
        sint32 enabled = 1;

        if ((sint16)r_u16(0x800E0582u) == 6 && (sint16)r_u16(0x800E0584u) == 1 && (sint32)vehicle_menu(menu)->mode != 4)
            enabled = 0;
        if (enabled != 0 && r_u8(menu + 2u) == 0u)
        {
            w_u32(0x800B6AC0u, 1u);
            w_u8(menu + 2u, 1u);
            if (r_u32(0x80083484u) == 4u)
                replay_begin(&vehicle_players[0]->contacts.points[0]);
        }
        else if (enabled != 0)
        {
            uint32 count;

            xport_update_u8(menu + 2u, XPORT_MEMORY_UPDATE_ADD, 1u);
            {
                uint32 table = r_u32(0x8008349Cu);
                uint32 checkpoint = r_u8(menu + 2u);
                sint32 checkpoint_count = (sint16)r_u16(table + 30u);

                if (checkpoint == (uint32)checkpoint_count)
                {
                    completed = results_advance_page(state, menu, unused, argument);
                    if (r_u32(0x80083484u) == 4u)
                        sound_queue_command(state, 8, 1, argument);
                }
            }
            {
                uint32 lap = r_u8(menu);
                sint32 source = (sint32)r_u32(menu + 528u);
                uint32 checkpoint = r_u8(menu + 2u);

                race_format_time(menu + 80u * lap + 100u + 16u * checkpoint, source);
            }
            count = r_u32(menu + 4u) + 1u;
            w_u32(menu + 4u, count);
            if ((sint32)r_u32(0x800B6AF4u) < (sint32)count)
            {
                w_u32(0x800B6AF4u, count);
                if (((uint32)completed << 16) == 0u && ((sint16)r_u16(0x800E0582u) != 6 || (sint16)r_u16(0x800E0584u) != 1) && r_u32(0x80083484u) != 4u)
                {
                    phase = (sint32)vehicle_menu(menu)->mode;
                    if (phase == 6 && r_u32(0x800B6AC0u) != 0u)
                    {
                        uint32 scratch = frame + 0x10u;
                        uint32 peer;

                        sound_queue_command(state, 9, 0, argument);
                        race_format_time(scratch, 300);
                        time_packed_sub(scratch, 0x800F2578u);
                        time_packed_sub(0x800D6B48u, scratch);
                        peer = r_u32(0x800DE154u);
                        if (vehicle_menu(peer)->mode == 6u)
                        {
                            uint32 player_index;

                            time_packed_add(r_u32(0x800DE154u) + 516u, scratch);
                            peer = r_u32(0x800DE154u);
                            player_index = r_u8(peer);
                            peer = r_u32(0x800DE154u);
                            time_packed_add(peer + 20u + 16u * player_index, scratch);
                            vehicle_menu(r_u32(0x800DE154u))->mode = 4u;
                        }
                        if (r_u32(0x80083478u) == 2u)
                        {
                            peer = r_u32(0x800DF0FCu);
                            if (vehicle_menu(peer)->mode == 6u)
                            {
                                uint32 player_index;

                                time_packed_add(r_u32(0x800DF0FCu) + 516u, scratch);
                                peer = r_u32(0x800DF0FCu);
                                player_index = r_u8(peer);
                                peer = r_u32(0x800DF0FCu);
                                time_packed_add(peer + 20u + 16u * player_index, scratch);
                                vehicle_menu(r_u32(0x800DF0FCu))->mode = 4u;
                            }
                        }
                        race_format_time(0x800F2578u, 300);
                    }
                    {
                        uint32 table = r_u32(0x8008349Cu);
                        sint32 lap_count = (sint16)r_u16(table + 28u);
                        uint32 lap = r_u8(menu);

                        if (lap == (uint32)(lap_count - 1))
                        {
                            sint32 checkpoint_count = (sint16)r_u16(table + 30u);
                            uint32 checkpoint = r_u8(menu + 2u);

                            if (checkpoint == (uint32)(checkpoint_count - 1))
                                mesh_fn_8003429c(0x800DD4A0u);
                        }
                    }
                    {
                        uint32 mode = r_u32(0x80083484u);

                        if (mode != 9u && mode != 5u)
                            sound_queue_command(state, 8, 1, argument);
                    }
                    {
                        uint32 checkpoint = r_u8(menu + 2u);
                        uint32 table;
                        uint32 record;

                        w_u32(0x800B69ECu, 120u);
                        table = r_u32(0x8008349Cu);
                        record = table + 36u + 20u * checkpoint;
                        time_packed_add(0x800D6B48u, record);
                        time_copy_rec(0x800D6948u, record);
                    }
                }
            }
            else
            {
                uint32 peer;

                w_u32(menu + 548u, 240u);
                peer = state == 0x800DE0F0u ? r_u32(0x800DF0FCu) : r_u32(0x800DE154u);
                w_u32(peer + 548u, 240u);
                sound_queue_command(state, 8, 1, argument);
            }
        }
    }
    result = (sint16)(uint16)vehicle_menu(menu)->mode;
    guest_stack_pop(0x38u);
    return result;
}

sint32 results_2p_complete_route(void)
{
    sint32 state = (sint16)r_u16(0x800E0582u);
    sint32 selection = (sint16)r_u16(0x800E0580u);

    FUNCTION_MARKER(0x8005D8E8u, "MAIN.EXE");
    w_u8(0x800E058Bu, 10u);
    w_u8(0x800E058Cu, 0u);
    if (state == 0)
    {
        if (selection == -3)
        {
            w_u8(0x800E058Bu, 48u);
            return 48;
        }
        if (selection != -4 && selection != -2)
            return 51;
        w_u8(0x800E058Bu, 51u);
        w_u16(0x800B6A62u, (uint16)((sint32)r_u32(r_u32(0x800DE154u) + 528u) >= (sint32)r_u32(r_u32(0x800DF0FCu) + 528u)));
        return 1;
    }
    if (state == 1)
    {
        w_u16(0x800E1BB0u, (uint16)((uint16)vehicle_racer_count + vehicle_leader_count + vehicle_trailer_count));
        return profile_complete_selection();
    }
    return 1;
}
