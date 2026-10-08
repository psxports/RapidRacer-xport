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

static void object_write_packet_pair(uint32 first, uint32 second, uint32 offset, uint32 value)
{
    w_u32(first + offset, value);
    w_u32(second + offset, value);
}

static void object_write_packet_word(uint32 first, uint32 second, uint32 offset, uint16 value)
{
    w_u16(first + offset, value);
    w_u16(second + offset, value);
}

static uint32 object_alloc_index_group(uint32 group, uint32 payload_size)
{
    uint32 count = r_u16(group + 2u);
    uint32 entries;
    uint32 index;

    uint32 indices = game_alloc_arena_bytes((sint32)(count * 4u));
    count = r_u16(group + 2u);
    w_u32(group + 4u, indices);
    entries = game_alloc_arena_bytes((sint32)(count * 8u));
    count = r_u16(group + 2u);
    w_u32(group + 8u, entries);
    if (count == 0u)
        return entries;
    index = 0u;
    do
    {
        uint32 entry = entries + index * 8u;
        uint32 payload = game_alloc_arena_bytes((sint32)payload_size);

        ++index;
        w_u32(entry + 4u, payload);
        w_u16(entry + 2u, 0u);
        w_u16(entry, 0xFFu);
        count = r_u16(group + 2u);
    } while (index < count);
    return 0u;
}

sint32 object_node_flags_update_linked(uint32 unused)
{
    uint32 table;
    uint32 node;
    sint32 index;

    FUNCTION_MARKER(0x80015370u, "MAIN.EXE");
    if (r_u32(0x800834A0u) == 2u && r_u32(0x8008348Cu) != 1u)
    {
        table = r_u32(0x800B6B80u);
        node = r_u32(table);
        node = r_u32(table + 4u * r_u16(node + 2u));
        node = r_u32(table + 4u * r_u16(node + 2u));
        for (index = 0; index < 9; ++index)
        {
            sint32 item;
            for (item = 0; item < r_u8(node + 7u); ++item)
            {
                uint32 flags = node + 33u + 14u * (uint32)item;
                uint8 value = r_u8(flags);

                if ((value >> 5) != 0u)
                {
                    value = r_u8(flags);
                    w_u8(flags, value & 0x1Fu);
                }
            }
            {
                uint16 next = r_u16(node);

                table = r_u32(0x800B6B80u);
                node = r_u32(table + 4u * next);
            }
        }
    }
    table = r_u32(0x800B6B80u);
    {
        sint32 count = r_s32(0x800B6A98u);

        node = r_u32(table);
        if (count > 0)
        {
            index = 0;
            do
            {
                sint32 item;

                for (item = 0; item < r_u8(node + 7u); ++item)
                {
                    uint32 entry = node + 33u + 14u * (uint32)item;
                    uint8 flags = r_u8(entry);

                    w_u16(entry + 7u, 0xFF00u);
                    if ((flags >> 5) >= 7u)
                    {
                        flags = r_u8(entry);
                        w_u8(entry, flags & 0x1Fu);
                    }
                }
                ++index;
                {
                    uint16 next = r_u16(node);
                    uint32 next_address;

                    table = r_u32(0x800B6B80u);
                    next_address = table + 4u * next;
                    count = r_s32(0x800B6A98u);
                    node = r_u32(next_address);
                }
            } while (index < count);
        }
    }
    {
        uint32 descriptors = r_u32(0x8008349Cu);
        uint32 cursor_base = r_u32(0x8008349Cu);
        sint32 count = (sint16)r_u16(descriptors + 30u);
        uint32 cursor = cursor_base + 36u;

        index = 0;
        if (count > 0)
        {
            do
            {
                sint32 node_index = (sint16)r_u16(cursor + 16u);

                if (node_index < r_s32(0x800B6A98u))
                {
                    sint32 slot;
                    uint32 entry;

                    table = r_u32(0x800B6B80u);
                    node = r_u32(table + 4u * (uint32)node_index);
                    entry = node + 14u * (r_u8(node + 14u) >> 4);
                    for (slot = 0; slot < 14; ++slot)
                    {
                        uint8 flags = r_u8(node + 16u + (uint32)slot);

                        if ((flags & 0x0Cu) == 0x0Cu)
                            break;
                        if ((flags & 3u) == 0u)
                        {
                            w_u8(entry + 33u, (uint8)((r_u8(entry + 33u) & 0x1Fu) | 0x20u));
                            entry += 14u;
                            w_u8(entry + 33u, (uint8)((r_u8(entry + 33u) & 0x1Fu) | 0x20u));
                        }
                        else if ((flags & 3u) != 2u)
                            entry += 14u;
                    }
                }
                descriptors = r_u32(0x8008349Cu);
                ++index;
                count = (sint16)r_u16(descriptors + 30u);
                cursor += 20u;
            } while (index < count);
        }
    }
    if ((sint16)r_u16(0x800E0582u) == 6 && (sint16)r_u16(0x800E0584u) == 1)
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
    if ((sint16)r_u16(0x800E0582u) != 0)
        scene_clear_group_item_flags(3);
    else
        profile_cell_dispatch_complete();
    return scene_clear_group_item_flags(6);
}

sint32 object_alloc_index_groups(uint32 first, uint32 second, sint32 mode)
{
    uint32 payload_size = r_u16(r_u32(0x80083498u) + 2u);

    FUNCTION_MARKER(0x8001DFCCu, "MAIN.EXE");
    w_u16(first + 2u, 16u);
    w_u16(second + 2u, mode == 1 ? 0u : 16u);
    object_alloc_index_group(first, payload_size);
    if (mode == 2)
        return (sint32)object_alloc_index_group(second, payload_size);
    return 2;
}

sint32 render_reconcile_slots(uint32 state, uint8 slot)
{
    uint32 index;
    uint32 count;
    uint32 items;
    uint32 slots;

    FUNCTION_MARKER_ARGS(0x8001E13Cu, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 2u, XPORT_CALL_GUEST_POINTER(state, 12u), XPORT_CALL_SCALAR(slot));
    count = r_u16(state + 2u);
    slots = r_u32(state + 8u);
    if (count != 0u)
    {
        index = 0u;
        do
        {
            uint32 status = slots + index * 8u + 2u;
            uint16 value = r_u16(status);

            ++index;
            if (value != 0u)
                w_u16(status, (uint16)(r_u16(status) - 1u));
            count = r_u16(state + 2u);
        } while (index < count);
    }

    count = r_u16(state);
    items = r_u32(state + 4u);
    if (count != 0u)
    {
        index = 0u;
        do
        {
            uint32 slot_count = r_u16(state + 2u);
            uint32 slot_cursor = r_u32(state + 8u);
            uint32 candidate = 0u;

            while (candidate < slot_count)
            {
                uint16 slot_identifier = r_u16(slot_cursor);
                uint16 item_identifier = r_u16(items);

                if (slot_identifier == item_identifier)
                {
                    w_u16(slot_cursor + 2u, 2u);
                    w_u16(items + 2u, 1u);
                    break;
                }
                ++candidate;
                slot_count = r_u16(state + 2u);
                slot_cursor += 8u;
            }
            ++index;
            count = r_u16(state);
            items += 4u;
        } while (index < count);
    }

    {
        uint32 slot_index = 0u;
        uint32 slot_cursor = r_u32(state + 8u);
        uint32 item_count = r_u16(state);
        uint32 item_cursor = r_u32(state + 4u);

        if (item_count == 0u)
            return 0;
        index = 0u;
        do
        {
            if (r_u16(item_cursor + 2u) != 1u)
            {
                uint32 slot_count = r_u16(state + 2u);

                if (slot_index >= slot_count)
                    return 0;
                while (r_u16(slot_cursor + 2u) != 0u)
                {
                    ++slot_index;
                    slot_cursor += 8u;
                    slot_count = r_u16(state + 2u);
                    if (slot_index >= slot_count)
                        return 0;
                }
                {
                    uint16 identifier;
                    uint32 definitions;
                    uint32 payload;

                    ++slot_index;
                    identifier = r_u16(item_cursor);
                    definitions = r_u32(0x80083498u);
                    payload = r_u32(slot_cursor + 4u);
                    object_linked_lists_expand2(definitions + 4u + ((uint32)identifier - 1u) * 32u, payload, slot);
                    w_u16(item_cursor + 2u, 1u);
                    w_u16(slot_cursor + 2u, 2u);
                    identifier = r_u16(item_cursor);
                    w_u16(slot_cursor, identifier);
                    slot_cursor += 8u;
                }
            }
            ++index;
            item_count = r_u16(state);
            item_cursor += 4u;
        } while (index < item_count);
    }
    return 0;
}

sint32 object_init_racer_object_groups(uint32 first, uint32 second, sint32 mode)
{
    uint32 first_group = first + 3740u;
    uint32 second_group = second + 3740u;

    FUNCTION_MARKER(0x8001E344u, "MAIN.EXE");
    object_alloc_index_groups(first_group, second_group, mode);
    object_groups_build_linked(vehicle_player(first)->contacts.points[0].object, first_group, camera_for_view(first));
    render_reconcile_slots(first_group, (uint8)r_u32(first + 4u));
    if (mode == 2)
    {
        object_groups_build_linked(vehicle_player(second)->contacts.points[0].object, second_group, camera_for_view(second));
        return render_reconcile_slots(second_group, (uint8)r_u32(second + 4u));
    }
    return 2;
}

sint32 object_groups_build_linked(uint32 object, uint32 output, const CAMERA_STATE *configuration)
{
    uint32 record = r_u32(output + 4u);
    sint32 first_count = (sint16)(uint16)configuration->visible[0];
    sint32 second_count = (sint16)(uint16)configuration->visible[1];
    sint32 third_count = (sint16)(uint16)configuration->visible[2];
    sint32 total;
    sint16 direction;
    uint32 table;
    uint32 current;
    sint32 index;

    FUNCTION_MARKER(0x8001E3F0u, "MAIN.EXE");
    w_u16(output, 1u);
    total = first_count + second_count + third_count;
    w_u16(record, r_u8(object + 15u));
    w_u16(record + 2u, 0u);
    direction = (sint16)(uint16)configuration->direction;
    current = r_u16(object + (direction == 1 ? 0u : 2u));
    table = r_u32(0x800B6B80u);
    current = r_u32(table + current * 4u);
    if (total <= 1)
        return 0;
    for (index = 1; index < total; ++index)
    {
        if (r_u8(current + 15u) != r_u16(record))
        {
            uint16 identifier;

            w_u16(output, (uint16)(r_u16(output) + 1u));
            identifier = r_u8(current + 15u);
            record += 4u;
            w_u16(record + 2u, 0u);
            w_u16(record, identifier);
            {
                uint16 count = r_u16(output);
                uint16 capacity = r_u16(output + 2u);

                if (count == capacity)
                    return capacity;
            }
        }
        current = r_u32(table + (uint32)r_u16(current + (direction == 1 ? 0u : 2u)) * 4u);
    }
    return 0;
}

uint32 object_linked_lists_expand2(uint32 structure, uint32 output, uint8 slot)
{
    FUNCTION_MARKER_ARGS(0x8001E564u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 3u, XPORT_CALL_GUEST_POINTER(structure, 24u), XPORT_CALL_GUEST_POINTER(output, 96u), XPORT_CALL_SCALAR(slot));
    output = object_list_expand_linked(r_u32(structure + 16u), output, slot);
    return object_list_expand_linked(r_u32(structure + 20u), output, slot);
}

uint32 object_list_expand_linked(uint32 list, uint32 output, uint8 slot)
{
    FUNCTION_MARKER_ARGS(0x8001E5B0u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 3u, XPORT_CALL_GUEST_POINTER(list, 8u), XPORT_CALL_GUEST_POINTER(output, 96u), XPORT_CALL_SCALAR(slot));
    while (list != 0u)
    {
        output = object_build_prim_group_packets(r_u32(list), output, slot);
        list = r_u32(list + 4u);
    }
    return output;
}

uint32 object_build_prim_group_packets(uint32 source, uint32 output, uint8 slot)
{
    uint32 cursor = output + 96u;
    uint32 count;
    uint32 loop_count;
    uint32 first;
    uint32 second;
    uint32 vertices;
    uint32 tags;
    uint32 index;

    FUNCTION_MARKER_ARGS(0x8001E608u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 3u, XPORT_CALL_GUEST_POINTER(source, 96u), XPORT_CALL_GUEST_POINTER(output, 96u), XPORT_CALL_SCALAR(slot));
    w_u32(source + (uint32)slot * 4u, output);

    count = r_u32(source + 8u);
    w_u32(output, count);
    loop_count = r_u32(source + 8u);
    if (loop_count != 0u)
    {
        first = cursor;
        second = first + count * 20u;
        cursor = second + count * 20u;
        tags = r_u32(source + 16u);
        vertices = r_u32(source + 12u);
        w_u32(output + 8u, first);
        w_u32(output + 4u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 4u);
            uint32 header = object_packet_header(raw, 0x20000000u, 0x22000000u);

            w_u32(first + index * 20u + 4u, header);
            w_u32(second + index * 20u + 4u, header);
            w_u16(vertices + index * 24u + 6u, (uint16)((raw >> 24) & 0x7Fu));
        }
    }

    count = r_u32(source + 20u);
    w_u32(output + 12u, count);
    loop_count = r_u32(source + 20u);
    if (loop_count != 0u)
    {
        first = cursor;
        second = first + count * 24u;
        cursor = second + count * 24u;
        tags = r_u32(source + 28u);
        vertices = r_u32(source + 24u);
        w_u32(output + 20u, first);
        w_u32(output + 16u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 4u);
            uint32 header = object_packet_header(raw, 0x28000000u, 0x2A000000u);

            w_u32(first + index * 24u + 4u, header);
            w_u32(second + index * 24u + 4u, header);
            w_u16(vertices + index * 32u + 6u, (uint16)((raw >> 24) & 0x7Fu));
        }
    }

    count = r_u32(source + 32u);
    count += r_u32(source + 36u);
    w_u32(output + 24u, count);
    if (count != 0u)
    {
        uint32 first_count;
        uint32 second_count;

        first = cursor;
        second = first + count * 32u;
        cursor = second + count * 32u;
        first_count = r_u32(source + 32u);
        tags = r_u32(source + 44u);
        vertices = r_u32(source + 40u);
        w_u32(output + 32u, first);
        w_u32(output + 28u, vertices);
        for (index = 0u; index < first_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 8u);
            uint32 packet0 = first + index * 32u;
            uint32 packet1 = second + index * 32u;

            object_write_packet_word(packet0, packet1, 12u, r_u16(vertices + index * 24u + 6u));
            object_write_packet_word(packet0, packet1, 20u, r_u16(vertices + index * 24u + 14u));
            object_write_packet_word(packet0, packet1, 28u, r_u16(vertices + index * 24u + 22u));
            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x24000000u, 0x26000000u));
            object_write_packet_word(packet0, packet1, 30u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tags + index * 8u + 4u));
            object_write_packet_word(packet0, packet1, 22u, r_u16(tags + index * 8u + 6u));
            w_u8(packet0 + 3u, 7u);
            w_u8(packet1 + 3u, 7u);
        }
        second_count = r_u32(source + 36u);
        tags = r_u32(source + 48u);
        for (index = 0u; index < second_count; ++index)
        {
            uint32 packet_index = first_count + index;
            uint32 packet0 = first + packet_index * 32u;
            uint32 packet1 = second + packet_index * 32u;
            uint32 tag = tags + index * 8u;
            uint8 tag_byte = r_u8(tag + 3u);

            object_write_packet_word(packet0, packet1, 12u, r_u16(vertices + packet_index * 24u + 6u));
            object_write_packet_word(packet0, packet1, 20u, r_u16(vertices + packet_index * 24u + 14u));
            object_write_packet_word(packet0, packet1, 28u, r_u16(vertices + packet_index * 24u + 22u));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tag));
            object_write_packet_word(packet0, packet1, 22u, r_u16(tag + 4u));
            object_write_packet_pair(packet0, packet1, 4u, (tag_byte & 0x80u) != 0u ? 0x27000000u : 0x25000000u);
            object_write_packet_word(packet0, packet1, 30u, tag_byte);
            w_u8(packet0 + 3u, 7u);
            w_u8(packet1 + 3u, 7u);
        }
    }

    count = r_u32(source + 52u);
    count += r_u32(source + 56u);
    w_u32(output + 36u, count);
    if (count != 0u)
    {
        uint32 first_count;
        uint32 second_count;

        first = cursor;
        second = first + count * 40u;
        cursor = second + count * 40u;
        first_count = r_u32(source + 52u);
        tags = r_u32(source + 64u);
        vertices = r_u32(source + 60u);
        w_u32(output + 44u, first);
        w_u32(output + 40u, vertices);
        for (index = 0u; index < first_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 8u);
            uint32 packet0 = first + index * 40u;
            uint32 packet1 = second + index * 40u;
            uint32 vertex = vertices + index * 32u;

            object_write_packet_word(packet0, packet1, 12u, r_u16(vertex + 6u));
            object_write_packet_word(packet0, packet1, 20u, r_u16(vertex + 14u));
            object_write_packet_word(packet0, packet1, 28u, r_u16(vertex + 22u));
            object_write_packet_word(packet0, packet1, 36u, r_u16(vertex + 30u));
            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x2C000000u, 0x2E000000u));
            object_write_packet_word(packet0, packet1, 30u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tags + index * 8u + 4u));
            object_write_packet_word(packet0, packet1, 22u, r_u16(tags + index * 8u + 6u));
            w_u8(packet0 + 3u, 9u);
            w_u8(packet1 + 3u, 9u);
        }
        second_count = r_u32(source + 56u);
        tags = r_u32(source + 68u);
        for (index = 0u; index < second_count; ++index)
        {
            uint32 packet_index = first_count + index;
            uint32 packet0 = first + packet_index * 40u;
            uint32 packet1 = second + packet_index * 40u;
            uint32 vertex = vertices + packet_index * 32u;
            uint32 tag = tags + index * 8u;
            uint8 tag_byte = r_u8(tag + 3u);

            object_write_packet_word(packet0, packet1, 12u, r_u16(vertex + 6u));
            object_write_packet_word(packet0, packet1, 20u, r_u16(vertex + 14u));
            object_write_packet_word(packet0, packet1, 28u, r_u16(vertex + 22u));
            object_write_packet_word(packet0, packet1, 36u, r_u16(vertex + 30u));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tag));
            object_write_packet_word(packet0, packet1, 22u, r_u16(tag + 4u));
            object_write_packet_pair(packet0, packet1, 4u, (tag_byte & 0x80u) != 0u ? 0x2F000000u : 0x2D000000u);
            object_write_packet_word(packet0, packet1, 30u, tag_byte);
            w_u8(packet0 + 3u, 9u);
            w_u8(packet1 + 3u, 9u);
        }
    }

    count = r_u32(source + 72u);
    w_u32(output + 48u, count);
    loop_count = r_u32(source + 72u);
    if (loop_count != 0u)
    {
        first = cursor;
        second = first + count * 28u;
        cursor = second + count * 28u;
        tags = r_u32(source + 80u);
        vertices = r_u32(source + 76u);
        w_u32(output + 56u, first);
        w_u32(output + 52u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 12u);
            uint32 packet0 = first + index * 28u;
            uint32 packet1 = second + index * 28u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x30000000u, 0x32000000u));
            w_u16(vertices + index * 24u + 6u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 12u, r_u32(tags + index * 12u + 4u));
            object_write_packet_pair(packet0, packet1, 20u, r_u32(tags + index * 12u + 8u));
            w_u8(packet0 + 3u, 6u);
            w_u8(packet1 + 3u, 6u);
        }
    }

    count = r_u32(source + 84u);
    w_u32(output + 60u, count);
    loop_count = r_u32(source + 84u);
    if (loop_count != 0u)
    {
        first = cursor;
        second = first + count * 36u;
        cursor = second + count * 36u;
        tags = r_u32(source + 92u);
        vertices = r_u32(source + 88u);
        w_u32(output + 68u, first);
        w_u32(output + 64u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 16u);
            uint32 packet0 = first + index * 36u;
            uint32 packet1 = second + index * 36u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x38000000u, 0x3A000000u));
            w_u16(vertices + index * 32u + 6u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 12u, r_u32(tags + index * 16u + 4u));
            object_write_packet_pair(packet0, packet1, 20u, r_u32(tags + index * 16u + 8u));
            object_write_packet_pair(packet0, packet1, 28u, r_u32(tags + index * 16u + 12u));
            w_u8(packet0 + 3u, 8u);
            w_u8(packet1 + 3u, 8u);
        }
    }

    count = r_u32(source + 96u);
    w_u32(output + 72u, count);
    if (count != 0u)
    {
        first = cursor;
        second = first + count * 40u;
        cursor = second + count * 40u;
        loop_count = r_u32(source + 96u);
        tags = r_u32(source + 104u);
        vertices = r_u32(source + 100u);
        w_u32(output + 80u, first);
        w_u32(output + 76u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 16u);
            uint32 packet0 = first + index * 40u;
            uint32 packet1 = second + index * 40u;
            uint32 vertex = vertices + index * 24u;
            uint32 tag = tags + index * 16u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x34000000u, 0x36000000u));
            object_write_packet_word(packet0, packet1, 38u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 16u, r_u32(tag + 4u));
            object_write_packet_pair(packet0, packet1, 28u, r_u32(tag + 8u));
            object_write_packet_word(packet0, packet1, 12u, r_u16(vertex + 6u));
            object_write_packet_word(packet0, packet1, 24u, r_u16(vertex + 14u));
            object_write_packet_word(packet0, packet1, 36u, r_u16(vertex + 22u));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tag + 12u));
            object_write_packet_word(packet0, packet1, 26u, r_u16(tag + 14u));
            w_u8(packet0 + 3u, 9u);
            w_u8(packet1 + 3u, 9u);
        }
    }

    count = r_u32(source + 108u);
    w_u32(output + 84u, count);
    if (count != 0u)
    {
        first = cursor;
        second = first + count * 52u;
        cursor = second + count * 52u;
        loop_count = r_u32(source + 108u);
        tags = r_u32(source + 116u);
        vertices = r_u32(source + 112u);
        w_u32(output + 92u, first);
        w_u32(output + 88u, vertices);
        for (index = 0u; index < loop_count; ++index)
        {
            uint32 raw = r_u32(tags + index * 20u);
            uint32 packet0 = first + index * 52u;
            uint32 packet1 = second + index * 52u;
            uint32 vertex = vertices + index * 32u;
            uint32 tag = tags + index * 20u;

            object_write_packet_pair(packet0, packet1, 4u, object_packet_header(raw, 0x3C000000u, 0x3E000000u));
            object_write_packet_word(packet0, packet1, 38u, (uint16)((raw >> 24) & 0x7Fu));
            object_write_packet_pair(packet0, packet1, 16u, r_u32(tag + 4u));
            object_write_packet_pair(packet0, packet1, 28u, r_u32(tag + 8u));
            object_write_packet_pair(packet0, packet1, 40u, r_u32(tag + 12u));
            object_write_packet_word(packet0, packet1, 12u, r_u16(vertex + 6u));
            object_write_packet_word(packet0, packet1, 24u, r_u16(vertex + 14u));
            object_write_packet_word(packet0, packet1, 36u, r_u16(vertex + 22u));
            object_write_packet_word(packet0, packet1, 48u, r_u16(vertex + 30u));
            object_write_packet_word(packet0, packet1, 14u, r_u16(tag + 16u));
            object_write_packet_word(packet0, packet1, 26u, r_u16(tag + 18u));
            w_u8(packet0 + 3u, 12u);
            w_u8(packet1 + 3u, 12u);
        }
    }
    return cursor;
}
