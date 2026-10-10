#include <string.h>
#include "route.h"
#include "game.h"
#include "camera.h"
#include "scene.h"
#include "vehicle_select.h"
#include "vehicle.h"
#include "object.h"
/* Implements dual-buffer packet expansion from MAIN.EXE:0x8001E608 */
#include "arena.h"
#include "global.h"
#include "motion.h"
#include "render.h"
#include <stdlib.h>

static uint32 object_packet_header(uint32 raw, uint32 regular_code, uint32 alternate_code)
{
    return (raw & 0xFFFFFFu) | (((raw >> 24) & 0x80u) != 0u ? alternate_code : regular_code);
}

static void object_write_packet_pair(uint8 *first, uint8 *second, uint32 offset, uint32 value)
{
    memcpy(first + offset, &value, sizeof(value));
    memcpy(second + offset, &value, sizeof(value));
}

static void object_write_packet_word(uint8 *first, uint8 *second, uint32 offset, uint16 value)
{
    memcpy(first + offset, &value, sizeof(value));
    memcpy(second + offset, &value, sizeof(value));
}

static OBJECT_GROUP_STATE object_views[2];

OBJECT_GROUP_STATE *object_for_camera(const CAMERA_STATE *camera)
{
    for (uint32 idx = 0; idx < 2u; ++idx)
        if (camera == &camera_views[idx])
            return &object_views[idx];
    abort();
    return NULL;
}

static void object_init_group(OBJECT_GROUP_STATE *group)
{
    if (group->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    for (uint32 idx = 0; idx < group->capacity; ++idx)
    {
        group->slots[idx].section = 255u;
        group->slots[idx].status = 0u;
    }
}

sint32 object_node_flags_update_linked(uint32 unused)
{
    ROUTE_SEGMENT *node;
    sint32 index;

    FUNCTION_MARKER(0x80015370u, "MAIN.EXE");
    if (r_u32(0x800834A0u) == 2u && r_u32(0x8008348Cu) != 1u)
    {
        node = route_resources.segments[0];
        node = node->prev.seg;
        node = node->prev.seg;
        for (index = 0; index < 9; ++index)
        {
            sint32 item;
            for (item = 0; item < node->vertex_count; ++item)
            {
                uint8 *flags = &node->vertices[item].flags;
                uint8 value = *flags;

                if ((value >> 5) != 0u)
                {
                    value = *flags;
                    *flags = value & 0x1Fu;
                }
            }
            node = node->next.seg;
        }
    }

    {
        sint32 count = (sint32)route_resources.count;
        node = route_resources.segments[0];
        if (count > 0)
        {
            index = 0;
            do
            {
                sint32 item;

                for (item = 0; item < node->vertex_count; ++item)
                {
                    ROUTE_VERTEX *entry = &node->vertices[item];
                    uint8 flags = entry->flags;

                    entry->impact_tick = 0xFF00u;
                    if ((flags >> 5) >= 7u)
                    {
                        flags = entry->flags;
                        entry->flags = flags & 0x1Fu;
                    }
                }
                ++index;
                count = (sint32)route_resources.count;
                node = node->next.seg;
            } while (index < count);
        }
    }
    {
        sint32 count = scene_race->checkpoint_count;

        index = 0;
        if (count > 0)
        {
            do
            {
                sint32 node_index = scene_checkpoint((uint32)index)->seg;

                if ((uint32)node_index < route_resources.count)
                {
                    sint32 slot;
                    uint32 vtx_idx;
                    node = route_resources.segments[(uint32)node_index];
                    vtx_idx = node->join[0];
                    for (slot = 0; slot < 14; ++slot)
                    {
                        uint8 flags = node->commands[slot];

                        if ((flags & 0x0Cu) == 0x0Cu)
                            break;
                        if ((flags & 3u) == 0u)
                        {
                            if (vtx_idx >= ROUTE_VERTEX_CAPACITY - 1u)
                                abort();
                            node->vertices[vtx_idx].flags = (uint8)((node->vertices[vtx_idx].flags & 0x1Fu) | 0x20u);
                            ++vtx_idx;
                            node->vertices[vtx_idx].flags = (uint8)((node->vertices[vtx_idx].flags & 0x1Fu) | 0x20u);
                        }
                        else if ((flags & 3u) != 2u)
                        {
                            if (vtx_idx >= ROUTE_VERTEX_CAPACITY)
                                abort();
                            ++vtx_idx;
                        }
                    }
                }
                ++index;
                count = scene_race->checkpoint_count;
            } while (index < count);
        }
    }
    if ((sint16)game_selection.mode == 6 && (sint16)game_selection.rules == 1)
        scene_clear_group_item_flags(5);
    {
        uint32 mode = r_u32(0x80083484u);

        if (mode == 9u || mode == 4u)
        {
            scene_clear_group_item_flags(2);
            scene_clear_group_item_flags(5);
            scene_clear_group_item_flags(6);
        }
    }
    if (r_u32(0x80083478u) == 2u)
    {
        scene_clear_group_item_flags(3);
        return scene_clear_group_item_flags(5);
    }
    if ((sint16)game_selection.mode != 0)
        scene_clear_group_item_flags(3);
    else
        profile_cell_dispatch_complete();
    return scene_clear_group_item_flags(6);
}

sint32 object_init_groups(OBJECT_GROUP_STATE *first, OBJECT_GROUP_STATE *second, sint32 mode)
{
    FUNCTION_MARKER(0x8001DFCCu, "MAIN.EXE");
    first->capacity = OBJECT_GROUP_CAPACITY;
    second->capacity = mode == 1 ? 0u : OBJECT_GROUP_CAPACITY;
    object_init_group(first);
    if (mode == 2)
    {
        object_init_group(second);
        return 0;
    }
    return 2;
}

sint32 render_reconcile_slots(OBJECT_GROUP_STATE *state, uint8 slot)
{
    uint32 slot_idx = 0u;
    FUNCTION_MARKER(0x8001E13Cu, "MAIN.EXE");
    if (slot >= 2u || state->count > state->capacity || state->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    for (uint32 idx = 0; idx < state->capacity; ++idx)
        if (state->slots[idx].status)
            --state->slots[idx].status;
    for (uint32 idx = 0; idx < state->count; ++idx)
        for (uint32 candidate = 0; candidate < state->capacity; ++candidate)
            if (state->slots[candidate].section == state->items[idx].section)
            {
                state->slots[candidate].status = 2u;
                state->items[idx].status = 1u;
                break;
            }
    for (uint32 idx = 0; idx < state->count; ++idx)
    {
        if (state->items[idx].status == 1u)
            continue;
        while (slot_idx < state->capacity && state->slots[slot_idx].status != 0u)
            ++slot_idx;
        if (slot_idx == state->capacity)
            return 0;
        OBJECT_GROUP_ENTRY *dst = &state->slots[slot_idx++];
        uint16 section = state->items[idx].section;
        object_linked_lists_expand2(scene_section(section), slot);
        state->items[idx].status = 1u;
        dst->section = section;
        dst->status = 2u;
    }
    return 0;
}

sint32 object_init_racer_object_groups(uint32 first, uint32 second, sint32 mode)
{
    OBJECT_GROUP_STATE *first_group = object_for_camera(camera_for_view(first));
    OBJECT_GROUP_STATE *second_group = object_for_camera(camera_for_view(second));

    FUNCTION_MARKER(0x8001E344u, "MAIN.EXE");
    object_init_groups(first_group, second_group, mode);
    object_groups_build_linked(vehicle_player(first)->contacts.points[0].seg, first_group, camera_for_view(first));
    render_reconcile_slots(first_group, (uint8)r_u32(first + 4u));
    if (mode == 2)
    {
        object_groups_build_linked(vehicle_player(second)->contacts.points[0].seg, second_group, camera_for_view(second));
        return render_reconcile_slots(second_group, (uint8)r_u32(second + 4u));
    }
    return 2;
}

sint32 object_groups_build_linked(const ROUTE_SEGMENT *object, OBJECT_GROUP_STATE *output, const CAMERA_STATE *configuration)
{
    OBJECT_GROUP_ENTRY *record = &output->items[0];
    sint32 first_count = (sint16)(uint16)configuration->visible[0];
    sint32 second_count = (sint16)(uint16)configuration->visible[1];
    sint32 third_count = (sint16)(uint16)configuration->visible[2];
    sint32 total;
    sint16 direction;
    const ROUTE_SEGMENT *current;
    sint32 index;

    FUNCTION_MARKER(0x8001E3F0u, "MAIN.EXE");
    if (output->capacity == 0u || output->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    output->count = 1u;
    total = first_count + second_count + third_count;
    record->section = object->section;
    record->status = 0u;
    direction = (sint16)(uint16)configuration->direction;
    current = direction == 1 ? object->next.seg : object->prev.seg;
    if (total <= 1)
        return 0;
    for (index = 1; index < total; ++index)
    {
        if (current->section != record->section)
        {
            uint16 identifier;

            ++output->count;
            identifier = current->section;
            ++record;
            record->status = 0u;
            record->section = identifier;
            {
                uint16 count = output->count;
                uint16 capacity = output->capacity;

                if (count == capacity)
                    return capacity;
            }
        }
        current = direction == 1 ? current->next.seg : current->prev.seg;
    }
    return 0;
}

void object_linked_lists_expand2(const SCENE_SECTION *section, uint8 slot)
{
    FUNCTION_MARKER(0x8001E564u, "MAIN.EXE");
    object_list_expand_linked(section->groups[0], slot);
    object_list_expand_linked(section->groups[1], slot);
}

void object_list_expand_linked(SCENE_GROUP_NODE *node, uint8 slot)
{
    FUNCTION_MARKER(0x8001E5B0u, "MAIN.EXE");
    if (slot >= 2u)
        abort();
    while (node)
    {
        object_build_prim_group_packets(node->group, slot);
        node = node->next;
    }
}

void object_build_prim_group_packets(SCENE_PRIM_GROUP *src, uint8 slot)
{
    uint32 count;
    uint32 loop_count;
    uint8 *first;
    uint8 *second;
    SCENE_PRIM_FACE *faces;
    uint32 index;

    FUNCTION_MARKER(0x8001E608u, "MAIN.EXE");
    if (slot >= 2u)
        abort();

    count = src->sets[0].count;
    loop_count = src->sets[0].count;
    if (loop_count != 0u)
    {
        if (!src->prims[slot][0].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_F3)))
                abort();
            src->prims[slot][0].data = calloc((size_t)count * 2u, sizeof(POLY_F3));
            if (!src->prims[slot][0].data)
                abort();
        }
        first = src->prims[slot][0].data;
        second = first + count * sizeof(POLY_F3);
        faces = src->sets[0].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint32 header = object_packet_header(raw, 0x20000000u, 0x22000000u);

            object_write_packet_pair(first + index * 20u, second + index * 20u, 4u, header);
            faces[index].v[0].pad = (sint16)((raw >> 24) & 0x7Fu);
        }
    }

    count = src->sets[1].count;
    loop_count = src->sets[1].count;
    if (loop_count != 0u)
    {
        if (!src->prims[slot][1].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_F4)))
                abort();
            src->prims[slot][1].data = calloc((size_t)count * 2u, sizeof(POLY_F4));
            if (!src->prims[slot][1].data)
                abort();
        }
        first = src->prims[slot][1].data;
        second = first + count * sizeof(POLY_F4);
        faces = src->sets[1].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint32 header = object_packet_header(raw, 0x28000000u, 0x2A000000u);

            object_write_packet_pair(first + index * 24u, second + index * 24u, 4u, header);
            faces[index].v[0].pad = (sint16)((raw >> 24) & 0x7Fu);
        }
    }

    count = src->sets[2].split;
    count += src->sets[2].count - src->sets[2].split;
    if (count != 0u)
    {
        uint32 first_count;
        uint32 second_count;

        if (!src->prims[slot][2].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_FT3)))
                abort();
            src->prims[slot][2].data = calloc((size_t)count * 2u, sizeof(POLY_FT3));
            if (!src->prims[slot][2].data)
                abort();
        }
        first = src->prims[slot][2].data;
        second = first + count * sizeof(POLY_FT3);
        first_count = src->sets[2].split;
        faces = src->sets[2].faces;
        for (index = 0u; index < first_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 32u;
            uint8 *packet1 = second + index * 32u;

            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 20u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 28u, (uint16)faces[index].v[2].pad);
            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x24000000u, 0x26000000u));
            object_write_packet_word(packet0, packet1, 30u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_word(packet0, packet1, 14u, (uint16)(faces[index].attrs[1] >> 0));
            object_write_packet_word(packet0, packet1, 22u, (uint16)(faces[index].attrs[1] >> 16));
            packet0[3] = 7u;
            packet1[3] = 7u;
        }
        second_count = src->sets[2].count - src->sets[2].split;
        faces = src->sets[2].faces + src->sets[2].split;
        for (index = 0u; index < second_count; ++index)
        {
            uint32 packet_index = first_count + index;
            uint8 *packet0 = first + packet_index * 32u;
            uint8 *packet1 = second + packet_index * 32u;
            uint8 tag_byte = (uint8)(faces[index].attrs[0] >> 24);

            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 20u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 28u, (uint16)faces[index].v[2].pad);
            object_write_packet_word(packet0, packet1, 14u, (uint16)faces[index].attrs[0]);
            object_write_packet_word(packet0, packet1, 22u, (uint16)(faces[index].attrs[1] >> 0));
            object_write_packet_pair(packet0, packet1, 4u, (tag_byte & 0x80u) != 0u ? 0x27000000u : 0x25000000u);
            object_write_packet_word(packet0, packet1, 30u, tag_byte);
            packet0[3] = 7u;
            packet1[3] = 7u;
        }
    }

    count = src->sets[3].split;
    count += src->sets[3].count - src->sets[3].split;
    if (count != 0u)
    {
        uint32 first_count;
        uint32 second_count;

        if (!src->prims[slot][3].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_FT4)))
                abort();
            src->prims[slot][3].data = calloc((size_t)count * 2u, sizeof(POLY_FT4));
            if (!src->prims[slot][3].data)
                abort();
        }
        first = src->prims[slot][3].data;
        second = first + count * sizeof(POLY_FT4);
        first_count = src->sets[3].split;
        faces = src->sets[3].faces;
        for (index = 0u; index < first_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 40u;
            uint8 *packet1 = second + index * 40u;

            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 20u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 28u, (uint16)faces[index].v[2].pad);
            object_write_packet_word(packet0, packet1, 36u, (uint16)faces[index].v[3].pad);
            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x2C000000u, 0x2E000000u));
            object_write_packet_word(packet0, packet1, 30u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_word(packet0, packet1, 14u, (uint16)(faces[index].attrs[1] >> 0));
            object_write_packet_word(packet0, packet1, 22u, (uint16)(faces[index].attrs[1] >> 16));
            packet0[3] = 9u;
            packet1[3] = 9u;
        }
        second_count = src->sets[3].count - src->sets[3].split;
        faces = src->sets[3].faces + src->sets[3].split;
        for (index = 0u; index < second_count; ++index)
        {
            uint32 packet_index = first_count + index;
            uint8 *packet0 = first + packet_index * 40u;
            uint8 *packet1 = second + packet_index * 40u;
            uint8 tag_byte = (uint8)(faces[index].attrs[0] >> 24);

            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 20u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 28u, (uint16)faces[index].v[2].pad);
            object_write_packet_word(packet0, packet1, 36u, (uint16)faces[index].v[3].pad);
            object_write_packet_word(packet0, packet1, 14u, (uint16)faces[index].attrs[0]);
            object_write_packet_word(packet0, packet1, 22u, (uint16)(faces[index].attrs[1] >> 0));
            object_write_packet_pair(packet0, packet1, 4u, (tag_byte & 0x80u) != 0u ? 0x2F000000u : 0x2D000000u);
            object_write_packet_word(packet0, packet1, 30u, tag_byte);
            packet0[3] = 9u;
            packet1[3] = 9u;
        }
    }

    count = src->sets[4].count;
    loop_count = src->sets[4].count;
    if (loop_count != 0u)
    {
        if (!src->prims[slot][4].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_G3)))
                abort();
            src->prims[slot][4].data = calloc((size_t)count * 2u, sizeof(POLY_G3));
            if (!src->prims[slot][4].data)
                abort();
        }
        first = src->prims[slot][4].data;
        second = first + count * sizeof(POLY_G3);
        faces = src->sets[4].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 28u;
            uint8 *packet1 = second + index * 28u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x30000000u, 0x32000000u));
            faces[index].v[0].pad = (sint16)((raw >> 24) & 0x7Fu);
            object_write_packet_pair(packet0, packet1, 12u, faces[index].attrs[1]);
            object_write_packet_pair(packet0, packet1, 20u, faces[index].attrs[2]);
            packet0[3] = 6u;
            packet1[3] = 6u;
        }
    }

    count = src->sets[5].count;
    loop_count = src->sets[5].count;
    if (loop_count != 0u)
    {
        if (!src->prims[slot][5].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_G4)))
                abort();
            src->prims[slot][5].data = calloc((size_t)count * 2u, sizeof(POLY_G4));
            if (!src->prims[slot][5].data)
                abort();
        }
        first = src->prims[slot][5].data;
        second = first + count * sizeof(POLY_G4);
        faces = src->sets[5].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 36u;
            uint8 *packet1 = second + index * 36u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x38000000u, 0x3A000000u));
            faces[index].v[0].pad = (sint16)((raw >> 24) & 0x7Fu);
            object_write_packet_pair(packet0, packet1, 12u, faces[index].attrs[1]);
            object_write_packet_pair(packet0, packet1, 20u, faces[index].attrs[2]);
            object_write_packet_pair(packet0, packet1, 28u, faces[index].attrs[3]);
            packet0[3] = 8u;
            packet1[3] = 8u;
        }
    }

    count = src->sets[6].count;
    if (count != 0u)
    {
        if (!src->prims[slot][6].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_GT3)))
                abort();
            src->prims[slot][6].data = calloc((size_t)count * 2u, sizeof(POLY_GT3));
            if (!src->prims[slot][6].data)
                abort();
        }
        first = src->prims[slot][6].data;
        second = first + count * sizeof(POLY_GT3);
        loop_count = src->sets[6].count;
        faces = src->sets[6].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 40u;
            uint8 *packet1 = second + index * 40u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x34000000u, 0x36000000u));
            object_write_packet_word(packet0, packet1, 38u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 16u, faces[index].attrs[1]);
            object_write_packet_pair(packet0, packet1, 28u, faces[index].attrs[2]);
            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 24u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 36u, (uint16)faces[index].v[2].pad);
            object_write_packet_word(packet0, packet1, 14u, (uint16)(faces[index].attrs[3] >> 0));
            object_write_packet_word(packet0, packet1, 26u, (uint16)(faces[index].attrs[3] >> 16));
            packet0[3] = 9u;
            packet1[3] = 9u;
        }
    }

    count = src->sets[7].count;
    if (count != 0u)
    {
        if (!src->prims[slot][7].data)
        {
            if ((size_t)count > SIZE_MAX / (2u * sizeof(POLY_GT4)))
                abort();
            src->prims[slot][7].data = calloc((size_t)count * 2u, sizeof(POLY_GT4));
            if (!src->prims[slot][7].data)
                abort();
        }
        first = src->prims[slot][7].data;
        second = first + count * sizeof(POLY_GT4);
        loop_count = src->sets[7].count;
        faces = src->sets[7].faces;
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = faces[index].attrs[0];
            uint8 *packet0 = first + index * 52u;
            uint8 *packet1 = second + index * 52u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x3C000000u, 0x3E000000u));
            object_write_packet_word(packet0, packet1, 38u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 16u, faces[index].attrs[1]);
            object_write_packet_pair(packet0, packet1, 28u, faces[index].attrs[2]);
            object_write_packet_pair(packet0, packet1, 40u, faces[index].attrs[3]);
            object_write_packet_word(packet0, packet1, 12u, (uint16)faces[index].v[0].pad);
            object_write_packet_word(packet0, packet1, 24u, (uint16)faces[index].v[1].pad);
            object_write_packet_word(packet0, packet1, 36u, (uint16)faces[index].v[2].pad);
            object_write_packet_word(packet0, packet1, 48u, (uint16)faces[index].v[3].pad);
            object_write_packet_word(packet0, packet1, 14u, (uint16)(faces[index].attrs[4] >> 0));
            object_write_packet_word(packet0, packet1, 26u, (uint16)(faces[index].attrs[4] >> 16));
            packet0[3] = 12u;
            packet1[3] = 12u;
        }
    }
}
