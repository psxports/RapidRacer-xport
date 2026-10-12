#include "river.h"
#include "name.h"
#include "route.h"
#include "boat.h"
#include "render.h"
#include "game.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    uint32 turn_step;
    uint32 turn_amount;
    uint32 turn_phase;
    uint32 slope_step;
    uint32 slope_amount;
    uint32 slope_phase;
    uint32 left_offsets;
    uint32 outer_offsets;
    uint32 random_ranges;
    uint32 color_ranges;
    uint32 color_bases;
    uint32 control;
    uint32 delta;
} ROUTE_GEN_DEFAULTS;

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
    route_resources.count = 1;
    route_resources.owned_count = 0;
    route_resources.segs[0] = &route_showcase_seg;
}

// Immutable defaults and material transitions from the original generator
static const ROUTE_GEN_DEFAULTS route_gen_defaults = {
    0x01000100, 0x00000000, 0x00000000, 0x01000100, 0x00000000, 0x00180000, 0x00E10000, 0x003200FA, 0x05050301, 0x0A0A0700, 0x5A461400, 0x00000000, 0x00000000,
};
static const uint8 route_gen_commands[14] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6};
static const uint8 route_gen_srcs[15] = {0, 8, 4, 0, 1, 1, 1, 1, 1, 1, 2, 6, 10, 10, 10};
static const uint8 route_gen_materials[12][3] = {
    {0, 0, 16}, {0, 16, 2}, {0, 0, 8}, {0, 0, 12}, {0, 8, 2}, {0, 12, 2}, {0, 16, 2}, {0, 0, 0}, {16, 2, 2}, {2, 2, 2}, {8, 2, 2}, {12, 2, 2},
};
static const uint8 route_gen_patterns[12][4] = {
    {3, 3, 7, 7}, {2, 2, 5, 6}, {0, 3, 7, 3}, {1, 2, 5, 6}, {1, 2, 5, 6}, {4, 8, 11, 8}, {1, 2, 5, 2}, {0, 3, 3, 0}, {4, 11, 11, 4}, {9, 9, 10, 10}, {4, 8, 4, 11}, {9, 10, 9, 10},
};

// Import identity exists only while resolving disk offsets and directory links
typedef struct
{
    uint32 offset;
    uint16 next;
    uint16 prev;
} ROUTE_IMPORT;

static ROUTE_IMPORT route_imports[ROUTE_SEGMENT_CAPACITY];
static uint32 route_import_map[131072];

static uint16 route_read_u16(const uint8 *data)
{
    return data[0] | data[1] << 8;
}

static uint32 route_read_u32(const uint8 *data)
{
    return data[0] | data[1] << 8 | data[2] << 16 | (uint32)data[3] << 24;
}

static void route_decode_vertex(ROUTE_VERTEX *vertex, const uint8 *data)
{
    vertex->pos[0] = data[0];
    vertex->pos[1] = data[1];
    vertex->pos[2] = data[2];
    vertex->flags = data[3];
    vertex->phase[0] = data[4];
    vertex->phase[1] = data[5];
    vertex->curve = data[6];
    vertex->tex_phase = data[7];
    vertex->deform[0] = data[8];
    vertex->deform[1] = data[9];
    vertex->impact_tick = route_read_u16(data + 10);
    vertex->lane = data[12];
    vertex->shade = data[13];
}

ROUTE_DECODE_RESULT route_decode_archive(const uint8 *data, uint32 size, ROUTE_RESOURCES *archive)
{
    uint32 version;
    uint32 count;
    uint32 recs;
    uint32 directory = 0;
    uint32 idx;

    if (!data || !archive || size < 8 || route_read_u16(data) != 0xBABE)
        return ROUTE_DECODE_HEADER;
    version = route_read_u16(data + 2);
    if (version < 4 || version > 7)
        return ROUTE_DECODE_HEADER;
    recs = version == 4 ? 8u : version == 5 ? 16u : 20u;
    if (size < recs)
        return ROUTE_DECODE_BOUNDS;
    count = route_read_u16(data + 6);
    if (version >= 6)
    {
        uint32 words = route_read_u32(data + 8);
        if (words > 0x3FFFFFFF || words > size / 4)
            return ROUTE_DECODE_BOUNDS;
        directory = words * 4;
        if (count * 4 > size - directory)
            return ROUTE_DECODE_BOUNDS;
    }
    archive->count = 0;
    archive->owned_count = 0;
    memset(route_import_map, 0, sizeof(route_import_map));
    for (idx = 0; idx < count; ++idx)
    {
        uint32 relative = version < 6 ? idx * 226 : route_read_u32(data + directory + 4 * idx);
        uint32 offset;
        uint32 hash;
        uint32 owned;
        uint32 vertices;
        uint32 vertex;
        const uint8 *src;
        ROUTE_SEGMENT *seg;

        if (relative > 0xFFFFFFFF - recs || relative > size - recs || size - recs - relative < 30)
            return ROUTE_DECODE_BOUNDS;
        offset = recs + relative;
        hash = offset * 0x9e3779b1 & 131071;
        while (route_import_map[hash] != 0 && route_imports[route_import_map[hash] - 1].offset != offset)
            hash += 1 & 131071;
        if (route_import_map[hash] != 0)
        {
            archive->segs[idx] = &archive->storage[route_import_map[hash] - 1];
            continue;
        }
        src = data + offset;
        vertices = version < 6 ? 14u : src[7];
        if (vertices > ROUTE_VERTEX_CAPACITY)
            return ROUTE_DECODE_VERTICES;
        if (size - offset - 30 < 14 * vertices)
            return ROUTE_DECODE_BOUNDS;
        owned = archive->owned_count++;
        route_imports[owned].offset = offset;
        route_imports[owned].next = route_read_u16(src);
        route_imports[owned].prev = route_read_u16(src + 2);
        route_import_map[hash] = owned + 1;
        seg = &archive->storage[owned];
        memset(seg, 0, sizeof(*seg));
        seg->pal = version == 4 ? 0u : route_read_u16(src + 4);
        seg->lighting = version == 4 ? 0u : src[6];
        seg->vertex_count = version < 6 ? 12u : src[7];
        for (vertex = 0; vertex < 3; ++vertex)
            seg->origin[vertex] = route_read_u16(src + 8u + 2 * vertex);
        seg->join[0] = src[14] >> 4;
        seg->join[1] = src[14] & 15u;
        seg->section = src[15];
        memcpy(seg->commands, src + 16, 14);
        for (vertex = 0; vertex < vertices; ++vertex)
            route_decode_vertex(&seg->vertices[vertex], src + 30u + 14 * vertex);
        archive->segs[idx] = seg;
    }
    for (idx = 0; idx < archive->owned_count; ++idx)
    {
        if (route_imports[idx].next >= count || route_imports[idx].prev >= count)
            return ROUTE_DECODE_LINK;
        archive->storage[idx].next.seg = archive->segs[route_imports[idx].next];
        archive->storage[idx].next.idx = route_imports[idx].next;
        archive->storage[idx].prev.seg = archive->segs[route_imports[idx].prev];
        archive->storage[idx].prev.idx = route_imports[idx].prev;
    }
    archive->count = count;
    return ROUTE_DECODE_OK;
}

sint32 route_sample_geom(const ROUTE_SEGMENT *src, sint32 idx, sint32 apply_height, sint32 dst[4])
{
    const ROUTE_VERTEX *vertex = &src->vertices[idx];
    uint32 idx0 = vertex->phase[0];
    uint32 phase = game_timing.ticks;
    uint32 curve_sel = vertex->curve;
    const uint8 *curve0 = render_curve[curve_sel & 0x0F];
    uint32 idx1;
    const uint8 *curve1;
    sint32 correction;
    uint32 wobble_phase;
    sint32 result;

    correction = -curve0[2 * idx0 + phase & 0x1FF];
    idx1 = vertex->phase[1];
    curve1 = render_curve[curve_sel >> 4];
    correction -= curve1[2 * idx1 + phase & 0x1FF];
    wobble_phase = (uint16)(phase - vertex->impact_tick);
    if (wobble_phase < 256)
    {
        sint32 wobble = sra(2u * render_curve[15][wobble_phase] - 63, 1u);

        correction -= wobble;
        if (correction < -127)
            correction = -127;
    }
    {
        uint32 coord = vertex->pos[0];

        dst[0] = src->origin[0] + 16 * coord;
    }
    {
        uint32 coord = vertex->pos[1];

        dst[1] = src->origin[1] + 16 * coord;
    }
    {
        uint32 coord = vertex->pos[2];

        dst[2] = src->origin[2] + 16 * coord;
    }
    dst[3] = 2 * (vertex->flags & 0x1F);
    if (apply_height)
        dst[1] -= correction;
    result = vertex->deform[0];
    if (result != 0 || (result = vertex->deform[1]) != 0)
    {
        if (correction < 0)
        {
            if (!apply_height)
                dst[1] -= correction;
            dst[0] += sra(correction * vertex->deform[0], 5u);
            result = dst[2] + sra(correction * vertex->deform[1], 5u);
            dst[2] = result;
        }
    }
    return result;
}

extern sint32 route_sample_geom(const ROUTE_SEGMENT *, sint32, sint32, sint32[4]);

static volatile sint32 route_trap_line;
static volatile uintptr_t route_trap_state;
static volatile uintptr_t route_trap_object;
static volatile uint32 route_trap_type;
static volatile uint32 route_trap_crossings;
static volatile VECTOR route_trap_vertices[4];
static volatile VECTOR route_trap_pos;
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

static sint32 route_contact_nclip(sint32 ax, sint32 az, sint32 bx, sint32 bz, sint32 cx, sint32 cz)
{
    sint64 result = (sint64)(sint16)ax * (sint16)bz + (sint64)(sint16)bx * (sint16)cz + (sint64)(sint16)cx * (sint16)az;

    result -= (sint64)(sint16)ax * (sint16)cz + (sint64)(sint16)bx * (sint16)az + (sint64)(sint16)cx * (sint16)bz;
    return -result;
}

static uint32 route_contact_lzcr(uint32 val)
{
    uint32 count = 0;

    if ((val & 0x80000000) != 0)
        val = ~val;
    while (count < 32 && (val & 0x80000000) == 0)
    {
        ++count;
        val <<= 1;
    }
    return count;
}

static sint32 route_contact_pos(VEC3 *output, ROUTE_SEGMENT *seg, sint32 entry)
{
    ROUTE_SEGMENT *object;
    ROUTE_SEGMENT *linked;
    uint32 linked_idx;
    uint32 left_idx;
    uint32 right_idx;
    uint32 left_offset;
    uint32 right_offset;
    uint32 val;
    sint32 cursor;
    sint32 count = 2;
    uint16 x;
    uint16 y;
    uint16 z;
    sint32 result;

    FUNCTION_MARKER(0x8001A57C, "MAIN.EXE");
    object = seg;
    left_idx = object->join[0];
    right_idx = object->join[1];
    for (cursor = 0; cursor < entry; ++cursor)
    {
        uint32 type = object->commands[cursor] & 3;

        if (type != 2)
            ++left_idx;
        if (type != 1)
            ++right_idx;
    }
    left_offset = left_idx;
    right_offset = right_idx;
    object = seg;
    linked_idx = object->prev.idx;
    linked = object->prev.seg;
    val = object->vertices[left_offset].pos[0];
    val += linked->vertices[right_offset].pos[0];
    x = val << 4;
    object = seg;
    linked_idx = object->prev.idx;
    linked = object->prev.seg;
    val = object->vertices[left_offset].pos[1];
    val += linked->vertices[right_offset].pos[1];
    y = val << 4;
    object = seg;
    linked_idx = object->prev.idx;
    linked = object->prev.seg;
    val = object->vertices[left_offset].pos[2];
    val += linked->vertices[right_offset].pos[2];
    z = val << 4;
    object = seg;
    if ((object->commands[(sint16)entry] & 3) != 2)
    {
        left_offset = left_idx + 1;
        x += object->vertices[left_offset].pos[0] << 4;
        object = seg;
        y += object->vertices[left_offset].pos[1] << 4;
        object = seg;
        z += object->vertices[left_offset].pos[2] << 4;
        ++count;
        object = seg;
    }
    if ((object->commands[(sint16)entry] & 3) != 1)
    {
        right_offset = right_idx + 1;
        linked_idx = object->prev.idx;
        linked = object->prev.seg;
        x += linked->vertices[right_offset].pos[0] << 4;
        object = seg;
        linked_idx = object->prev.idx;
        linked = object->prev.seg;
        y += linked->vertices[right_offset].pos[1] << 4;
        object = seg;
        linked_idx = object->prev.idx;
        linked = object->prev.seg;
        ++count;
        z += linked->vertices[right_offset].pos[2] << 4;
    }
    result = (sint16)x / count;
    object = seg;
    result = (sint16)(result + (uint16)object->origin[0]);
    output->x = result;
    result = (sint16)y / count;
    object = seg;
    result = (sint16)(result + (uint16)object->origin[1]);
    output->y = result;
    result = (sint16)z / count;
    object = seg;
    result = (sint16)(result + (uint16)object->origin[2]);
    output->z = result;
    return result;
}

void route_contact_init(ROUTE_CONTACT *state, ROUTE_SEGMENT *seg, sint32 entry)
{
    ROUTE_SEGMENT *object;
    uint32 left;
    uint32 right;
    sint32 cursor;

    FUNCTION_MARKER(0x8001A920, "MAIN.EXE");
    object = seg;
    state->entry = (sint16)entry;
    state->seg = object;
    object = seg;
    left = object->join[0];
    right = object->join[1];
    for (cursor = 0; cursor < entry; ++cursor)
    {
        uint32 type = object->commands[cursor] & 3;

        if (type != 2)
            ++left;
        if (type != 1)
            ++right;
    }
    state->left = left;
    state->right = right;
    route_contact_pos(&state->pos, seg, (sint16)entry);
    state->height = 0;
    state->penetration = 0;
}

sint32 route_contact_update(ROUTE_CONTACT *state)
{
    VECTOR pos;
    VECTOR vertices[4];
    VECTOR dir;
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

    FUNCTION_MARKER(0x8001A9FC, "MAIN.EXE");
    route_trace_count = 0;
    RotTrans(&state->sample, &pos, &flags);
    pos.vx -= state->pos.x;
    pos.vy -= state->pos.y;
    pos.vz -= state->pos.z;

    for (;;)
    {
        uint32 low_type;
        uint32 crossings = 0;
        sint16 key;
        sint32 idx;
        sint32 action = 0;

        object = state->seg;
        type = object->commands[state->entry];
        low_type = type & 3;
        if ((type & 0x0C) == 0x0C)
            route_contact_trap();

        uint16 next_idx = object->next.idx;
        key = (((next_idx != 0 ? (uint16)(next_idx - 1u) : (uint16)(object->prev.idx + 1u)) & 0xFFu) << 8) + state->entry;
        for (idx = 0; idx < 6; ++idx)
        {
            if (visited[idx] == key)
                route_contact_trap();
        }
        for (idx = 5; idx > 0; --idx)
            visited[idx] = visited[idx - 1];
        visited[0] = key;

        count = 0;
        {
            ROUTE_SEGMENT *cur = state->seg;
            sint32 left = state->left;

            route_sample_geom(cur, left, low_type == 0, (sint32 *)&vertices[count++]);
        }
        if (low_type != 2)
        {
            sint32 left = state->left;
            ROUTE_SEGMENT *cur = state->seg;

            route_sample_geom(cur, left + 1, low_type == 0, (sint32 *)&vertices[count++]);
        }
        if (low_type != 1)
        {
            ROUTE_SEGMENT *cur = state->seg;
            uint16 link_idx = cur->prev.idx;
            sint32 right = state->right;
            ROUTE_SEGMENT *linked = cur->prev.seg;

            route_sample_geom(linked, right + 1, low_type == 0, (sint32 *)&vertices[count++]);
        }
        {
            ROUTE_SEGMENT *cur = state->seg;
            sint32 right = state->right;
            uint16 link_idx = cur->prev.idx;
            ROUTE_SEGMENT *linked = cur->prev.seg;

            route_sample_geom(linked, right, low_type == 0, (sint32 *)&vertices[count++]);
        }

        for (idx = 0; idx < count; ++idx)
        {
            vertices[idx].vx = (sint16)((uint16)vertices[idx].vx - state->pos.x);
            vertices[idx].vy = (sint16)((uint16)vertices[idx].vy - state->pos.y);
            vertices[idx].vz = (sint16)((uint16)vertices[idx].vz - state->pos.z);
        }
        for (idx = 0; idx < count; ++idx)
        {
            sint32 next = idx + 1 == count ? 0 : idx + 1;
            crossings <<= 1;
            if (route_contact_nclip(vertices[idx].vx, vertices[idx].vz, vertices[next].vx, vertices[next].vz, pos.vx, pos.vz) < 0)
                ++crossings;
        }

        if (crossings == 0)
            action = 1;
        else if (low_type == 2)
        {
            switch (crossings)
            {
                case 1:
                    action = 2;
                    break;
                case 2:
                    action = 5;
                    break;
                case 3:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 2 : 5;
                    break;
                case 4:
                    action = 3;
                    break;
                case 5:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, pos.vx, pos.vz, 0, 0) < 0 ? 2 : 3;
                    break;
                case 6:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, pos.vx, pos.vz, 0, 0) < 0 ? 3 : 5;
                    break;
                default:
                    route_contact_trap();
            }
        }
        else if (low_type == 1)
        {
            switch (crossings)
            {
                case 1:
                    action = 2;
                    break;
                case 2:
                    action = 3;
                    break;
                case 3:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, pos.vx, pos.vz, 0, 0) < 0 ? 3 : 2;
                    break;
                case 4:
                    action = 4;
                    break;
                case 5:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 4 : 2;
                    break;
                case 6:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 3 : 4;
                    break;
                default:
                    route_contact_trap();
            }
        }
        else
        {
            switch (crossings)
            {
                case 1:
                case 11:
                    action = 2;
                    break;
                case 2:
                case 7:
                    action = 5;
                    break;
                case 3:
                    action = route_contact_nclip(vertices[3].vx, vertices[3].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 2 : 5;
                    break;
                case 4:
                case 14:
                    action = 3;
                    break;
                case 6:
                    action = route_contact_nclip(vertices[2].vx, vertices[2].vz, pos.vx, pos.vz, 0, 0) < 0 ? 3 : 5;
                    break;
                case 8:
                case 13:
                    action = 4;
                    break;
                case 9:
                    action = route_contact_nclip(vertices[0].vx, vertices[0].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 4 : 2;
                    break;
                case 12:
                    action = route_contact_nclip(vertices[1].vx, vertices[1].vz, pos.vx, pos.vz, 0, 0) >= 0 ? 3 : 4;
                    break;
                default:
                    route_trap_state = (uintptr_t)state;
                    route_trap_object = (uintptr_t)object;
                    route_trap_type = type;
                    route_trap_crossings = crossings;
                    route_trap_pos = pos;
                    route_trap_transitions = transitions;
                    for (idx = 0; idx < 4; ++idx)
                        route_trap_vertices[idx] = vertices[idx];
                    route_contact_trap();
            }
        }
        if (route_trace_count < 16)
        {
            uint32 trace_idx = route_trace_count++;

            route_trace_object[trace_idx] = (uintptr_t)object;
            route_trace_entry[trace_idx] = state->entry;
            route_trace_type[trace_idx] = type;
            route_trace_crossings[trace_idx] = crossings;
            route_trace_action[trace_idx] = action;
            if (trace_idx < 4)
            {
                for (idx = 0; idx < 4; ++idx)
                    route_trace_vertices[trace_idx][idx] = vertices[idx];
            }
        }
        if (action == 2 && (type & 4) == 0)
        {
            uint32 prev = state->entry - 1;
            state->entry = prev;
            if ((state->seg->commands[prev] & 3u) != 2)
                state->left = state->left - 1;
            if ((state->seg->commands[state->entry] & 3u) != 1)
                state->right = state->right - 1;
            continue;
        }
        if (action == 3 && (type & 8) == 0)
        {
            if (low_type != 2)
                state->left = state->left + 1;
            if (low_type != 1)
                state->right = state->right + 1;
            state->entry = state->entry + 1;
            continue;
        }
        if (action == 4)
        {
            ROUTE_SEGMENT *old_object = state->seg;
            uint16 link_idx = old_object->next.idx;
            ROUTE_SEGMENT *candidate = old_object->next.seg;
            uint32 left = candidate->join[0];
            uint32 right = candidate->join[1];
            sint32 entry = 0;

            ++transitions;
            state->seg = candidate;
            while ((candidate->commands[entry] & 0x0Cu) != 0x0C)
            {
                uint32 candidate_type = candidate->commands[entry] & 3;
                if (candidate_type == 1)
                    ++left;
                else
                {
                    if (right == state->left)
                        break;
                    ++right;
                    if ((candidate_type + 1 & 3) < 2)
                        ++left;
                }
                ++entry;
            }
            if ((candidate->commands[entry] & 0x0Cu) != 0x0C)
            {
                uint32 old_left = state->left;
                state->left = left;
                state->right = old_left;
                state->entry = entry;
                continue;
            }
            --transitions;
            state->seg = old_object;
            state->surface = 1;
            dir.vx = vertices[0].vx - vertices[1].vx;
            dir.vy = vertices[0].vy - vertices[1].vy;
            dir.vz = vertices[0].vz - vertices[1].vz;
            base_vertex = 0;
        }
        else if (action == 5)
        {
            ROUTE_SEGMENT *old_object = state->seg;
            uint16 link_idx = old_object->prev.idx;
            ROUTE_SEGMENT *candidate = old_object->prev.seg;
            uint32 left = candidate->join[0];
            uint32 right = candidate->join[1];
            sint32 entry = 0;

            --transitions;
            state->seg = candidate;
            while ((candidate->commands[entry] & 0x0Cu) != 0x0C)
            {
                uint32 candidate_type = candidate->commands[entry] & 3;
                if (candidate_type == 2)
                    ++right;
                else
                {
                    if (left == state->right)
                        break;
                    ++left;
                    if ((candidate_type + 1 & 3) < 2)
                        ++right;
                }
                ++entry;
            }
            if ((candidate->commands[entry] & 0x0Cu) != 0x0C)
            {
                uint32 old_right = state->right;
                state->right = right;
                state->left = old_right;
                state->entry = entry;
                continue;
            }
            ++transitions;
            state->seg = old_object;
            state->surface = 1;
            if ((type + 1 & 3) < 2)
            {
                dir.vx = vertices[2].vx - vertices[3].vx;
                dir.vy = vertices[2].vy - vertices[3].vy;
                dir.vz = vertices[2].vz - vertices[3].vz;
            }
            else
            {
                dir.vx = vertices[1].vx - vertices[2].vx;
                dir.vy = vertices[1].vy - vertices[2].vy;
                dir.vz = vertices[1].vz - vertices[2].vz;
            }
            base_vertex = 2;
        }
        else if (action == 2)
        {
            state->surface = 1;
            dir.vx = vertices[count - 1].vx - vertices[0].vx;
            dir.vy = vertices[count - 1].vy - vertices[0].vy;
            dir.vz = vertices[count - 1].vz - vertices[0].vz;
            base_vertex = 0;
        }
        else if (action == 3)
        {
            state->surface = 1;
            if (low_type == 2)
            {
                dir.vx = vertices[0].vx - vertices[1].vx;
                dir.vy = vertices[0].vy - vertices[1].vy;
                dir.vz = vertices[0].vz - vertices[1].vz;
            }
            else
            {
                dir.vx = vertices[1].vx - vertices[2].vx;
                dir.vy = vertices[1].vy - vertices[2].vy;
                dir.vz = vertices[1].vz - vertices[2].vz;
            }
            base_vertex = 1;
        }
        else
        {
            sint32 other_vertex;
            sint32 vertex2;
            uint32 left = object->join[0];
            uint32 right = object->join[1];
            sint32 entry_limit = state->entry;

            for (idx = 0; idx < entry_limit; ++idx)
            {
                uint32 entry = object->commands[idx];
                uint32 entry_type = entry & 3;

                if (entry_type != 2)
                    ++left;
                if (entry_type != 1)
                    ++right;
                if ((entry & 0x0C) == 0x0C)
                    route_contact_trap();
            }
            if (state->left != left || state->right != right)
                route_contact_trap();
            state->surface = low_type != 0 ? render_material(2 * (type >> 4))->surface : 0u;
            if ((type + 1 & 3) >= 2)
            {
                base_vertex = 0;
                other_vertex = 1;
                vertex2 = 2;
            }
            else
            {
                if (route_contact_nclip(vertices[1].vx, vertices[1].vz, vertices[3].vx, vertices[3].vz, pos.vx, pos.vz) <= 0)
                {
                    base_vertex = 1;
                    other_vertex = 2;
                }
                else
                {
                    base_vertex = 0;
                    other_vertex = 1;
                }
                vertex2 = 3;
            }
            dir.vx = vertices[other_vertex].vx - vertices[base_vertex].vx;
            dir.vy = vertices[other_vertex].vy - vertices[base_vertex].vy;
            dir.vz = vertices[other_vertex].vz - vertices[base_vertex].vz;
            secondary.vx = vertices[vertex2].vx - vertices[base_vertex].vx;
            secondary.vy = vertices[vertex2].vy - vertices[base_vertex].vy;
            secondary.vz = vertices[vertex2].vz - vertices[base_vertex].vz;
            state->n.pad = 0;
            goto finish;
        }
        secondary.vx = 0;
        secondary.vy = 100;
        secondary.vz = 0;
        state->n.pad = 1;

    finish:
        OuterProduct0(&dir, &secondary, &dir);
        sint32 maximum = abs(dir.vx);
        sint32 val = abs(dir.vy);
        sint32 shift;

        if (maximum < val)
            maximum = val;
        val = abs(dir.vz);
        if (maximum < val)
            maximum = val;
        shift = 18 - route_contact_lzcr(maximum);
        if (shift > 0)
        {
            dir.vx >>= shift;
            dir.vy >>= shift;
            dir.vz >>= shift;
        }
        VectorNormalS(&dir, &state->n);
        plane = state->n.vx * vertices[base_vertex].vx + state->n.vy * vertices[base_vertex].vy + state->n.vz * vertices[base_vertex].vz;
        mode = state->surface;
        state->plane = plane;
        if (mode != 0 && mode != 2)
        {
            sint32 height = state->height;
            if (height < 0)
                height = 0;
            sint32 proj = plane - state->n.vx * pos.vx;

            proj -= state->n.vy * (height + pos.vy);
            proj -= state->n.vz * pos.vz;
            state->penetration = proj >> 12;
        }
        else
        {
            sint32 numerator = plane - state->n.vx * pos.vx;
            sint32 vertical;

            numerator -= state->n.vz * pos.vz;
            vertical = state->n.vy;
            if (vertical == 0 || vertical == -1 && numerator == INT32_MIN)
                route_contact_trap();
            state->penetration = 0;
            state->height = numerator / vertical - pos.vy;
        }
        state->pos.x = state->pos.x + pos.vx;
        state->pos.y = state->pos.y + pos.vy;
        state->pos.z = state->pos.z + pos.vz;
        state->delta.x = pos.vx;
        state->delta.y = pos.vy;
        state->delta.z = pos.vz;
        mode = state->surface;
        if (mode != 0 || state->height <= 0)
        {
            state->weight = 0;
            state->diagonal.z = 0;
            state->diagonal.y = 0;
            state->diagonal.x = 0;
        }
        else
        {
            sint32 diagonal_x = vertices[0].vx - vertices[3].vx + (vertices[1].vx - vertices[2].vx);
            sint32 diagonal_y = vertices[0].vy - vertices[3].vy + (vertices[1].vy - vertices[2].vy);
            sint32 diagonal_z = vertices[0].vz - vertices[3].vz + (vertices[1].vz - vertices[2].vz);
            sint32 pads = vertices[0].pad + vertices[1].pad + (vertices[2].pad + vertices[3].pad);

            state->diagonal.x = diagonal_x;
            state->diagonal.y = diagonal_y;
            state->diagonal.z = diagonal_z;
            state->weight = pads;
        }
        return transitions;
    }
}

sint32 route_advance_target(BOAT_ROUTE *route, const ROUTE_RESOURCES *archive)
{
    uint32 target = route->seg + route->lookahead;
    uint32 cur = route->target_seg;
    ROUTE_SEGMENT *const *table = archive->segs;
    const ROUTE_SEGMENT *object = table[route->target_seg];
    sint32 delta = cur - (uint16)target;
    sint32 result;

    FUNCTION_MARKER(0x8002F5C0, "MAIN.EXE");
    if (delta >= 21)
        target += (uint16)archive->count;
    else if (delta < -20)
        cur += (uint16)archive->count;
    if ((uint16)cur == (uint16)target)
        return 0;
    if ((uint16)cur < (uint16)target)
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
            if (right != route->target_lane)
            {
                for (;;)
                {
                    uint8 desc = *cursor;
                    uint8 type = desc & 3u;

                    if ((desc & 12u) == 12)
                    {
                        route->target_lane = left - 1;
                        break;
                    }
                    if (type != 2)
                        ++left;
                    ++cursor;
                    if (type != 1)
                        ++right;
                    if ((uint16)right == route->target_lane)
                        break;
                }
            }
            ++cur;
            route->target_lane = left;
        } while ((uint16)cur != (uint16)target);
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
            if (left != route->target_lane)
            {
                for (;;)
                {
                    uint8 desc = *cursor;
                    uint8 type = desc & 3u;

                    if ((desc & 12u) == 12)
                    {
                        route->target_lane = right - 1;
                        break;
                    }
                    if (type != 2)
                        ++left;
                    ++cursor;
                    if (type != 1)
                        ++right;
                    if ((uint16)left == route->target_lane)
                        break;
                }
            }
            cur += 0xFFFF;
            route->target_lane = right;
        } while ((uint16)cur != (uint16)target);
    }
    result = (uint16)target < (sint32)archive->count;
    route->target_seg = target;
    if (!result)
    {
        result = target - (uint16)archive->count;
        route->target_seg = result;
    }
    return result;
}

sint32 route_init_steps(BOAT *boat, const ROUTE_SEGMENT *seg)
{
    BOAT_ROUTE *route = &boat->route;
    const ROUTE_SEGMENT *object = seg;
    sint32 result = route->lookahead;
    uint32 lane = (uint8)boat->contacts.points[0].left;
    uint32 count = 0;

    FUNCTION_MARKER(0x8002F7DC, "MAIN.EXE");
    route->hazard = 0;
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

                if ((desc & 12u) == 12)
                    break;
                if (type != 2)
                    ++left;
                ++cursor;
                if (type != 1)
                    ++right;
                if ((uint8)right == (uint8)lane)
                    break;
            }
        }
        ++count;
        lane = left;
        hazard = object->vertices[(uint8)lane].lane;
        if (hazard == 0)
        {
            route->hazard = 8;
            return 8;
        }
        if (hazard == 1)
            route->hazard = 7;
        result = (uint8)count < route->lookahead;
        if (!result)
            return result;
    }
}

sint32 route_init_lookahead(BOAT *boat, const ROUTE_SEGMENT *seg, uint32 seg_count)
{
    const ROUTE_SEGMENT *desc = seg;
    sint32 seg_idx;
    sint32 next;
    sint32 count;

    FUNCTION_MARKER(0x8002E3F8, "MAIN.EXE");
    boat->route.hazard = 0;
    boat->route.behavior = 0;
    boat->route.lookahead = 5;
    boat->route.steering = 0;
    seg_idx = desc->next.idx - 1;
    if (seg_idx < 0)
        seg_idx = desc->prev.idx + 1;
    else
        seg_idx = desc->next.idx - 1;
    boat->route.target_seg = seg_idx + boat->route.lookahead;
    next = boat->route.target_seg;
    count = seg_count;
    boat->route.seg = seg_idx;
    if (next >= count)
        boat->route.target_seg = next - seg_count;
    boat->route.target_lane = 4;
    boat->race.mode = 8;
    boat->route.ticks = 0;
    return 8;
}

sint32 route_calc_boundary_dir(const ROUTE_SEGMENT *src, SVECTOR *dst)
{
    const uint8 *cursor;
    uint32 pos;
    uint32 idx0 = 0;
    uint32 idx1 = 0;
    sint32 found = 0;
    uint32 val0;
    uint32 last_val;
    VECTOR n;

    FUNCTION_MARKER(0x8001BA08, "MAIN.EXE");
    if ((src->commands[0] & 0x0Cu) == 0x0C)
    {
        dst->vz = 0;
        dst->vy = 0;
        dst->vx = 0;
        return 12;
    }
    cursor = src->commands;
    pos = src->join[0];
    do
    {
        uint32 type = *cursor & 3;

        if (type == 0)
        {
            if (found)
                idx1 = pos;
            else
            {
                idx0 = pos;
                found = 1;
            }
        }
        type = *cursor & 3;
        ++cursor;
        if (type != 2)
            ++pos;
    } while ((*cursor & 0x0Cu) != 0x0C);
    val0 = src->vertices[idx0].pos[2];
    last_val = src->vertices[idx1].pos[2];
    n.vy = 0;
    n.vx = val0 - last_val;
    val0 = src->vertices[idx0].pos[0];
    last_val = src->vertices[idx1].pos[0];
    n.vz = last_val - val0;
    return VectorNormalS(&n, dst);
}

sint32 route_init_lanes(ROUTE_RESOURCES *archive, uint32 mode)
{
    sint32 count = archive->count;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x8002FF48, "MAIN.EXE");
    if (count > 0)
    {
        idx = 0;
        do
        {
            ROUTE_SEGMENT *seg = archive->segs[idx];
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
            if (vtx_count != 0)
            {
                do
                {
                    ROUTE_VERTEX *vtx = &seg->vertices[vtx_idx];

                    if (vtx->lane != 3)
                        vtx->lane = 2;
                    ++vtx_idx;
                    vtx_count = seg->vertex_count;
                } while (vtx_idx < vtx_count);
            }
            left = seg->join[0];
            vtx_l = &seg->vertices[left];
            for (vtx_idx = left; vtx_idx > 0; --vtx_idx)
            {
                --vtx_l;
                vtx_l->lane = 0;
            }
            vtx_l = &seg->vertices[left];
            right = seg->join[1];
            vtx_r = &prev->vertices[right];
            for (vtx_idx = right; vtx_idx > 0; --vtx_idx)
            {
                --vtx_r;
                vtx_r->lane = 0;
            }
            vtx_r = &prev->vertices[right];
            cmd = seg->commands;
            in_span = 0;
            vtx_idx = 0;
            do
            {
                uint8 flags = *cmd;
                uint8 type;

                if ((flags & 12u) == 12)
                {
                    vtx_idx = 14;
                    if (in_span)
                    {
                        if (vtx_l[-1].lane != 0)
                            vtx_l[-1].lane = 1;
                        in_span = 0;
                        if (vtx_r[-1].lane != 0)
                            vtx_r[-1].lane = 1;
                    }
                    vtx_r->lane = 0;
                    vtx_l->lane = 0;
                }
                else
                {
                    type = flags & 3u;
                    if (type != 0)
                    {
                        if (in_span)
                        {
                            if (vtx_l[-1].lane != 0)
                                vtx_l[-1].lane = 1;
                            in_span = 0;
                            if (vtx_r[-1].lane != 0)
                                vtx_r[-1].lane = 1;
                        }
                        vtx_r->lane = 0;
                        vtx_l->lane = 0;
                        type = *cmd & 3u;
                        if (type != 2)
                        {
                            ++vtx_l;
                            ++left;
                        }
                        if (type != 1)
                        {
                            ++vtx_r;
                            ++right;
                        }
                        vtx_r->lane = 0;
                        vtx_l->lane = 0;
                    }
                    else if (!in_span)
                    {
                        vtx_r->lane = 0;
                        vtx_l->lane = 0;
                        ++vtx_l;
                        ++left;
                        ++vtx_r;
                        ++right;
                        in_span = 1;
                        vtx_r->lane = 1;
                        vtx_l->lane = 1;
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
                vtx_l->lane = 0;
                ++vtx_l;
            }
            while (right < prev->vertex_count)
            {
                ++right;
                vtx_r->lane = 0;
                ++vtx_r;
            }
            count = archive->count;
            ++idx;
        } while (idx < count);
        count = archive->count;
    }
    if (count > 0)
    {
        idx = 0;
        do
        {
            ROUTE_SEGMENT *seg = archive->segs[idx];
            uint8 *cmd = seg->commands;
            ROUTE_VERTEX *vtx_l = &seg->vertices[seg->join[0]];
            ROUTE_SEGMENT *prev = seg->prev.seg;
            ROUTE_VERTEX *vtx_r = &prev->vertices[seg->join[1]];
            sint32 vtx_idx = 0;

            do
            {
                uint8 val = vtx_l->lane;

                if (val == 0)
                {
                    val = vtx_r->lane;
                    if (val == 2)
                    {
                        vtx_r->lane = 1;
                        val = vtx_r->lane;
                    }
                }
                else
                    val = vtx_r->lane;
                if (val == 0)
                {
                    val = vtx_l->lane;
                    if (val == 2)
                        vtx_l->lane = 1;
                }
                val = *cmd;
                if ((val & 12u) == 12)
                    vtx_idx = 14;
                else
                {
                    val &= 3;
                    if (val != 2)
                        ++vtx_l;
                    if (val != 1)
                        ++vtx_r;
                }
                ++vtx_idx;
                ++cmd;
            } while (vtx_idx < 14);
            count = archive->count;
            ++idx;
        } while (idx < count);
    }
    result = 5;
    if (mode != 5)
        return result;
    count = archive->count;
    if (count <= 0)
        return count;
    idx = 0;
    do
    {
        ROUTE_SEGMENT *seg = archive->segs[idx];
        sint32 vertices = seg->vertex_count;
        sint32 vtx_idx;
        ROUTE_VERTEX *v0 = NULL;
        ROUTE_VERTEX *v1 = NULL;
        ROUTE_VERTEX *entry = seg->vertices;
        uint32 minimum = 256;

        for (vtx_idx = 0; vtx_idx < vertices; ++vtx_idx, ++entry)
        {
            uint32 priority = entry->pos[1];

            if (priority < minimum && entry->lane == 2)
            {
                minimum = priority;
                v1 = v0;
                v0 = entry;
            }
        }
        if (v0 != NULL)
            v0->lane = 3;
        ++idx;
        if (v1 != NULL)
            v1->lane = 3;
        count = archive->count;
        result = idx < count;
    } while (result != 0);
    return result;
}

sint32 route_select_dir(BOAT *boat, const ROUTE_RESOURCES *archive)
{
    BOAT_ROUTE *route = &boat->route;
    const AI_ROUTE_CFG *cfg = route->settings;
    BOAT_ROUTE_TARGET *dst = &boat->route_target;
    const ROUTE_SEGMENT *seg;
    ROUTE_SEGMENT *const *table;
    sint32 indices[5];
    sint32 weights[5];
    sint32 candidate_x[5] = {0};
    sint32 candidate_z[5] = {0};
    sint32 candidate_yaw[5] = {0};
    sint32 cur;
    sint32 heading;
    sint32 sel;
    sint32 x;
    sint32 z;
    sint32 len;
    sint32 yaw;
    sint32 idx;
    sint32 component;

    FUNCTION_MARKER(0x8002EBDC, "MAIN.EXE");

    idx = route->target_seg;
    table = archive->segs;
    cur = route->target_lane;
    seg = table[idx];
    if (cur < 0)
        route->target_lane = 0;
    else if (cur >= seg->vertex_count)
        route->target_lane = seg->vertex_count - 1u;
    cur = route->target_lane;
    indices[2] = cur;
    indices[3] = cur + 1 < seg->vertex_count ? cur + 1 : -1;
    indices[4] = cur + 2 < seg->vertex_count ? cur + 2 : -1;
    indices[1] = cur - 1 >= 0 ? cur - 1 : -1;
    indices[0] = cur - 2 >= 0 ? cur - 2 : -1;
    for (idx = 0; idx < 5; ++idx)
    {
        const ROUTE_VERTEX *vtx;
        uint8 type;

        weights[idx] = 0;
        if (indices[idx] < 0)
            continue;
        vtx = &seg->vertices[indices[idx]];
        if (vtx->pos[0] == 255 && vtx->pos[2] == 255 && vtx->pos[1] == 255)
        {
            indices[idx] = -1;
            continue;
        }
        if (route->hazard != 8 && route->behavior == 3 && indices[idx] != (sint16)route->lane_timer)
            weights[idx] = weights[idx] + cfg->lane_change;
        type = vtx->lane;
        if (type == 1)
            weights[idx] = weights[idx] + 1u;
        else if (type == 2)
            weights[idx] = weights[idx] + cfg->lane2;
        else if (type == 3)
            weights[idx] = weights[idx] + cfg->lane3;
        else
            weights[idx] = 0;
    }
    if (indices[1] < 0)
        indices[1] = indices[0];
    if (indices[3] < 0)
        indices[3] = indices[4];
    weights[1] = weights[1] + weights[0];
    weights[2] = (uint32)weights[2] << 1;
    weights[3] = weights[3] + weights[4];
    x = boat->pose.m[0][2];
    z = boat->pose.m[2][2];
    heading = ratan2(x, z);
    idx = route->target_seg;
    table = archive->segs;
    seg = table[idx];
    for (idx = 1; idx <= 3; ++idx)
    {
        const ROUTE_VERTEX *vtx;
        sint32 angle;
        sint32 turns;
        sint32 delta;
        uint8 coord;

        if (indices[idx] < 0)
            continue;
        vtx = &seg->vertices[indices[idx]];
        candidate_x[idx] = seg->origin[0] + (vtx->pos[0] << 4) - boat->contacts.points[0].pos.x;
        coord = vtx->pos[2];
        component = seg->origin[2];
        candidate_z[idx] = component + (coord << 4) - boat->contacts.points[0].pos.z;
        angle = ratan2(candidate_x[idx], candidate_z[idx]);
        turns = sra(angle + (angle < 0 ? 4095u : 0u), 12u);
        delta = heading - (angle - ((uint32)turns << 12));
        candidate_yaw[idx] = delta;
        if (abs(delta) < cfg->yaw_limit && vtx->lane >= 2u)
            weights[idx] = weights[idx] + cfg->heading;
    }
    if (weights[3] < weights[1])
        sel = weights[2] < weights[1] ? 1 : 2;
    else if (weights[1] < weights[3])
        sel = weights[2] < weights[3] ? 3 : 2;
    else
    {
        sel = 2;
        if (weights[2] <= 0 && indices[2] < 0)
        {
            sel = 1;
            if (indices[1] < 0)
            {
                sel = 3;
                if (indices[3] < 0)
                {
                    sel = 4;
                    if (indices[4] < 0)
                        sel = 0;
                }
            }
        }
    }
    if (sel < 1 || sel > 3)
        exit(1);
    if (indices[sel] < 0)
        exit(1);
    x = candidate_x[sel];
    z = candidate_z[sel];
    dst->vector.x = x;
    dst->vector.z = z;
    if (x + 6096u >= 12193u || z + 6096u >= 12193u)
    {
        dst->vector.z = 0;
        dst->vector.y = 0;
        dst->vector.x = 0;
        return 0;
    }
    len = SquareRoot0(mul32(x, x) + mul32(z, z));
    dst->len = len;
    if (len == 0)
    {
        dst->vector.z = 0;
        dst->vector.x = 0;
    }
    dst->vector.x = (sint32)((uint32)x << 12) / len;
    dst->vector.z = (sint32)((uint32)z << 12) / len;
    yaw = candidate_yaw[sel];
    if (yaw >= 2049)
        yaw -= 4096u;
    else if (yaw < -2048)
        yaw += 4096u;
    dst->vector.y = yaw;
    route->target_lane = indices[sel];
    return indices[sel];
}

sint32 route_aim(BOAT *boat, const ROUTE_SEGMENT *seg)
{
    const ROUTE_SEGMENT *target = seg->next.seg->next.seg;
    sint16 lane = boat->route.target_lane;
    uint32 pos_x = boat->contacts.points[0].pos.x;
    const ROUTE_VERTEX *vtx = &target->vertices[lane];
    uint8 vertex_x = vtx->pos[0];
    sint32 base_x = target->origin[0];
    sint32 x = base_x + (vertex_x << 4) - pos_x;
    uint32 x_sq;
    uint8 vertex_z;
    uint32 pos_z;
    sint32 z;
    uint32 z_sq;
    sint32 len;
    sint32 n_x;
    sint32 n_z;

    FUNCTION_MARKER(0x8002D1DC, "MAIN.EXE");
    boat->route_target.vector.x = x;
    x = boat->route_target.vector.x;
    x_sq = x * x;
    vertex_z = vtx->pos[2];
    pos_z = boat->contacts.points[0].pos.z;
    z = target->origin[2] + (vertex_z << 4) - pos_z;
    z_sq = z * z;
    boat->route_target.vector.z = z;
    len = SquareRoot0(x_sq + z_sq);
    n_x = (sint32)((uint32)boat->route_target.vector.x << 12) / len;
    boat->route_target.len = len;
    n_z = (sint32)((uint32)boat->route_target.vector.z << 12) / boat->route_target.len;
    boat->route.speed = 7168;
    boat->route_target.vector.x = n_x;
    boat->route_target.vector.z = n_z;
    return n_z;
}

sint32 route_build_boundaries(ROUTE_RESOURCES *route, ROUTE_BOUNDARIES *dst)
{
    ROUTE_BOUNDARY *entry = dst->entries;
    sint32 mode = 0;
    sint32 object_idx = 0;
    sint32 seg_state = 1;

    FUNCTION_MARKER(0x8001BC3C, "MAIN.EXE");
    while (object_idx < (sint32)route->count)
    {
        ROUTE_SEGMENT *const *table = route->segs;
        ROUTE_SEGMENT *object = table[object_idx];
        uint32 left_idx = object->join[0];
        uint8 *cursor = object->commands;
        uint8 flags = *cursor;
        ROUTE_SEGMENT *linked = object->prev.seg;
        uint32 right_idx = object->join[1];
        uint8 *prev = NULL;
        sint32 transitions = 0;

        if ((flags & 0x0Cu) != 0x0C)
            *cursor = flags | 4u;
        if ((*cursor & 0x0Cu) != 0x0C)
        {
            for (;;)
            {
                uint32 type;

                if (seg_state != 1 || (*cursor & 3u) != 0)
                {
                    if (!seg_state)
                    {
                        flags = *cursor;
                        if ((flags & 3u) != 0)
                        {
                            seg_state = 1;
                            if (mode == 2 && transitions == 1)
                                *cursor = flags | 8u;
                            ++transitions;
                        }
                    }
                }
                else
                {
                    seg_state = 0;
                    if (mode == 2 && transitions == 2 && prev != NULL)
                        *prev = *prev | 4u;
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
                type = *cursor & 3;
                if (type != 2)
                    ++left_idx;
                prev = cursor;
                if (type != 1)
                    ++right_idx;
                ++cursor;
                if ((*cursor & 0x0Cu) == 0x0C)
                    break;
            }
        }
        if (prev != NULL)
        {
            flags = *prev;
            if ((flags & 4u) == 0)
                *prev = flags | 8u;
        }
        if (!seg_state)
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
                --object_idx;
            }
        }
        ++object_idx;
        seg_state = 1;
    }
    entry->seg = NULL;
    entry->vertex_idx = 0;
    dst->count = entry - dst->entries;
    for (entry = dst->entries; entry->seg != NULL; ++entry)
    {
        const ROUTE_VERTEX *v0 = &entry->seg->vertices[entry->vertex_idx];
        const ROUTE_VERTEX *v1 = v0 + 1;
        VECTOR dir;
        sint32 last = entry->seg->next.idx - 1;

        if (last < 0)
            last = entry->seg->prev.idx + 1;
        entry->route_idx = last;
        dir.vx = 16 * (v1->pos[0] - v0->pos[0]);
        dir.vy = 0;
        dir.vz = 16 * (v1->pos[2] - v0->pos[2]);
        dir.pad = 0;
        VectorNormalS(&dir, &entry->dir);
        entry->n.vx = -entry->dir.vz;
        entry->n.vy = 0;
        entry->n.vz = entry->dir.vx;
    }
    return 0;
}

static sint32 route_init_cmds(ROUTE_RESOURCES *route)
{
    sint32 count = route->count;
    sint32 idx = 0;
    sint32 result = count;

    FUNCTION_MARKER(0x8001BB04, "MAIN.EXE");
    if (count <= 0)
        return result;
    do
    {
        uint8 *object = route->segs[idx]->commands;
        uint8 *end = object + 14u;

        do
        {
            uint8 old_val = *object;
            uint32 val = old_val & 6;
            uint8 next = old_val & 0xF0u;

            *object = next;
            if (val == 6)
            {
                *object = 0xFF;
            }
            else if ((old_val & 8u) != 0)
            {
                if (val != 4)
                    abort();
                *object = *object;
            }
            else if (val == 0)
            {
                *object = next | 1u;
            }
            else if (val == 2)
            {
                *object = *object | 2u;
            }
            else if (val == 4)
            {
                *object = *object | 3u;
            }
            ++object;
        } while (object < end);
        count = route->count;
        idx++;
        result = idx < count;
    } while (result != 0);
    return result;
}

extern sint32 route_sample_geom(const ROUTE_SEGMENT *, sint32, sint32, sint32[4]);

static void route_sample_boundary(const sint32 pos[3], const ROUTE_BOUNDARY *src)
{
    sint32 output[4];
    VECTOR delta;
    VECTOR sq;

    FUNCTION_MARKER(0x80026CB8, "MAIN.EXE");
    route_sample_geom(src->seg, src->vertex_idx, 1, output);
    delta.vx = (sint16)((uint16)pos[0] - (uint16)output[0]);
    delta.vy = 0;
    delta.vz = (sint16)((uint16)pos[2] - (uint16)output[2]);
    delta.pad = 0;
    Square0(&delta, &sq);
}

void boat_sample_boundary(BOAT *boat, const ROUTE_BOUNDARIES *boundaries)
{
    const sint32 slots[3] = {1, 3, 4};
    BOAT_CONTACTS *contacts = &boat->contacts;
    sint32 cur = contacts->boundary_section;
    sint32 prev = cur - 1u;
    sint32 next = cur + 1u;
    const ROUTE_BOUNDARY *desc = &boundaries->entries[cur];
    const ROUTE_BOUNDARY *cursor = desc;
    SVECTOR t;

    FUNCTION_MARKER(0x80026DC0, "MAIN.EXE");
    if (boat->control.driver - 2 >= 2)
        return;
    SetRotMatrix(&boat->pose);
    SetTransMatrix(&boat->pose);
    for (sint32 idx = 0; idx < 3; ++idx)
        RotTransSV(&contacts->points[slots[idx]].sample, &t, NULL);
    if (desc->seg == NULL)
        return;
    if (prev < 0)
    {
        do
        {
            ++cursor;
            ++prev;
        } while (cursor->seg != NULL);
    }
    if (cursor[1].seg == NULL)
        next = 0;
    route_sample_boundary(boat->pos.v, &boundaries->entries[prev]);
    route_sample_boundary(boat->pos.v, desc);
    route_sample_boundary(boat->pos.v, &boundaries->entries[next]);
    // Original equal stack-pointer returns exclude every correction path
}

static sint16 route_geometry_lo_s16(uint32 val)
{
    return val;
}

static sint16 route_geometry_hi_s16(uint32 val)
{
    return val >> 16;
}

static uint32 route_geometry_set_lo_u16(uint32 val, sint32 low)
{
    return val & 0xFFFF0000 | (uint16)low;
}

static sint32 route_geometry_mul_shift12(sint32 v0, sint32 v1)
{
    return (sint64)v0 * v1 >> 12;
}

static sint16 route_vertices_lo_s16(uint32 val)
{
    return val;
}

static sint16 route_vertices_hi_s16(uint32 val)
{
    return val >> 16;
}

static uint32 route_vertices_set_lo_s16(uint32 val, sint32 low)
{
    return val & 0xFFFF0000 | (uint16)low;
}

static uint32 route_vertices_set_hi_s16(uint32 val, sint32 high)
{
    return val & 0x0000FFFF | (uint32)(uint16)high << 16;
}

static sint8 route_vertices_slope_clamp(sint32 numerator, sint32 denominator)
{
    sint32 val = numerator / denominator;

    return clamp(val, -127, 127);
}

static void route_gen_geom_fractal(ROUTE_SEGMENT *recs, sint32 count)
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
    ROUTE_SEGMENT *cur_rec = recs;
    sint32 straight = 1;
    sint32 prev_rise = 0;
    sint32 prev_fall = 0;
    uint16 width = 256;
    uint16 width_delta = 1;
    uint8 pattern = 0;
    sint32 idx;

    FUNCTION_MARKER(0x8003DA8C, "MAIN.EXE");
    if (count > 0)
    {
        ROUTE_SEGMENT *last = &recs[count - 1];
        sint32 point;

        last->origin[0] = 0;
        last->origin[1] = 0;
        last->origin[2] = 0;
        for (point = 0; point < 14; ++point)
            last->vertices[point].tex_phase = random_next();
    }
    for (idx = 0; idx < count; ++idx)
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
        sint32 angle;
        sint32 c;
        sint32 s;
        sint16 minimum_x;
        sint16 minimum_y;
        sint16 minimum_z;
        sint32 point;

        remaining = count - idx - 1;
        if (route_geometry_lo_s16(turn_phase) / 4096 == 1)
        {
            if ((sint16)remaining < 65)
            {
                turn_phase = route_geometry_set_lo_u16(turn_phase, 0);
                turn_step = 0x00200020;
                turn_amount = route_geometry_set_lo_u16(turn_amount, 0);
            }
            else
            {
                sint32 bounded = remaining >= 129 ? 128 : remaining;
                sint32 half = bounded / 2;
                sint32 len = half + random_next() % half;
                sint32 quarter = len / 4;
                sint32 len8 = len / 8;
                sint32 v0 = len8 + random_next() % quarter;
                sint32 divisor0 = 2048 / v0;
                sint32 duration0 = 2048 / divisor0;
                sint32 divisor1 = 2048 / (len - duration0);
                sint32 duration1 = 2048 / divisor1;

                straight = 0;
                turn_step = (uint16)divisor0 | (uint32)(uint16)divisor1 << 16;
                turn_amount = route_geometry_set_lo_u16(turn_amount, 8 * duration1);
                if (route_geometry_lo_s16(turn_amount) / duration0 >= 65)
                    turn_amount = route_geometry_set_lo_u16(turn_amount, duration0 << 6);
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
                slope_step = 0x00200020;
                slope_amount = 0;
                slope_phase = 0x00180000;
            }
            else
            {
                sint32 bounded = remaining >= 129 ? 128 : remaining;
                sint32 half = bounded / 2;
                sint32 len = bounded / 4 + random_next() % half;
                sint32 quarter = len / 4;
                sint32 v0 = quarter + random_next() % quarter;
                sint32 divisor0 = 2048 / v0;
                sint32 duration0 = 2048 / divisor0;
                sint32 divisor1 = 2048 / (len - duration0);
                sint32 duration1 = 2048 / divisor1;
                sint32 phase_divisor;

                slope_step = (uint16)divisor0 | (uint32)(uint16)divisor1 << 16;
                slope_amount = route_geometry_set_lo_u16(slope_amount, 32 * (duration0 + random_next() % duration0));
                phase_divisor = duration1 + random_next() % duration1;
                slope_amount = slope_amount & 0x0000FFFF | (uint32)(uint16)(32 * phase_divisor) << 16;
                phase_divisor = random_next();
                phase_divisor -= 8 * (phase_divisor / 8) + 24;
                slope_phase = (uint32)(uint16)phase_divisor << 16;
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
            if (width == 256)
                width_delta = 0;
            else
            {
                width_delta = 1;
                if (width > 256u)
                    width_delta = 0xFFFF;
            }
        }
        else if (width_delta == 0)
            width_delta = 1;
        if (width >= 301u)
            width_delta = -(random_next() & 3);
        else if (width < 140u)
            width_delta = random_next() & 3;
        width += width_delta;

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
            sint32 random = random_next() % (sint8)(random_ranges >> 16);

            radius[2] -= route_geometry_hi_s16(left_offsets);
            radius[1] = radius[2];
            radius[0] = radius[2] - (sint16)width - route_geometry_lo_s16(outer_offsets) - (random << 6);
        }
        else
        {
            radius[0] -= route_geometry_hi_s16(outer_offsets) + (random_next() % (sint8)(random_ranges >> 24) << 6);
            radius[1] -= route_geometry_lo_s16(outer_offsets) + (random_next() % (sint8)(random_ranges >> 16) << 6);
            radius[2] -= route_geometry_hi_s16(left_offsets) + (random_next() % (sint8)(random_ranges >> 8) << 6);
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
            sint32 random = random_next() % (sint8)(right_random_ranges >> 16);

            radius[10] += route_geometry_hi_s16(right_offsets);
            radius[11] = radius[10];
            radius[12] = radius[10] + (sint16)width + route_geometry_lo_s16(right_outer_offsets) + (random << 6);
        }
        else
        {
            radius[10] += route_geometry_hi_s16(right_offsets) + (random_next() % (sint8)(right_random_ranges >> 8) << 6);
            radius[11] += route_geometry_lo_s16(right_outer_offsets) + (random_next() % (sint8)(right_random_ranges >> 16) << 6);
            radius[12] += route_geometry_hi_s16(right_outer_offsets) + (random_next() % (sint8)(right_random_ranges >> 24) << 6);
        }

        angle = (idx << 12) / count;
        c = rcos(angle);
        s = rsin(angle);
        if (straight)
        {
            for (point = 0; point < 14; ++point)
                y[point] = curve;
        }
        else if (route_geometry_lo_s16(slope_phase) >= 2049)
        {
            for (point = 13; point >= 0; --point)
            {
                if (point >= 9)
                    y[point] = curve;
                else if (point < 3)
                    y[point] = y[point + 1];
                else if (route_geometry_lo_s16(slope_phase) >= 3073)
                    y[point] = y[point + 1] + (4096 - route_geometry_lo_s16(slope_phase)) / route_geometry_hi_s16(slope_phase);
                else
                    y[point] = y[point + 1] + (route_geometry_lo_s16(slope_phase) - 2048) / route_geometry_hi_s16(slope_phase);
            }
        }
        else
        {
            for (point = 0; point < 14; ++point)
            {
                if (point < 4)
                    y[point] = curve;
                else if (point >= 10)
                    y[point] = y[point - 1];
                else if (route_geometry_lo_s16(slope_phase) >= 1025)
                    y[point] = y[point - 1] + (2048 - route_geometry_lo_s16(slope_phase)) / route_geometry_hi_s16(slope_phase);
                else
                    y[point] = y[point - 1] + route_geometry_lo_s16(slope_phase) / route_geometry_hi_s16(slope_phase);
            }
        }
        for (point = 0; point < 14; ++point)
        {
            x[point] = route_geometry_mul_shift12(c, radius[point]);
            z[point] = route_geometry_mul_shift12(s, radius[point]);
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
        cur_rec->origin[0] = minimum_x;
        cur_rec->origin[1] = minimum_y;
        cur_rec->origin[2] = minimum_z;
        for (point = 0; point < 14; ++point)
        {
            ROUTE_VERTEX *packed = &cur_rec->vertices[point];

            packed->pos[0] = clamp(x[point] - minimum_x >> 4, 0, 255);
            packed->pos[1] = clamp(y[point] - minimum_y >> 4, 0, 255);
            packed->pos[2] = clamp(z[point] - minimum_z >> 4, 0, 255);
        }
        cur_rec->vertex_count = 14;
        cur_rec->next.seg = &recs[idx + 1];
        cur_rec->next.idx = idx + 1;
        cur_rec->prev.seg = idx > 0 ? &recs[idx - 1] : NULL;
        cur_rec->prev.idx = idx - 1;
        cur_rec->join[0] = cur_rec->join[1] = 0;
        cur_rec->section = 1;
        cur_rec->vertices[12].pos[1] = 0;

        cur_rec->vertices[1].pos[1] = cur_rec->vertices[1].pos[1] + (color_bases >> 16) + random_next() % (sint8)(color_ranges >> 16);
        if (straight)
        {
            cur_rec->vertices[2].pos[1] = cur_rec->vertices[2].pos[1] + 20u + cur_rec->vertices[3].pos[1];
            cur_rec->vertices[0].pos[1] = cur_rec->vertices[0].pos[1] + (color_bases >> 24) + random_next() % (sint8)(color_ranges >> 24);
        }
        else if (rise == 1)
        {
            uint8 val;

            cur_rec->vertices[2].pos[1] = cur_rec->vertices[2].pos[1] + (color_bases >> 8);
            val = cur_rec->vertices[2].pos[1] + 20u;
            cur_rec->vertices[0].pos[1] = cur_rec->vertices[1].pos[1];
            cur_rec->vertices[1].pos[1] = val;
        }
        else
        {
            cur_rec->vertices[2].pos[1] = cur_rec->vertices[2].pos[1] + (color_bases >> 8) + random_next() % (sint8)(color_ranges >> 8);
            cur_rec->vertices[0].pos[1] = cur_rec->vertices[0].pos[1] + (color_bases >> 24) + random_next() % (sint8)(color_ranges >> 24);
        }

        cur_rec->vertices[11].pos[1] = cur_rec->vertices[11].pos[1] + (right_color_bases >> 16) + random_next() % (sint8)(right_color_ranges >> 16);
        if (straight)
        {
            cur_rec->vertices[10].pos[1] = cur_rec->vertices[9].pos[1] + 20u;
            cur_rec->vertices[12].pos[1] = cur_rec->vertices[12].pos[1] + (right_color_bases >> 24) + random_next() % (sint8)(right_color_ranges >> 24);
        }
        else if (fall == 1)
        {
            uint8 val;

            cur_rec->vertices[10].pos[1] = cur_rec->vertices[10].pos[1] + (right_color_bases >> 8);
            val = cur_rec->vertices[10].pos[1] + 20u;
            cur_rec->vertices[12].pos[1] = cur_rec->vertices[11].pos[1];
            cur_rec->vertices[11].pos[1] = val;
        }
        else
        {
            cur_rec->vertices[10].pos[1] = cur_rec->vertices[10].pos[1] + (right_color_bases >> 8) + random_next() % (sint8)(right_color_ranges >> 8);
            cur_rec->vertices[12].pos[1] = cur_rec->vertices[12].pos[1] + (right_color_bases >> 24) + random_next() % (sint8)(right_color_ranges >> 24);
        }

        pattern = route_gen_patterns[pattern][random_next() & 3];
        for (point = 0; point < 14; ++point)
        {
            uint8 src = route_gen_srcs[point + 1];
            uint8 material;
            const uint8 *choices = route_gen_materials[pattern];

            if ((src & 1u) != 0)
                material = 1;
            else if ((src & 2u) != 0)
            {
                if (straight && point == 9 || !straight && fall == 1 && prev_fall == 1 && point == 10)
                    material = 24;
                else
                {
                    uint8 choice = src - 2u;

                    if (!straight && fall == 1 && prev_fall == 1 && point >= 11)
                        choice = route_gen_srcs[point] - 2u;
                    material = choices[choice >> 2];
                }
                if (material >= 7u)
                    material += 2u;
            }
            else if (straight && point == 2)
                material = 24;
            else if (!straight && rise == 1 && prev_rise == 1 && point == 1)
                material = 24;
            else
            {
                material = choices[(!straight && rise == 1 && prev_rise == 1 && point == 0 ? route_gen_srcs[2] : src) >> 2];
            }
            cur_rec->commands[point] = route_gen_commands[point] | (uint8)(material << 3);
        }
        prev_rise = rise;
        prev_fall = fall;
        ++cur_rec;
    }
    if (count > 0)
    {
        recs->prev.seg = &recs[count - 1];
        recs->prev.idx = count - 1;
        recs[count - 1].next.seg = recs;
        recs[count - 1].next.idx = 0;
    }
}

static sint32 route_gen_vertices_fractal(ROUTE_SEGMENT *rec, sint32 count)
{
    ROUTE_SEGMENT *prev = rec->prev.seg;
    uint32 control = route_gen_defaults.control;
    uint32 delta = route_gen_defaults.delta;
    sint16 decoration_gap = 0;
    sint32 idx;
    sint32 point;

    FUNCTION_MARKER(0x8003F164, "MAIN.EXE");
    for (point = 0; point < 14; ++point)
    {
        prev->vertices[point].phase[0] = 32;
        prev->vertices[point].phase[1] = 32;
        prev->vertices[point].curve = 72;
    }
    for (idx = 0; idx < count; ++idx)
    {
        if (route_vertices_lo_s16(control) != 0)
        {
            control = route_vertices_set_lo_s16(control, route_vertices_lo_s16(control) - 1);
            control = route_vertices_set_hi_s16(control, route_vertices_hi_s16(control) + route_vertices_lo_s16(delta));
        }
        else if (count - idx < 65)
        {
            delta = route_vertices_set_lo_s16(delta, -2);
            control = route_vertices_set_lo_s16(control, route_vertices_hi_s16(control) / 2);
        }
        else
        {
            sint32 val;

            delta = route_vertices_set_hi_s16(delta, (random_next() & 3) + 10);
            val = random_next() % 64 - route_vertices_hi_s16(control) / 2;
            if ((sint16)val < 0)
            {
                control = route_vertices_set_lo_s16(control, -(sint16)val);
                delta = route_vertices_set_lo_s16(delta, -2);
            }
            else
            {
                control = route_vertices_set_lo_s16(control, (sint16)val);
                delta = route_vertices_set_lo_s16(delta, 2);
            }
        }

        rec->vertices[0].tex_phase = prev->vertices[0].tex_phase - 94u;
        rec->vertices[1].tex_phase = prev->vertices[1].tex_phase - 94u;
        rec->vertices[2].tex_phase = prev->vertices[2].tex_phase - 94u;
        rec->vertices[3].tex_phase = prev->vertices[3].tex_phase - 59u;
        rec->vertices[4].tex_phase = prev->vertices[4].tex_phase - 27u;
        rec->vertices[5].tex_phase = prev->vertices[5].tex_phase - 16u;
        rec->vertices[6].tex_phase = prev->vertices[6].tex_phase - 40u;
        rec->vertices[7].tex_phase = prev->vertices[7].tex_phase - 67u;
        rec->vertices[8].tex_phase = prev->vertices[8].tex_phase - 98u;
        rec->vertices[9].tex_phase = prev->vertices[9].tex_phase - 98u;
        rec->vertices[10].tex_phase = prev->vertices[10].tex_phase - 84u;
        rec->vertices[11].tex_phase = prev->vertices[11].tex_phase - 84u;
        rec->vertices[12].tex_phase = prev->vertices[12].tex_phase - 84u;
        rec->vertices[13].tex_phase = prev->vertices[13].tex_phase - 84u;
        for (point = 0; point < 14; ++point)
        {
            ROUTE_VERTEX *entry = &rec->vertices[point];
            sint32 control_high = route_vertices_hi_s16(control);
            sint32 delta_high = route_vertices_hi_s16(delta);
            sint32 high_div16 = control_high / 16;

            entry->flags = 14;
            entry->phase[0] = control_high / 2 + prev->vertices[point].phase[0];
            entry->phase[1] = point * ((control >> 16) + 32);
            entry->curve = 16 * ((delta_high - high_div16) / 2) | (delta_high - (high_div16 - 4)) / 2;
            entry->deform[0] = 0;
            entry->deform[1] = 0;
            entry->impact_tick = -256;
            entry->lane = 2;
        }
        {
            sint32 denominator = 16 * (rec->vertices[2].pos[1] - rec->vertices[3].pos[1]);
            sint8 x_slope = route_vertices_slope_clamp(rec->vertices[2].pos[0] - rec->vertices[3].pos[0] << 9, denominator);
            sint8 z_slope = route_vertices_slope_clamp(rec->vertices[2].pos[2] - rec->vertices[3].pos[2] << 9, denominator);

            rec->vertices[3].deform[0] = -x_slope;
            rec->vertices[3].deform[1] = -z_slope;
        }
        {
            sint32 denominator = 16 * (rec->vertices[10].pos[1] - rec->vertices[9].pos[1]);
            sint8 x_slope = route_vertices_slope_clamp(rec->vertices[10].pos[0] - rec->vertices[9].pos[0] << 9, denominator);
            sint8 z_slope = route_vertices_slope_clamp(rec->vertices[10].pos[2] - rec->vertices[9].pos[2] << 9, denominator);

            rec->vertices[9].deform[0] = -x_slope;
            rec->vertices[9].deform[1] = -z_slope;
        }

        prev = rec;
        if (decoration_gap >= 31 && (random_next() & 0xEu) != 0)
        {
            sint32 placed = 0;
            sint16 pos = random_next() & 3 | 4;

            while (pos < 9 && placed < 3)
            {
                sint32 random = random_next() & 3;
                sint32 kind = random + 2;

                if (random == 1)
                    kind = 4;
                else if (random == 3)
                {
                    if ((random_next() & 7) == 0)
                    {
                        if ((random_next() & 1) != 0)
                            break;
                        decoration_gap = 0;
                    }
                    else
                        kind = 2;
                }
                else if (kind >= 6)
                    break;
                decoration_gap = 0;
                rec->vertices[pos].flags = rec->vertices[pos].flags | (uint8)(kind << 5);
                ++placed;
                ++pos;
            }
        }
        ++decoration_gap;
        rec = prev->next.seg;
    }
    return idx < count;
}

sint32 route_update_speed_ctrl(const BOAT *src, BOAT *boat, uint32 race_mode)
{
    sint32 distance;
    sint32 dir;
    const AI_ROUTE_CFG *cfg;
    sint32 val;
    sint32 limit;
    sint32 result;

    FUNCTION_MARKER(0x8002F3E8, "MAIN.EXE");
    if (race_mode == 5)
    {
        boat->route.speed_adjust = 0;
        return 5;
    }
    distance = boat->route.distance >> 12;
    dir = (sint16)boat->route.dir;
    cfg = boat->route.settings;
    boat->route.behavior = 0;
    if (dir == 1)
    {
        sint32 delta;
        sint32 divisor;

        if (distance < 137)
        {
            boat->route.behavior = 4;
            boat->route.lane_timer = src->contacts.points[0].left;
        }
        delta = distance - cfg->gap;
        if (delta < 0)
        {
            val = mul32(delta, cfg->max_adjust);
            divisor = cfg->boost_range;
            val = -val;
        }
        else
        {
            val = mul32(delta, cfg->min_adjust);
            divisor = cfg->slow_range;
        }
        val = val / divisor;
        boat->route.speed_adjust = val;
    }
    else if (dir == 2)
    {
        sint32 delta;
        sint32 divisor;

        if (distance < 137)
        {
            uint16 src_lane = src->contacts.points[0].left;

            boat->route.behavior = 3;
            boat->route.lane_timer = src_lane;
            if (distance < 51 && (sint16)boat->route.lane_timer == (sint32)src->contacts.points[0].left)
                boat->route.behavior = 5;
        }
        delta = distance + cfg->gap;
        if (delta < 0)
        {
            val = mul32(delta, cfg->min_adjust);
            divisor = cfg->slow_range;
            val = -val;
        }
        else
        {
            val = mul32(delta, cfg->max_adjust);
            divisor = cfg->boost_range;
        }
        val = val / divisor;
        boat->route.speed_adjust = val;
    }
    else
        boat->route.speed_adjust = 0;
    limit = cfg->min_adjust;
    val = boat->route.speed_adjust;
    if (val < limit)
    {
        boat->route.speed_adjust = limit;
        return 1;
    }
    limit = cfg->max_adjust;
    result = limit < val;
    if (result)
        boat->route.speed_adjust = limit;
    return result;
}

sint32 route_gen_init_fractal(const RACE_CFG *cfg)
{
    sint32 count;
    ROUTE_SEGMENT *recs = route_resources.storage;
    sint32 idx;

    FUNCTION_MARKER(0x8003D8DC, "MAIN.EXE");
    random_seed = name_reels.code;
    count = 600 + 8 * (random_next() & 0x31);
    memset(recs, 0, count * sizeof(*recs));
    route_resources.owned_count = count;
    route_resources.count = count;
    for (idx = 0; idx < count; ++idx)
        route_resources.segs[idx] = &recs[idx];
    route_gen_geom_fractal(recs, count);
    route_gen_vertices_fractal(recs, count);
    river_init_light_fractal(cfg, count);
    render_select_lighting(RENDER_LIGHT_NORMAL);
    return 192;
}
