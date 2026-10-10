#include "scene.h"
#include "route.h"
#include "game.h"
#include "polygon.h"
#include "display.h"
#include "global.h"
#include "mesh.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

// Shared billboard vertex colors from both original polygon passes
static const uint32 poly_billboard_colors[8] = {
    0x3C000000u, 0x3CE0F0F0u, 0x3C0000FFu, 0x3C10E0E0u,
    0x3C80FF80u, 0x3CFF8080u, 0x3C000000u, 0x3C000000u,
};

// Pack material fields only at the PSX polygon packet boundary
static void poly_material_words(uint32 index, uint32 *page, uint32 *uv0, uint32 *uv1)
{
    const RENDER_MATERIAL *material = render_material(index);
    *page = material->tpage | ((uint32)material->clut << 16);
    *uv0 = material->uv[0].u | ((uint32)material->uv[0].v << 8) | ((uint32)material->uv[1].u << 16) | ((uint32)material->uv[1].v << 24);
    *uv1 = material->uv[2].u | ((uint32)material->uv[2].v << 8) | ((uint32)material->uv[3].u << 16) | ((uint32)material->uv[3].v << 24);
}

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

static uint32 *poly_order_bucket(uint32 *ot, sint32 relative_depth)
{
    return relative_depth < 0 ? ot : ot + 1 + (uint32)(relative_depth >> 5);
}

void poly_dispatch_prim_groups(const SCENE_PRIM_GROUP *src, uint32 slot, uint32 *ot, sint32 adjusted, sint32 alternate)
{
    uint32 count;
    const SCENE_PRIM_FACE *vertices;
    const void *prims;

    if (slot >= 2u)
        abort();
    if (adjusted != 0)
    {
        count = src->sets[0].count;
        vertices = src->sets[0].faces;
        prims = count ? src->prims[slot][0].f3 + count : NULL;
        (alternate != 0 ? poly_render_tri_alt_flat_strip : poly_render_tri_flat_strip)(vertices, prims, (sint32)count, ot);
        count = src->sets[1].count;
        vertices = src->sets[1].faces;
        prims = count ? src->prims[slot][1].f4 + count : NULL;
        (alternate != 0 ? poly_render_quad_alt_flat_strip : poly_render_quad_flat_strip)(vertices, prims, (sint32)count, ot);
        count = src->sets[4].count;
        vertices = src->sets[4].faces;
        prims = count ? src->prims[slot][4].g3 + count : NULL;
        (alternate != 0 ? poly_render_tri_alt_shaded_strip : poly_render_tri_shaded_strip)(vertices, prims, (sint32)count, ot);
        count = src->sets[5].count;
        vertices = src->sets[5].faces;
        prims = count ? src->prims[slot][5].g4 + count : NULL;
        (alternate != 0 ? poly_render_quad_alt_shaded_strip : poly_render_quad_shaded_strip)(vertices, prims, (sint32)count, ot);
        vertices = src->sets[2].faces;
        count = src->sets[2].count;
        prims = count ? src->prims[slot][2].ft3 + count : NULL;
        (alternate != 0 ? poly_render_tri_alt_tex_strip : poly_render_tri_tex_strip)(vertices, prims, (sint32)count, ot);
        if (alternate != 0)
        {
            count = src->sets[3].count;
            vertices = src->sets[3].faces;
            prims = count ? src->prims[slot][3].ft4 + count : NULL;
            poly_render_quad_alt_tex_strip(vertices, prims, (sint32)count, ot);
        }
        else
        {
            uint32 mode = r_u32(0x800834A0u);

            count = src->sets[3].count;
            vertices = src->sets[3].faces;
            prims = count ? src->prims[slot][3].ft4 + count : NULL;
            if (mode == 1u)
                poly_render_quad_clipped_tex_strip(vertices, prims, (sint32)count, ot);
            else
                poly_render_quad_tex_strip(vertices, prims, (sint32)count, ot);
        }
        count = src->sets[6].count;
        vertices = src->sets[6].faces;
        prims = count ? src->prims[slot][6].gt3 + count : NULL;
        (alternate != 0 ? poly_render_tri_mixed_strip : poly_render_tri_tex_shaded_strip)(vertices, prims, (sint32)count, ot);
        count = src->sets[7].count;
        vertices = src->sets[7].faces;
        prims = count ? src->prims[slot][7].gt4 + count : NULL;
        (alternate != 0 ? poly_render_quad_mixed_strip : poly_render_quad_tex_shaded_strip)(vertices, prims, (sint32)count, ot);
        return;
    }

    vertices = src->sets[0].faces;
    prims = src->prims[slot][0].f3;
    count = src->sets[0].count;
    (alternate != 0 ? poly_render_tri_alt_flat_strip : poly_render_tri_flat_strip)(vertices, prims, (sint32)count, ot);
    vertices = src->sets[1].faces;
    prims = src->prims[slot][1].f4;
    count = src->sets[1].count;
    (alternate != 0 ? poly_render_quad_alt_flat_strip : poly_render_quad_flat_strip)(vertices, prims, (sint32)count, ot);
    vertices = src->sets[4].faces;
    prims = src->prims[slot][4].g3;
    count = src->sets[4].count;
    (alternate != 0 ? poly_render_tri_alt_shaded_strip : poly_render_tri_shaded_strip)(vertices, prims, (sint32)count, ot);
    vertices = src->sets[5].faces;
    prims = src->prims[slot][5].g4;
    count = src->sets[5].count;
    (alternate != 0 ? poly_render_quad_alt_shaded_strip : poly_render_quad_shaded_strip)(vertices, prims, (sint32)count, ot);
    vertices = src->sets[2].faces;
    prims = src->prims[slot][2].ft3;
    count = src->sets[2].count;
    (alternate != 0 ? poly_render_tri_alt_tex_strip : poly_render_tri_tex_strip)(vertices, prims, (sint32)count, ot);
    if (alternate != 0)
    {
        vertices = src->sets[3].faces;
        prims = src->prims[slot][3].ft4;
        count = src->sets[3].count;
        poly_render_quad_alt_tex_strip(vertices, prims, (sint32)count, ot);
    }
    else
    {
        uint32 mode = r_u32(0x800834A0u);

        vertices = src->sets[3].faces;
        prims = src->prims[slot][3].ft4;
        count = src->sets[3].count;
        if (mode == 1u)
            poly_render_quad_clipped_tex_strip(vertices, prims, (sint32)count, ot);
        else
            poly_render_quad_tex_strip(vertices, prims, (sint32)count, ot);
    }
    vertices = src->sets[6].faces;
    prims = src->prims[slot][6].gt3;
    count = src->sets[6].count;
    (alternate != 0 ? poly_render_tri_mixed_strip : poly_render_tri_tex_shaded_strip)(vertices, prims, (sint32)count, ot);
    vertices = src->sets[7].faces;
    prims = src->prims[slot][7].gt4;
    count = src->sets[7].count;
    (alternate != 0 ? poly_render_quad_mixed_strip : poly_render_quad_tex_shaded_strip)(vertices, prims, (sint32)count, ot);
}


static sint32 poly_project_mesh_vertex(const ROUTE_VERTEX *record, sint32 origin_x, sint32 origin_y, sint32 origin_z, sint32 *screen)
{
    SVECTOR point;
    sint32 flags;

    point.vx = (sint16)(origin_x + 16 * record->position[0]);
    point.vy = (sint16)(origin_y + 16 * record->position[1]);
    point.vz = (sint16)(origin_z + 16 * record->position[2]);
    point.pad = 0;
    return gte_project_full_depth(&point, screen, &flags);
}

static uint16 mesh_texture_coordinate(const ROUTE_VERTEX *record, uint32 phase)
{
    uint32 index = (2u * record->texture_phase + phase) & 0x1FEu;

    return (uint16)(render_curve[2][index] + 0x40u);
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

POLY_G3 *poly_subdiv_tri_shaded(POLY_G3 *prim, POLY_G3 *dst)
{
    POLY_SHADED_VERTEX vertices[3];
    POLY_SHADED_VERTEX midpoint[3];
    uint32 tag;
    uint8 shade;

    FUNCTION_MARKER(0x80079CACu, "MAIN.EXE");
    shade = prim->code;
    prim->pad1 = shade;
    prim->pad2 = shade;
    tag = prim->tag;
    vertices[0].xy = prim->xy0;
    vertices[0].color = prim->color0;
    vertices[1].xy = prim->xy1;
    vertices[1].color = prim->color1;
    vertices[2].xy = prim->xy2;
    vertices[2].color = prim->color2;
    midpoint[0] = poly_tri_shaded_verts_avg(vertices[0], vertices[1], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[1] = poly_tri_shaded_verts_avg(vertices[1], vertices[2], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[2] = poly_tri_shaded_verts_avg(vertices[2], vertices[0], UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_copy_tri_shaded_pkt_planes(vertices[0], dst, tag, midpoint[0], midpoint[2]);
    AddPrim(&tag, dst);
    poly_copy_tri_shaded_pkt_planes(midpoint[2], dst + 1, tag, midpoint[1], vertices[2]);
    AddPrim(&tag, dst + 1);
    poly_copy_tri_shaded_pkt_planes(midpoint[1], dst + 2, tag, midpoint[2], midpoint[0]);
    AddPrim(&tag, dst + 2);
    poly_copy_tri_shaded_pkt_planes(midpoint[0], prim, tag, vertices[1], midpoint[1]);
    return dst + 3;
}

void poly_copy_tri_shaded_pkt_planes(POLY_SHADED_VERTEX v0, POLY_G3 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2)
{
    FUNCTION_MARKER(0x80079D64u, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->color0 = v0.color;
    dst->color1 = v1.color;
    dst->color2 = v2.color;
    dst->tag = tag;
}

POLY_SHADED_VERTEX poly_tri_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask)
{
    POLY_SHADED_VERTEX result;
    uint32 left = v0.color;
    uint32 right = v1.color;

    FUNCTION_MARKER(0x80079DA8u, "MAIN.EXE");
    result.color = (left + right - ((left ^ right) & first_mask)) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = (left + right - ((left ^ right) & second_mask)) >> 1;
    return result;
}

POLY_GT3 *poly_subdiv_tri_tex_shaded(POLY_GT3 *prim, POLY_GT3 *dst)
{
    POLY_TEX_SHADED_VERTEX vertices[3];
    POLY_TEX_SHADED_VERTEX midpoint[3];
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;
    uint8 shade;

    FUNCTION_MARKER(0x80079DF4u, "MAIN.EXE");
    shade = prim->code;
    prim->p1 = shade;
    prim->p2 = shade;
    tag = prim->tag;
    vertices[0].xy = prim->xy0;
    vertices[0].color = prim->color0;
    vertices[0].uv = prim->uv0;
    vertices[1].xy = prim->xy1;
    vertices[1].color = prim->color1;
    vertices[1].uv = prim->uv1;
    vertices[2].xy = prim->xy2;
    vertices[2].color = prim->color2;
    vertices[2].uv = prim->uv2;
    midpoint[0] = poly_tri_tex_shaded_verts_avg(vertices[0], vertices[1], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[1] = poly_tri_tex_shaded_verts_avg(vertices[1], vertices[2], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[2] = poly_tri_tex_shaded_verts_avg(vertices[2], vertices[0], UINT32_C(0x01010101), UINT32_C(0x00010001));
    clut_word = (uint32)prim->clut << 16;
    tpage_word = (uint32)prim->tpage << 16;
    poly_copy_tri_tex_shaded_pkt_planes(vertices[0], dst, tag, midpoint[0], midpoint[2], clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_tri_tex_shaded_pkt_planes(midpoint[2], dst + 1, tag, midpoint[1], vertices[2], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_tri_tex_shaded_pkt_planes(midpoint[1], dst + 2, tag, midpoint[2], midpoint[0], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_tri_tex_shaded_pkt_planes(midpoint[0], prim, tag, vertices[1], midpoint[1], clut_word, tpage_word);
    return dst + 3;
}

void poly_copy_tri_tex_shaded_pkt_planes(POLY_TEX_SHADED_VERTEX v0, POLY_GT3 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x80079EBCu, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->color0 = v0.color;
    dst->color1 = v1.color;
    dst->color2 = v2.color;
    dst->uv0 = v0.uv;
    dst->uv1 = v1.uv;
    dst->uv2 = v2.uv;
    dst->clut = (uint16)(clut_word >> 16);
    dst->tpage = (uint16)(tpage_word >> 16);
    dst->tag = tag;
}

POLY_TEX_SHADED_VERTEX poly_tri_tex_shaded_verts_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask)
{
    POLY_TEX_SHADED_VERTEX result;
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x80079F20u, "MAIN.EXE");
    left = v0.color;
    right = v1.color;
    result.color = (left + right - ((left ^ right) & first_mask)) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = (left + right - ((left ^ right) & second_mask)) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = (uint16)((left + right - ((left ^ right) & first_mask)) >> 1);
    return result;
}

POLY_F3 *poly_subdiv_tri_flat(POLY_F3 *prim, POLY_F3 *dst)
{
    uint32 vertices[3];
    uint32 color;
    uint32 midpoint[3];
    uint32 tag;

    FUNCTION_MARKER(0x80079F90u, "MAIN.EXE");
    tag = prim->tag;
    color = prim->color0;
    vertices[0] = prim->xy0;
    vertices[1] = prim->xy1;
    vertices[2] = prim->xy2;
    midpoint[0] = poly_tri_flat_verts_avg(vertices[0], vertices[1], UINT32_C(0x00010001));
    midpoint[1] = poly_tri_flat_verts_avg(vertices[1], vertices[2], UINT32_C(0x00010001));
    midpoint[2] = poly_tri_flat_verts_avg(vertices[2], vertices[0], UINT32_C(0x00010001));
    poly_copy_tri_flat_verts(color, vertices[0], dst, tag, midpoint[0], midpoint[2]);
    AddPrim(&tag, dst);
    poly_copy_tri_flat_verts(color, midpoint[2], dst + 1, tag, midpoint[1], vertices[2]);
    AddPrim(&tag, dst + 1);
    poly_copy_tri_flat_verts(color, midpoint[1], dst + 2, tag, midpoint[2], midpoint[0]);
    AddPrim(&tag, dst + 2);
    poly_copy_tri_flat_verts(color, midpoint[0], prim, tag, vertices[1], midpoint[1]);
    return dst + 3;
}

void poly_copy_tri_flat_verts(uint32 color, uint32 v0, POLY_F3 *dst, uint32 tag, uint32 v1, uint32 v2)
{
    FUNCTION_MARKER(0x8007A038u, "MAIN.EXE");
    dst->xy0 = v0;
    dst->xy1 = v1;
    dst->xy2 = v2;
    dst->color0 = color;
    dst->tag = tag;
}

uint32 poly_tri_flat_verts_avg(uint32 v0, uint32 v1, uint32 mask)
{
    FUNCTION_MARKER(0x8007A06Cu, "MAIN.EXE");
    return (v0 + v1 - ((v0 ^ v1) & mask)) >> 1;
}

POLY_FT3 *poly_subdiv_tri_tex(POLY_FT3 *prim, POLY_FT3 *dst)
{
    POLY_TEX_VERTEX vertices[3];
    uint32 color;
    POLY_TEX_VERTEX midpoint[3];
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;

    FUNCTION_MARKER(0x8007A094u, "MAIN.EXE");
    tag = prim->tag;
    color = prim->color0;
    vertices[0].xy = prim->xy0;
    vertices[0].uv = prim->uv0;
    vertices[1].xy = prim->xy1;
    vertices[1].uv = prim->uv1;
    vertices[2].xy = prim->xy2;
    vertices[2].uv = prim->uv2;
    midpoint[0] = poly_tri_tex_verts_avg(vertices[0], vertices[1], UINT32_C(0x00010001), UINT32_C(0x01010101));
    midpoint[1] = poly_tri_tex_verts_avg(vertices[1], vertices[2], UINT32_C(0x00010001), UINT32_C(0x01010101));
    midpoint[2] = poly_tri_tex_verts_avg(vertices[2], vertices[0], UINT32_C(0x00010001), UINT32_C(0x01010101));
    clut_word = (uint32)prim->clut << 16;
    tpage_word = (uint32)prim->tpage << 16;
    poly_copy_tri_tex_verts(color, vertices[0], dst, tag, midpoint[0], midpoint[2], clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_tri_tex_verts(color, midpoint[2], dst + 1, tag, midpoint[1], vertices[2], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_tri_tex_verts(color, midpoint[1], dst + 2, tag, midpoint[2], midpoint[0], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_tri_tex_verts(color, midpoint[0], prim, tag, vertices[1], midpoint[1], clut_word, tpage_word);
    return dst + 3;
}

void poly_copy_tri_tex_verts(uint32 color, POLY_TEX_VERTEX v0, POLY_FT3 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A14Cu, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->color0 = color;
    dst->uv0 = v0.uv;
    dst->uv1 = v1.uv;
    dst->uv2 = v2.uv;
    dst->clut = (uint16)(clut_word >> 16);
    dst->tpage = (uint16)(tpage_word >> 16);
    dst->tag = tag;
}

POLY_TEX_VERTEX poly_tri_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 word_mask, uint32 half_mask)
{
    POLY_TEX_VERTEX result;
    uint32 left = v0.xy;
    uint32 right = v1.xy;

    FUNCTION_MARKER(0x8007A1A0u, "MAIN.EXE");
    result.xy = (left + right - ((left ^ right) & word_mask)) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = (uint16)((left + right - ((left ^ right) & half_mask)) >> 1);
    return result;
}

POLY_G4 *poly_subdiv_quad_shaded(POLY_G4 *prim, POLY_G4 *dst)
{
    POLY_SHADED_VERTEX vertices[4];
    POLY_SHADED_VERTEX midpoint[4];
    POLY_SHADED_VERTEX center;
    uint32 tag;
    uint8 shade;

    FUNCTION_MARKER(0x8007A1ECu, "MAIN.EXE");
    shade = prim->code;
    prim->pad1 = shade;
    prim->pad2 = shade;
    prim->pad3 = shade;
    tag = prim->tag;
    vertices[0].xy = prim->xy0;
    vertices[0].color = prim->color0;
    vertices[1].xy = prim->xy1;
    vertices[1].color = prim->color1;
    vertices[2].xy = prim->xy2;
    vertices[2].color = prim->color2;
    vertices[3].xy = prim->xy3;
    vertices[3].color = prim->color3;
    midpoint[0] = poly_quad_shaded_verts_avg(vertices[0], vertices[1], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[1] = poly_quad_shaded_verts_avg(vertices[1], vertices[3], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[2] = poly_quad_shaded_verts_avg(vertices[3], vertices[2], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[3] = poly_quad_shaded_verts_avg(vertices[2], vertices[0], UINT32_C(0x01010101), UINT32_C(0x00010001));
    center = poly_quad_shaded_verts_avg(midpoint[1], midpoint[3], UINT32_C(0x01010101), UINT32_C(0x00010001));
    poly_copy_quad_shaded_pkt_planes(vertices[0], dst, tag, midpoint[0], midpoint[3], center);
    AddPrim(&tag, dst);
    poly_copy_quad_shaded_pkt_planes(midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1]);
    AddPrim(&tag, dst + 1);
    poly_copy_quad_shaded_pkt_planes(center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3]);
    AddPrim(&tag, dst + 2);
    poly_copy_quad_shaded_pkt_planes(midpoint[3], prim, tag, center, vertices[2], midpoint[2]);
    return dst + 3;
}

void poly_copy_quad_shaded_pkt_planes(POLY_SHADED_VERTEX v0, POLY_G4 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2, POLY_SHADED_VERTEX v3)
{
    FUNCTION_MARKER(0x8007A2D8u, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->xy3 = v3.xy;
    dst->color0 = v0.color;
    dst->color1 = v1.color;
    dst->color2 = v2.color;
    dst->color3 = v3.color;
    dst->tag = tag;
}

POLY_SHADED_VERTEX poly_quad_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask)
{
    FUNCTION_MARKER(0x8007A32Cu, "MAIN.EXE");
    return poly_tri_shaded_verts_avg(v0, v1, first_mask, second_mask);
}

POLY_GT4 *poly_subdiv_quad_tex_shaded(POLY_GT4 *prim, POLY_GT4 *dst)
{
    POLY_TEX_SHADED_VERTEX vertices[4];
    POLY_TEX_SHADED_VERTEX midpoint[4];
    POLY_TEX_SHADED_VERTEX center;
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;
    uint8 shade;

    FUNCTION_MARKER(0x8007A378u, "MAIN.EXE");
    shade = prim->code;
    prim->p1 = shade;
    prim->p2 = shade;
    prim->p3 = shade;
    tag = prim->tag;
    vertices[0].xy = prim->xy0;
    vertices[0].color = prim->color0;
    vertices[0].uv = prim->uv0;
    vertices[1].xy = prim->xy1;
    vertices[1].color = prim->color1;
    vertices[1].uv = prim->uv1;
    vertices[2].xy = prim->xy2;
    vertices[2].color = prim->color2;
    vertices[2].uv = prim->uv2;
    vertices[3].xy = prim->xy3;
    vertices[3].color = prim->color3;
    vertices[3].uv = prim->uv3;
    midpoint[0] = poly_masked_vert_pairs_avg(vertices[0], vertices[1], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[1] = poly_masked_vert_pairs_avg(vertices[1], vertices[3], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[2] = poly_masked_vert_pairs_avg(vertices[3], vertices[2], UINT32_C(0x01010101), UINT32_C(0x00010001));
    midpoint[3] = poly_masked_vert_pairs_avg(vertices[2], vertices[0], UINT32_C(0x01010101), UINT32_C(0x00010001));
    center = poly_masked_vert_pairs_avg(midpoint[1], midpoint[3], UINT32_C(0x01010101), UINT32_C(0x00010001));
    clut_word = (uint32)prim->clut << 16;
    tpage_word = (uint32)prim->tpage << 16;
    poly_copy_pkt_subdiv_attrs(vertices[0], dst, tag, midpoint[0], midpoint[3], center, clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_pkt_subdiv_attrs(midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_pkt_subdiv_attrs(center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_pkt_subdiv_attrs(midpoint[3], prim, tag, center, vertices[2], midpoint[2], clut_word, tpage_word);
    return dst + 3;
}

void poly_copy_pkt_subdiv_attrs(POLY_TEX_SHADED_VERTEX v0, POLY_GT4 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, POLY_TEX_SHADED_VERTEX v3, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A474u, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->xy3 = v3.xy;
    dst->color0 = v0.color;
    dst->color1 = v1.color;
    dst->color2 = v2.color;
    dst->color3 = v3.color;
    dst->uv0 = v0.uv;
    dst->uv1 = v1.uv;
    dst->uv2 = v2.uv;
    dst->uv3 = v3.uv;
    dst->clut = (uint16)(clut_word >> 16);
    dst->tpage = (uint16)(tpage_word >> 16);
    dst->tag = tag;
}

POLY_TEX_SHADED_VERTEX poly_masked_vert_pairs_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask)
{
    POLY_TEX_SHADED_VERTEX result;
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x8007A4F0u, "MAIN.EXE");
    left = v0.color;
    right = v1.color;
    result.color = (left + right - ((left ^ right) & first_mask)) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = (left + right - ((left ^ right) & second_mask)) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = (uint16)((left + right - ((left ^ right) & first_mask)) >> 1);
    return result;
}

POLY_F4 *poly_subdiv_quad_flat(POLY_F4 *prim, POLY_F4 *dst)
{
    uint32 vertices[4];
    uint32 color;
    uint32 midpoint[4];
    uint32 center;
    uint32 tag;

    FUNCTION_MARKER(0x8007A560u, "MAIN.EXE");
    tag = prim->tag;
    color = prim->color0;
    vertices[0] = prim->xy0;
    vertices[1] = prim->xy1;
    vertices[2] = prim->xy2;
    vertices[3] = prim->xy3;
    midpoint[0] = poly_coord_packed_avg(vertices[0], vertices[1], UINT32_C(0x00010001));
    midpoint[1] = poly_coord_packed_avg(vertices[1], vertices[3], UINT32_C(0x00010001));
    midpoint[2] = poly_coord_packed_avg(vertices[3], vertices[2], UINT32_C(0x00010001));
    midpoint[3] = poly_coord_packed_avg(vertices[2], vertices[0], UINT32_C(0x00010001));
    center = poly_coord_packed_avg(midpoint[1], midpoint[3], UINT32_C(0x00010001));
    poly_copy_prim_tex_verts(color, vertices[0], dst, tag, midpoint[0], midpoint[3], center);
    AddPrim(&tag, dst);
    poly_copy_prim_tex_verts(color, midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1]);
    AddPrim(&tag, dst + 1);
    poly_copy_prim_tex_verts(color, center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3]);
    AddPrim(&tag, dst + 2);
    poly_copy_prim_tex_verts(color, midpoint[3], prim, tag, center, vertices[2], midpoint[2]);
    return dst + 3;
}

void poly_copy_prim_tex_verts(uint32 color, uint32 v0, POLY_F4 *dst, uint32 tag, uint32 v1, uint32 v2, uint32 v3)
{
    FUNCTION_MARKER(0x8007A638u, "MAIN.EXE");
    dst->xy0 = v0;
    dst->xy1 = v1;
    dst->xy2 = v2;
    dst->xy3 = v3;
    dst->color0 = color;
    dst->tag = tag;
}

uint32 poly_coord_packed_avg(uint32 v0, uint32 v1, uint32 mask)
{
    FUNCTION_MARKER(0x8007A674u, "MAIN.EXE");
    return poly_tri_flat_verts_avg(v0, v1, mask);
}

POLY_FT4 *poly_subdiv_quad_tex(POLY_FT4 *prim, POLY_FT4 *dst)
{
    POLY_TEX_VERTEX vertices[4];
    uint32 color;
    POLY_TEX_VERTEX midpoint[4];
    POLY_TEX_VERTEX center;
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;

    FUNCTION_MARKER(0x8007A69Cu, "MAIN.EXE");
    tag = prim->tag;
    color = prim->color0;
    vertices[0].xy = prim->xy0;
    vertices[0].uv = prim->uv0;
    vertices[1].xy = prim->xy1;
    vertices[1].uv = prim->uv1;
    vertices[2].xy = prim->xy2;
    vertices[2].uv = prim->uv2;
    vertices[3].xy = prim->xy3;
    vertices[3].uv = prim->uv3;
    midpoint[0] = poly_quad_tex_verts_avg(vertices[0], vertices[1], UINT32_C(0x00010001), UINT32_C(0x01010101));
    midpoint[1] = poly_quad_tex_verts_avg(vertices[1], vertices[3], UINT32_C(0x00010001), UINT32_C(0x01010101));
    midpoint[2] = poly_quad_tex_verts_avg(vertices[3], vertices[2], UINT32_C(0x00010001), UINT32_C(0x01010101));
    midpoint[3] = poly_quad_tex_verts_avg(vertices[2], vertices[0], UINT32_C(0x00010001), UINT32_C(0x01010101));
    center = poly_quad_tex_verts_avg(midpoint[1], midpoint[3], UINT32_C(0x00010001), UINT32_C(0x01010101));
    clut_word = (uint32)prim->clut << 16;
    tpage_word = (uint32)prim->tpage << 16;
    poly_copy_quad_tex_verts(color, vertices[0], dst, tag, midpoint[0], midpoint[3], center, clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_quad_tex_verts(color, midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_quad_tex_verts(color, center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_quad_tex_verts(color, midpoint[3], prim, tag, center, vertices[2], midpoint[2], clut_word, tpage_word);
    return dst + 3;
}

void poly_copy_quad_tex_verts(uint32 color, POLY_TEX_VERTEX v0, POLY_FT4 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, POLY_TEX_VERTEX v3, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A784u, "MAIN.EXE");
    dst->xy0 = v0.xy;
    dst->xy1 = v1.xy;
    dst->xy2 = v2.xy;
    dst->xy3 = v3.xy;
    dst->color0 = color;
    dst->uv0 = v0.uv;
    dst->uv1 = v1.uv;
    dst->uv2 = v2.uv;
    dst->uv3 = v3.uv;
    dst->clut = (uint16)(clut_word >> 16);
    dst->tpage = (uint16)(tpage_word >> 16);
    dst->tag = tag;
}

POLY_TEX_VERTEX poly_quad_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 word_mask, uint32 half_mask)
{
    FUNCTION_MARKER(0x8007A7E8u, "MAIN.EXE");
    return poly_tri_tex_verts_avg(v0, v1, word_mask, half_mask);
}

// Native strip producers retain packet layout only at the GPU boundary

void poly_render_quad_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007A8A8u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_FT4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad1;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_FT4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x09000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad1 = prim->pad1;
                packet->uv3 = prim->uv3;
                packet->pad2 = prim->pad2;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    POLY_FT4 *next_packet = poly_subdiv_quad_tex(packet, packet + 1);
                    next_packet = poly_subdiv_quad_tex(next_packet - 4, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 6, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 8, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 10, next_packet);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007ACC4u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_FT3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad1;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x07000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad1 = prim->pad1;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_tex(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
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
    render_packet_publish(packet_offset);
}

void poly_render_quad_tex_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B014u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_GT4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad2;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x0C000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->color3 = prim->color3;
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad2 = prim->pad2;
                packet->uv3 = prim->uv3;
                packet->pad3 = prim->pad3;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_tex_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_tex_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B418u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_GT3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad2;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x09000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad2 = prim->pad2;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_tex_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
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
    render_packet_publish(packet_offset);
}

void poly_render_quad_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B778u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_F4 *prim = prims + iteration - 1;
            sint32 base_depth = previous_average - 1;
            sint32 relative_depth = base_depth - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)base_depth < 0xC60u)
            {
                bucket = poly_order_bucket(ot, relative_depth);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, base_depth, 0x280u);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_F4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x05000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    POLY_F4 *next_packet = poly_subdiv_quad_flat(packet, packet + 1);
                    next_packet = poly_subdiv_quad_flat(next_packet - 4, next_packet);
                    next_packet = poly_subdiv_quad_flat(next_packet - 6, next_packet);
                    next_packet = poly_subdiv_quad_flat(next_packet - 8, next_packet);
                    next_packet = poly_subdiv_quad_flat(next_packet - 10, next_packet);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007BB90u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_F3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x04000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_flat(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
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
    render_packet_publish(packet_offset);
}

void poly_render_quad_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007BEC8u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_G4 *prim = prims + iteration - 1;
            sint32 base_depth = previous_average - 1;
            sint32 relative_depth = base_depth - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)base_depth < 0xC60u)
            {
                bucket = poly_order_bucket(ot, relative_depth);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, base_depth, 0x3C0u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x08000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->color3 = prim->color3;
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = current_cross > 0;
        if (previous_valid)
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007C2C8u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 current_cross;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_G3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x06000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
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
    render_packet_publish(packet_offset);
}

void poly_render_quad_clipped_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007C610u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        current_cross = NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_FT4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad1;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_FT4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x09000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad1 = prim->pad1;
                packet->uv3 = prim->uv3;
                packet->pad2 = prim->pad2;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    POLY_FT4 *next_packet = poly_subdiv_quad_tex(packet, packet + 1);
                    next_packet = poly_subdiv_quad_tex(next_packet - 4, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 6, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 8, next_packet);
                    next_packet = poly_subdiv_quad_tex(next_packet - 10, next_packet);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 0;
        if (current_cross > 0)
        {
            fourth = faces[iteration].v[3];
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
    render_packet_publish(packet_offset);
}

void poly_render_quad_alt_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007CA40u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_FT4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad1;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x09000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad1 = prim->pad1;
                packet->uv3 = prim->uv3;
                packet->pad2 = prim->pad2;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_tex(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_alt_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007CE20u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_FT3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad1;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x07000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad1 = prim->pad1;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_tex(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
    render_packet_publish(packet_offset);
}

void poly_render_quad_mixed_strip(const SCENE_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D164u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_GT4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad2;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x0C000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->color3 = prim->color3;
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad2 = prim->pad2;
                packet->uv3 = prim->uv3;
                packet->pad3 = prim->pad3;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_tex_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_mixed_strip(const SCENE_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D55Cu, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_GT3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)prim->pad2;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x09000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->uv0 = prim->uv0;
                packet->clut = prim->clut;
                packet->uv1 = prim->uv1;
                packet->tpage = prim->tpage;
                packet->uv2 = prim->uv2;
                packet->pad2 = prim->pad2;
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_tex_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
    render_packet_publish(packet_offset);
}

void poly_render_quad_alt_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D8B0u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_F4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x05000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_flat(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_alt_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007DC70u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_F3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));
                uint32 color = prim->color0;

                packet->tag = 0x04000000u;
                packet->color0 = color;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_flat(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
    render_packet_publish(packet_offset);
}

void poly_render_quad_alt_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[4];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007DF9Cu, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
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
            first_three[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(first_three, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_G4 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            outcode &= poly_screen_outcode(previous_screen[3], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 4, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G4 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x08000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                packet->color3 = prim->color3;
                packet->x3 = (sint16)previous_screen[3];
                packet->y3 = (sint16)((uint32)previous_screen[3] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_quad_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        {
            fourth = faces[iteration].v[3];
            current_depth[3] = gte_project_full_depth(&fourth, &current_screen[3], &fourth_flag);
            previous_screen[0] = current_screen[0];
            previous_screen[1] = current_screen[1];
            previous_screen[2] = current_screen[2];
            previous_screen[3] = current_screen[3];
            previous_average = AverageZ4(current_depth[0], current_depth[1], current_depth[2], current_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

void poly_render_tri_alt_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot)
{
    sint32 previous_screen[3];
    sint32 previous_average = 0;
    sint32 previous_valid = 0;
    sint32 width;
    sint32 height;
    sint32 iteration;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007E374u, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = (sint32)display_state.scene_height;
    width = (sint32)display_state.scene_width;
    for (iteration = 0; iteration <= count; ++iteration)
    {
        SVECTOR proj_vertices[3];
        sint32 current_screen[3];
        sint32 current_depth[3];
        sint32 current_flag;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
        {
            proj_vertices[vertex] = faces[iteration].v[vertex];
        }
        gte_project3_full_depth(proj_vertices, current_screen, current_depth, &current_flag);
        (void)NormalClip(current_screen[0], current_screen[1], current_screen[2]);
        if (previous_valid)
        {
            const POLY_G3 *prim = prims + iteration - 1;
            sint32 relative_depth = previous_average - 1 - (sint32)(uint16)faces[iteration - 1].v[0].pad;
            uint32 outcode = poly_screen_outcode(previous_screen[0], width, height);
            uint32 *bucket;

            outcode &= poly_screen_outcode(previous_screen[1], width, height);
            outcode &= poly_screen_outcode(previous_screen[2], width, height);
            if (outcode == 0u && (uint32)relative_depth < 0xC60u)
            {
                bucket = ot + 1 + ((uint32)relative_depth >> 5);
                sint32 subdiv = poly_needs_subdivision(previous_screen, 3, relative_depth, 0x280u);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G3 *packet = render_packet_at(packet_offset, packets * sizeof(*packet));

                packet->tag = 0x06000000u;
                packet->color0 = prim->color0;
                packet->x0 = (sint16)previous_screen[0];
                packet->y0 = (sint16)((uint32)previous_screen[0] >> 16);
                packet->color1 = prim->color1;
                packet->x1 = (sint16)previous_screen[1];
                packet->y1 = (sint16)((uint32)previous_screen[1] >> 16);
                packet->color2 = prim->color2;
                packet->x2 = (sint16)previous_screen[2];
                packet->y2 = (sint16)((uint32)previous_screen[2] >> 16);
                AddPrim(bucket, packet);
                if (subdiv)
                {
                    poly_subdiv_tri_shaded(packet, packet + 1);
                    height = (sint32)display_state.scene_height;
                    width = (sint32)display_state.scene_width;
                }
                packet_offset += packets * sizeof(*packet);
            }
        }
        previous_valid = 1;
        previous_screen[0] = current_screen[0];
        previous_screen[1] = current_screen[1];
        previous_screen[2] = current_screen[2];
        previous_average = AverageZ3(current_depth[0], current_depth[1], current_depth[2]);
    }
    render_packet_publish(packet_offset);
}


// Encode registered packets through the shared PsyQ boundary
static uint32 poly_packet_link(void *packet)
{
    uint32 *tag = packet;
    uint32 saved = *tag;
    uint32 link = 0u;

    AddPrim(&link, packet);
    *tag = saved;
    return link;
}

static const ROUTE_SEGMENT *mesh_pass_packets_render_mode(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3], sint32 generated_course)
{
    RR_ANIMATED_VERTEX proj[2][32];
    uint32 *ordering;
    uint32 *pending_bucket;
    uint32 packet_offset;
    const ROUTE_SEGMENT *desc;
    uint32 remaining_passes = (uint32)pass_count;
    sint32 screen_limit_x;
    sint32 screen_limit_y;
    sint32 previous_buffer = -1;
    sint32 current_buffer = 0;

    if (remaining_passes == 0u)
        return mesh;
    ordering = render_order.route_ot;
    pending_bucket = ordering;
    packet_offset = render_packet_offset();
    render_billboards.count = 0u;
    screen_limit_y = (sint16)(uint16)display_state.scene_height;
    screen_limit_x = (sint16)(uint16)display_state.scene_width;
    desc = mesh->prev.seg;
    while (remaining_passes != 0u)
    {
        uint32 phase = (uint32)game_timing.ticks;
        sint32 origin_x = desc->origin[0] + (sint32)(uint32)transform[0];
        sint32 origin_y = desc->origin[1] + (sint32)(uint32)transform[1];
        sint32 origin_z = desc->origin[2] + (sint32)(uint32)transform[2];
        sint32 vertex_count = desc->vertex_count;
        const uint32 *palette_entry = render_palette(desc->palette);
        uint32 palette = palette_entry[0];
        sint32 vertex;

        for (vertex = 0; vertex < vertex_count; ++vertex)
        {
            const ROUTE_VERTEX *src = &desc->vertices[vertex];
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

            point.vx = (sint16)(origin_x + 16 * src->position[0]);
            point.vy = (sint16)(origin_y + 16 * src->position[1]);
            point.vz = (sint16)(origin_z + 16 * src->position[2]);
            point.pad = 0;
            low_deformation = src->curve & 15u;
            high_deformation = src->curve >> 4;
            first_deformation = render_curve[low_deformation][((sint32)(sint8)src->phase[0] * 2 + (sint32)phase) & 0x1FF];
            second_deformation = render_curve[high_deformation][((sint32)(sint8)src->phase[1] * 2 + (sint32)phase) & 0x1FF];
            vertical_deformation = -(first_deformation + second_deformation);
            shading = 2 * ((sint32)low_deformation + (sint32)high_deformation);
            if ((uint16)(phase - src->impact_tick) < 256u)
            {
                sint32 adjustment = 4 * (sint32)render_curve[10][(uint16)(phase - src->impact_tick)] - 80;

                shading -= adjustment;
                vertical_deformation -= adjustment >> 3;
            }
            deformation = ((uint16)(uint8)src->deformation[0] | ((uint16)(uint8)src->deformation[1] << 8));
            proj[current_buffer][vertex].alternate_screen = 0;
            if (deformation != 0u)
            {
                SVECTOR base = point;
                sint32 unused_flags;

                gte_project_full_depth(&base, &proj[current_buffer][vertex].alternate_screen, &unused_flags);
                point.vx = (sint16)((sint32)base.vx + ((vertical_deformation * ((sint32)(sint8)(uint8)deformation * 128)) >> 12));
                point.vy = (sint16)((sint32)base.vy - vertical_deformation);
                point.vz = (sint16)((sint32)base.vz + ((vertical_deformation * ((sint32)(sint8)(uint8)(deformation >> 8) * 128)) >> 12));
                proj[current_buffer][vertex].color = 0x3C3F3F3Fu;
                outcode = 0u;
            }
            else
            {
                point.vy = (sint16)((sint32)point.vy - vertical_deformation);
                shading = -shading - vertical_deformation + src->shade;
                if (shading < 3)
                    shading = 3;
                proj[current_buffer][vertex].color = poly_anim_vertex_color(palette, shading << 5);
                outcode = 0x10u;
            }
            depth = gte_project_full_depth(&point, &screen, &flags);
            if (deformation == 0u)
                proj[current_buffer][vertex].alternate_screen = screen;
            type = src->flags >> 5;
            if (type != 0u && render_billboards.count < 64u)
            {
                render_add_billboard(point.vx, point.vy, point.vz, type);
                proj[current_buffer][vertex].color = poly_billboard_colors[type];
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
                texture = (uint8)(render_curve[8][(2u * src->texture_phase + phase) & 0x1FFu] + 32u);
            else
                texture = render_curve[15][(2u * src->texture_phase + 2u * phase) & 0x1FFu];
            proj[current_buffer][vertex].screen = screen;
            proj[current_buffer][vertex].depth = depth;
            proj[current_buffer][vertex].flags = (uint16)((uint16)texture << 8) | outcode;
        }
        if (previous_buffer >= 0)
        {
            uint8 selector = ((desc->join[0] << 4) | desc->join[1]);
            const uint32 *colors = palette_entry + 2;
            sint32 first_index = selector & 15u;
            sint32 second_index = selector >> 4;
            uint32 pending_head = 0u;
            uint32 *pending_patch = NULL;
            uint32 *link_bucket = NULL;
            sint32 link_mode = 1;
            sint32 prim;

            for (prim = 0; prim < 14; ++prim)
            {
                uint8 control = desc->commands[prim];
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
                uint32 *bucket;

                if (control == 255u)
                    break;
                if ((control & 3u) != 0u)
                {
                    uint8 kind = control & 3u;
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
                    uint32 *bucket;
                    uint32 link_value;

                    if ((control & 0xF0u) == 0xF0u)
                    {
                        ++first_index;
                        if ((control & 6u) != 2u)
                            ++second_index;
                        ++colors;
                        link_mode = 1;
                        continue;
                    }
                    poly_material_words(2u * ((uint32)control >> 4), &first_texture, &second_texture, &third_texture);
                    shared = (uint16)(first_texture ^ second_texture);
                    color = (*colors & 0x00FFFFFFu) | (kind == 3u ? 0x2C000000u : 0x24000000u);
                    ++colors;
                    if (kind == 1u)
                    {
                        RR_ANIMATED_VERTEX *a = &proj[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *b = &proj[current_buffer][second_index + 1];
                        RR_ANIMATED_VERTEX *c = &proj[previous_buffer][first_index];

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
                        bucket = ordering + (uint32)bucket_index;
                        POLY_FT3 *packet = render_packet_at(packet_offset, ((uint32)bucket_index < 20u ? 4u : 1u) * sizeof(*packet));
                        {
                            uint32 word = (uint32)(color);
                            packet->r0 = (uint8)(word);
                            packet->g0 = (uint8)(word >> 8);
                            packet->b0 = (uint8)(word >> 16);
                            packet->code = (uint8)(word >> 24);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[0]);
                            packet->x0 = (sint16)(word);
                            packet->y0 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)(first_texture ^ shared);
                            packet->u0 = (uint8)(word);
                            packet->v0 = (uint8)(word >> 8);
                            packet->clut = (uint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[1]);
                            packet->x1 = (sint16)(word);
                            packet->y1 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)(((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                            packet->u1 = (uint8)(word);
                            packet->v1 = (uint8)(word >> 8);
                            packet->tpage = (uint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[2]);
                            packet->x2 = (sint16)(word);
                            packet->y2 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint16)third_texture);
                            packet->u2 = (uint8)(word);
                            packet->v2 = (uint8)(word >> 8);
                        }
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_value = link_mode > 0 ? pending_head : *link_bucket;
                        if (link_value == 0u)
                            pending_patch = &packet[(uint32)bucket_index < 20u ? 1u : 0u].tag;
                        packet->tag = link_value | 0x07000000u;
                        if (link_mode > 0)
                            AddPrim(&pending_head, packet);
                        else
                            AddPrim(link_bucket, packet);
                        packet_offset += sizeof(*packet);
                        if ((uint32)bucket_index < 20u)
                        {
                            poly_subdiv_tri_tex(packet, packet + 1);
                            packet_offset += 3u * sizeof(*packet);
                        }
                        link_mode = 1;
                        continue;
                    }
                    if (kind == 2u)
                    {
                        RR_ANIMATED_VERTEX *a = &proj[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *c = &proj[previous_buffer][first_index];
                        RR_ANIMATED_VERTEX *d = &proj[previous_buffer][first_index + 1];

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
                        bucket = ordering + (uint32)bucket_index;
                        POLY_FT3 *packet = render_packet_at(packet_offset, ((uint32)bucket_index < 20u ? 4u : 1u) * sizeof(*packet));
                        {
                            uint32 word = (uint32)(color);
                            packet->r0 = (uint8)(word);
                            packet->g0 = (uint8)(word >> 8);
                            packet->b0 = (uint8)(word >> 16);
                            packet->code = (uint8)(word >> 24);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[0]);
                            packet->x0 = (sint16)(word);
                            packet->y0 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)(first_texture ^ shared);
                            packet->u0 = (uint8)(word);
                            packet->v0 = (uint8)(word >> 8);
                            packet->clut = (uint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[2]);
                            packet->x1 = (sint16)(word);
                            packet->y1 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((first_texture << 16) | (third_texture >> 16));
                            packet->u1 = (uint8)(word);
                            packet->v1 = (uint8)(word >> 8);
                            packet->tpage = (uint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint32)screens[1]);
                            packet->x2 = (sint16)(word);
                            packet->y2 = (sint16)(word >> 16);
                        }
                        {
                            uint32 word = (uint32)((uint16)third_texture);
                            packet->u2 = (uint8)(word);
                            packet->v2 = (uint8)(word >> 8);
                        }
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_value = link_mode > 0 ? pending_head : *link_bucket;
                        if (link_value == 0u)
                            pending_patch = &packet[(uint32)bucket_index < 20u ? 1u : 0u].tag;
                        packet->tag = link_value | 0x07000000u;
                        if (link_mode > 0)
                            AddPrim(&pending_head, packet);
                        else
                            AddPrim(link_bucket, packet);
                        packet_offset += sizeof(*packet);
                        if ((uint32)bucket_index < 20u)
                        {
                            poly_subdiv_tri_tex(packet, packet + 1);
                            packet_offset += 3u * sizeof(*packet);
                        }
                        link_mode = 1;
                        continue;
                    }
                    {
                        RR_ANIMATED_VERTEX *a = &proj[current_buffer][second_index];
                        RR_ANIMATED_VERTEX *b = &proj[current_buffer][second_index + 1];
                        RR_ANIMATED_VERTEX *c = &proj[previous_buffer][first_index];
                        RR_ANIMATED_VERTEX *d = &proj[previous_buffer][first_index + 1];

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
                        bucket = ordering + (uint32)bucket_index;
                        if (first_cross >= 0 && second_cross <= 0)
                        {
                            POLY_FT4 *packet = render_packet_at(packet_offset, ((uint32)bucket_index < 20u ? 16u : 1u) * sizeof(*packet));
                            {
                                uint32 word = (uint32)(color);
                                packet->r0 = (uint8)(word);
                                packet->g0 = (uint8)(word >> 8);
                                packet->b0 = (uint8)(word >> 16);
                                packet->code = (uint8)(word >> 24);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[0]);
                                packet->x0 = (sint16)(word);
                                packet->y0 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)(first_texture ^ shared);
                                packet->u0 = (uint8)(word);
                                packet->v0 = (uint8)(word >> 8);
                                packet->clut = (uint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[1]);
                                packet->x1 = (sint16)(word);
                                packet->y1 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)(((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                                packet->u1 = (uint8)(word);
                                packet->v1 = (uint8)(word >> 8);
                                packet->tpage = (uint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[2]);
                                packet->x2 = (sint16)(word);
                                packet->y2 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint16)third_texture);
                                packet->u2 = (uint8)(word);
                                packet->v2 = (uint8)(word >> 8);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[3]);
                                packet->x3 = (sint16)(word);
                                packet->y3 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint16)(third_texture >> 16));
                                packet->u3 = (uint8)(word);
                                packet->v3 = (uint8)(word >> 8);
                            }
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_value = link_mode > 0 ? pending_head : *link_bucket;
                            if (link_value == 0u)
                                pending_patch = &packet[(uint32)bucket_index < 20u ? 7u : 0u].tag;
                            packet->tag = link_value | 0x09000000u;
                            if (link_mode > 0)
                                AddPrim(&pending_head, packet);
                            else
                                AddPrim(link_bucket, packet);
                            packet_offset += sizeof(*packet);
                            if ((uint32)bucket_index < 20u)
                            {
                                POLY_FT4 *src = packet;
                                POLY_FT4 *next_packet = packet + 1;

                                next_packet = poly_subdiv_quad_tex(src, next_packet);
                                next_packet = poly_subdiv_quad_tex(src, next_packet);
                                next_packet = poly_subdiv_quad_tex(src + 1, next_packet);
                                next_packet = poly_subdiv_quad_tex(src + 2, next_packet);
                                next_packet = poly_subdiv_quad_tex(src + 3, next_packet);
                                packet_offset += 15u * sizeof(*packet);
                            }
                        }
                        else if (first_cross >= 0 || second_cross <= 0)
                        {
                            sint32 first = first_cross >= 0 ? 0 : 1;
                            sint32 middle = first + 1;
                            sint32 last = first + 2;

                            POLY_FT3 *packet = render_packet_at(packet_offset, ((uint32)bucket_index < 20u ? 4u : 1u) * sizeof(*packet));
                            {
                                uint32 word = (uint32)((color & 0x00FFFFFFu) | 0x24000000u);
                                packet->r0 = (uint8)(word);
                                packet->g0 = (uint8)(word >> 8);
                                packet->b0 = (uint8)(word >> 16);
                                packet->code = (uint8)(word >> 24);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[first]);
                                packet->x0 = (sint16)(word);
                                packet->y0 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)(first_cross >= 0 ? first_texture ^ shared : (first_texture & 0xFFFF0000u) | (second_texture >> 16));
                                packet->u0 = (uint8)(word);
                                packet->v0 = (uint8)(word >> 8);
                                packet->clut = (uint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[middle]);
                                packet->x1 = (sint16)(word);
                                packet->y1 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)(first_cross >= 0 ? ((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16) : (first_texture << 16) | (third_texture & 0xFFFFu));
                                packet->u1 = (uint8)(word);
                                packet->v1 = (uint8)(word >> 8);
                                packet->tpage = (uint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint32)screens[last]);
                                packet->x2 = (sint16)(word);
                                packet->y2 = (sint16)(word >> 16);
                            }
                            {
                                uint32 word = (uint32)((uint16)(first_cross >= 0 ? third_texture : third_texture >> 16));
                                packet->u2 = (uint8)(word);
                                packet->v2 = (uint8)(word >> 8);
                            }
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_value = link_mode > 0 ? pending_head : *link_bucket;
                            if (link_value == 0u)
                                pending_patch = &packet[(uint32)bucket_index < 20u ? 1u : 0u].tag;
                            packet->tag = link_value | 0x07000000u;
                            if (link_mode > 0)
                                AddPrim(&pending_head, packet);
                            else
                                AddPrim(link_bucket, packet);
                            packet_offset += sizeof(*packet);
                            if ((uint32)bucket_index < 20u)
                            {
                                poly_subdiv_tri_tex(packet, packet + 1);
                                packet_offset += 3u * sizeof(*packet);
                            }
                        }
                    }
                    link_mode = 1;
                    continue;
                }
                next_first = first_index + ((control & 6u) != 1u);
                next_second = second_index + ((control & 6u) != 2u);
                a = &proj[current_buffer][second_index];
                b = &proj[current_buffer][next_second];
                c = &proj[previous_buffer][first_index];
                d = &proj[previous_buffer][next_first];
                first_index = next_first;
                second_index = next_second;
                POLY_GT4 *packet = render_packet_at(packet_offset, sizeof(*packet));
                {
                    uint32 word = (uint32)(a->color);
                    packet->r0 = (uint8)(word);
                    packet->g0 = (uint8)(word >> 8);
                    packet->b0 = (uint8)(word >> 16);
                    packet->code = (uint8)(word >> 24);
                }
                {
                    uint32 word = (uint32)((uint32)a->screen);
                    packet->x0 = (sint16)(word);
                    packet->y0 = (sint16)(word >> 16);
                }
                {
                    uint32 word = (uint32)(b->color);
                    packet->r1 = (uint8)(word);
                    packet->g1 = (uint8)(word >> 8);
                    packet->b1 = (uint8)(word >> 16);
                    packet->p1 = (uint8)(word >> 24);
                }
                {
                    uint32 word = (uint32)((uint32)b->screen);
                    packet->x1 = (sint16)(word);
                    packet->y1 = (sint16)(word >> 16);
                }
                {
                    uint32 word = (uint32)(c->color);
                    packet->r2 = (uint8)(word);
                    packet->g2 = (uint8)(word >> 8);
                    packet->b2 = (uint8)(word >> 16);
                    packet->p2 = (uint8)(word >> 24);
                }
                {
                    uint32 word = (uint32)((uint32)c->screen);
                    packet->x2 = (sint16)(word);
                    packet->y2 = (sint16)(word >> 16);
                }
                {
                    uint32 word = (uint32)(d->color);
                    packet->r3 = (uint8)(word);
                    packet->g3 = (uint8)(word >> 8);
                    packet->b3 = (uint8)(word >> 16);
                    packet->p3 = (uint8)(word >> 24);
                }
                {
                    uint32 word = (uint32)((uint32)d->screen);
                    packet->x3 = (sint16)(word);
                    packet->y3 = (sint16)(word >> 16);
                }
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
                        pending_bucket = NULL;
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
                {
                    uint32 word = (uint32)((uint32)uv0 | ((uint32)render_material(1u)->clut << 16));
                    packet->u0 = (uint8)(word);
                    packet->v0 = (uint8)(word >> 8);
                    packet->clut = (uint16)(word >> 16);
                }
                {
                    uint32 word = (uint32)((uint32)uv1 | ((uint32)render_material(1u)->tpage << 16));
                    packet->u1 = (uint8)(word);
                    packet->v1 = (uint8)(word >> 8);
                    packet->tpage = (uint16)(word >> 16);
                }
                {
                    uint32 word = (uint32)(uv2);
                    packet->u2 = (uint8)(word);
                    packet->v2 = (uint8)(word >> 8);
                }
                {
                    uint32 word = (uint32)(uv3);
                    packet->u3 = (uint8)(word);
                    packet->v3 = (uint8)(word >> 8);
                }
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
                            pending_bucket = NULL;
                            link_mode = -1;
                        }
                        continue;
                    }
                }
                bucket = ordering + (uint32)bucket_index;
                pending_bucket = bucket;
                packet->tag = *bucket | 0x0C000000u;
                if (pending_head != 0u)
                {
                    uint32 head = pending_head;

                    *pending_patch |= poly_packet_link(packet);
                    pending_head = 0u;
                    *bucket = head;
                }
                else
                {
                    AddPrim(bucket, packet);
                }
                link_bucket = bucket;
                link_mode = 0;
                packet_offset += sizeof(*packet);
                if ((uint32)bucket_index < 20u)
                {
                    render_packet_at(packet_offset, 3u * sizeof(*packet));
                    poly_subdiv_quad_tex_shaded(packet, packet + 1);
                    packet_offset += 3u * sizeof(*packet);
                }
            }
            if (pending_head != 0u && pending_bucket != NULL)
            {
                uint32 old_bucket = *pending_bucket;

                *pending_bucket = pending_head;
                *pending_patch |= old_bucket;
            }
        }
        if (previous_buffer >= 0)
            --remaining_passes;
        previous_buffer = current_buffer;
        current_buffer ^= 1;
        desc = desc->next.seg;
    }
    render_packet_publish(packet_offset);
    return desc;
}

const ROUTE_SEGMENT *mesh_render_pass_packets(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007E850u, "MAIN.EXE");
    return mesh_pass_packets_render_mode(mesh, pass_count, transform, 0);
}

const ROUTE_SEGMENT *mesh_render_pass(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3])
{
    const ROUTE_SEGMENT *current = mesh;
    uint32 remaining_passes = (uint32)pass_count;
    uint32 *ordering;
    uint32 packet_offset;
    uint32 phase;
    const ROUTE_SEGMENT *partner;
    sint32 partner_x;
    sint32 partner_y;
    sint32 partner_z;

    FUNCTION_MARKER(0x8007F490u, "MAIN.EXE");
    if (remaining_passes == 0u)
        return current;
    phase = (uint32)game_timing.ticks;
    partner = current->prev.seg;
    ordering = render_order.route_ot;
    packet_offset = render_packet_offset();
    {
        sint32 transform_x = (sint32)(uint32)transform[0];
        sint32 transform_y = (sint32)(uint32)transform[1];
        sint32 transform_z = (sint32)(uint32)transform[2];
        sint32 offset_x = partner->origin[0];
        sint32 offset_y = partner->origin[1];
        sint32 offset_z = partner->origin[2];

        partner_x = (sint32)((uint32)transform_x + (uint32)offset_x);
        partner_y = (sint32)((uint32)transform_y + (uint32)offset_y);
        partner_z = (sint32)((uint32)transform_z + (uint32)offset_z);
    }
    while (remaining_passes != 0u)
    {
        const uint8 *control = current->commands;
        uint32 palette_index = current->palette;
        const uint32 *palette = render_palette(palette_index);
        uint32 palette_color = palette[1] ^ 0x10000000u;
        const uint32 *colors = palette + 2;
        uint8 selector = ((current->join[0] << 4) | current->join[1]);
        const ROUTE_VERTEX *current_record = &current->vertices[selector >> 4];
        const ROUTE_VERTEX *partner_record = &partner->vertices[selector & 15u];
        const ROUTE_VERTEX *prior_current = current_record;
        const ROUTE_VERTEX *prior_partner = partner_record;
        sint32 transform_x = (sint32)(uint32)transform[0];
        sint32 transform_y = (sint32)(uint32)transform[1];
        sint32 transform_z = (sint32)(uint32)transform[2];
        sint32 offset_x = current->origin[0];
        sint32 offset_y = current->origin[1];
        sint32 offset_z = current->origin[2];
        sint32 current_x = (sint32)((uint32)transform_x + (uint32)offset_x);
        sint32 current_y = (sint32)((uint32)transform_y + (uint32)offset_y);
        sint32 current_z = (sint32)((uint32)transform_z + (uint32)offset_z);
        sint32 joined = 1;

        for (;;)
        {
            uint8 command = *control++;
            uint8 kind = command & 3u;
            sint32 screens[4];
            sint32 depths[4];
            sint32 depth;
            sint32 first_cross;
            sint32 second_cross;
            uint32 *bucket;
            uint32 first_texture;
            uint32 second_texture;
            uint32 third_texture;
            uint32 shared;
            uint32 primitive_color;

            if (kind == 0u)
            {
                ++current_record;
                ++partner_record;
                if (joined == 0)
                    continue;
                prior_current = current_record - 1;
                prior_partner = partner_record - 1;
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
                    uint32 texture_page = (render_material(1u)->tpage | ((uint32)render_material(1u)->clut << 16));

                    POLY_FT4 *packet = render_packet_at(packet_offset, sizeof(*packet));
                    uint32 word;
                    word = (uint32)(palette_color);
                    packet->color0 = word;
                    word = (uint32)((uint32)screens[0]);
                    packet->x0 = (sint16)word;
                    packet->y0 = (sint16)(word >> 16);
                    word = (uint32)(((texture_page >> 16) << 16) | ((uint32)mesh_texture_coordinate(current_record, phase) << 8));
                    packet->u0 = (uint8)word;
                    packet->v0 = (uint8)(word >> 8);
                    packet->clut = (uint16)(word >> 16);
                    word = (uint32)((uint32)screens[1]);
                    packet->x1 = (sint16)word;
                    packet->y1 = (sint16)(word >> 16);
                    word = (uint32)(((texture_page & 0xFFFFu) << 16) | ((uint32)mesh_texture_coordinate(prior_current, phase) << 8) | 0x3Fu);
                    packet->u1 = (uint8)word;
                    packet->v1 = (uint8)(word >> 8);
                    packet->tpage = (uint16)(word >> 16);
                    word = (uint32)((uint32)screens[2]);
                    packet->x2 = (sint16)word;
                    packet->y2 = (sint16)(word >> 16);
                    word = (uint32)((uint16)((uint32)mesh_texture_coordinate(partner_record, phase) << 8));
                    packet->u2 = (uint8)word;
                    packet->v2 = (uint8)(word >> 8);
                    word = (uint32)((uint32)screens[3]);
                    packet->x3 = (sint16)word;
                    packet->y3 = (sint16)(word >> 16);
                    word = (uint32)((uint16)(((uint32)mesh_texture_coordinate(prior_partner, phase) << 8) | 0x3Fu));
                    packet->u3 = (uint8)word;
                    packet->v3 = (uint8)(word >> 8);
                    if ((uint32)depth < 0x7D0u)
                    {
                        bucket = ordering + ((uint32)depth >> 3);
                        packet->tag = 0x09000000u;
                        AddPrim(bucket, packet);
                        packet_offset += sizeof(*packet);
                    }
                }
            }

            if (command == 0xFFu)
                break;

            if (kind == 3u)
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                ++current_record;
                depths[1] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[1]);
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                ++partner_record;
                depths[3] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[3]);
                first_cross = NormalClip(screens[0], screens[1], screens[2]);
                second_cross = NormalClip(screens[1], screens[2], screens[3]);
                depth = AverageZ4(depths[0], depths[1], depths[2], depths[3]) - 1;
                primitive_color = *colors;
                poly_material_words(2u * ((uint32)command >> 4), &first_texture, &second_texture, &third_texture);
                shared = (uint16)(first_texture ^ second_texture);
                POLY_FT4 *packet = render_packet_at(packet_offset, sizeof(*packet));
                uint32 word;
                word = (uint32)(primitive_color | 0x2C000000u);
                packet->color0 = word;
                word = (uint32)((uint32)screens[0]);
                packet->x0 = (sint16)word;
                packet->y0 = (sint16)(word >> 16);
                word = (uint32)(first_texture ^ shared);
                packet->u0 = (uint8)word;
                packet->v0 = (uint8)(word >> 8);
                packet->clut = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[1]);
                packet->x1 = (sint16)word;
                packet->y1 = (sint16)(word >> 16);
                word = (uint32)(((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                packet->u1 = (uint8)word;
                packet->v1 = (uint8)(word >> 8);
                packet->tpage = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[2]);
                packet->x2 = (sint16)word;
                packet->y2 = (sint16)(word >> 16);
                word = (uint32)((uint16)third_texture);
                packet->u2 = (uint8)word;
                packet->v2 = (uint8)(word >> 8);
                word = (uint32)((uint32)screens[3]);
                packet->x3 = (sint16)word;
                packet->y3 = (sint16)(word >> 16);
                word = (uint32)((uint16)(third_texture >> 16));
                packet->u3 = (uint8)word;
                packet->v3 = (uint8)(word >> 8);
                if ((second_cross <= 0 || first_cross >= 0) && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + ((uint32)depth >> 3);
                    packet->tag = 0x09000000u;
                    AddPrim(bucket, packet);
                    packet_offset += sizeof(*packet);
                }
                ++colors;
            }
            else if (kind == 1u)
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                ++current_record;
                depths[1] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[1]);
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                depth = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                primitive_color = *colors;
                poly_material_words(2u * ((uint32)command >> 4), &first_texture, &second_texture, &third_texture);
                shared = (uint16)(first_texture ^ second_texture);
                POLY_FT3 *packet = render_packet_at(packet_offset, sizeof(*packet));
                uint32 word;
                word = (uint32)(primitive_color | 0x24000000u);
                packet->color0 = word;
                word = (uint32)((uint32)screens[0]);
                packet->x0 = (sint16)word;
                packet->y0 = (sint16)(word >> 16);
                word = (uint32)(first_texture ^ shared);
                packet->u0 = (uint8)word;
                packet->v0 = (uint8)(word >> 8);
                packet->clut = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[1]);
                packet->x1 = (sint16)word;
                packet->y1 = (sint16)(word >> 16);
                word = (uint32)(((second_texture ^ shared) << 16) | ((second_texture ^ shared) >> 16));
                packet->u1 = (uint8)word;
                packet->v1 = (uint8)(word >> 8);
                packet->tpage = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[2]);
                packet->x2 = (sint16)word;
                packet->y2 = (sint16)(word >> 16);
                word = (uint32)((uint16)third_texture);
                packet->u2 = (uint8)word;
                packet->v2 = (uint8)(word >> 8);
                if (NormalClip(screens[0], screens[1], screens[2]) >= 0 && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + ((uint32)depth >> 3);
                    packet->tag = 0x07000000u;
                    AddPrim(bucket, packet);
                    packet_offset += sizeof(*packet);
                }
                ++colors;
            }
            else
            {
                depths[0] = poly_project_mesh_vertex(current_record, current_x, current_y, current_z, &screens[0]);
                depths[1] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[1]);
                ++partner_record;
                depths[2] = poly_project_mesh_vertex(partner_record, partner_x, partner_y, partner_z, &screens[2]);
                depth = AverageZ3(depths[0], depths[1], depths[2]) - 1;
                primitive_color = *colors;
                poly_material_words(2u * ((uint32)command >> 4), &first_texture, &second_texture, &third_texture);
                shared = (uint16)(first_texture ^ second_texture);
                POLY_FT3 *packet = render_packet_at(packet_offset, sizeof(*packet));
                uint32 word;
                word = (uint32)(primitive_color | 0x24000000u);
                packet->color0 = word;
                word = (uint32)((uint32)screens[0]);
                packet->x0 = (sint16)word;
                packet->y0 = (sint16)(word >> 16);
                word = (uint32)(first_texture ^ shared);
                packet->u0 = (uint8)word;
                packet->v0 = (uint8)(word >> 8);
                packet->clut = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[2]);
                packet->x1 = (sint16)word;
                packet->y1 = (sint16)(word >> 16);
                word = (uint32)((first_texture << 16) | (third_texture >> 16));
                packet->u1 = (uint8)word;
                packet->v1 = (uint8)(word >> 8);
                packet->tpage = (uint16)(word >> 16);
                word = (uint32)((uint32)screens[1]);
                packet->x2 = (sint16)word;
                packet->y2 = (sint16)(word >> 16);
                word = (uint32)((uint16)third_texture);
                packet->u2 = (uint8)word;
                packet->v2 = (uint8)(word >> 8);
                if (NormalClip(screens[0], screens[1], screens[2]) <= 0 && (uint32)depth < 0x7D0u)
                {
                    bucket = ordering + ((uint32)depth >> 3);
                    packet->tag = 0x07000000u;
                    AddPrim(bucket, packet);
                    packet_offset += sizeof(*packet);
                }
                ++colors;
            }
        }
        partner = current;
        partner_x = current_x;
        partner_y = current_y;
        partner_z = current_z;
        current = current->next.seg;
        --remaining_passes;
    }
    render_packet_publish(packet_offset);
    return current;
}

const ROUTE_SEGMENT *poly_fn_8007fe50(const ROUTE_SEGMENT *object, sint32 enabled, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007FE50u, "MAIN.EXE");
    return mesh_pass_packets_render_mode(object, enabled, transform, 1);
}
