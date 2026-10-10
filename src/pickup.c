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

sint32 pickup_update_slots(uint32 state, uint32 *ot, const BOAT *boat)
{
    uint32 mode = r_u32(0x80083484u);
    uint32 menu = r_u32(state + 100u);
    sint16 command;

    FUNCTION_MARKER(0x8003D72Cu, "MAIN.EXE");
    render_order.ot = ot;
    command = mode == 8u ? (sint16)race_events_nearby_event_check(boat) : (sint16)pickup_scan(boat);
    mode = r_u32(0x80083484u);
    if (mode == 5u)
        return 5;
    pickup_apply_command(state, menu, (sint16)(command - 1), vehicle_legacy(boat));
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














