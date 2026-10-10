#include "game.h"
#include "route.h"
#include "name.h"
#include "arena.h"
#include "race_events.h"
#include "timer.h"
#include "pickup.h"
#include "global.h"
#include "vehicle.h"
#include "sound.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

// Restore pickup flags when the alternating checkpoint buffer becomes active
typedef struct
{
    ROUTE_VERTEX *vtx;
    uint8 type;
} PICKUP_RESTORE;
typedef struct
{
    PICKUP_RESTORE entries[40];
    uint8 count;
} PICKUP_RESTORE_BUF;
static PICKUP_RESTORE_BUF pickup_restore_bufs[2];
static uint8 pickup_restore_idx;

PICKUP_HUD pickup_hud[4];

void pickup_reset(void)
{
    pickup_restore_bufs[0].count = 0;
    pickup_restore_bufs[1].count = 0;
    pickup_restore_idx = 0;
}

static sint32 pickup_queue(ROUTE_VERTEX *vtx, uint32 type)
{
    PICKUP_RESTORE_BUF *buf;
    PICKUP_RESTORE *entry;

    if (pickup_restore_idx >= 2u)
        abort();
    buf = &pickup_restore_bufs[pickup_restore_idx];
    if (buf->count >= 40u)
        abort();
    entry = &buf->entries[buf->count];
    entry->vtx = vtx;
    entry->type = (uint8)type;
    if (buf->count >= 39u)
        return 0;
    ++buf->count;
    return 1;
}

void pickup_restore(void)
{
    PICKUP_RESTORE_BUF *buf;
    uint32 idx;

    pickup_restore_idx = pickup_restore_idx == 0u;
    buf = &pickup_restore_bufs[pickup_restore_idx];
    if (buf->count > 40u)
        abort();
    for (idx = 0; idx < buf->count; ++idx)
    {
        PICKUP_RESTORE *entry = &buf->entries[idx];
        entry->vtx->flags |= (uint8)(32u * entry->type);
    }
    buf->count = 0;
}

uint32 pickup_scan(const BOAT *boat)
{
    ROUTE_SEGMENT *object = boat->contacts.points[0].seg;
    sint32 object_index;

    FUNCTION_MARKER(0x80024738u, "MAIN.EXE");
    for (object_index = 0; object_index < 2; ++object_index)
    {
        sint32 item_index = 0;

        while (item_index < object->vertex_count)
        {
            ROUTE_VERTEX *vtx = &object->vertices[item_index];
            uint32 type = vtx->flags >> 5;
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

                first = vtx->position[0];
                second = (uint16)object->origin[0];
                x = (sint16)(second + (first << 4));
                first = vtx->position[1];
                second = (uint16)object->origin[1];
                y = (sint16)(second + (first << 4));
                first = vtx->position[2];
                second = (uint16)object->origin[2];
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
                        if (type == 3u && (uint32)boat->control.driver == 0u)
                            return 0;
                        if (type == 5u && (uint32)boat->control.driver == 0u)
                            return 0;
                        if (type != 3u && !pickup_queue(vtx, type))
                            return 0;
                        vtx->flags &= 0x1Fu;
                        return type;
                    }
                }
            }
            ++item_index;
        }
        object = object->prev.seg;
    }
    return 0;
}

sint32 pickup_apply_command(const RACE_VIEW *state, struct BOAT_MENU *menu, sint16 command)
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
        first = menu->pickups[2].count;
        if ((sint32)first >= (sint16)pickup_hud[2].limit)
            return 0;
        first = menu->pickups[2].count;
        menu->pickups[2].count = (uint8)(first + 1u);
        sound_queue_command(sound_for_context(state), 1, 1);
        return sound_queue_command(sound_for_context(state), 30, 2);
    }
    if (command == 3)
    {
        first = menu->pickups[1].count;
        if (first != 0u)
        {
            first = menu->pickups[1].count;
            menu->pickups[1].count = (uint8)(first - 1u);
            return sound_queue_command(sound_for_context(state), 7, 1);
        }
        first = menu->pickups[3].count;
        if (first >= 3u)
            return 0;
        first = menu->pickups[3].count;
        second = menu->pickups[1].count;
        updated = (uint8)(first + 1u);
        menu->pickups[3].count = updated;
        first = menu->pickups[3].count;
        if ((uint32)first + second >= 4u)
            menu->pickups[1].count = (uint8)(3u - updated);
        sound_queue_command(sound_for_context(state), 1, 1);
        return sound_queue_command(sound_for_context(state), 30, 2);
    }
    if (command == 4)
    {
        time_format(&race_freeze_time, 300);
        sound_queue_command(sound_for_context(state), 10, 1);
        return sound_queue_command(sound_for_context(state), 30, 2);
    }
    if (command == 1)
    {
        if ((uint32)boat->control.mode == 1u)
        {
            boat->control.mode = (uint32)(0u);
            boat->control.boost_tick = (sint32)(0u);
            sound_queue_command(sound_for_context(state), 0, 6);
        }
        first = menu->pickups[1].count;
        if (first >= 3u)
            return 0;
        first = menu->pickups[1].count;
        second = menu->pickups[3].count;
        updated = (uint8)(first + 1u);
        menu->pickups[1].count = updated;
        first = menu->pickups[1].count;
        if ((uint32)first + second >= 4u)
            menu->pickups[3].count = (uint8)(3u - updated);
        first = menu->pickups[1].count;
        if (first == 3u)
        {
            vehicle_damp_vel(boat);
            menu->pickups[1].count = 0u;
            return sound_queue_command(sound_for_context(state), 6, 1);
        }
        sound_queue_command(sound_for_context(state), 2, 1);
        return sound_queue_command(sound_for_context(state), -30, 2);
    }
    return 4;
}

sint32 pickup_update_slot_countdown(BOAT_MENU *menu, sint16 slot)
{
    PICKUP_SLOT *record;
    uint8 active;
    sint32 result;

    FUNCTION_MARKER(0x8003D668u, "MAIN.EXE");
    if ((uint16)slot >= 4u)
        abort();
    record = &menu->pickups[slot];
    active = record->count;
    result = (sint16)pickup_hud[slot].limit < active;
    if (result != 0)
        return result;
    result = (sint16)record->ticks;
    if (result == 0)
        return result;
    result -= 1;
    record->ticks = (uint16)result;
    result = (sint32)((uint32)(uint16)result << 16);
    if (result != 0)
        return result;
    if (record->pending != 0u)
    {
        result = (sint16)record->delta;
        if (result > 0)
            return result;
        record->count = 0u;
        record->pending = 0u;
        record->delta = 0u;
    }
    result = (sint16)record->delta;
    if (result < 0)
    {
        result = (sint32)record->count - 1;
        record->count = (uint8)result;
    }
    return result;
}

sint32 pickup_update_slots(const RACE_VIEW *state, uint32 *ot, const BOAT *boat)
{
    uint32 mode = race_cfg.mode;
    BOAT_MENU *menu = vehicle_player_menu(state);
    sint16 command;

    FUNCTION_MARKER(0x8003D72Cu, "MAIN.EXE");
    render_order.ot = ot;
    command = mode == 8u ? (sint16)race_events_nearby_event_check(boat) : (sint16)pickup_scan(boat);
    mode = race_cfg.mode;
    if (mode == 5u)
        return 5;
    pickup_apply_command(state, menu, (sint16)(command - 1));
    pickup_update_slot_countdown(menu, 2);
    pickup_update_slot_countdown(menu, 3);
    return pickup_update_slot_countdown(menu, 1);
}

sint32 pickup_write_racer_indices(uint8 *output)
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
        output[index] = (uint8)(vehicle_boats[index].menu.racer_num - 1u);
    return 0;
}

sint32 pickup_update_racer_flags(void)
{
    sint32 count = (sint32)route_resources.count;
    sint32 index;

    FUNCTION_MARKER(0x8003D850u, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        ROUTE_SEGMENT *seg = route_resources.segments[(uint32)index];
        seg->commands[2] |= 4u;
        seg->commands[9] |= 8u;
    }
    return index < (sint32)route_resources.count;
}














