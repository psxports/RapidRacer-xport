#include "game.h"
#include "profile.h"
#include "ai.h"
#include "utils.h"
#include "route.h"
#include "boat.h"
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
    {8, 128, -2}, {6, 128, -2}, {8, 100, -2}, {8, 108, -2}, {8, 128, -2}, {7, 128, -2}, {8, 128, -2},
};
static const uint16 ai_control_speeds[7][9][2] = {
    {{29020, 1300}, {29520, 2200}, {30020, 2900}, {29520, 2000}, {31020, 2900}, {32020, 4000}, {30020, 2200}, {31520, 3000}, {32020, 4000}}, {{26320, 2200}, {27020, 2950}, {27520, 3300}, {27020, 2150}, {27520, 2450}, {28020, 3100}, {27520, 2300}, {28020, 3150}, {28520, 3300}}, {{26520, 2500}, {27020, 2950}, {27520, 3500}, {26520, 2300}, {26520, 2400}, {33020, 4100}, {26520, 2600}, {27520, 3250}, {28020, 4100}}, {{29220, 1600}, {30520, 2200}, {31020, 2700}, {30520, 2000}, {31020, 2500}, {32020, 3600}, {30520, 2100}, {32020, 2600}, {33020, 3600}}, {{27620, 2300}, {28170, 2800}, {28620, 3600}, {28170, 3300}, {28620, 3400}, {29620, 4200}, {28170, 3400}, {29620, 3500}, {30620, 4200}}, {{35520, 2600}, {40520, 3750}, {41070, 4200}, {41520, 3400}, {42520, 3850}, {43520, 4400}, {41520, 3500}, {42020, 3950}, {42520, 4000}}, {{40020, 3800}, {40020, 4000}, {40020, 4200}, {26320, 2200}, {26820, 2700}, {27320, 3200}, {26820, 2700}, {27320, 3200}, {27820, 3700}},
};

// Mutable route settings and immutable tuning from MAIN.EXE
static AI_ROUTE_CFG ai_route_cfg[9] = {{100, 1100, 350, -16, 384, 128, 4, 2, 4, 4, 5400}, {100, 1100, 350, -32, 512, 156, 1, 4, 3, 2, 5300}, {50, 1600, 256, -44, 256, 128, 4, 3, 4, 4, 5400}, {50, 1024, 128, 0, 256, 48, 2, 2, 3, 2, 5300}, {300, 1600, 350, -38, 256, 156, 2, 3, 3, 4, 5400}, {50, 600, 400, -32, 384, 106, 2, 2, 4, 3, 5400}, {100, 1100, 350, -16, 384, 156, 1, 3, 4, 2, 5500}, {150, 1100, 250, 0, 256, 64, 1, 3, 4, 4, 5500}, {150, 1100, 250, 0, 256, 64, 1, 3, 4, 1, 5600}};
static AI_ROUTE_CFG ai_alt_route_cfg[2] = {{230, 1200, 600, -2024, 2024, 128, 2, 3, 3, 3, 5100}, {500, 800, 500, -2024, 2024, 128, 1, 3, 1, 3, 5400}};

typedef struct
{
    sint16 min_base, max_base, min_step, max_step;
} AI_ROUTE_TUNING;

static const AI_ROUTE_TUNING ai_route_tuning[7] = {{-32, 1024, 8, 4}, {-64, 1256, 16, 4}, {-88, 768, 16, 4}, {0, 768, 0, 4}, {-76, 768, 16, 4}, {-64, 768, 16, 4}, {-32, 1024, 8, 4}};

static sint32 ai_place(BOAT *boat, sint16 seg_idx)
{
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002D714, "MAIN.EXE");
    route_contact_init(&boat->contacts.points[0], route_resources.segs[seg_idx], 4);

    boat->contacts.points[0].n.vx = 0;
    boat->contacts.points[1].n.vx = 0;
    boat->contacts.points[0].n.vz = 0;
    boat->contacts.points[1].n.vz = 0;
    boat->pos.x = boat->contacts.points[0].pos.x - boat->contacts.points[0].sample.vx;
    boat->pos.y = boat->contacts.points[0].pos.y - boat->contacts.points[0].sample.vy;
    boat->pos.z = boat->contacts.points[0].pos.z - boat->contacts.points[0].sample.vz;
    boat->contacts.points[0].n.vy = ONE;
    boat->contacts.points[1].n.vy = ONE;
    boat->pose.t[0] = boat->pos.x;
    boat->pose.t[1] = boat->pos.y;
    boat->pose.t[2] = boat->pos.z;
    next = random_next();
    boat->route.behavior = 12 + (next & 1);
    boat->route.lookahead = 2;
    next = seg_idx + 2u;
    boat->route.target_seg = next;
    next = boat->route.target_seg;
    count = route_resources.count;
    boat->route.lane_timer = 0;
    if (next >= count)
    {
        next = next - (uint16)route_resources.count;
        boat->route.target_seg = next;
    }
    boat->race.mode = 3;
    boat->route.seg = seg_idx;
    boat->route.speed = 0;
    boat->route_target.vector.x = 0;
    boat->route_target.vector.y = 0;
    boat->route_target.vector.z = 0;
    boat->route_target.len = 0;
    boat->race.phase = 1;
    boat->route.ticks = 0;
    boat->route.target_lane = boat->contacts.points[0].left;
    return 1;
}

static sint32 ai_update_trailing_pool(const RACE_VIEW *entry)
{
    BOAT_MENU *menu;
    BOAT **list_slot;
    BOAT *state;
    BOAT *adjacent;
    uint32 val;
    sint32 rank;
    sint32 count;
    sint32 distant;
    sint32 idx;
    sint32 result;
    sint32 numerator;
    sint32 divisor;
    sint32 remainder;

    FUNCTION_MARKER(0x8002D324, "MAIN.EXE");
    menu = boat_player_menu(entry);
    rank = menu->rank;
    idx = rank + 1;
    list_slot = boat_racers + rank;
    state = *list_slot;
    count = boat_racer_count;
    val = state->race.progress;
    distant = 0;
    while (idx < count)
    {
        sint32 mode;

        list_slot = boat_racers + idx;
        state = *list_slot;
        mode = state->race.mode;
        if (mode == 3)
        {
            sint32 gap = val - state->race.progress;

            if (gap >= 4)
                distant++;
        }
        idx++;
    }

    result = distant < 2;
    if (distant < 2)
    {
        if (boat_trailer_count <= 0)
        {
            result = distant < 3;
            goto done;
        }

        state = *boat_racers;
        result = state->race.phase | (uint32)state->race.mode << 16;
        if (result != 0x00030001)
            goto done;

        idx = boat_leader_count;
        state = *boat_racers;
        *(boat_leaders + idx) = state;
        val = boat_leader_count + 1u;
        count = boat_racer_count;
        boat_leader_count = val;
        idx = 1;
        if (idx < count)
        {
            BOAT **dst = boat_racers;
            BOAT **src = dst + 1;

            do
            {
                state = *src;
                src++;
                idx++;
                *dst = state;
                count = boat_racer_count;
                dst++;
            } while (idx < count);
        }

        val = boat_trailer_count - 1u;
        boat_trailer_count = val;
        idx = (sint16)val;
        count = boat_racer_count;
        state = *(boat_trailers + idx);
        *(boat_racers + (count - 1)) = state;
        count = boat_racer_count;
        adjacent = *(boat_racers + (count - 2));
        numerator = adjacent->race.progress - 20u;
        divisor = route_resources.count;

        remainder = numerator % divisor;
        state->race.progress = numerator;
        if (remainder < 0)
            remainder = remainder + divisor;
        ai_place(state, remainder);

        menu = boat_player_menu(entry);
        menu->rank = menu->rank - 1u;
        menu = boat_player_menu(entry);
        result = menu->prev_rank - 1u;
        menu->prev_rank = result;
        goto done;
    }

    result = distant < 3;
    if (result != 0)
        goto done;
    result = boat_leader_count;
    if (result <= 0)
        goto done;

    count = boat_racer_count;
    state = *(boat_racers + (count - 1));
    result = state->race.phase | (uint32)state->race.mode << 16;
    if (result != 0x00030001)
        goto done;

    idx = boat_trailer_count;
    *(boat_trailers + idx) = state;
    val = boat_trailer_count + 1u;
    count = boat_racer_count;
    idx = count - 1;
    boat_trailer_count = val;
    if (idx > 0)
    {
        BOAT **src = boat_racers + (idx - 1);
        BOAT **dst = src + 1;

        do
        {
            state = *src;
            src--;
            idx--;
            *dst = state;
            dst--;
        } while (idx > 0);
    }

    val = boat_leader_count - 1u;
    adjacent = *(boat_racers + 1);
    boat_leader_count = val;
    idx = (sint16)val;
    state = *(boat_leaders + idx);
    *boat_racers = state;
    numerator = adjacent->race.progress + 20u;
    state = *boat_racers;
    state->race.progress = numerator;
    state = *boat_racers;
    divisor = route_resources.count;
    remainder = state->race.progress % divisor;
    if (remainder < 0)
        remainder = remainder + divisor;
    state = *boat_racers;
    ai_place(state, remainder);

    menu = boat_player_menu(entry);
    menu->rank = menu->rank + 1u;
    menu = boat_player_menu(entry);
    result = menu->prev_rank + 1u;
    menu->prev_rank = result;

done:
    return result;
}

void ai_sort_racers_by_rank(const RACE_VIEW *view0, const RACE_VIEW *view1)
{
    sint16 idx;
    sint32 count;

    FUNCTION_MARKER(0x8002D888, "MAIN.EXE");
    if (view0 == NULL)
        return;

    count = boat_racer_count;
    if (count > 1)
    {
        idx = 1;
        do
        {
            BOAT **left_slot = boat_racers + (idx - 1);
            BOAT **right_slot = left_slot + 1;
            BOAT *left = *left_slot;
            BOAT *right = *right_slot;

            if ((sint16)((uint16)left->race.progress - (uint16)right->race.progress) < 0)
            {
                *left_slot = right;
                *right_slot = left;
                if (idx >= 2)
                    --idx;
            }
            else
                ++idx;
            count = boat_racer_count;
        } while (idx < count);
    }

    if (view1 == NULL)
    {
        BOAT *target;

        count = boat_racer_count;
        if (count <= 0)
            return;
        idx = 0;
        target = boat_player(view0);
        do
        {
            BOAT *state = *(boat_racers + idx);

            if (state == target)
            {
                BOAT_MENU *menu = boat_player_menu(view0);

                menu->rank = idx;
                return;
            }
            ++idx;
        } while (idx < count);
        return;
    }

    count = boat_racer_count;
    if (count <= 0)
        return;
    idx = 0;
    do
    {
        BOAT *state = *(boat_racers + idx);

        if (state == boat_player(view0))
        {
            BOAT_MENU *menu = boat_player_menu(view0);

            menu->rank = idx;
        }
        else if (state == boat_player(view1))
        {
            BOAT_MENU *menu = boat_player_menu(view1);

            menu->rank = idx;
        }
        ++idx;
        count = boat_racer_count;
    } while (idx < count);
    return;
}

sint32 ai_update_racer(const RACE_VIEW *entry)
{
    BOAT *candidates[2] = {NULL, NULL};
    uint8 bindings[2];
    BOAT_MENU *menu;
    BOAT **list_slot;
    BOAT *state;
    BOAT *adjacent;
    BOAT *candidate;
    BOAT **candidate_slot;
    BOAT *tracked;
    sint32 val;
    sint32 other_val;
    sint32 count;
    sint16 idx;
    sint16 candidate_idx;

    FUNCTION_MARKER(0x8002DA2C, "MAIN.EXE");
    count = boat_racer_count;
    idx = 0;
    while (idx < count)
    {
        list_slot = boat_racers + idx;
        state = *list_slot;
        if (state->race.phase == 2)
            state->race.phase = 1;
        else
            state->race.phase = 2;
        idx = idx + 1;
        state->route.ticks++;
        count = boat_racer_count;
    }
    ai_sort_racers_by_rank(entry, 0);
    ai_update_trailing_pool(entry);
    state = boat_player(entry);
    state->route.distance = 0;
    state->route.link_distance = 0;
    menu = boat_player_menu(entry);
    idx = menu->rank - 1;
    candidate_idx = 0;
    while (idx >= 0)
    {
        list_slot = boat_racers + idx;
        state = *list_slot;
        state->route.dir = 1;
        state = *list_slot;
        adjacent = *(list_slot + 1);
        val = vec_dist_sq(&state->pos, &adjacent->pos);
        state = *list_slot;
        state->route.link_distance = val;
        state = *list_slot;
        adjacent = *(list_slot + 1);
        val = state->route.link_distance;
        other_val = adjacent->route.distance;
        state->route.distance = val + other_val;
        if (candidate_idx < 2)
        {
            candidate_slot = candidates + candidate_idx;
            candidate = *candidate_slot;
            if (candidate == 0u)
            {
                state = *list_slot;
                *candidate_slot = state;
            }
            else
            {
                state = *list_slot;
                val = candidate->route.distance;
                other_val = state->route.distance;
                if (other_val < val)
                    *candidate_slot = state;
            }
        }
        idx = idx - 1;
        candidate_idx = candidate_idx + 1;
    }

    menu = boat_player_menu(entry);
    idx = menu->rank + 1;
    count = boat_racer_count;
    candidate_idx = 0;
    while (idx < count)
    {
        list_slot = boat_racers + idx;
        state = *list_slot;
        state->route.dir = 2;
        adjacent = *(list_slot - 1);
        state = *list_slot;
        val = vec_dist_sq(&adjacent->pos, &state->pos);
        state = *list_slot;
        state->route.link_distance = val;
        state = *list_slot;
        adjacent = *(list_slot - 1);
        val = state->route.link_distance;
        other_val = adjacent->route.distance;
        state->route.distance = val + other_val;
        if (candidate_idx < 2)
        {
            candidate_slot = candidates + candidate_idx;
            candidate = *candidate_slot;
            if (candidate == 0u)
            {
                state = *list_slot;
                *candidate_slot = state;
            }
            else
            {
                state = *list_slot;
                val = candidate->route.distance;
                other_val = state->route.distance;
                if (other_val < val)
                {
                    if (candidate_idx == 0)
                    {
                        candidates[1] = candidates[0];
                    }
                    state = *list_slot;
                    *candidate_slot = state;
                }
            }
        }
        idx = idx + 1;
        candidate_idx = candidate_idx + 1;
        count = boat_racer_count;
    }

    for (idx = 0; idx < 2; ++idx)
    {
        sint16 owner = 0;

        bindings[idx] = 5;
        while (owner < 2)
        {
            candidate = candidates[idx];
            tracked = boat_tracks(entry)[owner];
            if (candidate == tracked)
            {
                bindings[idx] = owner;
                break;
            }
            owner = owner + 1;
        }
    }
    {
        sint32 prev_idx = 1;

        for (idx = 0; idx < 2; ++idx)
        {
            if (bindings[idx] == 5)
            {
                uint8 sel_idx;
                sint32 delta;

                val = bindings[prev_idx];
                if (val == 1)
                    sel_idx = 0;
                else if (val != 0)
                    sel_idx = idx;
                else
                    sel_idx = 1;
                tracked = boat_tracks(entry)[sel_idx];
                candidate = candidates[idx];
                val = tracked->route.distance;
                other_val = candidate->route.distance;
                delta = val - other_val;
                delta = abs(delta);
                if (delta > 90000 && tracked->race.phase == 2)
                {
                    if (candidate->race.phase == 1)
                    {
                        ai_random_mode(tracked);
                        boat_tracks(entry)[sel_idx]->race.phase = 1;
                    }
                    else
                    {
                        tracked->race.mode = 6;
                        boat_tracks(entry)[sel_idx]->race.phase = 2;
                    }
                    candidate_slot = candidates + idx;
                    candidate = *candidate_slot;
                    candidate->race.phase = 2;
                    candidate = *candidate_slot;
                    candidate->race.mode = 5;
                    candidate = *candidate_slot;
                    boat_init_motion(candidate);
                    candidate = *candidate_slot;
                    boat_tracks(entry)[sel_idx] = candidate;
                    bindings[idx] = sel_idx;
                }
            }
            prev_idx = idx;
        }
    }

    menu = boat_player_menu(entry);
    idx = menu->rank - 1;
    while (idx >= 0)
    {
        sint32 mode;
        sint32 target;

        list_slot = boat_racers + idx;
        state = *list_slot;
        mode = state->race.mode;
        if (mode == 3 || mode == 5)
        {
            target = 0x009C4000u - state->route.link_distance;

            if (target < 0)
                val = sra(target + 2047u, 11u);
            else if (target > 0)
                val = sra(target, 10u);
            else
                val = 0;
            state->route.speed_adjust = val;
            target = state->route.speed_adjust + 7000;
            state->route.speed = target;
            if (target < 3500)
                state->route.speed = 3500;
            else if (target >= 14001)
                state->route.speed = 14000;
        }
        idx = idx - 1;
    }

    menu = boat_player_menu(entry);
    idx = menu->rank + 1;
    count = boat_racer_count;
    candidate_idx = 0;
    while (idx < count)
    {
        sint32 mode;
        sint32 target;

        list_slot = boat_racers + idx;
        state = *list_slot;
        mode = state->race.mode;
        if (mode == 3 || mode == 5)
        {
            if (candidate_idx == 0)
            {
                val = state->route.distance;
                candidate_idx = candidate_idx + 1;
            }
            else
                val = state->route.link_distance;
            target = val + 0xFF63C000;

            if (target > 0)
                val = sra(target, 11u);
            else if (target < 0)
                val = sra(target + 1023u, 10u);
            else
                val = 0;
            state->route.speed_adjust = val;
            target = state->route.speed_adjust + 7000;
            state->route.speed = target;
            if (target < 0)
                state->route.speed = 0;
            else if (target >= 14001)
                state->route.speed = 14000;
        }
        idx = idx + 1;
        count = boat_racer_count;
    }
    return 0;
}

sint32 ai_random_mode(BOAT *boat)
{
    const ROUTE_SEGMENT *seg;
    sint32 seg_idx;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002E33C, "MAIN.EXE");
    boat->route.speed = 1024;
    next = random_next();
    seg = boat->contacts.points[0].seg;
    boat->route.behavior = 12 + (next & 1);
    boat->route.lane_timer = 0;
    boat->route.lookahead = 2;
    seg_idx = seg->next.idx - 1;
    if (seg_idx < 0)
        seg_idx = seg->prev.idx + 1;
    boat->route.target_seg = seg_idx + 2;
    next = boat->route.target_seg;
    count = route_resources.count;
    if (next >= count)
    {
        next = next - (uint16)route_resources.count;
        boat->route.target_seg = next;
    }
    boat->route.target_lane = 4;
    boat->route.seg = seg_idx;
    boat->race.mode = 3;
    boat->route.ticks = 0;
    return 3;
}

sint32 ai_choose_route_target(BOAT *boat)
{
    BOAT_ROUTE *route = &boat->route;
    BOAT_ROUTE_TARGET *output = &boat->route_target;
    sint32 choices[3];
    sint32 weights[3] = {0};
    ROUTE_SEGMENT *const *table;
    uint32 route_idx;
    sint32 cur;
    sint32 sel;
    sint32 idx;
    const ROUTE_SEGMENT *object;
    sint32 choice;
    sint32 x;
    sint32 z;
    sint32 len;
    sint32 val0;
    sint32 val1;

    FUNCTION_MARKER(0x8002E494, "MAIN.EXE");
    route_idx = route->target_seg;
    table = route_resources.segs;
    cur = route->target_lane;
    object = table[route_idx];
    if (cur < 0 || cur >= object->vertex_count)
    {
        route->target_lane = 4;
    }
    cur = route->target_lane;
    choices[1] = cur;
    choice = cur + 1u;
    choices[2] = choice < object->vertex_count ? choice : -1;
    choice = cur - 1u;
    choices[0] = choice >= 0 ? choice : -1;
    for (idx = 0; idx < 3; ++idx)
    {
        choice = choices[idx];
        if (choice != -1)
        {
            const ROUTE_VERTEX *vertex = &object->vertices[choice];
            uint8 type;

            type = vertex->pos[0];
            if (type == 255 && vertex->pos[2] == type && vertex->pos[1] == type)
            {
                choices[idx] = -1;
                continue;
            }
            type = vertex->lane;
            weights[idx] = type == 1 ? 1 : type == 2 ? 2 : type == 3 ? 4 : 0;
        }
    }
    {
        uint16 timer = route->lane_timer + 1u;

        route->lane_timer = timer;
        if (timer == 40)
        {
            sint16 mode = route->behavior;
            sint32 randomize = 0;
            sint32 weight_idx = 0;

            if (mode == 13 && choices[2] >= 2)
            {
                randomize = 1;
                weight_idx = 2;
            }
            else if (mode == 12 && choices[0] >= 2)
            {
                randomize = 1;
                weight_idx = 0;
            }
            if (randomize != 0)
            {
                sint32 random = random_next();

                weights[weight_idx] += random & 1;
                random = random_next();
                route->behavior = (random & 1) + 12;
            }
            route->lane_timer = 0;
        }
    }
    sint32 weight0 = weights[0];
    sint32 weight2 = weights[2];

    if (weight2 < weight0)
    {
        sel = weights[1] < weight0 ? 0 : 1;
    }
    else if (weight0 < weight2)
    {
        sel = weights[1] < weight2 ? 2 : 1;
    }
    else
    {
        sint32 middle_weight = weights[1];

        sel = 1;
        if (middle_weight <= 0 && choices[1] == -1)
        {
            sel = 0;
            if (choices[0] == -1)
            {
                sel = 2;
                if (choices[2] == -1)
                {
                    output->vector.z = 0;
                    output->vector.y = 0;
                    output->vector.x = 0;
                    return -1;
                }
            }
        }
    }
    choice = choices[sel];
    if (choice == -1)
    {
        output->vector.z = 0;
        output->vector.y = 0;
        output->vector.x = 0;
        return (uint32)choice << 3;
    }
    {
        const ROUTE_VERTEX *vertex = &object->vertices[choice];
        uint32 coord = vertex->pos[0];
        sint32 base = object->origin[0];
        uint32 pos = boat->contacts.points[0].pos.x;
        sint32 sq0;
        sint32 sq1;

        x = base + (coord << 4) - pos;
        output->vector.x = x;
        coord = vertex->pos[2];
        base = object->origin[2];
        pos = boat->contacts.points[0].pos.z;
        x = output->vector.x;
        z = base + (coord << 4) - pos;
        output->vector.z = z;
        if (x + 8096u >= 16193u || z + 8096u >= 16193u)
        {
            output->vector.z = 0;
            output->vector.y = 0;
            output->vector.x = 0;
            return 0;
        }
        sq0 = x * x;
        sq1 = z * z;
        len = SquareRoot0(sq0 + sq1);
        output->len = len;
        if (len == 0)
        {
            output->vector.z = 0;
            output->vector.x = 0;
        }
        else
        {
            val0 = output->vector.x;
            val0 = (sint32)((uint32)val0 << 12) / len;
            val1 = output->vector.z;
            val1 = (sint32)((uint32)val1 << 12) / output->len;
            output->vector.x = val0;
            output->vector.z = val1;
        }
    }
    choice = (uint16)choices[sel];
    route->target_lane = choice;
    return choice;
}

sint32 ai_config_template_desc(sint32 idx, uint32 variation)
{
    sint32 sel = variation >= 5 ? 4 : variation;
    sint32 row;
    uint32 mode;
    uint8 profile = profile_selection.slot;
    const AI_CONTROL_TEMPLATE *src;

    FUNCTION_MARKER(0x8002F8D4, "MAIN.EXE");
    if ((uint32)idx >= 7 || profile >= 3u)
        abort();
    mode = race_cfg.variant;
    row = (mode == 1 ? 0 : mode == 2 ? 3 : 6) + profile;
    if (race_cfg.course == 6)
    {
        sel = profile;
        row = sel;
    }
    src = &ai_control_templates[idx];
    ai_control_cfg.lookahead = src->lookahead;
    ai_control_cfg.max_speed = ai_control_speeds[idx][row][0];
    ai_control_cfg.target_speed = ai_control_speeds[idx][row][1];
    return src->return_base + src->return_step * sel;
}

sint32 ai_init_race(BOAT *boat, sint16 seg_idx, sint32 cfg_idx)
{
    uint32 player_idx = race_cfg.course;
    const AI_ROUTE_TUNING *tuning = &ai_route_tuning[player_idx];
    AI_ROUTE_CFG *cfg;
    sint32 variant = player_idx == 6 ? profile_selection.slot + 3 : race_selection.ai_variant;
    sint32 result;

    FUNCTION_MARKER(0x8002FA94, "MAIN.EXE");
    if (race_cfg.players == 1)
    {
        uint16 val;

        if ((uint32)cfg_idx >= 9)
            cfg_idx = 7;
        cfg = &ai_route_cfg[cfg_idx];
        val = tuning->min_base + tuning->min_step * variant;
        cfg->min_adjust = val;
        if ((sint16)val > 0)
            cfg->min_adjust = 0;
        val = tuning->max_base + tuning->max_step * variant;
        cfg->max_adjust = val;
    }
    else
    {
        uint32 alternate = 1 - ai_settings_alternate;

        cfg = &ai_alt_route_cfg[alternate];
        ai_settings_alternate = alternate;
    }
    boat->control.force_arm.vz = 100;
    boat->control.force_arm.vx = 0;
    boat->control.force_arm.vy = 0;
    boat->route.speed = 10240;
    boat->route.lookahead = 2;
    uint16 advance = boat->route.lookahead;
    sint32 object_count;
    sint32 next;

    boat->route.lane_timer = 4;
    object_count = route_resources.count;
    boat->route.speed_adjust = 0;
    boat->route.hazard = 0;
    boat->route.behavior = 0;
    boat->route.settings = cfg;
    boat->route.target_seg = seg_idx + advance;
    next = boat->route.target_seg;
    if (next >= object_count)
        boat->route.target_seg = next - route_resources.count;
    boat->route.seg = seg_idx;
    boat->route.target_lane = boat->contacts.points[0].left;
    result = boat->pose.m[0][2];
    boat->route.link_distance = 0;
    boat->route.distance = 0;
    boat->route_target.vector.x = result;
    boat->route_target.vector.y = boat->pose.m[1][2];
    result = boat->pose.m[2][2];
    boat->route_target.len = 0;
    boat->route.steering = 0;
    boat->route_target.vector.z = result;
    return result;
}

sint32 ai_assign_ctrl_states(const RACE_VIEW *grp, const RACE_VIEW *grp1)
{
    BOAT *nearest = NULL;
    BOAT *next_nearest = NULL;
    uint32 mode = race_cfg.players;
    sint32 gap0 = 4096;
    sint32 gap1 = 4096;
    sint32 count;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x8002FC28, "MAIN.EXE");
    if (mode == 1)
    {
        sint32 phase = 2;

        boat_player(grp)->race.phase = 1;
        boat_player(grp)->race.mode = 4;
        count = boat_racer_count;
        idx = 0;
        while (idx < count)
        {
            BOAT *state = boat_racers[idx];
            if (state != boat_player(grp))
            {
                BOAT *primary;
                sint32 gap;

                state->race.phase = 0;
                state->race.mode = 3;
                primary = boat_player(grp);
                gap = state->race.progress - primary->race.progress;
                gap = abs(gap);
                if (gap < gap0)
                {
                    gap1 = gap0;
                    next_nearest = nearest;
                    gap0 = gap;
                    nearest = state;
                }
                else if (gap < gap1)
                {
                    gap1 = gap;
                    next_nearest = state;
                }
            }
            ++idx;
            count = boat_racer_count;
        }
        if (nearest != NULL)
        {
            nearest->race.phase = 1;
            nearest->race.mode = 4;
        }
        if (next_nearest != NULL)
        {
            next_nearest->race.phase = 2;
            next_nearest->race.mode = 4;
        }
        boat_tracks(grp)[0] = nearest;
        boat_tracks(grp)[1] = next_nearest;
        count = boat_racer_count;
        idx = 0;
        while (idx < count)
        {
            BOAT **entry = boat_racers + idx;
            BOAT *state = *entry;

            *entry = state;
            if (state->race.phase == 0)
            {
                state->race.phase = phase;
                phase = phase == 2 ? 1 : 2;
                state->route.behavior = 12 + (random_next() & 1);
                state->route.lane_timer = random_next() & 31;
            }
            ++idx;
            count = boat_racer_count;
        }
        result = 2;
        if (boat_player_menu(grp)->mode == 2)
        {
            count = boat_racer_count;
            result = count;
            idx = 0;
            while (idx < count)
            {
                (*(boat_racers + idx))->race.mode = 7;
                ++idx;
                count = boat_racer_count;
                result = idx < count;
            }
        }
    }
    else
    {
        BOAT *primary = boat_player(grp);
        BOAT *peer = boat_player(grp1);
        sint32 slot = 0;

        peer->race.phase = 1;
        primary->race.phase = 1;
        count = boat_racer_count;
        result = count;
        idx = 0;
        while (idx < count)
        {
            BOAT *state = boat_racers[idx];
            state->race.mode = 4;
            if (state != boat_player(grp) && state != boat_player(grp1))
            {
                state->race.phase = 2;
                boat_tracks(grp1)[slot] = state;
                boat_tracks(grp)[slot] = state;
                ++slot;
            }
            ++idx;
            count = boat_racer_count;
            result = idx < count;
        }
    }
    return result;
}
