#include "name.h"
#include "arena.h"
#include "race_events.h"
#include "timer.h"
#include "pickup.h"
#include "global.h"
#include "vehicle.h"
#include "sound.h"
#include "xport_trace.h"
#include <stdlib.h>

uint32 pickup_scan(const BOAT *boat)
{
    uint32 object = (uint32)boat->contacts.points[0].object;
    sint32 object_index;

    FUNCTION_MARKER(0x80024738u, "MAIN.EXE");
    for (object_index = 0; object_index < 2; ++object_index)
    {
        uint32 coordinate = object + 30u;
        uint32 flags = object + 33u;
        sint32 item_index = 0;

        while (item_index < r_u8(object + 7u))
        {
            uint32 type = r_u8(flags) >> 5;
            sint32 x;
            sint32 y;
            sint32 z;
            sint32 distance;
            sint32 square_sum;

            if (type >= 2u)
            {
                SVECTOR direction;
                VECTOR projection;
                VECTOR squared;
                uint32 first;
                uint32 second;
                uint32 third;

                first = r_u8(coordinate);
                second = r_u16(object + 8u);
                x = (sint16)(second + (first << 4));
                first = r_u8(flags - 2u);
                second = r_u16(object + 10u);
                y = (sint16)(second + (first << 4));
                first = r_u8(flags - 1u);
                second = r_u16(object + 12u);
                z = (sint16)(second + (first << 4));
                first = (uint16)boat->motion.position[0];
                x = (sint16)(x - (sint32)first);
                first = (uint16)boat->motion.position[1];
                y = (sint16)(y - (sint32)first);
                first = (uint16)boat->motion.position[2];
                z = (sint16)(z - (sint32)first);
                if ((sint32)((uint32)math_abs_s32(x) + (uint32)math_abs_s32(y) + (uint32)math_abs_s32(z)) <= 79999)
                {
                    sint32 dot;

                    first = (uint16)boat->motion.transform.pose.m[0][2];
                    second = (uint16)boat->motion.transform.pose.m[1][2];
                    third = (uint16)boat->motion.transform.pose.m[2][2];
                    dot = (sint32)((uint32)x * (uint32)(sint32)(sint16)first + (uint32)y * (uint32)(sint32)(sint16)second + (uint32)z * (uint32)(sint32)(sint16)third);
                    distance = dot / 4096;
                    if (distance < -100)
                        distance = -100;
                    if (distance >= 151)
                        distance = 150;
                    direction.vx = (sint16)first;
                    direction.vy = (sint16)second;
                    direction.vz = (sint16)third;
                    direction.pad = 0;
                    gte_gpf12(&direction, distance, &projection);
                    x = (sint16)(x - projection.vx);
                    y = (sint16)(y - projection.vy);
                    z = (sint16)(z - projection.vz);
                    projection.vx = x;
                    projection.vy = y;
                    projection.vz = z;
                    Square12(&projection, &squared);
                    square_sum = (sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz);
                    if ((type == 2u && square_sum < 2500) || (type >= 3u && square_sum < 10000))
                    {
                        uint32 player;
                        uint32 count;
                        uint32 entry;

                        if (type == 3u && (uint32)boat->control.driver == 0u)
                            return 0;
                        if (type == 5u && (uint32)boat->control.driver == 0u)
                            return 0;
                        if (type != 3u)
                        {
                            player = r_u8(0x8008373Au);
                            count = r_u8(0x80083738u + player);
                            entry = 0x800834B8u + player * 320u + count * 8u;
                            w_u32(entry + 4u, type);
                            w_u32(entry, coordinate);
                            player = r_u8(0x8008373Au);
                            count = r_u8(0x80083738u + player);
                            if (count >= 39u)
                                return 0;
                            w_u8(0x80083738u + player, (uint8)(count + 1u));
                        }
                        w_u8(flags, (uint8)(r_u8(flags) & 0x1Fu));
                        return type;
                    }
                }
            }
            ++item_index;
            flags += 14u;
            coordinate += 14u;
        }
        {
            uint32 link = r_u16(object + 2u);
            uint32 table = r_u32(0x800B6B80u);

            object = r_u32(table + link * 4u);
        }
    }
    return 0;
}

sint32 pickup_apply_command(uint32 state, uint32 menu, sint16 command, uint32 argument)
{
    BOAT *boat = vehicle_player(state);
    uint8 first;
    uint8 second;
    uint8 updated;

    FUNCTION_MARKER(0x8003D454u, "MAIN.EXE");
    if (command <= 0)
        return 0;
    if (command == 2)
    {
        first = r_u8(menu + 582u);
        if ((sint32)first >= (sint16)r_u16(0x800D6A10u))
            return 0;
        first = r_u8(menu + 582u);
        w_u8(menu + 582u, (uint8)(first + 1u));
        sound_queue_command(state, 1, 1, argument);
        return sound_queue_command(state, 30, 2, argument);
    }
    if (command == 3)
    {
        first = r_u8(menu + 572u);
        if (first != 0u)
        {
            first = r_u8(menu + 572u);
            w_u8(menu + 572u, (uint8)(first - 1u));
            return sound_queue_command(state, 7, 1, argument);
        }
        first = r_u8(menu + 592u);
        if (first >= 3u)
            return 0;
        first = r_u8(menu + 592u);
        second = r_u8(menu + 572u);
        updated = (uint8)(first + 1u);
        w_u8(menu + 592u, updated);
        first = r_u8(menu + 592u);
        if ((uint32)first + second >= 4u)
            w_u8(menu + 572u, (uint8)(3u - updated));
        sound_queue_command(state, 1, 1, argument);
        return sound_queue_command(state, 30, 2, argument);
    }
    if (command == 4)
    {
        race_format_time(0x800DCFC8u, 300);
        sound_queue_command(state, 10, 1, argument);
        return sound_queue_command(state, 30, 2, argument);
    }
    if (command == 1)
    {
        if ((uint32)boat->control.mode == 1u)
        {
            boat->control.mode = (uint32)(0u);
            boat->control.boost_tick = (sint32)(0u);
            sound_queue_command(state, 0, 6, argument);
        }
        first = r_u8(menu + 572u);
        if (first >= 3u)
            return 0;
        first = r_u8(menu + 572u);
        second = r_u8(menu + 592u);
        updated = (uint8)(first + 1u);
        w_u8(menu + 572u, updated);
        first = r_u8(menu + 572u);
        if ((uint32)first + second >= 4u)
            w_u8(menu + 592u, (uint8)(3u - updated));
        first = r_u8(menu + 572u);
        if (first == 3u)
        {
            vehicle_damp_vel(boat);
            w_u8(menu + 572u, 0u);
            return sound_queue_command(state, 6, 1, argument);
        }
        sound_queue_command(state, 2, 1, argument);
        return sound_queue_command(state, -30, 2, argument);
    }
    return 4;
}

sint32 pickup_update_slot_countdown(uint32 state, sint16 slot)
{
    uint32 record = state + 562u + (uint32)((sint32)slot * 10);
    uint8 active;
    sint32 result;

    FUNCTION_MARKER(0x8003D668u, "MAIN.EXE");
    active = r_u8(record);
    result = (sint16)r_u16(0x800D69D8u + (uint32)((sint32)slot * 28)) < active;
    if (result != 0)
        return result;
    result = (sint16)r_u16(record + 4u);
    if (result == 0)
        return result;
    result -= 1;
    w_u16(record + 4u, (uint16)result);
    result = (sint32)((uint32)(uint16)result << 16);
    if (result != 0)
        return result;
    if (r_u8(record + 1u) != 0u)
    {
        result = (sint16)r_u16(record + 2u);
        if (result > 0)
            return result;
        w_u8(record, 0u);
        w_u8(record + 1u, 0u);
        w_u16(record + 2u, 0u);
    }
    result = (sint16)r_u16(record + 2u);
    if (result < 0)
    {
        result = (sint32)r_u8(record) - 1;
        w_u8(record, (uint8)result);
    }
    return result;
}

sint32 pickup_update_slots(uint32 state, uint32 selection, const BOAT *boat)
{
    uint32 mode = r_u32(0x80083484u);
    uint32 menu = r_u32(state + 100u);
    sint16 command;

    FUNCTION_MARKER(0x8003D72Cu, "MAIN.EXE");
    w_u32(0x800B6AB4u, selection);
    command = mode == 8u ? (sint16)race_events_nearby_event_check(boat) : (sint16)pickup_scan(boat);
    mode = r_u32(0x80083484u);
    if (mode == 5u)
        return 5;
    pickup_apply_command(state, menu, (sint16)(command - 1), vehicle_legacy(boat));
    pickup_update_slot_countdown(menu, 2);
    pickup_update_slot_countdown(menu, 3);
    return pickup_update_slot_countdown(menu, 1);
}

sint32 pickup_write_racer_indices(uint32 output)
{
    uint32 partial;
    sint32 count;
    sint32 index;

    FUNCTION_MARKER(0x8003D7E8u, "MAIN.EXE");
    partial = (uint32)(sint32)(sint16)vehicle_leader_count;
    partial += vehicle_racer_count;
    count = (sint32)(partial + (uint32)(sint32)(sint16)vehicle_trailer_count);
    if (count <= 0)
        return (sint32)partial;
    for (index = 0; index < count; ++index)
        w_u8(output + (uint32)index, (uint8)(vehicle_boats[index].menu.racer_num - 1u));
    return 0;
}

sint32 pickup_update_racer_flags(void)
{
    sint32 count = (sint32)r_u32(0x800B6A98u);
    sint32 index;

    FUNCTION_MARKER(0x8003D850u, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        uint32 object = r_u32(r_u32(0x800B6B80u) + (uint32)index * 4u);
        xport_update_u8(object + 18u, XPORT_MEMORY_UPDATE_OR, 4u);
        xport_update_u8(object + 25u, XPORT_MEMORY_UPDATE_OR, 8u);
    }
    return index < (sint32)r_u32(0x800B6A98u);
}

sint32 pickup_fn_8003d8dc(uint32 state)
{
    sint32 count;
    uint32 records;
    uint32 table;
    sint32 index;

    FUNCTION_MARKER(0x8003D8DCu, "MAIN.EXE");
    w_u32(0x800D2498u, name_reels.code);
    count = 600 + 8 * (global_fn_8006e9d8() & 0x31);
    table = game_alloc_arena_bytes(4 * count);
    records = game_alloc_arena_bytes(226 * count);
    w_u32(0x800B6B80u, table);
    w_u32(0x800B6A98u, (uint32)count);
    for (index = 0; index < count; ++index)
        w_u32(table + (uint32)index * 4u, records + (uint32)index * 226u);
    pickup_fn_8003da8c(records, count);
    race_events_fn_8003f164(state, count);
    race_events_init_fractal_light(state, count);
    w_u32(state + 52u, 0x80089580u);
    w_u32(0x800CB5B4u, 0u);
    w_u32(0x800CB5A0u, 0u);
    w_u32(0x800CB594u, 0u);
    w_u32(0x800CB588u, 0u);
    w_u32(0x800B68C0u, 0x800CB580u);
    w_u32(0x800CB5ECu, 0u);
    w_u32(0x800CB5E0u, 0u);
    w_u32(0x800CB5D4u, 0u);
    w_u32(0x800CB5C8u, 0u);
    w_u32(0x800B68C4u, 0u);
    w_u8(0x800CB5FCu, 1u);
    w_u8(0x800CB601u, 30u);
    w_u8(0x800CB5FEu, 30u);
    w_u8(0x800CB602u, 15u);
    w_u8(0x800CB5FFu, 15u);
    w_u32(0x800CB610u, 0x800B68C0u);
    w_u32(0x800CB60Cu, 0x800B68C0u);
    w_u8(0x800CB603u, 0u);
    w_u8(0x800CB600u, 0u);
    w_u32(0x800CB614u, 0u);
    w_u32(0x800CB618u, 0u);
    w_u8(0x800CB5F9u, 1u);
    w_u8(0x800CB5FAu, 192u);
    w_u32(state + 32u, 0x800CB5F8u);
    return 192;
}

static sint16 pickup_lo_s16(uint32 value)
{
    return (sint16)(uint16)value;
}

static sint16 pickup_hi_s16(uint32 value)
{
    return (sint16)(uint16)(value >> 16);
}

static uint32 pickup_set_lo_u16(uint32 value, sint32 low)
{
    return (value & 0xFFFF0000u) | (uint16)low;
}

static sint32 pickup_mul_shift12(sint32 first, sint32 second)
{
    return (sint32)(((sint64)first * (sint64)second) >> 12);
}

static uint8 pickup_clamp_byte(sint32 value)
{
    if (value < 0)
        return 0u;
    if (value >= 256)
        return 255u;
    return (uint8)value;
}

uint32 pickup_fn_8003da8c(uint32 records, sint32 count)
{
    uint32 turn_step = r_u32(0x80080F38u);
    uint32 turn_amount = r_u32(0x80080F3Cu);
    uint32 turn_phase = r_u32(0x80080F40u);
    uint32 slope_step = r_u32(0x80080F44u);
    uint32 slope_amount = r_u32(0x80080F48u);
    uint32 slope_phase = r_u32(0x80080F4Cu);
    uint32 left_offsets = r_u32(0x80080F50u);
    uint32 outer_offsets = r_u32(0x80080F54u);
    uint32 random_ranges = r_u32(0x80080F58u);
    uint32 color_ranges = r_u32(0x80080F5Cu);
    uint32 color_bases = r_u32(0x80080F60u);
    uint32 right_offsets = left_offsets;
    uint32 right_outer_offsets = outer_offsets;
    uint32 right_random_ranges = random_ranges;
    uint32 right_color_ranges = color_ranges;
    uint32 right_color_bases = color_bases;
    uint32 current_record = records;
    sint32 straight = 1;
    sint32 previous_rise = 0;
    sint32 previous_fall = 0;
    uint16 width = 256u;
    uint16 width_delta = 1u;
    uint8 pattern = 0u;
    sint32 index;

    FUNCTION_MARKER(0x8003DA8Cu, "MAIN.EXE");
    if (count > 0)
    {
        uint32 last = records + (uint32)(count - 1) * 226u;
        sint32 point;

        w_u16(last + 8u, 0u);
        w_u16(last + 10u, 0u);
        w_u16(last + 12u, 0u);
        for (point = 0; point < 14; ++point)
            w_u8(last + 37u + (uint32)point * 14u, (uint8)global_fn_8006e9d8());
    }
    for (index = 0; index < count; ++index)
    {
        sint32 radius[14];
        sint16 x[14];
        sint16 y[14];
        sint16 z[14];
        sint32 remaining;
        sint32 curve;
        sint32 slope = 0;
        sint32 rise = 0;
        sint32 fall = 0;
        sint32 prior_rise = previous_rise;
        sint32 prior_fall = previous_fall;
        sint32 angle;
        sint32 cosine;
        sint32 sine;
        sint16 minimum_x;
        sint16 minimum_y;
        sint16 minimum_z;
        sint32 point;

        remaining = count - index - 1;
        if (pickup_lo_s16(turn_phase) / 4096 == 1)
        {
            if ((sint16)remaining < 65)
            {
                turn_phase = pickup_set_lo_u16(turn_phase, 0);
                turn_step = 0x00200020u;
                turn_amount = pickup_set_lo_u16(turn_amount, 0);
            }
            else
            {
                sint32 bounded = remaining >= 129 ? 128 : remaining;
                sint32 half = bounded / 2;
                sint32 length = half + global_fn_8006e9d8() % half;
                sint32 quarter = length / 4;
                sint32 eighth = length / 8;
                sint32 first = eighth + global_fn_8006e9d8() % quarter;
                sint32 first_divisor = 2048 / first;
                sint32 first_duration = 2048 / first_divisor;
                sint32 second_divisor = 2048 / (length - first_duration);
                sint32 second_duration = 2048 / second_divisor;

                straight = 0;
                turn_step = (uint32)(uint16)first_divisor | ((uint32)(uint16)second_divisor << 16);
                turn_amount = pickup_set_lo_u16(turn_amount, 8 * second_duration);
                if (pickup_lo_s16(turn_amount) / first_duration >= 65)
                    turn_amount = pickup_set_lo_u16(turn_amount, first_duration << 6);
                turn_phase = pickup_set_lo_u16(turn_phase, 0);
            }
        }
        else
        {
            sint32 phase = pickup_lo_s16(turn_phase);
            phase += phase < 2048 ? pickup_lo_s16(turn_step) : pickup_hi_s16(turn_step);
            turn_phase = pickup_set_lo_u16(turn_phase, phase);
        }
        curve = pickup_mul_shift12(rcos(pickup_lo_s16(turn_phase)) - 4096, pickup_lo_s16(turn_amount));

        if (pickup_lo_s16(slope_phase) / 4096 == 1)
        {
            if ((sint16)remaining < 49)
            {
                straight = 1;
                slope_step = 0x00200020u;
                slope_amount = 0u;
                slope_phase = 0x00180000u;
            }
            else
            {
                sint32 bounded = remaining >= 129 ? 128 : remaining;
                sint32 half = bounded / 2;
                sint32 length = bounded / 4 + global_fn_8006e9d8() % half;
                sint32 quarter = length / 4;
                sint32 first = quarter + global_fn_8006e9d8() % quarter;
                sint32 first_divisor = 2048 / first;
                sint32 first_duration = 2048 / first_divisor;
                sint32 second_divisor = 2048 / (length - first_duration);
                sint32 second_duration = 2048 / second_divisor;
                sint32 phase_divisor;

                slope_step = (uint32)(uint16)first_divisor | ((uint32)(uint16)second_divisor << 16);
                slope_amount = pickup_set_lo_u16(slope_amount, 32 * (first_duration + global_fn_8006e9d8() % first_duration));
                phase_divisor = second_duration + global_fn_8006e9d8() % second_duration;
                slope_amount = (slope_amount & 0x0000FFFFu) | ((uint32)(uint16)(32 * phase_divisor) << 16);
                phase_divisor = global_fn_8006e9d8();
                phase_divisor = phase_divisor - 8 * (phase_divisor / 8) + 24;
                slope_phase = ((uint32)(uint16)phase_divisor << 16);
            }
        }
        else if (pickup_lo_s16(slope_phase) < 2048)
        {
            slope_phase = pickup_set_lo_u16(slope_phase, pickup_lo_s16(slope_phase) + pickup_lo_s16(slope_step));
            slope = pickup_mul_shift12(rsin(pickup_lo_s16(slope_phase)), pickup_lo_s16(slope_amount));
            if (pickup_lo_s16(slope_phase) >= 1024)
                fall = 1;
        }
        else
        {
            slope_phase = pickup_set_lo_u16(slope_phase, pickup_lo_s16(slope_phase) + pickup_hi_s16(slope_step));
            slope = pickup_mul_shift12(rsin(pickup_lo_s16(slope_phase)), pickup_hi_s16(slope_amount));
            if (pickup_lo_s16(slope_phase) >= 3072)
                rise = 1;
        }

        if (straight)
        {
            if (width == 256u)
                width_delta = 0u;
            else
            {
                width_delta = 1u;
                if (width > 256u)
                    width_delta = 0xFFFFu;
            }
        }
        else if (width_delta == 0u)
            width_delta = 1u;
        if (width >= 301u)
            width_delta = (uint16) - (global_fn_8006e9d8() & 3);
        else if (width < 140u)
            width_delta = (uint16)(global_fn_8006e9d8() & 3);
        width = (uint16)(width + width_delta);

        radius[0] = slope + 21500;
        for (point = 1; point < 14; ++point)
            radius[point] = radius[point - 1] + (sint16)width;
        if (straight)
        {
            radius[0] -= pickup_hi_s16(outer_offsets);
            radius[1] -= pickup_lo_s16(outer_offsets);
            radius[2] -= pickup_hi_s16(left_offsets);
            radius[3] -= pickup_lo_s16(left_offsets);
        }
        else if (rise == 1)
        {
            sint32 random = global_fn_8006e9d8() % (sint8)(random_ranges >> 16);

            radius[2] -= pickup_hi_s16(left_offsets);
            radius[1] = radius[2];
            radius[0] = radius[2] - (sint16)width - pickup_lo_s16(outer_offsets) - (random << 6);
        }
        else
        {
            radius[0] -= pickup_hi_s16(outer_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 24)) << 6);
            radius[1] -= pickup_lo_s16(outer_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 16)) << 6);
            radius[2] -= pickup_hi_s16(left_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 8)) << 6);
        }
        if (straight)
        {
            radius[9] += pickup_lo_s16(right_offsets);
            radius[10] += pickup_hi_s16(right_offsets);
            radius[11] += pickup_lo_s16(right_outer_offsets);
            radius[12] += pickup_hi_s16(right_outer_offsets);
        }
        else if (fall == 1)
        {
            sint32 random = global_fn_8006e9d8() % (sint8)(right_random_ranges >> 16);

            radius[10] += pickup_hi_s16(right_offsets);
            radius[11] = radius[10];
            radius[12] = radius[10] + (sint16)width + pickup_lo_s16(right_outer_offsets) + (random << 6);
        }
        else
        {
            radius[10] += pickup_hi_s16(right_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 8)) << 6);
            radius[11] += pickup_lo_s16(right_outer_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 16)) << 6);
            radius[12] += pickup_hi_s16(right_outer_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 24)) << 6);
        }

        angle = (index << 12) / count;
        cosine = rcos(angle);
        sine = rsin(angle);
        if (straight)
        {
            for (point = 0; point < 14; ++point)
                y[point] = (sint16)curve;
        }
        else if (pickup_lo_s16(slope_phase) >= 2049)
        {
            for (point = 13; point >= 0; --point)
            {
                if (point >= 9)
                    y[point] = (sint16)curve;
                else if (point < 3)
                    y[point] = y[point + 1];
                else if (pickup_lo_s16(slope_phase) >= 3073)
                    y[point] = (sint16)(y[point + 1] + (4096 - pickup_lo_s16(slope_phase)) / pickup_hi_s16(slope_phase));
                else
                    y[point] = (sint16)(y[point + 1] + (pickup_lo_s16(slope_phase) - 2048) / pickup_hi_s16(slope_phase));
            }
        }
        else
        {
            for (point = 0; point < 14; ++point)
            {
                if (point < 4)
                    y[point] = (sint16)curve;
                else if (point >= 10)
                    y[point] = y[point - 1];
                else if (pickup_lo_s16(slope_phase) >= 1025)
                    y[point] = (sint16)(y[point - 1] + (2048 - pickup_lo_s16(slope_phase)) / pickup_hi_s16(slope_phase));
                else
                    y[point] = (sint16)(y[point - 1] + pickup_lo_s16(slope_phase) / pickup_hi_s16(slope_phase));
            }
        }
        for (point = 0; point < 14; ++point)
        {
            x[point] = (sint16)pickup_mul_shift12(cosine, radius[point]);
            z[point] = (sint16)pickup_mul_shift12(sine, radius[point]);
        }
        minimum_x = x[0];
        minimum_y = y[0];
        minimum_z = z[0];
        for (point = 1; point < 14; ++point)
        {
            if (x[point] < minimum_x)
                minimum_x = x[point];
            if (y[point] < minimum_y)
                minimum_y = y[point];
            if (z[point] < minimum_z)
                minimum_z = z[point];
        }
        w_u16(current_record + 8u, (uint16)minimum_x);
        w_u16(current_record + 10u, (uint16)minimum_y);
        w_u16(current_record + 12u, (uint16)minimum_z);
        for (point = 0; point < 14; ++point)
        {
            uint32 packed = current_record + 30u + (uint32)point * 14u;

            w_u8(packed, pickup_clamp_byte(((sint32)x[point] - minimum_x) >> 4));
            w_u8(packed + 1u, pickup_clamp_byte(((sint32)y[point] - minimum_y) >> 4));
            w_u8(packed + 2u, pickup_clamp_byte(((sint32)z[point] - minimum_z) >> 4));
        }
        w_u8(current_record + 7u, 14u);
        w_u16(current_record, (uint16)(index + 1));
        w_u16(current_record + 2u, (uint16)(index - 1));
        w_u8(current_record + 14u, 0u);
        w_u8(current_record + 15u, 1u);
        w_u8(current_record + 199u, 0u);

        w_u8(current_record + 45u, (uint8)(r_u8(current_record + 45u) + (uint8)(color_bases >> 16) + global_fn_8006e9d8() % (sint8)(color_ranges >> 16)));
        if (straight)
        {
            w_u8(current_record + 59u, (uint8)(r_u8(current_record + 59u) + 20u + r_u8(current_record + 73u)));
            w_u8(current_record + 31u, (uint8)(r_u8(current_record + 31u) + (uint8)(color_bases >> 24) + global_fn_8006e9d8() % (sint8)(color_ranges >> 24)));
        }
        else if (rise == 1)
        {
            uint8 value;

            w_u8(current_record + 59u, (uint8)(r_u8(current_record + 59u) + (uint8)(color_bases >> 8)));
            value = (uint8)(r_u8(current_record + 59u) + 20u);
            w_u8(current_record + 31u, r_u8(current_record + 45u));
            w_u8(current_record + 45u, value);
        }
        else
        {
            w_u8(current_record + 59u, (uint8)(r_u8(current_record + 59u) + (uint8)(color_bases >> 8) + global_fn_8006e9d8() % (sint8)(color_ranges >> 8)));
            w_u8(current_record + 31u, (uint8)(r_u8(current_record + 31u) + (uint8)(color_bases >> 24) + global_fn_8006e9d8() % (sint8)(color_ranges >> 24)));
        }

        w_u8(current_record + 185u, (uint8)(r_u8(current_record + 185u) + (uint8)(right_color_bases >> 16) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 16)));
        if (straight)
        {
            w_u8(current_record + 171u, (uint8)(r_u8(current_record + 157u) + 20u));
            w_u8(current_record + 199u, (uint8)(r_u8(current_record + 199u) + (uint8)(right_color_bases >> 24) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 24)));
        }
        else if (fall == 1)
        {
            uint8 value;

            w_u8(current_record + 171u, (uint8)(r_u8(current_record + 171u) + (uint8)(right_color_bases >> 8)));
            value = (uint8)(r_u8(current_record + 171u) + 20u);
            w_u8(current_record + 199u, r_u8(current_record + 185u));
            w_u8(current_record + 185u, value);
        }
        else
        {
            w_u8(current_record + 171u, (uint8)(r_u8(current_record + 171u) + (uint8)(right_color_bases >> 8) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 8)));
            w_u8(current_record + 199u, (uint8)(r_u8(current_record + 199u) + (uint8)(right_color_bases >> 24) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 24)));
        }

        pattern = r_u8(0x8009316Cu + (uint32)pattern * 4u + ((uint32)global_fn_8006e9d8() & 3u));
        for (point = 0; point < 14; ++point)
        {
            uint8 source = r_u8(0x80093124u + (uint32)point);
            uint8 material;
            uint32 choices = 0x80093148u + (uint32)pattern * 3u;

            if ((source & 1u) != 0u)
                material = 1u;
            else if ((source & 2u) != 0u)
            {
                if ((straight && point == 9) || (!straight && fall == 1 && prior_fall == 1 && point == 10))
                    material = 24u;
                else
                {
                    uint8 choice = (uint8)(source - 2u);

                    if (!straight && fall == 1 && prior_fall == 1 && point >= 11)
                        choice = (uint8)(r_u8(0x80093123u + (uint32)point) - 2u);
                    material = r_u8(choices + (choice >> 2));
                }
                if (material >= 7u)
                    material = (uint8)(material + 2u);
            }
            else if (straight && point == 2)
                material = 24u;
            else if (!straight && rise == 1 && prior_rise == 1 && point == 1)
                material = 24u;
            else
            {
                uint8 choice = (!straight && rise == 1 && prior_rise == 1 && point == 0) ? r_u8(0x80093125u) : source;
                material = r_u8(choices + (choice >> 2));
            }
            w_u8(current_record + 16u + (uint32)point, (uint8)(r_u8(0x80093114u + (uint32)point) | (uint8)(material << 3)));
        }
        previous_rise = rise;
        previous_fall = fall;
        current_record += 226u;
    }
    if (count > 0)
    {
        w_u16(records + 2u, (uint16)(count - 1));
        w_u16(records + (uint32)(count - 1) * 226u, 0u);
    }
    return records + (uint32)count * 226u;
}
