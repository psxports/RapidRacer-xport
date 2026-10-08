#include "polygon.h"
#include "global.h"
#include "mesh.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

static sint32 poly_abs_sint32(sint32 value)
{
    return value < 0 ? -value : value;
}

static uint32 poly_screen_outcode(sint32 screen, sint32 width, sint32 height)
{
    sint32 x = (sint16)(uint16)screen;
    sint32 y = (sint16)(uint16)((uint32)screen >> 16);
    uint32 result = 0u;

    if (x < 0)
        result |= 1u;
    if (x >= width)
        result |= 2u;
    if (y < 0)
        result |= 4u;
    if (y >= height)
        result |= 8u;
    return result;
}

static sint32 poly_project_prim(uint32 vertices, sint32 vertex_count, sint32 *screen, sint32 *depth, sint32 cull_backface)
{
    sint32 flags = 0;
    sint32 index;

    for (index = 0; index < vertex_count; ++index)
        depth[index] = gte_project_full_depth((SVECTOR *)psx_addr(vertices + 8u * (uint32)index, sizeof(SVECTOR)), &screen[index], &flags);
    if (cull_backface != 0 && NormalClip(screen[0], screen[1], screen[2]) <= 0)
        return 0;
    return 1;
}

static sint32 poly_needs_subdivision(const sint32 *screen, sint32 vertex_count, sint32 depth, uint32 depth_limit)
{
    sint32 x_perimeter = 0;
    sint32 y_perimeter = 0;
    sint32 index;

    if ((uint32)depth >= depth_limit)
        return 0;
    for (index = 0; index < vertex_count; ++index)
    {
        sint32 next = (index + 1) % vertex_count;
        x_perimeter += poly_abs_sint32((sint16)(uint16)screen[index] - (sint16)(uint16)screen[next]);
        y_perimeter += poly_abs_sint32((sint16)(uint16)((uint32)screen[index] >> 16) - (sint16)(uint16)((uint32)screen[next] >> 16));
    }
    return y_perimeter >= 1024 || x_perimeter >= 2048;
}

static uint32 poly_order_bucket(uint32 ordering_table, sint32 relative_depth)
{
    sint32 bucket_index = relative_depth < 0 ? -1 : relative_depth >> 5;

    return ordering_table + 4u + (uint32)(bucket_index * 4);
}

static uint32 poly_subdivide_prim(sint32 kind, uint32 primitive, uint32 destination)
{
    if (kind == 0)
        return poly_subdivide_quad_tex(primitive, destination);
    if (kind == 1)
        return poly_subdivide_quad_flat(primitive, destination);
    if (kind == 2)
        return (uint32)poly_subdivide_tri_flat(primitive, destination);
    if (kind == 3)
        return (uint32)poly_subdivide_quad_shaded(primitive, destination);
    if (kind == 4)
        return (uint32)poly_subdivide_tri_shaded(primitive, destination);
    if (kind == 5)
        return (uint32)poly_subdivide_tri_tex(primitive, destination);
    if (kind == 6)
        return (uint32)poly_subdivide_quad_tex_shaded(primitive, destination);
    return (uint32)poly_subdivide_tri_tex_shaded(primitive, destination);
}

static void render_subdividable_prims(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table, sint32 vertex_count, uint32 primitive_stride, uint32 screen_base, uint32 screen_step, uint32 packet_code, sint32 subdivision_kind, uint32 subdivision_size, sint32 subdivision_passes, sint32 bias_from_vertex, uint32 bias_offset, sint32 cull_backface, uint32 subdivision_depth_limit, sint32 limits_before_bias)
{
    sint32 index;

    if (count <= 0)
        return;
    primitives &= 0x00FFFFFFu;
    for (index = 0; index < count; ++index, vertices += 8u * (uint32)vertex_count, primitives += primitive_stride)
    {
        sint32 screen[4], depth[4];
        sint32 average, relative_depth;
        uint32 outcode;
        uint32 bucket;
        sint32 vertex;

        if (!poly_project_prim(vertices, vertex_count, screen, depth, cull_backface))
            continue;
        outcode = poly_screen_outcode(screen[0], (sint32)r_u32(0x800B3DF8u), (sint32)r_u32(0x800B3DFCu));
        for (vertex = 1; vertex < vertex_count; ++vertex)
            outcode &= poly_screen_outcode(screen[vertex], (sint32)r_u32(0x800B3DF8u), (sint32)r_u32(0x800B3DFCu));
        if (outcode != 0u)
            continue;
        average = vertex_count == 3 ? AverageZ3(depth[0], depth[1], depth[2]) : AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        relative_depth = average - 1 - (sint32)(uint16)r_u16(bias_from_vertex != 0 ? vertices + bias_offset : primitives + bias_offset);
        sint32 limit_depth = limits_before_bias != 0 ? average - 1 : relative_depth;

        if ((uint32)limit_depth >= 0xC60u)
            continue;
        bucket = poly_order_bucket(ordering_table, relative_depth);
        if (poly_needs_subdivision(screen, vertex_count, limit_depth, subdivision_depth_limit))
        {
            uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
            uint32 next_packet;
            uint32 offset;
            sint32 pass;

            for (offset = 0u; offset < primitive_stride; offset += 4u)
                w_u32(packet + offset, r_u32(primitives + offset));
            w_u32(packet, (r_u32(bucket) & 0x00FFFFFFu) | packet_code);
            w_u32(bucket, packet);
            for (vertex = 0; vertex < vertex_count; ++vertex)
                w_u32(packet + screen_base + screen_step * (uint32)vertex, (uint32)screen[vertex]);
            next_packet = poly_subdivide_prim(subdivision_kind, packet, packet + primitive_stride);
            for (pass = 1; pass < subdivision_passes; ++pass)
            {
                uint32 source = next_packet - subdivision_size * (uint32)(pass + 1) / 2u;
                next_packet = poly_subdivide_prim(subdivision_kind, source, next_packet);
            }
            w_u32(0x800B6C00u, next_packet);
        }
        else
        {
            w_u32(primitives, (r_u32(bucket) & 0x00FFFFFFu) | packet_code);
            w_u32(bucket, primitives);
            for (vertex = 0; vertex < vertex_count; ++vertex)
                w_u32(primitives + screen_base + screen_step * (uint32)vertex, (uint32)screen[vertex]);
        }
    }
}

void poly_dispatch_prim_groups(uint32 groups, uint32 ordering_table, sint32 adjusted, sint32 alternate)
{
    uint32 count;
    uint32 vertices;
    uint32 primitives;

    if (adjusted != 0)
    {
        count = r_u32(groups);
        vertices = r_u32(groups + 4u);
        primitives = r_u32(groups + 8u) + 20u * count;
        (alternate != 0 ? poly_render_tri_alt_flat_strip : poly_render_tri_flat_strip)(vertices, primitives, (sint32)count, ordering_table);
        count = r_u32(groups + 12u);
        vertices = r_u32(groups + 16u);
        primitives = r_u32(groups + 20u) + 24u * count;
        (alternate != 0 ? poly_render_quad_alt_flat_strip : poly_render_quad_flat_strip)(vertices, primitives, (sint32)count, ordering_table);
        count = r_u32(groups + 48u);
        vertices = r_u32(groups + 52u);
        primitives = r_u32(groups + 56u) + 28u * count;
        (alternate != 0 ? poly_render_tri_alt_shaded_strip : poly_render_tri_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
        count = r_u32(groups + 60u);
        vertices = r_u32(groups + 64u);
        primitives = r_u32(groups + 68u) + 36u * count;
        (alternate != 0 ? poly_render_quad_alt_shaded_strip : poly_render_quad_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
        vertices = r_u32(groups + 28u);
        count = r_u32(groups + 24u);
        primitives = r_u32(groups + 32u) + 32u * count;
        (alternate != 0 ? poly_render_tri_alt_tex_strip : poly_render_tri_tex_strip)(vertices, primitives, (sint32)count, ordering_table);
        if (alternate != 0)
        {
            count = r_u32(groups + 36u);
            vertices = r_u32(groups + 40u);
            primitives = r_u32(groups + 44u) + 40u * count;
            poly_render_quad_alt_tex_strip(vertices, primitives, (sint32)count, ordering_table);
        }
        else
        {
            uint32 mode = r_u32(0x800834A0u);

            count = r_u32(groups + 36u);
            vertices = r_u32(groups + 40u);
            primitives = r_u32(groups + 44u) + 40u * count;
            if (mode == 1u)
                poly_render_quad_clipped_tex_strip(vertices, primitives, (sint32)count, ordering_table);
            else
                poly_render_quad_tex_strip(vertices, primitives, (sint32)count, ordering_table);
        }
        count = r_u32(groups + 72u);
        vertices = r_u32(groups + 76u);
        primitives = r_u32(groups + 80u) + 40u * count;
        (alternate != 0 ? poly_render_tri_mixed_strip : poly_render_tri_tex_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
        count = r_u32(groups + 84u);
        vertices = r_u32(groups + 88u);
        primitives = r_u32(groups + 92u) + 52u * count;
        (alternate != 0 ? poly_render_quad_mixed_strip : poly_render_quad_tex_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
        return;
    }

    vertices = r_u32(groups + 4u);
    primitives = r_u32(groups + 8u);
    count = r_u32(groups);
    (alternate != 0 ? poly_render_tri_alt_flat_strip : poly_render_tri_flat_strip)(vertices, primitives, (sint32)count, ordering_table);
    vertices = r_u32(groups + 16u);
    primitives = r_u32(groups + 20u);
    count = r_u32(groups + 12u);
    (alternate != 0 ? poly_render_quad_alt_flat_strip : poly_render_quad_flat_strip)(vertices, primitives, (sint32)count, ordering_table);
    vertices = r_u32(groups + 52u);
    primitives = r_u32(groups + 56u);
    count = r_u32(groups + 48u);
    (alternate != 0 ? poly_render_tri_alt_shaded_strip : poly_render_tri_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
    vertices = r_u32(groups + 64u);
    primitives = r_u32(groups + 68u);
    count = r_u32(groups + 60u);
    (alternate != 0 ? poly_render_quad_alt_shaded_strip : poly_render_quad_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
    vertices = r_u32(groups + 28u);
    primitives = r_u32(groups + 32u);
    count = r_u32(groups + 24u);
    (alternate != 0 ? poly_render_tri_alt_tex_strip : poly_render_tri_tex_strip)(vertices, primitives, (sint32)count, ordering_table);
    if (alternate != 0)
    {
        vertices = r_u32(groups + 40u);
        primitives = r_u32(groups + 44u);
        count = r_u32(groups + 36u);
        poly_render_quad_alt_tex_strip(vertices, primitives, (sint32)count, ordering_table);
    }
    else
    {
        uint32 mode = r_u32(0x800834A0u);

        vertices = r_u32(groups + 40u);
        primitives = r_u32(groups + 44u);
        count = r_u32(groups + 36u);
        if (mode == 1u)
            poly_render_quad_clipped_tex_strip(vertices, primitives, (sint32)count, ordering_table);
        else
            poly_render_quad_tex_strip(vertices, primitives, (sint32)count, ordering_table);
    }
    vertices = r_u32(groups + 76u);
    primitives = r_u32(groups + 80u);
    count = r_u32(groups + 72u);
    (alternate != 0 ? poly_render_tri_mixed_strip : poly_render_tri_tex_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
    vertices = r_u32(groups + 88u);
    primitives = r_u32(groups + 92u);
    count = r_u32(groups + 84u);
    (alternate != 0 ? poly_render_quad_mixed_strip : poly_render_quad_tex_shaded_strip)(vertices, primitives, (sint32)count, ordering_table);
}

void poly_emit_object_packet_group(uint32 descriptor, uint32 projected, uint32 ordering_entry, uint32 *indices, uint32 *primitive, const RR_OBJECT_PACKET_LAYOUT *layout)
{
    sint32 count = (sint32)r_u32(descriptor + layout->count_offset);

    while (count > 0)
    {
        uint32 packed_indices = r_u32(*indices);
        sint32 screens[3];

        *indices += 4u;
        screens[0] = (sint32)r_u32(projected + 4u * (packed_indices & 0xffu));
        screens[1] = (sint32)r_u32(projected + 4u * ((packed_indices >> 8) & 0xffu));
        screens[2] = (sint32)r_u32(projected + 4u * ((packed_indices >> 16) & 0xffu));
        if (NormalClip(screens[0], screens[1], screens[2]) > 0)
        {
            w_u32(*primitive + layout->screen_offset[0], (uint32)screens[0]);
            w_u32(*primitive + layout->screen_offset[1], (uint32)screens[1]);
            w_u32(*primitive + layout->screen_offset[2], (uint32)screens[2]);
            if (layout->fourth_offset != 0u)
                w_u32(*primitive + layout->fourth_offset, r_u32(projected + 4u * (packed_indices >> 24)));
            w_u32(*primitive, r_u32(ordering_entry) | layout->command);
            w_u32(ordering_entry, *primitive);
        }
        *primitive += layout->stride;
        --count;
    }
}

static sint32 poly_project_mesh_vertex(uint32 record, sint32 origin_x, sint32 origin_y, sint32 origin_z, sint32 *screen)
{
    SVECTOR point;
    sint32 flags;

    point.vx = (sint16)(origin_x + 16 * r_u8(record));
    point.vy = (sint16)(origin_y + 16 * r_u8(record + 1u));
    point.vz = (sint16)(origin_z + 16 * r_u8(record + 2u));
    point.pad = 0;
    return gte_project_full_depth(&point, screen, &flags);
}

static uint16 mesh_texture_coordinate(uint32 record, uint32 phase)
{
    uint32 index = (2u * r_u8(record + 7u) + phase) & 0x1FEu;

    return (uint16)(r_u8(0x800F0908u + index) + 0x40u);
}

static uint32 poly_anim_vertex_color(uint32 palette, sint32 factor)
{
    uint32 result = 0x3C000000u;
    uint32 shift;

    for (shift = 0u; shift < 24u; shift += 8u)
    {
        sint32 value = factor * (sint32)((palette >> shift) & 0xFFu) >> 12;

        if (value < 0)
            value = 0;
        if (value > 255)
            value = 255;
        result |= (uint32)value << shift;
    }
    return result;
}

sint32 poly_subdivide_tri_shaded(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 midpoint0;
    uint32 midpoint1;
    uint32 midpoint2;
    uint32 tag;
    uint8 shade;
    sint32 result;

    FUNCTION_MARKER(0x80079CACu, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    shade = r_u8(primitive + 7u);
    w_u8(primitive + 15u, shade);
    w_u8(primitive + 23u, shade);
    frame = guest_stack_push(0x18u);
    midpoint0 = frame;
    midpoint1 = frame + 8u;
    midpoint2 = frame + 16u;
    tag = r_u32(primitive);
    poly_tri_shaded_verts_avg(primitive + 4u, primitive + 12u, midpoint0, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_tri_shaded_verts_avg(primitive + 12u, primitive + 20u, midpoint1, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_tri_shaded_verts_avg(primitive + 20u, primitive + 4u, midpoint2, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_copy_tri_shaded_pkt_planes(primitive + 4u, destination, tag, midpoint0, midpoint2);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_tri_shaded_pkt_planes(midpoint2, destination + 28u, tag, midpoint1, primitive + 20u);
    tag = (tag & 0xFF000000u) | (destination + 28u);
    poly_copy_tri_shaded_pkt_planes(midpoint1, destination + 56u, tag, midpoint2, midpoint0);
    tag = (tag & 0xFF000000u) | (destination + 56u);
    poly_copy_tri_shaded_pkt_planes(midpoint0, primitive, tag, primitive + 12u, midpoint1);
    result = (sint32)(destination + 84u);
    guest_stack_pop(0x18u);
    return result;
}

void poly_copy_tri_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third)
{
    uint32 first_second;
    uint32 second_second;
    uint32 third_second;
    uint32 first_first;
    uint32 second_first;
    uint32 third_first;

    FUNCTION_MARKER(0x80079D64u, "MAIN.EXE");
    first_second = r_u32(first + 4u);
    second_second = r_u32(second + 4u);
    third_second = r_u32(third + 4u);
    w_u32(destination + 8u, first_second);
    w_u32(destination + 16u, second_second);
    w_u32(destination + 24u, third_second);
    first_first = r_u32(first);
    second_first = r_u32(second);
    third_first = r_u32(third);
    w_u32(destination + 4u, first_first);
    w_u32(destination + 12u, second_first);
    w_u32(destination + 20u, third_first);
    w_u32(destination, tag);
}

void poly_tri_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask)
{
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x80079DA8u, "MAIN.EXE");
    left = r_u32(first);
    right = r_u32(second);
    w_u32(destination, (left + right - ((left ^ right) & first_mask)) >> 1);
    left = r_u32(first + 4u);
    right = r_u32(second + 4u);
    w_u32(destination + 4u, (left + right - ((left ^ right) & second_mask)) >> 1);
}

sint32 poly_subdivide_tri_tex_shaded(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 midpoint0;
    uint32 midpoint1;
    uint32 midpoint2;
    uint32 tag;
    uint32 first_flag;
    uint32 second_flag;
    uint8 shade;
    sint32 result;

    FUNCTION_MARKER(0x80079DF4u, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    shade = r_u8(primitive + 7u);
    w_u8(primitive + 19u, shade);
    w_u8(primitive + 31u, shade);
    frame = guest_stack_push(0x24u);
    midpoint0 = frame;
    midpoint1 = frame + 12u;
    midpoint2 = frame + 24u;
    tag = r_u32(primitive);
    poly_tri_tex_shaded_verts_avg(primitive + 4u, primitive + 16u, midpoint0, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_tri_tex_shaded_verts_avg(primitive + 16u, primitive + 28u, midpoint1, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_tri_tex_shaded_verts_avg(primitive + 28u, primitive + 4u, midpoint2, UINT32_C(0x01010101), UINT32_C(0x00010001));
    first_flag = (uint32)r_u16(primitive + 14u) << 16;
    second_flag = (uint32)r_u16(primitive + 26u) << 16;
    poly_copy_tri_tex_shaded_pkt_planes(primitive + 4u, destination, tag, midpoint0, midpoint2, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_tri_tex_shaded_pkt_planes(midpoint2, destination + 40u, tag, midpoint1, primitive + 28u, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 40u);
    poly_copy_tri_tex_shaded_pkt_planes(midpoint1, destination + 80u, tag, midpoint2, midpoint0, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 80u);
    poly_copy_tri_tex_shaded_pkt_planes(midpoint0, primitive, tag, primitive + 16u, midpoint1, first_flag, second_flag);
    result = (sint32)(destination + 120u);
    guest_stack_pop(0x24u);
    return result;
}

void poly_copy_tri_tex_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 first_flag, uint32 second_flag)
{
    uint32 first_second;
    uint32 second_second;
    uint32 third_second;
    uint32 first_first;
    uint32 second_first;
    uint32 third_first;
    uint16 first_last;
    uint16 second_last;
    uint16 third_last;

    FUNCTION_MARKER(0x80079EBCu, "MAIN.EXE");
    first_second = r_u32(first + 4u);
    second_second = r_u32(second + 4u);
    third_second = r_u32(third + 4u);
    w_u32(destination + 8u, first_second);
    w_u32(destination + 20u, second_second);
    w_u32(destination + 32u, third_second);
    first_first = r_u32(first);
    second_first = r_u32(second);
    third_first = r_u32(third);
    w_u32(destination + 4u, first_first);
    w_u32(destination + 16u, second_first);
    w_u32(destination + 28u, third_first);
    first_last = r_u16(first + 8u);
    second_last = r_u16(second + 8u);
    third_last = r_u16(third + 8u);
    w_u32(destination + 12u, first_last | first_flag);
    w_u32(destination + 24u, second_last | second_flag);
    w_u16(destination + 36u, third_last);
    w_u32(destination, tag);
}

void poly_tri_tex_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask)
{
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x80079F20u, "MAIN.EXE");
    left = r_u32(first);
    right = r_u32(second);
    w_u32(destination, (left + right - ((left ^ right) & first_mask)) >> 1);
    left = r_u32(first + 4u);
    right = r_u32(second + 4u);
    w_u32(destination + 4u, (left + right - ((left ^ right) & second_mask)) >> 1);
    left = r_u16(first + 8u);
    right = r_u16(second + 8u);
    w_u16(destination + 8u, (uint16)((left + right - ((left ^ right) & first_mask)) >> 1));
}

sint32 poly_subdivide_tri_flat(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 midpoint0;
    uint32 midpoint1;
    uint32 midpoint2;
    uint32 tag;
    sint32 result;

    FUNCTION_MARKER(0x80079F90u, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    frame = guest_stack_push(0x0Cu);
    midpoint0 = frame;
    midpoint1 = frame + 4u;
    midpoint2 = frame + 8u;
    tag = r_u32(primitive);
    poly_tri_flat_verts_avg(primitive + 8u, primitive + 12u, midpoint0, UINT32_C(0x00010001));
    poly_tri_flat_verts_avg(primitive + 12u, primitive + 16u, midpoint1, UINT32_C(0x00010001));
    poly_tri_flat_verts_avg(primitive + 16u, primitive + 8u, midpoint2, UINT32_C(0x00010001));
    poly_copy_tri_flat_verts(primitive, primitive + 8u, destination, tag, midpoint0, midpoint2);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_tri_flat_verts(primitive, midpoint2, destination + 20u, tag, midpoint1, primitive + 16u);
    tag = (tag & 0xFF000000u) | (destination + 20u);
    poly_copy_tri_flat_verts(primitive, midpoint1, destination + 40u, tag, midpoint2, midpoint0);
    tag = (tag & 0xFF000000u) | (destination + 40u);
    poly_copy_tri_flat_verts(primitive, midpoint0, primitive, tag, primitive + 12u, midpoint1);
    result = (sint32)(destination + 60u);
    guest_stack_pop(0x0Cu);
    return result;
}

void poly_copy_tri_flat_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third)
{
    uint32 first_value;
    uint32 second_value;
    uint32 third_value;
    uint32 color;

    FUNCTION_MARKER(0x8007A038u, "MAIN.EXE");
    first_value = r_u32(first);
    second_value = r_u32(second);
    third_value = r_u32(third);
    w_u32(destination + 8u, first_value);
    w_u32(destination + 12u, second_value);
    w_u32(destination + 16u, third_value);
    color = r_u32(primitive + 4u);
    w_u32(destination, tag);
    w_u32(destination + 4u, color);
}

void poly_tri_flat_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 mask)
{
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x8007A06Cu, "MAIN.EXE");
    left = r_u32(first);
    right = r_u32(second);
    w_u32(destination, (left + right - ((left ^ right) & mask)) >> 1);
}

sint32 poly_subdivide_tri_tex(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 midpoint0;
    uint32 midpoint1;
    uint32 midpoint2;
    uint32 tag;
    uint32 first_flag;
    uint32 second_flag;
    sint32 result;

    FUNCTION_MARKER(0x8007A094u, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    frame = guest_stack_push(0x18u);
    midpoint0 = frame;
    midpoint1 = frame + 8u;
    midpoint2 = frame + 16u;
    tag = r_u32(primitive);
    poly_tri_tex_verts_avg(primitive + 8u, primitive + 16u, midpoint0, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_tri_tex_verts_avg(primitive + 16u, primitive + 24u, midpoint1, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_tri_tex_verts_avg(primitive + 24u, primitive + 8u, midpoint2, UINT32_C(0x00010001), UINT32_C(0x01010101));
    first_flag = (uint32)r_u16(primitive + 14u) << 16;
    second_flag = (uint32)r_u16(primitive + 22u) << 16;
    poly_copy_tri_tex_verts(primitive, primitive + 8u, destination, tag, midpoint0, midpoint2, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_tri_tex_verts(primitive, midpoint2, destination + 32u, tag, midpoint1, primitive + 24u, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 32u);
    poly_copy_tri_tex_verts(primitive, midpoint1, destination + 64u, tag, midpoint2, midpoint0, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 64u);
    poly_copy_tri_tex_verts(primitive, midpoint0, primitive, tag, primitive + 16u, midpoint1, first_flag, second_flag);
    result = (sint32)(destination + 96u);
    guest_stack_pop(0x18u);
    return result;
}

void poly_copy_tri_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 first_flag, uint32 second_flag)
{
    uint32 first_word;
    uint32 second_word;
    uint32 third_word;
    uint16 first_half;
    uint16 second_half;
    uint16 third_half;
    uint32 color;

    FUNCTION_MARKER(0x8007A14Cu, "MAIN.EXE");
    first_word = r_u32(first);
    second_word = r_u32(second);
    third_word = r_u32(third);
    w_u32(destination + 8u, first_word);
    w_u32(destination + 16u, second_word);
    w_u32(destination + 24u, third_word);
    first_half = r_u16(first + 4u);
    second_half = r_u16(second + 4u);
    third_half = r_u16(third + 4u);
    w_u32(destination + 12u, first_half | first_flag);
    w_u32(destination + 20u, second_half | second_flag);
    w_u16(destination + 28u, third_half);
    color = r_u32(primitive + 4u);
    w_u32(destination, tag);
    w_u32(destination + 4u, color);
}

void poly_tri_tex_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 word_mask, uint32 half_mask)
{
    uint32 left = r_u32(first);
    uint32 right = r_u32(second);

    FUNCTION_MARKER(0x8007A1A0u, "MAIN.EXE");
    w_u32(destination, (left + right - ((left ^ right) & word_mask)) >> 1);
    left = r_u16(first + 4u);
    right = r_u16(second + 4u);
    w_u16(destination + 4u, (uint16)((left + right - ((left ^ right) & half_mask)) >> 1));
}

sint32 poly_subdivide_quad_shaded(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 m0;
    uint32 m1;
    uint32 m2;
    uint32 m3;
    uint32 center;
    uint32 tag;
    uint8 shade;
    sint32 result;

    FUNCTION_MARKER(0x8007A1ECu, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    shade = r_u8(primitive + 7u);
    w_u8(primitive + 15u, shade);
    w_u8(primitive + 23u, shade);
    w_u8(primitive + 31u, shade);
    frame = guest_stack_push(0x28u);
    m0 = frame;
    m1 = frame + 8u;
    m2 = frame + 16u;
    m3 = frame + 24u;
    center = frame + 32u;
    tag = r_u32(primitive);
    poly_quad_shaded_verts_avg(primitive + 4u, primitive + 12u, m0, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_quad_shaded_verts_avg(primitive + 12u, primitive + 28u, m1, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_quad_shaded_verts_avg(primitive + 28u, primitive + 20u, m2, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_quad_shaded_verts_avg(primitive + 20u, primitive + 4u, m3, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_quad_shaded_verts_avg(m1, m3, center, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_copy_quad_shaded_pkt_planes(primitive + 4u, destination, tag, m0, m3, center);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_quad_shaded_pkt_planes(m0, destination + 36u, tag, primitive + 12u, center, m1);
    tag = (tag & 0xFF000000u) | (destination + 36u);
    poly_copy_quad_shaded_pkt_planes(center, destination + 72u, tag, m1, m2, primitive + 28u);
    tag = (tag & 0xFF000000u) | (destination + 72u);
    poly_copy_quad_shaded_pkt_planes(m3, primitive, tag, center, primitive + 20u, m2);
    result = (sint32)(destination + 108u);
    guest_stack_pop(0x28u);
    return result;
}

void poly_copy_quad_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth)
{
    uint32 first_second;
    uint32 second_second;
    uint32 third_second;
    uint32 fourth_second;
    uint32 first_first;
    uint32 second_first;
    uint32 third_first;
    uint32 fourth_first;

    FUNCTION_MARKER(0x8007A2D8u, "MAIN.EXE");
    first_second = r_u32(first + 4u);
    second_second = r_u32(second + 4u);
    third_second = r_u32(third + 4u);
    fourth_second = r_u32(fourth + 4u);
    w_u32(destination + 8u, first_second);
    w_u32(destination + 16u, second_second);
    w_u32(destination + 24u, third_second);
    w_u32(destination + 32u, fourth_second);
    first_first = r_u32(first);
    second_first = r_u32(second);
    third_first = r_u32(third);
    fourth_first = r_u32(fourth);
    w_u32(destination + 4u, first_first);
    w_u32(destination + 12u, second_first);
    w_u32(destination + 20u, third_first);
    w_u32(destination + 28u, fourth_first);
    w_u32(destination, tag);
}

void poly_quad_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask)
{
    FUNCTION_MARKER(0x8007A32Cu, "MAIN.EXE");
    poly_tri_shaded_verts_avg(first, second, destination, first_mask, second_mask);
}

sint32 poly_subdivide_quad_tex_shaded(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 m0;
    uint32 m1;
    uint32 m2;
    uint32 m3;
    uint32 center;
    uint32 tag;
    uint32 first_flag;
    uint32 second_flag;
    uint8 shade;
    sint32 result;

    FUNCTION_MARKER(0x8007A378u, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    shade = r_u8(primitive + 7u);
    w_u8(primitive + 19u, shade);
    w_u8(primitive + 31u, shade);
    w_u8(primitive + 43u, shade);
    frame = guest_stack_push(0x3Cu);
    m0 = frame;
    m1 = frame + 12u;
    m2 = frame + 24u;
    m3 = frame + 36u;
    center = frame + 48u;
    tag = r_u32(primitive);
    poly_masked_vert_pairs_avg(primitive + 4u, primitive + 16u, m0, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_masked_vert_pairs_avg(primitive + 16u, primitive + 40u, m1, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_masked_vert_pairs_avg(primitive + 40u, primitive + 28u, m2, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_masked_vert_pairs_avg(primitive + 28u, primitive + 4u, m3, UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_masked_vert_pairs_avg(m1, m3, center, UINT32_C(0x01010101), UINT32_C(0x00010001));
    first_flag = (uint32)r_u16(primitive + 14u) << 16;
    second_flag = (uint32)r_u16(primitive + 26u) << 16;
    poly_copy_pkt_subdiv_attrs(primitive + 4u, destination, tag, m0, m3, center, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_pkt_subdiv_attrs(m0, destination + 52u, tag, primitive + 16u, center, m1, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 52u);
    poly_copy_pkt_subdiv_attrs(center, destination + 104u, tag, m1, m2, primitive + 40u, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 104u);
    poly_copy_pkt_subdiv_attrs(m3, primitive, tag, center, primitive + 28u, m2, first_flag, second_flag);
    result = (sint32)(destination + 156u);
    guest_stack_pop(0x3Cu);
    return result;
}

void poly_copy_pkt_subdiv_attrs(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth, uint32 first_flag, uint32 second_flag)
{
    uint32 first_second;
    uint32 second_second;
    uint32 third_second;
    uint32 fourth_second;
    uint32 first_first;
    uint32 second_first;
    uint32 third_first;
    uint32 fourth_first;
    uint16 first_last;
    uint16 second_last;
    uint16 third_last;
    uint16 fourth_last;

    FUNCTION_MARKER(0x8007A474u, "MAIN.EXE");
    first_second = r_u32(first + 4u);
    second_second = r_u32(second + 4u);
    third_second = r_u32(third + 4u);
    fourth_second = r_u32(fourth + 4u);
    w_u32(destination + 8u, first_second);
    w_u32(destination + 20u, second_second);
    w_u32(destination + 32u, third_second);
    w_u32(destination + 44u, fourth_second);
    first_first = r_u32(first);
    second_first = r_u32(second);
    third_first = r_u32(third);
    fourth_first = r_u32(fourth);
    w_u32(destination + 4u, first_first);
    w_u32(destination + 16u, second_first);
    w_u32(destination + 28u, third_first);
    w_u32(destination + 40u, fourth_first);
    first_last = r_u16(first + 8u);
    second_last = r_u16(second + 8u);
    third_last = r_u16(third + 8u);
    fourth_last = r_u16(fourth + 8u);
    w_u32(destination + 12u, first_last | first_flag);
    w_u32(destination + 24u, second_last | second_flag);
    w_u16(destination + 36u, third_last);
    w_u16(destination + 48u, fourth_last);
    w_u32(destination, tag);
}

void poly_masked_vert_pairs_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask)
{
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x8007A4F0u, "MAIN.EXE");
    left = r_u32(first);
    right = r_u32(second);
    w_u32(destination, (left + right - ((left ^ right) & first_mask)) >> 1);
    left = r_u32(first + 4u);
    right = r_u32(second + 4u);
    w_u32(destination + 4u, (left + right - ((left ^ right) & second_mask)) >> 1);
    left = r_u16(first + 8u);
    right = r_u16(second + 8u);
    w_u16(destination + 8u, (uint16)((left + right - ((left ^ right) & first_mask)) >> 1));
}

uint32 poly_subdivide_quad_flat(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 m0;
    uint32 m1;
    uint32 m2;
    uint32 m3;
    uint32 center;
    uint32 tag;
    uint32 result;

    FUNCTION_MARKER_ARGS(0x8007A560u, "MAIN.EXE", XPORT_CALL_VALUE_GUEST_POINTER, 2u, XPORT_CALL_GUEST_POINTER(primitive, 24u), XPORT_CALL_GUEST_POINTER(destination, 72u));
    destination &= 0x00FFFFFFu;
    frame = guest_stack_push(0x14u);
    m0 = frame;
    m1 = frame + 4u;
    m2 = frame + 8u;
    m3 = frame + 12u;
    center = frame + 16u;
    tag = r_u32(primitive);
    poly_coord_packed_avg(primitive + 8u, primitive + 12u, m0, UINT32_C(0x00010001));
    poly_coord_packed_avg(primitive + 12u, primitive + 20u, m1, UINT32_C(0x00010001));
    poly_coord_packed_avg(primitive + 20u, primitive + 16u, m2, UINT32_C(0x00010001));
    poly_coord_packed_avg(primitive + 16u, primitive + 8u, m3, UINT32_C(0x00010001));
    poly_coord_packed_avg(m1, m3, center, UINT32_C(0x00010001));
    poly_copy_prim_tex_verts(primitive, primitive + 8u, destination, tag, m0, m3, center);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_prim_tex_verts(primitive, m0, destination + 24u, tag, primitive + 12u, center, m1);
    tag = (tag & 0xFF000000u) | (destination + 24u);
    poly_copy_prim_tex_verts(primitive, center, destination + 48u, tag, m1, m2, primitive + 20u);
    tag = (tag & 0xFF000000u) | (destination + 48u);
    poly_copy_prim_tex_verts(primitive, m3, primitive, tag, center, primitive + 16u, m2);
    result = destination + 72u;
    guest_stack_pop(0x14u);
    return result;
}

void poly_copy_prim_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth)
{
    uint32 first_value = r_u32(first);
    uint32 second_value = r_u32(second);
    uint32 third_value = r_u32(third);
    uint32 fourth_value = r_u32(fourth);
    uint32 primitive_value;

    FUNCTION_MARKER(0x8007A638u, "MAIN.EXE");
    w_u32(destination + 8u, first_value);
    w_u32(destination + 12u, second_value);
    w_u32(destination + 16u, third_value);
    w_u32(destination + 20u, fourth_value);
    primitive_value = r_u32(primitive + 4u);
    w_u32(destination, tag);
    w_u32(destination + 4u, primitive_value);
}

void poly_coord_packed_avg(uint32 first, uint32 second, uint32 destination, uint32 mask)
{
    FUNCTION_MARKER(0x8007A674u, "MAIN.EXE");
    poly_tri_flat_verts_avg(first, second, destination, mask);
}

uint32 poly_subdivide_quad_tex(uint32 primitive, uint32 destination)
{
    uint32 frame;
    uint32 m0;
    uint32 m1;
    uint32 m2;
    uint32 m3;
    uint32 center;
    uint32 tag;
    uint32 first_flag;
    uint32 second_flag;
    uint32 result;

    FUNCTION_MARKER(0x8007A69Cu, "MAIN.EXE");
    destination &= 0x00FFFFFFu;
    frame = guest_stack_push(0x28u);
    m0 = frame;
    m1 = frame + 8u;
    m2 = frame + 16u;
    m3 = frame + 24u;
    center = frame + 32u;
    tag = r_u32(primitive);
    poly_quad_tex_verts_avg(primitive + 8u, primitive + 16u, m0, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_quad_tex_verts_avg(primitive + 16u, primitive + 32u, m1, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_quad_tex_verts_avg(primitive + 32u, primitive + 24u, m2, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_quad_tex_verts_avg(primitive + 24u, primitive + 8u, m3, UINT32_C(0x00010001), UINT32_C(0x01010101));
    poly_quad_tex_verts_avg(m1, m3, center, UINT32_C(0x00010001), UINT32_C(0x01010101));
    first_flag = (uint32)r_u16(primitive + 14u) << 16;
    second_flag = (uint32)r_u16(primitive + 22u) << 16;
    poly_copy_quad_tex_verts(primitive, primitive + 8u, destination, tag, m0, m3, center, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | destination;
    poly_copy_quad_tex_verts(primitive, m0, destination + 40u, tag, primitive + 16u, center, m1, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 40u);
    poly_copy_quad_tex_verts(primitive, center, destination + 80u, tag, m1, m2, primitive + 32u, first_flag, second_flag);
    tag = (tag & 0xFF000000u) | (destination + 80u);
    poly_copy_quad_tex_verts(primitive, m3, primitive, tag, center, primitive + 24u, m2, first_flag, second_flag);
    result = destination + 120u;
    guest_stack_pop(0x28u);
    return result;
}

void poly_copy_quad_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth, uint32 first_flag, uint32 second_flag)
{
    uint32 first_word;
    uint32 second_word;
    uint32 third_word;
    uint32 fourth_word;
    uint16 first_half;
    uint16 second_half;
    uint16 third_half;
    uint16 fourth_half;
    uint32 color;

    FUNCTION_MARKER(0x8007A784u, "MAIN.EXE");
    first_word = r_u32(first);
    second_word = r_u32(second);
    third_word = r_u32(third);
    fourth_word = r_u32(fourth);
    w_u32(destination + 8u, first_word);
    w_u32(destination + 16u, second_word);
    w_u32(destination + 24u, third_word);
    w_u32(destination + 32u, fourth_word);
    first_half = r_u16(first + 4u);
    second_half = r_u16(second + 4u);
    third_half = r_u16(third + 4u);
    fourth_half = r_u16(fourth + 4u);
    w_u32(destination + 12u, first_half | first_flag);
    w_u32(destination + 20u, second_half | second_flag);
    w_u16(destination + 28u, third_half);
    w_u16(destination + 36u, fourth_half);
    color = r_u32(primitive + 4u);
    w_u32(destination, tag);
    w_u32(destination + 4u, color);
}

void poly_quad_tex_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 word_mask, uint32 half_mask)
{
    FUNCTION_MARKER(0x8007A7E8u, "MAIN.EXE");
    poly_tri_tex_verts_avg(first, second, destination, word_mask, half_mask);
}

void poly_render_quad_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007A8A8u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 40u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 30u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;
                    uint32 next_packet;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    fourth_attribute = r_u32(primitive + 28u);
                    fifth = r_u32(primitive + 36u);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 36u, fifth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(packet + 32u, (uint32)previous_screen[3]);
                    next_packet = poly_subdivide_quad_tex(packet, packet + 40u);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0xA0u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0xF0u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0x140u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0x190u, next_packet);
                    w_u32(0x800B6C00u, next_packet);
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                    w_u32(primitive + 32u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007ACC4u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 32u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 30u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x07000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    fourth = r_u32(primitive + 28u);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 28u, fourth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_tex(packet, packet + 32u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x07000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
        }
    }
}

void poly_render_quad_tex_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007B014u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 52u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 38u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;
                    uint32 sixth;
                    uint32 seventh;
                    uint32 eighth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x0C000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 24u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 24u, third);
                    fourth_attribute = r_u32(primitive + 36u);
                    fifth = r_u32(primitive + 48u);
                    sixth = r_u32(primitive + 16u);
                    w_u32(packet + 36u, fourth_attribute);
                    w_u32(packet + 48u, fifth);
                    w_u32(packet + 16u, sixth);
                    seventh = r_u32(primitive + 28u);
                    eighth = r_u32(primitive + 40u);
                    w_u32(packet + 28u, seventh);
                    w_u32(packet + 40u, eighth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 20u, (uint32)previous_screen[1]);
                    w_u32(packet + 32u, (uint32)previous_screen[2]);
                    w_u32(packet + 44u, (uint32)previous_screen[3]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_tex_shaded(packet, packet + 52u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x0C000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 20u, (uint32)previous_screen[1]);
                    w_u32(primitive + 32u, (uint32)previous_screen[2]);
                    w_u32(primitive + 44u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_tex_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007B418u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 40u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 38u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth;
                    uint32 fifth;
                    uint32 sixth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 24u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 24u, third);
                    fourth = r_u32(primitive + 36u);
                    fifth = r_u32(primitive + 16u);
                    sixth = r_u32(primitive + 28u);
                    w_u32(packet + 36u, fourth);
                    w_u32(packet + 16u, fifth);
                    w_u32(packet + 28u, sixth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 20u, (uint32)previous_screen[1]);
                    w_u32(packet + 32u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_tex_shaded(packet, packet + 40u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 20u, (uint32)previous_screen[1]);
                    w_u32(primitive + 32u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
        }
    }
}

void poly_render_quad_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007B778u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 32u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 24u * (uint32)(iteration - 1);
            sint32 base_depth = previous_average - 1;
            sint32 relative_depth = base_depth - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)base_depth < 0xC60u)
            {
                bucket = poly_order_bucket(ordering_table, relative_depth);
                if (poly_needs_subdivision(previous_screen, 4, base_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 color;
                    uint32 next_packet;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x05000000u);
                    w_u32(bucket, packet);
                    color = r_u32(primitive + 4u);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 12u, (uint32)previous_screen[1]);
                    w_u32(packet + 16u, (uint32)previous_screen[2]);
                    w_u32(packet + 20u, (uint32)previous_screen[3]);
                    w_u32(packet + 4u, color);
                    next_packet = poly_subdivide_quad_flat(packet, packet + 24u);
                    next_packet = poly_subdivide_quad_flat(next_packet - 0x60u, next_packet);
                    next_packet = poly_subdivide_quad_flat(next_packet - 0x90u, next_packet);
                    next_packet = poly_subdivide_quad_flat(next_packet - 0xC0u, next_packet);
                    next_packet = poly_subdivide_quad_flat(next_packet - 0xF0u, next_packet);
                    w_u32(0x800B6C00u, next_packet);
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x05000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 12u, (uint32)previous_screen[1]);
                    w_u32(primitive + 16u, (uint32)previous_screen[2]);
                    w_u32(primitive + 20u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007BB90u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 24u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 20u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 color;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x04000000u);
                    w_u32(bucket, packet);
                    color = r_u32(primitive + 4u);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 12u, (uint32)previous_screen[1]);
                    w_u32(packet + 16u, (uint32)previous_screen[2]);
                    w_u32(packet + 4u, color);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_flat(packet, packet + 20u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x04000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 12u, (uint32)previous_screen[1]);
                    w_u32(primitive + 16u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
        }
    }
}

void poly_render_quad_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007BEC8u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 32u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 36u * (uint32)(iteration - 1);
            sint32 base_depth = previous_average - 1;
            sint32 relative_depth = base_depth - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)base_depth < 0xC60u)
            {
                bucket = poly_order_bucket(ordering_table, relative_depth);
                if (poly_needs_subdivision(previous_screen, 4, base_depth, 0x3C0u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x08000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    fourth_attribute = r_u32(primitive + 28u);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(packet + 32u, (uint32)previous_screen[3]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_shaded(packet, packet + 36u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x08000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                    w_u32(primitive + 32u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007C2C8u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 24u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 28u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x06000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_shaded(packet, packet + 28u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x06000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
        }
    }
}

void poly_render_quad_clipped_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007C610u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 40u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 30u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;
                    uint32 next_packet;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    fourth_attribute = r_u32(primitive + 28u);
                    fifth = r_u32(primitive + 36u);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 36u, fifth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(packet + 32u, (uint32)previous_screen[3]);
                    next_packet = poly_subdivide_quad_tex(packet, packet + 40u);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0xA0u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0xF0u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0x140u, next_packet);
                    next_packet = poly_subdivide_quad_tex(next_packet - 0x190u, next_packet);
                    w_u32(0x800B6C00u, next_packet);
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                    w_u32(primitive + 32u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = 0;
        if (current_cross > 0)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
            if ((((uint32)current_flag | (uint32)fourth_flag) >> 22 & 7u) == 0u)
            {
                previous_screen[0] = current_screen[0];
                previous_screen[1] = current_screen[1];
                previous_screen[2] = current_screen[2];
                previous_screen[3] = current_screen[3];
                previous_valid = 1;
            }
        }
    }
}

void poly_render_quad_alt_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007CA40u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 40u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 30u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    fourth_attribute = r_u32(primitive + 28u);
                    fifth = r_u32(primitive + 36u);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 36u, fifth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(packet + 32u, (uint32)previous_screen[3]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_tex(packet, packet + 40u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                    w_u32(primitive + 32u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = 1;
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_alt_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007CE20u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 32u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 30u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x07000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    fourth_attribute = r_u32(primitive + 28u);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_tex(packet, packet + 32u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x07000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
}

void poly_render_quad_mixed_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007D164u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 52u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 38u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;
                    uint32 sixth;
                    uint32 seventh;
                    uint32 eighth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x0C000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 24u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 24u, third);
                    fourth_attribute = r_u32(primitive + 36u);
                    fifth = r_u32(primitive + 48u);
                    sixth = r_u32(primitive + 16u);
                    w_u32(packet + 36u, fourth_attribute);
                    w_u32(packet + 48u, fifth);
                    w_u32(packet + 16u, sixth);
                    seventh = r_u32(primitive + 28u);
                    eighth = r_u32(primitive + 40u);
                    w_u32(packet + 28u, seventh);
                    w_u32(packet + 40u, eighth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 20u, (uint32)previous_screen[1]);
                    w_u32(packet + 32u, (uint32)previous_screen[2]);
                    w_u32(packet + 44u, (uint32)previous_screen[3]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_tex_shaded(packet, packet + 52u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x0C000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 20u, (uint32)previous_screen[1]);
                    w_u32(primitive + 32u, (uint32)previous_screen[2]);
                    w_u32(primitive + 44u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = 1;
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_mixed_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007D55Cu, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 primitive = primitives + 40u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(primitive + 38u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;
                    uint32 fifth;
                    uint32 sixth;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 24u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 24u, third);
                    fourth_attribute = r_u32(primitive + 36u);
                    fifth = r_u32(primitive + 16u);
                    sixth = r_u32(primitive + 28u);
                    w_u32(packet + 36u, fourth_attribute);
                    w_u32(packet + 16u, fifth);
                    w_u32(packet + 28u, sixth);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 20u, (uint32)previous_screen[1]);
                    w_u32(packet + 32u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_tex_shaded(packet, packet + 40u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x09000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 20u, (uint32)previous_screen[1]);
                    w_u32(primitive + 32u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
}

void poly_render_quad_alt_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007D8B0u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 32u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 24u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 color;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x05000000u);
                    w_u32(bucket, packet);
                    color = r_u32(primitive + 4u);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 12u, (uint32)previous_screen[1]);
                    w_u32(packet + 16u, (uint32)previous_screen[2]);
                    w_u32(packet + 20u, (uint32)previous_screen[3]);
                    w_u32(packet + 4u, color);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_flat(packet, packet + 24u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x05000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 12u, (uint32)previous_screen[1]);
                    w_u32(primitive + 16u, (uint32)previous_screen[2]);
                    w_u32(primitive + 20u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = 1;
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_alt_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007DC70u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 24u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 20u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 color;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x04000000u);
                    w_u32(bucket, packet);
                    color = r_u32(primitive + 4u);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 12u, (uint32)previous_screen[1]);
                    w_u32(packet + 16u, (uint32)previous_screen[2]);
                    w_u32(packet + 4u, color);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_flat(packet, packet + 20u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x04000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 12u, (uint32)previous_screen[1]);
                    w_u32(primitive + 16u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
}

void poly_render_quad_alt_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007DF9Cu, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR first_three[3];
        SVECTOR fourth;
        sint32 current_screen[4];
        sint32 current_depth[4];
        sint32 current_flag;
        sint32 fourth_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 32u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            first_three[vertex].vx = (sint16)(uint16)xy;
            first_three[vertex].vy = (sint16)(uint16)(xy >> 16);
            first_three[vertex].vz = (sint16)(uint16)zp;
            first_three[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 32u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 36u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;
                    uint32 fourth_attribute;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x08000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    fourth_attribute = r_u32(primitive + 28u);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 28u, fourth_attribute);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(packet + 32u, (uint32)previous_screen[3]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_quad_shaded(packet, packet + 36u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x08000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                    w_u32(primitive + 32u, (uint32)previous_screen[3]);
                }
            }
        }
        previous_valid = 1;
        {
            uint32 source = vertices + 32u * (uint32)iteration + 24u;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            fourth.vx = (sint16)(uint16)xy;
            fourth.vy = (sint16)(uint16)(xy >> 16);
            fourth.vz = (sint16)(uint16)zp;
            fourth.pad = (sint16)(uint16)(zp >> 16);
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
}

void poly_render_tri_alt_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;

    FUNCTION_MARKER_ARGS(0x8007E374u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 4u, XPORT_CALL_GUEST_POINTER(vertices, 1u), XPORT_CALL_GUEST_POINTER(primitives, 1u), XPORT_CALL_SCALAR((uint32)count), XPORT_CALL_GUEST_POINTER(ordering_table, 4u));
    if (count <= 0)
        return;
    height = (sint32)r_u32(0x800B3DFCu);
    width = (sint32)r_u32(0x800B3DF8u);
    primitives &= 0x00FFFFFFu;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR projected_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source = vertices + 24u * (uint32)iteration + 8u * (uint32)vertex;
            uint32 xy = r_u32(source);
            uint32 zp = r_u32(source + 4u);

            projected_vertices[vertex].vx = (sint16)(uint16)xy;
            projected_vertices[vertex].vy = (sint16)(uint16)(xy >> 16);
            projected_vertices[vertex].vz = (sint16)(uint16)zp;
            projected_vertices[vertex].pad = (sint16)(uint16)(zp >> 16);
        }
        gte_project3_full_depth(projected_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            uint32 previous_vertices = vertices + 24u * (uint32)(iteration - 1);
            uint32 primitive = primitives + 28u * (uint32)(iteration - 1);
            sint32 relative_depth = previous_average - 1 - (sint32)r_u16(previous_vertices + 6u);
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ordering_table + 4u + 4u * ((uint32)relative_depth >> 5);
                if (poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u))
                {
                    uint32 old = r_u32(bucket);
                    uint32 packet = r_u32(0x800B6C00u) & 0x00FFFFFFu;
                    uint32 first;
                    uint32 second;
                    uint32 third;

                    w_u32(packet, (old & 0x00FFFFFFu) | 0x06000000u);
                    w_u32(bucket, packet);
                    first = r_u32(primitive + 4u);
                    second = r_u32(primitive + 12u);
                    third = r_u32(primitive + 20u);
                    w_u32(packet + 4u, first);
                    w_u32(packet + 12u, second);
                    w_u32(packet + 20u, third);
                    w_u32(packet + 8u, (uint32)previous_screen[0]);
                    w_u32(packet + 16u, (uint32)previous_screen[1]);
                    w_u32(packet + 24u, (uint32)previous_screen[2]);
                    w_u32(0x800B6C00u, (uint32)poly_subdivide_tri_shaded(packet, packet + 28u));
                    height = (sint32)r_u32(0x800B3DFCu);
                    width = (sint32)r_u32(0x800B3DF8u);
                }
                else
                {
                    uint32 old = r_u32(bucket);

                    w_u32(bucket, primitive);
                    w_u32(primitive, (old & 0x00FFFFFFu) | 0x06000000u);
                    w_u32(primitive + 8u, (uint32)previous_screen[0]);
                    w_u32(primitive + 16u, (uint32)previous_screen[1]);
                    w_u32(primitive + 24u, (uint32)previous_screen[2]);
                }
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
}

void poly_interp_anim_verts(uint32 first, uint32 second, uint32 destination, sint32 count)
{
    sint32 offset = 0;
    sint32 remaining = count;

    FUNCTION_MARKER(0x8007E6B0u, "MAIN.EXE");
    do
    {
        SVECTOR first_vertices[3];
        SVECTOR second_vertices[3];
        sint32 screens[3];
        sint32 depths[3];
        sint32 flags;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            uint32 source_offset = 8u * (uint32)(offset + vertex);

            first_vertices[vertex].vx = (sint16)r_u16(first + source_offset);
            first_vertices[vertex].vy = (sint16)r_u16(first + source_offset + 2u);
            first_vertices[vertex].vz = (sint16)r_u16(first + source_offset + 4u);
            first_vertices[vertex].pad = 0;
            second_vertices[vertex].vx = (sint16)r_u16(second + source_offset);
            second_vertices[vertex].vy = (sint16)r_u16(second + source_offset + 2u);
            second_vertices[vertex].vz = (sint16)r_u16(second + source_offset + 4u);
            second_vertices[vertex].pad = 0;
        }
        gte_interpolate_project3(first_vertices, second_vertices, screens, depths, &flags);
        for (vertex = 0; vertex < 3; ++vertex)
            w_u32(destination + 4u * (uint32)(offset + vertex), (uint32)screens[vertex]);
        offset += 3;
        remaining -= 3;
    } while (remaining > 0);
}

static uint32 mesh_pass_packets_render_mode(uint32 mesh, sint32 pass_count, const sint32 transform[3], sint32 generated_course)
{
    RR_ANIMATED_VERTEX projected[2][32];
    uint32 table;
    uint32 ordering;
    uint32 pending_bucket;
    uint32 packet;
    uint32 special_output;
    uint32 special_count = 0u;
    uint32 descriptor;
    uint32 remaining_passes = (uint32)pass_count;
    sint32 screen_limit_x;
    sint32 screen_limit_y;
    sint32 previous_buffer = -1;
    sint32 current_buffer = 0;

    if (remaining_passes == 0u)
        return mesh;
    table = r_u32(0x800B6B80u);
    ordering = r_u32(0x800B6AB8u);
    pending_bucket = ordering;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    special_output = r_u32(0x800B3E10u);
    w_u16(generated_course ? 0x80080234u : 0x8007EC34u, (uint16)r_u32(0x800B3DFCu));
    w_u16(generated_course ? 0x80080258u : 0x8007EC58u, (uint16)r_u32(0x800B3DF8u));
    screen_limit_y = (sint16)r_u16(generated_course ? 0x80080234u : 0x8007EC34u);
    screen_limit_x = (sint16)r_u16(generated_course ? 0x80080258u : 0x8007EC58u);
    descriptor = r_u32(table + 4u * r_u16(mesh + 2u));
    while (remaining_passes != 0u)
    {
        uint32 phase = r_u32(0x800B3D94u);
        sint32 origin_x = (sint16)r_u16(descriptor + 8u) + (sint32)(uint32)transform[0];
        sint32 origin_y = (sint16)r_u16(descriptor + 10u) + (sint32)(uint32)transform[1];
        sint32 origin_z = (sint16)r_u16(descriptor + 12u) + (sint32)(uint32)transform[2];
        sint32 vertex_count = r_u8(descriptor + 7u);
        uint32 palette_entry = r_u32(0x800B6A50u) + 4u * r_u16(descriptor + 4u);
        uint32 palette = r_u32(palette_entry);
        sint32 vertex;

        for (vertex = 0; vertex < vertex_count; ++vertex)
        {
            uint32 source = descriptor + 30u + 14u * (uint32)vertex;
            SVECTOR point;
            sint32 flags;
            sint32 screen;
            sint32 depth;
            sint32 x;
            sint32 y;
            sint32 first_deformation;
            sint32 second_deformation;
            sint32 vertical_deformation;
            sint32 shading;
            uint16 deformation;
            uint8 low_deformation;
            uint8 high_deformation;
            uint8 outcode;
            uint8 texture;
            uint8 type;

            point.vx = (sint16)(origin_x + 16 * r_u8(source));
            point.vy = (sint16)(origin_y + 16 * r_u8(source + 1u));
            point.vz = (sint16)(origin_z + 16 * r_u8(source + 2u));
            point.pad = 0;
            low_deformation = r_u8(source + 6u) & 15u;
            high_deformation = r_u8(source + 6u) >> 4;
            first_deformation = r_u8(0x800F0508u + ((((sint32)(sint8)r_u8(source + 4u) * 2 + (sint32)phase) & 0x1FF) + 512 * low_deformation));
            second_deformation = r_u8(0x800F0508u + ((((sint32)(sint8)r_u8(source + 5u) * 2 + (sint32)phase) & 0x1FF) + 512 * high_deformation));
            vertical_deformation = -(first_deformation + second_deformation);
            shading = 2 * ((sint32)low_deformation + (sint32)high_deformation);
            if ((uint16)(phase - r_u16(source + 10u)) < 256u)
            {
                sint32 adjustment = 4 * (sint32)r_u8(0x800F1908u + (uint16)(phase - r_u16(source + 10u))) - 80;

                shading -= adjustment;
                vertical_deformation -= adjustment >> 3;
            }
            deformation = r_u16(source + 8u);
            projected[current_buffer][vertex].alternate_screen = 0;
            if (deformation != 0u)
            {
                SVECTOR base = point;
                sint32 unused_flags;

                gte_project_full_depth(&base, &projected[current_buffer][vertex].alternate_screen, &unused_flags);
                point.vx = (sint16)((sint32)base.vx + ((vertical_deformation * ((sint32)(sint8)(uint8)deformation * 128)) >> 12));
                point.vy = (sint16)((sint32)base.vy - vertical_deformation);
                point.vz = (sint16)((sint32)base.vz + ((vertical_deformation * ((sint32)(sint8)(uint8)(deformation >> 8) * 128)) >> 12));
                projected[current_buffer][vertex].color = 0x3C3F3F3Fu;
                outcode = 0u;
            }
            else
            {
                point.vy = (sint16)((sint32)point.vy - vertical_deformation);
                shading = -shading - vertical_deformation + r_u8(source + 13u);
                if (shading < 3)
                    shading = 3;
                projected[current_buffer][vertex].color = poly_anim_vertex_color(palette, shading << 5);
                outcode = 0x10u;
            }
            depth = gte_project_full_depth(&point, &screen, &flags);
            if (deformation == 0u)
                projected[current_buffer][vertex].alternate_screen = screen;
            type = r_u8(source + 3u) >> 5;
            if (type != 0u && special_count < 64u)
            {
                w_u32(0x800D7478u + 4u * special_count, 0x3C000000u);
                w_u16(special_output, (uint16)point.vx);
                w_u16(special_output + 2u, (uint16)point.vy);
                w_u16(special_output + 4u, (uint16)point.vz);
                w_u16(special_output + 6u, type);
                special_output += 8u;
                ++special_count;
                projected[current_buffer][vertex].color = r_u32((generated_course ? 0x80080120u : 0x8007EB20u) + 4u * type);
            }
            x = (sint16)(uint16)screen;
            y = (sint16)(uint16)((uint32)screen >> 16);
            if (y < 0)
                outcode |= 8u;
            if (y >= screen_limit_y)
                outcode |= 4u;
            if (x < 0)
                outcode |= 2u;
            if (x >= screen_limit_x)
                outcode |= 1u;
            if (generated_course)
                texture = (uint8)(r_u8(0x800F1508u + ((2u * r_u8(source + 7u) + phase) & 0x1FFu)) + 32u);
            else
                texture = r_u8(0x800F2308u + ((2u * r_u8(source + 7u) + 2u * phase) & 0x1FFu));
            projected[current_buffer][vertex].screen = screen;
            projected[current_buffer][vertex].depth = depth;
            projected[current_buffer][vertex].flags = (uint16)((uint16)texture << 8) | outcode;
        }
        if (previous_buffer >= 0)
        {
            uint8 selector = r_u8(descriptor + 14u);
            uint32 colors = palette_entry + 8u;
            sint32 first_index = selector & 15u;
            sint32 second_index = selector >> 4;
            uint32 pending_head = 0u;
            uint32 pending_patch = 0u;
            uint32 link_bucket = 0u;
            sint32 link_mode = 1;
            sint32 primitive;

            for (primitive = 0; primitive < 14; ++primitive)
            {
                uint8 control = r_u8(descriptor + 16u + (uint32)primitive);
                sint32 next_first;
                sint32 next_second;
                RR_ANIMATED_VERTEX *a;
                RR_ANIMATED_VERTEX *b;
                RR_ANIMATED_VERTEX *c;
                RR_ANIMATED_VERTEX *d;
                uint16 uv0;
                uint16 uv1;
                uint16 uv2;
                uint16 uv3;
                sint32 bucket_index;
                uint32 bucket;

                if (control == 255u)
                    break;
                if ((control & 3u) != 0u)
                {
                    uint8 kind = control & 3u;
                    uint32 texture;
                    uint32 first_texture;
                    uint32 second_texture;
                    uint32 third_texture;
                    uint32 shared;
                    uint32 color;
                    sint32 screens[4];
                    sint32 depths[4];
                    sint32 first_cross;
                    sint32 second_cross = 0;
                    sint32 bucket_index;
                    uint32 bucket;
                    uint32 link_value;

                    if ((control & 0xF0u) == 0xF0u)
                    {
                        ++first_index;
                        if ((control & 6u) != 2u)
                            ++second_index;
                        colors += 4u;
                        link_mode = 1;
                        continue;
                    }
                    texture = 0x800F2588u + 2u * (uint32)(control & 0xF0u);
                    first_texture = r_u32(texture);
                    second_texture = r_u32(texture + 4u);
                    third_texture = r_u32(texture + 8u);
                    shared = (uint16)(first_texture ^ second_texture);
                    color = (r_u32(colors) & 0x00FFFFFFu) | (kind == 3u ? 0x2C000000u : 0x24000000u);
                    colors += 4u;
                    if (kind == 1u)
                    {
                        RR_ANIMATED_VERTEX *a = &projected[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *b = &projected[current_buffer][second_index + 1];
                        RR_ANIMATED_VERTEX *c = &projected[previous_buffer][first_index];

                        screens[0] = a->alternate_screen;
                        screens[1] = b->alternate_screen;
                        screens[2] = c->alternate_screen;
                        depths[0] = a->depth;
                        depths[1] = b->depth;
                        depths[2] = c->depth;
                        ++second_index;
                        first_cross = NormalClip(screens[0], screens[1], screens[2]);
                        bucket_index = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                        if (first_cross < 0 || (uint32)bucket_index >= 2000u)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_index = (sint32)((uint32)bucket_index >> 3);
                        bucket = ordering + (uint32)bucket_index * 4u;
                        w_u32(packet + 4u, color);
                        w_u32(packet + 8u, (uint32)screens[0]);
                        w_u32(packet + 12u, first_texture ^ shared);
                        w_u32(packet + 16u, (uint32)screens[1]);
                        w_u32(packet + 20u, ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                        w_u32(packet + 24u, (uint32)screens[2]);
                        w_u16(packet + 28u, (uint16)third_texture);
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_value = link_mode > 0 ? pending_head : r_u32(link_bucket);
                        if (link_value == 0u)
                            pending_patch = packet + ((uint32)bucket_index < 20u ? 32u : 0u);
                        w_u32(packet, link_value | 0x07000000u);
                        if (link_mode > 0)
                            pending_head = packet;
                        else
                            w_u32(link_bucket, packet);
                        packet += 32u;
                        if ((uint32)bucket_index < 20u)
                            packet = (uint32)poly_subdivide_tri_tex(packet - 32u, packet);
                        link_mode = 1;
                        continue;
                    }
                    if (kind == 2u)
                    {
                        RR_ANIMATED_VERTEX *a = &projected[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *c = &projected[previous_buffer][first_index];
                        RR_ANIMATED_VERTEX *d = &projected[previous_buffer][first_index + 1];

                        screens[0] = a->alternate_screen;
                        screens[1] = c->alternate_screen;
                        screens[2] = d->alternate_screen;
                        depths[0] = a->depth;
                        depths[1] = c->depth;
                        depths[2] = d->depth;
                        ++first_index;
                        first_cross = NormalClip(screens[0], screens[2], screens[1]);
                        bucket_index = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                        if (first_cross < 0 || (uint32)bucket_index >= 2000u)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_index = (sint32)((uint32)bucket_index >> 3);
                        bucket = ordering + (uint32)bucket_index * 4u;
                        w_u32(packet + 4u, color);
                        w_u32(packet + 8u, (uint32)screens[0]);
                        w_u32(packet + 12u, first_texture ^ shared);
                        w_u32(packet + 16u, (uint32)screens[2]);
                        w_u32(packet + 20u, (first_texture << 16) | (third_texture >> 16));
                        w_u32(packet + 24u, (uint32)screens[1]);
                        w_u16(packet + 28u, (uint16)third_texture);
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_value = link_mode > 0 ? pending_head : r_u32(link_bucket);
                        if (link_value == 0u)
                            pending_patch = packet + ((uint32)bucket_index < 20u ? 32u : 0u);
                        w_u32(packet, link_value | 0x07000000u);
                        if (link_mode > 0)
                            pending_head = packet;
                        else
                            w_u32(link_bucket, packet);
                        packet += 32u;
                        if ((uint32)bucket_index < 20u)
                            packet = (uint32)poly_subdivide_tri_tex(packet - 32u, packet);
                        link_mode = 1;
                        continue;
                    }
                    {
                        RR_ANIMATED_VERTEX *a = &projected[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *b = &projected[current_buffer][second_index + 1];
                        RR_ANIMATED_VERTEX *c = &projected[previous_buffer][first_index];
                        RR_ANIMATED_VERTEX *d = &projected[previous_buffer][first_index + 1];

                        screens[0] = a->alternate_screen;
                        screens[1] = b->alternate_screen;
                        screens[2] = c->alternate_screen;
                        screens[3] = d->alternate_screen;
                        depths[0] = a->depth;
                        depths[1] = b->depth;
                        depths[2] = c->depth;
                        depths[3] = d->depth;
                        ++first_index;
                        ++second_index;
                        if ((a->flags & b->flags & c->flags & d->flags & 15u) != 0u)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_index = (sint32)(((uint32)(uint16)depths[0] + (uint32)(uint16)depths[1] + (uint32)(uint16)depths[2] + (uint32)(uint16)depths[3]) & 0xFFFFu) - 1;
                        if ((uint32)bucket_index >= 0x7D00u)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_index = (sint32)((uint32)bucket_index >> 7);
                        first_cross = NormalClip(screens[0], screens[1], screens[2]);
                        second_cross = NormalClip(screens[1], screens[2], screens[3]);
                        bucket = ordering + (uint32)bucket_index * 4u;
                        if (first_cross >= 0 && second_cross <= 0)
                        {
                            w_u32(packet + 4u, color);
                            w_u32(packet + 8u, (uint32)screens[0]);
                            w_u32(packet + 12u, first_texture ^ shared);
                            w_u32(packet + 16u, (uint32)screens[1]);
                            w_u32(packet + 20u, ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                            w_u32(packet + 24u, (uint32)screens[2]);
                            w_u16(packet + 28u, (uint16)third_texture);
                            w_u32(packet + 32u, (uint32)screens[3]);
                            w_u16(packet + 36u, (uint16)(third_texture >> 16));
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_value = link_mode > 0 ? pending_head : r_u32(link_bucket);
                            if (link_value == 0u)
                                pending_patch = packet + ((uint32)bucket_index < 20u ? 0x118u : 0u);
                            w_u32(packet, link_value | 0x09000000u);
                            if (link_mode > 0)
                                pending_head = packet;
                            else
                                w_u32(link_bucket, packet);
                            packet += 40u;
                            if ((uint32)bucket_index < 20u)
                            {
                                uint32 source = packet - 40u;

                                packet = poly_subdivide_quad_tex(source, packet);
                                packet = poly_subdivide_quad_tex(source, packet);
                                packet = poly_subdivide_quad_tex(source + 40u, packet);
                                packet = poly_subdivide_quad_tex(source + 80u, packet);
                                packet = poly_subdivide_quad_tex(source + 120u, packet);
                            }
                        }
                        else if (first_cross >= 0 || second_cross <= 0)
                        {
                            sint32 first = first_cross >= 0 ? 0 : 1;
                            sint32 middle = first + 1;
                            sint32 last = first + 2;

                            w_u32(packet + 4u, (color & 0x00FFFFFFu) | 0x24000000u);
                            w_u32(packet + 8u, (uint32)screens[first]);
                            w_u32(packet + 12u, first_cross >= 0 ? first_texture ^ shared : (first_texture & 0xFFFF0000u) | (second_texture >> 16));
                            w_u32(packet + 16u, (uint32)screens[middle]);
                            w_u32(packet + 20u, first_cross >= 0 ? ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16) : (first_texture << 16) | (third_texture & 0xFFFFu));
                            w_u32(packet + 24u, (uint32)screens[last]);
                            w_u16(packet + 28u, (uint16)(first_cross >= 0 ? third_texture : third_texture >> 16));
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_value = link_mode > 0 ? pending_head : r_u32(link_bucket);
                            if (link_value == 0u)
                                pending_patch = packet + ((uint32)bucket_index < 20u ? 32u : 0u);
                            w_u32(packet, link_value | 0x07000000u);
                            if (link_mode > 0)
                                pending_head = packet;
                            else
                                w_u32(link_bucket, packet);
                            packet += 32u;
                            if ((uint32)bucket_index < 20u)
                                packet = (uint32)poly_subdivide_tri_tex(packet - 32u, packet);
                        }
                    }
                    link_mode = 1;
                    continue;
                }
                next_first = first_index + ((control & 6u) != 1u);
                next_second = second_index + ((control & 6u) != 2u);
                a = &projected[current_buffer][second_index];
                b = &projected[current_buffer][next_second];
                c = &projected[previous_buffer][first_index];
                d = &projected[previous_buffer][next_first];
                first_index = next_first;
                second_index = next_second;
                w_u32(packet + 4u, a->color);
                w_u32(packet + 8u, (uint32)a->screen);
                w_u32(packet + 16u, b->color);
                w_u32(packet + 20u, (uint32)b->screen);
                w_u32(packet + 28u, c->color);
                w_u32(packet + 32u, (uint32)c->screen);
                w_u32(packet + 40u, d->color);
                w_u32(packet + 44u, (uint32)d->screen);
                if ((a->flags & b->flags & c->flags & d->flags & 15u) != 0u)
                {
                    sint32 rejected_depth = a->depth + b->depth + c->depth + d->depth;

                    if (rejected_depth < 0x7D00)
                    {
                        pending_bucket = ordering;
                        link_bucket = ordering;
                        link_mode = 0;
                    }
                    else
                    {
                        pending_bucket = 0u;
                        link_mode = -1;
                    }
                    continue;
                }
                if (((a->flags | c->flags) & 0xF0u) == 0u)
                {
                    uv0 = (uint16)((a->flags & 0xFF00u) | (generated_course ? 0x2Fu : 0u));
                    uv1 = (uint16)((b->flags & 0xFF00u) | (generated_course ? 0u : 0x3Fu));
                    uv2 = (uint16)((c->flags & 0xFF00u) | (generated_course ? 0x2Fu : 0u));
                    uv3 = (uint16)((d->flags & 0xFF00u) | (generated_course ? 0u : 0x3Fu));
                }
                else if (((b->flags | d->flags) & 0xF0u) == 0u)
                {
                    uv0 = (uint16)((a->flags & 0xFF00u) | (generated_course ? 0u : 0x3Fu));
                    uv1 = (uint16)((b->flags & 0xFF00u) | (generated_course ? 0x2Fu : 0u));
                    uv2 = (uint16)((c->flags & 0xFF00u) | (generated_course ? 0u : 0x3Fu));
                    uv3 = (uint16)((d->flags & 0xFF00u) | (generated_course ? 0x2Fu : 0u));
                }
                else
                {
                    uv0 = (uint16)((a->flags & 0xFF00u) | (generated_course ? 0u : 0x10u));
                    uv1 = (uint16)((b->flags & 0xFF00u) | (generated_course ? 0x27u : 0x3Fu));
                    uv2 = (uint16)((c->flags & 0xFF00u) | (generated_course ? 0u : 0x10u));
                    uv3 = (uint16)((d->flags & 0xFF00u) | (generated_course ? 0x27u : 0x3Fu));
                }
                w_u32(packet + 12u, (uint32)uv0 | ((uint32)r_u16(0x800F259Au) << 16));
                w_u32(packet + 24u, (uint32)uv1 | ((uint32)r_u16(0x800F2598u) << 16));
                w_u16(packet + 36u, uv2);
                w_u16(packet + 48u, uv3);
                {
                    sint32 rejected_depth = a->depth + b->depth + c->depth + d->depth - 1;

                    bucket_index = (sint32)((uint32)rejected_depth >> 7);
                    if ((uint32)rejected_depth >= 0x7D00u)
                    {
                        if (rejected_depth < 0x7D00)
                        {
                            pending_bucket = ordering;
                            link_bucket = ordering;
                            link_mode = 0;
                        }
                        else
                        {
                            pending_bucket = 0u;
                            link_mode = -1;
                        }
                        continue;
                    }
                }
                bucket = ordering + (uint32)bucket_index * 4u;
                pending_bucket = bucket;
                w_u32(packet, r_u32(bucket) | 0x0C000000u);
                if (pending_head != 0u)
                {
                    uint32 head = pending_head;

                    w_u32(pending_patch, r_u32(pending_patch) | packet);
                    pending_head = 0u;
                    w_u32(bucket, head);
                }
                else
                {
                    w_u32(bucket, packet);
                }
                link_bucket = bucket;
                link_mode = 0;
                packet += 52u;
                if ((uint32)bucket_index < 20u)
                    packet = (uint32)poly_subdivide_quad_tex_shaded(packet - 52u, packet);
            }
            if (pending_head != 0u && pending_bucket != 0u)
            {
                uint32 old_bucket = r_u32(pending_bucket);

                w_u32(pending_bucket, pending_head);
                w_u32(pending_patch, r_u32(pending_patch) | old_bucket);
            }
        }
        if (previous_buffer >= 0)
            --remaining_passes;
        previous_buffer = current_buffer;
        current_buffer ^= 1;
        descriptor = r_u32(table + 4u * r_u16(descriptor));
    }
    w_u32(0x800B3E14u, special_count);
    w_u32(0x800B6C00u, packet);
    return descriptor;
}

uint32 mesh_render_pass_packets(uint32 mesh, sint32 pass_count, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007E850u, "MAIN.EXE");
    return mesh_pass_packets_render_mode(mesh, pass_count, transform, 0);
}

uint32 mesh_render_pass(uint32 mesh, sint32 pass_count, const sint32 transform[3])
{
    uint32 current = mesh;
    uint32 remaining_passes = (uint32)pass_count;
    uint32 table;
    uint32 ordering;
    uint32 packet;
    uint32 phase;
    uint32 partner;
    uint32 partner_index;
    sint32 partner_x;
    sint32 partner_y;
    sint32 partner_z;

    FUNCTION_MARKER(0x8007F490u, "MAIN.EXE");
    partner_index = r_u16(current + 2u);
    if (remaining_passes == 0u)
        return current;
    phase = r_u32(0x800B3D94u);
    table = r_u32(0x800B6B80u);
    partner = r_u32(table + 4u * partner_index);
    ordering = r_u32(0x800B6AB8u);
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    {
        sint32 transform_x = (sint32)(uint32)transform[0];
        sint32 transform_y = (sint32)(uint32)transform[1];
        sint32 transform_z = (sint32)(uint32)transform[2];
        sint32 offset_x = (sint16)r_u16(partner + 8u);
        sint32 offset_y = (sint16)r_u16(partner + 10u);
        sint32 offset_z = (sint16)r_u16(partner + 12u);

        partner_x = (sint32)((uint32)transform_x + (uint32)offset_x);
        partner_y = (sint32)((uint32)transform_y + (uint32)offset_y);
        partner_z = (sint32)((uint32)transform_z + (uint32)offset_z);
    }
    while (remaining_passes != 0u)
    {
        uint32 control = current + 16u;
        uint32 palette_index = r_u16(current + 4u);
        uint32 palette_base = r_u32(0x800B6A50u);
        uint32 palette = palette_base + 4u * palette_index;
        uint32 palette_color = r_u32(palette + 4u) ^ 0x10000000u;
        uint32 colors = palette + 8u;
        uint8 selector = r_u8(current + 14u);
        uint32 current_record = current + 30u + 14u * (uint32)(selector >> 4);
        uint32 partner_record = partner + 30u + 14u * (uint32)(selector & 15u);
        uint32 prior_current = current_record;
        uint32 prior_partner = partner_record;
        sint32 transform_x = (sint32)(uint32)transform[0];
        sint32 transform_y = (sint32)(uint32)transform[1];
        sint32 transform_z = (sint32)(uint32)transform[2];
        sint32 offset_x = (sint16)r_u16(current + 8u);
        sint32 offset_y = (sint16)r_u16(current + 10u);
        sint32 offset_z = (sint16)r_u16(current + 12u);
        sint32 current_x = (sint32)((uint32)transform_x + (uint32)offset_x);
        sint32 current_y = (sint32)((uint32)transform_y + (uint32)offset_y);
        sint32 current_z = (sint32)((uint32)transform_z + (uint32)offset_z);
        sint32 joined = 1;

        for (;;)
        {
            uint8 command = r_u8(control++);
            uint8 kind = command & 3u;
            sint32 screens[4];
            sint32 depths[4];
            sint32 depth;
            sint32 first_cross;
            sint32 second_cross;
            uint32 bucket;
            uint32 texture;
            uint32 first_texture;
            uint32 second_texture;
            uint32 third_texture;
            uint32 shared;
            uint32 primitive_color;

            if (kind == 0u)
            {
                current_record += 14u;
                partner_record += 14u;
                if (joined == 0)
                    continue;
                prior_current = current_record - 14u;
                prior_partner = partner_record - 14u;
                joined = 0;
                continue;
            }

            if (joined == 0)
            {
                joined = 1;
                depths[0] = poly_project_mesh_vertex(prior_current, current_x, current_y, current_z, &screens[0]);
                depths[1] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[1]);
                depths[2] = poly_project_mesh_vertex(prior_partner, partner_x, partner_y, partner_z, &screens[2]);
                depths[3] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[3]);
                depth = AverageZ4(depths[0], depths[1], depths[2], depths[3]) - 1;
                {
                    uint32 texture_page = r_u32(0x800F2598u);

                    w_u32(packet + 4u, palette_color);
                    w_u32(packet + 8u, (uint32)screens[0]);
                    w_u32(packet + 12u, ((texture_page >> 16) << 16) | ((uint32)mesh_texture_coordinate(current_record, phase) << 8));
                    w_u32(packet + 16u, (uint32)screens[1]);
                    w_u32(packet + 20u, ((texture_page & 0xFFFFu) << 16) | ((uint32)mesh_texture_coordinate(prior_current, phase) << 8) | 0x3Fu);
                    w_u32(packet + 24u, (uint32)screens[2]);
                    w_u16(packet + 28u, (uint16)((uint32)mesh_texture_coordinate(partner_record, phase) << 8));
                    w_u32(packet + 32u, (uint32)screens[3]);
                    w_u16(packet + 36u, (uint16)(((uint32)mesh_texture_coordinate(prior_partner, phase) << 8) | 0x3Fu));
                    if ((uint32)depth < 0x7D0u)
                    {
                        bucket = ordering + 4u * ((uint32)depth >> 3);
                        w_u32(packet, r_u32(bucket) | 0x09000000u);
                        w_u32(bucket, packet);
                        packet += 40u;
                    }
                }
            }

            if (command == 0xFFu)
                break;

            if (kind == 3u)
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                current_record += 14u;
                depths[1] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[1]);
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                partner_record += 14u;
                depths[3] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[3]);
                first_cross = NormalClip(screens[0], screens[1], screens[2]);
                second_cross = NormalClip(screens[1], screens[2], screens[3]);
                depth = AverageZ4(depths[0], depths[1], depths[2], depths[3]) - 1;
                primitive_color = r_u32(colors);
                texture = 0x800F2588u + 2u * (uint32)(command & 0xF0u);
                first_texture = r_u32(texture);
                second_texture = r_u32(texture + 4u);
                third_texture = r_u32(texture + 8u);
                shared = (uint16)(first_texture ^ second_texture);
                w_u32(packet + 4u, primitive_color | 0x2C000000u);
                w_u32(packet + 8u, (uint32)screens[0]);
                w_u32(packet + 12u, first_texture ^ shared);
                w_u32(packet + 16u, (uint32)screens[1]);
                w_u32(packet + 20u, ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                w_u32(packet + 24u, (uint32)screens[2]);
                w_u16(packet + 28u, (uint16)third_texture);
                w_u32(packet + 32u, (uint32)screens[3]);
                w_u16(packet + 36u, (uint16)(third_texture >> 16));
                if ((second_cross <= 0 || first_cross >= 0) && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + 4u * ((uint32)depth >> 3);
                    w_u32(packet, r_u32(bucket) | 0x09000000u);
                    w_u32(bucket, packet);
                    packet += 40u;
                }
                colors += 4u;
            }
            else if (kind == 1u)
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                current_record += 14u;
                depths[1] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[1]);
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                depth = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                primitive_color = r_u32(colors);
                texture = 0x800F2588u + 2u * (uint32)(command & 0xF0u);
                first_texture = r_u32(texture);
                second_texture = r_u32(texture + 4u);
                third_texture = r_u32(texture + 8u);
                shared = (uint16)(first_texture ^ second_texture);
                w_u32(packet + 4u, primitive_color | 0x24000000u);
                w_u32(packet + 8u, (uint32)screens[0]);
                w_u32(packet + 12u, first_texture ^ shared);
                w_u32(packet + 16u, (uint32)screens[1]);
                w_u32(packet + 20u, ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                w_u32(packet + 24u, (uint32)screens[2]);
                w_u16(packet + 28u, (uint16)third_texture);
                if (NormalClip(screens[0], screens[1], screens[2]) >= 0 && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + 4u * ((uint32)depth >> 3);
                    w_u32(packet, r_u32(bucket) | 0x07000000u);
                    w_u32(bucket, packet);
                    packet += 32u;
                }
                colors += 4u;
            }
            else
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                depths[1] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[1]);
                partner_record += 14u;
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                depth = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                primitive_color = r_u32(colors);
                texture = 0x800F2588u + 2u * (uint32)(command & 0xF0u);
                first_texture = r_u32(texture);
                second_texture = r_u32(texture + 4u);
                third_texture = r_u32(texture + 8u);
                shared = (uint16)(first_texture ^ second_texture);
                w_u32(packet + 4u, primitive_color | 0x24000000u);
                w_u32(packet + 8u, (uint32)screens[0]);
                w_u32(packet + 12u, first_texture ^ shared);
                w_u32(packet + 16u, (uint32)screens[2]);
                w_u32(packet + 20u, (first_texture << 16) | (third_texture >> 16));
                w_u32(packet + 24u, (uint32)screens[1]);
                w_u16(packet + 28u, (uint16)third_texture);
                if (NormalClip(screens[0], screens[1], screens[2]) <= 0 && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + 4u * ((uint32)depth >> 3);
                    w_u32(packet, r_u32(bucket) | 0x07000000u);
                    w_u32(bucket, packet);
                    packet += 32u;
                }
                colors += 4u;
            }
        }
        partner = current;
        partner_x = current_x;
        partner_y = current_y;
        partner_z = current_z;
        current = r_u32(table + 4u * r_u16(current));
        --remaining_passes;
    }
    w_u32(0x800B6C00u, packet);
    return current;
}

uint32 poly_fn_8007fe50(uint32 object, sint32 enabled, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007FE50u, "MAIN.EXE");
    return mesh_pass_packets_render_mode(object, enabled, transform, 1);
}
