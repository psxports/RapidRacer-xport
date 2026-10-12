#include "river.h"
#include "game.h"
#include "route.h"
#include "name.h"
#include "arena.h"
#include "timer.h"
#include "pickup.h"
#include "utils.h"
#include "boat.h"
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
    entry->type = type;
    if (buf->count >= 39u)
        return 0;
    ++buf->count;
    return 1;
}

void pickup_restore(void)
{
    PICKUP_RESTORE_BUF *buf;
    uint32 idx;

    pickup_restore_idx = pickup_restore_idx == 0;
    buf = &pickup_restore_bufs[pickup_restore_idx];
    if (buf->count > 40u)
        abort();
    for (idx = 0; idx < buf->count; ++idx)
    {
        PICKUP_RESTORE *entry = &buf->entries[idx];
        entry->vtx->flags |= 32u * entry->type;
    }
    buf->count = 0;
}

uint32 pickup_scan(const BOAT *boat)
{
    ROUTE_SEGMENT *object = boat->contacts.points[0].seg;
    sint32 object_idx;

    FUNCTION_MARKER(0x80024738, "MAIN.EXE");
    for (object_idx = 0; object_idx < 2; ++object_idx)
    {
        sint32 item_idx = 0;

        while (item_idx < object->vertex_count)
        {
            ROUTE_VERTEX *vtx = &object->vertices[item_idx];
            uint32 type = vtx->flags >> 5;
            sint32 x;
            sint32 y;
            sint32 z;
            sint32 distance;
            sint32 sq_sum;

            if (type >= 2)
            {
                SVECTOR dir;
                VECTOR proj;
                VECTOR sq;

                x = (sint16)(object->origin[0] + (vtx->pos[0] << 4));
                y = (sint16)(object->origin[1] + (vtx->pos[1] << 4));
                z = (sint16)(object->origin[2] + (vtx->pos[2] << 4));
                x = (sint16)(x - boat->pos.x);
                y = (sint16)(y - boat->pos.y);
                z = (sint16)(z - boat->pos.z);
                if (abs(x) + abs(y) + abs(z) <= 79999)
                {
                    sint32 dot;

                    dir.vx = boat->pose.m[0][2];
                    dir.vy = boat->pose.m[1][2];
                    dir.vz = boat->pose.m[2][2];
                    dir.pad = 0;
                    dot = x * dir.vx + y * dir.vy + z * dir.vz;
                    distance = dot / 4096;
                    distance = clamp(distance, -100, 150);
                    gte_gpf12(&dir, distance, &proj);
                    x = (sint16)(x - proj.vx);
                    y = (sint16)(y - proj.vy);
                    z = (sint16)(z - proj.vz);
                    proj.vx = x;
                    proj.vy = y;
                    proj.vz = z;
                    Square12(&proj, &sq);
                    sq_sum = sq.vx + sq.vy + sq.vz;
                    if (type == 2 && sq_sum < 2500 || type >= 3 && sq_sum < 10000)
                    {
                        if (type == 3 && boat->control.driver == 0)
                            return 0;
                        if (type == 5 && boat->control.driver == 0)
                            return 0;
                        if (type != 3 && !pickup_queue(vtx, type))
                            return 0;
                        vtx->flags &= 0x1F;
                        return type;
                    }
                }
            }
            ++item_idx;
        }
        object = object->prev.seg;
    }
    return 0;
}

static sint32 pickup_apply_command(const RACE_VIEW *state, struct BOAT_MENU *menu, sint16 command)
{
    BOAT *boat = boat_player(state);

    FUNCTION_MARKER(0x8003D454, "MAIN.EXE");
    if (command <= 0)
        return 0;
    if (command == 2)
    {
        if (menu->pickups[2].count >= (sint16)pickup_hud[2].limit)
            return 0;
        ++menu->pickups[2].count;
        sound_queue_command(sound_for_context(state), 1, 1);
        return sound_queue_command(sound_for_context(state), 30, 2);
    }
    if (command == 3)
    {
        if (menu->pickups[1].count != 0)
        {
            --menu->pickups[1].count;
            return sound_queue_command(sound_for_context(state), 7, 1);
        }
        if (menu->pickups[3].count >= 3)
            return 0;
        ++menu->pickups[3].count;
        if (menu->pickups[3].count + menu->pickups[1].count >= 4)
            menu->pickups[1].count = 3 - menu->pickups[3].count;
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
        if (boat->control.mode == 1)
        {
            boat->control.mode = 0;
            boat->control.boost_tick = 0;
            sound_queue_command(sound_for_context(state), 0, 6);
        }
        if (menu->pickups[1].count >= 3)
            return 0;
        ++menu->pickups[1].count;
        if (menu->pickups[1].count + menu->pickups[3].count >= 4)
            menu->pickups[3].count = 3 - menu->pickups[1].count;
        if (menu->pickups[1].count == 3)
        {
            boat_damp_vel(boat);
            menu->pickups[1].count = 0;
            return sound_queue_command(sound_for_context(state), 6, 1);
        }
        sound_queue_command(sound_for_context(state), 2, 1);
        return sound_queue_command(sound_for_context(state), -30, 2);
    }
    return 4;
}

static sint32 pickup_update_slot_countdown(BOAT_MENU *menu, sint16 slot)
{
    PICKUP_SLOT *rec;
    sint32 result;

    FUNCTION_MARKER(0x8003D668, "MAIN.EXE");
    if ((uint16)slot >= 4u)
        abort();
    rec = &menu->pickups[slot];
    result = (sint16)pickup_hud[slot].limit < rec->count;
    if (result != 0)
        return result;
    result = (sint16)rec->ticks;
    if (result == 0)
        return result;
    result -= 1;
    rec->ticks = result;
    result = (uint32)(uint16)result << 16;
    if (result != 0)
        return result;
    if (rec->pending != 0)
    {
        result = (sint16)rec->delta;
        if (result > 0)
            return result;
        rec->count = 0;
        rec->pending = 0;
        rec->delta = 0;
    }
    result = (sint16)rec->delta;
    if (result < 0)
    {
        result = rec->count - 1;
        rec->count = result;
    }
    return result;
}

sint32 pickup_update_slots(const RACE_VIEW *state, uint32 *ot, const BOAT *boat)
{
    uint32 mode = race_cfg.mode;
    BOAT_MENU *menu = boat_player_menu(state);
    sint16 command;

    FUNCTION_MARKER(0x8003D72C, "MAIN.EXE");
    render_order.ot = ot;
    command = mode == 8 ? river_nearby_event_check(boat) : pickup_scan(boat);
    mode = race_cfg.mode;
    if (mode == 5)
        return 5;
    pickup_apply_command(state, menu, command - 1);
    pickup_update_slot_countdown(menu, 2);
    pickup_update_slot_countdown(menu, 3);
    return pickup_update_slot_countdown(menu, 1);
}

sint32 pickup_write_racer_indices(uint8 *output)
{
    uint32 partial;
    sint32 count;
    sint32 idx;

    FUNCTION_MARKER(0x8003D7E8, "MAIN.EXE");
    partial = boat_leader_count;
    partial += boat_racer_count;
    count = partial + boat_trailer_count;
    if (count <= 0)
        return partial;
    for (idx = 0; idx < count; ++idx)
        output[idx] = boat_boats[idx].menu.racer_num - 1u;
    return 0;
}

sint32 pickup_update_racer_flags(void)
{
    sint32 count = route_resources.count;
    sint32 idx;

    FUNCTION_MARKER(0x8003D850, "MAIN.EXE");
    for (idx = 0; idx < count; ++idx)
    {
        ROUTE_SEGMENT *seg = route_resources.segs[idx];
        seg->commands[2] |= 4;
        seg->commands[9] |= 8;
    }
    return idx < (sint32)route_resources.count;
}
