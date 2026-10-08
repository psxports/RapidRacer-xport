#include "vehicle.h"
#include "menu.h"
#include "route.h"
#include "text.h"
#include "polygon.h"
#include "psx_gpu.h"
#include "arena.h"
#include "render.h"
#include "input.h"
#include "global.h"
#include "motion.h"
#include <stdlib.h>
#include <stdio.h>

static BOAT *render_indicator_target;
static sint32 render_indicator_distance;
static uint32 render_indicator_ticks;

static void render_project_horizon(sint16 sine, sint16 cosine, const sint16 heights[3], uint32 projected[3])
{
    SVECTOR vertices[3];
    sint32 screens[3];
    sint32 depths[3];
    sint32 flags;
    uint32 index;

    for (index = 0; index < 3u; ++index)
    {
        vertices[index].vx = sine;
        vertices[index].vy = heights[index];
        vertices[index].vz = cosine;
        vertices[index].pad = 0;
    }
    gte_project3_full_depth(vertices, screens, depths, &flags);
    for (index = 0; index < 3u; ++index)
        projected[index] = (uint32)screens[index];
}

uint32 render_select_tex_desc(uint32 descriptor)
{
    uint32 type = r_u32(descriptor + 0x28u);
    uint32 mode = r_u32(descriptor + 0x14u);
    uint32 output;
    uint32 source;
    uint32 index;
    uint32 palette;

    FUNCTION_MARKER(0x800146ECu, "MAIN.EXE");
    if (type >= 7u)
    {
        input_stop_exit();
        return 0;
    }
    output = r_u32(0x800837F4u + type * 4u);
    w_u32(descriptor + 0x24u, output);
    if (type == 1u)
    {
        if (mode == 2u)
        {
            sint16 value = (sint16)r_u16(output + 0x20u);

            output = 0x80093070u;
            w_u32(descriptor + 0x24u, output);
            w_u16(0x80093090u, (uint16)value);
            w_u32(output + 0x8Cu, r_u32(0x80083848u + type * 4u));
        }
    }
    else
    {
        source = r_u32((mode == 2u ? 0x80083810u : 0x8008382Cu) + type * 4u);
        for (index = 0; index < 3u; ++index)
            w_u32(output + 0x10u + index * 4u, r_u32(source + index * 4u));
        w_u8(output + 1u, r_u8(source + 12u));
    }
    palette = 0x80092040u + type * 28u;
    if (mode < 1u || mode > 2u)
        palette += 196u;
    w_u32(output + 0x90u, palette);
    return output;
}

sint32 render_init_prims(uint32 state, uint32 context)
{
    uint32 environment = r_u32(context + 36u);
    uint32 packet;
    uint32 index;
    PSX_RECT window;

    FUNCTION_MARKER(0x8001483Cu, "MAIN.EXE");
    packet = game_alloc_arena_bytes(1600);
    w_u32(state + 1720u, packet);
    for (index = 0u; index < 40u; ++index, packet += 40u)
    {
        w_u8(packet + 3u, 9u);
        w_u8(packet + 7u, 0x2Du);
        w_u16(packet + 22u, (uint16)getTPage(0, 0, 896, 256));
        w_u16(packet + 14u, (uint16)getClut(896, 511));
    }
    packet = game_alloc_arena_bytes(480);
    w_u32(state + 1724u, packet);
    for (index = 0u; index < 20u; ++index, packet += 24u)
    {
        w_u8(packet + 3u, 5u);
        w_u8(packet + 7u, 0x28u);
    }
    packet = game_alloc_arena_bytes(360);
    w_u32(state + 1728u, packet);
    for (index = 0u; index < 10u; ++index, packet += 36u)
    {
        w_u8(packet + 3u, 8u);
        w_u8(packet + 7u, 0x38u);
        w_u32(packet + 4u, r_u32(environment + 24u));
        w_u32(packet + 12u, r_u32(environment + 24u));
        w_u32(packet + 20u, r_u32(environment + 20u));
        w_u32(packet + 28u, r_u32(environment + 20u));
    }
    if (r_u16(environment + 14u) != 64u && r_u16(environment + 14u) != 128u && r_u16(environment + 14u) != 256u)
    {
        w_u16(environment + 14u, 256u);
    }
    setRECT(&window, 16, 0, 48, 64);
    SetTexWindow(psx_addr(state + 176u, sizeof(DR_MODE)), &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(psx_addr(state + 188u, sizeof(DR_MODE)), &window);
    window.w = (sint16)r_u16(environment + 14u);
    SetTexWindow(psx_addr(state + 200u, sizeof(DR_MODE)), &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(psx_addr(state + 212u, sizeof(DR_MODE)), &window);
    packet = game_alloc_arena_bytes(128);
    w_u32(state + 1732u, packet);
    for (index = 0u; index < 8u; ++index, packet += 16u)
    {
        w_u8(packet + 3u, 3u);
        w_u8(packet + 4u, 0xFFu);
        w_u8(packet + 5u, 0xFFu);
        w_u8(packet + 6u, 0xFFu);
        w_u8(packet + 7u, 0x40u);
    }
    return 0;
}

sint32 render_build_curve_scaled_lut(void)
{
    sint32 scale;

    FUNCTION_MARKER(0x80014A68u, "MAIN.EXE");
    for (scale = 0; scale < 16; ++scale)
    {
        uint32 output = 0x800F0508u + (uint32)scale * 512u;
        sint32 index;

        for (index = 0; index < 128; ++index)
            w_u8(output + (uint32)index, (uint8)((scale * r_u8(0x80083774u + (uint32)index)) >> 5));
        for (index = 0; index < 128; ++index)
            w_u8(output + 128u + (uint32)index, (uint8)((scale * r_u8(0x800837F3u - (uint32)index)) >> 5));
        for (index = 0; index < 128; ++index)
            w_u8(output + 256u + (uint32)index, (uint8)((scale * (128 - r_u8(0x80083775u + (uint32)index))) >> 5));
        for (index = 0; index < 128; ++index)
            w_u8(output + 384u + (uint32)index, (uint8)((scale * (128 - r_u8(0x800837F3u - (uint32)index))) >> 5));
    }
    return 0;
}

sint32 render_build_tex_desc_table(sint32 group)
{
    uint32 source;
    sint32 count;
    sint32 index;
    uint16 tpage;

    FUNCTION_MARKER(0x80014B7Cu, "MAIN.EXE");
    switch (group)
    {
        case 0:
            source = 0x800922C0u;
            count = 23;
            break;
        case 1:
            source = 0x80092520u;
            count = 11;
            break;
        case 2:
            source = 0x800925D0u;
            count = 16;
            break;
        case 3:
        case 5:
            source = 0x800928D0u;
            count = 32;
            break;
        case 4:
            source = 0x80092430u;
            count = 15;
            break;
        case 6:
            source = 0x800926D0u;
            count = 32;
            break;
        default:
            input_stop_exit();
            return 0;
    }
    tpage = GetTPage(0, 0, 768, 256);
    for (index = 0; index < count; ++index)
    {
        uint32 output = 0x800F2588u + (uint32)index * 16u;
        uint32 input = source + (uint32)index * 16u;
        uint32 byte_index;

        w_u16(output, tpage);
        w_u16(output + 2u, GetClut(r_u16(input), r_u16(input + 2u)));
        for (byte_index = 0; byte_index < 8u; ++byte_index)
            w_u8(output + 4u + byte_index, r_u8(input + 4u + byte_index));
        w_u32(output + 12u, r_u32(input + 12u));
    }
    return 0;
}

sint32 render_init_ui_prims(void)
{
    uint32 address = 0x800B7410u;
    uint32 index;

    FUNCTION_MARKER(0x80015A78u, "MAIN.EXE");
    for (index = 0; index < 6u; ++index)
    {
        w_u8(address + 3u, 5u);
        w_u8(address + 7u, 72u);
        w_u32(address + 20u, 0x55555555u);
        w_u8(address + 4u, 0xFFu);
        w_u8(address + 5u, 0xFFu);
        w_u8(address + 6u, 16u);
        address += 24u;
    }
    address = 0x800B73F0u;
    for (index = 0; index < 2u; ++index)
    {
        w_u8(address + 3u, 3u);
        w_u8(address + 7u, 64u);
        w_u8(address + 4u, 0xFFu);
        w_u8(address + 5u, 0xFFu);
        w_u8(address + 6u, 16u);
        address += 16u;
    }
    render_indicator_target = NULL;
    render_indicator_distance = 0;
    render_indicator_ticks = 0;
    return 0;
}

uint32 render_target_indicator(uint32 owner, uint32 ordering_table, sint32 alternate)
{
    SVECTOR vertices[8];
    MATRIX matrix;
    sint32 first[3];
    sint32 second[3];
    sint32 third[3];
    sint32 depths[3];
    sint32 flags;
    uint32 primitive = alternate ? 0x800B7458u : 0x800B7410u;
    uint32 final_primitive = alternate ? 0x800B7400u : 0x800B73F0u;
    BOAT *candidate = NULL;
    uint32 vehicle;
    sint32 angle = 0;
    sint32 steering = 0;
    uint32 index;

    FUNCTION_MARKER(0x80015B4Cu, "MAIN.EXE");
    for (index = 0; index < sizeof(vertices); ++index)
        ((uint8 *)vertices)[index] = r_u8(0x80080AE4u + index);
    for (index = 0; index < sizeof(MATRIX) / 4u; ++index)
        ((uint32 *)&matrix)[index] = r_u32(0x80080B24u + index * 4u);
    if (r_u32(0x80083484u) == 5u)
        return 2;
    if (r_u32(0x80083478u) == 2u)
        return 4;
    vehicle = r_u32(owner + 0x64u);
    if (vehicle_menu(vehicle)->mode != 4u)
        return 4;
    for (index = (uint32)r_u8(vehicle + 14u) + 1u; index < vehicle_racer_count; ++index)
    {
        BOAT *entry = vehicle_racers[index];

        if ((sint16)entry->race.mode != 4)
            break;
        if ((sint32)(uint32)entry->route.distance < 0x1000000 && (candidate == 0 || (sint32)(uint32)entry->route.distance < (sint32)(uint32)candidate->route.distance))
            candidate = entry;
    }
    if (render_indicator_target == 0)
    {
        if (candidate == 0 || (sint16)candidate->race.mode != 4)
            return 4;
        render_indicator_target = candidate;
        render_indicator_ticks = 0;
    }
    else
    {
        BOAT *current = render_indicator_target;

        // Original empty-candidate low-RAM read pending caller invariant audit
        if (candidate != current && (sint32)((uint32)current->route.distance - (candidate ? (uint32)candidate->route.distance : r_u32(0x0000093Cu))) < 2500)
            candidate = current;
        if (candidate == current)
        {
            if ((sint16)candidate->race.mode != 4)
            {
                render_indicator_target = NULL;
                return 4;
            }
            if ((sint32)(uint32)render_indicator_distance >= (sint32)(uint32)candidate->route.distance && (sint32)render_indicator_ticks < 240)
                render_indicator_ticks += 4u;
        }
        else
        {
            if (candidate == 0 || (sint16)candidate->race.mode != 4)
            {
                render_indicator_target = NULL;
                return 4;
            }
            render_indicator_target = candidate;
        }
    }
    render_indicator_distance = candidate->route.distance;
    if ((sint32)render_indicator_ticks < 120)
        return 1;
    if (candidate != 0)
    {
        const BOAT *camera = vehicle_player(owner);
        sint32 target_angle = ratan2((sint32)((uint32)candidate->motion.position[0] - (uint32)camera->motion.position[0]), (sint32)((uint32)candidate->motion.position[2] - (uint32)camera->motion.position[2]));
        sint32 camera_angle = ratan2((sint16)(uint16)camera->motion.transform.pose.m[0][2], (sint16)(uint16)camera->motion.transform.pose.m[2][2]);
        uint32 adjusted;

        angle = (target_angle - ((camera_angle & 0xFFF) - 0x800)) & 0xFFF;
        adjusted = (uint32)angle - 1u;
        if ((uint32)(angle - 0x201) < 0x5FFu)
        {
            angle = 0x200;
            adjusted = 0x1FFu;
        }
        else if ((uint32)(angle - 0x801) < 0x5FFu)
        {
            angle = 0xE00;
            adjusted = 0xDFFu;
        }
        if (adjusted < 0x200u)
        {
            steering = -angle / 16;
            if (steering < -40)
                steering = -40;
        }
        else
        {
            steering = (0x1000 - angle) / 16;
            if (steering > 40)
                steering = 40;
        }
    }
    matrix.m[0][0] = 1000;
    matrix.m[1][1] = 1000;
    RotMatrixZ(angle, &matrix);
    matrix.t[0] = steering;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    gte_project3_full_depth(&vertices[0], first, depths, &flags);
    gte_project3_full_depth(&vertices[2], second, depths, &flags);
    {
        uint32 primitive_tag = r_u32(primitive);
        uint32 ordering_tag;

        w_u32(primitive + 8u, (uint32)first[0]);
        w_u32(primitive + 12u, (uint32)first[1]);
        w_u32(primitive + 16u, (uint32)first[2]);
        ordering_tag = r_u32(ordering_table);
        w_u32(primitive, (primitive_tag & 0xFF000000u) | (ordering_tag & 0xFFFFFFu));
        ordering_tag = r_u32(ordering_table);
        w_u32(ordering_table, (ordering_tag & 0xFF000000u) | (primitive & 0xFFFFFFu));
    }
    primitive += 24u;
    gte_project3_full_depth(&vertices[5], third, depths, &flags);
    {
        uint32 primitive_tag = r_u32(primitive);
        uint32 ordering_tag;

        w_u32(primitive + 8u, (uint32)second[0]);
        w_u32(primitive + 12u, (uint32)second[1]);
        w_u32(primitive + 16u, (uint32)second[2]);
        ordering_tag = r_u32(ordering_table);
        w_u32(primitive, (primitive_tag & 0xFF000000u) | (ordering_tag & 0xFFFFFFu));
        ordering_tag = r_u32(ordering_table);
        w_u32(ordering_table, (ordering_tag & 0xFF000000u) | (primitive & 0xFFFFFFu));
    }
    primitive += 24u;
    {
        uint32 primitive_tag;
        uint32 ordering_tag;

        w_u32(primitive + 8u, (uint32)second[2]);
        primitive_tag = r_u32(primitive);
        w_u32(primitive + 12u, (uint32)third[0]);
        w_u32(primitive + 16u, (uint32)third[1]);
        ordering_tag = r_u32(ordering_table);
        w_u32(primitive, (primitive_tag & 0xFF000000u) | (ordering_tag & 0xFFFFFFu));
        ordering_tag = r_u32(ordering_table);
        w_u32(ordering_table, (ordering_tag & 0xFF000000u) | (primitive & 0xFFFFFFu));
    }
    {
        uint32 primitive_tag = r_u32(final_primitive);
        uint32 ordering_tag;
        uint32 result;

        w_u32(final_primitive + 8u, (uint32)third[1]);
        w_u32(final_primitive + 12u, (uint32)third[2]);
        ordering_tag = r_u32(ordering_table);
        w_u32(final_primitive, (primitive_tag & 0xFF000000u) | (ordering_tag & 0xFFFFFFu));
        ordering_tag = r_u32(ordering_table);
        result = (ordering_tag & 0xFF000000u) | (final_primitive & 0xFFFFFFu);
        w_u32(ordering_table, result);
        return result;
    }
}

void render_set_order_depth(uint32 first, uint32 second)
{
    FUNCTION_MARKER(0x80016190u, "MAIN.EXE");
    w_u32(0x800B6AB4u, first);
    w_u32(0x800B6A08u, second);
}

sint32 render_scale_proj_transform(uint32 shift)
{
    MATRIX matrix;
    sint16 *values = &matrix.m[0][0];
    uint32 shift_bits = shift & 31u;
    uint32 index;
    sint32 projection;

    FUNCTION_MARKER(0x800161A0u, "MAIN.EXE");
    ReadRotMatrix(&matrix);
    for (index = 0; index < 6u; ++index)
    {
        sint32 value = (sint32)((uint32)(sint32)values[index] << shift_bits);

        if (shift == 3u)
        {
            if (value < INT16_MIN)
                value = INT16_MIN;
            if (value > INT16_MAX)
                value = INT16_MAX;
        }
        values[index] = (sint16)value;
    }
    matrix.t[0] = (sint32)((uint32)matrix.t[0] << shift_bits);
    matrix.t[1] = (sint32)((uint32)matrix.t[1] << shift_bits);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    projection = math_sra_signed(r_s32(0x800B6BDCu), shift);
    gte_write_h((uint16)projection);
    return projection;
}

void render_set_proj_dist(uint32 projection)
{
    FUNCTION_MARKER(0x80016310u, "MAIN.EXE");
    gte_write_h((uint16)projection);
    w_u32(0x800B6BDCu, projection);
}

void render_set_geom_offset(sint32 x, sint32 y)
{
    FUNCTION_MARKER(0x80016320u, "MAIN.EXE");
    SetGeomOffset(x, y);
    w_u32(0x800B6AC4u, (uint32)x);
    w_u32(0x800B6AC8u, (uint32)y);
}

uint32 render_alloc_mode_bufs(void)
{
    uint32 size = r_u16(0x800E1B9Cu) != 0u ? 0x20000u : (r_u32(0x80083478u) == 2u ? 0x16000u : 0x10000u);
    uint32 first;
    uint32 second;

    FUNCTION_MARKER(0x80016340u, "MAIN.EXE");
    first = game_alloc_arena_bytes((sint32)size);
    w_u32(0x800B6798u, first);
    second = game_alloc_arena_bytes((sint32)size);
    first = r_u32(0x800B6798u);
    w_u32(0x800B679Cu, second);
    w_u32(0x800B6A5Cu, first);
    w_u32(0x800B6C00u, first);
    return second;
}

uint32 render_select_prim_pool(sint32 alternate)
{
    uint32 result;

    FUNCTION_MARKER(0x800163B0u, "MAIN.EXE");
    result = r_u32(alternate ? 0x800B679Cu : 0x800B6798u);
    w_u32(0x800B6C00u, result);
    return result;
}

sint32 render_generate_entries(const sint32 transform[3], uint32 object, sint32 remaining)
{
    uint32 count_address = 0x800B3E14u;
    uint32 output;
    uint32 result;
    uint32 count;

    FUNCTION_MARKER_ARGS(0x80018F48u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 3u, XPORT_CALL_GUEST_POINTER(transform, 52u), XPORT_CALL_GUEST_POINTER(object, 1u), XPORT_CALL_SCALAR((uint32)remaining));
    count = r_u32(count_address);
    result = count * 8u;
    output = 0x800B7680u + result;
    while (remaining != 0)
    {
        sint32 index;
        sint32 values[4];

        count = r_u32(count_address);
        if ((sint32)count >= 64)
            return 0;
        for (index = 0; index < (sint32)r_u8(object + 7u); ++index)
        {
            uint32 entry = object + 33u + (uint32)index * 14u;
            uint8 type = r_u8(entry) >> 5;
            uint16 value;
            uint16 base;

            if (type == 0u)
                continue;
            route_sample_geom_host(object, index, 1, values);
            value = (uint16)values[0];
            base = (uint16)transform[0];
            w_u16(output, (uint16)(value + base));
            value = (uint16)values[1];
            base = (uint16)transform[1];
            w_u16(output + 2u, (uint16)(value + base));
            value = (uint16)values[2];
            base = (uint16)transform[2];
            w_u16(output + 4u, (uint16)(value + base));
            output += 8u;
            count = r_u32(count_address);
            type = r_u8(entry) >> 5;
            w_u16(output - 2u, type);
            w_u32(count_address, count + 1u);
        }
        --remaining;
        {
            uint16 object_index = r_u16(object);
            uint32 table = r_u32(0x800B6B80u);

            result = table + (uint32)object_index * 4u;
        }
        object = r_u32(result);
    }
    return (sint32)result;
}

sint32 render_build_tex_descs(void)
{
    sint32 x = 816;
    sint32 y = 256;
    sint32 palette_y = 503;
    uint32 output = 0x800D6A48u;
    uint32 index;

    FUNCTION_MARKER(0x8001909Cu, "MAIN.EXE");
    for (index = 0; index < 16u; ++index)
    {
        uint8 u = (uint8)((x % 64) * 4);
        uint8 v = (uint8)(y % 256);

        w_u16(output, GetTPage(0, 1, x, y));
        w_u16(output + 2u, GetClut(784, palette_y));
        w_u8(output + 4u, u);
        w_u8(output + 5u, v);
        w_u8(output + 6u, (uint8)(r_u8(output + 4u) + 31u));
        w_u8(output + 7u, r_u8(output + 5u));
        w_u8(output + 8u, r_u8(output + 4u));
        w_u8(output + 9u, (uint8)(r_u8(output + 5u) + 31u));
        w_u8(output + 10u, (uint8)(r_u8(output + 4u) + 31u));
        v = r_u8(output + 5u);
        y += 32;
        w_u8(output + 11u, (uint8)(v + 31u));
        ++palette_y;
        if (y == 512)
        {
            y = 256;
            x += 8;
        }
        output += 16u;
    }
    w_u8(0x80083739u, 0);
    w_u8(0x80083738u, 0);
    w_u8(0x8008373Au, 0);
    w_u16(0x800F2508u, GetTPage(0, 1, 808, 448));
    w_u16(0x800F250Au, GetClut(800, 481));
    w_u8(0x800F250Cu, 160u);
    w_u8(0x800F250Du, 192u);
    w_u8(0x800F250Eu, 191u);
    w_u8(0x800F250Fu, 192u);
    w_u8(0x800F2510u, 160u);
    w_u8(0x800F2511u, 223u);
    w_u8(0x800F2512u, 191u);
    w_u8(0x800F2513u, 223u);
    w_u16(0x800DCC30u, GetTPage(0, 1, 800, 448));
    w_u16(0x800DCC32u, GetClut(800, 480));
    w_u8(0x800DCC34u, 128u);
    w_u8(0x800DCC35u, 192u);
    w_u8(0x800DCC36u, 159u);
    w_u8(0x800DCC37u, 192u);
    w_u8(0x800DCC38u, 128u);
    w_u8(0x800DCC39u, 223u);
    w_u8(0x800DCC3Au, 159u);
    w_u8(0x800DCC3Bu, 223u);
    return 159;
}

uint32 render_billboard(uint32 ordering_table, uint32 entries, sint32 count, sint32 lower)
{
    uint32 frame = guest_stack_push(0xA0u);
    MATRIX saved;
    MATRIX identity = {0};
    uint32 packet;
    uint32 animation;
    uint32 animation_phase;
    uint32 result;
    sint32 width3;
    sint32 height3;
    sint32 index;

    FUNCTION_MARKER(0x80019340u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 8u)
    {
        uint32 phase = r_u16(0x800B3D94u) & 0x7Fu;

        if (phase >= 64u)
            phase = 127u - phase;
        width3 = (sint32)phase + 64;
        height3 = ((sint32)phase * 4 + 256) / 3;
    }
    else
    {
        width3 = 64;
        height3 = 84;
    }
    animation_phase = (uint32)r_u16(0x800B3D94u);
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    animation = (animation_phase >> 2) % 15u;
    ReadRotMatrix(&saved);
    SetLightMatrix(&saved);
    gte_set_back_color_raw(saved.t[0], saved.t[1], saved.t[2]);
    identity.m[0][0] = 4096;
    identity.m[1][1] = 4096;
    identity.m[2][2] = 4096;
    SetRotMatrix(&identity);
    SetTransMatrix(&identity);
    for (index = 0; index < count; ++index, entries += 8u)
    {
        sint32 type = (sint16)r_u16(entries + 6u);
        sint32 half_width;
        sint32 height;
        SVECTOR center;
        VECTOR transformed;
        SVECTOR vertices[4];
        sint32 projected[4];
        sint32 depths[3];
        sint32 flags;
        sint32 vertex_flags;
        sint32 depth;
        uint32 texture;
        uint32 bucket;

        if (type == 0)
            continue;
        if (type == 1)
        {
            half_width = 24;
            height = 48;
        }
        else if (type == 2)
        {
            half_width = 28;
            height = 70;
        }
        else if (type == 3)
        {
            half_width = width3;
            height = height3;
        }
        else
        {
            half_width = 64;
            height = 84;
        }
        if (r_u32(0x80083478u) == 2u)
            height /= 2;
        center.vx = (sint16)r_u16(entries);
        center.vy = (sint16)r_u16(entries + 2u);
        center.vz = (sint16)r_u16(entries + 4u);
        center.pad = 0;
        gte_transform_light(&center, &transformed, &vertex_flags);
        vertices[0].vx = (sint16)(transformed.vx - half_width);
        vertices[0].vy = (sint16)(transformed.vy + (lower ? height : -height));
        vertices[0].vz = (sint16)transformed.vz;
        vertices[0].pad = 0;
        vertices[1].vx = (sint16)(transformed.vx + half_width);
        vertices[1].vy = vertices[0].vy;
        vertices[1].vz = vertices[0].vz;
        vertices[1].pad = 0;
        vertices[2].vx = vertices[0].vx;
        vertices[2].vy = (sint16)transformed.vy;
        vertices[2].vz = vertices[0].vz;
        vertices[2].pad = 0;
        vertices[3].vx = vertices[1].vx;
        vertices[3].vy = vertices[2].vy;
        vertices[3].vz = vertices[0].vz;
        vertices[3].pad = 0;
        gte_project3_full_depth(vertices, projected, depths, &flags);
        if (flags < 0)
            continue;
        w_u32(packet + 8u, (uint32)projected[0]);
        w_u32(packet + 16u, (uint32)projected[1]);
        w_u32(packet + 24u, (uint32)projected[2]);
        depth = AverageZ3(depths[0], depths[1], depths[2]);
        --depth;
        if ((uint32)depth >= 2000u)
            continue;
        depth = (sint32)((uint32)depth >> 3);
        gte_project(&vertices[3], &projected[3], &vertex_flags);
        w_u32(packet + 32u, (uint32)projected[3]);
        type = (sint16)r_u16(entries + 6u);
        if (type == 1)
        {
            texture = 0x800DCC30u;
            w_u16(packet + 22u, r_u16(texture));
            w_u16(packet + 14u, r_u16(texture + 2u));
        }
        else if (type == 2)
        {
            texture = 0x800F2508u;
            w_u16(packet + 22u, r_u16(texture));
            w_u16(packet + 14u, r_u16(texture + 2u));
        }
        else
        {
            texture = 0x800D6A48u + animation * 16u;
            w_u16(packet + 22u, r_u16(texture));
            type = (sint16)r_u16(entries + 6u);
            w_u16(packet + 14u, r_u16(0x800D6A4Au + (uint32)type * 16u));
        }
        w_u32(packet + 4u, lower ? 0x2C453030u : 0x2C808080u);
        w_u16(packet + 12u, r_u16(texture + 4u));
        w_u16(packet + 20u, r_u16(texture + 6u));
        w_u16(packet + 28u, r_u16(texture + 8u));
        w_u16(packet + 36u, r_u16(texture + 10u));
        bucket = ordering_table + (uint32)depth * 4u;
        w_u32(packet, r_u32(bucket) | 0x09000000u);
        w_u32(bucket, packet);
        packet += 40u;
    }
    SetRotMatrix(&saved);
    SetTransMatrix(&saved);
    w_u32(0x800B6C00u, packet);
    result = frame + 0x20u;
    guest_stack_pop(0xA0u);
    return result;
}

/* Implements each RTPT group from MAIN.EXE:0x800198AC */
uint32 render_horizon(uint32 owner, const CAMERA_STATE *view)
{
    uint32 strip_pool;
    uint32 edge_pool;
    uint32 triangle_pool;
    uint32 config;
    sint16 heights[12] = {0};
    uint32 projected_a[9] = {0};
    uint32 projected_b[9] = {0};
    uint32 *current = projected_a;
    uint32 *next = projected_b;
    sint32 angle;
    sint32 position;
    sint32 shade;
    sint32 boundary;
    sint32 ascending = 1;
    sint32 height_count;
    sint32 group_count;
    sint32 radius_scale;
    sint32 ring_count;
    sint32 stripe_bucket_step;
    sint32 ring;
    sint32 group;
    uint32 result;

    FUNCTION_MARKER(0x800198ACu, "MAIN.EXE");
    triangle_pool = r_u32(owner + 0x6C0u);
    strip_pool = r_u32(owner + 0x6B8u);
    edge_pool = r_u32(owner + 0x6BCu);
    {
        sint32 first = view->rotation.m[2][2];
        sint32 second = view->rotation.m[0][2];

        config = r_u32(0x8008349Cu);
        angle = 1024 - (ratan2(first, second) & 0xFFF);
    }
    if (angle < 0)
        angle += 4096;
    {
        sint32 projection = (sint16)gte_read_h();
        uint32 mode = r_u32(0x80083478u);
        sint32 numerator;

        if (mode == 1u)
        {
            numerator = 256;
            ring_count = 10;
        }
        else
        {
            mode = r_u32(0x80083488u);
            if (mode == 1u)
            {
                numerator = 256;
                ring_count = 10;
            }
            else
            {
                numerator = 128;
                ring_count = 8;
            }
        }
        angle = (sint16)(-angle - ratan2(numerator, projection) - 135);
    }
    position = angle % 684;
    if (position < 0)
    {
        if (r_u8(config) != 0u)
            position = -position;
        else
            position += 683;
    }
    shade = 255 * position / 683;
    if (r_u8(config) != 0u)
    {
        if (((angle / 684) & 1) != 0)
            shade = 255 - shade;
        else
            ascending = 0;
        if ((sint16)shade < 0)
            shade = 0;
        else if ((sint16)shade >= 256)
            shade = 255;
    }
    heights[0] = (sint16)r_u16(config + 4u);
    height_count = 1;
    if (r_u8(config + 2u) != 0u)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)r_u16(config + 6u));
        ++height_count;
    }
    for (group = 0; group < (sint32)r_u8(config + 3u); ++group)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)r_u16(config + 8u));
        ++height_count;
    }
    if (r_u8(config + 1u) != 0u)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] - (sint16)r_u16(config + 12u));
        ++height_count;
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)r_u16(config + 10u));
        ++height_count;
    }
    else
    {
        heights[height_count] = (sint16)(heights[height_count - 1] - (sint16)r_u16(config + 12u));
        ++height_count;
    }
    group_count = (height_count + 2) / 3;
    radius_scale = r_u32(0x80083484u) == 8u ? 1 : 2;
    for (group = 0; group < group_count; ++group)
    {
        sint16 group_heights[3] = {heights[group * 3], heights[group * 3 + 1], heights[group * 3 + 2]};
        sint16 sine = (sint16)(rsin((sint16)angle) * radius_scale);
        sint16 cosine = (sint16)(rcos((sint16)angle) * radius_scale);

        render_project_horizon(sine, cosine, group_heights, &projected_a[group * 3]);
    }
    if (ascending)
    {
        boundary = (sint16)((shade + (shade < 0 ? 63 : 0)) | 63);
        angle = (sint16)(angle + (171 * (boundary - (sint16)shade)) / 63);
    }
    else
    {
        boundary = ((sint16)shade / 64) * 64;
        angle = (sint16)(angle + (171 * ((sint16)shade - boundary)) / 63);
    }
    stripe_bucket_step = (sint16)r_u16(config + 14u) / 64;
    for (ring = 0; ring < ring_count; ++ring)
    {
        uint32 edge_packet = edge_pool;
        uint32 edge_present;

        for (group = 0; group < group_count; ++group)
        {
            sint16 group_heights[3] = {heights[group * 3], heights[group * 3 + 1], heights[group * 3 + 2]};
            sint16 sine = (sint16)(rsin((sint16)angle) * radius_scale);
            sint16 cosine = (sint16)(rcos((sint16)angle) * radius_scale);

            render_project_horizon(sine, cosine, group_heights, &next[group * 3]);
        }
        edge_present = r_u8(config + 2u) != 0u;
        if (edge_present)
        {
            uint32 packet = edge_packet;
            uint32 old;
            uint32 tag;

            w_u32(packet + 16u, current[0]);
            w_u32(packet + 8u, current[1]);
            w_u32(packet + 20u, next[0]);
            w_u32(packet + 12u, next[1]);
            w_u32(packet + 4u, r_u32(config + 16u));
            old = r_u32(owner + 0x694u);
            tag = r_u32(packet);
            w_u32(packet, (tag & 0xFF000000u) | (old & 0xFFFFFFu));
            old = r_u32(owner + 0x694u);
            w_u32(owner + 0x694u, (old & 0xFF000000u) | (packet & 0xFFFFFFu));
            edge_pool += 24u;
            edge_packet = edge_pool;
        }
        for (group = 0; group < (sint32)r_u8(config + 3u); ++group)
        {
            uint32 packet = strip_pool;
            uint32 point = edge_present + (uint32)group;
            uint32 next_second;
            uint32 packet_tag;
            uint32 bucket_tag;
            sint32 texture_left = -2 - 32 * (group + 1);
            sint32 texture_right = -2 - 32 * group;
            sint32 bucket_offset = (group / 2) * stripe_bucket_step;
            sint32 shade_bucket = (ascending ? (sint16)shade : (sint16)boundary) / 64;
            uint32 bucket = owner + (uint32)(shade_bucket + 413 + bucket_offset) * 4u;

            w_u32(packet + 24u, current[point]);
            w_u32(packet + 8u, current[point + 1u]);
            w_u32(packet + 32u, next[point]);
            next_second = next[point + 1u];
            w_u8(packet + 28u, (uint8)shade);
            w_u8(packet + 12u, (uint8)shade);
            w_u8(packet + 36u, (uint8)boundary);
            w_u8(packet + 20u, (uint8)boundary);
            w_u8(packet + 21u, (uint8)texture_left);
            w_u8(packet + 13u, (uint8)texture_left);
            w_u8(packet + 37u, (uint8)texture_right);
            w_u8(packet + 29u, (uint8)texture_right);
            w_u32(packet + 16u, next_second);
            packet_tag = r_u32(packet);
            bucket_tag = r_u32(bucket);
            w_u32(packet, (packet_tag & 0xFF000000u) | (bucket_tag & 0xFFFFFFu));
            bucket_tag = r_u32(bucket);
            w_u32(bucket, (bucket_tag & 0xFF000000u) | (packet & 0xFFFFFFu));
            strip_pool += 40u;
        }
        {
            uint8 first = r_u8(strip_pool - 27u);
            uint8 second = r_u8(strip_pool - 19u);

            w_u8(strip_pool - 27u, (uint8)(first + 1u));
            w_u8(strip_pool - 19u, (uint8)(second + 1u));
        }
        if (r_u8(config + 1u) != 0u)
        {
            uint32 packet = triangle_pool;
            uint32 old;
            uint32 tag;

            w_u32(packet + 24u, current[height_count - 2]);
            w_u32(packet + 8u, current[height_count - 1]);
            w_u32(packet + 32u, next[height_count - 2]);
            w_u32(packet + 16u, next[height_count - 1]);
            old = r_u32(owner + 0x694u);
            tag = r_u32(packet);
            w_u32(packet, (tag & 0xFF000000u) | (old & 0xFFFFFFu));
            old = r_u32(owner + 0x694u);
            w_u32(owner + 0x694u, (old & 0xFF000000u) | (packet & 0xFFFFFFu));
            triangle_pool += 36u;
        }
        {
            uint32 packet = edge_packet;
            uint32 old;
            uint32 tag;
            uint32 point;

            point = current[height_count - 1];
            w_u32(packet + 8u, point);
            w_u32(packet + 16u, point);
            point = next[height_count - 1];
            w_u32(packet + 12u, point);
            w_u32(packet + 20u, point);
            w_u16(packet + 10u, 0);
            w_u16(packet + 14u, 0);
            w_u32(packet + 4u, r_u32(config + 24u));
            w_u8(packet + 7u, 40u);
            old = r_u32(owner + 0x694u);
            tag = r_u32(packet);
            w_u32(packet, (tag & 0xFF000000u) | (old & 0xFFFFFFu));
            old = r_u32(owner + 0x694u);
            w_u32(owner + 0x694u, (old & 0xFF000000u) | (packet & 0xFFFFFFu));
            edge_pool += 24u;
        }
        if (ascending)
        {
            shade = boundary + 1;
            boundary += 64;
            if ((sint16)shade >= 256)
            {
                shade = 255;
                if (r_u8(config) != 0u)
                {
                    boundary = 192;
                    ascending = 0;
                }
                else
                {
                    shade = 0;
                    boundary = 63;
                }
            }
        }
        else
        {
            shade = boundary - 1;
            boundary -= 64;
            if ((sint16)shade < 0)
            {
                shade = 0;
                if (r_u8(config) != 0u)
                {
                    boundary = 63;
                    ascending = 1;
                }
                else
                {
                    shade = 255;
                    boundary = 192;
                }
            }
        }
        angle = (sint16)(angle + 171);
        if ((ring & 1) != 0)
        {
            current = projected_a;
            next = projected_b;
        }
        else
        {
            current = projected_b;
            next = projected_a;
        }
    }
    {
        uint32 first = r_u32(owner + 200u);
        uint32 active_first = r_u32(owner + 0x694u);
        uint32 active_second = r_u32(owner + 0x694u);
        uint32 second = r_u32(owner + 212u);
        uint32 alternate_first = r_u32(owner + 0x674u);
        uint32 first_result = (first & 0xFF000000u) | (active_first & 0xFFFFFFu);
        uint32 active_result = (active_second & 0xFF000000u) | ((owner + 200u) & 0xFFFFFFu);
        uint32 second_result = (second & 0xFF000000u) | (alternate_first & 0xFFFFFFu);
        uint32 alternate_second = r_u32(owner + 0x674u);

        w_u32(owner + 200u, first_result);
        w_u32(owner + 0x694u, active_result);
        w_u32(owner + 212u, second_result);
        result = (owner + 212u) & 0xFFFFFFu;
        w_u32(owner + 0x674u, (alternate_second & 0xFF000000u) | result);
    }
    return result;
}

sint32 render_dispatch_prim_groups(uint32 object, uint32 table, sint32 adjusted, uint32 ordering_table)
{
    uint32 entries;
    sint32 entry_index;
    sint32 result;

    FUNCTION_MARKER(0x8001DA94u, "MAIN.EXE");
    render_set_order_depth(ordering_table, 100u);
    result = (sint32)r_u16(object + 2u);
    entries = r_u32(object + 8u);
    if (result == 0)
        return result;
    entry_index = 0;
    do
    {
        if (r_u16(entries + 2u) == 2u)
        {
            uint32 item = r_u16(entries);
            uint32 base = r_u32(0x80083498u);
            uint32 offset = 32u * (item - 1u) + 4u;
            uint32 descriptor = base + offset;
            uint32 node;

            if (descriptor == 0u)
                return (sint32)offset;
            node = r_u32(descriptor + 16u);
            while (node != 0u)
            {
                uint32 group_table = r_u32(node);
                uint32 groups = r_u32(group_table + 4u * table);

                poly_dispatch_prim_groups(groups, ordering_table, adjusted, 0);
                node = r_u32(node + 4u);
            }
            node = r_u32(descriptor + 20u);
            while (node != 0u)
            {
                uint32 group_table = r_u32(node);
                uint32 groups = r_u32(group_table + 4u * table);

                poly_dispatch_prim_groups(groups, ordering_table, adjusted, 1);
                node = r_u32(node + 4u);
            }
        }
        ++entry_index;
        result = entry_index < (sint32)r_u16(object + 2u);
        entries += 8u;
    } while (result != 0);
    return result;
}

sint32 render_init_context(sint32 mode, uint32 unused)
{
    FUNCTION_MARKER(0x8001F0FCu, "MAIN.EXE");
    menu_init_layout();
    if (mode == 1)
    {
        sprite_build_rotating_pkt(0x800B4060u, 0, 0x800DE820u);
        sprite_build_rotating_pkt(0x800B4060u, 1, 0x800DEF18u);
        render_build_text_records(0x800839E8u, 0u, 0x800DE820u);
        render_build_text_records(0x800839E8u, 1u, 0x800DEF18u);
        w_u32(0x800B6ADCu, 17u);
        w_u32(0x800B6B9Cu, 0x8009365Cu);
        return 0x8009365C;
    }
    if (mode == 2)
    {
        uint32 source;
        uint32 records;
        uint32 final_table;
        sint32 base_index;
        sint32 index;
        if (r_u8(0x800E0599u) != 0u)
        {
            source = 0x800B406Cu;
            records = 0x80084028u;
            base_index = 4;
            final_table = 0x80094BF4u;
        }
        else
        {
            source = 0x800B4074u;
            records = 0x80083D58u;
            base_index = 0;
            final_table = 0x8009422Cu;
        }
        for (index = 0; index < 4; ++index)
        {
            uint32 context = index < 2 ? 0x800DE820u + 1784u * (uint32)index : 0x800DF7C8u + 1784u * (uint32)(index - 2);
            sprite_build_rotating_pkt(source, base_index + index, context);
        }
        for (index = 0; index < 4; ++index)
        {
            uint32 context = index < 2 ? 0x800DE820u + 1784u * (uint32)index : 0x800DF7C8u + 1784u * (uint32)(index - 2);
            uint32 record_group = index < 2 ? records : records + 360u;
            render_build_text_records(record_group, (uint32)(base_index + index), context);
        }
        w_u32(0x800B6B9Cu, final_table);
        w_u32(0x800B6ADCu, 33u);
        return 33;
    }
    return 2;
}

sint32 render_publish_state(uint32 state)
{
    uint32 result;
    uint16 first;
    uint32 second;
    uint16 third;
    uint16 fourth;
    uint16 fifth;

    FUNCTION_MARKER(0x8002085Cu, "MAIN.EXE");
    result = r_u32(state + 32u);
    first = r_u16(state + 24u);
    second = r_u32(state + 36u);
    third = r_u16(state + 26u);
    fourth = r_u16(state + 28u);
    fifth = r_u16(state + 30u);
    w_u32(0x800B69A0u, result);
    w_u16(0x800B6ACEu, first);
    w_u32(0x800B69F4u, second);
    w_u16(0x800B6A88u, third);
    w_u16(0x800B69D0u, fourth);
    w_u16(0x800B69C8u, fifth);
    return (sint32)result;
}

sint32 sprite_build_rotating_pkt(uint32 source, sint32 buffer_index, uint32 state)
{
    uint32 record = r_u32(source + 4u);
    sint32 buffer_slot = buffer_index % 4;
    uint32 packet = 0x800D8DD8u + (uint32)(buffer_slot * 3960);
    sint32 count = 0;

    FUNCTION_MARKER(0x80020894u, "MAIN.EXE");
    w_u32(state + 32u, packet);
    while ((sint16)r_u16(record) >= 0)
    {
        sint16 clut;
        uint16 tpage;
        uint8 variant;

        w_u8(packet, 4u);
        w_u8(packet + 1u, r_u8(record + 16u));
        clut = (sint16)getClut((sint16)r_u16(record + 12u), (sint16)r_u16(record + 14u));
        tpage = (uint16)getTPage(0, 0, (sint16)r_u16(record + 4u), (sint16)r_u16(record + 6u));
        w_u8(packet + 8u, 0x80u);
        w_u8(packet + 9u, 0x80u);
        w_u8(packet + 10u, 0x80u);
        w_u16(packet + 12u, r_u16(record));
        w_u16(packet + 14u, (uint16)((sint16)r_u16(record + 2u) - ((uint32)(buffer_index - 2) < 2u ? 10 : 0)));
        if ((uint32)(buffer_index - 6) < 2u)
            w_u16(packet + 12u, r_u16(packet + 12u));
        w_u8(packet + 16u, (uint8)(((sint16)r_u16(record + 4u) % 64) * 4));
        w_u8(packet + 17u, (uint8)r_u16(record + 6u));
        w_u16(packet + 18u, (uint16)clut);
        w_u16(packet + 20u, (uint16)(4 * (sint16)r_u16(record + 8u)));
        w_u16(packet + 22u, r_u16(record + 10u));
        w_u8(packet + 7u, 4u);
        w_u8(packet + 11u, 0x64u);
        variant = r_u8(record + 16u);
        SetDrawMode(psx_addr(packet + 24u, sizeof(DR_MODE)), 1, 0, tpage, NULL);
        if (variant >= 2u)
            w_u16(packet + 14u, r_u16(record + 2u));
        packet += 36u;
        record += 18u;
        ++count;
    }
    w_u16(state + 24u, (uint16)count);
    w_u16(state + 28u, r_u16(source));
    w_u16(state + 30u, r_u16(source + 2u));
    SetDrawMode(psx_addr(state, sizeof(DR_MODE)), 1, 0, getTPage(0, 0, 832, 256), NULL);
    SetDrawMode(psx_addr(state + 12u, sizeof(DR_MODE)), 1, 0, getTPage(0, 0, 832, 256), NULL);
    return 0;
}

sint32 render_submit_active_recs(uint32 primitives, uint32 ordering_table)
{
    sint32 count;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80020D48u, "MAIN.EXE");
    count = (sint16)r_u16(0x800B6ACEu);
    w_u32(0x800B6AB4u, ordering_table);
    if (count <= 0)
        return count;
    index = 0;
    do
    {
        if (r_u8(primitives + 1u) == 1u)
        {
            uint32 active_ordering_table = r_u32(0x800B6AB4u);

            AddPrim(psx_addr(active_ordering_table, sizeof(uint32)), psx_addr(primitives + 4u, sizeof(uint32)));
            if (r_u8(primitives) == 4u)
            {
                active_ordering_table = r_u32(0x800B6AB4u);
                AddPrim(psx_addr(active_ordering_table, sizeof(uint32)), psx_addr(primitives + 24u, sizeof(uint32)));
            }
        }
        index = (sint16)(index + 1);
        primitives += 36u;
        count = (sint16)r_u16(0x800B6ACEu);
        result = index < count;
    } while (result);
    return result;
}

sint32 render_activate_recs(uint32 records)
{
    sint32 count = (sint16)r_u16(0x800B6ACEu);
    sint32 index;

    FUNCTION_MARKER(0x800210CCu, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        if (r_u8(records + (uint32)index * 36u + 1u) == 1u)
            w_u8(records + (uint32)index * 36u + 1u, 2u);
    }
    return index < count;
}

uint32 render_fn_8006c284(uint32 ordering_table, sint32 count)
{
    uint32 current = ordering_table;
    sint32 index;

    FUNCTION_MARKER(0x8006C284u, "MAIN.EXE");
    for (index = 0; index + 1 < count; ++index)
    {
        uint32 value = (r_u32(current) & 0xFF000000u) | ((current + 4u) & 0x00FFFFFFu);

        w_u32(current, value);
        current += 4u;
    }
    w_u32(current, 0x000A0CF4u);
    return current;
}

void render_fn_8006c434(uint32 ordering_table)
{
    FUNCTION_MARKER_ARGS(0x8006C434u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 1u, XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    DrawOTag(psx_addr(ordering_table, 4u));
}

sint32 render_build_text_records(uint32 records, uint32 unused, uint32 context)
{
    uint32 base;
    sint32 count = 0;
    uint16 identifier;
    sint16 result;

    FUNCTION_MARKER(0x8001F374u, "MAIN.EXE");
    w_u32(context + 36u, records);
    identifier = r_u16(records);
    result = (sint16)r_u16(records);
    base = r_u32(context + 32u);
    while (result >= 0)
    {
        uint32 table_offset = (uint32)(sint32)(sint16)identifier * 12u;
        sint16 glyph = (sint16)r_u16(0x800842FCu + table_offset);
        uint32 template = r_u32(0x800842F8u + table_offset);
        sint16 current_identifier;
        uint32 glyph_packet = base + (uint32)(sint32)glyph * 36u;
        uint32 secondary;

        ++count;
        w_u32(records + 4u, template);
        current_identifier = (sint16)r_u16(records);
        secondary = r_u32(0x80084300u + (uint32)(sint32)current_identifier * 12u);
        w_u32(records + 36u, glyph_packet + 4u);
        w_u32(records + 32u, glyph_packet + 24u);
        w_u32(records + 20u, secondary);
        records += 40u;
        result = (sint16)r_u16(records);
        identifier = r_u16(records);
    }
    w_u16(context + 26u, (uint16)count);
    return result;
}

void render_build_text_packets(void)
{
    sint32 descriptor_count;
    sint32 descriptor_index;
    sint32 glyph_index;
    sint32 index;
    uint32 output;
    uint32 last_output = 0u;
    sint32 last_output_valid = 0;

    FUNCTION_MARKER(0x800205ACu, "MAIN.EXE");
    descriptor_count = (sint16)r_u16(0x800B6A88u);
    w_u16(0x800B6BC4u, 0u);
    if (descriptor_count <= 0)
        return;
    glyph_index = (sint16)r_u16(0x800B6ACEu);
    output = r_u32(0x800B69A0u) + (uint32)(glyph_index * 36);
    descriptor_index = 0;
    do
    {
        uint32 descriptor = r_u32(0x800B69F4u) + (uint32)descriptor_index * 40u;

        if (r_u8(descriptor + 12u) == 1u)
        {
            uint32 advance = 0u;
            uint32 text = r_u32(descriptor + 24u);
            uint32 output_start = output;
            uint32 metrics_base = r_u32(descriptor + 4u);
            uint32 metrics_characters = r_u32(descriptor + 20u);
            uint32 shape = r_u32(descriptor + 36u);
            uint32 color = r_u32(descriptor + 32u);
            uint32 x = r_u16(descriptor + 8u);
            uint32 character = r_u8(text);
            uint32 y = r_u16(descriptor + 10u);
            sint32 character_count = 0;

            while (character != 0u)
            {
                sint32 metric_index;
                uint32 metric;
                uint32 width;
                uint32 height;

                w_u8(output, 2u);
                w_u8(output + 1u, 1u);
                metric_index = text_find_char_index(text_bind(text), text_bind(metrics_characters));
                w_u32(output + 4u, r_u32(shape));
                w_u32(output + 8u, r_u32(shape + 4u));
                w_u32(output + 12u, r_u32(shape + 8u));
                w_u32(output + 16u, r_u32(shape + 12u));
                w_u32(output + 20u, r_u32(shape + 16u));
                w_u16(output + 12u, (uint16)(x + advance));
                w_u16(output + 14u, (uint16)y);
                metric = metrics_base + (uint32)((sint16)metric_index * 8);
                w_u8(output + 16u, (uint8)(r_u8(output + 16u) + r_u8(metric)));
                w_u8(output + 17u, (uint8)(r_u8(output + 17u) + r_u8(metric + 2u)));
                w_u16(output + 20u, r_u16(metric + 4u));
                height = r_u16(metric + 6u);
                width = r_u16(output + 20u);
                ++text;
                w_u16(output + 22u, (uint16)height);
                w_u16(output + 18u, r_u16(descriptor + 28u));
                ++character_count;
                w_u32(output + 24u, r_u32(color));
                last_output = output;
                last_output_valid = 1;
                w_u32(output + 28u, r_u32(color + 4u));
                advance += width - 2u;
                w_u32(output + 32u, r_u32(color + 8u));
                w_u16(0x800B6ACEu, (uint16)(r_u16(0x800B6ACEu) + 1u));
                character = r_u8(text);
                output += 36u;
            }
            w_u16(0x800B6BC4u, (uint16)(r_u16(0x800B6BC4u) + character_count));
            {
                sint32 alignment = (sint16)r_u16(descriptor + 14u);

                if (alignment == 0)
                {
                    sint32 signed_advance = (sint16)advance;

                    signed_advance += (sint32)(((uint32)advance << 16) >> 31);
                    advance = (uint32)signed_advance >> 1;
                }
                else if (alignment == 1)
                    advance = 0u;
            }
            if (!last_output_valid)
                abort();
            w_u8(last_output, 4u);
            for (index = 0; index < character_count; ++index)
            {
                w_u16(output_start + 12u, (uint16)(r_u16(output_start + 12u) - advance));
                output_start += 36u;
            }
        }
        descriptor_index = (sint16)(descriptor_index + 1);
        descriptor_count = (sint16)r_u16(0x800B6A88u);
    } while (descriptor_index < descriptor_count);
}
