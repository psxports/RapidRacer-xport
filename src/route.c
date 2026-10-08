#include "vehicle.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "route.h"
#include "global.h"
#include "motion.h"
#include "xport_trace.h"

static sint32 route_sample_geom_values(uint32 base, sint32 index, sint32 apply_height, sint32 output[4])
{
    uint32 record = base + (uint32)index * 14u;
    uint32 first_index = (uint32)r_u8(record + 34u);
    uint32 phase = r_u16(0x800B3D94u);
    uint32 table_selector = (uint32)r_u8(record + 36u);
    uint32 first_table = 0x800F0508u + (table_selector & 0x0Fu) * 512u;
    uint32 second_index;
    uint32 second_table;
    sint32 correction;
    uint32 wobble_phase;
    sint32 result;

    correction = -(sint32)r_u8(first_table + ((2u * first_index + phase) & 0x1FFu));
    second_index = (uint32)r_u8(record + 35u);
    second_table = 0x800F0508u + (table_selector >> 4) * 512u;
    correction -= (sint32)r_u8(second_table + ((2u * second_index + phase) & 0x1FFu));
    wobble_phase = (uint16)(phase - r_u16(record + 40u));
    if (wobble_phase < 256u)
    {
        sint32 wobble = math_sra_signed((sint32)(2u * r_u8(0x800F2308u + wobble_phase)) - 63, 1u);

        correction -= wobble;
        if (correction < -127)
            correction = -127;
    }
    {
        uint32 coordinate = (uint32)r_u8(record + 30u);

        output[0] = (sint16)r_u16(base + 8u) + 16 * (sint32)coordinate;
    }
    {
        uint32 coordinate = (uint32)r_u8(record + 31u);

        output[1] = (sint16)r_u16(base + 10u) + 16 * (sint32)coordinate;
    }
    {
        uint32 coordinate = (uint32)r_u8(record + 32u);

        output[2] = (sint16)r_u16(base + 12u) + 16 * (sint32)coordinate;
    }
    output[3] = (sint32)(2u * ((uint32)r_u8(record + 33u) & 0x1Fu));
    if (apply_height)
        output[1] -= correction;
    result = (sint8)r_u8(record + 38u);
    if (result != 0 || (result = (sint8)r_u8(record + 39u)) != 0)
    {
        if (correction < 0)
        {
            if (!apply_height)
                output[1] -= correction;
            output[0] += math_sra_signed(correction * (sint8)r_u8(record + 38u), 5u);
            result = output[2] + math_sra_signed(correction * (sint8)r_u8(record + 39u), 5u);
            output[2] = result;
        }
    }
    return result;
}

static volatile sint32 rr_route_trap_line;
static volatile uintptr_t rr_route_trap_state;
static volatile uint32 rr_route_trap_object;
static volatile uint32 rr_route_trap_type;
static volatile uint32 rr_route_trap_crossings;
static volatile VECTOR rr_route_trap_vertices[4];
static volatile VECTOR rr_route_trap_position;
static volatile sint32 rr_route_trap_transitions;
static volatile uint32 rr_route_trace_count;
static volatile uint32 rr_route_trace_object[16];
static volatile uint32 rr_route_trace_entry[16];
static volatile uint32 rr_route_trace_type[16];
static volatile uint32 rr_route_trace_crossings[16];
static volatile sint32 rr_route_trace_action[16];
static volatile VECTOR rr_route_trace_vertices[4][4];

static void route_contact_trap_at(sint32 line)
{
    rr_route_trap_line = line;
    abort();
}

#define route_contact_trap() route_contact_trap_at(__LINE__)

static sint32 route_contact_add(sint32 left, sint32 right)
{
    return (sint32)((uint32)left + (uint32)right);
}

static sint32 route_contact_sub(sint32 left, sint32 right)
{
    return (sint32)((uint32)left - (uint32)right);
}

static sint32 route_contact_mul(sint32 left, sint32 right)
{
    return (sint32)((uint32)left * (uint32)right);
}

static sint32 route_contact_nclip(sint32 ax, sint32 az, sint32 bx, sint32 bz, sint32 cx, sint32 cz)
{
    sint64 result = (sint64)(sint16)ax * (sint16)bz + (sint64)(sint16)bx * (sint16)cz + (sint64)(sint16)cx * (sint16)az;

    result -= (sint64)(sint16)ax * (sint16)cz + (sint64)(sint16)bx * (sint16)az + (sint64)(sint16)cx * (sint16)bz;
    return (sint32)(0u - (uint32)result);
}

static uint32 route_contact_lzcr(uint32 value)
{
    uint32 count = 0u;

    if ((value & 0x80000000u) != 0u)
        value = ~value;
    while (count < 32u && (value & 0x80000000u) == 0u)
    {
        ++count;
        value <<= 1;
    }
    return count;
}

sint32 route_sample_geom(uint32 base, sint32 index, sint32 apply_height, uint32 output)
{
    sint32 values[4];
    sint32 result;

    FUNCTION_MARKER(0x8001A3B8u, "MAIN.EXE");
    result = route_sample_geom_values(base, index, apply_height, values);
    w_u32(output, (uint32)values[0]);
    w_u32(output + 4u, (uint32)values[1]);
    w_u32(output + 8u, (uint32)values[2]);
    w_u32(output + 12u, (uint32)values[3]);
    return result;
}

static sint32 route_contact_position(sint32 output[3], sint32 object_index, sint32 entry)
{
    uint32 table;
    uint32 slot;
    uint32 object;
    uint32 linked;
    uint32 linked_index;
    uint32 packed;
    uint32 left_index;
    uint32 right_index;
    uint32 left_offset;
    uint32 right_offset;
    uint32 value;
    sint32 cursor;
    sint32 count = 2;
    uint16 x;
    uint16 y;
    uint16 z;
    sint32 result;

    FUNCTION_MARKER(0x8001A57Cu, "MAIN.EXE");
    table = r_u32(0x800B6B80u);
    slot = table + (uint32)(sint16)object_index * 4u;
    object = r_u32(slot);
    packed = r_u8(object + 14u);
    left_index = packed >> 4;
    right_index = packed & 0x0Fu;
    for (cursor = 0; cursor < (sint16)entry; ++cursor)
    {
        uint32 type = (uint32)r_u8(object + 16u + (uint32)cursor) & 3u;

        if (type != 2u)
            ++left_index;
        if (type != 1u)
            ++right_index;
    }
    left_offset = left_index * 14u;
    right_offset = right_index * 14u;
    table = r_u32(0x800B6B80u);
    slot = table + (uint32)(sint16)object_index * 4u;
    object = r_u32(slot);
    linked_index = r_u16(object + 2u);
    linked = r_u32(table + linked_index * 4u);
    value = r_u8(object + left_offset + 30u);
    value += r_u8(linked + right_offset + 30u);
    x = (uint16)(value << 4);
    object = r_u32(slot);
    linked_index = r_u16(object + 2u);
    linked = r_u32(table + linked_index * 4u);
    value = r_u8(object + left_offset + 31u);
    value += r_u8(linked + right_offset + 31u);
    y = (uint16)(value << 4);
    object = r_u32(slot);
    linked_index = r_u16(object + 2u);
    linked = r_u32(table + linked_index * 4u);
    value = r_u8(object + left_offset + 32u);
    value += r_u8(linked + right_offset + 32u);
    z = (uint16)(value << 4);
    object = r_u32(slot);
    if (((uint32)r_u8(object + 16u + (uint32)(sint16)entry) & 3u) != 2u)
    {
        left_offset = (left_index + 1u) * 14u;
        x = (uint16)(x + ((uint32)r_u8(object + left_offset + 30u) << 4));
        object = r_u32(slot);
        y = (uint16)(y + ((uint32)r_u8(object + left_offset + 31u) << 4));
        object = r_u32(slot);
        z = (uint16)(z + ((uint32)r_u8(object + left_offset + 32u) << 4));
        ++count;
        object = r_u32(slot);
    }
    if (((uint32)r_u8(object + 16u + (uint32)(sint16)entry) & 3u) != 1u)
    {
        right_offset = (right_index + 1u) * 14u;
        linked_index = r_u16(object + 2u);
        linked = r_u32(table + linked_index * 4u);
        x = (uint16)(x + ((uint32)r_u8(linked + right_offset + 30u) << 4));
        object = r_u32(slot);
        linked_index = r_u16(object + 2u);
        linked = r_u32(table + linked_index * 4u);
        y = (uint16)(y + ((uint32)r_u8(linked + right_offset + 31u) << 4));
        object = r_u32(slot);
        linked_index = r_u16(object + 2u);
        linked = r_u32(table + linked_index * 4u);
        ++count;
        z = (uint16)(z + ((uint32)r_u8(linked + right_offset + 32u) << 4));
    }
    result = (sint16)x / count;
    object = r_u32(slot);
    result = (sint16)(result + (sint32)r_u16(object + 8u));
    output[0] = result;
    result = (sint16)y / count;
    object = r_u32(slot);
    result = (sint16)(result + (sint32)r_u16(object + 10u));
    output[1] = result;
    result = (sint16)z / count;
    object = r_u32(slot);
    result = (sint16)(result + (sint32)r_u16(object + 12u));
    output[2] = result;
    return result;
}

void route_contact_init(ROUTE_CONTACT *state, sint32 object_index, sint32 entry)
{
    uint32 table;
    uint32 slot;
    uint32 object;
    uint32 packed;
    uint32 left;
    uint32 right;
    sint32 cursor;

    FUNCTION_MARKER(0x8001A920u, "MAIN.EXE");
    table = r_u32(0x800B6B80u);
    slot = table + (uint32)(sint16)object_index * 4u;
    object = r_u32(slot);
    state->entry = (uint32)(sint16)entry;
    state->object = object;
    object = r_u32(slot);
    packed = r_u8(object + 14u);
    left = packed >> 4;
    right = packed & 0x0Fu;
    for (cursor = 0; cursor < (sint16)entry; ++cursor)
    {
        uint32 type = (uint32)r_u8(object + 16u + (uint32)cursor) & 3u;

        if (type != 2u)
            ++left;
        if (type != 1u)
            ++right;
    }
    state->left = left;
    state->right = right;
    route_contact_position(state->position, (sint16)object_index, (sint16)entry);
    state->height = 0;
    state->penetration = 0;
}

sint32 route_contact_update(ROUTE_CONTACT *state)
{
    VECTOR position;
    VECTOR vertices[4];
    VECTOR direction;
    VECTOR secondary;
    sint16 visited[6] = {-1, -1, -1, -1, -1, -1};
    sint32 flags;
    sint32 transitions = 0;
    sint32 base_vertex = 0;
    sint32 count;
    sint32 plane;
    uint32 mode;
    uint32 object;
    uint32 type;

    FUNCTION_MARKER(0x8001A9FCu, "MAIN.EXE");
    rr_route_trace_count = 0u;
    RotTrans(&state->sample, &position, &flags);
    position.vx = route_contact_sub(position.vx, (sint32)state->position[0]);
    position.vy = route_contact_sub(position.vy, (sint32)state->position[1]);
    position.vz = route_contact_sub(position.vz, (sint32)state->position[2]);

    for (;;)
    {
        uint32 low_type;
        uint32 crossings = 0u;
        sint16 key;
        sint32 index;
        sint32 action = 0;

        object = state->object;
        type = (uint32)r_u8(object + state->entry + 16u);
        low_type = type & 3u;
        if ((type & 0x0Cu) == 0x0Cu)
            route_contact_trap();

        {
            uint16 first = r_u16(object);
            uint16 route = first != 0u ? (uint16)(first - 1u) : (uint16)(r_u16(object + 2u) + 1u);

            key = (sint16)(((route & 0xFFu) << 8) + (uint16)state->entry);
        }
        for (index = 0; index < 6; ++index)
        {
            if (visited[index] == key)
                route_contact_trap();
        }
        for (index = 5; index > 0; --index)
            visited[index] = visited[index - 1];
        visited[0] = key;

        count = 0;
        {
            uint32 current = state->object;
            sint32 left = (sint32)state->left;

            route_sample_geom_host(current, left, low_type == 0u, (sint32 *)&vertices[count++]);
        }
        if (low_type != 2u)
        {
            sint32 left = (sint32)state->left;
            uint32 current = state->object;

            route_sample_geom_host(current, route_contact_add(left, 1), low_type == 0u, (sint32 *)&vertices[count++]);
        }
        if (low_type != 1u)
        {
            uint32 current = state->object;
            uint32 table = r_u32(0x800B6B80u);
            uint16 link_index = r_u16(current + 2u);
            sint32 right = (sint32)state->right;
            uint32 linked = r_u32(table + (uint32)link_index * 4u);

            route_sample_geom_host(linked, route_contact_add(right, 1), low_type == 0u, (sint32 *)&vertices[count++]);
        }
        {
            uint32 current = state->object;
            sint32 right = (sint32)state->right;
            uint16 link_index = r_u16(current + 2u);
            uint32 table = r_u32(0x800B6B80u);
            uint32 linked = r_u32(table + (uint32)link_index * 4u);

            route_sample_geom_host(linked, right, low_type == 0u, (sint32 *)&vertices[count++]);
        }

        for (index = 0; index < count; ++index)
        {
            vertices[index].vx = (sint16)((uint16)vertices[index].vx - state->position[0]);
            vertices[index].vy = (sint16)((uint16)vertices[index].vy - state->position[1]);
            vertices[index].vz = (sint16)((uint16)vertices[index].vz - state->position[2]);
        }
        for (index = 0; index < count; ++index)
        {
            sint32 next = index + 1 == count ? 0 : index + 1;
            crossings <<= 1;
            if (route_contact_nclip(vertices[index].vx, vertices[index].vz, vertices[next].vx, vertices[next].vz, position.vx, position.vz) < 0)
                ++crossings;
        }

        if (crossings == 0u)
            action = 1;
        else if (low_type == 2u)
        {
            switch (crossings)
            {
                case 1u:
                    action = 2;
                    break;
                case 2u:
                    action = 5;
                    break;
                case 3u:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, position.vx, position.vz, 0, 0) >= 0 ? 2 : 5;
                    break;
                case 4u:
                    action = 3;
                    break;
                case 5u:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, position.vx, position.vz, 0, 0) < 0 ? 2 : 3;
                    break;
                case 6u:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, position.vx, position.vz, 0, 0) < 0 ? 3 : 5;
                    break;
                default:
                    route_contact_trap();
            }
        }
        else if (low_type == 1u)
        {
            switch (crossings)
            {
                case 1u:
                    action = 2;
                    break;
                case 2u:
                    action = 3;
                    break;
                case 3u:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, position.vx, position.vz, 0, 0) < 0 ? 3 : 2;
                    break;
                case 4u:
                    action = 4;
                    break;
                case 5u:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, position.vx, position.vz, 0, 0) >= 0 ? 4 : 2;
                    break;
                case 6u:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, position.vx, position.vz, 0, 0) >= 0 ? 3 : 4;
                    break;
                default:
                    route_contact_trap();
            }
        }
        else
        {
            switch (crossings)
            {
                case 1u:
                case 11u:
                    action = 2;
                    break;
                case 2u:
                case 7u:
                    action = 5;
                    break;
                case 3u:
                    action = route_contact_nclip(vertices[3].vx, vertices[3].vz, position.vx, position.vz, 0, 0) >= 0 ? 2 : 5;
                    break;
                case 4u:
                case 14u:
                    action = 3;
                    break;
                case 6u:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, position.vx, position.vz, 0, 0) < 0 ? 3 : 5;
                    break;
                case 8u:
                case 13u:
                    action = 4;
                    break;
                case 9u:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, position.vx, position.vz, 0, 0) >= 0 ? 4 : 2;
                    break;
                case 12u:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, position.vx, position.vz, 0, 0) >= 0 ? 3 : 4;
                    break;
                default:
                    rr_route_trap_state = (uintptr_t)state;
                    rr_route_trap_object = object;
                    rr_route_trap_type = type;
                    rr_route_trap_crossings = crossings;
                    rr_route_trap_position = position;
                    rr_route_trap_transitions = transitions;
                    for (index = 0; index < 4; ++index)
                        rr_route_trap_vertices[index] = vertices[index];
                    route_contact_trap();
            }
        }
        if (rr_route_trace_count < 16u)
        {
            uint32 trace_index = rr_route_trace_count++;

            rr_route_trace_object[trace_index] = object;
            rr_route_trace_entry[trace_index] = state->entry;
            rr_route_trace_type[trace_index] = type;
            rr_route_trace_crossings[trace_index] = crossings;
            rr_route_trace_action[trace_index] = action;
            if (trace_index < 4u)
            {
                for (index = 0; index < 4; ++index)
                    rr_route_trace_vertices[trace_index][index] = vertices[index];
            }
        }
        if (action == 2 && (type & 4u) == 0u)
        {
            uint32 previous = state->entry - 1u;
            state->entry = previous;
            if ((r_u8(state->object + previous + 16u) & 3u) != 2u)
                state->left = (uint32)state->left - (1u);
            if ((r_u8(state->object + state->entry + 16u) & 3u) != 1u)
                state->right = (uint32)state->right - (1u);
            continue;
        }
        if (action == 3 && (type & 8u) == 0u)
        {
            if (low_type != 2u)
                state->left = (uint32)state->left + (1u);
            if (low_type != 1u)
                state->right = (uint32)state->right + (1u);
            state->entry = (uint32)state->entry + (1u);
            continue;
        }
        if (action == 4)
        {
            uint32 old_object = state->object;
            uint16 link_index = r_u16(old_object);
            uint32 table = r_u32(0x800B6B80u);
            uint32 candidate = r_u32(table + (uint32)link_index * 4u);
            uint32 header = (uint32)r_u8(candidate + 14u);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry = 0;

            ++transitions;
            state->object = candidate;
            while ((r_u8(candidate + (uint32)entry + 16u) & 0x0Cu) != 0x0Cu)
            {
                uint32 candidate_type = (uint32)r_u8(candidate + (uint32)entry + 16u) & 3u;
                if (candidate_type == 1u)
                    ++left;
                else
                {
                    if (right == state->left)
                        break;
                    ++right;
                    if (((candidate_type + 1u) & 3u) < 2u)
                        ++left;
                }
                ++entry;
            }
            if ((r_u8(candidate + (uint32)entry + 16u) & 0x0Cu) != 0x0Cu)
            {
                uint32 old_left = state->left;
                state->left = left;
                state->right = old_left;
                state->entry = (uint32)entry;
                continue;
            }
            --transitions;
            state->object = old_object;
            state->surface = 1u;
            direction.vx = route_contact_sub(vertices[0].vx, vertices[1].vx);
            direction.vy = route_contact_sub(vertices[0].vy, vertices[1].vy);
            direction.vz = route_contact_sub(vertices[0].vz, vertices[1].vz);
            base_vertex = 0;
        }
        else if (action == 5)
        {
            uint32 old_object = state->object;
            uint16 link_index = r_u16(old_object + 2u);
            uint32 table = r_u32(0x800B6B80u);
            uint32 candidate = r_u32(table + (uint32)link_index * 4u);
            uint32 header = (uint32)r_u8(candidate + 14u);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry = 0;

            --transitions;
            state->object = candidate;
            while ((r_u8(candidate + (uint32)entry + 16u) & 0x0Cu) != 0x0Cu)
            {
                uint32 candidate_type = (uint32)r_u8(candidate + (uint32)entry + 16u) & 3u;
                if (candidate_type == 2u)
                    ++right;
                else
                {
                    if (left == state->right)
                        break;
                    ++left;
                    if (((candidate_type + 1u) & 3u) < 2u)
                        ++right;
                }
                ++entry;
            }
            if ((r_u8(candidate + (uint32)entry + 16u) & 0x0Cu) != 0x0Cu)
            {
                uint32 old_right = state->right;
                state->right = right;
                state->left = old_right;
                state->entry = (uint32)entry;
                continue;
            }
            ++transitions;
            state->object = old_object;
            state->surface = 1u;
            if (((type + 1u) & 3u) < 2u)
            {
                direction.vx = route_contact_sub(vertices[2].vx, vertices[3].vx);
                direction.vy = route_contact_sub(vertices[2].vy, vertices[3].vy);
                direction.vz = route_contact_sub(vertices[2].vz, vertices[3].vz);
            }
            else
            {
                direction.vx = route_contact_sub(vertices[1].vx, vertices[2].vx);
                direction.vy = route_contact_sub(vertices[1].vy, vertices[2].vy);
                direction.vz = route_contact_sub(vertices[1].vz, vertices[2].vz);
            }
            base_vertex = 2;
        }
        else if (action == 2)
        {
            state->surface = 1u;
            direction.vx = route_contact_sub(vertices[count - 1].vx, vertices[0].vx);
            direction.vy = route_contact_sub(vertices[count - 1].vy, vertices[0].vy);
            direction.vz = route_contact_sub(vertices[count - 1].vz, vertices[0].vz);
            base_vertex = 0;
        }
        else if (action == 3)
        {
            state->surface = 1u;
            if (low_type == 2u)
            {
                direction.vx = route_contact_sub(vertices[0].vx, vertices[1].vx);
                direction.vy = route_contact_sub(vertices[0].vy, vertices[1].vy);
                direction.vz = route_contact_sub(vertices[0].vz, vertices[1].vz);
            }
            else
            {
                direction.vx = route_contact_sub(vertices[1].vx, vertices[2].vx);
                direction.vy = route_contact_sub(vertices[1].vy, vertices[2].vy);
                direction.vz = route_contact_sub(vertices[1].vz, vertices[2].vz);
            }
            base_vertex = 1;
        }
        else
        {
            sint32 other_vertex;
            sint32 third_vertex;
            uint32 header = (uint32)r_u8(object + 14u);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry_limit = (sint32)state->entry;

            for (index = 0; index < entry_limit; ++index)
            {
                uint32 entry = (uint32)r_u8(object + (uint32)index + 16u);
                uint32 entry_type = entry & 3u;

                if (entry_type != 2u)
                    ++left;
                if (entry_type != 1u)
                    ++right;
                if ((entry & 0x0Cu) == 0x0Cu)
                    route_contact_trap();
            }
            if (state->left != left || state->right != right)
                route_contact_trap();
            state->surface = low_type != 0u ? r_u32(0x800F2594u + 32u * (type >> 4)) : 0u;
            if (((type + 1u) & 3u) >= 2u)
            {
                base_vertex = 0;
                other_vertex = 1;
                third_vertex = 2;
            }
            else
            {
                if (route_contact_nclip(vertices[1].vx, vertices[1].vz, vertices[3].vx, vertices[3].vz, position.vx, position.vz) <= 0)
                {
                    base_vertex = 1;
                    other_vertex = 2;
                }
                else
                {
                    base_vertex = 0;
                    other_vertex = 1;
                }
                third_vertex = 3;
            }
            direction.vx = route_contact_sub(vertices[other_vertex].vx, vertices[base_vertex].vx);
            direction.vy = route_contact_sub(vertices[other_vertex].vy, vertices[base_vertex].vy);
            direction.vz = route_contact_sub(vertices[other_vertex].vz, vertices[base_vertex].vz);
            secondary.vx = route_contact_sub(vertices[third_vertex].vx, vertices[base_vertex].vx);
            secondary.vy = route_contact_sub(vertices[third_vertex].vy, vertices[base_vertex].vy);
            secondary.vz = route_contact_sub(vertices[third_vertex].vz, vertices[base_vertex].vz);
            state->normal.pad = 0u;
            goto finish;
        }
        secondary.vx = 0;
        secondary.vy = 100;
        secondary.vz = 0;
        state->normal.pad = 1u;

    finish:
        OuterProduct0(&direction, &secondary, &direction);
        {
            sint32 maximum = direction.vx < 0 ? (sint32)(0u - (uint32)direction.vx) : direction.vx;
            sint32 value = direction.vy < 0 ? (sint32)(0u - (uint32)direction.vy) : direction.vy;
            sint32 shift;

            if (maximum < value)
                maximum = value;
            value = direction.vz < 0 ? (sint32)(0u - (uint32)direction.vz) : direction.vz;
            if (maximum < value)
                maximum = value;
            shift = 18 - (sint32)route_contact_lzcr((uint32)maximum);
            if (shift > 0)
            {
                direction.vx >>= shift;
                direction.vy >>= shift;
                direction.vz >>= shift;
            }
            VectorNormalS(&direction, &state->normal);
        }
        plane = route_contact_mul((sint16)state->normal.vx, vertices[base_vertex].vx);
        plane = route_contact_add(plane, route_contact_mul((sint16)state->normal.vy, vertices[base_vertex].vy));
        {
            sint32 final_term = route_contact_mul((sint16)state->normal.vz, vertices[base_vertex].vz);

            mode = state->surface;
            plane = route_contact_add(plane, final_term);
        }
        state->plane = (uint32)plane;
        if (mode != 0u && mode != 2u)
        {
            sint32 height = (sint32)state->height;
            if (height < 0)
                height = 0;
            sint32 projected = route_contact_sub(plane, route_contact_mul((sint16)state->normal.vx, position.vx));

            projected = route_contact_sub(projected, route_contact_mul((sint16)state->normal.vy, route_contact_add(height, position.vy)));
            projected = route_contact_sub(projected, route_contact_mul((sint16)state->normal.vz, position.vz));
            state->penetration = (uint32)(projected >> 12);
        }
        else
        {
            sint32 numerator = route_contact_sub(plane, route_contact_mul((sint16)state->normal.vx, position.vx));
            sint32 vertical;

            numerator = route_contact_sub(numerator, route_contact_mul((sint16)state->normal.vz, position.vz));
            vertical = (sint16)state->normal.vy;
            if (vertical == 0 || (vertical == -1 && numerator == (sint32)0x80000000u))
                route_contact_trap();
            state->penetration = 0u;
            state->height = (uint32)route_contact_sub(numerator / vertical, position.vy);
        }
        state->position[0] = (uint32)state->position[0] + ((uint32)position.vx);
        state->position[1] = (uint32)state->position[1] + ((uint32)position.vy);
        state->position[2] = (uint32)state->position[2] + ((uint32)position.vz);
        state->delta[0] = (uint32)position.vx;
        state->delta[1] = (uint32)position.vy;
        state->delta[2] = (uint32)position.vz;
        mode = state->surface;
        if (mode != 0u || (sint32)state->height <= 0)
        {
            state->diagonal[3] = 0u;
            state->diagonal[2] = 0u;
            state->diagonal[1] = 0u;
            state->diagonal[0] = 0u;
        }
        else
        {
            sint32 diagonal_x = route_contact_add(route_contact_sub(vertices[0].vx, vertices[3].vx), route_contact_sub(vertices[1].vx, vertices[2].vx));
            sint32 diagonal_y = route_contact_add(route_contact_sub(vertices[0].vy, vertices[3].vy), route_contact_sub(vertices[1].vy, vertices[2].vy));
            sint32 diagonal_z = route_contact_add(route_contact_sub(vertices[0].vz, vertices[3].vz), route_contact_sub(vertices[1].vz, vertices[2].vz));
            sint32 pads = route_contact_add(route_contact_add(vertices[0].pad, vertices[1].pad), route_contact_add(vertices[2].pad, vertices[3].pad));

            state->diagonal[0] = (uint32)diagonal_x;
            state->diagonal[1] = (uint32)diagonal_y;
            state->diagonal[2] = (uint32)diagonal_z;
            state->diagonal[3] = (uint32)pads;
        }
        return transitions;
    }
}

sint32 route_calc_boundary_direction(uint32 object, SVECTOR *output)
{
    uint32 cursor;
    uint32 position;
    uint32 first = 0;
    uint32 last = 0;
    sint32 found = 0;
    uint32 first_value;
    uint32 last_value;
    VECTOR normal;

    FUNCTION_MARKER(0x8001BA08u, "MAIN.EXE");
    if ((r_u8(object + 16u) & 0x0Cu) == 0x0Cu)
    {
        output->vz = 0;
        output->vy = 0;
        output->vx = 0;
        return 12;
    }
    cursor = object;
    position = (uint32)r_u8(object + 14u) >> 4;
    do
    {
        uint32 type = (uint32)r_u8(cursor + 16u) & 3u;

        if (type == 0u)
        {
            if (found)
                last = position;
            else
            {
                first = position;
                found = 1;
            }
        }
        type = (uint32)r_u8(cursor + 16u) & 3u;
        ++cursor;
        if (type != 2u)
            ++position;
    } while ((r_u8(cursor + 16u) & 0x0Cu) != 0x0Cu);
    first_value = r_u8(object + first * 14u + 32u);
    last_value = r_u8(object + last * 14u + 32u);
    normal.vy = 0;
    normal.vx = (sint32)(first_value - last_value);
    first_value = r_u8(object + first * 14u + 30u);
    last_value = r_u8(object + last * 14u + 30u);
    normal.vz = (sint32)(last_value - first_value);
    return VectorNormalS(&normal, output);
}

sint32 route_sample_geom_host(uint32 base, sint32 index, sint32 apply_height, sint32 output[4])
{
    FUNCTION_MARKER_ALIAS(0x8001A3B8u, "MAIN.EXE");
    return route_sample_geom_values(base, index, apply_height, output);
}

sint32 route_build_boundary_vec(void)
{
    uint32 first = 0x800F3FD8u;
    uint32 second = first + 24u;
    sint32 mode = 0;
    sint32 object_index = 0;
    sint32 segment_state = 1;
    sint32 result;

    FUNCTION_MARKER(0x8001BC3Cu, "MAIN.EXE");
    while (object_index < (sint32)r_u32(0x800B6A98u))
    {
        uint32 table = r_u32(0x800B6B80u);
        uint32 object = r_u32(table + (uint32)object_index * 4u);
        uint32 linked_index = r_u16(object + 2u);
        uint32 left_offset = (uint32)r_u8(object + 14u) >> 4;
        uint8 edge = r_u8(object + 14u);
        uint32 cursor = object + 16u;
        uint8 flags = r_u8(cursor);
        uint32 linked = r_u32(table + linked_index * 4u);
        uint32 right_offset = (uint32)edge & 0x0Fu;
        uint32 previous = 0;
        sint32 transitions = 0;

        left_offset = left_offset * 14u + 30u;
        right_offset = right_offset * 14u + 30u;
        if ((flags & 0x0Cu) != 0x0Cu)
            w_u8(cursor, (uint8)(flags | 4u));
        if ((r_u8(cursor) & 0x0Cu) != 0x0Cu)
        {
            for (;;)
            {
                uint32 type;

                if (segment_state != 1 || (r_u8(cursor) & 3u) != 0u)
                {
                    if (!segment_state)
                    {
                        flags = r_u8(cursor);
                        if ((flags & 3u) != 0u)
                        {
                            segment_state = 1;
                            if (mode == 2 && transitions == 1)
                                w_u8(cursor, (uint8)(flags | 8u));
                            ++transitions;
                        }
                    }
                }
                else
                {
                    segment_state = 0;
                    if (mode == 2 && transitions == 2 && previous != 0u)
                        w_u8(previous, (uint8)(r_u8(previous) | 4u));
                    ++transitions;
                }
                if (mode == 1 && transitions == 2)
                {
                    w_u32(first, linked);
                    w_u32(second - 20u, linked + right_offset);
                }
                else if (mode == 2 && transitions == 2)
                {
                    w_u32(first, object);
                    w_u32(second - 20u, object + left_offset);
                }
                type = (uint32)r_u8(cursor) & 3u;
                if (type != 2u)
                    left_offset += 14u;
                previous = cursor;
                if (type != 1u)
                    right_offset += 14u;
                ++cursor;
                if ((r_u8(cursor) & 0x0Cu) == 0x0Cu)
                    break;
            }
        }
        if (previous != 0u)
        {
            flags = r_u8(previous);
            if ((flags & 4u) == 0u)
                w_u8(previous, (uint8)(flags | 8u));
        }
        if (!segment_state)
            ++transitions;
        if (transitions == 2)
        {
            if (mode == 2)
            {
                mode = 1;
                w_u32(second, 1);
                second += 32u;
                first += 32u;
            }
            if (mode == 0)
                mode = 1;
        }
        else
        {
            if (transitions >= 5 || mode == 0)
                abort();
            if (mode == 1)
            {
                mode = 2;
                w_u32(second, 0);
                second += 32u;
                first += 32u;
                --object_index;
            }
        }
        ++object_index;
        segment_state = 1;
    }
    w_u32(first, 0);
    w_u32(first + 4u, 0);
    first = 0x800F3FD8u;
    second = first + 4u;
    result = (sint32)r_u32(second);
    while (result != 0)
    {
        uint32 object = r_u32(first);
        sint32 last = (sint32)r_u16(object) - 1;
        uint32 record;
        uint32 geometry;
        sint32 coordinate;
        uint8 cell;
        VECTOR direction;
        uint16 normal_x;
        uint16 normal_z;

        if (last < 0)
            last = (sint32)r_u16(object + 2u) + 1;
        record = r_u32(second);
        w_u32(second + 24u, (uint32)last);
        geometry = r_u32(first);
        cell = r_u8(record + 14u);
        coordinate = (sint16)r_u16(geometry + 8u);
        direction.vx = coordinate + 16 * (sint32)cell;
        cell = r_u8(record + 15u);
        coordinate = (sint16)r_u16(geometry + 10u);
        direction.vy = coordinate + 16 * (sint32)cell;
        cell = r_u8(record + 16u);
        coordinate = (sint16)r_u16(geometry + 12u);
        direction.vz = coordinate + 16 * (sint32)cell;
        record = r_u32(second);
        cell = r_u8(record);
        coordinate = (sint16)r_u16(geometry + 8u);
        direction.vx -= coordinate + 16 * (sint32)cell;
        record = r_u32(second);
        cell = r_u8(record + 1u);
        coordinate = (sint16)r_u16(geometry + 10u);
        direction.vy -= coordinate + 16 * (sint32)cell;
        record = r_u32(second);
        coordinate = (sint16)r_u16(geometry + 12u);
        cell = r_u8(record + 2u);
        direction.vy = 0;
        direction.vz -= coordinate + 16 * (sint32)cell;
        VectorNormalS(&direction, (SVECTOR *)psx_addr(first + 8u, sizeof(SVECTOR)));
        normal_x = r_u16(second + 8u);
        normal_z = r_u16(second + 4u);
        w_u16(second + 14u, 0);
        w_u16(second + 12u, (uint16)(0u - normal_x));
        w_u16(second + 16u, normal_z);
        first += 32u;
        second += 32u;
        result = (sint32)r_u32(second);
    }
    return result;
}

sint32 route_sample_dist_slot(const BOAT *boat, uint32 descriptor, uint32 output)
{
    uint32 frame;
    sint32 end;
    sint32 start;
    sint32 difference;
    sint32 index;
    uint16 value;
    sint16 delta_x;
    sint16 delta_z;
    VECTOR delta;
    VECTOR squared;
    sint32 result;

    FUNCTION_MARKER(0x80026CB8u, "MAIN.EXE");
    frame = guest_stack_push(0x30u);
    end = (sint32)r_u32(descriptor + 4u);
    start = (sint32)r_u32(descriptor);
    difference = (sint32)((uint32)end - 30u - (uint32)start);
    index = math_sra_s32((uint32)math_mul_lo_s32(difference, (sint32)0xB6DB6DB7u), 1u);
    route_sample_geom((uint32)start, index, 1, output);
    value = (uint16)((uint16)boat->motion.position[0] - r_u16(output));
    w_u16(frame + 16u, value);
    delta_x = (sint16)value;
    value = (uint16)((uint16)boat->motion.position[1] - r_u16(output + 4u));
    w_u16(frame + 18u, value);
    value = (uint16)((uint16)boat->motion.position[2] - r_u16(output + 8u));
    w_u16(frame + 20u, value);
    delta_z = (sint16)value;
    w_u16(frame + 18u, 0u);
    delta.vx = delta_x;
    delta.vy = 0;
    delta.vz = delta_z;
    delta.pad = 0;
    Square12(&delta, &squared);
    w_u32(frame + 24u, (uint32)(((sint64)delta_x * delta_x >> 12) + ((sint64)delta_z * delta_z >> 12)));
    result = (sint32)(frame + 24u);
    guest_stack_pop(0x30u);
    return result;
}

sint32 route_aim(BOAT *boat)
{
    uint32 first = (uint32)boat->contacts.points[0].object;
    uint16 first_index = r_u16(first);
    uint32 table = r_u32(0x800B6B80u);
    uint32 second = r_u32(table + 4u * (uint32)first_index);
    sint16 route_index = (sint16)(uint16)boat->route.target_lane;
    uint16 second_index = r_u16(second);
    uint32 position_x = (uint32)boat->contacts.points[0].position[0];
    uint32 object = r_u32(table + 4u * (uint32)second_index);
    uint32 vertex = object + 14u * (uint32)(sint32)route_index + 30u;
    uint8 vertex_x = r_u8(vertex);
    sint32 base_x = (sint16)r_u16(object + 8u);
    sint32 x = (sint32)((uint32)base_x + ((uint32)vertex_x << 4) - position_x);
    uint32 x_square;
    uint8 vertex_z;
    uint32 position_z;
    sint32 base_z;
    sint32 z;
    uint32 z_square;
    sint32 length;
    sint32 normalized_x;
    sint32 normalized_z;

    FUNCTION_MARKER(0x8002D1DCu, "MAIN.EXE");
    boat->route_target.vector[0] = (sint32)((uint32)x);
    x = (sint32)(uint32)boat->route_target.vector[0];
    x_square = (uint32)x * (uint32)x;
    vertex_z = r_u8(vertex + 2u);
    position_z = (uint32)boat->contacts.points[0].position[2];
    base_z = (sint16)r_u16(object + 12u);
    z = (sint32)((uint32)base_z + ((uint32)vertex_z << 4) - position_z);
    z_square = (uint32)z * (uint32)z;
    boat->route_target.vector[2] = (sint32)((uint32)z);
    length = (sint32)SquareRoot0((sint32)(x_square + z_square));
    x = (sint32)(uint32)boat->route_target.vector[0];
    normalized_x = math_div_s32((sint32)((uint32)x << 12), length);
    boat->route_target.length = (sint32)((uint32)length);
    z = (sint32)(uint32)boat->route_target.vector[2];
    length = (sint32)(uint32)boat->route_target.length;
    normalized_z = math_div_s32((sint32)((uint32)z << 12), length);
    boat->route.speed = (sint32)(7168u);
    boat->route_target.vector[0] = (sint32)((uint32)normalized_x);
    boat->route_target.vector[2] = (sint32)((uint32)normalized_z);
    return normalized_z;
}

sint32 route_init_lookahead(BOAT *boat)
{
    uint32 descriptor = (uint32)boat->contacts.points[0].object;
    sint32 segment;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002E3F8u, "MAIN.EXE");
    boat->route.hazard = (uint16)(0u);
    boat->route.behavior = (uint16)(0u);
    boat->route.lookahead = (uint16)(5u);
    boat->route.steering = (uint16)(0u);
    segment = (sint32)r_u16(descriptor) - 1;
    if (segment < 0)
        segment = (sint32)r_u16(descriptor + 2u) + 1;
    else
        segment = (sint32)r_u16(descriptor) - 1;
    next = (sint32)((uint32)segment + (uint16)boat->route.lookahead);
    boat->route.target_segment = (uint16)((uint16)next);
    next = (uint16)boat->route.target_segment;
    count = (sint32)r_u32(0x800B6A98u);
    boat->route.segment = (uint16)((uint16)segment);
    if (next >= count)
        boat->route.target_segment = (uint16)((uint16)(next - (sint32)r_u16(0x800B6A98u)));
    boat->route.target_lane = (uint16)(4u);
    boat->race.mode = (uint16)(8u);
    boat->route.ticks = (uint32)(0u);
    return 8;
}

sint32 route_select_direction_vec(BOAT *boat)
{
    BOAT_ROUTE_STATE *route = &boat->route;
    BOAT_ROUTE_TARGET *output = &boat->route_target;
    uint32 object;
    uint32 table;
    uint32 settings;
    sint32 indices[5];
    sint32 weights[5];
    sint32 candidate_x[5] = {0};
    sint32 candidate_z[5] = {0};
    sint32 candidate_yaw[5] = {0};
    sint32 current;
    sint32 heading;
    sint32 selected;
    sint32 x;
    sint32 z;
    sint32 length;
    sint32 yaw;
    sint32 index;
    sint32 component;

    FUNCTION_MARKER(0x8002EBDCu, "MAIN.EXE");

    index = (uint16)route->target_segment;
    table = r_u32(0x800B6B80u);
    settings = (uint32)route->settings;
    current = (sint16)(uint16)route->target_lane;
    object = r_u32(table + (uint32)index * 4u);
    if (current < 0)
        route->target_lane = (uint16)(0u);
    else if (current >= r_u8(object + 7u))
        route->target_lane = (uint16)((uint16)(r_u8(object + 7u) - 1u));
    current = (sint16)(uint16)route->target_lane;
    indices[2] = current;
    indices[3] = current + 1 < r_u8(object + 7u) ? current + 1 : -1;
    indices[4] = current + 2 < r_u8(object + 7u) ? current + 2 : -1;
    indices[1] = current - 1 >= 0 ? current - 1 : -1;
    indices[0] = current - 2 >= 0 ? current - 2 : -1;
    for (index = 0; index < 5; ++index)
    {
        uint32 vertex;
        uint8 type;

        weights[index] = 0;
        if (indices[index] < 0)
            continue;
        vertex = object + (uint32)indices[index] * 14u + 30u;
        if (r_u8(vertex) == 255u && r_u8(vertex + 2u) == 255u && r_u8(vertex + 1u) == 255u)
        {
            indices[index] = -1;
            continue;
        }
        if ((sint16)(uint16)route->hazard != 8 && (sint16)(uint16)route->behavior == 3 && indices[index] != (sint16)(uint16)route->lane_timer)
            weights[index] = (sint32)((uint32)weights[index] + r_u8(settings + 24u));
        type = r_u8(vertex + 12u);
        if (type == 1u)
            weights[index] = (sint32)((uint32)weights[index] + 1u);
        else if (type == 2u)
            weights[index] = (sint32)((uint32)weights[index] + r_u8(settings + 29u));
        else if (type == 3u)
            weights[index] = (sint32)((uint32)weights[index] + r_u8(settings + 28u));
        else
            weights[index] = 0;
    }
    if (indices[1] < 0)
        indices[1] = indices[0];
    if (indices[3] < 0)
        indices[3] = indices[4];
    weights[1] = (sint32)((uint32)weights[1] + (uint32)weights[0]);
    weights[2] = (sint32)((uint32)weights[2] << 1);
    weights[3] = (sint32)((uint32)weights[3] + (uint32)weights[4]);
    x = (sint16)(uint16)boat->motion.transform.pose.m[0][2];
    z = (sint16)(uint16)boat->motion.transform.pose.m[2][2];
    heading = ratan2(x, z);
    index = (uint16)route->target_segment;
    table = r_u32(0x800B6B80u);
    object = r_u32(table + (uint32)index * 4u);
    for (index = 1; index <= 3; ++index)
    {
        uint32 vertex;
        sint32 angle;
        sint32 turns;
        sint32 delta;
        uint8 coordinate;

        if (indices[index] < 0)
            continue;
        vertex = object + (uint32)indices[index] * 14u + 30u;
        coordinate = r_u8(vertex);
        component = (sint16)r_u16(object + 8u);
        candidate_x[index] = (sint32)((uint32)component + ((uint32)coordinate << 4) - (uint32)boat->contacts.points[0].position[0]);
        coordinate = r_u8(vertex + 2u);
        component = (sint16)r_u16(object + 12u);
        candidate_z[index] = (sint32)((uint32)component + ((uint32)coordinate << 4) - (uint32)boat->contacts.points[0].position[2]);
        angle = ratan2(candidate_x[index], candidate_z[index]);
        turns = math_sra_s32((uint32)angle + (angle < 0 ? 4095u : 0u), 12u);
        delta = (sint32)((uint32)heading - ((uint32)angle - ((uint32)turns << 12)));
        candidate_yaw[index] = delta;
        if (math_abs_s32(delta) < (sint16)r_u16(settings + 22u) && r_u8(vertex + 12u) >= 2u)
            weights[index] = (sint32)((uint32)weights[index] + r_u8(settings + 30u));
    }
    if (weights[3] < weights[1])
        selected = weights[2] < weights[1] ? 1 : 2;
    else if (weights[1] < weights[3])
        selected = weights[2] < weights[3] ? 3 : 2;
    else
    {
        selected = 2;
        if (weights[2] <= 0 && indices[2] < 0)
        {
            selected = 1;
            if (indices[1] < 0)
            {
                selected = 3;
                if (indices[3] < 0)
                {
                    selected = 4;
                    if (indices[4] < 0)
                        selected = 0;
                }
            }
        }
    }
    if (selected < 1 || selected > 3)
        exit(1);
    if (indices[selected] < 0)
        exit(1);
    x = candidate_x[selected];
    z = candidate_z[selected];
    output->vector[0] = (sint32)((uint32)x);
    output->vector[2] = (sint32)((uint32)z);
    if ((uint32)x + 6096u >= 12193u || (uint32)z + 6096u >= 12193u)
    {
        output->vector[2] = (sint32)(0u);
        output->vector[1] = (sint32)(0u);
        output->vector[0] = (sint32)(0u);
        return 0;
    }
    length = (sint32)SquareRoot0((uint32)math_mul_lo_s32(x, x) + (uint32)math_mul_lo_s32(z, z));
    output->length = (sint32)((uint32)length);
    if (length == 0)
    {
        output->vector[2] = (sint32)(0u);
        output->vector[0] = (sint32)(0u);
    }
    output->vector[0] = (sint32)((uint32)math_div_s32((sint32)((uint32)x << 12), length));
    output->vector[2] = (sint32)((uint32)math_div_s32((sint32)((uint32)z << 12), length));
    yaw = candidate_yaw[selected];
    if (yaw >= 2049)
        yaw = (sint32)((uint32)yaw - 4096u);
    else if (yaw < -2048)
        yaw = (sint32)((uint32)yaw + 4096u);
    output->vector[1] = (sint32)((uint32)yaw);
    route->target_lane = (uint16)((uint16)indices[selected]);
    return indices[selected];
}

sint32 route_update_speed_ctrl(const BOAT *source, BOAT *boat)
{
    sint32 distance;
    sint32 direction;
    uint32 settings;
    sint32 value;
    sint32 limit;
    sint32 result;

    FUNCTION_MARKER(0x8002F3E8u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 5u)
    {
        boat->route.speed_adjust = (uint16)(0u);
        return 5;
    }
    distance = (sint32)(uint32)boat->route.distance >> 12;
    direction = (sint16)(uint16)boat->route.direction;
    settings = (uint32)boat->route.settings;
    boat->route.behavior = (uint16)(0u);
    if (direction == 1)
    {
        sint32 delta;
        sint32 divisor;

        if (distance < 137)
        {
            boat->route.behavior = (uint16)(4u);
            boat->route.lane_timer = (uint16)((uint16)source->contacts.points[0].left);
        }
        delta = (sint32)((uint32)distance - (uint32)(sint32)(sint16)r_u16(settings + 2u));
        if (delta < 0)
        {
            value = math_mul_lo_s32(delta, (sint16)r_u16(settings + 10u));
            divisor = (sint16)r_u16(settings + 6u);
            value = (sint32)(0u - (uint32)value);
        }
        else
        {
            value = math_mul_lo_s32(delta, (sint16)r_u16(settings + 8u));
            divisor = (sint16)r_u16(settings + 4u);
        }
        value = math_div_s32(value, divisor);
        boat->route.speed_adjust = (uint16)((uint16)value);
    }
    else if (direction == 2)
    {
        sint32 delta;
        sint32 divisor;

        if (distance < 137)
        {
            uint16 source_lane = (uint16)source->contacts.points[0].left;

            boat->route.behavior = (uint16)(3u);
            boat->route.lane_timer = (uint16)(source_lane);
            if (distance < 51 && (sint16)(uint16)boat->route.lane_timer == (sint32)(uint32)source->contacts.points[0].left)
                boat->route.behavior = (uint16)(5u);
        }
        delta = (sint32)((uint32)distance + (uint32)(sint32)(sint16)r_u16(settings + 2u));
        if (delta < 0)
        {
            value = math_mul_lo_s32(delta, (sint16)r_u16(settings + 8u));
            divisor = (sint16)r_u16(settings + 4u);
            value = (sint32)(0u - (uint32)value);
        }
        else
        {
            value = math_mul_lo_s32(delta, (sint16)r_u16(settings + 10u));
            divisor = (sint16)r_u16(settings + 6u);
        }
        value = math_div_s32(value, divisor);
        boat->route.speed_adjust = (uint16)((uint16)value);
    }
    else
        boat->route.speed_adjust = (uint16)(0u);
    limit = (sint16)r_u16(settings + 8u);
    value = (sint16)(uint16)boat->route.speed_adjust;
    if (value < limit)
    {
        boat->route.speed_adjust = (uint16)((uint16)limit);
        return 1;
    }
    limit = (sint16)r_u16(settings + 10u);
    result = limit < value;
    if (result)
        boat->route.speed_adjust = (uint16)((uint16)limit);
    return result;
}

sint32 route_advance_target(BOAT_ROUTE_STATE *route)
{
    uint32 target = (uint32)(uint16)route->segment + (uint32)(uint16)route->lookahead;
    uint32 current = (uint16)route->target_segment;
    uint32 table = r_u32(0x800B6B80u);
    uint32 object = r_u32(table + (uint32)(uint16)route->target_segment * 4u);
    sint32 delta = (sint32)(current - (uint16)target);
    sint32 result;

    FUNCTION_MARKER(0x8002F5C0u, "MAIN.EXE");
    if (delta >= 21)
        target += r_u16(0x800B6A98u);
    else if (delta < -20)
        current += r_u16(0x800B6A98u);
    if ((uint16)current == (uint16)target)
        return 0;
    if ((uint16)current < (uint16)target)
    {
        table = r_u32(0x800B6B80u);
        do
        {
            uint32 cursor;
            uint32 left;
            uint32 right;

            object = r_u32(table + (uint32)r_u16(object) * 4u);
            left = r_u8(object + 14u) >> 4;
            right = r_u8(object + 14u) & 15u;
            cursor = object + 16u;
            if (right != (uint16)route->target_lane)
            {
                for (;;)
                {
                    uint8 descriptor = r_u8(cursor);
                    uint8 type = descriptor & 3u;

                    if ((descriptor & 12u) == 12u)
                    {
                        route->target_lane = (uint16)((uint16)(left - 1u));
                        break;
                    }
                    if (type != 2u)
                        ++left;
                    ++cursor;
                    if (type != 1u)
                        ++right;
                    if ((sint32)(uint16)right == (sint16)(uint16)route->target_lane)
                        break;
                }
            }
            ++current;
            route->target_lane = (uint16)((uint16)left);
        } while ((uint16)current != (uint16)target);
    }
    else
    {
        table = r_u32(0x800B6B80u);
        do
        {
            uint32 cursor;
            uint32 left;
            uint32 right;

            object = r_u32(table + (uint32)r_u16(object + 2u) * 4u);
            left = r_u8(object + 14u) >> 4;
            right = r_u8(object + 14u) & 15u;
            cursor = object + 16u;
            if (left != (uint16)route->target_lane)
            {
                for (;;)
                {
                    uint8 descriptor = r_u8(cursor);
                    uint8 type = descriptor & 3u;

                    if ((descriptor & 12u) == 12u)
                    {
                        route->target_lane = (uint16)((uint16)(right - 1u));
                        break;
                    }
                    if (type != 2u)
                        ++left;
                    ++cursor;
                    if (type != 1u)
                        ++right;
                    if ((sint32)(uint16)left == (sint16)(uint16)route->target_lane)
                        break;
                }
            }
            current += 0xFFFFu;
            route->target_lane = (uint16)((uint16)right);
        } while ((uint16)current != (uint16)target);
    }
    result = (sint32)(uint16)target < (sint32)r_u32(0x800B6A98u);
    route->target_segment = (uint16)((uint16)target);
    if (!result)
    {
        result = (sint32)(target - r_u16(0x800B6A98u));
        route->target_segment = (uint16)((uint16)result);
    }
    return result;
}

sint32 route_init_steps(BOAT *boat)
{
    BOAT_ROUTE_STATE *route = &boat->route;
    uint32 object = boat->contacts.points[0].object;
    sint32 result = (sint16)(uint16)route->lookahead;
    uint32 lane = (uint8)boat->contacts.points[0].left;
    uint32 count = 0;
    uint32 table;

    FUNCTION_MARKER(0x8002F7DCu, "MAIN.EXE");
    route->hazard = (uint16)(0u);
    if (result <= 0)
        return result;
    table = r_u32(0x800B6B80u);
    for (;;)
    {
        uint32 cursor;
        uint32 left;
        uint32 right;
        uint8 hazard;

        object = r_u32(table + (uint32)r_u16(object) * 4u);
        left = r_u8(object + 14u) >> 4;
        right = r_u8(object + 14u) & 15u;
        cursor = object + 16u;
        if ((uint8)right != (uint8)lane)
        {
            for (;;)
            {
                uint8 descriptor = r_u8(cursor);
                uint8 type = descriptor & 3u;

                if ((descriptor & 12u) == 12u)
                    break;
                if (type != 2u)
                    ++left;
                ++cursor;
                if (type != 1u)
                    ++right;
                if ((uint8)right == (uint8)lane)
                    break;
            }
        }
        ++count;
        lane = left;
        hazard = r_u8(object + (uint32)(uint8)lane * 14u + 42u);
        if (hazard == 0u)
        {
            route->hazard = (uint16)(8u);
            return 8;
        }
        if (hazard == 1u)
            route->hazard = (uint16)(7u);
        result = (sint32)(uint8)count < (sint16)(uint16)route->lookahead;
        if (!result)
            return result;
    }
}

sint32 route_assign_topology_states(void)
{
    sint32 count = (sint32)r_u32(0x800B6A98u);
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8002FF48u, "MAIN.EXE");
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 table = r_u32(0x800B6B80u);
            uint32 object = r_u32(table + (uint32)index * 4u);
            uint16 linked_index = r_u16(object + 2u);
            uint8 object_vertices = r_u8(object + 7u);
            uint32 linked = r_u32(table + (uint32)linked_index * 4u);
            sint32 vertex;
            sint32 left;
            sint32 right;
            sint32 state;
            uint32 left_status;
            uint32 right_status;
            uint32 cursor;

            vertex = 0;
            if (object_vertices != 0u)
            {
                do
                {
                    uint32 status = object + 42u + (uint32)vertex * 14u;

                    if (r_u8(status) != 3u)
                        w_u8(status, 2u);
                    ++vertex;
                    object_vertices = r_u8(object + 7u);
                } while (vertex < object_vertices);
            }
            left = r_u8(object + 14u) >> 4;
            left_status = object + 42u + (uint32)left * 14u;
            for (vertex = left; vertex > 0; --vertex)
            {
                left_status -= 14u;
                w_u8(left_status, 0u);
            }
            left_status = object + 42u + (uint32)left * 14u;
            right = r_u8(object + 14u) & 15u;
            right_status = linked + 42u + (uint32)right * 14u;
            for (vertex = right; vertex > 0; --vertex)
            {
                right_status -= 14u;
                w_u8(right_status, 0u);
            }
            right_status = linked + 42u + (uint32)right * 14u;
            cursor = object + 16u;
            state = 11;
            vertex = 0;
            do
            {
                uint8 flags = r_u8(cursor);
                uint8 type;

                if ((flags & 12u) == 12u)
                {
                    vertex = 14;
                    if (state == 12)
                    {
                        if (r_u8(left_status - 14u) != 0u)
                            w_u8(left_status - 14u, 1u);
                        state = 11;
                        if (r_u8(right_status - 14u) != 0u)
                            w_u8(right_status - 14u, 1u);
                    }
                    w_u8(right_status, 0u);
                    w_u8(left_status, 0u);
                }
                else
                {
                    type = flags & 3u;
                    if (type != 0u)
                    {
                        if (state == 12)
                        {
                            if (r_u8(left_status - 14u) != 0u)
                                w_u8(left_status - 14u, 1u);
                            state = 11;
                            if (r_u8(right_status - 14u) != 0u)
                                w_u8(right_status - 14u, 1u);
                        }
                        w_u8(right_status, 0u);
                        w_u8(left_status, 0u);
                        type = r_u8(cursor) & 3u;
                        if (type != 2u)
                        {
                            left_status += 14u;
                            ++left;
                        }
                        if (type != 1u)
                        {
                            right_status += 14u;
                            ++right;
                        }
                        w_u8(right_status, 0u);
                        w_u8(left_status, 0u);
                    }
                    else if (state == 11)
                    {
                        w_u8(right_status, 0u);
                        w_u8(left_status, 0u);
                        left_status += 14u;
                        ++left;
                        right_status += 14u;
                        ++right;
                        state = 12;
                        w_u8(right_status, 1u);
                        w_u8(left_status, 1u);
                    }
                    else
                    {
                        left_status += 14u;
                        ++left;
                        right_status += 14u;
                        ++right;
                    }
                }
                ++vertex;
                ++cursor;
            } while (vertex < 14);
            while (left < r_u8(object + 7u))
            {
                ++left;
                w_u8(left_status, 0u);
                left_status += 14u;
            }
            while (right < r_u8(linked + 7u))
            {
                ++right;
                w_u8(right_status, 0u);
                right_status += 14u;
            }
            count = r_s32(0x800B6A98u);
            ++index;
        } while (index < count);
        count = r_s32(0x800B6A98u);
    }
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 table = r_u32(0x800B6B80u);
            uint32 object = r_u32(table + (uint32)index * 4u);
            uint8 endpoints = r_u8(object + 14u);
            uint32 cursor = object + 16u;
            uint32 left_status = object + 42u + 14u * (endpoints >> 4);
            uint32 linked = r_u32(table + 4u * r_u16(object + 2u));
            uint32 right_status = linked + 42u + 14u * (endpoints & 15u);
            sint32 vertex = 0;

            do
            {
                uint8 value = r_u8(left_status);

                if (value == 0u)
                {
                    value = r_u8(right_status);
                    if (value == 2u)
                    {
                        w_u8(right_status, 1u);
                        value = r_u8(right_status);
                    }
                }
                else
                    value = r_u8(right_status);
                if (value == 0u)
                {
                    value = r_u8(left_status);
                    if (value == 2u)
                        w_u8(left_status, 1u);
                }
                value = r_u8(cursor);
                if ((value & 12u) == 12u)
                    vertex = 14;
                else
                {
                    value &= 3u;
                    if (value != 2u)
                        left_status += 14u;
                    if (value != 1u)
                        right_status += 14u;
                }
                ++vertex;
                ++cursor;
            } while (vertex < 14);
            count = r_s32(0x800B6A98u);
            ++index;
        } while (index < count);
    }
    result = 5;
    if (r_u32(0x800834A0u) != 5u)
        return result;
    count = r_s32(0x800B6A98u);
    if (count <= 0)
        return count;
    index = 0;
    do
    {
        uint32 table = r_u32(0x800B6B80u);
        uint32 object = r_u32(table + (uint32)index * 4u);
        sint32 vertices = r_u8(object + 7u);
        sint32 vertex;
        uint32 first = 0u;
        uint32 second = 0u;
        uint32 entry = object + 30u;
        uint32 minimum = 256u;

        for (vertex = 0; vertex < vertices; ++vertex, entry += 14u)
        {
            uint32 priority = r_u8(entry + 1u);

            if (priority < minimum && r_u8(entry + 12u) == 2u)
            {
                minimum = priority;
                second = first;
                first = entry;
            }
        }
        if (first != 0u)
            w_u8(first + 12u, 3u);
        ++index;
        if (second != 0u)
            w_u8(second + 12u, 3u);
        count = r_s32(0x800B6A98u);
        result = index < count;
    } while (result != 0);
    return result;
}

void route_sample_boundary(const sint32 position[3], uint32 descriptor)
{
    sint32 output[4];
    sint32 start = (sint32)r_u32(descriptor);
    sint32 end = (sint32)r_u32(descriptor + 4u);
    sint32 difference = (sint32)((uint32)end - 30u - (uint32)start);
    sint32 index = math_sra_s32((uint32)math_mul_lo_s32(difference, (sint32)0xB6DB6DB7u), 1u);
    VECTOR delta;
    VECTOR squared;

    FUNCTION_MARKER_ALIAS(0x80026CB8u, "MAIN.EXE");
    route_sample_geom_host((uint32)start, index, 1, output);
    delta.vx = (sint16)(uint16)((uint16)position[0] - (uint16)output[0]);
    delta.vy = 0;
    delta.vz = (sint16)(uint16)((uint16)position[2] - (uint16)output[2]);
    delta.pad = 0;
    Square0(&delta, &squared);
}
