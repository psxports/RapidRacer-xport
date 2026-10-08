#include "replay.h"
#include "vehicle.h"
#include <string.h>
#include "mesh.h"
#include "camera.h"
#include "trail.h"
#include "global.h"
#include "motion.h"
#include "render.h"
#include <stdlib.h>

#if defined(_DEBUG)
    #if defined(_WIN32)
        #include <windows.h>
    #endif
typedef struct
{
    const BOAT *player;
    uint32 groups;
    uint32 source_rgb;
    uint32 output_rgb;
    uint32 intensity[3];
    uint32 vertex;
    uint32 lighting_record;
    SVECTOR normal;
    uint8 lighting[56];
    PsxGteSnapshot gte;
} MESH_COLOR_PROBE;

volatile uint32 mesh_color_probe_armed;
volatile uint32 mesh_color_probe_peak_limit = 204u;
volatile uint32 mesh_color_probe_hits;
MESH_COLOR_PROBE mesh_color_probe;
static const BOAT *mesh_color_probe_player;
static uint32 mesh_color_probe_groups;

// Break here after the native check has saved its evidence
__declspec(noinline) void mesh_color_probe_hit(void)
{
    ++mesh_color_probe_hits;
}

static void mesh_color_probe_check(const SVECTOR *normal, uint32 source, uint32 output, uint32 vertex)
{
    uint32 intensity[3];
    uint32 maximum = 0u;
    uint32 channel;
    uint32 descriptor;

    if (!mesh_color_probe_player || !mesh_color_probe_armed)
        return;
    // Express lighting as the output of a neutral 128 channel
    for (channel = 0u; channel < 3u; ++channel)
    {
        uint32 input = (source >> (channel * 8u)) & 255u;
        uint32 result = (output >> (channel * 8u)) & 255u;

        intensity[channel] = input ? result * 128u / input : (result ? 255u : 0u);
        if (intensity[channel] > maximum)
            maximum = intensity[channel];
    }
    if (maximum <= mesh_color_probe_peak_limit)
        return;
    mesh_color_probe_armed = 0u;
    mesh_color_probe.player = mesh_color_probe_player;
    mesh_color_probe.groups = mesh_color_probe_groups;
    mesh_color_probe.source_rgb = source;
    mesh_color_probe.output_rgb = output;
    memcpy(mesh_color_probe.intensity, intensity, sizeof(intensity));
    mesh_color_probe.vertex = vertex;
    mesh_color_probe.normal = *normal;
    descriptor = (uint32)mesh_color_probe_player->contacts.points[0].object;
    mesh_color_probe.lighting_record = r_u32(0x800834ACu) + (uint32)r_u8(descriptor + 6u) * 56u;
    memcpy(mesh_color_probe.lighting, psx_addr(mesh_color_probe.lighting_record, 56u), 56u);
    psx_gte_snapshot(&mesh_color_probe.gte);
    mesh_color_probe_hit();
}

static uint32 mesh_color_eval(const SVECTOR *normal, uint32 color, uint32 vertex)
{
    uint32 result = gte_normal_color_col(normal, color);

    mesh_color_probe_check(normal, color, result, vertex);
    return result;
}

static void mesh_color_eval3(const SVECTOR normals[3], uint32 color, uint32 output[3])
{
    uint32 vertex;

    gte_normal_color_col3(normals, color, output);
    for (vertex = 0; vertex < 3u; ++vertex)
        mesh_color_probe_check(&normals[vertex], color, output[vertex], vertex);
}
#else
    #define mesh_color_eval(normal, color, vertex) gte_normal_color_col(normal, color)
    #define mesh_color_eval3 gte_normal_color_col3
#endif

static uint32 mesh_gte_div(uint32 numerator, uint32 denominator)
{
    uint32 divisor;
    uint32 index;
    sint32 table_value;
    sint32 reciprocal_seed;
    sint32 correction;
    uint32 reciprocal;
    uint32 result;

    while ((denominator & 0x8000u) == 0u)
    {
        numerator <<= 1;
        denominator <<= 1;
    }
    divisor = denominator | 0x8000u;
    index = ((divisor & 0x7FFFu) + 0x40u) >> 7;
    table_value = (sint32)((0x40000u / (index + 0x100u) + 1u) / 2u) - 0x101;
    if (table_value < 0)
        table_value = 0;
    reciprocal_seed = 0x101 + table_value;
    correction = ((sint32)divisor * -reciprocal_seed + 0x80) >> 8;
    reciprocal = (uint32)((reciprocal_seed * (0x20000 + correction) + 0x80) >> 8);
    result = (uint32)(((uint64)numerator * reciprocal + 0x8000u) >> 16);
    return result > 0x1FFFFu ? 0x1FFFFu : result;
}

static void mesh_rotate_mat_y(sint32 angle, MATRIX *matrix)
{
    sint32 cosine;
    sint32 sine;
    sint32 column;

    rsincos(angle, &sine, &cosine);
    for (column = 0; column < 3; ++column)
    {
        sint32 first = matrix->m[0][column];
        sint32 third = matrix->m[2][column];
        uint32 first_product = (uint32)((uint64)(uint32)cosine * (uint32)first);
        uint32 second_product = (uint32)((uint64)(uint32)sine * (uint32)third);
        uint32 third_product = (uint32)((uint64)(0u - (uint32)sine) * (uint32)first);
        uint32 fourth_product = (uint32)((uint64)(uint32)cosine * (uint32)third);

        matrix->m[0][column] = (sint16)((sint32)(first_product + second_product) >> 12);
        matrix->m[2][column] = (sint16)((sint32)(third_product + fourth_product) >> 12);
    }
}

static void mesh_write_mat(uint32 address, const MATRIX *matrix, uint16 pad)
{
    sint32 row;
    sint32 column;

    for (row = 0; row < 3; ++row)
        for (column = 0; column < 3; ++column)
            w_u16(address + (uint32)(row * 3 + column) * 2u, (uint16)matrix->m[row][column]);
    w_u16(address + 18u, pad);
    for (row = 0; row < 3; ++row)
        w_u32(address + 20u + (uint32)row * 4u, (uint32)matrix->t[row]);
}

static void mesh_transform_point(const MATRIX *matrix, const SVECTOR *point, VECTOR *output)
{
    sint32 flags;

    SetRotMatrix((MATRIX *)matrix);
    SetTransMatrix((MATRIX *)matrix);
    RotTrans((SVECTOR *)point, output, &flags);
}

static void mesh_read_svector(uint32 address, SVECTOR *output)
{
    uint32 xy = r_u32(address);
    uint32 zp = r_u32(address + 4u);

    output->vx = (sint16)xy;
    output->vy = (sint16)(xy >> 16);
    output->vz = (sint16)zp;
    output->pad = (sint16)(zp >> 16);
}

void mesh_render_tri_flat_lit(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001C6D8u, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x04000000u);
            w_u32(bucket, packet);
            packet += 20u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 12u, (uint32)screen[1]);
            w_u32(packet + 16u, (uint32)screen[2]);
            memcpy(&normal, psx_addr(faces + 24u, sizeof(normal)), sizeof(normal));
            color = r_u32(faces + 32u);
            color = gte_normal_color_col(&normal, color);
            w_u32(packet + 4u, color);
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        faces += 36u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x04000000u);
            w_u32(bucket, packet);
            packet += 20u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_quad_flat_lit(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001C864u, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR fourth;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x05000000u);
            w_u32(bucket, packet);
            packet += 24u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 12u, (uint32)screen[1]);
            w_u32(packet + 16u, (uint32)screen[2]);
            memcpy(&fourth, psx_addr(faces + 24u, sizeof(fourth)), sizeof(fourth));
            depth[3] = gte_project_full_depth(&fourth, &screen[3], &flags);
            w_u32(packet + 20u, (uint32)screen[3]);
            memcpy(&normal, psx_addr(faces + 32u, sizeof(normal)), sizeof(normal));
            color = r_u32(faces + 40u);
            color = gte_normal_color_col(&normal, color);
            w_u32(packet + 4u, color);
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        faces += 44u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x05000000u);
            w_u32(bucket, packet);
            packet += 24u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_tri_flat_lit_tex(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CA10u, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;
        uint32 texture;
        uint16 texture_high;
        uint16 texture_low;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x07000000u);
            w_u32(bucket, packet);
            packet += 32u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            memcpy(&normal, psx_addr(faces + 24u, sizeof(normal)), sizeof(normal));
            color = r_u32(faces + 36u);
            color = gte_normal_color_col(&normal, color);
            texture_high = r_u16(faces + 34u);
            texture_low = r_u16(faces + 6u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 12u, texture);
            texture_high = r_u16(faces + 32u);
            texture_low = r_u16(faces + 14u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 20u, texture);
            w_u16(packet + 28u, r_u16(faces + 22u));
            w_u32(packet + 4u, color);
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        faces += 40u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x07000000u);
            w_u32(bucket, packet);
            packet += 32u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_quad_flat_lit_tex(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CBDCu, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR fourth;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;
        uint32 texture;
        uint16 texture_high;
        uint16 texture_low;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            memcpy(&fourth, psx_addr(faces + 24u, sizeof(fourth)), sizeof(fourth));
            depth[3] = gte_project_full_depth(&fourth, &screen[3], &flags);
            w_u32(packet + 32u, (uint32)screen[3]);
            memcpy(&normal, psx_addr(faces + 32u, sizeof(normal)), sizeof(normal));
            color = r_u32(faces + 44u);
            color = gte_normal_color_col(&normal, color);
            texture_high = r_u16(faces + 42u);
            texture_low = r_u16(faces + 6u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 12u, texture);
            texture_high = r_u16(faces + 40u);
            texture_low = r_u16(faces + 14u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 20u, texture);
            w_u16(packet + 28u, r_u16(faces + 22u));
            w_u16(packet + 36u, r_u16(faces + 30u));
            w_u32(packet + 4u, color);
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        faces += 48u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_tri_gouraud_lit(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CDD4u, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x06000000u);
            w_u32(bucket, packet);
            packet += 28u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            sint32 vertex;

            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            for (vertex = 0; vertex < 3; ++vertex)
            {
                memcpy(&normal, psx_addr(faces + 24u + (uint32)vertex * 8u, sizeof(normal)), sizeof(normal));
                color = r_u32(faces + 48u + (uint32)vertex * 4u);
                color = mesh_color_eval(&normal, color, (uint32)vertex);
                w_u32(packet + 4u + (uint32)vertex * 8u, color);
            }
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        faces += 60u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x06000000u);
            w_u32(bucket, packet);
            packet += 28u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_quad_gouraud_lit(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CFACu, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR fourth;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x08000000u);
            w_u32(bucket, packet);
            packet += 36u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            sint32 vertex;

            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            memcpy(&fourth, psx_addr(faces + 24u, sizeof(fourth)), sizeof(fourth));
            depth[3] = gte_project_full_depth(&fourth, &screen[3], &flags);
            w_u32(packet + 32u, (uint32)screen[3]);
            for (vertex = 0; vertex < 4; ++vertex)
            {
                memcpy(&normal, psx_addr(faces + 32u + (uint32)vertex * 8u, sizeof(normal)), sizeof(normal));
                color = r_u32(faces + 64u + (uint32)vertex * 4u);
                color = mesh_color_eval(&normal, color, (uint32)vertex);
                w_u32(packet + 4u + (uint32)vertex * 8u, color);
            }
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        faces += 80u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x08000000u);
            w_u32(bucket, packet);
            packet += 36u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_tri_gouraud_lit_tex(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001D1CCu, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normals[3];
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 colors[3];
        uint32 source_color;
        uint32 texture;
        uint16 texture_high;
        uint16 texture_low;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 20u, (uint32)screen[1]);
            w_u32(packet + 32u, (uint32)screen[2]);
            memcpy(normals, psx_addr(faces + 24u, sizeof(normals)), sizeof(normals));
            source_color = r_u32(faces + 52u);
            mesh_color_eval3(normals, source_color, colors);
            texture_high = r_u16(faces + 50u);
            texture_low = r_u16(faces + 6u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 12u, texture);
            texture_high = r_u16(faces + 48u);
            texture_low = r_u16(faces + 14u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 24u, texture);
            w_u16(packet + 36u, r_u16(faces + 22u));
            w_u32(packet + 4u, colors[0]);
            w_u32(packet + 16u, colors[1]);
            w_u32(packet + 28u, colors[2]);
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        faces += 64u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_quad_gouraud_lit_tex(uint32 faces, sint32 count)
{
    uint32 packet;
    uint32 ordering_table;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001D3B0u, "MAIN.EXE");
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    ordering_table = r_u32(0x800B6AB4u);
    depth_limit = r_u32(0x800B6A08u);
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR fourth;
        SVECTOR normals[3];
        SVECTOR fourth_normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 colors[4];
        uint32 source_color;
        uint32 texture;
        uint16 texture_high;
        uint16 texture_low;

        memcpy(vertices, psx_addr(faces, sizeof(vertices)), sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x0C000000u);
            w_u32(bucket, packet);
            packet += 52u;
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 20u, (uint32)screen[1]);
            w_u32(packet + 32u, (uint32)screen[2]);
            memcpy(&fourth, psx_addr(faces + 24u, sizeof(fourth)), sizeof(fourth));
            depth[3] = gte_project_full_depth(&fourth, &screen[3], &flags);
            w_u32(packet + 44u, (uint32)screen[3]);
            memcpy(normals, psx_addr(faces + 32u, sizeof(normals)), sizeof(normals));
            source_color = r_u32(faces + 68u);
            mesh_color_eval3(normals, source_color, colors);
            texture_high = r_u16(faces + 66u);
            texture_low = r_u16(faces + 6u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 12u, texture);
            texture_high = r_u16(faces + 64u);
            texture_low = r_u16(faces + 14u);
            texture = (uint32)texture_low | ((uint32)texture_high << 16);
            w_u32(packet + 24u, texture);
            w_u16(packet + 36u, r_u16(faces + 22u));
            w_u16(packet + 48u, r_u16(faces + 30u));
            w_u32(packet + 4u, colors[0]);
            w_u32(packet + 16u, colors[1]);
            w_u32(packet + 28u, colors[2]);
            memcpy(&fourth_normal, psx_addr(faces + 56u, sizeof(fourth_normal)), sizeof(fourth_normal));
            colors[3] = mesh_color_eval(&fourth_normal, source_color, 3u);
            w_u32(packet + 40u, colors[3]);
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        faces += 84u;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + r_u32(0x800B69F0u));

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 bucket = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(bucket) | 0x0C000000u);
            w_u32(bucket, packet);
            packet += 52u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_dispatch_renderers(uint32 first, uint32 second, uint32 groups)
{
    uint32 faces;
    sint32 count;

    FUNCTION_MARKER(0x8001D5D8u, "MAIN.EXE");
    render_set_order_depth(first, second);
    faces = r_u32(groups + 32u);
    count = (sint32)r_u32(groups);
    mesh_render_tri_flat_lit(faces, count);
    faces = r_u32(groups + 36u);
    count = (sint32)r_u32(groups + 4u);
    mesh_render_quad_flat_lit(faces, count);
    faces = r_u32(groups + 40u);
    count = (sint32)r_u32(groups + 8u);
    mesh_render_tri_flat_lit_tex(faces, count);
    faces = r_u32(groups + 44u);
    count = (sint32)r_u32(groups + 12u);
    mesh_render_quad_flat_lit_tex(faces, count);
    faces = r_u32(groups + 48u);
    count = (sint32)r_u32(groups + 16u);
    mesh_render_tri_gouraud_lit(faces, count);
    faces = r_u32(groups + 52u);
    count = (sint32)r_u32(groups + 20u);
    mesh_render_quad_gouraud_lit(faces, count);
    faces = r_u32(groups + 56u);
    count = (sint32)r_u32(groups + 24u);
    mesh_render_tri_gouraud_lit_tex(faces, count);
    faces = r_u32(groups + 60u);
    count = (sint32)r_u32(groups + 28u);
    mesh_render_quad_gouraud_lit_tex(faces, count);
}

void mesh_render_quad_faces(uint32 faces, sint32 count, uint32 stride)
{
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 ordering_table = r_u32(0x800B6AB4u);
    uint32 depth_limit = r_u32(0x800B6A08u);
    sint32 index;

    FUNCTION_MARKER(0x8001D680u, "MAIN.EXE");
    for (index = 0; index < count; ++index, faces += stride)
    {
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross;
        sint32 average;
        uint32 bucket;
        sint32 vertex;

        for (vertex = 0; vertex < 4; ++vertex)
            depth[vertex] = gte_project((SVECTOR *)psx_addr(faces + (uint32)vertex * 8u, sizeof(SVECTOR)), &screen[vertex], &flags);
        cross = NormalClip(screen[0], screen[1], screen[2]);
        average = AverageZ4(depth[0], depth[1], depth[2], depth[3]) + (sint32)r_u32(0x800B69F0u);
        if ((uint32)average >= depth_limit)
            continue;
        bucket = ordering_table + (uint32)average * 4u;
        w_u32(packet, r_u32(bucket) | 0x07000000u);
        w_u32(bucket, packet);
        w_u32(packet + 4u, cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u);
        w_u32(packet + 8u, (uint32)screen[0]);
        w_u32(packet + 12u, (uint32)screen[1]);
        w_u32(packet + 16u, (uint32)screen[3]);
        w_u32(packet + 20u, (uint32)screen[2]);
        w_u32(packet + 24u, (uint32)screen[0]);
        w_u32(packet + 28u, 0x55555555u);
        packet += 32u;
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_render_tri_faces(uint32 faces, sint32 count, uint32 stride)
{
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 ordering_table = r_u32(0x800B6AB4u);
    uint32 depth_limit = r_u32(0x800B6A08u);
    sint32 index;

    FUNCTION_MARKER(0x8001D840u, "MAIN.EXE");
    for (index = 0; index < count; ++index, faces += stride)
    {
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 cross;
        sint32 average;
        uint32 bucket;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
            depth[vertex] = gte_project((SVECTOR *)psx_addr(faces + (uint32)vertex * 8u, sizeof(SVECTOR)), &screen[vertex], &flags);
        cross = NormalClip(screen[0], screen[1], screen[2]);
        average = AverageZ3(depth[0], depth[1], depth[2]) + (sint32)r_u32(0x800B69F0u);
        if ((uint32)average >= depth_limit)
            continue;
        bucket = ordering_table + (uint32)average * 4u;
        w_u32(packet, r_u32(bucket) | 0x06000000u);
        w_u32(bucket, packet);
        w_u32(packet + 4u, cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u);
        w_u32(packet + 8u, (uint32)screen[0]);
        w_u32(packet + 12u, (uint32)screen[1]);
        w_u32(packet + 16u, (uint32)screen[2]);
        w_u32(packet + 20u, (uint32)screen[0]);
        w_u32(packet + 24u, 0x55555555u);
        packet += 28u;
    }
    w_u32(0x800B6C00u, packet);
}

void mesh_dispatch_face_render_groups(uint32 first, uint32 second, uint32 groups)
{
    FUNCTION_MARKER(0x8001D9ECu, "MAIN.EXE");
    render_set_order_depth(first, second);
    mesh_render_tri_faces(r_u32(groups + 32u), (sint32)r_u32(groups), 36u);
    mesh_render_quad_faces(r_u32(groups + 36u), (sint32)r_u32(groups + 4u), 44u);
    mesh_render_tri_faces(r_u32(groups + 40u), (sint32)r_u32(groups + 8u), 40u);
    mesh_render_quad_faces(r_u32(groups + 44u), (sint32)r_u32(groups + 12u), 48u);
    mesh_render_tri_faces(r_u32(groups + 48u), (sint32)r_u32(groups + 16u), 60u);
    mesh_render_quad_faces(r_u32(groups + 52u), (sint32)r_u32(groups + 20u), 80u);
    mesh_render_tri_faces(r_u32(groups + 56u), (sint32)r_u32(groups + 24u), 64u);
    mesh_render_quad_faces(r_u32(groups + 60u), (sint32)r_u32(groups + 28u), 84u);
}

sint32 mesh_face_groups_project(uint32 descriptor)
{
    uint32 count;
    uint32 face;

    FUNCTION_MARKER(0x800329A8u, "MAIN.EXE");
    count = r_u32(descriptor + 8u);
    if (count != 0u)
    {
        face = r_u32(descriptor + 40u);
        do
        {
            sint32 screen_xy[3];
            sint32 shade[3];
            sint32 flags;
            sint32 vertex;

            gte_project3_full_depth((SVECTOR *)psx_addr(face, 3u * sizeof(SVECTOR)), screen_xy, shade, &flags);
            w_u32(face + 36u, r_u32(face + 36u) | 0x02000000u);
            for (vertex = 0; vertex < 3; ++vertex)
            {
                if (shade[vertex] < 0)
                    shade[vertex] = 0;
                if (shade[vertex] > 63)
                    shade[vertex] = 63;
                w_u16(face + (uint32)vertex * 8u + 6u, (uint16)((shade[vertex] << 8) | 0x80));
            }
            face += 40u;
        } while (--count != 0u);
    }
    count = r_u32(descriptor + 12u);
    if (count != 0u)
    {
        face = r_u32(descriptor + 44u);
        do
        {
            sint32 screen_xy[3];
            sint32 shade[4];
            sint32 flags;
            sint32 vertex;

            gte_project3_full_depth((SVECTOR *)psx_addr(face, 3u * sizeof(SVECTOR)), screen_xy, shade, &flags);
            w_u32(face + 44u, r_u32(face + 44u) | 0x02000000u);
            shade[3] = gte_project_full_depth((SVECTOR *)psx_addr(face + 24u, sizeof(SVECTOR)), 0, &flags);
            for (vertex = 0; vertex < 4; ++vertex)
            {
                if (shade[vertex] < 0)
                    shade[vertex] = 0;
                if (shade[vertex] > 63)
                    shade[vertex] = 63;
            }
            w_u16(face + 6u, (uint16)((shade[0] << 8) | 0x80));
            w_u16(face + 14u, (uint16)((shade[1] << 8) | 0x80));
            w_u16(face + 30u, (uint16)((shade[3] << 8) | 0x80));
            w_u16(face + 22u, (uint16)((shade[2] << 8) | 0x80));
            face += 48u;
        } while (--count != 0u);
    }
    return -1;
}

uint32 mesh_prepare_player_ot_packet(uint32 state, sint32 player)
{
    uint32 cursor = r_u32(0x800B682Cu);
    uint32 next = cursor + 1u;
    uint32 packet = 0x800C39D0u + (uint32)player * 13600u + cursor * 400u;
    uint32 chain = state + (uint32)player * 1784u + 1744u;

    FUNCTION_MARKER(0x80033028u, "MAIN.EXE");
    w_u32(0x800B682Cu, next);
    if (next == 34u)
        w_u32(0x800B682Cu, 0u);
    ClearOTagR(psx_addr(packet, 400u), 100);
    w_u32(packet, r_u32(chain));
    w_u32(chain, (packet + 396u) & 0xFFFFFFu);
    return packet;
}

sint32 mesh_vis_vert_project(const BOAT *boat)
{
    SVECTOR input;
    VECTOR transformed;
    sint32 screen;
    sint32 flags;
    uint32 depth;
    uint32 projection;
    sint32 radius;
    sint32 screen_x;
    sint32 screen_y;
    sint32 edge;

    FUNCTION_MARKER(0x800330F8u, "MAIN.EXE");
    input.vx = (sint16)(uint16)boat->motion.position[0];
    input.vy = (sint16)(uint16)boat->motion.position[1];
    input.vz = (sint16)(uint16)boat->motion.position[2];
    input.pad = 0;
    gte_transform(&input, &transformed, &flags);
    depth = (uint32)gte_project_full_depth(&input, &screen, &flags);
    projection = depth == 0u || (uint32)gte_read_h() >= depth * 2u ? 0x1FFFFu : mesh_gte_div(gte_read_h(), depth);
    radius = (sint32)(15u * projection) >> 12;
    if (radius > 4096)
        radius = 4096;
    screen_x = (sint16)screen;
    screen_y = (sint16)((uint32)screen >> 16);
    edge = (sint32)((uint32)screen_x + (uint32)radius);
    if (edge < 0)
        return 0;
    edge = (sint32)((uint32)screen_x - (uint32)radius);
    if ((sint32)r_u32(0x800B3DF8u) < edge)
        return 0;
    edge = (sint32)((uint32)screen_y + (uint32)radius);
    if (edge < 0)
        return 0;
    edge = (sint32)((uint32)screen_y - (uint32)radius);
    if ((sint32)r_u32(0x800B3DFCu) < edge)
        return 0;
    return (sint32)((uint32)(sint32)(sint16)transformed.vz + 240u) >= 0;
}

uint32 mesh_prepare_racer_ot_packet(uint32 state, uint32 bucket, sint32 player)
{
    uint32 cursor;
    uint32 packet;
    uint32 chain;

    FUNCTION_MARKER(0x800331F0u, "MAIN.EXE");
    if (bucket >= 250u)
        return 0u;
    cursor = r_u32(0x800B682Cu);
    packet = 0x800C39D0u + (uint32)player * 13600u + cursor * 400u;
    w_u32(0x800B682Cu, cursor + 1u);
    if (cursor + 1u == 34u)
        w_u32(0x800B682Cu, 0u);
    chain = state + (uint32)player * 1784u + 328u + bucket * 4u;
    ClearOTagR(psx_addr(packet, 400u), 100);
    w_u32(packet, r_u32(chain));
    w_u32(chain, (packet + 396u) & 0xFFFFFFu);
    return packet;
}

sint32 route_segment_is_in_window(uint32 entry, sint32 offset)
{
    uint32 descriptor;
    sint32 count;
    sint32 current;
    sint32 target;
    sint32 width;
    sint32 edge;
    sint32 first_width;
    sint32 second_width;
    sint32 direction;

    FUNCTION_MARKER(0x800332DCu, "MAIN.EXE");
    descriptor = camera_for_view(entry)->route;
    current = (sint32)((uint32)r_u16(descriptor) - 1u);
    if (current < 0)
        current = (sint32)((uint32)r_u16(descriptor + 2u) + 1u);
    count = (sint32)r_u32(0x800B6A98u);
    target = math_rem_s32(offset, count);
    if (target < 0)
        target = (sint32)((uint32)target + (uint32)count);
    first_width = (sint16)camera_for_view(entry)->visible[0];
    second_width = (sint16)camera_for_view(entry)->visible[1];
    width = (sint32)((uint32)first_width + (uint32)second_width);
    direction = (sint16)camera_for_view(entry)->direction;
    edge = (sint32)((uint32)current - (uint32)width);
    if (direction == 1)
    {
        edge = (sint32)((uint32)current + (uint32)width);
        if (count < edge)
            return target >= current || target <= (sint32)((uint32)edge - (uint32)count);
        return target >= current && target <= edge;
    }
    if (edge >= 0)
        return target >= edge && target <= current;
    return target >= (sint32)((uint32)edge + (uint32)count) || target <= current;
}

sint32 mesh_order_bucket(uint32 first, const sint32 position[3], sint16 *host_depth, sint32 *visible)
{
    MATRIX *matrix = &camera_for_view(first)->rotation;
    SVECTOR input;
    VECTOR transformed;
    sint32 depth;
    sint32 threshold;
    uint32 component;

    FUNCTION_MARKER(0x800333DCu, "MAIN.EXE");
    component = (uint16)position[0];
    input.vx = (sint16)(component + (uint16)camera_for_view(first)->position[0]);
    component = (uint16)position[1];
    input.vy = (sint16)(component + (uint16)camera_for_view(first)->position[1]);
    component = (uint16)camera_for_view(first)->position[2];
    input.vz = (sint16)(component + (uint16)position[2]);
    input.pad = 0;
    ApplyMatrix(matrix, &input, &transformed);
    threshold = (sint32)((uint32)transformed.vz + 200u);
    if (threshold < 0)
        threshold = (sint32)((uint32)transformed.vz + 203u);
    depth = (sint32)((uint32)transformed.vz - 600u);
    if (visible)
        *visible = threshold >= 0;
    if (threshold < 0)
        return 250;
    if (host_depth != NULL)
        *host_depth = (sint16)depth;
    return depth >= 0 ? depth >> 5 : 0;
}

sint32 mesh_calc_order_bucket(uint32 view, const BOAT *boat, uint32 output, sint16 *host_depth)
{
    const sint32 *position = boat->motion.position;
    sint16 depth;
    sint32 visible;
    sint32 result = mesh_order_bucket(view, position, &depth, &visible);
    if (visible)
    {
        if (host_depth)
            *host_depth = depth;
        else
            w_u16(output, (uint16)depth);
    }
    return result;
}

void mesh_render_racer_model(uint32 view, sint32 player)
{
    BOAT *candidates[32];
    uint32 orderings[32];
    uint16 buckets[32];
    sint16 sort_depths[32];
    sint32 count = 0;
    sint32 visible_limit;
    sint32 index;
    BOAT *carried_boat = NULL;

    FUNCTION_MARKER_ARGS(0x8003347Cu, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 2u, XPORT_CALL_GUEST_POINTER(view, 1u), XPORT_CALL_SCALAR((uint32)player));
#if defined(_DEBUG) && defined(_WIN32)
    if (player == 0 && (GetAsyncKeyState(VK_F8) & 1))
        mesh_color_probe_armed = 1u;
#endif
    if (r_u32(0x800B6988u) == 0u)
        return;
    if ((sint32)(uint32)vehicle_player(view)->contacts.points[0].height <= 0)
    {
        w_u32(0x800B698Cu, 0u);
        w_u32(0x800B6990u, 0u);
    }
    else
    {
        w_u32(0x800B698Cu, (uint32)(global_fn_8006e9d8() & 1));
        w_u32(0x800B6990u, (uint32)(global_fn_8006e9d8() & 3));
    }
    camera_bias(camera_for_view(view), 0x800843D4u);
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *racer = vehicle_racers[index];
        sint32 visible = 1;
        uint32 depth;
        sint16 sort_depth;

        carried_boat = racer;
        if (racer == vehicle_player(view) && (sint32)camera_for_view(view)->mode < 2)
            visible = 0;
        if (visible != 0 && (uint32)racer->control.driver == 0u && r_u32(0x80083478u) == 1u)
        {
            BOAT *current = vehicle_player(view);
            uint32 racer_route = (uint32)racer->race.progress;
            uint32 current_route = (uint32)current->race.progress;
            sint32 route_delta = (sint32)(racer_route - current_route);

            if (route_delta < 0)
                route_delta = (sint32)(0u - (uint32)route_delta);
            if (route_delta >= 101)
                visible = 0;
        }
        if (visible != 0 && r_u32(0x80083484u) != 8u)
        {
            visible = route_segment_is_in_window(view, (sint32)(uint32)racer->race.progress);
            if (visible != 0)
                visible = mesh_vis_vert_project(racer);
        }
        if (visible == 0)
        {
            if (r_u32(0x80083478u) == 1u)
                racer->trail.phase = 0u;
            continue;
        }
        depth = (uint32)mesh_calc_order_bucket(view, racer, 0u, &sort_depth);
        if (depth < 250u)
        {
            candidates[count] = racer;
            buckets[count] = (uint16)depth;
            sort_depths[count] = sort_depth;
            ++count;
        }
    }
    for (index = 1; index < count;)
    {
        sint32 previous = index - 1;

        if (sort_depths[index] < sort_depths[previous])
        {
            BOAT *candidate = candidates[index];
            uint16 bucket = buckets[index];
            sint16 sort_depth = sort_depths[index];

            carried_boat = candidate;
            candidates[index] = candidates[previous];
            sort_depths[index] = sort_depths[previous];
            buckets[index] = buckets[previous];
            candidates[previous] = candidate;
            sort_depths[previous] = sort_depth;
            buckets[previous] = bucket;
            if (index >= 2)
            {
                index = previous;
                continue;
            }
        }
        ++index;
    }
    if (camera_for_view(view)->mode == 2u)
    {
        if (count > 0)
        {
            BOAT *current = vehicle_player(view);

            for (index = 0; index < count; ++index)
            {
                if (candidates[index] == current)
                {
                    BOAT *first_candidate = candidates[0];
                    uint16 first_bucket = buckets[0];
                    sint16 first_sort_depth = sort_depths[0];

                    candidates[index] = first_candidate;
                    buckets[index] = first_bucket;
                    sort_depths[index] = first_sort_depth;
                    buckets[0] = 0u;
                    candidates[0] = vehicle_player(view);
                    break;
                }
            }
        }
    }
    visible_limit = 3;
    if ((sint32)camera_for_view(view)->mode < 2)
        visible_limit = 2;
    if (r_u32(0x80083478u) == 2u)
        visible_limit = 8;
    for (index = 0; index < count; ++index)
    {
        BOAT *racer = candidates[index];

        if (index < visible_limit)
        {
            if (racer->trail.phase == 0u)
                racer->trail.phase = 1u;
        }
        else if (r_u32(0x80083478u) == 1u)
            racer->trail.phase = 0u;
        orderings[index] = mesh_prepare_racer_ot_packet(view, buckets[index], player);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        uint32 descriptor;
        uint32 model;
        uint32 view_index;

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        SetRotMatrix(&racer->contacts.ground);
        SetTransMatrix(&racer->contacts.ground);
        view_index = r_u32(view + 4u);
        model = racer->model_slot;
        descriptor = r_u32(0x80101788u + view_index * 64u + model * 4u);
        mesh_face_groups_project(descriptor);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        w_u32(0x800843E8u, (uint32)racer->trail.origin[0]);
        w_u32(0x800843ECu, (uint32)racer->trail.origin[1]);
        w_u32(0x800843F0u, (uint32)racer->trail.origin[2]);
        camera_bias(camera_for_view(view), 0x800843D4u);
        w_u32(0x800843E8u, 0u);
        w_u32(0x800843ECu, 0u);
        w_u32(0x800843F0u, 0u);
        trail_render(racer, orderings[index], 100);
        trail_render_spray(racer, orderings[index], 100);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        uint32 model;
        uint32 view_index;
        uint32 descriptor;

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        if (r_u32(0x80083478u) == 2u && racer != vehicle_player(view))
        {
            camera_bias(camera_for_view(view), 0x800843D4u);
            trail_render_marker(racer, orderings[index]);
        }
        camera_config_lighting(racer, &racer->motion.transform.matrix);
        camera_bias_matrix(camera_for_view(view), &racer->motion.transform.matrix, 0);
        if (buckets[index] < 10u)
            render_scale_proj_transform(2);
        else if (buckets[index] < 20u)
            render_scale_proj_transform(1);
        view_index = r_u32(view + 4u);
        model = racer->model_slot;
        descriptor = r_u32(0x80101788u + view_index * 64u + model * 4u);
#if defined(_DEBUG)
        mesh_color_probe_player = racer == vehicle_player(view) ? racer : NULL;
        mesh_color_probe_groups = descriptor;
#endif
        mesh_dispatch_renderers(orderings[index], 100u, descriptor);
#if defined(_DEBUG)
        mesh_color_probe_player = NULL;
#endif
        render_scale_proj_transform(0);
        if (racer != vehicle_player(view) || camera_for_view(view)->mode != 2u)
        {
            const uint32 temporary = 0x1F800220u;
            MATRIX source;
            MATRIX accessory;
            uint16 matrix_pad;
            const BOAT_SETUP *setup;
            sint32 mode;
            sint32 saved_bias;

            source = racer->motion.transform.matrix;
            SetRotMatrix(&source);
            SetTransMatrix(&source);
            accessory = source;
            memcpy(&matrix_pad, (const uint8 *)&source + 18u, sizeof(matrix_pad));
            mesh_rotate_mat_y((sint32)(uint32)racer->control.steering / 2, &accessory);
            setup = (&racer->setup);
            mode = (sint32)setup->propellers;
            saved_bias = (sint32)r_u32(0x800B69F0u);
            if (mode == 1 || mode == 2)
            {
                SVECTOR point;
                VECTOR positions[2];
                sint32 position_count = mode;
                sint32 position_index;

                if (mode == 1)
                {
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[0]);
                }
                else
                {
                    uint16 displacement = r_u16(0x80089978u + setup->model_index * 4u);

                    racer->trail.propeller.vx = (sint16)displacement;
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[0]);
                    setup = (&racer->setup);
                    displacement = r_u16(0x80089978u + setup->model_index * 4u);
                    racer->trail.propeller.vx = (sint16)(uint16)(0u - (uint32)displacement);
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[1]);
                    racer->trail.propeller.vx = 0;
                }
                for (position_index = 0; position_index < position_count; ++position_index)
                {
                    uint32 table = position_index == 0 ? 0x800DDD18u : 0x800DDD58u;

                    accessory.t[0] = positions[position_index].vx;
                    accessory.t[1] = positions[position_index].vy;
                    accessory.t[2] = positions[position_index].vz;
                    mesh_write_mat(temporary, &accessory, matrix_pad);
                    camera_config_lighting(racer, &accessory);
                    camera_bias(camera_for_view(view), temporary);
                    view_index = r_u32(view + 4u);
                    model = racer->model_slot;
                    descriptor = r_u32(table + view_index * 128u + model * 4u);
                    w_u32(0x800B69F0u, (uint32)saved_bias);
                    mesh_dispatch_renderers(orderings[index], 100u, descriptor);
                    if ((uint32)racer->control.mode == 1u)
                        trail_render_flare(racer, orderings[index], 100u);
                }
            }
        }
    }
    if (r_u32(0x80083484u) == 4u && replay_state.playing != 0u)
    {
        uint32 route = replay_state.contact.object;
        sint32 route_offset = (sint32)((uint32)r_u16(route) - 1u);

        if (route_offset < 0)
            route_offset = (sint32)((uint32)r_u16(route + 2u) + 1u);
        if (route_segment_is_in_window(view, route_offset) != 0)
        {
            sint16 sort_depth;
            uint32 depth = (uint32)mesh_order_bucket(view, replay_state.position, &sort_depth, NULL);

            if (depth < 250u)
            {
                uint32 ordering = mesh_prepare_racer_ot_packet(view, depth, player);
                uint32 view_index;
                uint32 model;
                uint32 descriptor;

                camera_bias_matrix(camera_for_view(view), &replay_state.matrix, 0);
                view_index = r_u32(view + 4u);
                model = carried_boat ? carried_boat->model_slot : 0u;
                descriptor = r_u32(0x80101788u + view_index * 64u + model * 4u);
                mesh_dispatch_face_render_groups(ordering, 100u, descriptor);
            }
        }
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        uint32 ordering;
        uint32 model;
        uint32 view_index;
        uint32 descriptor;
        const BOAT_SETUP *setup;

        if (racer->trail.phase == 2u)
            continue;
        ordering = mesh_prepare_racer_ot_packet(view, buckets[index], player);
        if (r_u32(0x80083478u) == 2u && racer != vehicle_player(view))
        {
            camera_bias(camera_for_view(view), 0x800843D4u);
            trail_render_marker(racer, ordering);
        }
        camera_bias_matrix(camera_for_view(view), &racer->motion.transform.matrix, 0);
        view_index = r_u32(view + 4u);
        model = racer->model_slot;
        descriptor = r_u32(0x800E8DA8u + view_index * 64u + model * 4u);
        mesh_render_lit_quads(ordering, descriptor, &racer->contacts.points[0].normal);
        setup = (&racer->setup);
        if ((sint32)setup->propellers > 0)
        {
            view_index = r_u32(view + 4u);
            model = racer->model_slot;
            descriptor = r_u32(0x800F0488u + view_index * 64u + model * 4u);
            mesh_render_lit_quads(ordering, descriptor, &racer->contacts.points[0].normal);
        }
    }
}

void mesh_render_lit_quads(uint32 ordering, uint32 descriptor, const SVECTOR *normal)
{
    sint32 count;
    uint32 face;
    uint32 packet;
    uint32 color;
    sint32 previous_cross = -1;
    sint32 previous_depth = 0;
    uint32 remaining;

    FUNCTION_MARKER_ARGS(0x80033F54u, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 3u, XPORT_CALL_GUEST_POINTER(ordering, 4u), XPORT_CALL_GUEST_POINTER(descriptor, 48u), XPORT_CALL_HOST_POINTER(normal, sizeof(*normal)));
    if (descriptor == 0u)
        return;
    count = (sint32)r_u32(descriptor + 12u);
    face = r_u32(descriptor + 44u);
    if (count == 0)
        return;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    {
        uint32 source_color = r_u32(face + 44u);

        color = gte_normal_color_col(normal, source_color);
    }
    remaining = (uint32)count;
    while (remaining-- != 0u)
    {
        sint32 screen[4];
        sint32 depths[4];
        sint32 flags;
        sint32 vertex;
        sint32 cross;

        for (vertex = 0; vertex < 3; ++vertex)
            depths[vertex] = gte_project_full_depth((SVECTOR *)psx_addr(face + (uint32)vertex * 8u, sizeof(SVECTOR)), &screen[vertex], &flags);
        {
            sint32 depth = (sint32)((uint32)previous_depth + r_u32(0x800B69F0u));

            previous_depth = depth;
            if (previous_cross > 0 && (uint32)depth < 99u)
            {
                uint32 bucket = ordering + (uint32)depth * 4u;

                w_u32(packet, r_u32(bucket) | 0x09000000u);
                w_u32(bucket, packet);
                packet += 40u;
            }
        }
        cross = NormalClip(screen[0], screen[1], screen[2]);
        previous_cross = cross;
        if (cross > 0)
        {
            uint32 high;
            uint32 low;
            sint32 average;

            w_u32(packet + 8u, (uint32)screen[0]);
            w_u32(packet + 16u, (uint32)screen[1]);
            w_u32(packet + 24u, (uint32)screen[2]);
            depths[3] = gte_project_full_depth((SVECTOR *)psx_addr(face + 24u, sizeof(SVECTOR)), &screen[3], &flags);
            high = r_u16(face + 42u);
            low = r_u16(face + 6u);
            w_u32(packet + 12u, low | (high << 16));
            high = r_u16(face + 40u);
            low = r_u16(face + 14u);
            w_u32(packet + 20u, low | (high << 16));
            w_u16(packet + 28u, r_u16(face + 22u));
            w_u16(packet + 36u, r_u16(face + 30u));
            w_u32(packet + 32u, (uint32)screen[3]);
            average = AverageZ4(depths[0], depths[1], depths[2], depths[3]);
            w_u32(packet + 4u, color);
            previous_depth = average;
        }
        face += 48u;
    }
    {
        sint32 depth = (sint32)((uint32)previous_depth + r_u32(0x800B69F0u));

        previous_depth = depth;
        if (previous_cross > 0 && (uint32)depth < 99u)
        {
            uint32 bucket = ordering + (uint32)depth * 4u;

            w_u32(packet, r_u32(bucket) | 0x09000000u);
            w_u32(bucket, packet);
            packet += 40u;
        }
    }
    w_u32(0x800B6C00u, packet);
}

sint32 mesh_init_tex_templates(void)
{
    FUNCTION_MARKER(0x80034158u, "MAIN.EXE");
    trail_init_tex_templates8();
    return trail_init_quad_templates5();
}

void mesh_fn_80034180(void)
{
    FUNCTION_MARKER(0x80034180u, "MAIN.EXE");
}

sint32 mesh_fn_8003429c(uint32 state)
{
    uint32 packed = r_u32(0x800B6840u);
    uint32 palette = r_u32(0x800B684Cu);
    sint32 pass;

    FUNCTION_MARKER(0x8003429Cu, "MAIN.EXE");
    for (pass = 0; pass < 2; ++pass)
    {
        uint32 source = packed;
        uint32 destination = state + (pass == 0 ? 32u : 1568u);
        sint32 row;
        for (row = 0; row < 8; ++row)
        {
            uint32 low = r_u32(source);
            uint32 high = r_u32(source + 8u);
            sint32 column;
            source += 12u;
            for (column = 0; column < 15; ++column)
            {
                uint32 left;
                uint32 right;
                if (column == 8)
                {
                    low = high;
                    high = r_u32(source + 8u);
                    source += 4u;
                }
                left = low & 15u;
                right = high & 15u;
                low >>= 4;
                high >>= 4;
                w_u32(destination + (uint32)column * 32u, (uint32)r_u16(palette + left * 2u) | ((uint32)r_u16(palette + right * 2u) << 16));
            }
            destination += 4u;
            source += 8u;
        }
    }
    w_u32(state + 24u, 1u);
    w_u32(state + 28u, 3u);
    w_u32(state, 0x80080E6Cu);
    w_u32(state + 20u, 0xFFFFFFF0u);
    w_u32(state + 12u, 0u);
    w_u32(state + 8u, 0x80034638u);
    w_u32(state + 16u, r_u32(state));
    xport_update_u32(r_u32(0x800B6850u) + 8u, XPORT_MEMORY_UPDATE_OR, 8u);
    return (sint32)r_u32(r_u32(0x800B6850u) + 8u);
}
