#include "camera.h"
#include <string.h>
#include "mesh.h"
#include "records.h"
#include "sound.h"
#include "title.h"
#include "polygon.h"
#include "arena.h"
#include "callbacks.h"
#include "effects.h"
#include "xport_trace.h"

static void effects_rotate_mat_x(sint32 angle, uint32 matrix)
{
    sint32 cosine;
    sint32 sine;
    sint32 m10 = (sint16)r_u16(matrix + 6u);
    sint32 m11 = (sint16)r_u16(matrix + 8u);
    sint32 m12 = (sint16)r_u16(matrix + 10u);
    sint32 m20 = (sint16)r_u16(matrix + 12u);
    sint32 m21 = (sint16)r_u16(matrix + 14u);
    sint32 m22 = (sint16)r_u16(matrix + 16u);

    rsincos(angle, &sine, &cosine);
    w_u16(matrix + 6u, (uint16)((cosine * m10 - sine * m20) >> 12));
    w_u16(matrix + 8u, (uint16)((cosine * m11 - sine * m21) >> 12));
    w_u16(matrix + 10u, (uint16)((cosine * m12 - sine * m22) >> 12));
    w_u16(matrix + 12u, (uint16)((sine * m10 + cosine * m20) >> 12));
    w_u16(matrix + 14u, (uint16)((sine * m11 + cosine * m21) >> 12));
    w_u16(matrix + 16u, (uint16)((sine * m12 + cosine * m22) >> 12));
}

#include "global.h"
#include "motion.h"
#include "render.h"
#include <stdlib.h>

static sint32 guest_fourcc_equal(const uint8 tag[4], uint32 expected)
{
    uint32 index;

    for (index = 0u; index < 5u; ++index)
    {
        uint8 actual = index < 4u ? tag[index] : 0u;
        uint8 wanted = r_u8(expected + index);

        if (actual != wanted)
            return 0;
        if (actual == 0u)
            return 1;
    }
    return 0;
}

static uint32 effects_bswap_u32(uint32 value)
{
    return (value >> 24) | ((value >> 8) & 0x0000FF00u) | ((value << 8) & 0x00FF0000u) | (value << 24);
}

static uint32 effects_sra_two(uint32 value)
{
    return (value >> 2) | ((value & 0x80000000u) != 0u ? 0xC0000000u : 0u);
}

static void effects_emit_circle(uint32 ordering_entry, uint32 *packet, uint32 x, uint32 y, uint32 width)
{
    uint32 tag;

    w_u32(*packet + 4u, 0x62101010u);
    w_u32(*packet + 8u, (x & 0xFFFFu) | (y << 16));
    w_u32(*packet + 12u, width | 0x00010000u);
    tag = r_u32(ordering_entry);
    w_u32(*packet, tag | 0x03000000u);
    w_u32(ordering_entry, *packet);
    *packet += 16u;
}

sint32 effects_advance_tex_cb(uint32 node)
{
    uint32 state = r_u32(node + 12u);
    sint32 countdown = (sint32)r_u32(state + 20u);
    sint32 period;
    sint32 count;
    sint32 index;

    FUNCTION_MARKER(0x80040C6Cu, "MAIN.EXE");
    countdown = (sint32)((uint32)countdown - (r_u32(0x80083478u) == 2u ? 2u : 1u));
    w_u32(state + 20u, (uint32)countdown);
    if (countdown >= 0)
        return countdown;
    period = (sint32)r_u32(state + 12u);
    countdown = (sint32)((uint32)countdown + (uint32)period);
    w_u32(state + 20u, (uint32)countdown);
    index = (sint32)r_u32(state + 16u);
    count = (sint32)r_u32(state + 8u);
    while (countdown < 0)
    {
        index = math_rem_s32((sint32)((uint32)index + 1u), count);
        w_u32(state + 16u, (uint32)index);
        countdown = (sint32)((uint32)countdown + (uint32)period);
        w_u32(state + 20u, (uint32)countdown);
    }
    {
        uint32 source = r_u32(state) + (uint32)index * 8u;
        PSX_RECT rectangle;

        rectangle.x = (sint16)r_u16(source);
        rectangle.y = (sint16)r_u16(source + 2u);
        rectangle.w = (sint16)r_u16(source + 4u);
        rectangle.h = (sint16)r_u16(source + 6u);
        MoveImage(&rectangle, (sint16)r_u16(state + 4u), (sint16)r_u16(state + 6u));
    }
    index = math_rem_s32((sint32)((uint32)index + 1u), count);
    w_u32(state + 16u, (uint32)index);
    return index;
}

uint32 effects_register_cb(uint32 argument)
{
    uint32 node;

    FUNCTION_MARKER(0x80040DA0u, "MAIN.EXE");
    node = cb_alloc_node(0x80040C6Cu);
    w_u32(node + 12u, argument);
    return node;
}

sint32 effects_update_transform_cb(uint32 node)
{
    uint32 state = r_u32(node + 12u);
    sint32 counter = (sint32)r_u32(state + 148u);
    sint32 frame_count = (sint32)r_u32(0x800B69DCu);
    sint32 remaining;
    sint32 value;
    sint32 half;

    FUNCTION_MARKER(0x80040DD8u, "MAIN.EXE");
    counter = (sint32)((uint32)counter + 1u);
    w_u32(state + 148u, (uint32)counter);
    if (counter == frame_count)
    {
        remaining = (sint32)r_u32(state + 144u);
        w_u16(state, 4096u);
        w_u16(state + 8u, 4096u);
        w_u16(state + 16u, 4096u);
        remaining = (sint32)((uint32)remaining - 1u);
        w_u32(state + 144u, (uint32)remaining);
        remaining = (sint32)r_u32(state + 144u);
        w_u16(state + 2u, 0u);
        w_u16(state + 4u, 0u);
        w_u16(state + 10u, 0u);
        w_u16(state + 6u, 0u);
        w_u16(state + 12u, 0u);
        w_u16(state + 14u, 0u);
        w_u32(state + 20u, 0u);
        w_u32(state + 24u, 0u);
        w_u32(state + 28u, 0u);
        w_u32(state + 148u, 0u);
        if (remaining == -1)
            return (sint32)cb_unlink_active_node(node);
        sound_queue_command(0x800DE0F0u, 11, 0, 0u);
    }
    remaining = (sint32)r_u32(state + 144u);
    counter = (sint32)r_u32(state + 148u);
    frame_count = (sint32)r_u32(0x800B69DCu);
    if (remaining != 0)
        value = (sint32)((uint32)math_div_s32((sint32)(0u - (uint32)counter * 800u), frame_count) + 1000u);
    else
        value = (sint32)((uint32)math_div_s32((sint32)((uint32)counter * 800u), frame_count) + 200u);
    w_u32(state + 28u, (uint32)value);
    value = (sint32)r_u32(state + 28u);
    w_u32(state + 152u, (uint32)math_div_s32((sint32)(200u - (uint32)value), 4));
    frame_count = (sint32)r_u32(0x800B69DCu);
    half = frame_count / 2;
    counter = (sint32)r_u32(state + 148u);
    if (half >= counter)
        return 0;
    effects_rotate_mat_x(math_div_s32(1024, half), state);
    return (sint32)state;
}

uint32 sprite_packets_build2(uint32 ordering_entry)
{
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 tag;
    uint32 head;
    uint32 result;

    FUNCTION_MARKER(0x80040FCCu, "MAIN.EXE");
    w_u16(packet + 8u, 224u);
    w_u16(packet + 10u, 32u);
    w_u16(packet + 16u, 128u);
    w_u16(packet + 18u, 66u);
    w_u8(packet + 12u, 0u);
    w_u8(packet + 13u, 0u);
    w_u16(packet + 14u, 21164u);
    w_u32(packet + 4u, 0x65000000u);
    w_u32(packet, r_u32(ordering_entry) | 0x04000000u);
    w_u32(ordering_entry, packet);
    packet += 20u;
    w_u16(packet + 8u, 288u);
    w_u16(packet + 10u, 400u);
    w_u16(packet + 16u, 64u);
    w_u16(packet + 18u, 80u);
    w_u8(packet + 12u, 128u);
    w_u8(packet + 13u, 0u);
    w_u16(packet + 14u, 21228u);
    w_u32(packet + 4u, 0x65000000u);
    w_u32(packet, r_u32(ordering_entry) | 0x04000000u);
    w_u32(ordering_entry, packet);
    packet += 20u;
    SetDrawMode(psx_addr(packet, sizeof(DR_MODE)), 1, 0, 0x1Bu, NULL);
    tag = r_u32(packet) & 0xFF000000u;
    head = r_u32(ordering_entry) & 0x00FFFFFFu;
    w_u32(packet, tag | head);
    head = r_u32(ordering_entry);
    w_u32(0x800B6C00u, packet + 12u);
    result = (head & 0xFF000000u) | (packet & 0x00FFFFFFu);
    w_u32(ordering_entry, result);
    return result;
}

sint32 effects_fn_80041104(uint32 ordering_table)
{
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 dimensions = r_u32(0x800B3D88u) == r_u32(0x800B69DCu) ? 0x800B4000u : 0x800B4008u;
    sint16 half_width = (sint16)r_u16(dimensions);
    sint16 half_height = (sint16)r_u16(dimensions + 2u);
    sint32 index;

    FUNCTION_MARKER(0x80041104u, "MAIN.EXE");
    if (r_u32(0x8008348Cu) == 0u)
    {
        w_u16(0x800B3FF0u, (uint16) - (sint16)r_u16(0x800B3FF0u));
        w_u16(0x800B3FF8u, (uint16) - (sint16)r_u16(0x800B3FF8u));
    }
    for (index = 0; index < 2; ++index)
    {
        uint32 point = 0x800B3FF0u + (uint32)index * 8u;
        sint16 x = (sint16)r_u16(point);
        sint16 y = (sint16)r_u16(point + 2u);
        uint32 bucket = ordering_table;
        w_u32(packet, r_u32(bucket) | 0x09000000u);
        w_u32(packet + 4u, 0x2E808080u);
        w_u16(packet + 8u, (uint16)(x - half_width));
        w_u16(packet + 10u, (uint16)(y - half_height));
        w_u32(packet + 12u, r_u32(0x80093574u));
        w_u16(packet + 16u, (uint16)(x + half_width));
        w_u16(packet + 18u, (uint16)(y - half_height));
        w_u32(packet + 20u, r_u32(0x80093578u));
        w_u16(packet + 24u, (uint16)(x - half_width));
        w_u16(packet + 26u, (uint16)(y + half_height));
        w_u32(packet + 28u, r_u32(0x8009357Cu));
        w_u16(packet + 32u, (uint16)(x + half_width));
        w_u16(packet + 34u, (uint16)(y + half_height));
        w_u32(packet + 36u, r_u32(0x80093580u));
        w_u32(bucket, packet);
        packet += 40u;
    }
    w_u32(0x800B6C00u, packet);
    if (r_u32(0x8008348Cu) == 0u)
    {
        w_u16(0x800B3FF0u, (uint16) - (sint16)r_u16(0x800B3FF0u));
        w_u16(0x800B3FF8u, (uint16) - (sint16)r_u16(0x800B3FF8u));
    }
    return (sint32)r_u32(0x8008348Cu);
}

uint32 effects_init_transform_object(uint32 state)
{
    uint32 block = 0x800CB9E0u + 156u * r_u32(state + 4u);
    uint32 node;
    uint32 result;
    sint32 offset;

    FUNCTION_MARKER(0x800413E4u, "MAIN.EXE");
    if (r_u32(0x800B4010u) != 0u)
    {
        w_u32(block + 128u, 0x800B5874u);
        w_u32(block + 132u, 0x800B53F4u);
        w_u32(block + 136u, 0x800B4C54u);
        w_u32(block + 140u, 0x800B428Cu);
    }
    else
    {
        w_u32(block + 128u, records_ptrs_relocate8(0x800B5874u));
        w_u32(block + 132u, records_ptrs_relocate8(0x800B53F4u));
        w_u32(block + 136u, records_ptrs_relocate8(0x800B4C54u));
        w_u32(block + 140u, records_ptrs_relocate8(0x800B428Cu));
        w_u32(0x800B4010u, 1u);
    }
    w_u32(block + 144u, 3u);
    w_u32(block + 148u, 0u);
    w_u16(block, 4096u);
    w_u16(block + 8u, 4096u);
    w_u16(block + 16u, 4096u);
    w_u16(block + 2u, 0u);
    w_u16(block + 4u, 0u);
    w_u16(block + 10u, 0u);
    w_u16(block + 6u, 0u);
    w_u16(block + 12u, 0u);
    w_u16(block + 14u, 0u);
    w_u32(block + 20u, 0u);
    w_u32(block + 24u, 0u);
    w_u32(block + 28u, 0u);
    w_u32(block + 20u, 0u);
    w_u32(block + 24u, 0u);
    w_u32(block + 28u, 500u);
    w_u16(block + 96u, 4096u);
    w_u16(block + 104u, 4096u);
    w_u16(block + 112u, 4096u);
    w_u16(block + 98u, 0u);
    w_u16(block + 100u, 0u);
    w_u16(block + 106u, 0u);
    w_u16(block + 102u, 0u);
    w_u16(block + 108u, 0u);
    w_u16(block + 110u, 0u);
    w_u32(block + 116u, 0u);
    w_u32(block + 120u, 0u);
    w_u32(block + 124u, 0u);
    offset = -5120;
    if (r_u32(0x80083478u) == 2u)
    {
        (void)r_u32(0x80083488u);
        offset = -1840;
    }
    w_u16(block + 104u, (uint16)offset);
    cb_set_active_head(state);
    node = cb_alloc_node(0x80040DD8u);
    result = cb_set_active_head(0u);
    w_u32(node + 12u, block);
    return result;
}

uint32 effects_render_track_billboard(uint32 state, uint32 ordering_table)
{
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    sint32 double_height = r_u32(0x80083478u) != 2u;
    MATRIX saved;
    MATRIX identity;
    sint32 index;

    FUNCTION_MARKER(0x8004158Cu, "MAIN.EXE");
    ReadRotMatrix(&saved);
    SetLightMatrix(&saved);
    gte_set_back_color_raw(saved.t[0], saved.t[1], saved.t[2]);
    memset(&identity, 0, sizeof(identity));
    identity.m[0][0] = 4096;
    identity.m[1][1] = 4096;
    identity.m[2][2] = 4096;
    SetRotMatrix(&identity);
    SetTransMatrix(&identity);
    for (index = 0; index < 40; ++index)
    {
        uint32 entry = state + (uint32)index * 8u;
        sint16 kind = (sint16)r_u16(entry + 54u);
        if (kind != 0)
        {
            SVECTOR source;
            VECTOR transformed;
            SVECTOR corners[4];
            sint32 screens[4];
            sint32 depths[4];
            sint32 flags;
            sint32 combined_flags;
            sint32 horizontal = (sint16)r_u16(entry + 374u);
            sint32 vertical = horizontal;
            sint32 average;
            uint32 bucket;
            uint32 xy;
            uint32 zp;

            if (double_height != 0)
                vertical = (sint16)(vertical * 2);
            xy = r_u32(state + 48u + (uint32)index * 8u);
            zp = r_u32(state + 52u + (uint32)index * 8u);
            source.vx = (sint16)xy;
            source.vy = (sint16)(xy >> 16);
            source.vz = (sint16)zp;
            source.pad = (sint16)(zp >> 16);
            gte_transform_light(&source, &transformed, &flags);
            if (flags < 0)
                continue;
            corners[0].vx = (sint16)((uint32)transformed.vx - (uint32)horizontal);
            corners[0].vy = (sint16)((uint32)transformed.vy - (uint32)vertical);
            corners[0].vz = (sint16)transformed.vz;
            corners[0].pad = 0;
            corners[1].vx = (sint16)((uint32)transformed.vx + (uint32)horizontal);
            corners[1].vy = corners[0].vy;
            corners[1].vz = corners[0].vz;
            corners[1].pad = 0;
            corners[2].vx = corners[0].vx;
            corners[2].vy = (sint16)transformed.vy;
            corners[2].vz = corners[0].vz;
            corners[2].pad = 0;
            corners[3].vx = corners[1].vx;
            corners[3].vy = corners[2].vy;
            corners[3].vz = corners[0].vz;
            corners[3].pad = 0;
            combined_flags = 0;
            depths[0] = gte_project_full_depth(&corners[0], &screens[0], &flags);
            combined_flags |= flags;
            depths[1] = gte_project_full_depth(&corners[1], &screens[1], &flags);
            combined_flags |= flags;
            depths[2] = gte_project_full_depth(&corners[2], &screens[2], &flags);
            combined_flags |= flags;
            if (combined_flags < 0)
                continue;
            w_u32(packet + 8u, (uint32)screens[0]);
            w_u32(packet + 16u, (uint32)screens[1]);
            w_u32(packet + 24u, (uint32)screens[2]);
            (void)NormalClip(screens[0], screens[1], screens[2]);
            average = AverageZ3(depths[0], depths[1], depths[2]);
            average = (sint32)((uint32)average - 1u);
            if ((uint32)average >= 3200u)
                continue;
            (void)gte_project_full_depth(&corners[3], &screens[3], &flags);
            if (flags < 0)
                continue;
            bucket = ordering_table + ((uint32)average >> 5) * 4u;
            w_u32(packet + 32u, (uint32)screens[3]);
            w_u32(packet + 4u, 0x2C808080u);
            w_u32(packet + 12u, r_u32(state));
            w_u32(packet + 20u, r_u32(state + 4u));
            w_u16(packet + 28u, r_u16(state + 8u));
            w_u16(packet + 36u, r_u16(state + 12u));
            if (kind == 2)
                w_u16(packet + 14u, r_u16(state + 10u));
            else if (kind == 3)
                w_u16(packet + 14u, r_u16(state + 14u));
            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
    }
    SetRotMatrix(&saved);
    SetTransMatrix(&saved);
    w_u32(0x800B6C00u, packet);
    return packet;
}

sint32 effects_update_particles(uint32 node)
{
    uint32 state = r_u32(node + 12u);
    sint32 timer = (sint32)(r_u32(state + 700u) - 1u);
    uint32 index;

    FUNCTION_MARKER(0x8004199Cu, "MAIN.EXE");
    w_u32(state + 700u, (uint32)timer);
    if (timer < 0)
    {
        for (index = 0u; index < 40u && (sint16)r_u16(state + index * 8u + 54u) != 0; ++index)
        {
        }
        if (index == 40u)
            w_u32(state + 700u, 0u);
        else
        {
            uint32 offset = index * 8u;
            sint32 random = global_fn_8006e9d8();
            sint32 factor;
            sint32 distance;
            sint32 scale;
            SVECTOR input;
            SVECTOR far_color;
            VECTOR output;

            w_u16(state + offset + 54u, (uint16)(math_rem_s32(random, 3) + 1));
            random = global_fn_8006e9d8();
            w_u16(state + offset + 374u, (uint16)(math_rem_s32(random, 50) + 50));
            factor = global_fn_8006e9d8() & 0x0FFF;
            far_color.vx = (sint16)r_u16(state + 16u);
            far_color.vy = (sint16)r_u16(state + 18u);
            far_color.vz = (sint16)r_u16(state + 20u);
            input.vx = (sint16)r_u16(state + 24u);
            input.vy = (sint16)r_u16(state + 26u);
            input.vz = (sint16)r_u16(state + 28u);
            gte_intpl12(&input, &far_color, factor, &output);
            w_u16(state + offset + 48u, (uint16)output.vx);
            w_u16(state + offset + 50u, (uint16)output.vy);
            w_u16(state + offset + 52u, (uint16)output.vz);
            distance = 2048 - factor;
            factor = 2048 - distance / 2;
            far_color.vx = (sint16)r_u16(state + 32u);
            far_color.vy = (sint16)r_u16(state + 34u);
            far_color.vz = (sint16)r_u16(state + 36u);
            input.vx = (sint16)r_u16(state + 40u);
            input.vy = (sint16)r_u16(state + 42u);
            input.vz = (sint16)r_u16(state + 44u);
            gte_intpl12(&input, &far_color, factor, &output);
            if (distance < 0)
                distance = -distance;
            scale = math_sra_s32((2048u - (uint32)(distance / 2)) * r_u32(state + 692u), 11u);
            random = global_fn_8006e9d8();
            factor = math_rem_s32(random, scale) + scale + (sint32)r_u32(state + 692u);
            input.vx = (sint16)output.vx;
            input.vy = (sint16)output.vy;
            input.vz = (sint16)output.vz;
            gte_gpf12(&input, factor, &output);
            w_u16(state + offset + 368u, (uint16)output.vx);
            w_u16(state + offset + 370u, (uint16)output.vy);
            w_u16(state + offset + 372u, (uint16)output.vz);
            w_u32(state + 700u, r_u32(state + 696u));
        }
    }
    for (index = 0u; index < 40u; ++index)
    {
        uint32 particle = state + index * 8u;

        if ((sint16)r_u16(particle + 54u) != 0)
        {
            uint16 x = r_u16(particle + 48u);
            uint16 velocity_x = r_u16(particle + 368u);
            uint16 velocity_y = r_u16(particle + 370u);
            uint16 velocity_z = r_u16(particle + 372u);
            uint16 y;
            uint16 z;
            sint16 updated_y;

            w_u16(particle + 48u, (uint16)(x + velocity_x));
            y = r_u16(particle + 50u);
            z = r_u16(particle + 52u);
            w_u16(particle + 50u, (uint16)(y + velocity_y));
            w_u16(particle + 52u, (uint16)(z + velocity_z));
            velocity_y = r_u16(particle + 370u);
            updated_y = (sint16)r_u16(particle + 50u);
            w_u16(particle + 370u, (uint16)(velocity_y - 2u));
            if ((sint32)r_u32(state + 688u) >= updated_y)
                w_u16(particle + 54u, 0u);
        }
    }
    return 0;
}

sint32 effects_fn_80041c8c(void)
{
    uint32 node;
    sint32 index;

    FUNCTION_MARKER(0x80041C8Cu, "MAIN.EXE");
    node = cb_alloc_node(0x8004199Cu);
    w_u32(node + 12u, 0x800E0240u);
    w_u32(0x800E0250u, r_u32(0x800B4014u));
    w_u32(0x800E0254u, r_u32(0x800B4018u));
    w_u32(0x800E0258u, r_u32(0x800B4024u));
    w_u32(0x800E025Cu, r_u32(0x800B4028u));
    w_u16(0x800E0260u, r_u16(0x800B401Cu) - r_u16(0x800B4014u));
    w_u16(0x800E0262u, r_u16(0x800B401Eu) - r_u16(0x800B4016u));
    w_u16(0x800E0264u, r_u16(0x800B4020u) - r_u16(0x800B4018u));
    w_u16(0x800E0268u, r_u16(0x800B402Cu) - r_u16(0x800B4024u));
    w_u16(0x800E026Au, r_u16(0x800B402Eu) - r_u16(0x800B4026u));
    w_u16(0x800E026Cu, r_u16(0x800B4030u) - r_u16(0x800B4028u));
    w_u32(0x800E04F8u, 5u);
    w_u32(0x800E04F4u, 50u);
    w_u32(0x800E04FCu, 0u);
    w_u32(0x800E04F0u, 1304u);
    for (index = 0; index < 40; ++index)
        w_u16(0x800E0398u + (uint32)index * 8u + 54u, 0u);
    w_u8(0x800E0241u, 32u);
    w_u8(0x800E0245u, 32u);
    w_u8(0x800E0249u, 63u);
    w_u8(0x800E024Du, 63u);
    w_u8(0x800E0246u, 46u);
    w_u16(0x800E0242u, 9596u);
    w_u16(0x800E024Au, 9340u);
    w_u8(0x800E0240u, (uint8)-64);
    w_u8(0x800E0244u, (uint8)-33);
    w_u8(0x800E0248u, (uint8)-64);
    w_u8(0x800E024Cu, (uint8)-33);
    w_u16(0x800E024Eu, 9404u);
    return 9404;
}

sint32 effects_register_mode(void)
{
    uint32 mode;

    FUNCTION_MARKER(0x80041E30u, "MAIN.EXE");
    mode = r_u32(0x80083484u);
    w_u32(0x800B69CCu, 0u);
    if (mode == 8u)
    {
        effects_register_cb(0x8009332Cu);
        effects_register_cb(0x8009336Cu);
        effects_register_cb(0x800933A4u);
        effects_register_cb(0x800933DCu);
        effects_register_cb(0x80093414u);
    }
    else
    {
        uint32 state = r_u32(0x800834A0u);

        if (state == 0u)
            effects_register_cb(0x8009328Cu);
        state = r_u32(0x800834A0u);
        if (state == 1u)
        {
            if (r_u32(0x8008348Cu) == 2u)
            {
                effects_register_cb(0x8009343Cu);
                effects_register_cb(0x8009347Cu);
                effects_register_cb(0x800934A4u);
                effects_register_cb(0x800934CCu);
                effects_register_cb(0x800934F4u);
                effects_register_cb(0x8009351Cu);
            }
            state = r_u32(0x800834A0u);
        }
        if (state == 4u && r_u32(0x8008348Cu) == 2u)
        {
            effects_register_cb(0x800931F4u);
            effects_fn_80041c8c();
        }
        title_init_cb(0x800DD4A0u);
    }
    w_u32(0x800CBA70u, UINT32_MAX);
    w_u32(0x800CBB0Cu, UINT32_MAX);
    effects_register_cb(0x800932DCu);
    return effects_init_cache();
}

uint32 effects_relocate_anim_ptrs(uint32 structure)
{
    sint32 count = (sint32)r_u32(structure);
    sint32 index;

    FUNCTION_MARKER(0x80041FE0u, "MAIN.EXE");
    w_u32(structure + 12u, structure + 4u * r_u32(structure + 12u));
    w_u32(structure + 16u, structure + 4u * r_u32(structure + 16u));
    w_u32(structure + 20u, structure + 4u * r_u32(structure + 20u));
    for (index = 0; index < count; ++index)
    {
        uint32 slot = structure + (uint32)index * 4u;
        w_u32(slot + 56u, structure + 4u * r_u32(slot + 56u));
    }
    return structure;
}

uint32 effects_load_boat_model_chunk(uint32 archive, sint32 wanted)
{
    uint8 tag[4];
    uint32 cursor;
    uint32 size_bits;
    sint32 remaining;
    sint32 found = 0;

    FUNCTION_MARKER(0x8004205Cu, "MAIN.EXE");
    tag[0] = r_u8(archive);
    tag[1] = r_u8(archive + 1u);
    tag[2] = r_u8(archive + 2u);
    tag[3] = r_u8(archive + 3u);
    cursor = archive + 4u;
    size_bits = effects_bswap_u32(r_u32(cursor));
    cursor += 4u;
    remaining = (sint32)(size_bits - 4u);
    if (!guest_fourcc_equal(tag, 0x800B4034u))
        abort();
    tag[0] = r_u8(cursor);
    tag[1] = r_u8(cursor + 1u);
    tag[2] = r_u8(cursor + 2u);
    tag[3] = r_u8(cursor + 3u);
    cursor += 4u;
    if (!guest_fourcc_equal(tag, 0x800B403Cu))
        abort();
    while (remaining > 0)
    {
        sint32 size;
        uint32 data;

        tag[0] = r_u8(cursor);
        tag[1] = r_u8(cursor + 1u);
        tag[2] = r_u8(cursor + 2u);
        tag[3] = r_u8(cursor + 3u);
        cursor += 4u;
        size_bits = effects_bswap_u32(r_u32(cursor));
        cursor += 4u;
        size = (sint32)size_bits;
        data = cursor;
        if (guest_fourcc_equal(tag, 0x800B4044u) && found++ == wanted)
        {
            uint32 output = r_u32(0x800B68D4u);
            uint32 total = r_u32(0x800B68D8u);

            w_u32(0x800B68D4u, output + size_bits);
            w_u32(0x800B68D8u, total + size_bits);
            if (output != 0u && size > 0)
                xport_guest_copy(xport_guest_ref(output), xport_guest_ref(data), (size_t)size);
            return records_ptrs_relocate8(output);
        }
        remaining = (sint32)((uint32)remaining - ((size_bits + 11u) & 0xFFFFFFFCu));
        size_bits += 3u;
        if ((sint32)size_bits < 0)
            size_bits += 3u;
        cursor = data + (effects_sra_two(size_bits) << 2);
    }
    return 0u;
}

uint32 effects_fn_80042258(uint32 archive, sint32 wanted)
{
    uint32 cursor = archive + 8u;
    sint32 remaining = (sint32)((r_u8(archive + 4u) << 24) | (r_u8(archive + 5u) << 16) | (r_u8(archive + 6u) << 8) | r_u8(archive + 7u)) - 4;
    sint32 found = 0;

    FUNCTION_MARKER(0x80042258u, "MAIN.EXE");
    if (r_u32(archive) != 0x4D524F46u || r_u32(cursor) != 0x5354454Au)
        return 0u;
    cursor += 4u;
    while (remaining > 0)
    {
        uint32 tag = r_u32(cursor);
        sint32 size = (sint32)((r_u8(cursor + 4u) << 24) | (r_u8(cursor + 5u) << 16) | (r_u8(cursor + 6u) << 8) | r_u8(cursor + 7u));
        uint32 data = cursor + 8u;
        if (tag == 0x4D494E41u && found++ == wanted)
        {
            uint32 output = r_u32(0x800B68D4u);
            sint32 offset;
            w_u32(0x800B68D4u, output + (uint32)size);
            xport_update_u32(0x800B68D8u, XPORT_MEMORY_UPDATE_ADD, (uint32)size);
            for (offset = 0; offset < size; ++offset)
                w_u8(output + (uint32)offset, r_u8(data + (uint32)offset));
            return effects_relocate_anim_ptrs(output);
        }
        remaining -= (size + 11) & ~3;
        cursor = data + ((uint32)(size + 3) & ~3u);
    }
    return 0u;
}

sint32 effects_capture_screen_region(uint32 state, sint32 buffer_index)
{
    PSX_RECT rectangle;
    uint32 side = state == 0x800DE0F0u ? 0u : 1u;
    uint32 cache = 0x800CBB18u + side * 8u;
    sint32 x = (sint16)r_u16(cache);
    sint32 y;
    uint32 buffer;
    uint32 pixels = 0x800B68E4u + side * 4u;
    uint32 current_frame;

    FUNCTION_MARKER(0x8004282Cu, "MAIN.EXE");
    if (x < 0 || x > (sint32)r_u32(0x800B3DF8u))
        return 0;
    y = (sint16)r_u16(cache + 2u);
    if (y < 0 || y > (sint32)r_u32(0x800B3DFCu))
        return 0;
    buffer = state + (uint32)buffer_index * 1784u;
    x += (sint16)r_u16(buffer + 112u);
    current_frame = r_u32(0x800B69DCu);
    y = (sint16)r_u16(buffer + 114u) + (uint16)r_u16(cache + 2u);
    if (current_frame == r_u32(0x800B3D88u))
        y = (y | 1) - ((sint32)r_u32(0x800B3FECu) >> 31);
    rectangle.x = (sint16)x;
    rectangle.y = (sint16)y;
    rectangle.w = (sint16)r_u16(cache + 4u);
    rectangle.h = (sint16)r_u16(cache + 6u);
    return StoreImage(&rectangle, (uint32 *)psx_addr(pixels, (size_t)(uint16)rectangle.w * (uint16)rectangle.h * 2u));
}

sint32 effects_init_cache(void)
{
    uint32 vector_words[2];
    uint32 mode;
    sint32 index;
    sint32 upper = 32000;
    sint32 lower = 31744;

    FUNCTION_MARKER(0x80042958u, "MAIN.EXE");
    vector_words[0] = r_u32(0x800B4054u);
    vector_words[1] = r_u32(0x800B4058u);
    w_u16(0x800CBB24u, 2u);
    w_u16(0x800CBB1Cu, 2u);
    w_u16(0x800CBB26u, 1u);
    w_u16(0x800CBB1Eu, 1u);
    mode = r_u32(0x80083484u);
    w_u32(0x800B68ECu, 255u);
    w_u32(0x800B68F0u, 255u);
    w_u16(0x800CBB20u, 0u);
    w_u16(0x800CBB18u, 0u);
    w_u16(0x800CBB22u, 0u);
    w_u16(0x800CBB1Au, 0u);
    w_u32(0x800B68E4u, 0u);
    w_u32(0x800B68E8u, 0u);
    if (mode == 8u)
    {
        w_u16(0x800B68DCu, 1313u);
        w_u16(0x800B68DEu, 1398u);
        w_u16(0x800B68E0u, 3615u);
    }
    else
    {
        MATRIX matrix;
        MATRIX rotation_matrix;
        SVECTOR rotation;
        SVECTOR source;
        VECTOR transformed;
        sint32 angle;
        sint32 value;

        memset(&matrix, 0, sizeof(matrix));
        matrix.m[0][0] = 4096;
        matrix.m[1][1] = 4096;
        matrix.m[2][2] = 4096;
        mode = r_u32(0x800834A0u);
        value = (sint32)r_u32(0x800935E0u + mode * 8u);
        angle = (sint32)((uint32)value << 12) / 360;
        memset(&rotation, 0, sizeof(rotation));
        rotation.vx = (sint16)angle;
        RotMatrix(&rotation, &rotation_matrix);
        MulMatrix2(&rotation_matrix, &matrix);
        mode = r_u32(0x800834A0u);
        value = (sint32)r_u32(0x800935E4u + mode * 8u);
        angle = (sint32)((uint32)value << 12) / 360;
        memset(&rotation, 0, sizeof(rotation));
        rotation.vy = (sint16)angle;
        RotMatrix(&rotation, &rotation_matrix);
        MulMatrix2(&rotation_matrix, &matrix);
        SetRotMatrix(&matrix);
        source.vx = (sint16)(uint16)vector_words[0];
        source.vy = (sint16)(uint16)(vector_words[0] >> 16);
        source.vz = (sint16)(uint16)vector_words[1];
        source.pad = (sint16)(uint16)(vector_words[1] >> 16);
        ApplyRotMatrix(&source, &transformed);
        transformed.vx = transformed.vx < -32768 ? -32768 : transformed.vx > 32767 ? 32767 : transformed.vx;
        transformed.vy = transformed.vy < -32768 ? -32768 : transformed.vy > 32767 ? 32767 : transformed.vy;
        transformed.vz = transformed.vz < -32768 ? -32768 : transformed.vz > 32767 ? 32767 : transformed.vz;
        w_u16(0x800B68DCu, (uint16)transformed.vx);
        w_u16(0x800B68DEu, (uint16)transformed.vy);
        w_u16(0x800B68E0u, (uint16)transformed.vz);
        if (r_u32(0x8008348Cu) == 0u)
            w_u16(0x800B68DCu, (uint16) - (sint16)r_u16(0x800B68DCu));
    }
    for (index = 0; index < 7; ++index)
    {
        uint32 record = 0x800DCFD8u + (uint32)index * 16u;
        w_u16(record + 2u, (uint16)((index >= 5 ? lower : upper) | 0x32));
        w_u16(record + 6u, 60u);
        if (index == 0)
        {
            w_u8(record, 128u);
            w_u8(record + 1u, 64u);
            w_u8(record + 4u, 191u);
            w_u8(record + 5u, 64u);
            w_u8(record + 8u, 128u);
            w_u8(record + 9u, 127u);
            w_u8(record + 12u, 191u);
            w_u8(record + 13u, 127u);
        }
        else
        {
            w_u8(record, 128u);
            w_u8(record + 1u, 128u);
            w_u8(record + 4u, 191u);
            w_u8(record + 5u, 128u);
            w_u8(record + 8u, 128u);
            w_u8(record + 9u, 191u);
            w_u8(record + 12u, 191u);
            w_u8(record + 13u, 191u);
        }
        upper += 64;
        lower += 64;
    }
    return 0;
}

sint32 effects_render_quad_projected_fade(uint32 state, sint32 buffer_index)
{
    SVECTOR source;
    SVECTOR transformed_ir;
    VECTOR transformed;
    VECTOR scaled;
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 side = state == 0x800DE0F0u ? 0u : 1u;
    uint32 buffer = state + (uint32)buffer_index * 1784u;
    uint32 primary = buffer + 1744u;
    uint32 alternate = buffer + 1752u;
    uint32 fade_address = 0x800B68ECu + side * 4u;
    sint32 geometry_x;
    sint32 geometry_y;
    sint32 geometry_h;
    sint32 projection;
    sint32 step;
    sint32 fade;
    sint32 intensity;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80042C34u, "MAIN.EXE");
    memcpy(&source, psx_addr(0x800B68DCu, sizeof(source)), sizeof(source));
    SetRotMatrix((MATRIX *)psx_addr(state + 28u, sizeof(MATRIX)));
    ApplyMatrix((MATRIX *)psx_addr(state + 28u, sizeof(MATRIX)), &source, &transformed);
    transformed_ir.vx = (sint16)(transformed.vx < -32768 ? -32768 : transformed.vx > 32767 ? 32767 : transformed.vx);
    transformed_ir.vy = (sint16)(transformed.vy < -32768 ? -32768 : transformed.vy > 32767 ? 32767 : transformed.vy);
    transformed_ir.vz = (sint16)(transformed.vz < -32768 ? -32768 : transformed.vz > 32767 ? 32767 : transformed.vz);
    transformed_ir.pad = 0;
    geometry_h = ReadGeomScreen();
    ReadGeomOffset(&geometry_x, &geometry_y);
    result = (sint32)transformed_ir.vz - 512;
    if (transformed_ir.vz < 512)
        return result;

    intensity = result >> 5;
    if (intensity > 255)
        intensity = 255;
    step = 20;
    if (r_u32(0x800B3D88u) == r_u32(0x800B69DCu))
        step = 10;
    if ((r_u16(0x800B68E4u + side * 4u) & 0x7FFFu) == 0x7FFFu)
    {
        fade = (sint32)((uint32)r_u32(fade_address) - (uint32)step);
        w_u32(fade_address, (uint32)fade);
        if (fade < 0)
        {
            fade = 0;
            w_u32(fade_address, 0u);
        }
    }
    else
    {
        fade = (sint32)((uint32)r_u32(fade_address) + (uint32)step);
        w_u32(fade_address, (uint32)fade);
        if (fade >= 256)
        {
            fade = 255;
            w_u32(fade_address, 255u);
        }
    }
    intensity = (sint32)((uint32)intensity - r_u32(fade_address));
    if (intensity < 0)
        intensity = 0;

    projection = (sint32)((uint32)geometry_h << 12) / transformed_ir.vz;
    result = 0;
    for (index = 0; index < 7; ++index)
    {
        sint32 doubled = (sint32)(0u - (uint32)projection - (uint32)projection);
        sint32 product = (sint32)((uint32)doubled * r_u32(0x800935C4u + (uint32)index * 4u));
        sint32 factor = (sint32)((uint32)projection + (uint32)(product / 4096));
        sint32 center_x;
        sint32 center_y;
        sint32 width;
        sint32 height;
        uint32 texture = 0x800DCFD8u + (uint32)index * 16u;
        uint32 ordering = index == 0 ? alternate : primary;
        uint32 color = index == 0 ? 0x2F808080u : 0x2E000000u | (uint32)intensity * 0x00010101u;
        uint32 ordering_tag;

        gte_gpf12(&transformed_ir, factor, &scaled);
        center_x = (sint16)((uint16)scaled.vx + (uint16)geometry_x);
        center_y = (sint16)((uint16)scaled.vy + (uint16)geometry_y);
        width = (sint16)r_u16(0x800935A8u + (uint32)index * 4u);
        height = r_u32(0x80083478u) == 2u ? width / 2 : width;
        w_u32(packet + 8u, (uint16)(center_x - width) | ((uint32)(uint16)(center_y - height) << 16));
        w_u32(packet + 16u, (uint16)(center_x + width) | ((uint32)(uint16)(center_y - height) << 16));
        w_u32(packet + 24u, (uint16)(center_x - width) | ((uint32)(uint16)(center_y + height) << 16));
        w_u32(packet + 32u, (uint16)(center_x + width) | ((uint32)(uint16)(center_y + height) << 16));
        w_u32(packet + 12u, r_u32(texture));
        w_u32(packet + 20u, r_u32(texture + 4u));
        w_u16(packet + 28u, r_u16(texture + 8u));
        w_u16(packet + 36u, r_u16(texture + 12u));
        w_u32(packet + 4u, color);
        ordering_tag = r_u32(ordering) | 0x09000000u;
        w_u32(packet, ordering_tag);
        w_u32(ordering, packet);
        packet += 40u;
        if (index == 0)
        {
            w_u16(0x800CBB18u + side * 8u, (uint16)center_x);
            w_u16(0x800CBB1Au + side * 8u, (uint16)center_y);
            result = (uint16)center_y;
        }
        else
            result = (sint32)ordering_tag;
        if (intensity == 0)
            break;
        result = index + 1 < 7;
    }
    w_u32(0x800B6C00u, packet);
    return result;
}

void object_render_anim(uint32 player, sint32 player_index)
{
    static const RR_OBJECT_PACKET_LAYOUT layouts[8] = {{0x18u, 0x14u, 0x04000000u, {0x08u, 0x0Cu, 0x10u}, 0u}, {0x1Cu, 0x18u, 0x05000000u, {0x08u, 0x0Cu, 0x10u}, 0x14u}, {0x20u, 0x20u, 0x07000000u, {0x08u, 0x10u, 0x18u}, 0u}, {0x24u, 0x28u, 0x09000000u, {0x08u, 0x10u, 0x18u}, 0x20u}, {0x28u, 0x1Cu, 0x06000000u, {0x08u, 0x10u, 0x18u}, 0u}, {0x2Cu, 0x24u, 0x08000000u, {0x08u, 0x10u, 0x18u}, 0x20u}, {0x30u, 0x28u, 0x09000000u, {0x08u, 0x14u, 0x20u}, 0u}, {0x34u, 0x34u, 0x0C000000u, {0x08u, 0x14u, 0x20u}, 0x2Cu}};
    uint32 state = player + 1784u * (uint32)player_index + 104u;
    uint32 model;
    uint32 frame;
    uint32 node;

    FUNCTION_MARKER(0x800430C8u, "MAIN.EXE");
    frame = guest_stack_push(0x150u);
    camera_bias(camera_for_view(player), 0x800843D4u);
    if (r_u32(0x800834A0u) == 4u && r_u32(0x8008348Cu) == 2u)
        effects_render_track_billboard(0x800E0240u, state + 0x4CCu);
    if (r_u32(0x80083484u) == 5u)
        sprite_packets_build2(state + 0x664u);
    if (r_u32(0x800834A0u) == 5u)
        effects_fn_80041104(state + 0x4CCu);
    model = 0x800CB9E0u + 156u * r_u32(player + 4u);
    if ((sint32)r_u32(model + 144u) != -1)
    {
        MATRIX combined = {{{0}}};
        MATRIX *first = (MATRIX *)psx_addr(model + 96u, sizeof(MATRIX));
        MATRIX *second = (MATRIX *)psx_addr(model, sizeof(MATRIX));
        uint32 ordering;
        uint32 groups;

        MulMatrix0(first, second, &combined);
        w_u32(0x800B69F0u, r_u32(model + 152u));
        SetRotMatrix(&combined);
        SetTransMatrix(second);
        SetLightMatrix((MATRIX *)psx_addr(0x80093534u, sizeof(MATRIX)));
        SetColorMatrix((MATRIX *)psx_addr(0x80093554u, sizeof(MATRIX)));
        SetBackColor((sint32)r_u32(0x80093568u), (sint32)r_u32(0x8009356Cu), (sint32)r_u32(0x80093570u));
        ordering = mesh_prepare_player_ot_packet(player, player_index);
        groups = r_u32(model + 128u + 4u * r_u32(model + 144u));
        mesh_dispatch_renderers(ordering, 100u, groups);
    }
    node = r_u32(0x800B69CCu);
    while (node != 0u)
    {
        uint32 descriptor = r_u32(node + 0x44u);

        if (descriptor != 0u)
        {
            uint32 projected = frame + 0x10u;
            uint32 indices;
            uint32 primitive;
            uint32 group;

            camera_bias(camera_for_view(player), node);
            gte_set_far_color_raw(0, 0, 300);
            gte_write_ir0((sint32)r_u32(node + 0x54u));
            poly_interp_anim_verts(r_u32(descriptor + 0x38u + 4u * r_u32(node + 0x48u)), r_u32(descriptor + 0x38u + 4u * r_u32(node + 0x4Cu)), projected, (sint32)r_u32(descriptor + 4u));
            indices = r_u32(descriptor + 0x0Cu);
            primitive = r_u32(descriptor + (player_index != 0 ? 0x10u : 0x14u)) & 0x00FFFFFFu;
            for (group = 0; group < 8u; ++group)
                poly_emit_object_packet_group(descriptor, projected, state + 0x660u, &indices, &primitive, &layouts[group]);
        }
        node = r_u32(node + 0x40u);
    }
    guest_stack_pop(0x150u);
}

uint32 effects_emit_filled_circle(uint32 ordering_entry, sint16 center_x, sint32 center_y, sint32 radius)
{
    uint32 width;
    uint32 diameter;
    uint32 x;
    uint32 step = 1u;
    uint32 packet;
    sint32 error;
    uint32 negative;

    FUNCTION_MARKER(0x80043934u, "MAIN.EXE");
    width = (uint32)radius * 2u - 1u;
    x = (uint32)(sint32)center_x - (uint32)radius + 1u;
    diameter = (uint32)radius * 2u + 1u;
    negative = 0u - diameter;
    error = (sint32)(negative + (negative >> 31)) / 2;
    packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
    for (;;)
    {
        sint32 previous_error = error;

        error = (sint32)((uint32)error - diameter);
        if (previous_error < 0)
        {
            error = (sint32)((uint32)error + diameter);
            do
            {
                error = (sint32)((uint32)error + step);
                if (step != 1u)
                    effects_emit_circle(ordering_entry, &packet, x, (uint32)center_y, width);
                center_y = (sint32)((uint32)center_y - 1u + step);
                effects_emit_circle(ordering_entry, &packet, x, (uint32)center_y, width);
                center_y = (sint32)((uint32)center_y - step);
                step += 2u;
            } while (error < 0);
            error = (sint32)((uint32)error - diameter);
        }
        diameter -= 2u;
        width -= 2u;
        ++x;
        if ((sint32)width <= 0)
            break;
    }

    w_u8(packet + 3u, 1u);
    if (GetGraphType() != 1)
        (void)GetGraphType();
    {
        uint32 packet_tag = r_u32(packet);
        uint32 ordering_tag;
        uint32 result;

        w_u32(packet + 4u, 0xE1000040u);
        ordering_tag = r_u32(ordering_entry);
        w_u32(packet, (packet_tag & 0xFF000000u) | (ordering_tag & 0x00FFFFFFu));
        result = packet & 0x00FFFFFFu;
        packet_tag = r_u32(ordering_entry);
        w_u32(0x800B6C00u, packet + 8u);
        result |= packet_tag & 0xFF000000u;
        w_u32(ordering_entry, result);
        return result;
    }
}

sint32 effects_run_expanding_circle_trans(void)
{
    sint32 radius = 10;
    uint32 mode;

    FUNCTION_MARKER(0x80043AD0u, "MAIN.EXE");
    mode = r_u32(0x80083478u);
    if (mode == 1u)
    {
        do
        {
            uint32 buffer;
            uint32 ordering;
            sint32 clamped = radius < 501 ? radius : 500;

            DrawSync(0);
            VSync(0);
            buffer = 1u - r_u32(0x800B3DA8u);
            ordering = 0x800B68C8u + buffer * 4u;
            w_u32(0x800B3DA8u, buffer);
            w_u32(ordering, 0x00FFFFFFu);
            render_select_prim_pool((sint32)buffer);
            spu_decrease_master_volume(0x200);
            effects_emit_filled_circle(ordering, 184, 256, clamped);
            radius += 10;
            buffer = r_u32(0x800B3DA8u);
            render_fn_8006c434(0x800B68C8u + buffer * 4u);
        } while (radius < 600);
    }
    else
    {
        uint32 alternate = 1u - r_u32(0x800B3DA8u);
        uint32 rectangle = 0x800DE848u + alternate * 1784u;

        do
        {
            uint32 buffer;
            uint32 ordering;
            uint32 packet;
            uint32 value;
            sint32 clamped = radius < 501 ? radius : 500;

            DrawSync(0);
            VSync(0);
            buffer = 1u - r_u32(0x800B3DA8u);
            ordering = 0x800B68C8u + buffer * 4u;
            w_u32(0x800B3DA8u, buffer);
            w_u32(ordering, 0x00FFFFFFu);
            render_select_prim_pool((sint32)buffer);
            spu_decrease_master_volume(0x200);
            effects_emit_filled_circle(ordering, 256, 128, clamped);
            radius += 10;

            packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
            SetDrawArea(psx_addr(packet, 12u), (PSX_RECT *)psx_addr(rectangle, sizeof(PSX_RECT)));
            ordering = 0x800B68C8u + r_u32(0x800B3DA8u) * 4u;
            value = r_u32(packet);
            w_u32(packet, (value & 0xFF000000u) | (r_u32(ordering) & 0x00FFFFFFu));
            value = packet & 0x00FFFFFFu;
            packet += 12u;
            w_u32(ordering, (r_u32(ordering) & 0xFF000000u) | value);

            SetDrawOffset(psx_addr(packet, 12u), (const sint16 *)psx_addr(rectangle, 4u));
            value = r_u32(packet);
            w_u32(0x800B6C00u, packet + 12u);
            ordering = 0x800B68C8u + r_u32(0x800B3DA8u) * 4u;
            w_u32(packet, (value & 0xFF000000u) | (r_u32(ordering) & 0x00FFFFFFu));
            value = packet & 0x00FFFFFFu;
            w_u32(ordering, (r_u32(ordering) & 0xFF000000u) | value);
            render_fn_8006c434(ordering);
        } while (radius < 600);
    }
    return 0;
}
