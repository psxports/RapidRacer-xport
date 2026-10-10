#include "game.h"
#include "profile.h"
#include "ai.h"
#include "global.h"
#include "route.h"
#include "vehicle.h"
#include "xport_trace.h"
#include <stdlib.h>

static uint32 ai_settings_alternate;

// Shared control cfg formerly published at MAIN.EXE 800DE0E0
AI_CONTROL_CFG ai_control_cfg;
typedef struct
{
    uint8 lookahead;
    uint16 return_base;
    sint16 return_step;
} AI_CONTROL_TEMPLATE;
static const AI_CONTROL_TEMPLATE ai_control_templates[7] = {
    { 8, 128, -2 },
    { 6, 128, -2 },
    { 8, 100, -2 },
    { 8, 108, -2 },
    { 8, 128, -2 },
    { 7, 128, -2 },
    { 8, 128, -2 },
};
static const uint16 ai_control_speeds[7][9][2] = {
    { { 29020, 1300 }, { 29520, 2200 }, { 30020, 2900 }, { 29520, 2000 }, { 31020, 2900 }, { 32020, 4000 }, { 30020, 2200 }, { 31520, 3000 }, { 32020, 4000 } },
    { { 26320, 2200 }, { 27020, 2950 }, { 27520, 3300 }, { 27020, 2150 }, { 27520, 2450 }, { 28020, 3100 }, { 27520, 2300 }, { 28020, 3150 }, { 28520, 3300 } },
    { { 26520, 2500 }, { 27020, 2950 }, { 27520, 3500 }, { 26520, 2300 }, { 26520, 2400 }, { 33020, 4100 }, { 26520, 2600 }, { 27520, 3250 }, { 28020, 4100 } },
    { { 29220, 1600 }, { 30520, 2200 }, { 31020, 2700 }, { 30520, 2000 }, { 31020, 2500 }, { 32020, 3600 }, { 30520, 2100 }, { 32020, 2600 }, { 33020, 3600 } },
    { { 27620, 2300 }, { 28170, 2800 }, { 28620, 3600 }, { 28170, 3300 }, { 28620, 3400 }, { 29620, 4200 }, { 28170, 3400 }, { 29620, 3500 }, { 30620, 4200 } },
    { { 35520, 2600 }, { 40520, 3750 }, { 41070, 4200 }, { 41520, 3400 }, { 42520, 3850 }, { 43520, 4400 }, { 41520, 3500 }, { 42020, 3950 }, { 42520, 4000 } },
    { { 40020, 3800 }, { 40020, 4000 }, { 40020, 4200 }, { 26320, 2200 }, { 26820, 2700 }, { 27320, 3200 }, { 26820, 2700 }, { 27320, 3200 }, { 27820, 3700 } },
};

// Mutable route settings and immutable tuning from MAIN.EXE
static AI_ROUTE_CFG ai_route_cfg[9] = {
    { 100, 1100, 350, -16, 384, 128, 4, 2, 4, 4, 5400 },
    { 100, 1100, 350, -32, 512, 156, 1, 4, 3, 2, 5300 },
    { 50, 1600, 256, -44, 256, 128, 4, 3, 4, 4, 5400 },
    { 50, 1024, 128, 0, 256, 48, 2, 2, 3, 2, 5300 },
    { 300, 1600, 350, -38, 256, 156, 2, 3, 3, 4, 5400 },
    { 50, 600, 400, -32, 384, 106, 2, 2, 4, 3, 5400 },
    { 100, 1100, 350, -16, 384, 156, 1, 3, 4, 2, 5500 },
    { 150, 1100, 250, 0, 256, 64, 1, 3, 4, 4, 5500 },
    { 150, 1100, 250, 0, 256, 64, 1, 3, 4, 1, 5600 }
};
static AI_ROUTE_CFG ai_alt_route_cfg[2] = {
    { 230, 1200, 600, -2024, 2024, 128, 2, 3, 3, 3, 5100 },
    { 500, 800, 500, -2024, 2024, 128, 1, 3, 1, 3, 5400 }
};
typedef struct
{
    sint16 min_base, max_base, min_step, max_step;
} AI_ROUTE_TUNING;
static const AI_ROUTE_TUNING ai_route_tuning[7] = {
    { -32, 1024, 8, 4 },
    { -64, 1256, 16, 4 },
    { -88, 768, 16, 4 },
    { 0, 768, 0, 4 },
    { -76, 768, 16, 4 },
    { -64, 768, 16, 4 },
    { -32, 1024, 8, 4 }
};

static sint32 ai_fn_8002fc28_random(void)
{
    return global_fn_8006e9d8();
}

sint32 ai_update_trailing_pool(uint32 entry)
{
    uint32 config;
    BOAT **list_slot;
    BOAT *state;
    BOAT *adjacent;
    uint32 value;
    sint32 rank;
    sint32 count;
    sint32 distant;
    sint32 index;
    sint32 result;
    sint32 numerator;
    sint32 divisor;
    sint32 remainder;

    FUNCTION_MARKER(0x8002D324u, "MAIN.EXE");
    config = r_u32(entry + 100u);
    rank = r_u8(config + 14u);
    index = rank + 1;
    list_slot = vehicle_racers + (rank);
    state = *(list_slot);
    count = (sint32)vehicle_racer_count;
    value = (uint32)state->race.progress;
    distant = 0;
    while (index < count)
    {
        sint32 mode;

        list_slot = vehicle_racers + (index);
        state = *(list_slot);
        mode = (sint16)(uint16)state->race.mode;
        if (mode == 3)
        {
            sint32 gap = (sint32)(value - (uint32)state->race.progress);

            if (gap >= 4)
                distant++;
        }
        index++;
    }

    result = distant < 2;
    if (distant < 2)
    {
        if ((sint16)vehicle_trailer_count <= 0)
        {
            result = distant < 3;
            goto done;
        }

        state = *(vehicle_racers);
        result = (sint32)((uint32)state->race.phase | ((uint32)state->race.mode << 16));
        if ((uint32)result != 0x00030001u)
            goto done;

        index = (sint16)vehicle_leader_count;
        state = *(vehicle_racers);
        *(vehicle_leaders + (index)) = state;
        value = (uint32)vehicle_leader_count + 1u;
        count = (sint32)vehicle_racer_count;
        vehicle_leader_count = (uint16)((uint16)value);
        index = 1;
        if (index < count)
        {
            BOAT **destination = vehicle_racers;
            BOAT **source = destination + 1;

            do
            {
                state = *(source);
                source++;
                index++;
                *(destination) = state;
                count = (sint32)vehicle_racer_count;
                destination++;
            } while (index < count);
        }

        value = (uint32)vehicle_trailer_count - 1u;
        vehicle_trailer_count = (uint16)((uint16)value);
        index = (sint16)(uint16)value;
        count = (sint32)vehicle_racer_count;
        state = *(vehicle_trailers + ((sint32)index));
        *(vehicle_racers + (count - 1)) = state;
        count = (sint32)vehicle_racer_count;
        adjacent = *(vehicle_racers + (count - 2));
        numerator = (sint32)((uint32)adjacent->race.progress - 20u);
        divisor = (sint32)route_resources.count;
        (void)math_div_s32(numerator, divisor);
        remainder = numerator % divisor;
        state->race.progress = (sint32)((uint32)numerator);
        if (remainder < 0)
            remainder = (sint32)((uint32)remainder + (uint32)divisor);
        ai_place(state, (sint16)remainder);

        config = r_u32(entry + 100u);
        value = (uint32)r_u8(config + 14u) - 1u;
        w_u8(config + 14u, (uint8)value);
        config = r_u32(entry + 100u);
        result = (sint32)((uint32)r_u8(config + 15u) - 1u);
        w_u8(config + 15u, (uint8)result);
        goto done;
    }

    result = distant < 3;
    if (result != 0)
        goto done;
    result = (sint16)vehicle_leader_count;
    if (result <= 0)
        goto done;

    count = (sint32)vehicle_racer_count;
    state = *(vehicle_racers + (count - 1));
    result = (sint32)((uint32)state->race.phase | ((uint32)state->race.mode << 16));
    if ((uint32)result != 0x00030001u)
        goto done;

    index = (sint16)vehicle_trailer_count;
    *(vehicle_trailers + ((sint32)index)) = state;
    value = (uint32)vehicle_trailer_count + 1u;
    count = (sint32)vehicle_racer_count;
    index = count - 1;
    vehicle_trailer_count = (uint16)((uint16)value);
    if (index > 0)
    {
        BOAT **source = vehicle_racers + (index - 1);
        BOAT **destination = source + 1;

        do
        {
            state = *(source);
            source--;
            index--;
            *(destination) = state;
            destination--;
        } while (index > 0);
    }

    value = (uint32)vehicle_leader_count - 1u;
    adjacent = *(vehicle_racers + 1);
    vehicle_leader_count = (uint16)((uint16)value);
    index = (sint16)(uint16)value;
    state = *(vehicle_leaders + ((sint32)index));
    *(vehicle_racers) = state;
    numerator = (sint32)((uint32)adjacent->race.progress + 20u);
    state = *(vehicle_racers);
    state->race.progress = (sint32)((uint32)numerator);
    state = *(vehicle_racers);
    divisor = (sint32)route_resources.count;
    numerator = (sint32)(uint32)state->race.progress;
    (void)math_div_s32(numerator, divisor);
    remainder = numerator % divisor;
    if (remainder < 0)
        remainder = (sint32)((uint32)remainder + (uint32)divisor);
    state = *(vehicle_racers);
    ai_place(state, (sint16)remainder);

    config = r_u32(entry + 100u);
    value = (uint32)r_u8(config + 14u) + 1u;
    w_u8(config + 14u, (uint8)value);
    config = r_u32(entry + 100u);
    result = (sint32)((uint32)r_u8(config + 15u) + 1u);
    w_u8(config + 15u, (uint8)result);

done:
    return result;
}

sint32 ai_place(BOAT *boat, sint16 segment)
{
    uint32 first;
    uint32 second;
    uint32 third;
    sint32 x;
    sint32 y;
    sint32 z;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002D714u, "MAIN.EXE");
    route_contact_init(&boat->contacts.points[0], route_resources.segments[(uint32)segment], 4);

    x = (sint16)(uint16)boat->contacts.points[0].sample.vx;
    first = (uint32)boat->contacts.points[0].position[0];
    y = (sint16)(uint16)boat->contacts.points[0].sample.vy;
    z = (sint16)(uint16)boat->contacts.points[0].sample.vz;
    first -= (uint32)x;
    second = (uint32)boat->contacts.points[0].position[2];
    boat->contacts.points[0].normal.vx = (sint16)(0u);
    boat->contacts.points[1].normal.vx = (sint16)(0u);
    boat->contacts.points[0].normal.vz = (sint16)(0u);
    boat->contacts.points[1].normal.vz = (sint16)(0u);
    boat->motion.position[0] = (sint32)(first);
    third = (uint32)boat->contacts.points[0].position[1];
    second -= (uint32)z;
    boat->motion.position[2] = (sint32)(second);
    first = (uint32)boat->motion.position[0];
    third -= (uint32)y;
    boat->motion.position[1] = (sint32)(third);
    y = (sint32)(uint32)boat->motion.position[1];
    z = (sint32)(uint32)boat->motion.position[2];
    boat->contacts.points[0].normal.vy = (sint16)(4096u);
    boat->contacts.points[1].normal.vy = (sint16)(4096u);
    boat->motion.transform.pose.t[0] = (sint32)(first);
    boat->motion.transform.pose.t[1] = (sint32)((uint32)y);
    boat->motion.transform.pose.t[2] = (sint32)((uint32)z);
    next = ai_fn_8002fc28_random();
    boat->route.behavior = (uint16)((uint16)(12u + ((uint32)next & 1u)));
    boat->route.lookahead = (uint16)(2u);
    next = (sint32)((uint32)(sint32)segment + 2u);
    boat->route.target_segment = (uint16)((uint16)next);
    next = (uint16)boat->route.target_segment;
    count = (sint32)route_resources.count;
    boat->route.lane_timer = (uint16)(0u);
    if (next >= count)
    {
        next = (sint32)((uint32)next - (uint16)route_resources.count);
        boat->route.target_segment = (uint16)((uint16)next);
    }
    first = (uint16)boat->contacts.points[0].left;
    boat->race.mode = (uint16)(3u);
    boat->route.segment = (uint16)((uint16)segment);
    boat->route.speed = (sint32)(0u);
    boat->route_target.vector[0] = (sint32)(0u);
    boat->route_target.vector[1] = (sint32)(0u);
    boat->route_target.vector[2] = (sint32)(0u);
    boat->route_target.length = (sint32)(0u);
    boat->race.phase = (uint16)(1u);
    boat->route.ticks = (uint32)(0u);
    boat->route.target_lane = (uint16)((uint16)first);
    return 1;
}

sint32 ai_sort_racers_by_rank(uint32 first, uint32 second)
{
    sint16 index;
    sint32 count;

    FUNCTION_MARKER(0x8002D888u, "MAIN.EXE");
    if (first == 0u)
        return 1;

    count = (sint32)vehicle_racer_count;
    if (count > 1)
    {
        index = 1;
        do
        {
            BOAT **left_slot = vehicle_racers + ((sint32)index - 1);
            BOAT **right_slot = left_slot + 1;
            BOAT *left = *(left_slot);
            BOAT *right = *(right_slot);

            if ((sint16)((uint16)left->race.progress - (uint16)right->race.progress) < 0)
            {
                *(left_slot) = right;
                *(right_slot) = left;
                if (index >= 2)
                    --index;
            }
            else
                ++index;
            count = (sint32)vehicle_racer_count;
        } while ((sint32)index < count);
    }

    if (second == 0u)
    {
        BOAT *target;

        count = (sint32)vehicle_racer_count;
        if (count <= 0)
            return 0;
        index = 0;
        target = vehicle_player(first);
        do
        {
            BOAT *state = *(vehicle_racers + ((sint32)index));

            if (state == target)
            {
                uint32 menu = r_u32(first + 100u);

                w_u8(menu + 14u, (uint8)index);
                return (sint32)menu;
            }
            ++index;
        } while ((sint32)index < count);
        return (sint32)((uint32)(uint16)index << 16);
    }

    count = (sint32)vehicle_racer_count;
    if (count <= 0)
        return count;
    index = 0;
    do
    {
        BOAT *state = *(vehicle_racers + ((sint32)index));

        if (state == vehicle_player(first))
        {
            uint32 menu = r_u32(first + 100u);

            w_u8(menu + 14u, (uint8)index);
        }
        else if (state == vehicle_player(second))
        {
            uint32 menu = r_u32(second + 100u);

            w_u8(menu + 14u, (uint8)index);
        }
        ++index;
        count = (sint32)vehicle_racer_count;
    } while ((sint32)index < count);
    return (sint32)((uint32)(uint16)index << 16);
}

sint32 ai_update_racer(uint32 entry)
{
    BOAT *candidates[2] = {NULL, NULL};
    uint8 bindings[2];
    uint32 config;
    BOAT **list_slot;
    BOAT *state;
    BOAT *adjacent;
    BOAT *candidate;
    BOAT **candidate_slot;
    BOAT *tracked;
    uint32 value;
    uint32 other_value;
    sint32 count;
    sint32 rank;
    sint32 index;
    sint32 candidate_index;

    FUNCTION_MARKER(0x8002DA2Cu, "MAIN.EXE");
    count = (sint32)vehicle_racer_count;
    index = 0;
    while (index < count)
    {
        list_slot = vehicle_racers + ((sint32)(sint16)index);
        state = *(list_slot);
        if ((sint16)(uint16)state->race.phase == 2)
            state->race.phase = (uint16)(1u);
        else
            state->race.phase = (uint16)(2u);
        index = (sint16)(index + 1);
        value = (uint32)state->route.ticks;
        state->route.ticks = (uint32)(value + 1u);
        count = (sint32)vehicle_racer_count;
    }
    ai_sort_racers_by_rank(entry, 0u);
    ai_update_trailing_pool(entry);
    state = vehicle_player(entry);
    state->route.distance = (sint32)(0u);
    state->route.link_distance = (sint32)(0u);
    config = r_u32(entry + 100u);
    rank = (sint16)((sint32)r_u8(config + 14u) - 1);
    index = rank;
    candidate_index = 0;
    while (index >= 0)
    {
        list_slot = vehicle_racers + ((sint32)(sint16)index);
        state = *(list_slot);
        state->route.direction = (uint16)(1u);
        state = *(list_slot);
        adjacent = *(list_slot + 1);
        value = (uint32)vec_dist_sq(state->motion.position, adjacent->motion.position);
        state = *(list_slot);
        state->route.link_distance = (sint32)(value);
        state = *(list_slot);
        adjacent = *(list_slot + 1);
        value = (uint32)state->route.link_distance;
        other_value = (uint32)adjacent->route.distance;
        state->route.distance = (sint32)(value + other_value);
        if ((sint16)candidate_index < 2)
        {
            candidate_slot = candidates + (sint32)(sint16)candidate_index;
            candidate = *candidate_slot;
            if (candidate == 0u)
            {
                state = *(list_slot);
                *candidate_slot = state;
            }
            else
            {
                state = *(list_slot);
                value = (uint32)candidate->route.distance;
                other_value = (uint32)state->route.distance;
                if ((sint32)other_value < (sint32)value)
                    *candidate_slot = state;
            }
        }
        index = (sint16)(index - 1);
        candidate_index = (sint16)(candidate_index + 1);
    }

    config = r_u32(entry + 100u);
    index = (sint16)((sint32)r_u8(config + 14u) + 1);
    count = (sint32)vehicle_racer_count;
    candidate_index = 0;
    while (index < count)
    {
        list_slot = vehicle_racers + ((sint32)(sint16)index);
        state = *(list_slot);
        state->route.direction = (uint16)(2u);
        adjacent = *(list_slot - 1);
        state = *(list_slot);
        value = (uint32)vec_dist_sq(adjacent->motion.position, state->motion.position);
        state = *(list_slot);
        state->route.link_distance = (sint32)(value);
        state = *(list_slot);
        adjacent = *(list_slot - 1);
        value = (uint32)state->route.link_distance;
        other_value = (uint32)adjacent->route.distance;
        state->route.distance = (sint32)(value + other_value);
        if ((sint16)candidate_index < 2)
        {
            candidate_slot = candidates + (sint32)(sint16)candidate_index;
            candidate = *candidate_slot;
            if (candidate == 0u)
            {
                state = *(list_slot);
                *candidate_slot = state;
            }
            else
            {
                state = *(list_slot);
                value = (uint32)candidate->route.distance;
                other_value = (uint32)state->route.distance;
                if ((sint32)other_value < (sint32)value)
                {
                    if ((sint16)candidate_index == 0)
                    {
                        candidates[1] = candidates[0];
                    }
                    state = *(list_slot);
                    *candidate_slot = state;
                }
            }
        }
        index = (sint16)(index + 1);
        candidate_index = (sint16)(candidate_index + 1);
        count = (sint32)vehicle_racer_count;
    }

    for (index = 0; index < 2; ++index)
    {
        sint32 owner = 0;

        bindings[index] = 5u;
        while (owner < 2)
        {
            candidate = candidates[index];
            tracked = vehicle_tracks(entry)[(uint32)owner];
            if (candidate == tracked)
            {
                bindings[index] = (uint8)owner;
                break;
            }
            owner = (sint16)(owner + 1);
        }
    }
    {
        sint32 previous_index = 1;

        for (index = 0; index < 2; ++index)
        {
            if (bindings[index] == 5u)
            {
                sint32 selected_index;
                sint32 delta;

                value = bindings[previous_index];
                if (value == 1u)
                    selected_index = 0;
                else if (value != 0u)
                    selected_index = index;
                else
                    selected_index = 1;
                tracked = vehicle_tracks(entry)[(uint32)(uint8)selected_index];
                candidate = candidates[index];
                value = (uint32)tracked->route.distance;
                other_value = (uint32)candidate->route.distance;
                delta = (sint32)(value - other_value);
                if (delta < 0)
                    delta = (sint32)(0u - (uint32)delta);
                if (delta > 90000 && (sint16)(uint16)tracked->race.phase == 2)
                {
                    if ((sint16)(uint16)candidate->race.phase == 1)
                    {
                        ai_random_mode(tracked);
                        vehicle_tracks(entry)[(uint32)(uint8)selected_index]->race.phase = (uint16)(1u);
                    }
                    else
                    {
                        tracked->race.mode = (uint16)(6u);
                        vehicle_tracks(entry)[(uint32)(uint8)selected_index]->race.phase = (uint16)(2u);
                    }
                    candidate_slot = candidates + index;
                    candidate = *candidate_slot;
                    candidate->race.phase = (uint16)(2u);
                    candidate = *candidate_slot;
                    candidate->race.mode = (uint16)(5u);
                    candidate = *candidate_slot;
                    vehicle_init_motion(candidate);
                    candidate = *candidate_slot;
                    vehicle_tracks(entry)[(uint32)(uint8)selected_index] = candidate;
                    bindings[index] = (uint8)selected_index;
                }
            }
            previous_index = index;
        }
    }

    config = r_u32(entry + 100u);
    index = (sint16)((sint32)r_u8(config + 14u) - 1);
    while (index >= 0)
    {
        sint32 mode;
        sint32 target;

        list_slot = vehicle_racers + ((sint32)(sint16)index);
        state = *(list_slot);
        mode = (sint16)(uint16)state->race.mode;
        if (mode == 3 || mode == 5)
        {
            target = (sint32)(0x009C4000u - (uint32)state->route.link_distance);

            if (target < 0)
                value = (uint32)math_sra_s32((uint32)target + 2047u, 11u);
            else if (target > 0)
                value = (uint32)math_sra_s32((uint32)target, 10u);
            else
                value = 0u;
            state->route.speed_adjust = (uint16)((uint16)value);
            target = (sint16)(uint16)state->route.speed_adjust + 7000;
            state->route.speed = (sint32)((uint32)target);
            if (target < 3500)
                state->route.speed = (sint32)(3500u);
            else if (target >= 14001)
                state->route.speed = (sint32)(14000u);
        }
        index = (sint16)(index - 1);
    }

    config = r_u32(entry + 100u);
    index = (sint16)((sint32)r_u8(config + 14u) + 1);
    count = (sint32)vehicle_racer_count;
    candidate_index = 0;
    while (index < count)
    {
        sint32 mode;
        sint32 target;

        list_slot = vehicle_racers + ((sint32)(sint16)index);
        state = *(list_slot);
        mode = (sint16)(uint16)state->race.mode;
        if (mode == 3 || mode == 5)
        {
            if ((sint16)candidate_index == 0)
            {
                value = (uint32)state->route.distance;
                candidate_index = (sint16)(candidate_index + 1);
            }
            else
                value = (uint32)state->route.link_distance;
            target = (sint32)(value + 0xFF63C000u);

            if (target > 0)
                value = (uint32)math_sra_s32((uint32)target, 11u);
            else if (target < 0)
                value = (uint32)math_sra_s32((uint32)target + 1023u, 10u);
            else
                value = 0u;
            state->route.speed_adjust = (uint16)((uint16)value);
            target = (sint16)(uint16)state->route.speed_adjust + 7000;
            state->route.speed = (sint32)((uint32)target);
            if (target < 0)
                state->route.speed = (sint32)(0u);
            else if (target >= 14001)
                state->route.speed = (sint32)(14000u);
        }
        index = (sint16)(index + 1);
        count = (sint32)vehicle_racer_count;
    }
    return 0;
}

sint32 ai_random_mode(BOAT *boat)
{
    const ROUTE_SEGMENT *seg;
    sint32 segment;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002E33Cu, "MAIN.EXE");
    boat->route.speed = (sint32)(1024u);
    next = ai_fn_8002fc28_random();
    seg = boat->contacts.points[0].seg;
    boat->route.behavior = (uint16)((uint16)(12u + ((uint32)next & 1u)));
    boat->route.lane_timer = (uint16)(0u);
    boat->route.lookahead = (uint16)(2u);
    segment = (sint32)seg->next.idx - 1;
    if (segment < 0)
        segment = (sint32)(uint16)seg->prev.idx + 1;
    next = segment + 2;
    boat->route.target_segment = (uint16)((uint16)next);
    next = (uint16)boat->route.target_segment;
    count = (sint32)route_resources.count;
    if (next >= count)
    {
        next = (sint32)((uint32)next - (uint16)route_resources.count);
        boat->route.target_segment = (uint16)((uint16)next);
    }
    boat->route.target_lane = (uint16)(4u);
    boat->route.segment = (uint16)((uint16)segment);
    boat->race.mode = (uint16)(3u);
    boat->route.ticks = (uint32)(0u);
    return 3;
}

sint32 ai_choose_route_target(BOAT *boat)
{
    BOAT_ROUTE_STATE *route = &boat->route;
    BOAT_ROUTE_TARGET *output = &boat->route_target;
    sint32 choices[3];
    sint32 weights[3] = {0};
    ROUTE_SEGMENT *const *table;
    uint32 route_index;
    sint32 current;
    sint32 selected;
    sint32 index;
    const ROUTE_SEGMENT *object;
    sint32 choice;
    sint32 weight;
    sint32 x;
    sint32 z;
    sint32 length;
    sint32 first_value;
    sint32 second_value;

    FUNCTION_MARKER(0x8002E494u, "MAIN.EXE");
    route_index = (uint16)route->target_segment;
    table = route_resources.segments;
    current = (sint16)(uint16)route->target_lane;
    object = table[route_index];
    if (current < 0 || current >= (sint32)object->vertex_count)
    {
        ai_fn_800304fc();
        route->target_lane = (uint16)(4u);
    }
    current = (sint16)(uint16)route->target_lane;
    choices[1] = current;
    choice = (sint32)((uint32)current + 1u);
    choices[2] = choice < (sint32)object->vertex_count ? choice : -1;
    choice = (sint32)((uint32)current - 1u);
    choices[0] = choice >= 0 ? choice : -1;
    for (index = 0; index < 3; ++index)
    {
        weights[index] = 0;
        choice = choices[index];
        if (choice != -1)
        {
            const ROUTE_VERTEX *vertex = &object->vertices[(uint32)choice];
            uint8 type;

            type = vertex->position[0];
            if (type == 255u && vertex->position[2] == type && vertex->position[1] == type)
            {
                choices[index] = -1;
                continue;
            }
            type = vertex->lane;
            if (type == 2u)
            {
                weight = weights[index];
                weight = (sint32)((uint32)weight + 2u);
            }
            else if (type == 1u)
            {
                weight = weights[index];
                weight = (sint32)((uint32)weight + 1u);
            }
            else if (type == 3u)
            {
                weight = weights[index];
                weight = (sint32)((uint32)weight + 4u);
            }
            else
                weight = 0;
            weights[index] = weight;
        }
    }
    {
        uint16 timer = (uint16)((uint16)route->lane_timer + 1u);

        route->lane_timer = (uint16)(timer);
        if ((sint16)timer == 40)
        {
            sint32 mode = (sint16)(uint16)route->behavior;
            sint32 randomize = 0;
            sint32 weight_index = 0;

            if (mode == 13 && choices[2] >= 2)
            {
                randomize = 1;
                weight_index = 2;
            }
            else if (mode == 12 && choices[0] >= 2)
            {
                randomize = 1;
                weight_index = 0;
            }
            if (randomize != 0)
            {
                sint32 random = global_fn_8006e9d8();

                weight = weights[weight_index];
                weights[weight_index] = (sint32)((uint32)weight + ((uint32)random & 1u));
                random = global_fn_8006e9d8();
                route->behavior = (uint16)((uint16)(((uint32)random & 1u) + 12u));
            }
            route->lane_timer = (uint16)(0u);
        }
    }
    {
        sint32 first_weight = weights[0];
        sint32 third_weight = weights[2];

        if (third_weight < first_weight)
        {
            sint32 middle_weight = weights[1];

            selected = middle_weight < first_weight ? 0 : 1;
        }
        else if (first_weight < third_weight)
        {
            sint32 middle_weight = weights[1];

            selected = middle_weight < third_weight ? 2 : 1;
        }
        else
        {
            sint32 middle_weight = weights[1];

            selected = 1;
            if (middle_weight <= 0 && choices[1] == -1)
            {
                selected = 0;
                if (choices[0] == -1)
                {
                    selected = 2;
                    if (choices[2] == -1)
                    {
                        output->vector[2] = (sint32)(0u);
                        output->vector[1] = (sint32)(0u);
                        output->vector[0] = (sint32)(0u);
                        return -1;
                    }
                }
            }
        }
    }
    route_index = (uint16)route->target_segment;
    table = route_resources.segments;
    object = table[route_index];
    choice = choices[selected];
    if (choice == -1)
    {
        output->vector[2] = (sint32)(0u);
        output->vector[1] = (sint32)(0u);
        output->vector[0] = (sint32)(0u);
        return (sint32)((uint32)choice << 3);
    }
    {
        const ROUTE_VERTEX *vertex = &object->vertices[(uint32)choice];
        uint32 coordinate = vertex->position[0];
        sint32 base = object->origin[0];
        uint32 position = (uint32)boat->contacts.points[0].position[0];
        sint32 first_square;
        sint32 second_square;

        x = (sint32)((uint32)base + (coordinate << 4) - position);
        output->vector[0] = (sint32)((uint32)x);
        coordinate = vertex->position[2];
        base = object->origin[2];
        position = (uint32)boat->contacts.points[0].position[2];
        x = (sint32)(uint32)output->vector[0];
        z = (sint32)((uint32)base + (coordinate << 4) - position);
        output->vector[2] = (sint32)((uint32)z);
        if ((uint32)x + 8096u >= 16193u || (uint32)z + 8096u >= 16193u)
        {
            output->vector[2] = (sint32)(0u);
            output->vector[1] = (sint32)(0u);
            output->vector[0] = (sint32)(0u);
            return 0;
        }
        first_square = (sint32)((uint32)x * (uint32)x);
        second_square = (sint32)((uint32)z * (uint32)z);
        length = (sint32)SquareRoot0((sint32)((uint32)first_square + (uint32)second_square));
        output->length = (sint32)((uint32)length);
        if (length == 0)
        {
            output->vector[2] = (sint32)(0u);
            output->vector[0] = (sint32)(0u);
        }
        else
        {
            first_value = (sint32)(uint32)output->vector[0];
            first_value = math_div_s32((sint32)((uint32)first_value << 12), length);
            second_value = (sint32)(uint32)output->vector[2];
            length = (sint32)(uint32)output->length;
            second_value = math_div_s32((sint32)((uint32)second_value << 12), length);
            output->vector[0] = (sint32)((uint32)first_value);
            output->vector[2] = (sint32)((uint32)second_value);
        }
    }
    choice = (uint16)choices[selected];
    route->target_lane = (uint16)((uint16)choice);
    return choice;
}

sint32 ai_config_template_desc(sint32 idx, uint32 variation)
{
    sint32 selected = variation >= 5u ? 4 : (sint32)variation;
    sint32 row;
    uint32 mode;
    uint8 profile = profile_selection.slot;
    const AI_CONTROL_TEMPLATE *src;

    FUNCTION_MARKER(0x8002F8D4u, "MAIN.EXE");
    if ((uint32)idx >= 7u || profile >= 3u)
        abort();
    w_u32(0x800B6820u, 0u);
    mode = r_u32(0x8008348Cu);
    row = (mode == 1u ? 0 : mode == 2u ? 3 : 6) + profile;
    if (r_u32(0x800834A0u) == 6u)
    {
        selected = profile;
        row = selected;
    }
    src = &ai_control_templates[idx];
    ai_control_cfg.lookahead = src->lookahead;
    ai_control_cfg.max_speed = ai_control_speeds[idx][row][0];
    ai_control_cfg.target_speed = ai_control_speeds[idx][row][1];
    return (sint32)((uint32)src->return_base + (uint32)(src->return_step * selected));
}

sint32 ai_init_race(BOAT *boat, sint16 segment, sint32 cfg_idx)
{
    uint32 player_index = r_u32(0x800834A0u);
    const AI_ROUTE_TUNING *tuning = &ai_route_tuning[player_index];
    AI_ROUTE_CFG *cfg;
    sint32 variant = player_index == 6u ? profile_selection.slot + 3 : race_selection.ai_variant;
    sint32 result;

    FUNCTION_MARKER(0x8002FA94u, "MAIN.EXE");
    if (r_u32(0x80083478u) == 1u)
    {
        sint32 product;
        uint16 value;

        if ((uint32)cfg_idx >= 9u)
            cfg_idx = 7;
        cfg = &ai_route_cfg[cfg_idx];
        product = tuning->min_step * variant;
        value = (uint16)((uint16)tuning->min_base + (uint32)product);
        cfg->min_adjust = (sint16)value;
        if ((sint16)value > 0)
            cfg->min_adjust = 0;
        product = tuning->max_step * variant;
        value = (uint16)((uint16)tuning->max_base + (uint32)product);
        cfg->max_adjust = (sint16)value;
    }
    else
    {
        uint32 alternate = 1u - ai_settings_alternate;

        cfg = &ai_alt_route_cfg[alternate];
        ai_settings_alternate = alternate;
    }
    boat->control.force_arm[2] = 100;
    boat->control.force_arm[0] = 0;
    boat->control.force_arm[1] = 0;
    boat->route.speed = (sint32)(10240u);
    boat->route.lookahead = (uint16)(2u);
    {
        uint16 advance = (uint16)boat->route.lookahead;
        sint32 object_count;
        uint32 next;

        boat->route.lane_timer = (uint16)(4u);
        object_count = (sint32)route_resources.count;
        boat->route.speed_adjust = (uint16)(0u);
        boat->route.hazard = (uint16)(0u);
        boat->route.behavior = (uint16)(0u);
        boat->route.settings = cfg;
        boat->route.target_segment = (uint16)((uint16)((uint16)segment + advance));
        next = (uint16)boat->route.target_segment;
        if ((sint32)next >= object_count)
            boat->route.target_segment = (uint16)((uint16)(next - (uint16)route_resources.count));
    }
    boat->route.segment = (uint16)((uint16)segment);
    boat->route.target_lane = (uint16)((uint16)boat->contacts.points[0].left);
    result = (sint16)(uint16)boat->motion.transform.pose.m[0][2];
    boat->route.link_distance = (sint32)(0u);
    boat->route.distance = (sint32)(0u);
    boat->route_target.vector[0] = (sint32)((uint32)result);
    boat->route_target.vector[1] = (sint32)((uint32)(sint32)(sint16)(uint16)boat->motion.transform.pose.m[1][2]);
    result = (sint16)(uint16)boat->motion.transform.pose.m[2][2];
    boat->route_target.length = (sint32)(0u);
    boat->route.steering = (uint16)(0u);
    boat->route_target.vector[2] = (sint32)((uint32)result);
    return result;
}

sint32 ai_assign_ctrl_states(uint32 group, uint32 second_group)
{
    BOAT *nearest = NULL;
    BOAT *next_nearest = NULL;
    uint32 mode = r_u32(0x80083478u);
    sint32 first_gap = 4096;
    sint32 second_gap = 4096;
    sint32 count;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8002FC28u, "MAIN.EXE");
    if (mode == 1u)
    {
        sint32 phase = 2;

        vehicle_player(group)->race.phase = (uint16)(1u);
        vehicle_player(group)->race.mode = (uint16)(4u);
        count = (sint32)vehicle_racer_count;
        index = 0;
        while (index < count)
        {
            BOAT *state = vehicle_racers[index];
            if (state != vehicle_player(group))
            {
                BOAT *primary;
                sint32 gap;

                state->race.phase = (uint16)(0u);
                state->race.mode = (uint16)(3u);
                primary = vehicle_player(group);
                gap = (sint32)((uint32)state->race.progress - (uint32)primary->race.progress);
                if (gap < 0)
                    gap = (sint32)(0u - (uint32)gap);
                if (gap < first_gap)
                {
                    second_gap = first_gap;
                    next_nearest = nearest;
                    first_gap = gap;
                    nearest = state;
                }
                else if (gap < second_gap)
                {
                    second_gap = gap;
                    next_nearest = state;
                }
            }
            ++index;
            count = (sint32)vehicle_racer_count;
        }
        if (nearest != NULL)
        {
            nearest->race.phase = (uint16)(1u);
            nearest->race.mode = (uint16)(4u);
        }
        if (next_nearest != NULL)
        {
            next_nearest->race.phase = (uint16)(2u);
            next_nearest->race.mode = (uint16)(4u);
        }
        vehicle_tracks(group)[0] = nearest;
        vehicle_tracks(group)[1] = next_nearest;
        count = (sint32)vehicle_racer_count;
        index = 0;
        while (index < count)
        {
            BOAT **entry = vehicle_racers + (index);
            BOAT *state = *entry;

            *(entry) = state;
            if ((sint16)(uint16)state->race.phase == 0)
            {
                state->race.phase = (uint16)((uint16)phase);
                phase = phase == 2 ? 1 : 2;
                state->route.behavior = (uint16)((uint16)(12 + (ai_fn_8002fc28_random() & 1)));
                state->route.lane_timer = (uint16)((uint16)(ai_fn_8002fc28_random() & 31));
            }
            ++index;
            count = (sint32)vehicle_racer_count;
        }
        result = 2;
        if (vehicle_menu(r_u32(group + 100u))->mode == 2u)
        {
            count = (sint32)vehicle_racer_count;
            result = count;
            index = 0;
            while (index < count)
            {
                (*(vehicle_racers + (index)))->race.mode = (uint16)(7u);
                ++index;
                count = (sint32)vehicle_racer_count;
                result = index < count;
            }
        }
    }
    else
    {
        BOAT *primary = vehicle_player(group);
        BOAT *peer = vehicle_player(second_group);
        sint32 slot = 0;

        peer->race.phase = (uint16)(1u);
        primary->race.phase = (uint16)(1u);
        count = (sint32)vehicle_racer_count;
        result = count;
        index = 0;
        while (index < count)
        {
            BOAT *state = vehicle_racers[index];
            state->race.mode = (uint16)(4u);
            if (state != vehicle_player(group) && state != vehicle_player(second_group))
            {
                state->race.phase = (uint16)(2u);
                vehicle_tracks(second_group)[(uint32)slot] = state;
                vehicle_tracks(group)[(uint32)slot] = state;
                ++slot;
            }
            ++index;
            count = (sint32)vehicle_racer_count;
            result = index < count;
        }
    }
    return result;
}

void ai_fn_800304fc(void)
{
    FUNCTION_MARKER(0x800304FCu, "MAIN.EXE");
}
