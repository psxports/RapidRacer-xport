#include "camera.h"
#include "arena.h"
#include "effects.h"
#include "render.h"
#include "results.h"
#include "race_events.h"
#include "global.h"
#include "vehicle.h"
#include "sound.h"
#include "callbacks.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 race_events_racer_modes_trans(uint32 state)
{
    uint32 root;
    uint32 node;
    uint16 marker;
    uint32 value;
    sint32 participant_count;
    sint32 mode;
    sint32 index;

    FUNCTION_MARKER(0x8002CFA8u, "MAIN.EXE");
    root = r_u32(0x8008349Cu);
    node = r_u32(root + 144u);
    marker = r_u16(0x800B3D94u);
    value = r_u32(node + 8u);
    if (marker == value)
    {
        effects_init_transform_object(state);
        sound_queue_command(0x800DE0F0u, 11, 0, 0u);
    }
    participant_count = (sint32)vehicle_racer_count;
    if (participant_count > 0)
    {
        sint32 loop_mode = (sint32)r_u32(0x800B6B10u);

        index = 0;
        do
        {
            BOAT *object = vehicle_racers[index];
            uint16 object_state = (uint16)object->race.phase;

            object->race.phase = (uint16)(object_state == 1u ? 2u : 1u);
            if (loop_mode == 1)
                object->race.mode = (uint16)(3u);
            ++index;
        } while (index < (sint32)vehicle_racer_count);
    }
    mode = (sint32)r_u32(0x800B6B10u);
    if (mode == 0)
        return 1;
    if (mode == 1)
    {
        camera_views[0].mode = 7u;
        camera_attract.phase = 30u;
    }
    else if (mode == 4)
    {
        uint32 menu = r_u32(0x800DE154u);

        vehicle_menu(menu)->mode = (uint32)mode;
        return sound_queue_command(0x800DE0F0u, 10, 0, 0u);
    }
    mode = (sint32)r_u32(0x800B6B10u);
    if (mode <= 0)
        return mode;
    {
        BOAT *object = vehicle_player(state);

        if ((sint16)(uint16)object->race.phase == 2)
        {
            uint32 next;

            object->race.mode = (uint16)(4u);
            object = vehicle_player(state);
            vehicle_init_motion(object);
            object = vehicle_player(state);
            object->route.speed = (sint32)(0u);
            object = vehicle_player(state);
            value = r_u32(0x800B6B10u);
            next = object->setup.throttle;
            value += 1u;
            w_u32(0x800B6B10u, value);
            object->control.throttle = (sint32)(next);
        }
    }
    {
        BOAT *object = vehicle_tracks(state)[0];

        if ((sint16)(uint16)object->race.phase == 2)
        {
            object->race.mode = (uint16)(5u);
            object = vehicle_tracks(state)[0];
            vehicle_init_motion(object);
            value = r_u32(0x800B6B10u) + 1u;
            w_u32(0x800B6B10u, value);
        }
    }
    {
        BOAT *object = vehicle_tracks(state)[1];

        if ((sint16)(uint16)object->race.phase == 2)
        {
            object->race.mode = (uint16)(5u);
            object = vehicle_tracks(state)[1];
            vehicle_init_motion(object);
            value = r_u32(0x800B6B10u) + 1u;
            w_u32(0x800B6B10u, value);
            return (sint32)value;
        }
    }
    return 5;
}

sint32 race_events_fn_800389f4(sint32 index, sint32 variant)
{
    const BOAT *boat = vehicle_players[0];
    uint32 offset;

    FUNCTION_MARKER(0x800389F4u, "MAIN.EXE");
    w_u32(0x800B68B4u, (uint32)boat->motion.position[0] != (uint32)(sint32)(sint16)(uint16)boat->motion.position[0]);
    if ((uint32)boat->motion.position[1] != (uint32)(sint32)(sint16)(uint16)boat->motion.position[1] || (sint32)(uint32)boat->motion.position[2] < -30000 || (sint32)(uint32)boat->motion.position[2] > 34000)
        w_u32(0x800B68B4u, 1u);
    if (variant == 0)
    {
        w_u32(0x800B6874u, 0x800CB14Cu + (uint32)index * 24u);
        w_u32(0x800B6880u, r_u32(0x800B6878u + (uint32)index * 4u));
        w_u32(0x800B688Cu, r_u32(0x800B6884u + (uint32)index * 4u));
        w_u32(0x800B6898u, r_u32(0x800B6890u + (uint32)index * 4u));
    }
    offset = (uint32)(2 * variant + index) * 4u;
    w_u32(0x800B689Cu, r_u32(0x800CB17Cu + offset));
    w_u32(0x800B68A0u, r_u32(0x800CB18Cu + offset));
    w_u32(0x800B68A4u, r_u32(0x800CB19Cu + offset));
    return (sint32)r_u32(0x800B68A4u);
}

uint32 race_events_fn_80038b04(uint32 state)
{
    uint32 packet = r_u32(0x800B689Cu);
    uint32 ordering = r_u32(0x800B6898u);
    sint32 connections = (r_u32(0x80083478u) == 1u || r_u32(0x80083488u) == 1u) ? 9 : 7;
    sint32 index;

    FUNCTION_MARKER(0x80038B04u, "MAIN.EXE");
    w_u32(packet, r_u32(state + 1648u));
    w_u32(state + 1648u, (packet + 56u) & 0x00FFFFFFu);
    w_u32(ordering, (r_u32(ordering) & 0xFF000000u) | (r_u32(packet + 52u) & 0x00FFFFFFu));
    w_u32(packet + 52u, (r_u32(packet + 52u) & 0xFF000000u) | (ordering & 0x00FFFFFFu));
    ordering += 12u;
    packet += 56u;
    for (index = 0; index < connections; ++index)
    {
        sint32 primitive;
        for (primitive = 0; primitive < 11; ++primitive)
        {
            w_u8(packet + 7u, 0x3Cu);
            w_u8(packet + 12u, (uint8)(primitive < 6 ? 0u : 63u));
            w_u8(packet + 24u, (uint8)(primitive < 6 ? 63u : 0u));
            w_u32(packet, (r_u32(packet) & 0xFF000000u) | (r_u32(r_u32(0x800B689Cu) + 52u) & 0x00FFFFFFu));
            w_u32(r_u32(0x800B689Cu) + 52u, (r_u32(r_u32(0x800B689Cu) + 52u) & 0xFF000000u) | (packet & 0x00FFFFFFu));
            packet += 52u;
            if (primitive < 6)
                ordering += 40u;
        }
    }
    w_u32(0x800B6880u, packet);
    w_u32(0x800B688Cu, ordering);
    xport_update_u32(0x800B6898u, XPORT_MEMORY_UPDATE_ADD, 12u);
    xport_update_u32(0x800B689Cu, XPORT_MEMORY_UPDATE_ADD, 56u);
    return r_u32(r_u32(0x800B689Cu) - 4u);
}

uint32 race_events_fn_80039728(uint32 state, uint32 player, sint32 buffer_index)
{
    uint32 route = r_u32(0x800E8D50u);
    sint32 route_count = (sint32)r_u32(0x800E8D54u);
    uint32 collisions = r_u32(0x800B68B0u);
    sint32 collision_count = 0;
    sint32 rendered = 0;
    sint32 index;

    FUNCTION_MARKER(0x80039728u, "MAIN.EXE");
    w_u32(r_u32(0x800B68A0u), r_u32(r_u32(0x800B689Cu) + 48u));
    w_u32(r_u32(0x800B689Cu) + 48u, (r_u32(0x800B68A0u) + 1000u) & 0xFFFFFFu);
    w_u32(r_u32(0x800B68A4u), r_u32(r_u32(0x800B689Cu) + 48u));
    w_u32(r_u32(0x800B689Cu) + 48u, (r_u32(0x800B68A4u) + 1000u) & 0xFFFFFFu);
    w_u32(0x800B68A0u, state + (uint32)buffer_index * 1784u + 328u);
    if (vehicle_menu(r_u32(0x800DE154u))->mode == 5u || vehicle_menu(r_u32(0x800DE154u))->mode == 1u)
    {
        uint32 link = r_u32(0x800B6874u);
        w_u32(link, (r_u32(link) & 0xFF000000u) | (r_u32(player + 1684u) & 0xFFFFFFu));
        w_u32(player + 1684u, (r_u32(player + 1684u) & 0xFF000000u) | (link & 0xFFFFFFu));
    }
    if (r_u32(0x800B68B4u) != 0u)
        return 1u;
    for (index = 0; index < route_count && rendered < 14; ++index)
    {
        uint32 segment = route + (uint32)index * 20u;
        if ((r_u16(segment + 6u) >> 12) == 0u)
            continue;
        if (race_events_fn_80039e10(state, (sint32)player, r_u32(0x800B6C10u), segment, 0, 0, 0, 0, 0u) != 0)
        {
            sint32 point;
            if (r_u16(segment + 14u) != 0u && collision_count < 100 && r_u32(0x800B68A8u) != UINT32_MAX)
            {
                uint32 entry = collisions + (uint32)collision_count * 8u;
                w_u16(entry, r_u16(segment + 8u) + (uint16)camera_for_view(state)->position[0]);
                w_u16(entry + 2u, r_u16(segment + 10u) + (uint16)camera_for_view(state)->position[1]);
                w_u16(entry + 4u, r_u16(segment + 12u) + (uint16)camera_for_view(state)->position[2]);
                w_u16(entry + 6u, r_u16(segment + 14u));
                ++collision_count;
            }
            for (point = 0; point < (sint16)r_u16(segment + 18u) && collision_count < 100; ++point)
            {
                uint32 source = r_u32(0x800B68ACu) + 8u * (r_u16(segment + 16u) + (uint32)point);
                uint32 entry = collisions + (uint32)collision_count * 8u;
                w_u16(entry, r_u16(source) + (uint16)camera_for_view(state)->position[0]);
                w_u16(entry + 2u, r_u16(source + 2u) + (uint16)camera_for_view(state)->position[1]);
                w_u16(entry + 4u, r_u16(source + 4u) + (uint16)camera_for_view(state)->position[2]);
                w_u16(entry + 6u, r_u16(source + 6u));
                ++collision_count;
            }
            rendered += 2;
        }
    }
    render_billboard(r_u32(0x800B68A0u), collisions, collision_count, 0);
    return render_billboard(r_u32(0x800B68A4u), collisions, collision_count, 1);
}

sint32 race_events_nearby_event_check(const BOAT *boat)
{
    sint32 relative;

    FUNCTION_MARKER(0x80039CCCu, "MAIN.EXE");
    if ((sint32)r_u32(0x800B68A8u) == -1)
        return 0;
    for (relative = -1; relative < 2; ++relative)
    {
        sint32 index = relative + (sint32)r_u32(0x800E8D58u);
        uint32 entry;
        sint32 dx;
        sint32 dz;
        sint32 result;
        uint32 first;
        uint32 second;
        VECTOR delta;
        VECTOR squared;

        if (index < 0 || index >= (sint32)r_u32(0x800E8D54u))
            continue;
        entry = r_u32(0x800E8D50u) + (uint32)index * 20u;
        first = (uint16)boat->contacts.points[0].position[0];
        second = r_u16(entry + 8u);
        dx = (sint16)(first - second);
        first = (uint16)boat->contacts.points[0].position[2];
        second = r_u16(entry + 12u);
        dz = (sint16)(first - second);
        delta.vx = dx;
        delta.vy = 0;
        delta.vz = dz;
        Square12(&delta, &squared);
        if ((sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz) >= 20000)
            continue;
        result = (sint16)r_u16(entry + 14u);
        w_u16(entry + 14u, 0u);
        if (result == 3)
            sound_queue_command(0x800DE0F0u, r_u8(r_u32(0x800DE154u) + 582u) + 10, 7, 0x800E8D50u);
        return result;
    }
    return 0;
}

static void race_events_write_matrix(uint32 address, const MATRIX *matrix)
{
    sint32 row;
    sint32 column;

    for (row = 0; row < 3; ++row)
        for (column = 0; column < 3; ++column)
            w_u16(address + (uint32)(row * 3 + column) * 2u, (uint16)matrix->m[row][column]);
    w_u16(address + 18u, 0u);
    for (row = 0; row < 3; ++row)
        w_u32(address + 20u + (uint32)row * 4u, (uint32)matrix->t[row]);
}

sint32 race_events_fn_80039e10(uint32 state, sint32 unused, uint32 model, uint32 parameters, sint32 a5, sint32 a6, sint32 a7, sint32 a8, uint32 colors)
{
    uint32 frame;
    uint32 matrix_address;
    MATRIX matrix = {{{0}}};
    PsxGteSnapshot snapshot;
    SVECTOR origin = {0, 0, 0, 0};
    sint32 screen;
    sint32 flags;
    sint32 radius;
    sint32 depth;

    FUNCTION_MARKER(0x80039E10u, "MAIN.EXE");
    frame = guest_stack_push(0x80u);
    matrix_address = frame + 0x30u;
    matrix.m[0][0] = ONE;
    matrix.m[1][1] = ONE;
    matrix.m[2][2] = ONE;
    matrix.t[0] = (sint16)r_u16(parameters);
    matrix.t[1] = (sint16)r_u16(r_u32(0x8008349Cu) + 4u) + race_events_fn_8003a2fc();
    matrix.t[2] = (sint16)r_u16(parameters + 4u);
    RotMatrixZ(1024 - (sint16)r_u16(parameters + 6u), &matrix);
    race_events_write_matrix(matrix_address, &matrix);
    camera_bias(camera_for_view(state), matrix_address);
    (void)gte_project_full_depth(&origin, &screen, &flags);
    psx_gte_snapshot(&snapshot);
    radius = snapshot.ir0;
    depth = snapshot.ir[2];
    if ((sint16)screen + radius < 0 || (sint16)screen - radius > (sint32)r_u32(0x800B3DF8u) || (sint16)((uint32)screen >> 16) + radius < 0 || (sint16)((uint32)screen >> 16) - radius > (sint32)r_u32(0x800B3DFCu))
    {
        guest_stack_pop(0x80u);
        return 0;
    }
    if (depth + 1035 < 0 || depth - 1035 >= 8001)
    {
        guest_stack_pop(0x80u);
        return 0;
    }
    camera_config_lighting(vehicle_player(state), (const MATRIX *)psx_addr(matrix_address, sizeof(MATRIX)));
    camera_bias(camera_for_view(state), matrix_address);
    w_u32(0x800B69F0u, 0u);
    race_events_fn_8003a100(r_u32(0x800B68A0u), 250u, model, colors);
    matrix.m[0][0] = (sint16)(0u - (uint16)matrix.m[0][0]);
    matrix.m[0][1] = (sint16)(0u - (uint16)matrix.m[0][1]);
    matrix.m[1][0] = (sint16)(0u - (uint16)matrix.m[1][0]);
    matrix.m[1][1] = (sint16)(0u - (uint16)matrix.m[1][1]);
    matrix.m[2][0] = (sint16)(0u - (uint16)matrix.m[2][0]);
    matrix.m[2][1] = (sint16)(0u - (uint16)matrix.m[2][1]);
    matrix.t[1] = (sint32)((uint32)matrix.t[1] + 10u);
    race_events_write_matrix(matrix_address, &matrix);
    camera_config_lighting(vehicle_player(state), (const MATRIX *)psx_addr(matrix_address, sizeof(MATRIX)));
    camera_bias(camera_for_view(state), matrix_address);
    w_u32(0x800B69F0u, 0u);
    race_events_fn_8003a100(r_u32(0x800B68A4u), 250u, model, colors + 4u);
    guest_stack_pop(0x80u);
    return 1;
}

void race_events_fn_8003a100(uint32 ordering_table, uint32 bucket_count, uint32 model, uint32 colors)
{
    uint32 remaining;
    uint32 face;
    uint32 packet;
    sint32 previous_cross = -1;
    sint32 previous_depth = 0;

    FUNCTION_MARKER(0x8003A100u, "MAIN.EXE");
    if (model == 0u)
        return;
    remaining = r_u32(model + 12u) - 1u;
    face = r_u32(model + 44u);
    if (remaining == UINT32_MAX)
        return;
    packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
    do
    {
        SVECTOR vertices[4];
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross = -1;
        sint32 vertex;

        for (vertex = 0; vertex < 4; ++vertex)
        {
            vertices[vertex].vx = (sint16)r_u16(face + (uint32)vertex * 8u);
            vertices[vertex].vy = (sint16)r_u16(face + (uint32)vertex * 8u + 2u);
            vertices[vertex].vz = (sint16)r_u16(face + (uint32)vertex * 8u + 4u);
            vertices[vertex].pad = (sint16)r_u16(face + (uint32)vertex * 8u + 6u);
        }
        gte_project3_full_depth(vertices, screen, depth, &flags);
        if (previous_cross > 0 && (uint32)(previous_depth >> 3) < bucket_count)
        {
            uint32 bucket = ordering_table + (uint32)(previous_depth >> 3) * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
        if (flags >= 0)
            cross = NormalClip(screen[0], screen[1], screen[2]);
        previous_cross = cross;
        if (cross > 0)
        {
            uint32 high;
            uint32 low;
            uint32 color;

            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            depth[3] = gte_project_full_depth(&vertices[3], &screen[3], &flags);
            w_u32(packet + 32u, (uint32)screen[3]);
            color = gte_normal_color_col_depth(&vertices[3], r_u32(colors));
            low = r_u16(face + 6u);
            high = r_u16(face + 42u);
            w_u32(packet + 12u, low | (high << 16));
            low = r_u16(face + 14u);
            high = r_u16(face + 40u);
            w_u32(packet + 20u, low | (high << 16));
            w_u16(packet + 28u, r_u16(face + 22u));
            w_u16(packet + 36u, r_u16(face + 30u));
            w_u32(packet + 4u, color);
            previous_depth = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        face += 48u;
    } while (--remaining != UINT32_MAX);
    if (previous_cross > 0 && (uint32)(previous_depth >> 3) < bucket_count)
    {
        uint32 bucket = ordering_table + (uint32)(previous_depth >> 3) * 4u;

        w_u32(packet, r_u32(bucket) | 0x09000000u);
        w_u32(bucket, packet);
        packet += 40u;
    }
    w_u32(0x800B6C00u, packet);
}

sint32 race_events_fn_8003a2fc(void)
{
    FUNCTION_MARKER(0x8003A2FCu, "MAIN.EXE");
    return 0;
}

sint32 terrain_collide_capsule(BOAT *boat, uint32 segment)
{
    MATRIX *pose = &boat->motion.transform.pose;
    BOAT_CAPSULE *capsule = &boat->capsule;
    VECTOR *start = &capsule->world[0];
    VECTOR *end = &capsule->world[1];
    VECTOR direction;
    SVECTOR normal;
    SVECTOR points[2] = {{0}, {0}};
    sint32 length;
    sint32 index;
    sint32 sine;
    sint32 cosine;

    FUNCTION_MARKER(0x8003A310u, "MAIN.EXE");
    SetRotMatrix(pose);
    SetTransMatrix(pose);
    gte_transform_raw(&capsule->local[0], start);
    gte_transform_raw(&capsule->local[1], end);
    start->vy = 0;
    end->vy = 0;
    direction.vx = math_sub_wrap_s32(end->vx, start->vx);
    direction.vy = math_sub_wrap_s32(end->vy, start->vy);
    direction.vz = math_sub_wrap_s32(end->vz, start->vz);
    VectorNormalS(&direction, &normal);
    length = (sint16)math_trunc_shift12_s32(math_add_wrap_s32(math_add_wrap_s32(math_mul_lo_s32(normal.vx, direction.vx), math_mul_lo_s32(normal.vy, direction.vy)), math_mul_lo_s32(normal.vz, direction.vz)));
    cosine = math_sra_s32((uint32)math_mul_lo_s32(470, rcos(1024 - (sint16)r_u16(segment + 6u))), 12u);
    sine = math_sra_s32((uint32)math_mul_lo_s32(470, rsin(1024 - (sint16)r_u16(segment + 6u))), 12u);
    points[0].vx = (sint16)((uint32)r_u16(segment) - (uint32)cosine);
    points[0].vz = (sint16)((uint32)r_u16(segment + 4u) + (uint32)sine);
    points[1].vx = (sint16)((uint32)r_u16(segment) + (uint32)cosine);
    points[1].vz = (sint16)((uint32)r_u16(segment + 4u) - (uint32)sine);
    for (index = 0; index < 2; ++index)
    {
        VECTOR relative;
        VECTOR nearest;
        VECTOR offset;
        VECTOR squared;
        SVECTOR contact_normal;
        sint32 projection;
        sint32 distance_square;
        sint32 radius = (sint32)capsule->radius;
        sint32 limit = math_add_wrap_s32(radius, 80);

        relative.vx = math_sub_wrap_s32(points[index].vx, start->vx);
        relative.vy = math_sub_wrap_s32(points[index].vy, start->vy);
        relative.vz = math_sub_wrap_s32(points[index].vz, start->vz);
        projection = math_sra_s32((uint32)math_add_wrap_s32(math_add_wrap_s32(math_mul_lo_s32(normal.vx, relative.vx), math_mul_lo_s32(normal.vy, relative.vy)), math_mul_lo_s32(normal.vz, relative.vz)), 12u);
        if (projection < 0)
            projection = 0;
        else if (projection > length)
            projection = length;
        nearest.vx = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vx, projection), 12u), start->vx);
        nearest.vy = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vy, projection), 12u), start->vy);
        nearest.vz = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vz, projection), 12u), start->vz);
        offset.vx = math_sub_wrap_s32(points[index].vx, nearest.vx);
        offset.vy = math_sub_wrap_s32(points[index].vy, nearest.vy);
        offset.vz = math_sub_wrap_s32(points[index].vz, nearest.vz);
        Square0(&offset, &squared);
        distance_square = math_add_wrap_s32(math_add_wrap_s32(squared.vx, squared.vy), squared.vz);
        if (distance_square < math_mul_lo_s32(limit, limit))
        {
            sint32 penetration = math_sub_wrap_s32(radius, math_sub_wrap_s32(SquareRoot0(distance_square), 80));

            nearest.vx = math_add_wrap_s32(points[index].vx, nearest.vx) / 2;
            nearest.vy = math_add_wrap_s32(points[index].vy, nearest.vy) / 2;
            nearest.vz = math_add_wrap_s32(points[index].vz, nearest.vz) / 2;
            VectorNormalS(&offset, &contact_normal);
            vehicle_resolve_collision(boat, NULL, &contact_normal, &nearest, penetration);
        }
    }
    return 0;
}

sint32 race_events_fn_8003a7e0(const BOAT *boat, uint32 entry)
{
    uint32 segment = r_u32(entry) + r_u32(entry + 8u) * 20u;
    sint32 type = (uint16)r_u16(segment + 6u) >> 12;
    sint32 angle;
    sint32 sine;
    sint32 cosine;
    sint32 x;
    sint32 z;
    sint32 projection;

    FUNCTION_MARKER(0x8003A7E0u, "MAIN.EXE");
    if (type == 3)
        return type;
    angle = 1024 - (sint16)r_u16(segment + 6u);
    sine = rsin(angle);
    cosine = rcos(angle);
    x = (sint16)r_u16(segment) - (sint32)(uint32)boat->motion.position[0];
    z = (sint16)r_u16(segment + 4u) - (sint32)(uint32)boat->motion.position[2];
    projection = (sine * x + cosine * z) >> 12;
    if (projection <= 0 && (sint32)r_u32(entry + 28u) >= 0)
    {
        sint32 distance = (sint32)SquareRoot0((x - (projection * sine >> 12)) * (x - (projection * sine >> 12)) + (z - (projection * cosine >> 12)) * (z - (projection * cosine >> 12)));
        w_u32(entry + 28u, (uint32)projection);
        if (distance < 471 && (sint32)r_u32(0x800B68A8u) == -1)
        {
            w_u32(0x800B68A8u, (uint32)type);
            sound_queue_command(0x800DE0F0u, 1, 1, 0u);
            w_u32(0x800E05B4u, r_u32(0x80091480u + (uint32)type * 4u));
            return (sint32)r_u8(0x80091480u + (uint32)type * 4u);
        }
    }
    else
        w_u32(entry + 28u, (uint32)projection);
    return projection;
}

sint32 vehicle_update_route_segments(uint32 state)
{
    FUNCTION_MARKER(0x8003AAA4u, "MAIN.EXE");
    race_events_fn_8003aae4(state, 0x800F3FA8u);
    return race_events_fn_8003aae4(state, 0x800E8D50u);
}

sint32 race_events_fn_8003aae4(uint32 state, uint32 route)
{
    BOAT *boat;
    sint32 current;
    sint32 best;
    sint32 relative;

    FUNCTION_MARKER(0x8003AAE4u, "MAIN.EXE");
    if (r_u32(0x800B68B4u) != 0u)
        return (sint32)r_u32(0x800B68B4u);
    boat = vehicle_player(state);
    current = (sint32)r_u32(route + 8u);
    {
        uint32 segment = r_u32(route) + (uint32)current * 20u;
        sint32 dx = (sint16)((uint16)boat->contacts.points[0].position[0] - r_u16(segment));
        sint32 dz = (sint16)((uint16)boat->contacts.points[0].position[2] - r_u16(segment + 4u));
        best = dx * dx + dz * dz;
    }
    for (relative = -1; relative < 2; relative += 2)
    {
        sint32 candidate = current + relative;
        if (candidate >= 0 && candidate < (sint32)r_u32(route + 4u))
        {
            uint32 segment = r_u32(route) + (uint32)candidate * 20u;
            sint32 dx = (sint16)((uint16)boat->contacts.points[0].position[0] - r_u16(segment));
            sint32 dz = (sint16)((uint16)boat->contacts.points[0].position[2] - r_u16(segment + 4u));
            sint32 distance = dx * dx + dz * dz;
            if (distance < best)
            {
                best = distance;
                current = candidate;
            }
        }
    }
    w_u32(route + 8u, (uint32)current);
    w_u32(route + 20u, (uint32)best);
    if (best < ((sint32)boat->capsule.bound_radius + 1020) * ((sint32)boat->capsule.bound_radius + 1020))
    {
        uint32 segment = r_u32(route) + (uint32)current * 20u;
        terrain_collide_capsule(boat, segment);
        if ((sint32)r_u32(0x800B68A8u) == -1)
            return race_events_fn_8003a7e0(boat, route);
    }
    return best;
}

void terrain_init_contact(ROUTE_CONTACT *point)
{
    FUNCTION_MARKER(0x8003AD04u, "MAIN.EXE");
    point->object = 0x800CB068u;
    point->entry = 0u;
    point->right = 0u;
    point->left = 0u;
    point->position[2] = 0;
    point->position[1] = 0;
    point->position[0] = 0;
    point->height = (sint32)(0u);
    point->penetration = (sint32)(0u);
}

sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground)
{
    VECTOR world;
    VECTOR up = {0, 4096, 0, 0};
    sint32 flags;
    sint32 delta[3];
    sint32 shift;

    FUNCTION_MARKER(0x8003AD44u, "MAIN.EXE");
    RotTrans(&point->sample, &world, &flags);
    delta[0] = (sint32)((uint32)world.vx - (uint32)point->position[0]);
    delta[1] = (sint32)((uint32)world.vy - (uint32)point->position[1]);
    delta[2] = (sint32)((uint32)world.vz - (uint32)point->position[2]);
    shift = 18 - Lzc(4096);
    if (shift > 0)
    {
        up.vx >>= shift;
        up.vy >>= shift;
        up.vz >>= shift;
    }
    VectorNormalS(&up, &point->normal);
    point->surface = 0;
    point->height = (sint32)((uint32)ground - (uint32)point->position[1]);
    for (sint32 index = 0; index < 3; ++index)
    {
        point->position[index] = (sint32)((uint32)point->position[index] + (uint32)delta[index]);
        point->delta[index] = delta[index];
        point->diagonal[index] = diagonal[index];
    }
    point->diagonal[3] = 48;
    return 0;
}

sint32 race_events_fn_8003af00(uint32 setup, uint32 argument)
{
    sint32 count = (sint32)r_u32(setup);
    uint32 segments = game_alloc_arena_bytes(20 * count);
    uint32 geometry = game_alloc_arena_bytes((96 * count) | 24);
    uint32 scratch = game_alloc_arena_bytes(800);
    sint32 index;
    sint32 angle = -512;
    sint32 lane_count;
    sint32 lane;

    FUNCTION_MARKER(0x8003AF00u, "MAIN.EXE");
    w_u32(0x800B68ACu, geometry);
    w_u32(0x800B68B0u, scratch);
    w_u32(0x800E8D50u, segments);
    w_u32(0x800E8D54u, (uint32)count);
    w_u32(0x800E8D58u, 0u);
    w_u32(0x800E8D5Cu, argument);
    w_u32(0x800E8D60u, 0u);
    w_u32(0x800E8D68u, setup);
    w_u16(segments + 16u, 0u);
    w_u16(segments + 18u, 0u);
    lane_count = r_u8(0x800E0595u) == 1u ? 5 : r_u8(0x800E0595u) == 0u ? 6 : 4;
    lane = 1;
    for (index = 0; index < count; ++index)
    {
        uint32 segment = segments + (uint32)index * 20u;
        sint32 sine = rsin(angle * 4);
        sint32 cosine = rcos(angle * 4);
        sint32 kind;
        w_u32(0x800D2498u, 1103515245u * r_u32(0x800D2498u) + 12345u);
        w_u16(segment, (uint16)(r_u16(0x800CB070u) + (angle > 0 ? sine / 4 : 0)));
        w_u16(segment + 2u, r_u16(r_u32(0x8008349Cu) + 4u));
        w_u16(segment + 4u, (uint16)(r_u16(0x800CB074u) + 10096 + 8 * angle));
        w_u16(segment + 6u, (uint16)((1024 - (angle > 0 ? cosine >> 3 : 0)) & 0x0FFF));
        w_u16(segment + 8u, (uint16)(r_u16(0x800CB070u) + (sint32)((r_u32(0x800D2498u) >> 16) % 470u) - 235));
        w_u16(segment + 10u, r_u16(r_u32(0x8008349Cu) + 4u));
        w_u16(segment + 12u, r_u16(segment + 4u));
        if (index == (lane + 1) * ((count - 1) / (lane_count + 1)))
        {
            kind = 3;
            ++lane;
        }
        else if (index == count - 1)
            kind = 0;
        else
            kind = (sint32)((r_u32(0x800D2498u) >> 16) & 3u) == 0 ? 4 : 2;
        w_u16(segment + 14u, (uint16)kind);
        w_u16(segment + 16u, 12u);
        w_u16(segment + 18u, 0u);
        angle += 208;
    }
    return count > 0;
}

sint32 race_events_fn_8003b508(void)
{
    sint32 players = (sint32)r_u32(0x80083478u);
    sint32 index;
    sint32 side;
    uint32 textured;
    uint32 flat;
    uint32 windows;

    FUNCTION_MARKER(0x8003B508u, "MAIN.EXE");
    w_u32(0x800B68A8u, UINT32_MAX);
    w_u32(0x800B68B8u, r_u32(0x800914ACu));
    w_u32(0x8008349Cu, 0x80092CF8u);
    w_u16(0x800B6A00u, UINT16_MAX);
    w_u16(0x800B69FEu, UINT16_MAX);
    w_u32(0x80083498u, 0u);
    w_u32(0x800B69FCu, 0u);
    w_u16(0x800CB070u, 0u);
    w_u16(0x800CB072u, 0u);
    w_u32(0x800CB074u, (uint32)-20000);
    w_u32(0x800B68B4u, 0u);
    w_u32(0x800B6870u, 0x800CB068u);
    w_u32(0x800B6B80u, 0x800B6870u);
    for (index = 0; index < 14; ++index)
        w_u8(0x800CB055u - (uint32)index, 6u);
    w_u32(0x800B6A98u, 1u);
    if (r_u32(0x8008348Cu) == 2u)
    {
        w_u32(0x80092CF8u + 16u, r_u32(0x80092B80u));
        w_u32(0x80092CF8u + 20u, r_u32(0x80092B84u));
        w_u32(0x80092CF8u + 24u, r_u32(0x80092B88u));
        w_u8(0x80092CF8u + 1u, r_u8(0x80092B8Cu));
    }
    else
    {
        w_u32(0x80092CF8u + 16u, r_u32(0x80092BB0u));
        w_u32(0x80092CF8u + 20u, r_u32(0x80092BB4u));
        w_u32(0x80092CF8u + 24u, r_u32(0x80092BB8u));
        w_u8(0x80092CF8u + 1u, r_u8(0x80092BBCu));
    }
    w_u32(0x800834ACu, 0x800914CCu);
    textured = game_alloc_arena_bytes(10296 * players);
    w_u32(0x800B6878u, textured);
    w_u32(0x800B687Cu, textured + (uint32)(5148 * players));
    for (index = 0; index < 198 * players; ++index)
    {
        uint32 packet = textured + (uint32)index * 52u;
        w_u8(packet + 12u, 0u);
        w_u8(packet + 13u, 0u);
        w_u16(packet + 14u, 32752u);
        w_u16(packet + 26u, 28u);
        w_u16(packet + 24u, 63u);
        w_u8(packet + 36u, 0u);
        w_u8(packet + 37u, 63u);
        w_u8(packet + 48u, 63u);
        w_u8(packet + 49u, 63u);
    }
    flat = game_alloc_arena_bytes(4320 * players);
    w_u32(0x800B6884u, flat);
    w_u32(0x800B6888u, flat + (uint32)(2160 * players));
    for (index = 0; index < 108 * players; ++index)
    {
        uint32 packet = flat + (uint32)index * 40u;
        w_u8(packet + 4u, 108u);
        w_u8(packet + 5u, 108u);
        w_u8(packet + 6u, 108u);
        w_u16(packet + 14u, 32761u);
        w_u16(packet + 22u, 110u);
    }
    windows = game_alloc_arena_bytes(48 * players);
    w_u32(0x800B6890u, windows);
    w_u32(0x800B6894u, windows + (uint32)(24 * players));
    for (index = 0; index < players; ++index)
        for (side = 0; side < 2; ++side)
        {
            uint32 slot = (uint32)(2 * index + side) * 4u;
            w_u32(0x800CB17Cu + slot, game_alloc_arena_bytes(60));
            w_u32(0x800CB18Cu + slot, game_alloc_arena_bytes(1004));
            w_u32(0x800CB19Cu + slot, game_alloc_arena_bytes(1004));
        }
    race_events_fn_8003af00(0x80091498u, r_u32(0x80091498u));
    return results_init_marker_recs();
}

static sint16 race_events_lo_s16(uint32 value)
{
    return (sint16)(uint16)value;
}

static sint16 race_events_hi_s16(uint32 value)
{
    return (sint16)(uint16)(value >> 16);
}

static uint32 race_events_set_lo_s16(uint32 value, sint32 low)
{
    return (value & 0xFFFF0000u) | (uint16)low;
}

static uint32 race_events_set_hi_s16(uint32 value, sint32 high)
{
    return (value & 0x0000FFFFu) | ((uint32)(uint16)high << 16);
}

static sint8 race_events_slope_clamp(sint32 numerator, sint32 denominator)
{
    sint32 value = numerator / denominator;

    if (value < -127)
        value = -127;
    else if (value >= 128)
        value = 127;
    return (sint8)value;
}

sint32 race_events_fn_8003f164(uint32 unused, sint32 count)
{
    uint32 table = r_u32(0x800B6B80u);
    uint32 record = r_u32(table);
    uint32 previous = r_u32(table + (uint32)r_u16(record + 2u) * 4u);
    uint32 control = r_u32(0x800B3FC4u);
    uint32 delta = r_u32(0x800B3FC8u);
    sint16 decoration_gap = 0;
    sint32 index;
    sint32 point;

    FUNCTION_MARKER(0x8003F164u, "MAIN.EXE");
    for (point = 0; point < 14; ++point)
    {
        w_u8(previous + 34u + (uint32)point * 14u, 32u);
        w_u8(previous + 35u + (uint32)point * 14u, 32u);
        w_u8(previous + 36u + (uint32)point * 14u, 72u);
    }
    for (index = 0; index < count; ++index)
    {
        if (race_events_lo_s16(control) != 0)
        {
            control = race_events_set_lo_s16(control, race_events_lo_s16(control) - 1);
            control = race_events_set_hi_s16(control, race_events_hi_s16(control) + race_events_lo_s16(delta));
        }
        else if (count - index < 65)
        {
            delta = race_events_set_lo_s16(delta, -2);
            control = race_events_set_lo_s16(control, race_events_hi_s16(control) / 2);
        }
        else
        {
            sint32 value;

            delta = race_events_set_hi_s16(delta, (global_fn_8006e9d8() & 3) + 10);
            value = global_fn_8006e9d8() % 64 - race_events_hi_s16(control) / 2;
            if ((sint16)value < 0)
            {
                control = race_events_set_lo_s16(control, -(sint16)value);
                delta = race_events_set_lo_s16(delta, -2);
            }
            else
            {
                control = race_events_set_lo_s16(control, (sint16)value);
                delta = race_events_set_lo_s16(delta, 2);
            }
        }

        w_u8(record + 37u, (uint8)(r_u8(previous + 37u) - 94u));
        w_u8(record + 51u, (uint8)(r_u8(previous + 51u) - 94u));
        w_u8(record + 65u, (uint8)(r_u8(previous + 65u) - 94u));
        w_u8(record + 79u, (uint8)(r_u8(previous + 79u) - 59u));
        w_u8(record + 93u, (uint8)(r_u8(previous + 93u) - 27u));
        w_u8(record + 107u, (uint8)(r_u8(previous + 107u) - 16u));
        w_u8(record + 121u, (uint8)(r_u8(previous + 121u) - 40u));
        w_u8(record + 135u, (uint8)(r_u8(previous + 135u) - 67u));
        w_u8(record + 149u, (uint8)(r_u8(previous + 149u) - 98u));
        w_u8(record + 163u, (uint8)(r_u8(previous + 163u) - 98u));
        w_u8(record + 177u, (uint8)(r_u8(previous + 177u) - 84u));
        w_u8(record + 191u, (uint8)(r_u8(previous + 191u) - 84u));
        w_u8(record + 205u, (uint8)(r_u8(previous + 205u) - 84u));
        w_u8(record + 219u, (uint8)(r_u8(previous + 219u) - 84u));
        for (point = 0; point < 14; ++point)
        {
            uint32 entry = record + 33u + (uint32)point * 14u;
            sint32 control_high = race_events_hi_s16(control);
            sint32 delta_high = race_events_hi_s16(delta);
            sint32 high_div16 = control_high / 16;

            w_u8(entry, 14u);
            w_u8(entry + 1u, (uint8)(control_high / 2 + r_u8(previous + 34u + (uint32)point * 14u)));
            w_u8(entry + 2u, (uint8)(point * ((uint8)(control >> 16) + 32u)));
            w_u8(entry + 3u, (uint8)((16 * ((delta_high - high_div16) / 2)) | ((delta_high - (high_div16 - 4)) / 2)));
            w_u8(entry + 5u, 0u);
            w_u8(entry + 6u, 0u);
            w_u16(entry + 7u, (uint16)-256);
            w_u8(entry + 9u, 2u);
        }
        {
            sint32 denominator = 16 * ((sint32)r_u8(record + 59u) - (sint32)r_u8(record + 73u));
            sint8 x_slope = race_events_slope_clamp(((sint32)r_u8(record + 58u) - (sint32)r_u8(record + 72u)) << 9, denominator);
            sint8 z_slope = race_events_slope_clamp(((sint32)r_u8(record + 60u) - (sint32)r_u8(record + 74u)) << 9, denominator);

            w_u8(record + 80u, (uint8)-x_slope);
            w_u8(record + 81u, (uint8)-z_slope);
        }
        {
            sint32 denominator = 16 * ((sint32)r_u8(record + 171u) - (sint32)r_u8(record + 157u));
            sint8 x_slope = race_events_slope_clamp(((sint32)r_u8(record + 170u) - (sint32)r_u8(record + 156u)) << 9, denominator);
            sint8 z_slope = race_events_slope_clamp(((sint32)r_u8(record + 172u) - (sint32)r_u8(record + 158u)) << 9, denominator);

            w_u8(record + 164u, (uint8)-x_slope);
            w_u8(record + 165u, (uint8)-z_slope);
        }

        previous = record;
        if (decoration_gap >= 31 && (global_fn_8006e9d8() & 0xEu) != 0)
        {
            sint32 placed = 0;
            sint16 position = (sint16)((global_fn_8006e9d8() & 3) | 4);

            while (position < 9 && placed < 3)
            {
                sint32 random = global_fn_8006e9d8() & 3;
                sint32 kind = random + 2;

                if (random == 1)
                    kind = 4;
                else if (random == 3)
                {
                    if ((global_fn_8006e9d8() & 7) == 0)
                    {
                        if ((global_fn_8006e9d8() & 1) != 0)
                            break;
                        decoration_gap = 0;
                    }
                    else
                        kind = 2;
                }
                else if (kind >= 6)
                    break;
                decoration_gap = 0;
                w_u8(record + (uint32)position * 14u + 33u, (uint8)(r_u8(record + (uint32)position * 14u + 33u) | (uint8)(kind << 5)));
                ++placed;
                ++position;
            }
        }
        ++decoration_gap;
        record = r_u32(table + (uint32)r_u16(previous) * 4u);
    }
    return index < count;
}

static uint8 race_events_clamp_u8(sint32 value)
{
    return (uint8)(value < 0 ? 0 : value > 255 ? 255 : value);
}

static void race_events_hsv_to_rgb(uint8 output[3], sint32 hue, sint32 saturation, sint32 value)
{
    sint32 sector;
    sint32 fraction;
    sint32 low;
    sint32 falling;
    sint32 rising;

    if (hue == 1536)
        hue = 0;
    sector = hue / 256;
    fraction = hue % 256;
    low = value * (256 - saturation) / 256;
    falling = value * (256 - saturation * fraction / 256) / 256;
    rising = value * (256 - saturation * (256 - fraction) / 256) / 256;
    value = race_events_clamp_u8(value);
    low = race_events_clamp_u8(low);
    falling = race_events_clamp_u8(falling);
    rising = race_events_clamp_u8(rising);
    switch (sector)
    {
        case 0:
            output[0] = (uint8)value;
            output[1] = (uint8)rising;
            output[2] = (uint8)low;
            break;
        case 1:
            output[0] = (uint8)falling;
            output[1] = (uint8)value;
            output[2] = (uint8)low;
            break;
        case 2:
            output[0] = (uint8)low;
            output[1] = (uint8)value;
            output[2] = (uint8)rising;
            break;
        case 3:
            output[0] = (uint8)low;
            output[1] = (uint8)falling;
            output[2] = (uint8)value;
            break;
        case 4:
            output[0] = (uint8)rising;
            output[1] = (uint8)low;
            output[2] = (uint8)value;
            break;
        default:
            output[0] = (uint8)value;
            output[1] = (uint8)low;
            output[2] = (uint8)falling;
            break;
    }
}

uint32 race_events_init_fractal_light(uint32 state, sint32 count)
{
    uint8 ambient[3];
    uint8 light_colors[2][3];
    uint8 hsv_rgb[3];
    uint8 light_a[56];
    uint8 light_b[56];
    VECTOR light_vectors[2];
    MATRIX rotation = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    SVECTOR source;
    VECTOR transformed;
    uint32 table = r_u32(0x800B6B80u);
    uint32 previous_record = r_u32(table);
    uint32 light_state = r_u32(state + 52u);
    uint32 colors;
    uint32 output;
    sint32 random;
    sint32 course;
    sint32 index;

    FUNCTION_MARKER(0x8003F7D4u, "MAIN.EXE");
    ambient[0] = r_u8(0x800B3FCCu);
    ambient[1] = r_u8(0x800B3FCDu);
    ambient[2] = r_u8(0x800B3FCEu);
    random = global_fn_8006e9d8() & 3;
    if (random < 2)
    {
        sint32 hue_offset = global_fn_8006e9d8() % 128;
        sint32 secondary_saturation = (hue_offset / 2 + 64) / 2;

        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 672, hue_offset / 2 + 64, 128);
        w_u8(0x80092BC0u, hsv_rgb[0]);
        w_u8(0x80092BC1u, hsv_rgb[1]);
        w_u8(0x80092BC2u, hsv_rgb[2]);
        race_events_hsv_to_rgb(light_colors[1], hue_offset + 672, secondary_saturation, 64);
        w_u8(0x80092BC4u, hsv_rgb[0]);
        w_u8(0x80092BC5u, hsv_rgb[1]);
        w_u8(0x80092BC6u, hsv_rgb[2]);
        secondary_saturation = secondary_saturation * 2 + 128;
        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 800, secondary_saturation, 96);
        w_u8(0x80092BC8u, hsv_rgb[0]);
        w_u8(0x80092BC9u, hsv_rgb[1]);
        w_u8(0x80092BCAu, hsv_rgb[2]);
        race_events_hsv_to_rgb(ambient, hue_offset + 800, 128, 48);
        light_colors[0][0] = 0xE6u;
        light_colors[0][1] = 0xB9u;
        light_colors[0][2] = 0x8Cu;
        course = 1;
    }
    else if (random == 2)
    {
        sint32 hue_offset = global_fn_8006e9d8() % 128;

        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 162, 135, 255);
        w_u8(0x80092BC0u, hsv_rgb[0]);
        w_u8(0x80092BC1u, hsv_rgb[1]);
        w_u8(0x80092BC2u, hsv_rgb[2]);
        race_events_hsv_to_rgb(light_colors[0], hue_offset + 162, 135, 127);
        w_u8(0x80092BC4u, hsv_rgb[0]);
        w_u8(0x80092BC5u, hsv_rgb[1]);
        w_u8(0x80092BC6u, hsv_rgb[2]);
        race_events_hsv_to_rgb(hsv_rgb, 2 * (hue_offset + 82) + 1100, 150, 150);
        w_u8(0x80092BC8u, hsv_rgb[0]);
        w_u8(0x80092BC9u, hsv_rgb[1]);
        w_u8(0x80092BCAu, hsv_rgb[2]);
        race_events_hsv_to_rgb(ambient, 2 * (hue_offset + 82) + 1100, 150, 75);
        light_colors[1][0] = 'F';
        light_colors[1][1] = 'P';
        light_colors[1][2] = 'Z';
        course = 1;
    }
    else
    {
        sint32 hue_offset = global_fn_8006e9d8() % 64;

        race_events_hsv_to_rgb(hsv_rgb, 934, 196 - hue_offset, 112 - hue_offset);
        w_u8(0x80092B90u, hsv_rgb[0]);
        w_u8(0x80092B91u, hsv_rgb[1]);
        w_u8(0x80092B92u, hsv_rgb[2]);
        w_u8(0x80092B94u, hsv_rgb[0]);
        w_u8(0x80092B95u, hsv_rgb[1]);
        w_u8(0x80092B96u, hsv_rgb[2]);
        race_events_hsv_to_rgb(ambient, 998, 148 - hue_offset, 80 - hue_offset);
        w_u8(0x80092B98u, ambient[0]);
        w_u8(0x80092B99u, ambient[1]);
        w_u8(0x80092B9Au, ambient[2]);
        light_colors[0][0] = (uint8)(96 - hue_offset / 2);
        light_colors[0][1] = light_colors[0][0];
        light_colors[0][2] = 48u;
        light_colors[1][0] = ' ';
        light_colors[1][1] = ' ';
        light_colors[1][2] = ' ';
        course = 2;
    }
    w_u32(0x8008348Cu, (uint32)course);

    source.vx = (sint16)r_u16(0x800B3FD8u);
    source.vy = (sint16)r_u16(0x800B3FDAu);
    source.vz = (sint16)r_u16(0x800B3FDCu);
    source.pad = 0;
    index = (sint32)r_u32(state + 40u);
    RotMatrixX((sint32)((uint32)r_u32(0x800935E0u + (uint32)index * 8u) << 12) / 360, &rotation);
    RotMatrixY((sint32)((uint32)r_u32(0x800935E4u + (uint32)index * 8u) << 12) / 360, &rotation);
    SetRotMatrix(&rotation);
    ApplyRotMatrix(&source, &transformed);
    light_vectors[0].vx = -transformed.vx;
    light_vectors[0].vy = -transformed.vy;
    light_vectors[0].vz = -transformed.vz;
    light_vectors[0].pad = 0;
    light_vectors[1].vx = transformed.vx;
    light_vectors[1].vy = -2 * transformed.vy;
    light_vectors[1].vz = -transformed.vz;
    light_vectors[1].pad = 0;
    VectorNormal(&light_vectors[0], &light_vectors[0]);
    VectorNormal(&light_vectors[1], &light_vectors[1]);

    w_u16(light_state, (uint16)-light_vectors[0].vx);
    w_u16(light_state + 2u, (uint16)-light_vectors[0].vy);
    w_u16(light_state + 4u, (uint16)-light_vectors[0].vz);
    w_u16(light_state + 6u, 0u);
    w_u16(light_state + 8u, (uint16)-light_vectors[1].vx);
    w_u16(light_state + 10u, (uint16)-light_vectors[1].vy);
    w_u16(light_state + 12u, (uint16)-light_vectors[1].vz);
    w_u16(light_state + 14u, 0u);
    for (index = 16; index <= 22; index += 2)
        w_u16(light_state + (uint32)index, 0u);
    for (index = 0; index < 3; ++index)
    {
        w_u16(light_state + 24u + (uint32)index * 6u, (uint16)(32u * light_colors[0][index]));
        w_u16(light_state + 26u + (uint32)index * 6u, (uint16)(32u * light_colors[1][index]));
        w_u16(light_state + 28u + (uint32)index * 6u, 0u);
        w_u32(light_state + 44u + (uint32)index * 4u, ambient[index]);
    }

    colors = game_alloc_arena_bytes((sint32)(56u * (uint32)count));
    w_u32(state + 56u, colors);
    previous_record = r_u32(table + (uint32)r_u16(previous_record + 2u) * 4u);
    output = colors;
    for (index = 0; index <= count; ++index)
    {
        uint8 *current_light = (index & 1) != 0 ? light_b : light_a;
        uint8 *prior_light = index == 0 ? NULL : ((index & 1) != 0 ? light_a : light_b);
        uint32 record = r_u32(table + (uint32)r_u16(previous_record) * 4u);
        uint32 selector = r_u8(record + 14u);
        uint32 current_vertex = record + 30u + (selector >> 4) * 14u;
        uint32 prior_vertex = previous_record + 30u + (selector & 15u) * 14u;
        sint32 point;
        sint32 sum_r = 0;
        sint32 sum_g = 0;
        sint32 sum_b = 0;
        sint32 scale;

        w_u8(record + 6u, 0u);
        for (point = 0; point < 14; ++point)
        {
            VECTOR along;
            VECTOR across;
            VECTOR normal;
            sint32 channel;
            sint32 light;

            along.vx = (sint16)r_u16(previous_record + 8u) + 16 * r_u8(prior_vertex) - ((sint16)r_u16(record + 8u) + 16 * r_u8(current_vertex));
            along.vy = (sint16)r_u16(previous_record + 10u) + 16 * r_u8(prior_vertex + 1u) - ((sint16)r_u16(record + 10u) + 16 * r_u8(current_vertex + 1u));
            along.vz = (sint16)r_u16(previous_record + 12u) + 16 * r_u8(prior_vertex + 2u) - ((sint16)r_u16(record + 12u) + 16 * r_u8(current_vertex + 2u));
            along.pad = 0;
            if (point >= 12)
            {
                across.vx = 16 * (r_u8(current_vertex - 14u) - r_u8(current_vertex));
                across.vy = 16 * (r_u8(current_vertex - 13u) - r_u8(current_vertex + 1u));
                across.vz = 16 * (r_u8(current_vertex - 12u) - r_u8(current_vertex + 2u));
                OuterProduct12(&across, &along, &normal);
            }
            else
            {
                across.vx = 16 * (r_u8(current_vertex + 14u) - r_u8(current_vertex));
                across.vy = 16 * (r_u8(current_vertex + 15u) - r_u8(current_vertex + 1u));
                across.vz = 16 * (r_u8(current_vertex + 16u) - r_u8(current_vertex + 2u));
                OuterProduct12(&along, &across, &normal);
            }
            across.pad = 0;
            VectorNormal(&normal, &normal);
            for (channel = 0; channel < 3; ++channel)
            {
                sint32 total = 0;
                sint32 source_channel = channel;

                for (light = 0; light < 2; ++light)
                {
                    sint32 dot = (light_vectors[light].vx * normal.vx + light_vectors[light].vy * normal.vy + light_vectors[light].vz * normal.vz) >> 12;
                    if (dot >= 0)
                        total += (dot * light_colors[light][source_channel]) >> 12;
                }
                current_light[(uint32)point * 4u + (uint32)channel] = race_events_clamp_u8(total);
            }
            current_light[(uint32)point * 4u + 3u] = 0u;
            current_vertex += 14u;
            prior_vertex += 14u;
        }

        current_vertex = record + 30u + (selector >> 4) * 14u + 3u * 14u;
        w_u16(record + 4u, (uint16)((output - colors) >> 2));
        for (point = 3; point < 10; ++point)
        {
            sint32 average = (current_light[(uint32)point * 4u] + current_light[(uint32)point * 4u + 1u] + current_light[(uint32)point * 4u + 2u] + ambient[0] + ambient[1] + ambient[2]) / 3;

            w_u8(current_vertex + 13u, (uint8)average);
            sum_r += current_light[(uint32)point * 4u];
            sum_g += current_light[(uint32)point * 4u + 1u];
            sum_b += current_light[(uint32)point * 4u + 2u];
            current_vertex += 14u;
        }
        sum_r += 7 * ambient[0];
        sum_g += 7 * ambient[1];
        sum_b += 7 * ambient[2];
        if (sum_r < 33 && sum_g < 33 && sum_b < 33)
            sum_r = sum_g = sum_b = 1;
        scale = 1044480 / (sum_r >= sum_g && sum_r >= sum_b ? sum_r : (sum_g >= sum_b ? sum_g : sum_b));
        current_vertex = record + 30u + (selector >> 4) * 14u + 3u * 14u;
        for (point = 3; point < 10; ++point)
        {
            sint32 value = (current_light[(uint32)point * 4u] + current_light[(uint32)point * 4u + 1u] + current_light[(uint32)point * 4u + 2u] + ambient[0] + ambient[1] + ambient[2]) << 12;
            sint32 shade = value / (21 * scale);

            w_u8(current_vertex + 13u, race_events_clamp_u8(shade));
            current_vertex += 14u;
        }

        if (prior_light != NULL)
        {
            sint32 red = race_events_clamp_u8((sum_r * scale) >> 12);
            sint32 green = race_events_clamp_u8((sum_g * scale) >> 12);
            sint32 blue = race_events_clamp_u8((sum_b * scale) >> 12);

            w_u8(output, (uint8)red);
            w_u8(output + 1u, (uint8)green);
            w_u8(output + 2u, (uint8)blue);
            w_u8(output + 3u, 60u);
            w_u8(output + 4u, (uint8)(red / 2));
            w_u8(output + 5u, (uint8)(green / 2));
            w_u8(output + 6u, (uint8)(blue / 2));
            w_u8(output + 7u, 60u);
            output += 8u;
            for (point = 0; point < 14; ++point)
            {
                uint8 flags = r_u8(record + 16u + (uint32)point);

                if ((flags & 6u) == 6u)
                    break;
                if ((flags & 8u) == 0u)
                {
                    red = (current_light[(uint32)point * 4u] + current_light[(uint32)(point + 1) * 4u] + prior_light[(uint32)point * 4u] + prior_light[(uint32)(point + 1) * 4u]) / 4 + ambient[0];
                    green = (current_light[(uint32)point * 4u + 1u] + current_light[(uint32)(point + 1) * 4u + 1u] + prior_light[(uint32)point * 4u + 1u] + prior_light[(uint32)(point + 1) * 4u + 1u]) / 4 + ambient[1];
                    blue = (current_light[(uint32)point * 4u + 2u] + current_light[(uint32)(point + 1) * 4u + 2u] + prior_light[(uint32)point * 4u + 2u] + prior_light[(uint32)(point + 1) * 4u + 2u]) / 4 + ambient[2];
                    w_u8(output, race_events_clamp_u8(red));
                    w_u8(output + 1u, race_events_clamp_u8(green));
                    w_u8(output + 2u, race_events_clamp_u8(blue));
                    w_u8(output + 3u, 44u);
                    output += 4u;
                }
            }
        }
        previous_record = record;
    }
    return count < 0 ? colors : 1u;
}
