#include "name.h"
#include "race_events.h"
#include "route.h"
#include "vehicle.h"
#include "render.h"
#include "game.h"
#include "global.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

ROUTE_RESOURCES route_resources;
ROUTE_BOUNDARIES route_boundaries;
ROUTE_SEGMENT route_showcase_seg;

void route_init_showcase(void)
{
    route_showcase_seg.next.seg = &route_showcase_seg;
    route_showcase_seg.prev.seg = &route_showcase_seg;
    route_showcase_seg.origin[0] = 0;
    route_showcase_seg.origin[1] = 0;
    route_showcase_seg.origin[2] = -20000;
    memset(route_showcase_seg.commands, 6, sizeof(route_showcase_seg.commands));
    route_resources.count = 1u;
    route_resources.owned_count = 0u;
    route_resources.segments[0] = &route_showcase_seg;
}


// Immutable defaults and material transitions from the original generator
const ROUTE_GEN_DEFAULTS route_gen_defaults = {
    0x01000100u,
    0x00000000u,
    0x00000000u,
    0x01000100u,
    0x00000000u,
    0x00180000u,
    0x00E10000u,
    0x003200FAu,
    0x05050301u,
    0x0A0A0700u,
    0x5A461400u,
    0x00000000u,
    0x00000000u,
};
const uint8 route_gen_commands[14] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6};
const uint8 route_gen_sources[15] = {0, 8, 4, 0, 1, 1, 1, 1, 1, 1, 2, 6, 10, 10, 10};
const uint8 route_gen_materials[12][3] = {
    {0, 0, 16},
    {0, 16, 2},
    {0, 0, 8},
    {0, 0, 12},
    {0, 8, 2},
    {0, 12, 2},
    {0, 16, 2},
    {0, 0, 0},
    {16, 2, 2},
    {2, 2, 2},
    {8, 2, 2},
    {12, 2, 2},
};
const uint8 route_gen_patterns[12][4] = {
    {3, 3, 7, 7},
    {2, 2, 5, 6},
    {0, 3, 7, 3},
    {1, 2, 5, 6},
    {1, 2, 5, 6},
    {4, 8, 11, 8},
    {1, 2, 5, 2},
    {0, 3, 3, 0},
    {4, 11, 11, 4},
    {9, 9, 10, 10},
    {4, 8, 4, 11},
    {9, 10, 9, 10},
};




// Import identity exists only while resolving disk offsets and directory links
typedef struct
{
    uint32 offset;
    uint16 next;
    uint16 previous;
} ROUTE_IMPORT;

static ROUTE_IMPORT route_imports[ROUTE_SEGMENT_CAPACITY];
static uint32 route_import_map[131072];

static uint16 route_read_u16(const uint8 *data)
{
    return (uint16)((uint32)data[0] | ((uint32)data[1] << 8));
}

static uint32 route_read_u32(const uint8 *data)
{
    return (uint32)data[0] | ((uint32)data[1] << 8) | ((uint32)data[2] << 16) | ((uint32)data[3] << 24);
}

static void route_decode_vertex(ROUTE_VERTEX *vertex, const uint8 *data)
{
    vertex->position[0] = data[0];
    vertex->position[1] = data[1];
    vertex->position[2] = data[2];
    vertex->flags = data[3];
    vertex->phase[0] = data[4];
    vertex->phase[1] = data[5];
    vertex->curve = data[6];
    vertex->texture_phase = data[7];
    vertex->deformation[0] = (sint8)data[8];
    vertex->deformation[1] = (sint8)data[9];
    vertex->impact_tick = route_read_u16(data + 10);
    vertex->lane = data[12];
    vertex->shade = data[13];
}

ROUTE_DECODE_RESULT route_decode_archive(const uint8 *data, size_t size, ROUTE_RESOURCES *archive)
{
    uint32 version;
    uint32 count;
    uint32 records;
    uint32 directory = 0u;
    uint32 index;

    if (!data || !archive || size < 8u || route_read_u16(data) != 0xBABEu)
        return ROUTE_DECODE_HEADER;
    version = route_read_u16(data + 2);
    if (version < 4u || version > 7u)
        return ROUTE_DECODE_HEADER;
    records = version == 4u ? 8u : version == 5u ? 16u : 20u;
    if (size < records)
        return ROUTE_DECODE_BOUNDS;
    count = route_read_u16(data + 6);
    if (version >= 6u)
    {
        uint32 words = route_read_u32(data + 8);
        if (words > 0x3FFFFFFFu || (size_t)words > size / 4u)
            return ROUTE_DECODE_BOUNDS;
        directory = words * 4u;
        if ((size_t)count * 4u > size - directory)
            return ROUTE_DECODE_BOUNDS;
    }
    archive->count = 0u;
    archive->owned_count = 0u;
    memset(route_import_map, 0, sizeof(route_import_map));
    for (index = 0u; index < count; ++index)
    {
        uint32 relative = version < 6u ? index * 226u : route_read_u32(data + directory + 4u * index);
        uint32 offset;
        uint32 hash;
        uint32 owned;
        uint32 vertices;
        uint32 vertex;
        const uint8 *source;
        ROUTE_SEGMENT *segment;

        if (relative > 0xFFFFFFFFu - records || (size_t)relative > size - records || size - records - relative < 30u)
            return ROUTE_DECODE_BOUNDS;
        offset = records + relative;
        hash = (offset * 2654435761u) & 131071u;
        while (route_import_map[hash] != 0u && route_imports[route_import_map[hash] - 1u].offset != offset)
            hash = (hash + 1u) & 131071u;
        if (route_import_map[hash] != 0u)
        {
            archive->segments[index] = &archive->storage[route_import_map[hash] - 1u];
            continue;
        }
        source = data + offset;
        vertices = version < 6u ? 14u : source[7];
        if (vertices > ROUTE_VERTEX_CAPACITY)
            return ROUTE_DECODE_VERTICES;
        if (size - offset - 30u < 14u * vertices)
            return ROUTE_DECODE_BOUNDS;
        owned = archive->owned_count++;
        route_imports[owned].offset = offset;
        route_imports[owned].next = route_read_u16(source);
        route_imports[owned].previous = route_read_u16(source + 2);
        route_import_map[hash] = owned + 1u;
        segment = &archive->storage[owned];
        memset(segment, 0, sizeof(*segment));
        segment->palette = version == 4u ? 0u : route_read_u16(source + 4);
        segment->lighting = version == 4u ? 0u : source[6];
        segment->vertex_count = version < 6u ? 12u : source[7];
        for (vertex = 0u; vertex < 3u; ++vertex)
            segment->origin[vertex] = (sint16)route_read_u16(source + 8u + 2u * vertex);
        segment->join[0] = source[14] >> 4;
        segment->join[1] = source[14] & 15u;
        segment->section = source[15];
        memcpy(segment->commands, source + 16, 14);
        for (vertex = 0u; vertex < vertices; ++vertex)
            route_decode_vertex(&segment->vertices[vertex], source + 30u + 14u * vertex);
        archive->segments[index] = segment;
    }
    for (index = 0u; index < archive->owned_count; ++index)
    {
        if (route_imports[index].next >= count || route_imports[index].previous >= count)
            return ROUTE_DECODE_LINK;
        archive->storage[index].next.seg = archive->segments[route_imports[index].next];
        archive->storage[index].next.idx = route_imports[index].next;
        archive->storage[index].prev.seg = archive->segments[route_imports[index].previous];
        archive->storage[index].prev.idx = route_imports[index].previous;
    }
    archive->count = count;
    return ROUTE_DECODE_OK;
}



sint32 route_sample_geom(const ROUTE_SEGMENT *src, sint32 idx, sint32 apply_height, sint32 dst[4])
{
    const ROUTE_VERTEX *vertex = &src->vertices[idx];
    uint32 idx0 = (uint32)vertex->phase[0];
    uint32 phase = game_timing.ticks;
    uint32 curve_sel = (uint32)vertex->curve;
    const uint8 *curve0 = render_curve[curve_sel & 0x0Fu];
    uint32 idx1;
    const uint8 *curve1;
    sint32 correction;
    uint32 wobble_phase;
    sint32 result;

    correction = -(sint32)curve0[(2u * idx0 + phase) & 0x1FFu];
    idx1 = (uint32)vertex->phase[1];
    curve1 = render_curve[curve_sel >> 4];
    correction -= (sint32)curve1[(2u * idx1 + phase) & 0x1FFu];
    wobble_phase = (uint16)(phase - vertex->impact_tick);
    if (wobble_phase < 256u)
    {
        sint32 wobble = math_sra_signed((sint32)(2u * render_curve[15][wobble_phase]) - 63, 1u);

        correction -= wobble;
        if (correction < -127)
            correction = -127;
    }
    {
        uint32 coordinate = (uint32)vertex->position[0];

        dst[0] = (sint16)src->origin[0] + 16 * (sint32)coordinate;
    }
    {
        uint32 coordinate = (uint32)vertex->position[1];

        dst[1] = (sint16)src->origin[1] + 16 * (sint32)coordinate;
    }
    {
        uint32 coordinate = (uint32)vertex->position[2];

        dst[2] = (sint16)src->origin[2] + 16 * (sint32)coordinate;
    }
    dst[3] = (sint32)(2u * ((uint32)vertex->flags & 0x1Fu));
    if (apply_height)
        dst[1] -= correction;
    result = (sint8)vertex->deformation[0];
    if (result != 0 || (result = (sint8)vertex->deformation[1]) != 0)
    {
        if (correction < 0)
        {
            if (!apply_height)
                dst[1] -= correction;
            dst[0] += math_sra_signed(correction * (sint8)vertex->deformation[0], 5u);
            result = dst[2] + math_sra_signed(correction * (sint8)vertex->deformation[1], 5u);
            dst[2] = result;
        }
    }
    return result;
}


extern sint32 route_sample_geom(const ROUTE_SEGMENT *, sint32, sint32, sint32 [4]);

static volatile sint32 route_trap_line;
static volatile uintptr_t route_trap_state;
static volatile uintptr_t route_trap_object;
static volatile uint32 route_trap_type;
static volatile uint32 route_trap_crossings;
static volatile VECTOR route_trap_vertices[4];
static volatile VECTOR route_trap_position;
static volatile sint32 route_trap_transitions;
static volatile uint32 route_trace_count;
static volatile uintptr_t route_trace_object[16];
static volatile uint32 route_trace_entry[16];
static volatile uint32 route_trace_type[16];
static volatile uint32 route_trace_crossings[16];
static volatile sint32 route_trace_action[16];
static volatile VECTOR route_trace_vertices[4][4];

static void route_contact_trap_at(sint32 line)
{
    route_trap_line = line;
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


static sint32 route_contact_pos(sint32 output[3], ROUTE_SEGMENT *seg, sint32 entry)
{
    ROUTE_SEGMENT *object;
    ROUTE_SEGMENT *linked;
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
    object = seg;
    packed = ((uint32)object->join[0] << 4 | object->join[1]);
    left_index = packed >> 4;
    right_index = packed & 0x0Fu;
    for (cursor = 0; cursor < (sint16)entry; ++cursor)
    {
        uint32 type = (uint32)object->commands[cursor] & 3u;

        if (type != 2u)
            ++left_index;
        if (type != 1u)
            ++right_index;
    }
    left_offset = left_index;
    right_offset = right_index;
    object = seg;
    linked_index = object->prev.idx;
    linked = object->prev.seg;
    value = object->vertices[left_offset].position[0];
    value += linked->vertices[right_offset].position[0];
    x = (uint16)(value << 4);
    object = seg;
    linked_index = object->prev.idx;
    linked = object->prev.seg;
    value = object->vertices[left_offset].position[1];
    value += linked->vertices[right_offset].position[1];
    y = (uint16)(value << 4);
    object = seg;
    linked_index = object->prev.idx;
    linked = object->prev.seg;
    value = object->vertices[left_offset].position[2];
    value += linked->vertices[right_offset].position[2];
    z = (uint16)(value << 4);
    object = seg;
    if (((uint32)object->commands[(sint16)entry] & 3u) != 2u)
    {
        left_offset = left_index + 1u;
        x = (uint16)(x + ((uint32)object->vertices[left_offset].position[0] << 4));
        object = seg;
        y = (uint16)(y + ((uint32)object->vertices[left_offset].position[1] << 4));
        object = seg;
        z = (uint16)(z + ((uint32)object->vertices[left_offset].position[2] << 4));
        ++count;
        object = seg;
    }
    if (((uint32)object->commands[(sint16)entry] & 3u) != 1u)
    {
        right_offset = right_index + 1u;
        linked_index = object->prev.idx;
        linked = object->prev.seg;
        x = (uint16)(x + ((uint32)linked->vertices[right_offset].position[0] << 4));
        object = seg;
        linked_index = object->prev.idx;
        linked = object->prev.seg;
        y = (uint16)(y + ((uint32)linked->vertices[right_offset].position[1] << 4));
        object = seg;
        linked_index = object->prev.idx;
        linked = object->prev.seg;
        ++count;
        z = (uint16)(z + ((uint32)linked->vertices[right_offset].position[2] << 4));
    }
    result = (sint16)x / count;
    object = seg;
    result = (sint16)(result + (sint32)(uint16)object->origin[0]);
    output[0] = result;
    result = (sint16)y / count;
    object = seg;
    result = (sint16)(result + (sint32)(uint16)object->origin[1]);
    output[1] = result;
    result = (sint16)z / count;
    object = seg;
    result = (sint16)(result + (sint32)(uint16)object->origin[2]);
    output[2] = result;
    return result;
}

void route_contact_init(ROUTE_CONTACT *state, ROUTE_SEGMENT *seg, sint32 entry)
{
    ROUTE_SEGMENT *object;
    uint32 packed;
    uint32 left;
    uint32 right;
    sint32 cursor;

    FUNCTION_MARKER(0x8001A920u, "MAIN.EXE");
    object = seg;
    state->entry = (uint32)(sint16)entry;
    state->seg = object;
    object = seg;
    packed = ((uint32)object->join[0] << 4 | object->join[1]);
    left = packed >> 4;
    right = packed & 0x0Fu;
    for (cursor = 0; cursor < (sint16)entry; ++cursor)
    {
        uint32 type = (uint32)object->commands[cursor] & 3u;

        if (type != 2u)
            ++left;
        if (type != 1u)
            ++right;
    }
    state->left = left;
    state->right = right;
    route_contact_pos(state->position, seg, (sint16)entry);
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
    ROUTE_SEGMENT *object;
    uint32 type;

    FUNCTION_MARKER(0x8001A9FCu, "MAIN.EXE");
    route_trace_count = 0u;
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

        object = state->seg;
        type = (uint32)object->commands[state->entry];
        low_type = type & 3u;
        if ((type & 0x0Cu) == 0x0Cu)
            route_contact_trap();

        {
            uint16 first = object->next.idx;
            uint16 route = first != 0u ? (uint16)(first - 1u) : (uint16)(object->prev.idx + 1u);

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
            ROUTE_SEGMENT *current = state->seg;
            sint32 left = (sint32)state->left;

            route_sample_geom(current, left, low_type == 0u, (sint32 *)&vertices[count++]);
        }
        if (low_type != 2u)
        {
            sint32 left = (sint32)state->left;
            ROUTE_SEGMENT *current = state->seg;

            route_sample_geom(current, route_contact_add(left, 1), low_type == 0u, (sint32 *)&vertices[count++]);
        }
        if (low_type != 1u)
        {
            ROUTE_SEGMENT *current = state->seg;
            uint16 link_index = current->prev.idx;
            sint32 right = (sint32)state->right;
            ROUTE_SEGMENT *linked = current->prev.seg;

            route_sample_geom(linked, route_contact_add(right, 1), low_type == 0u, (sint32 *)&vertices[count++]);
        }
        {
            ROUTE_SEGMENT *current = state->seg;
            sint32 right = (sint32)state->right;
            uint16 link_index = current->prev.idx;
            ROUTE_SEGMENT *linked = current->prev.seg;

            route_sample_geom(linked, right, low_type == 0u, (sint32 *)&vertices[count++]);
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
                    route_trap_state = (uintptr_t)state;
                    route_trap_object = (uintptr_t)object;
                    route_trap_type = type;
                    route_trap_crossings = crossings;
                    route_trap_position = position;
                    route_trap_transitions = transitions;
                    for (index = 0; index < 4; ++index)
                        route_trap_vertices[index] = vertices[index];
                    route_contact_trap();
            }
        }
        if (route_trace_count < 16u)
        {
            uint32 trace_index = route_trace_count++;

            route_trace_object[trace_index] = (uintptr_t)object;
            route_trace_entry[trace_index] = state->entry;
            route_trace_type[trace_index] = type;
            route_trace_crossings[trace_index] = crossings;
            route_trace_action[trace_index] = action;
            if (trace_index < 4u)
            {
                for (index = 0; index < 4; ++index)
                    route_trace_vertices[trace_index][index] = vertices[index];
            }
        }
        if (action == 2 && (type & 4u) == 0u)
        {
            uint32 previous = state->entry - 1u;
            state->entry = previous;
            if ((state->seg->commands[previous] & 3u) != 2u)
                state->left = (uint32)state->left - (1u);
            if ((state->seg->commands[state->entry] & 3u) != 1u)
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
            ROUTE_SEGMENT *old_object = state->seg;
            uint16 link_index = old_object->next.idx;
            ROUTE_SEGMENT *candidate = old_object->next.seg;
            uint32 header = (uint32)((uint32)candidate->join[0] << 4 | candidate->join[1]);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry = 0;

            ++transitions;
            state->seg = candidate;
            while ((candidate->commands[entry] & 0x0Cu) != 0x0Cu)
            {
                uint32 candidate_type = (uint32)candidate->commands[entry] & 3u;
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
            if ((candidate->commands[entry] & 0x0Cu) != 0x0Cu)
            {
                uint32 old_left = state->left;
                state->left = left;
                state->right = old_left;
                state->entry = (uint32)entry;
                continue;
            }
            --transitions;
            state->seg = old_object;
            state->surface = 1u;
            direction.vx = route_contact_sub(vertices[0].vx, vertices[1].vx);
            direction.vy = route_contact_sub(vertices[0].vy, vertices[1].vy);
            direction.vz = route_contact_sub(vertices[0].vz, vertices[1].vz);
            base_vertex = 0;
        }
        else if (action == 5)
        {
            ROUTE_SEGMENT *old_object = state->seg;
            uint16 link_index = old_object->prev.idx;
            ROUTE_SEGMENT *candidate = old_object->prev.seg;
            uint32 header = (uint32)((uint32)candidate->join[0] << 4 | candidate->join[1]);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry = 0;

            --transitions;
            state->seg = candidate;
            while ((candidate->commands[entry] & 0x0Cu) != 0x0Cu)
            {
                uint32 candidate_type = (uint32)candidate->commands[entry] & 3u;
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
            if ((candidate->commands[entry] & 0x0Cu) != 0x0Cu)
            {
                uint32 old_right = state->right;
                state->right = right;
                state->left = old_right;
                state->entry = (uint32)entry;
                continue;
            }
            ++transitions;
            state->seg = old_object;
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
            uint32 header = (uint32)((uint32)object->join[0] << 4 | object->join[1]);
            uint32 left = header >> 4;
            uint32 right = header & 0x0Fu;
            sint32 entry_limit = (sint32)state->entry;

            for (index = 0; index < entry_limit; ++index)
            {
                uint32 entry = (uint32)object->commands[index];
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
            state->surface = low_type != 0u ? render_material(2u * (type >> 4))->surface : 0u;
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
            sint32 proj = route_contact_sub(plane, route_contact_mul((sint16)state->normal.vx, position.vx));

            proj = route_contact_sub(proj, route_contact_mul((sint16)state->normal.vy, route_contact_add(height, position.vy)));
            proj = route_contact_sub(proj, route_contact_mul((sint16)state->normal.vz, position.vz));
            state->penetration = (uint32)(proj >> 12);
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



sint32 route_advance_target(BOAT_ROUTE_STATE *route, const ROUTE_RESOURCES *archive)
{
    uint32 target = (uint32)(uint16)route->segment + (uint32)(uint16)route->lookahead;
    uint32 current = (uint16)route->target_segment;
    ROUTE_SEGMENT *const *table = archive->segments;
    const ROUTE_SEGMENT *object = table[(uint32)(uint16)route->target_segment];
    sint32 delta = (sint32)(current - (uint16)target);
    sint32 result;

    FUNCTION_MARKER(0x8002F5C0u, "MAIN.EXE");
    if (delta >= 21)
        target += (uint16)archive->count;
    else if (delta < -20)
        current += (uint16)archive->count;
    if ((uint16)current == (uint16)target)
        return 0;
    if ((uint16)current < (uint16)target)
    {
        do
        {
            const uint8 *cursor;
            uint32 left;
            uint32 right;

            object = object->next.seg;
            left = object->join[0];
            right = object->join[1];
            cursor = object->commands;
            if (right != (uint16)route->target_lane)
            {
                for (;;)
                {
                    uint8 desc = *cursor;
                    uint8 type = desc & 3u;

                    if ((desc & 12u) == 12u)
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
        do
        {
            const uint8 *cursor;
            uint32 left;
            uint32 right;

            object = object->prev.seg;
            left = object->join[0];
            right = object->join[1];
            cursor = object->commands;
            if (left != (uint16)route->target_lane)
            {
                for (;;)
                {
                    uint8 desc = *cursor;
                    uint8 type = desc & 3u;

                    if ((desc & 12u) == 12u)
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
    result = (sint32)(uint16)target < (sint32)archive->count;
    route->target_segment = (uint16)((uint16)target);
    if (!result)
    {
        result = (sint32)(target - (uint16)archive->count);
        route->target_segment = (uint16)((uint16)result);
    }
    return result;
}

sint32 route_init_steps(BOAT *boat, const ROUTE_SEGMENT *seg)
{
    BOAT_ROUTE_STATE *route = &boat->route;
    const ROUTE_SEGMENT *object = seg;
    sint32 result = (sint16)(uint16)route->lookahead;
    uint32 lane = (uint8)boat->contacts.points[0].left;
    uint32 count = 0;

    FUNCTION_MARKER(0x8002F7DCu, "MAIN.EXE");
    route->hazard = (uint16)(0u);
    if (result <= 0)
        return result;
    for (;;)
    {
        const uint8 *cursor;
        uint32 left;
        uint32 right;
        uint8 hazard;

        object = object->next.seg;
        left = object->join[0];
        right = object->join[1];
        cursor = object->commands;
        if ((uint8)right != (uint8)lane)
        {
            for (;;)
            {
                uint8 desc = *cursor;
                uint8 type = desc & 3u;

                if ((desc & 12u) == 12u)
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
        hazard = object->vertices[(uint8)lane].lane;
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

sint32 route_init_lookahead(BOAT *boat, const ROUTE_SEGMENT *seg, uint32 segment_count)
{
    const ROUTE_SEGMENT *desc = seg;
    sint32 segment;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002E3F8u, "MAIN.EXE");
    boat->route.hazard = (uint16)(0u);
    boat->route.behavior = (uint16)(0u);
    boat->route.lookahead = (uint16)(5u);
    boat->route.steering = (uint16)(0u);
    segment = (sint32)desc->next.idx - 1;
    if (segment < 0)
        segment = (sint32)desc->prev.idx + 1;
    else
        segment = (sint32)desc->next.idx - 1;
    next = (sint32)((uint32)segment + (uint16)boat->route.lookahead);
    boat->route.target_segment = (uint16)((uint16)next);
    next = (uint16)boat->route.target_segment;
    count = (sint32)segment_count;
    boat->route.segment = (uint16)((uint16)segment);
    if (next >= count)
        boat->route.target_segment = (uint16)((uint16)(next - (sint32)(uint16)segment_count));
    boat->route.target_lane = (uint16)(4u);
    boat->race.mode = (uint16)(8u);
    boat->route.ticks = (uint32)(0u);
    return 8;
}

sint32 route_calc_boundary_dir(const ROUTE_SEGMENT *src, SVECTOR *dst)
{
    const uint8 *cursor;
    uint32 position;
    uint32 idx0 = 0;
    uint32 idx1 = 0;
    sint32 found = 0;
    uint32 first_value;
    uint32 last_value;
    VECTOR normal;

    FUNCTION_MARKER(0x8001BA08u, "MAIN.EXE");
    if ((src->commands[0] & 0x0Cu) == 0x0Cu)
    {
        dst->vz = 0;
        dst->vy = 0;
        dst->vx = 0;
        return 12;
    }
    cursor = src->commands;
    position = (uint32)((uint32)src->join[0] << 4 | src->join[1]) >> 4;
    do
    {
        uint32 type = (uint32)*cursor & 3u;

        if (type == 0u)
        {
            if (found)
                idx1 = position;
            else
            {
                idx0 = position;
                found = 1;
            }
        }
        type = (uint32)*cursor & 3u;
        ++cursor;
        if (type != 2u)
            ++position;
    } while ((*cursor & 0x0Cu) != 0x0Cu);
    first_value = src->vertices[idx0].position[2];
    last_value = src->vertices[idx1].position[2];
    normal.vy = 0;
    normal.vx = (sint32)(first_value - last_value);
    first_value = src->vertices[idx0].position[0];
    last_value = src->vertices[idx1].position[0];
    normal.vz = (sint32)(last_value - first_value);
    return VectorNormalS(&normal, dst);
}



sint32 route_init_lanes(ROUTE_RESOURCES *archive, uint32 mode)
{
    sint32 count = (sint32)archive->count;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x8002FF48u, "MAIN.EXE");
    if (count > 0)
    {
        idx = 0;
        do
        {
            ROUTE_SEGMENT *seg = archive->segments[(uint32)idx];
            uint8 vtx_count = seg->vertex_count;
            ROUTE_SEGMENT *prev = seg->prev.seg;
            sint32 vtx_idx;
            sint32 left;
            sint32 right;
            sint32 in_span;
            ROUTE_VERTEX *vtx_l;
            ROUTE_VERTEX *vtx_r;
            uint8 *cmd;

            vtx_idx = 0;
            if (vtx_count != 0u)
            {
                do
                {
                    ROUTE_VERTEX *vtx = &seg->vertices[vtx_idx];

                    if (vtx->lane != 3u)
                        vtx->lane = 2u;
                    ++vtx_idx;
                    vtx_count = seg->vertex_count;
                } while (vtx_idx < vtx_count);
            }
            left = seg->join[0];
            vtx_l = &seg->vertices[left];
            for (vtx_idx = left; vtx_idx > 0; --vtx_idx)
            {
                --vtx_l;
                vtx_l->lane = 0u;
            }
            vtx_l = &seg->vertices[left];
            right = seg->join[1];
            vtx_r = &prev->vertices[right];
            for (vtx_idx = right; vtx_idx > 0; --vtx_idx)
            {
                --vtx_r;
                vtx_r->lane = 0u;
            }
            vtx_r = &prev->vertices[right];
            cmd = seg->commands;
            in_span = 0;
            vtx_idx = 0;
            do
            {
                uint8 flags = *cmd;
                uint8 type;

                if ((flags & 12u) == 12u)
                {
                    vtx_idx = 14;
                    if (in_span)
                    {
                        if (vtx_l[-1].lane != 0u)
                            vtx_l[-1].lane = 1u;
                        in_span = 0;
                        if (vtx_r[-1].lane != 0u)
                            vtx_r[-1].lane = 1u;
                    }
                    vtx_r->lane = 0u;
                    vtx_l->lane = 0u;
                }
                else
                {
                    type = flags & 3u;
                    if (type != 0u)
                    {
                        if (in_span)
                        {
                            if (vtx_l[-1].lane != 0u)
                                vtx_l[-1].lane = 1u;
                            in_span = 0;
                            if (vtx_r[-1].lane != 0u)
                                vtx_r[-1].lane = 1u;
                        }
                        vtx_r->lane = 0u;
                        vtx_l->lane = 0u;
                        type = *cmd & 3u;
                        if (type != 2u)
                        {
                            ++vtx_l;
                            ++left;
                        }
                        if (type != 1u)
                        {
                            ++vtx_r;
                            ++right;
                        }
                        vtx_r->lane = 0u;
                        vtx_l->lane = 0u;
                    }
                    else if (!in_span)
                    {
                        vtx_r->lane = 0u;
                        vtx_l->lane = 0u;
                        ++vtx_l;
                        ++left;
                        ++vtx_r;
                        ++right;
                        in_span = 1;
                        vtx_r->lane = 1u;
                        vtx_l->lane = 1u;
                    }
                    else
                    {
                        ++vtx_l;
                        ++left;
                        ++vtx_r;
                        ++right;
                    }
                }
                ++vtx_idx;
                ++cmd;
            } while (vtx_idx < 14);
            while (left < seg->vertex_count)
            {
                ++left;
                vtx_l->lane = 0u;
                ++vtx_l;
            }
            while (right < prev->vertex_count)
            {
                ++right;
                vtx_r->lane = 0u;
                ++vtx_r;
            }
            count = (sint32)archive->count;
            ++idx;
        } while (idx < count);
        count = (sint32)archive->count;
    }
    if (count > 0)
    {
        idx = 0;
        do
        {
            ROUTE_SEGMENT *seg = archive->segments[(uint32)idx];
            uint8 *cmd = seg->commands;
            ROUTE_VERTEX *vtx_l = &seg->vertices[seg->join[0]];
            ROUTE_SEGMENT *prev = seg->prev.seg;
            ROUTE_VERTEX *vtx_r = &prev->vertices[seg->join[1]];
            sint32 vtx_idx = 0;

            do
            {
                uint8 value = vtx_l->lane;

                if (value == 0u)
                {
                    value = vtx_r->lane;
                    if (value == 2u)
                    {
                        vtx_r->lane = 1u;
                        value = vtx_r->lane;
                    }
                }
                else
                    value = vtx_r->lane;
                if (value == 0u)
                {
                    value = vtx_l->lane;
                    if (value == 2u)
                        vtx_l->lane = 1u;
                }
                value = *cmd;
                if ((value & 12u) == 12u)
                    vtx_idx = 14;
                else
                {
                    value &= 3u;
                    if (value != 2u)
                        ++vtx_l;
                    if (value != 1u)
                        ++vtx_r;
                }
                ++vtx_idx;
                ++cmd;
            } while (vtx_idx < 14);
            count = (sint32)archive->count;
            ++idx;
        } while (idx < count);
    }
    result = 5;
    if (mode != 5u)
        return result;
    count = (sint32)archive->count;
    if (count <= 0)
        return count;
    idx = 0;
    do
    {
        ROUTE_SEGMENT *seg = archive->segments[(uint32)idx];
        sint32 vertices = seg->vertex_count;
        sint32 vtx_idx;
        ROUTE_VERTEX *v0 = NULL;
        ROUTE_VERTEX *v1 = NULL;
        ROUTE_VERTEX *entry = seg->vertices;
        uint32 minimum = 256u;

        for (vtx_idx = 0; vtx_idx < vertices; ++vtx_idx, ++entry)
        {
            uint32 priority = entry->position[1];

            if (priority < minimum && entry->lane == 2u)
            {
                minimum = priority;
                v1 = v0;
                v0 = entry;
            }
        }
        if (v0 != NULL)
            v0->lane = 3u;
        ++idx;
        if (v1 != NULL)
            v1->lane = 3u;
        count = (sint32)archive->count;
        result = idx < count;
    } while (result != 0);
    return result;
}



sint32 route_select_dir(BOAT *boat, const ROUTE_RESOURCES *archive)
{
    BOAT_ROUTE_STATE *route = &boat->route;
    const AI_ROUTE_CFG *cfg = route->settings;
    BOAT_ROUTE_TARGET *dst = &boat->route_target;
    const ROUTE_SEGMENT *seg;
    ROUTE_SEGMENT *const *table;
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
    sint32 idx;
    sint32 component;

    FUNCTION_MARKER(0x8002EBDCu, "MAIN.EXE");

    idx = (uint16)route->target_segment;
    table = archive->segments;
    current = (sint16)(uint16)route->target_lane;
    seg = table[(uint32)idx];
    if (current < 0)
        route->target_lane = (uint16)(0u);
    else if (current >= seg->vertex_count)
        route->target_lane = (uint16)((uint16)(seg->vertex_count - 1u));
    current = (sint16)(uint16)route->target_lane;
    indices[2] = current;
    indices[3] = current + 1 < seg->vertex_count ? current + 1 : -1;
    indices[4] = current + 2 < seg->vertex_count ? current + 2 : -1;
    indices[1] = current - 1 >= 0 ? current - 1 : -1;
    indices[0] = current - 2 >= 0 ? current - 2 : -1;
    for (idx = 0; idx < 5; ++idx)
    {
        const ROUTE_VERTEX *vtx;
        uint8 type;

        weights[idx] = 0;
        if (indices[idx] < 0)
            continue;
        vtx = &seg->vertices[indices[idx]];
        if (vtx->position[0] == 255u && vtx->position[2] == 255u && vtx->position[1] == 255u)
        {
            indices[idx] = -1;
            continue;
        }
        if ((sint16)(uint16)route->hazard != 8 && (sint16)(uint16)route->behavior == 3 && indices[idx] != (sint16)(uint16)route->lane_timer)
            weights[idx] = (sint32)((uint32)weights[idx] + cfg->lane_change);
        type = vtx->lane;
        if (type == 1u)
            weights[idx] = (sint32)((uint32)weights[idx] + 1u);
        else if (type == 2u)
            weights[idx] = (sint32)((uint32)weights[idx] + cfg->lane2);
        else if (type == 3u)
            weights[idx] = (sint32)((uint32)weights[idx] + cfg->lane3);
        else
            weights[idx] = 0;
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
    idx = (uint16)route->target_segment;
    table = archive->segments;
    seg = table[(uint32)idx];
    for (idx = 1; idx <= 3; ++idx)
    {
        const ROUTE_VERTEX *vtx;
        sint32 angle;
        sint32 turns;
        sint32 delta;
        uint8 coordinate;

        if (indices[idx] < 0)
            continue;
        vtx = &seg->vertices[indices[idx]];
        coordinate = vtx->position[0];
        component = seg->origin[0];
        candidate_x[idx] = (sint32)((uint32)component + ((uint32)coordinate << 4) - (uint32)boat->contacts.points[0].position[0]);
        coordinate = vtx->position[2];
        component = seg->origin[2];
        candidate_z[idx] = (sint32)((uint32)component + ((uint32)coordinate << 4) - (uint32)boat->contacts.points[0].position[2]);
        angle = ratan2(candidate_x[idx], candidate_z[idx]);
        turns = math_sra_s32((uint32)angle + (angle < 0 ? 4095u : 0u), 12u);
        delta = (sint32)((uint32)heading - ((uint32)angle - ((uint32)turns << 12)));
        candidate_yaw[idx] = delta;
        if (math_abs_s32(delta) < cfg->yaw_limit && vtx->lane >= 2u)
            weights[idx] = (sint32)((uint32)weights[idx] + cfg->heading);
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
    dst->vector[0] = (sint32)((uint32)x);
    dst->vector[2] = (sint32)((uint32)z);
    if ((uint32)x + 6096u >= 12193u || (uint32)z + 6096u >= 12193u)
    {
        dst->vector[2] = (sint32)(0u);
        dst->vector[1] = (sint32)(0u);
        dst->vector[0] = (sint32)(0u);
        return 0;
    }
    length = (sint32)SquareRoot0((uint32)math_mul_lo_s32(x, x) + (uint32)math_mul_lo_s32(z, z));
    dst->length = (sint32)((uint32)length);
    if (length == 0)
    {
        dst->vector[2] = (sint32)(0u);
        dst->vector[0] = (sint32)(0u);
    }
    dst->vector[0] = (sint32)((uint32)math_div_s32((sint32)((uint32)x << 12), length));
    dst->vector[2] = (sint32)((uint32)math_div_s32((sint32)((uint32)z << 12), length));
    yaw = candidate_yaw[selected];
    if (yaw >= 2049)
        yaw = (sint32)((uint32)yaw - 4096u);
    else if (yaw < -2048)
        yaw = (sint32)((uint32)yaw + 4096u);
    dst->vector[1] = (sint32)((uint32)yaw);
    route->target_lane = (uint16)((uint16)indices[selected]);
    return indices[selected];
}



sint32 route_aim(BOAT *boat, const ROUTE_SEGMENT *seg)
{
    const ROUTE_SEGMENT *target = seg->next.seg->next.seg;
    sint16 lane = (sint16)(uint16)boat->route.target_lane;
    uint32 pos_x = (uint32)boat->contacts.points[0].position[0];
    const ROUTE_VERTEX *vtx = &target->vertices[lane];
    uint8 vertex_x = vtx->position[0];
    sint32 base_x = target->origin[0];
    sint32 x = (sint32)((uint32)base_x + ((uint32)vertex_x << 4) - pos_x);
    uint32 x_square;
    uint8 vertex_z;
    uint32 pos_z;
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
    vertex_z = vtx->position[2];
    pos_z = (uint32)boat->contacts.points[0].position[2];
    base_z = target->origin[2];
    z = (sint32)((uint32)base_z + ((uint32)vertex_z << 4) - pos_z);
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



sint32 route_build_boundaries(ROUTE_RESOURCES *route, ROUTE_BOUNDARIES *dst)
{
    ROUTE_BOUNDARY *entry = dst->entries;
    sint32 mode = 0;
    sint32 object_index = 0;
    sint32 segment_state = 1;

    FUNCTION_MARKER(0x8001BC3Cu, "MAIN.EXE");
    while (object_index < (sint32)route->count)
    {
        ROUTE_SEGMENT *const *table = route->segments;
        ROUTE_SEGMENT *object = table[(uint32)object_index];
        uint32 left_idx = object->join[0];
                uint8 *cursor = object->commands;
        uint8 flags = *cursor;
        ROUTE_SEGMENT *linked = object->prev.seg;
        uint32 right_idx = object->join[1];
        uint8 *prev = NULL;
        sint32 transitions = 0;

        if ((flags & 0x0Cu) != 0x0Cu)
            *cursor = (uint8)(flags | 4u);
        if ((*cursor & 0x0Cu) != 0x0Cu)
        {
            for (;;)
            {
                uint32 type;

                if (segment_state != 1 || (*cursor & 3u) != 0u)
                {
                    if (!segment_state)
                    {
                        flags = *cursor;
                        if ((flags & 3u) != 0u)
                        {
                            segment_state = 1;
                            if (mode == 2 && transitions == 1)
                                *cursor = (uint8)(flags | 8u);
                            ++transitions;
                        }
                    }
                }
                else
                {
                    segment_state = 0;
                    if (mode == 2 && transitions == 2 && prev != NULL)
                        *prev = (uint8)(*prev | 4u);
                    ++transitions;
                }
                if (mode == 1 && transitions == 2)
                {
                    entry->seg = linked;
                    entry->vertex_idx = right_idx;
                }
                else if (mode == 2 && transitions == 2)
                {
                    entry->seg = object;
                    entry->vertex_idx = left_idx;
                }
                type = (uint32)*cursor & 3u;
                if (type != 2u)
                    ++left_idx;
                prev = cursor;
                if (type != 1u)
                    ++right_idx;
                ++cursor;
                if ((*cursor & 0x0Cu) == 0x0Cu)
                    break;
            }
        }
        if (prev != NULL)
        {
            flags = *prev;
            if ((flags & 4u) == 0u)
                *prev = (uint8)(flags | 8u);
        }
        if (!segment_state)
            ++transitions;
        if (transitions == 2)
        {
            if (mode == 2)
            {
                mode = 1;
                entry->mode = 1;
                ++entry;
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
                entry->mode = 0;
                ++entry;
                --object_index;
            }
        }
        ++object_index;
        segment_state = 1;
    }
    entry->seg = NULL;
    entry->vertex_idx = 0;
    dst->count = (uint32)(entry - dst->entries);
    for (entry = dst->entries; entry->seg != NULL; ++entry)
    {
        const ROUTE_VERTEX *v0 = &entry->seg->vertices[entry->vertex_idx];
        const ROUTE_VERTEX *v1 = v0 + 1;
        VECTOR dir;
        sint32 last = (sint32)entry->seg->next.idx - 1;

        if (last < 0)
            last = (sint32)entry->seg->prev.idx + 1;
        entry->route_idx = (uint32)last;
        dir.vx = 16 * ((sint32)v1->position[0] - (sint32)v0->position[0]);
        dir.vy = 0;
        dir.vz = 16 * ((sint32)v1->position[2] - (sint32)v0->position[2]);
        dir.pad = 0;
        VectorNormalS(&dir, &entry->dir);
        entry->normal.vx = (sint16)(uint16)(0u - (uint16)entry->dir.vz);
        entry->normal.vy = 0;
        entry->normal.vz = entry->dir.vx;
    }
    return 0;
}
sint32 route_init_cmds(ROUTE_RESOURCES *route)
{
    sint32 count = (sint32)route->count;
    sint32 index = 0;
    sint32 result = count;

    FUNCTION_MARKER(0x8001BB04u, "MAIN.EXE");
    if (count <= 0)
        return result;
    do
    {
        uint8 *object = route->segments[(uint32)index]->commands;
        uint8 *end = object + 14u;

        do
        {
            uint8 old_value = *object;
            uint32 value = (uint32)old_value & 6u;
            uint8 next = old_value & 0xF0u;

            *object = next;
            if (value == 6u)
            {
                *object = 0xFFu;
            }
            else if ((old_value & 8u) != 0u)
            {
                if (value != 4u)
                    abort();
                *object = *object;
            }
            else if (value == 0u)
            {
                *object = (uint8)(next | 1u);
            }
            else if (value == 2u)
            {
                *object = (uint8)(*object | 2u);
            }
            else if (value == 4u)
            {
                *object = (uint8)(*object | 3u);
            }
            ++object;
        } while (object < end);
        count = (sint32)route->count;
        index = (sint32)((uint32)index + 1u);
        result = index < count;
    } while (result != 0);
    return result;
}


extern sint32 route_sample_geom(const ROUTE_SEGMENT *, sint32, sint32, sint32 [4]);
void route_sample_boundary(const sint32 position[3], const ROUTE_BOUNDARY *src)
{
    sint32 output[4];
    VECTOR delta;
    VECTOR squared;

    FUNCTION_MARKER_ALIAS(0x80026CB8u, "MAIN.EXE");
    route_sample_geom(src->seg, (sint32)src->vertex_idx, 1, output);
    delta.vx = (sint16)(uint16)((uint16)position[0] - (uint16)output[0]);
    delta.vy = 0;
    delta.vz = (sint16)(uint16)((uint16)position[2] - (uint16)output[2]);
    delta.pad = 0;
    Square0(&delta, &squared);
}
void vehicle_sample_boundary(BOAT *boat, const ROUTE_BOUNDARIES *boundaries)
{
    const sint32 slots[3] = {1, 3, 4};
    BOAT_CONTACTS *contacts = &boat->contacts;
    sint32 current = contacts->boundary_section;
    sint32 previous = (sint32)((uint32)current - 1u);
    sint32 next = (sint32)((uint32)current + 1u);
    const ROUTE_BOUNDARY *desc = &boundaries->entries[current];
    const ROUTE_BOUNDARY *cursor = desc;
    SVECTOR transformed;

    FUNCTION_MARKER(0x80026DC0u, "MAIN.EXE");
    if (boat->control.driver - 2u >= 2u)
        return;
    SetRotMatrix(&boat->motion.transform.pose);
    SetTransMatrix(&boat->motion.transform.pose);
    for (sint32 index = 0; index < 3; ++index)
        RotTransSV(&contacts->points[slots[index]].sample, &transformed, NULL);
    if (desc->seg == NULL)
        return;
    if (previous < 0)
    {
        do
        {
            ++cursor;
            ++previous;
        } while (cursor->seg != NULL);
    }
    if (cursor[1].seg == NULL)
        next = 0;
    route_sample_boundary(boat->motion.position, &boundaries->entries[previous]);
    route_sample_boundary(boat->motion.position, desc);
    route_sample_boundary(boat->motion.position, &boundaries->entries[next]);
    // Original equal stack-pointer returns exclude every correction path
}


static sint16 route_geometry_lo_s16(uint32 value)
{
    return (sint16)(uint16)value;
}
static sint16 route_geometry_hi_s16(uint32 value)
{
    return (sint16)(uint16)(value >> 16);
}
static uint32 route_geometry_set_lo_u16(uint32 value, sint32 low)
{
    return (value & 0xFFFF0000u) | (uint16)low;
}
static sint32 route_geometry_mul_shift12(sint32 v0, sint32 v1)
{
    return (sint32)(((sint64)v0 * (sint64)v1) >> 12);
}
static uint8 route_geometry_clamp_byte(sint32 value)
{
    if (value < 0)
        return 0u;
    if (value >= 256)
        return 255u;
    return (uint8)value;
}
static sint16 route_vertices_lo_s16(uint32 value)
{
    return (sint16)(uint16)value;
}
static sint16 route_vertices_hi_s16(uint32 value)
{
    return (sint16)(uint16)(value >> 16);
}
static uint32 route_vertices_set_lo_s16(uint32 value, sint32 low)
{
    return (value & 0xFFFF0000u) | (uint16)low;
}
static uint32 route_vertices_set_hi_s16(uint32 value, sint32 high)
{
    return (value & 0x0000FFFFu) | ((uint32)(uint16)high << 16);
}
static sint8 route_vertices_slope_clamp(sint32 numerator, sint32 denominator)
{
    sint32 value = numerator / denominator;

    if (value < -127)
        value = -127;
    else if (value >= 128)
        value = 127;
    return (sint8)value;
}
void route_gen_geom(ROUTE_SEGMENT *records, sint32 count)
{
    uint32 turn_step = route_gen_defaults.turn_step;
    uint32 turn_amount = route_gen_defaults.turn_amount;
    uint32 turn_phase = route_gen_defaults.turn_phase;
    uint32 slope_step = route_gen_defaults.slope_step;
    uint32 slope_amount = route_gen_defaults.slope_amount;
    uint32 slope_phase = route_gen_defaults.slope_phase;
    uint32 left_offsets = route_gen_defaults.left_offsets;
    uint32 outer_offsets = route_gen_defaults.outer_offsets;
    uint32 random_ranges = route_gen_defaults.random_ranges;
    uint32 color_ranges = route_gen_defaults.color_ranges;
    uint32 color_bases = route_gen_defaults.color_bases;
    uint32 right_offsets = left_offsets;
    uint32 right_outer_offsets = outer_offsets;
    uint32 right_random_ranges = random_ranges;
    uint32 right_color_ranges = color_ranges;
    uint32 right_color_bases = color_bases;
    ROUTE_SEGMENT *current_record = records;
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
        ROUTE_SEGMENT *last = &records[count - 1];
        sint32 point;

        last->origin[0] = 0u;
        last->origin[1] = 0u;
        last->origin[2] = 0u;
        for (point = 0; point < 14; ++point)
            last->vertices[point].texture_phase = (uint8)global_fn_8006e9d8();
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
        if (route_geometry_lo_s16(turn_phase) / 4096 == 1)
        {
            if ((sint16)remaining < 65)
            {
                turn_phase = route_geometry_set_lo_u16(turn_phase, 0);
                turn_step = 0x00200020u;
                turn_amount = route_geometry_set_lo_u16(turn_amount, 0);
            }
            else
            {
                sint32 bounded = remaining >= 129 ? 128 : remaining;
                sint32 half = bounded / 2;
                sint32 length = half + global_fn_8006e9d8() % half;
                sint32 quarter = length / 4;
                sint32 eighth = length / 8;
                sint32 v0 = eighth + global_fn_8006e9d8() % quarter;
                sint32 first_divisor = 2048 / v0;
                sint32 first_duration = 2048 / first_divisor;
                sint32 second_divisor = 2048 / (length - first_duration);
                sint32 second_duration = 2048 / second_divisor;

                straight = 0;
                turn_step = (uint32)(uint16)first_divisor | ((uint32)(uint16)second_divisor << 16);
                turn_amount = route_geometry_set_lo_u16(turn_amount, 8 * second_duration);
                if (route_geometry_lo_s16(turn_amount) / first_duration >= 65)
                    turn_amount = route_geometry_set_lo_u16(turn_amount, first_duration << 6);
                turn_phase = route_geometry_set_lo_u16(turn_phase, 0);
            }
        }
        else
        {
            sint32 phase = route_geometry_lo_s16(turn_phase);
            phase += phase < 2048 ? route_geometry_lo_s16(turn_step) : route_geometry_hi_s16(turn_step);
            turn_phase = route_geometry_set_lo_u16(turn_phase, phase);
        }
        curve = route_geometry_mul_shift12(rcos(route_geometry_lo_s16(turn_phase)) - 4096, route_geometry_lo_s16(turn_amount));

        if (route_geometry_lo_s16(slope_phase) / 4096 == 1)
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
                sint32 v0 = quarter + global_fn_8006e9d8() % quarter;
                sint32 first_divisor = 2048 / v0;
                sint32 first_duration = 2048 / first_divisor;
                sint32 second_divisor = 2048 / (length - first_duration);
                sint32 second_duration = 2048 / second_divisor;
                sint32 phase_divisor;

                slope_step = (uint32)(uint16)first_divisor | ((uint32)(uint16)second_divisor << 16);
                slope_amount = route_geometry_set_lo_u16(slope_amount, 32 * (first_duration + global_fn_8006e9d8() % first_duration));
                phase_divisor = second_duration + global_fn_8006e9d8() % second_duration;
                slope_amount = (slope_amount & 0x0000FFFFu) | ((uint32)(uint16)(32 * phase_divisor) << 16);
                phase_divisor = global_fn_8006e9d8();
                phase_divisor = phase_divisor - 8 * (phase_divisor / 8) + 24;
                slope_phase = ((uint32)(uint16)phase_divisor << 16);
            }
        }
        else if (route_geometry_lo_s16(slope_phase) < 2048)
        {
            slope_phase = route_geometry_set_lo_u16(slope_phase, route_geometry_lo_s16(slope_phase) + route_geometry_lo_s16(slope_step));
            slope = route_geometry_mul_shift12(rsin(route_geometry_lo_s16(slope_phase)), route_geometry_lo_s16(slope_amount));
            if (route_geometry_lo_s16(slope_phase) >= 1024)
                fall = 1;
        }
        else
        {
            slope_phase = route_geometry_set_lo_u16(slope_phase, route_geometry_lo_s16(slope_phase) + route_geometry_hi_s16(slope_step));
            slope = route_geometry_mul_shift12(rsin(route_geometry_lo_s16(slope_phase)), route_geometry_hi_s16(slope_amount));
            if (route_geometry_lo_s16(slope_phase) >= 3072)
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
            radius[0] -= route_geometry_hi_s16(outer_offsets);
            radius[1] -= route_geometry_lo_s16(outer_offsets);
            radius[2] -= route_geometry_hi_s16(left_offsets);
            radius[3] -= route_geometry_lo_s16(left_offsets);
        }
        else if (rise == 1)
        {
            sint32 random = global_fn_8006e9d8() % (sint8)(random_ranges >> 16);

            radius[2] -= route_geometry_hi_s16(left_offsets);
            radius[1] = radius[2];
            radius[0] = radius[2] - (sint16)width - route_geometry_lo_s16(outer_offsets) - (random << 6);
        }
        else
        {
            radius[0] -= route_geometry_hi_s16(outer_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 24)) << 6);
            radius[1] -= route_geometry_lo_s16(outer_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 16)) << 6);
            radius[2] -= route_geometry_hi_s16(left_offsets) + ((global_fn_8006e9d8() % (sint8)(random_ranges >> 8)) << 6);
        }
        if (straight)
        {
            radius[9] += route_geometry_lo_s16(right_offsets);
            radius[10] += route_geometry_hi_s16(right_offsets);
            radius[11] += route_geometry_lo_s16(right_outer_offsets);
            radius[12] += route_geometry_hi_s16(right_outer_offsets);
        }
        else if (fall == 1)
        {
            sint32 random = global_fn_8006e9d8() % (sint8)(right_random_ranges >> 16);

            radius[10] += route_geometry_hi_s16(right_offsets);
            radius[11] = radius[10];
            radius[12] = radius[10] + (sint16)width + route_geometry_lo_s16(right_outer_offsets) + (random << 6);
        }
        else
        {
            radius[10] += route_geometry_hi_s16(right_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 8)) << 6);
            radius[11] += route_geometry_lo_s16(right_outer_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 16)) << 6);
            radius[12] += route_geometry_hi_s16(right_outer_offsets) + ((global_fn_8006e9d8() % (sint8)(right_random_ranges >> 24)) << 6);
        }

        angle = (index << 12) / count;
        cosine = rcos(angle);
        sine = rsin(angle);
        if (straight)
        {
            for (point = 0; point < 14; ++point)
                y[point] = (sint16)curve;
        }
        else if (route_geometry_lo_s16(slope_phase) >= 2049)
        {
            for (point = 13; point >= 0; --point)
            {
                if (point >= 9)
                    y[point] = (sint16)curve;
                else if (point < 3)
                    y[point] = y[point + 1];
                else if (route_geometry_lo_s16(slope_phase) >= 3073)
                    y[point] = (sint16)(y[point + 1] + (4096 - route_geometry_lo_s16(slope_phase)) / route_geometry_hi_s16(slope_phase));
                else
                    y[point] = (sint16)(y[point + 1] + (route_geometry_lo_s16(slope_phase) - 2048) / route_geometry_hi_s16(slope_phase));
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
                else if (route_geometry_lo_s16(slope_phase) >= 1025)
                    y[point] = (sint16)(y[point - 1] + (2048 - route_geometry_lo_s16(slope_phase)) / route_geometry_hi_s16(slope_phase));
                else
                    y[point] = (sint16)(y[point - 1] + route_geometry_lo_s16(slope_phase) / route_geometry_hi_s16(slope_phase));
            }
        }
        for (point = 0; point < 14; ++point)
        {
            x[point] = (sint16)route_geometry_mul_shift12(cosine, radius[point]);
            z[point] = (sint16)route_geometry_mul_shift12(sine, radius[point]);
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
        current_record->origin[0] = (uint16)minimum_x;
        current_record->origin[1] = (uint16)minimum_y;
        current_record->origin[2] = (uint16)minimum_z;
        for (point = 0; point < 14; ++point)
        {
            ROUTE_VERTEX *packed = &current_record->vertices[point];

            packed->position[0] = route_geometry_clamp_byte(((sint32)x[point] - minimum_x) >> 4);
            packed->position[1] = route_geometry_clamp_byte(((sint32)y[point] - minimum_y) >> 4);
            packed->position[2] = route_geometry_clamp_byte(((sint32)z[point] - minimum_z) >> 4);
        }
        current_record->vertex_count = 14u;
        current_record->next.seg = &records[index + 1]; current_record->next.idx = (uint16)(index + 1);
        current_record->prev.seg = index > 0 ? &records[index - 1] : NULL; current_record->prev.idx = (uint16)(index - 1);
        current_record->join[0] = current_record->join[1] = 0u;
        current_record->section = 1u;
        current_record->vertices[12].position[1] = 0u;

        current_record->vertices[1].position[1] = (uint8)(current_record->vertices[1].position[1] + (uint8)(color_bases >> 16) + global_fn_8006e9d8() % (sint8)(color_ranges >> 16));
        if (straight)
        {
            current_record->vertices[2].position[1] = (uint8)(current_record->vertices[2].position[1] + 20u + current_record->vertices[3].position[1]);
            current_record->vertices[0].position[1] = (uint8)(current_record->vertices[0].position[1] + (uint8)(color_bases >> 24) + global_fn_8006e9d8() % (sint8)(color_ranges >> 24));
        }
        else if (rise == 1)
        {
            uint8 value;

            current_record->vertices[2].position[1] = (uint8)(current_record->vertices[2].position[1] + (uint8)(color_bases >> 8));
            value = (uint8)(current_record->vertices[2].position[1] + 20u);
            current_record->vertices[0].position[1] = current_record->vertices[1].position[1];
            current_record->vertices[1].position[1] = value;
        }
        else
        {
            current_record->vertices[2].position[1] = (uint8)(current_record->vertices[2].position[1] + (uint8)(color_bases >> 8) + global_fn_8006e9d8() % (sint8)(color_ranges >> 8));
            current_record->vertices[0].position[1] = (uint8)(current_record->vertices[0].position[1] + (uint8)(color_bases >> 24) + global_fn_8006e9d8() % (sint8)(color_ranges >> 24));
        }

        current_record->vertices[11].position[1] = (uint8)(current_record->vertices[11].position[1] + (uint8)(right_color_bases >> 16) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 16));
        if (straight)
        {
            current_record->vertices[10].position[1] = (uint8)(current_record->vertices[9].position[1] + 20u);
            current_record->vertices[12].position[1] = (uint8)(current_record->vertices[12].position[1] + (uint8)(right_color_bases >> 24) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 24));
        }
        else if (fall == 1)
        {
            uint8 value;

            current_record->vertices[10].position[1] = (uint8)(current_record->vertices[10].position[1] + (uint8)(right_color_bases >> 8));
            value = (uint8)(current_record->vertices[10].position[1] + 20u);
            current_record->vertices[12].position[1] = current_record->vertices[11].position[1];
            current_record->vertices[11].position[1] = value;
        }
        else
        {
            current_record->vertices[10].position[1] = (uint8)(current_record->vertices[10].position[1] + (uint8)(right_color_bases >> 8) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 8));
            current_record->vertices[12].position[1] = (uint8)(current_record->vertices[12].position[1] + (uint8)(right_color_bases >> 24) + global_fn_8006e9d8() % (sint8)(right_color_ranges >> 24));
        }

        pattern = route_gen_patterns[pattern][(uint32)global_fn_8006e9d8() & 3u];
        for (point = 0; point < 14; ++point)
        {
            uint8 src = route_gen_sources[point + 1];
            uint8 material;
            const uint8 *choices = route_gen_materials[pattern];

            if ((src & 1u) != 0u)
                material = 1u;
            else if ((src & 2u) != 0u)
            {
                if ((straight && point == 9) || (!straight && fall == 1 && prior_fall == 1 && point == 10))
                    material = 24u;
                else
                {
                    uint8 choice = (uint8)(src - 2u);

                    if (!straight && fall == 1 && prior_fall == 1 && point >= 11)
                        choice = (uint8)(route_gen_sources[point] - 2u);
                    material = choices[choice >> 2];
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
                uint8 choice = (!straight && rise == 1 && prior_rise == 1 && point == 0) ? route_gen_sources[2] : src;
                material = choices[choice >> 2];
            }
            current_record->commands[point] = (uint8)(route_gen_commands[point] | (uint8)(material << 3));
        }
        previous_rise = rise;
        previous_fall = fall;
        ++current_record;
    }
    if (count > 0)
    {
        records->prev.seg = &records[count - 1]; records->prev.idx = (uint16)(count - 1);
        records[count - 1].next.seg = records; records[count - 1].next.idx = 0;
    }

}
sint32 route_gen_vertices(ROUTE_SEGMENT *record, sint32 count)
{
    ROUTE_SEGMENT *previous = record->prev.seg;
    uint32 control = route_gen_defaults.control;
    uint32 delta = route_gen_defaults.delta;
    sint16 decoration_gap = 0;
    sint32 index;
    sint32 point;

    FUNCTION_MARKER(0x8003F164u, "MAIN.EXE");
    for (point = 0; point < 14; ++point)
    {
        previous->vertices[point].phase[0] = 32u;
        previous->vertices[point].phase[1] = 32u;
        previous->vertices[point].curve = 72u;
    }
    for (index = 0; index < count; ++index)
    {
        if (route_vertices_lo_s16(control) != 0)
        {
            control = route_vertices_set_lo_s16(control, route_vertices_lo_s16(control) - 1);
            control = route_vertices_set_hi_s16(control, route_vertices_hi_s16(control) + route_vertices_lo_s16(delta));
        }
        else if (count - index < 65)
        {
            delta = route_vertices_set_lo_s16(delta, -2);
            control = route_vertices_set_lo_s16(control, route_vertices_hi_s16(control) / 2);
        }
        else
        {
            sint32 value;

            delta = route_vertices_set_hi_s16(delta, (global_fn_8006e9d8() & 3) + 10);
            value = global_fn_8006e9d8() % 64 - route_vertices_hi_s16(control) / 2;
            if ((sint16)value < 0)
            {
                control = route_vertices_set_lo_s16(control, -(sint16)value);
                delta = route_vertices_set_lo_s16(delta, -2);
            }
            else
            {
                control = route_vertices_set_lo_s16(control, (sint16)value);
                delta = route_vertices_set_lo_s16(delta, 2);
            }
        }

        record->vertices[0].texture_phase = (uint8)(previous->vertices[0].texture_phase - 94u);
        record->vertices[1].texture_phase = (uint8)(previous->vertices[1].texture_phase - 94u);
        record->vertices[2].texture_phase = (uint8)(previous->vertices[2].texture_phase - 94u);
        record->vertices[3].texture_phase = (uint8)(previous->vertices[3].texture_phase - 59u);
        record->vertices[4].texture_phase = (uint8)(previous->vertices[4].texture_phase - 27u);
        record->vertices[5].texture_phase = (uint8)(previous->vertices[5].texture_phase - 16u);
        record->vertices[6].texture_phase = (uint8)(previous->vertices[6].texture_phase - 40u);
        record->vertices[7].texture_phase = (uint8)(previous->vertices[7].texture_phase - 67u);
        record->vertices[8].texture_phase = (uint8)(previous->vertices[8].texture_phase - 98u);
        record->vertices[9].texture_phase = (uint8)(previous->vertices[9].texture_phase - 98u);
        record->vertices[10].texture_phase = (uint8)(previous->vertices[10].texture_phase - 84u);
        record->vertices[11].texture_phase = (uint8)(previous->vertices[11].texture_phase - 84u);
        record->vertices[12].texture_phase = (uint8)(previous->vertices[12].texture_phase - 84u);
        record->vertices[13].texture_phase = (uint8)(previous->vertices[13].texture_phase - 84u);
        for (point = 0; point < 14; ++point)
        {
            ROUTE_VERTEX *entry = &record->vertices[point];
            sint32 control_high = route_vertices_hi_s16(control);
            sint32 delta_high = route_vertices_hi_s16(delta);
            sint32 high_div16 = control_high / 16;

            entry->flags = 14u;
            entry->phase[0] = (uint8)(control_high / 2 + previous->vertices[point].phase[0]);
            entry->phase[1] = (uint8)(point * ((uint8)(control >> 16) + 32u));
            entry->curve = (uint8)((16 * ((delta_high - high_div16) / 2)) | ((delta_high - (high_div16 - 4)) / 2));
            entry->deformation[0] = 0u;
            entry->deformation[1] = 0u;
            entry->impact_tick = (uint16)-256;
            entry->lane = 2u;
        }
        {
            sint32 denominator = 16 * ((sint32)record->vertices[2].position[1] - (sint32)record->vertices[3].position[1]);
            sint8 x_slope = route_vertices_slope_clamp(((sint32)record->vertices[2].position[0] - (sint32)record->vertices[3].position[0]) << 9, denominator);
            sint8 z_slope = route_vertices_slope_clamp(((sint32)record->vertices[2].position[2] - (sint32)record->vertices[3].position[2]) << 9, denominator);

            record->vertices[3].deformation[0] = (uint8)-x_slope;
            record->vertices[3].deformation[1] = (uint8)-z_slope;
        }
        {
            sint32 denominator = 16 * ((sint32)record->vertices[10].position[1] - (sint32)record->vertices[9].position[1]);
            sint8 x_slope = route_vertices_slope_clamp(((sint32)record->vertices[10].position[0] - (sint32)record->vertices[9].position[0]) << 9, denominator);
            sint8 z_slope = route_vertices_slope_clamp(((sint32)record->vertices[10].position[2] - (sint32)record->vertices[9].position[2]) << 9, denominator);

            record->vertices[9].deformation[0] = (uint8)-x_slope;
            record->vertices[9].deformation[1] = (uint8)-z_slope;
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
                record->vertices[position].flags = (uint8)(record->vertices[position].flags | (uint8)(kind << 5));
                ++placed;
                ++position;
            }
        }
        ++decoration_gap;
        record = previous->next.seg;
    }
    return index < count;
}


sint32 route_update_speed_ctrl(const BOAT *src, BOAT *boat, uint32 race_mode)
{
    sint32 distance;
    sint32 direction;
    const AI_ROUTE_CFG *cfg;
    sint32 value;
    sint32 limit;
    sint32 result;

    FUNCTION_MARKER(0x8002F3E8u, "MAIN.EXE");
    if (race_mode == 5u)
    {
        boat->route.speed_adjust = (uint16)(0u);
        return 5;
    }
    distance = (sint32)(uint32)boat->route.distance >> 12;
    direction = (sint16)(uint16)boat->route.direction;
    cfg = boat->route.settings;
    boat->route.behavior = (uint16)(0u);
    if (direction == 1)
    {
        sint32 delta;
        sint32 divisor;

        if (distance < 137)
        {
            boat->route.behavior = (uint16)(4u);
            boat->route.lane_timer = (uint16)((uint16)src->contacts.points[0].left);
        }
        delta = (sint32)((uint32)distance - (uint32)(sint32)cfg->gap);
        if (delta < 0)
        {
            value = math_mul_lo_s32(delta, cfg->max_adjust);
            divisor = cfg->boost_range;
            value = (sint32)(0u - (uint32)value);
        }
        else
        {
            value = math_mul_lo_s32(delta, cfg->min_adjust);
            divisor = cfg->slow_range;
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
            uint16 source_lane = (uint16)src->contacts.points[0].left;

            boat->route.behavior = (uint16)(3u);
            boat->route.lane_timer = (uint16)(source_lane);
            if (distance < 51 && (sint16)(uint16)boat->route.lane_timer == (sint32)(uint32)src->contacts.points[0].left)
                boat->route.behavior = (uint16)(5u);
        }
        delta = (sint32)((uint32)distance + (uint32)(sint32)cfg->gap);
        if (delta < 0)
        {
            value = math_mul_lo_s32(delta, cfg->min_adjust);
            divisor = cfg->slow_range;
            value = (sint32)(0u - (uint32)value);
        }
        else
        {
            value = math_mul_lo_s32(delta, cfg->max_adjust);
            divisor = cfg->boost_range;
        }
        value = math_div_s32(value, divisor);
        boat->route.speed_adjust = (uint16)((uint16)value);
    }
    else
        boat->route.speed_adjust = (uint16)(0u);
    limit = cfg->min_adjust;
    value = (sint16)(uint16)boat->route.speed_adjust;
    if (value < limit)
    {
        boat->route.speed_adjust = (uint16)((uint16)limit);
        return 1;
    }
    limit = cfg->max_adjust;
    result = limit < value;
    if (result)
        boat->route.speed_adjust = (uint16)((uint16)limit);
    return result;
}

sint32 route_gen_init(uint32 state)
{
    sint32 count;
    ROUTE_SEGMENT *records = route_resources.storage;
    sint32 index;

    FUNCTION_MARKER(0x8003D8DCu, "MAIN.EXE");
    w_u32(0x800D2498u, name_reels.code);
    count = 600 + 8 * (global_fn_8006e9d8() & 0x31);
    memset(records, 0, (size_t)count * sizeof(*records));
    route_resources.owned_count = (uint32)count;
    route_resources.count = (uint32)count;
    for (index = 0; index < count; ++index)
        route_resources.segments[index] = &records[index];
    route_gen_geom(records, count);
    route_gen_vertices(records, count);
    race_events_init_fractal_light(state, count);
    render_select_lighting(RENDER_LIGHT_NORMAL);
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
