#include "track.h"
#include "route.h"
#include "game.h"
#include "polygon.h"
#include "disp.h"
#include "utils.h"
#include "mesh.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32 xy;
    uint16 uv;
} POLY_TEX_VERTEX;

typedef struct
{
    uint32 color;
    uint32 xy;
} POLY_SHADED_VERTEX;

typedef struct
{
    uint32 color;
    uint32 xy;
    uint16 uv;
} POLY_TEX_SHADED_VERTEX;

// Pack material fields only at the PSX polygon packet boundary
void poly_material_words(uint32 idx, uint32 *page, uint32 *uv0, uint32 *uv1)
{
    const RENDER_MATERIAL *material = render_material(idx);
    *page = material->tpage | (uint32)material->clut << 16;
    *uv0 = material->uv_words[0];
    *uv1 = material->uv_words[1];
}

static uint32 poly_scr_outcode(sint32 scr, sint32 width, sint32 height)
{
    sint16 x = scr;
    sint16 y = scr >> 16;
    uint32 result = 0;

    if (x < 0)
        result |= 1;
    if (x >= width)
        result |= 2;
    if (y < 0)
        result |= 4;
    if (y >= height)
        result |= 8;
    return result;
}

static sint32 poly_needs_subdiv(const sint32 *scr, sint32 vertex_count, sint32 depth, sint32 depth_limit)
{
    sint32 x_perimeter = 0;
    sint32 y_perimeter = 0;
    sint32 idx;

    if (depth < 0 || depth >= depth_limit)
        return 0;
    for (idx = 0; idx < vertex_count; ++idx)
    {
        sint32 next = (idx + 1) % vertex_count;
        XY pos0 = {.xy = scr[idx]};
        XY pos1 = {.xy = scr[next]};

        x_perimeter += abs(pos0.x - pos1.x);
        y_perimeter += abs(pos0.y - pos1.y);
    }
    return y_perimeter >= 1024 || x_perimeter >= 2048;
}

static uint32 *poly_order_bucket(uint32 *ot, sint32 relative_depth)
{
    return relative_depth < 0 ? ot : ot + 1 + (relative_depth >> 5);
}

static void poly_copy_ft4(uint32 color, POLY_TEX_VERTEX v0, POLY_FT4 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, POLY_TEX_VERTEX v3, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A784, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->p[3].xy.xy = v3.xy;
    dst->rgbc.rgbc = color;
    dst->p[0].uv.uv = v0.uv;
    dst->p[1].uv.uv = v1.uv;
    dst->p[2].uv.uv = v2.uv;
    dst->p[3].uv.uv = v3.uv;
    dst->p[0].attr = clut_word >> 16;
    dst->p[1].attr = tpage_word >> 16;
    dst->tag = tag;
}

static POLY_TEX_VERTEX poly_tri_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 xy_mask, uint32 uv_mask)
{
    POLY_TEX_VERTEX result;
    uint32 left = v0.xy;
    uint32 right = v1.xy;

    FUNCTION_MARKER(0x8007A1A0, "MAIN.EXE");
    result.xy = left + right - ((left ^ right) & xy_mask) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = left + right - ((left ^ right) & uv_mask) >> 1;
    return result;
}

static POLY_TEX_VERTEX poly_quad_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 xy_mask, uint32 uv_mask)
{
    FUNCTION_MARKER(0x8007A7E8, "MAIN.EXE");
    return poly_tri_tex_verts_avg(v0, v1, xy_mask, uv_mask);
}

POLY_FT4 *poly_subdiv_ft4(POLY_FT4 *prim, POLY_FT4 *dst)
{
    POLY_TEX_VERTEX vertices[4];
    uint32 color;
    POLY_TEX_VERTEX midpoint[4];
    POLY_TEX_VERTEX center;
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;

    FUNCTION_MARKER(0x8007A69C, "MAIN.EXE");
    tag = prim->tag;
    color = prim->rgbc.rgbc;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].uv = prim->p[0].uv.uv;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].uv = prim->p[1].uv.uv;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].uv = prim->p[2].uv.uv;
    vertices[3].xy = prim->p[3].xy.xy;
    vertices[3].uv = prim->p[3].uv.uv;
    midpoint[0] = poly_quad_tex_verts_avg(vertices[0], vertices[1], 0x00010001, 0x01010101);
    midpoint[1] = poly_quad_tex_verts_avg(vertices[1], vertices[3], 0x00010001, 0x01010101);
    midpoint[2] = poly_quad_tex_verts_avg(vertices[3], vertices[2], 0x00010001, 0x01010101);
    midpoint[3] = poly_quad_tex_verts_avg(vertices[2], vertices[0], 0x00010001, 0x01010101);
    center = poly_quad_tex_verts_avg(midpoint[1], midpoint[3], 0x00010001, 0x01010101);
    clut_word = (uint32)prim->p[0].attr << 16;
    tpage_word = (uint32)prim->p[1].attr << 16;
    poly_copy_ft4(color, vertices[0], dst, tag, midpoint[0], midpoint[3], center, clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_ft4(color, midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_ft4(color, center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_ft4(color, midpoint[3], prim, tag, center, vertices[2], midpoint[2], clut_word, tpage_word);
    return dst + 3;
}

// Native strip producers retain packet layout only at the GPU boundary

static void poly_render_ft4_strip(const TRACK_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007A8A8, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_FT4 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_FT4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyFT4(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].xy.xy = prev_scr[3];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                poly->p[3].uv.uv = prim->p[3].uv.uv;
                poly->p[3].attr = prim->p[3].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    POLY_FT4 *next_poly = poly_subdiv_ft4(poly, poly + 1);
                    next_poly = poly_subdiv_ft4(next_poly - 4, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 6, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 8, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 10, next_poly);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_ft3(uint32 color, POLY_TEX_VERTEX v0, POLY_FT3 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A14C, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->rgbc.rgbc = color;
    dst->p[0].uv.uv = v0.uv;
    dst->p[1].uv.uv = v1.uv;
    dst->p[2].uv.uv = v2.uv;
    dst->p[0].attr = clut_word >> 16;
    dst->p[1].attr = tpage_word >> 16;
    dst->tag = tag;
}

POLY_FT3 *poly_subdiv_ft3(POLY_FT3 *prim, POLY_FT3 *dst)
{
    POLY_TEX_VERTEX vertices[3];
    uint32 color;
    POLY_TEX_VERTEX midpoint[3];
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;

    FUNCTION_MARKER(0x8007A094, "MAIN.EXE");
    tag = prim->tag;
    color = prim->rgbc.rgbc;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].uv = prim->p[0].uv.uv;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].uv = prim->p[1].uv.uv;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].uv = prim->p[2].uv.uv;
    midpoint[0] = poly_tri_tex_verts_avg(vertices[0], vertices[1], 0x00010001, 0x01010101);
    midpoint[1] = poly_tri_tex_verts_avg(vertices[1], vertices[2], 0x00010001, 0x01010101);
    midpoint[2] = poly_tri_tex_verts_avg(vertices[2], vertices[0], 0x00010001, 0x01010101);
    clut_word = (uint32)prim->p[0].attr << 16;
    tpage_word = (uint32)prim->p[1].attr << 16;
    poly_copy_ft3(color, vertices[0], dst, tag, midpoint[0], midpoint[2], clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_ft3(color, midpoint[2], dst + 1, tag, midpoint[1], vertices[2], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_ft3(color, midpoint[1], dst + 2, tag, midpoint[2], midpoint[0], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_ft3(color, midpoint[0], prim, tag, vertices[1], midpoint[1], clut_word, tpage_word);
    return dst + 3;
}

static void poly_render_ft3_strip(const TRACK_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007ACC4, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_FT3 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyFT3(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_ft3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_gt4(POLY_TEX_SHADED_VERTEX v0, POLY_GT4 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, POLY_TEX_SHADED_VERTEX v3, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x8007A474, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->p[3].xy.xy = v3.xy;
    dst->p[0].rgbc.rgbc = v0.color;
    dst->p[1].rgbc.rgbc = v1.color;
    dst->p[2].rgbc.rgbc = v2.color;
    dst->p[3].rgbc.rgbc = v3.color;
    dst->p[0].uv.uv = v0.uv;
    dst->p[1].uv.uv = v1.uv;
    dst->p[2].uv.uv = v2.uv;
    dst->p[3].uv.uv = v3.uv;
    dst->p[0].attr = clut_word >> 16;
    dst->p[1].attr = tpage_word >> 16;
    dst->tag = tag;
}

static POLY_TEX_SHADED_VERTEX poly_masked_vert_pairs_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 color_uv_mask, uint32 xy_mask)
{
    POLY_TEX_SHADED_VERTEX result;
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x8007A4F0, "MAIN.EXE");
    left = v0.color;
    right = v1.color;
    result.color = left + right - ((left ^ right) & color_uv_mask) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = left + right - ((left ^ right) & xy_mask) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = left + right - ((left ^ right) & color_uv_mask) >> 1;
    return result;
}

POLY_GT4 *poly_subdiv_gt4(POLY_GT4 *prim, POLY_GT4 *dst)
{
    POLY_TEX_SHADED_VERTEX vertices[4];
    POLY_TEX_SHADED_VERTEX midpoint[4];
    POLY_TEX_SHADED_VERTEX center;
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;
    uint8 shade;

    FUNCTION_MARKER(0x8007A378, "MAIN.EXE");
    shade = prim->p[0].rgbc.code;
    prim->p[1].rgbc.code = shade;
    prim->p[2].rgbc.code = shade;
    prim->p[3].rgbc.code = shade;
    tag = prim->tag;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].color = prim->p[0].rgbc.rgbc;
    vertices[0].uv = prim->p[0].uv.uv;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].color = prim->p[1].rgbc.rgbc;
    vertices[1].uv = prim->p[1].uv.uv;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].color = prim->p[2].rgbc.rgbc;
    vertices[2].uv = prim->p[2].uv.uv;
    vertices[3].xy = prim->p[3].xy.xy;
    vertices[3].color = prim->p[3].rgbc.rgbc;
    vertices[3].uv = prim->p[3].uv.uv;
    midpoint[0] = poly_masked_vert_pairs_avg(vertices[0], vertices[1], 0x01010101, 0x00010001);
    midpoint[1] = poly_masked_vert_pairs_avg(vertices[1], vertices[3], 0x01010101, 0x00010001);
    midpoint[2] = poly_masked_vert_pairs_avg(vertices[3], vertices[2], 0x01010101, 0x00010001);
    midpoint[3] = poly_masked_vert_pairs_avg(vertices[2], vertices[0], 0x01010101, 0x00010001);
    center = poly_masked_vert_pairs_avg(midpoint[1], midpoint[3], 0x01010101, 0x00010001);
    clut_word = (uint32)prim->p[0].attr << 16;
    tpage_word = (uint32)prim->p[1].attr << 16;
    poly_copy_gt4(vertices[0], dst, tag, midpoint[0], midpoint[3], center, clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_gt4(midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_gt4(center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_gt4(midpoint[3], prim, tag, center, vertices[2], midpoint[2], clut_word, tpage_word);
    return dst + 3;
}

static void poly_render_gt4_strip(const TRACK_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B014, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_GT4 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyGT4(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].rgbc.rgbc = prim->p[3].rgbc.rgbc;
                poly->p[3].xy.xy = prev_scr[3];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                poly->p[3].uv.uv = prim->p[3].uv.uv;
                poly->p[3].attr = prim->p[3].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_gt4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_gt3(POLY_TEX_SHADED_VERTEX v0, POLY_GT3 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, uint32 clut_word, uint32 tpage_word)
{
    FUNCTION_MARKER(0x80079EBC, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->p[0].rgbc.rgbc = v0.color;
    dst->p[1].rgbc.rgbc = v1.color;
    dst->p[2].rgbc.rgbc = v2.color;
    dst->p[0].uv.uv = v0.uv;
    dst->p[1].uv.uv = v1.uv;
    dst->p[2].uv.uv = v2.uv;
    dst->p[0].attr = clut_word >> 16;
    dst->p[1].attr = tpage_word >> 16;
    dst->tag = tag;
}

static POLY_TEX_SHADED_VERTEX poly_tri_tex_shaded_verts_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 color_uv_mask, uint32 xy_mask)
{
    POLY_TEX_SHADED_VERTEX result;
    uint32 left;
    uint32 right;

    FUNCTION_MARKER(0x80079F20, "MAIN.EXE");
    left = v0.color;
    right = v1.color;
    result.color = left + right - ((left ^ right) & color_uv_mask) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = left + right - ((left ^ right) & xy_mask) >> 1;
    left = v0.uv;
    right = v1.uv;
    result.uv = left + right - ((left ^ right) & color_uv_mask) >> 1;
    return result;
}

static POLY_GT3 *poly_subdiv_gt3(POLY_GT3 *prim, POLY_GT3 *dst)
{
    POLY_TEX_SHADED_VERTEX vertices[3];
    POLY_TEX_SHADED_VERTEX midpoint[3];
    uint32 tag;
    uint32 clut_word;
    uint32 tpage_word;
    uint8 shade;

    FUNCTION_MARKER(0x80079DF4, "MAIN.EXE");
    shade = prim->p[0].rgbc.code;
    prim->p[1].rgbc.code = shade;
    prim->p[2].rgbc.code = shade;
    tag = prim->tag;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].color = prim->p[0].rgbc.rgbc;
    vertices[0].uv = prim->p[0].uv.uv;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].color = prim->p[1].rgbc.rgbc;
    vertices[1].uv = prim->p[1].uv.uv;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].color = prim->p[2].rgbc.rgbc;
    vertices[2].uv = prim->p[2].uv.uv;
    midpoint[0] = poly_tri_tex_shaded_verts_avg(vertices[0], vertices[1], 0x01010101, 0x00010001);
    midpoint[1] = poly_tri_tex_shaded_verts_avg(vertices[1], vertices[2], 0x01010101, 0x00010001);
    midpoint[2] = poly_tri_tex_shaded_verts_avg(vertices[2], vertices[0], 0x01010101, 0x00010001);
    clut_word = (uint32)prim->p[0].attr << 16;
    tpage_word = (uint32)prim->p[1].attr << 16;
    poly_copy_gt3(vertices[0], dst, tag, midpoint[0], midpoint[2], clut_word, tpage_word);
    AddPrim(&tag, dst);
    poly_copy_gt3(midpoint[2], dst + 1, tag, midpoint[1], vertices[2], clut_word, tpage_word);
    AddPrim(&tag, dst + 1);
    poly_copy_gt3(midpoint[1], dst + 2, tag, midpoint[2], midpoint[0], clut_word, tpage_word);
    AddPrim(&tag, dst + 2);
    poly_copy_gt3(midpoint[0], prim, tag, vertices[1], midpoint[1], clut_word, tpage_word);
    return dst + 3;
}

static void poly_render_gt3_strip(const TRACK_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B418, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_GT3 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyGT3(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_gt3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_f4(uint32 color, uint32 v0, POLY_F4 *dst, uint32 tag, uint32 v1, uint32 v2, uint32 v3)
{
    FUNCTION_MARKER(0x8007A638, "MAIN.EXE");
    dst->p[0].xy = v0;
    dst->p[1].xy = v1;
    dst->p[2].xy = v2;
    dst->p[3].xy = v3;
    dst->rgbc.rgbc = color;
    dst->tag = tag;
}

static uint32 poly_tri_flat_verts_avg(uint32 v0, uint32 v1, uint32 xy_mask)
{
    FUNCTION_MARKER(0x8007A06C, "MAIN.EXE");
    return v0 + v1 - ((v0 ^ v1) & xy_mask) >> 1;
}

static uint32 poly_coord_packed_avg(uint32 v0, uint32 v1, uint32 xy_mask)
{
    FUNCTION_MARKER(0x8007A674, "MAIN.EXE");
    return poly_tri_flat_verts_avg(v0, v1, xy_mask);
}

static POLY_F4 *poly_subdiv_f4(POLY_F4 *prim, POLY_F4 *dst)
{
    uint32 vertices[4];
    uint32 color;
    uint32 midpoint[4];
    uint32 center;
    uint32 tag;

    FUNCTION_MARKER(0x8007A560, "MAIN.EXE");
    tag = prim->tag;
    color = prim->rgbc.rgbc;
    vertices[0] = prim->p[0].xy;
    vertices[1] = prim->p[1].xy;
    vertices[2] = prim->p[2].xy;
    vertices[3] = prim->p[3].xy;
    midpoint[0] = poly_coord_packed_avg(vertices[0], vertices[1], 0x00010001);
    midpoint[1] = poly_coord_packed_avg(vertices[1], vertices[3], 0x00010001);
    midpoint[2] = poly_coord_packed_avg(vertices[3], vertices[2], 0x00010001);
    midpoint[3] = poly_coord_packed_avg(vertices[2], vertices[0], 0x00010001);
    center = poly_coord_packed_avg(midpoint[1], midpoint[3], 0x00010001);
    poly_copy_f4(color, vertices[0], dst, tag, midpoint[0], midpoint[3], center);
    AddPrim(&tag, dst);
    poly_copy_f4(color, midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1]);
    AddPrim(&tag, dst + 1);
    poly_copy_f4(color, center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3]);
    AddPrim(&tag, dst + 2);
    poly_copy_f4(color, midpoint[3], prim, tag, center, vertices[2], midpoint[2]);
    return dst + 3;
}

static void poly_render_f4_strip(const TRACK_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007B778, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_F4 *prim = prims + i - 1;
            sint32 base_depth = prev_avg - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = base_depth - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && base_depth >= 0 && base_depth < 0xC60)
            {
                bucket = poly_order_bucket(ot, relative_depth);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, base_depth, 0x280);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_F4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyF4(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].x = prev_scr[0];
                poly->p[0].y = (uint32)prev_scr[0] >> 16;
                poly->p[1].x = prev_scr[1];
                poly->p[1].y = (uint32)prev_scr[1] >> 16;
                poly->p[2].x = prev_scr[2];
                poly->p[2].y = (uint32)prev_scr[2] >> 16;
                poly->p[3].x = prev_scr[3];
                poly->p[3].y = (uint32)prev_scr[3] >> 16;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    POLY_F4 *next_poly = poly_subdiv_f4(poly, poly + 1);
                    next_poly = poly_subdiv_f4(next_poly - 4, next_poly);
                    next_poly = poly_subdiv_f4(next_poly - 6, next_poly);
                    next_poly = poly_subdiv_f4(next_poly - 8, next_poly);
                    next_poly = poly_subdiv_f4(next_poly - 10, next_poly);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_f3(uint32 color, uint32 v0, POLY_F3 *dst, uint32 tag, uint32 v1, uint32 v2)
{
    FUNCTION_MARKER(0x8007A038, "MAIN.EXE");
    dst->p[0].xy = v0;
    dst->p[1].xy = v1;
    dst->p[2].xy = v2;
    dst->rgbc.rgbc = color;
    dst->tag = tag;
}

static POLY_F3 *poly_subdiv_f3(POLY_F3 *prim, POLY_F3 *dst)
{
    uint32 vertices[3];
    uint32 color;
    uint32 midpoint[3];
    uint32 tag;

    FUNCTION_MARKER(0x80079F90, "MAIN.EXE");
    tag = prim->tag;
    color = prim->rgbc.rgbc;
    vertices[0] = prim->p[0].xy;
    vertices[1] = prim->p[1].xy;
    vertices[2] = prim->p[2].xy;
    midpoint[0] = poly_tri_flat_verts_avg(vertices[0], vertices[1], 0x00010001);
    midpoint[1] = poly_tri_flat_verts_avg(vertices[1], vertices[2], 0x00010001);
    midpoint[2] = poly_tri_flat_verts_avg(vertices[2], vertices[0], 0x00010001);
    poly_copy_f3(color, vertices[0], dst, tag, midpoint[0], midpoint[2]);
    AddPrim(&tag, dst);
    poly_copy_f3(color, midpoint[2], dst + 1, tag, midpoint[1], vertices[2]);
    AddPrim(&tag, dst + 1);
    poly_copy_f3(color, midpoint[1], dst + 2, tag, midpoint[2], midpoint[0]);
    AddPrim(&tag, dst + 2);
    poly_copy_f3(color, midpoint[0], prim, tag, vertices[1], midpoint[1]);
    return dst + 3;
}

static void poly_render_f3_strip(const TRACK_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007BB90, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_F3 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyF3(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].x = prev_scr[0];
                poly->p[0].y = (uint32)prev_scr[0] >> 16;
                poly->p[1].x = prev_scr[1];
                poly->p[1].y = (uint32)prev_scr[1] >> 16;
                poly->p[2].x = prev_scr[2];
                poly->p[2].y = (uint32)prev_scr[2] >> 16;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_f3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_g4(POLY_SHADED_VERTEX v0, POLY_G4 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2, POLY_SHADED_VERTEX v3)
{
    FUNCTION_MARKER(0x8007A2D8, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->p[3].xy.xy = v3.xy;
    dst->p[0].rgbc.rgbc = v0.color;
    dst->p[1].rgbc.rgbc = v1.color;
    dst->p[2].rgbc.rgbc = v2.color;
    dst->p[3].rgbc.rgbc = v3.color;
    dst->tag = tag;
}

static POLY_SHADED_VERTEX poly_tri_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 color_mask, uint32 xy_mask)
{
    POLY_SHADED_VERTEX result;
    uint32 left = v0.color;
    uint32 right = v1.color;

    FUNCTION_MARKER(0x80079DA8, "MAIN.EXE");
    result.color = left + right - ((left ^ right) & color_mask) >> 1;
    left = v0.xy;
    right = v1.xy;
    result.xy = left + right - ((left ^ right) & xy_mask) >> 1;
    return result;
}

static POLY_SHADED_VERTEX poly_quad_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 color_mask, uint32 xy_mask)
{
    FUNCTION_MARKER(0x8007A32C, "MAIN.EXE");
    return poly_tri_shaded_verts_avg(v0, v1, color_mask, xy_mask);
}

static POLY_G4 *poly_subdiv_g4(POLY_G4 *prim, POLY_G4 *dst)
{
    POLY_SHADED_VERTEX vertices[4];
    POLY_SHADED_VERTEX midpoint[4];
    POLY_SHADED_VERTEX center;
    uint32 tag;
    uint8 shade;

    FUNCTION_MARKER(0x8007A1EC, "MAIN.EXE");
    shade = prim->p[0].rgbc.code;
    // Preserve the GPU command in the high byte of each subdiv vertex color
    prim->p[1].rgbc.code = shade;
    prim->p[2].rgbc.code = shade;
    prim->p[3].rgbc.code = shade;
    tag = prim->tag;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].color = prim->p[0].rgbc.rgbc;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].color = prim->p[1].rgbc.rgbc;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].color = prim->p[2].rgbc.rgbc;
    vertices[3].xy = prim->p[3].xy.xy;
    vertices[3].color = prim->p[3].rgbc.rgbc;
    midpoint[0] = poly_quad_shaded_verts_avg(vertices[0], vertices[1], 0x01010101, 0x00010001);
    midpoint[1] = poly_quad_shaded_verts_avg(vertices[1], vertices[3], 0x01010101, 0x00010001);
    midpoint[2] = poly_quad_shaded_verts_avg(vertices[3], vertices[2], 0x01010101, 0x00010001);
    midpoint[3] = poly_quad_shaded_verts_avg(vertices[2], vertices[0], 0x01010101, 0x00010001);
    center = poly_quad_shaded_verts_avg(midpoint[1], midpoint[3], 0x01010101, 0x00010001);
    poly_copy_g4(vertices[0], dst, tag, midpoint[0], midpoint[3], center);
    AddPrim(&tag, dst);
    poly_copy_g4(midpoint[0], dst + 1, tag, vertices[1], center, midpoint[1]);
    AddPrim(&tag, dst + 1);
    poly_copy_g4(center, dst + 2, tag, midpoint[1], midpoint[2], vertices[3]);
    AddPrim(&tag, dst + 2);
    poly_copy_g4(midpoint[3], prim, tag, center, vertices[2], midpoint[2]);
    return dst + 3;
}

static void poly_render_g4_strip(const TRACK_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007BEC8, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_G4 *prim = prims + i - 1;
            sint32 base_depth = prev_avg - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = base_depth - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && base_depth >= 0 && base_depth < 0xC60)
            {
                bucket = poly_order_bucket(ot, relative_depth);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, base_depth, 0x3C0);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyG4(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].rgbc.rgbc = prim->p[3].rgbc.rgbc;
                poly->p[3].xy.xy = prev_scr[3];
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_g4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_copy_g3(POLY_SHADED_VERTEX v0, POLY_G3 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2)
{
    FUNCTION_MARKER(0x80079D64, "MAIN.EXE");
    dst->p[0].xy.xy = v0.xy;
    dst->p[1].xy.xy = v1.xy;
    dst->p[2].xy.xy = v2.xy;
    dst->p[0].rgbc.rgbc = v0.color;
    dst->p[1].rgbc.rgbc = v1.color;
    dst->p[2].rgbc.rgbc = v2.color;
    dst->tag = tag;
}

static POLY_G3 *poly_subdiv_g3(POLY_G3 *prim, POLY_G3 *dst)
{
    POLY_SHADED_VERTEX vertices[3];
    POLY_SHADED_VERTEX midpoint[3];
    uint32 tag;
    uint8 shade;

    FUNCTION_MARKER(0x80079CAC, "MAIN.EXE");
    shade = prim->p[0].rgbc.code;
    // Preserve the GPU command in the high byte of each subdiv vertex color
    prim->p[1].rgbc.code = shade;
    prim->p[2].rgbc.code = shade;
    tag = prim->tag;
    vertices[0].xy = prim->p[0].xy.xy;
    vertices[0].color = prim->p[0].rgbc.rgbc;
    vertices[1].xy = prim->p[1].xy.xy;
    vertices[1].color = prim->p[1].rgbc.rgbc;
    vertices[2].xy = prim->p[2].xy.xy;
    vertices[2].color = prim->p[2].rgbc.rgbc;
    midpoint[0] = poly_tri_shaded_verts_avg(vertices[0], vertices[1], 0x01010101, 0x00010001);
    midpoint[1] = poly_tri_shaded_verts_avg(vertices[1], vertices[2], 0x01010101, 0x00010001);
    midpoint[2] = poly_tri_shaded_verts_avg(vertices[2], vertices[0], 0x01010101, 0x00010001);
    poly_copy_g3(vertices[0], dst, tag, midpoint[0], midpoint[2]);
    AddPrim(&tag, dst);
    poly_copy_g3(midpoint[2], dst + 1, tag, midpoint[1], vertices[2]);
    AddPrim(&tag, dst + 1);
    poly_copy_g3(midpoint[1], dst + 2, tag, midpoint[2], midpoint[0]);
    AddPrim(&tag, dst + 2);
    poly_copy_g3(midpoint[0], prim, tag, vertices[1], midpoint[1]);
    return dst + 3;
}

static void poly_render_g3_strip(const TRACK_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007C2C8, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_G3 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyG3(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_g3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = cur_cross > 0;
        if (prev_valid)
        {
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_ft4_clipped_strip(const TRACK_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007C610, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;
        sint32 cur_cross;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        cur_cross = NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_FT4 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 16u : 1u;
                POLY_FT4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyFT4(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].xy.xy = prev_scr[3];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                poly->p[3].uv.uv = prim->p[3].uv.uv;
                poly->p[3].attr = prim->p[3].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    POLY_FT4 *next_poly = poly_subdiv_ft4(poly, poly + 1);
                    next_poly = poly_subdiv_ft4(next_poly - 4, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 6, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 8, next_poly);
                    next_poly = poly_subdiv_ft4(next_poly - 10, next_poly);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 0;
        if (cur_cross > 0)
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
            if ((((uint32)cur_flag | (uint32)flag3) >> 22 & 7) == 0)
            {
                prev_scr[0] = cur_scr[0];
                prev_scr[1] = cur_scr[1];
                prev_scr[2] = cur_scr[2];
                prev_scr[3] = cur_scr[3];
                prev_valid = 1;
            }
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_ft4_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007CA40, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_FT4 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyFT4(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].xy.xy = prev_scr[3];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                poly->p[3].uv.uv = prim->p[3].uv.uv;
                poly->p[3].attr = prim->p[3].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_ft4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_ft3_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007CE20, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_FT3 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_FT3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyFT3(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_ft3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        prev_scr[0] = cur_scr[0];
        prev_scr[1] = cur_scr[1];
        prev_scr[2] = cur_scr[2];
        prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
    }
    render_packet_publish(packet_offset);
}

static void poly_render_gt4_mixed_strip(const TRACK_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D164, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_GT4 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyGT4(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].rgbc.rgbc = prim->p[3].rgbc.rgbc;
                poly->p[3].xy.xy = prev_scr[3];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                poly->p[3].uv.uv = prim->p[3].uv.uv;
                poly->p[3].attr = prim->p[3].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_gt4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_gt3_mixed_strip(const TRACK_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D55C, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_GT3 *prim = prims + i - 1;
            // Unused GPU halfword stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - prim->p[2].attr;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_GT3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyGT3(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[0].uv.uv = prim->p[0].uv.uv;
                poly->p[0].attr = prim->p[0].attr;
                poly->p[1].uv.uv = prim->p[1].uv.uv;
                poly->p[1].attr = prim->p[1].attr;
                poly->p[2].uv.uv = prim->p[2].uv.uv;
                poly->p[2].attr = prim->p[2].attr;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_gt3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        prev_scr[0] = cur_scr[0];
        prev_scr[1] = cur_scr[1];
        prev_scr[2] = cur_scr[2];
        prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
    }
    render_packet_publish(packet_offset);
}

static void poly_render_f4_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007D8B0, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_F4 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyF4(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].x = prev_scr[0];
                poly->p[0].y = (uint32)prev_scr[0] >> 16;
                poly->p[1].x = prev_scr[1];
                poly->p[1].y = (uint32)prev_scr[1] >> 16;
                poly->p[2].x = prev_scr[2];
                poly->p[2].y = (uint32)prev_scr[2] >> 16;
                poly->p[3].x = prev_scr[3];
                poly->p[3].y = (uint32)prev_scr[3] >> 16;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_f4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_f3_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007DC70, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_F3 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_F3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));
                uint32 color = prim->rgbc.rgbc;

                setPolyF3(poly);
                poly->rgbc.rgbc = color;
                poly->p[0].x = prev_scr[0];
                poly->p[0].y = (uint32)prev_scr[0] >> 16;
                poly->p[1].x = prev_scr[1];
                poly->p[1].y = (uint32)prev_scr[1] >> 16;
                poly->p[2].x = prev_scr[2];
                poly->p[2].y = (uint32)prev_scr[2] >> 16;
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_f3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        prev_scr[0] = cur_scr[0];
        prev_scr[1] = cur_scr[1];
        prev_scr[2] = cur_scr[2];
        prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
    }
    render_packet_publish(packet_offset);
}

static void poly_render_g4_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[4];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007DF9C, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[4];
        sint32 cur_depth[4];
        sint32 cur_flag;
        sint32 flag3;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_G4 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            outcode &= poly_scr_outcode(prev_scr[3], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 4, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G4 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyG4(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                poly->p[3].rgbc.rgbc = prim->p[3].rgbc.rgbc;
                poly->p[3].xy.xy = prev_scr[3];
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_g4(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        {
            cur_depth[3] = gte_project_full_depth(&face->v[3], &cur_scr[3], &flag3);
            prev_scr[0] = cur_scr[0];
            prev_scr[1] = cur_scr[1];
            prev_scr[2] = cur_scr[2];
            prev_scr[3] = cur_scr[3];
            prev_avg = AverageZ4(cur_depth[0], cur_depth[1], cur_depth[2], cur_depth[3]);
        }
    }
    render_packet_publish(packet_offset);
}

static void poly_render_g3_alt_strip(const TRACK_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot)
{
    sint32 prev_scr[3];
    sint32 prev_avg = 0;
    sint32 prev_valid = 0;
    sint32 width;
    sint32 height;
    sint32 i;
    uint32 packet_offset;

    FUNCTION_MARKER(0x8007E374, "MAIN.EXE");
    if (count <= 0)
        return;
    packet_offset = render_packet_offset();
    height = disp_state.view_height;
    width = disp_state.view_width;
    for (i = 0; i <= count; ++i)
    {
        const TRACK_PRIM_FACE *face = faces + i;
        sint32 cur_scr[3];
        sint32 cur_depth[3];
        sint32 cur_flag;

        gte_project3_full_depth(face->v, cur_scr, cur_depth, &cur_flag);
        NormalClip(cur_scr[0], cur_scr[1], cur_scr[2]);
        if (prev_valid)
        {
            const POLY_G3 *prim = prims + i - 1;
            // Untextured vertex pad stores the previous face OT depth bias
            sint32 relative_depth = prev_avg - 1 - (uint16)faces[i - 1].v[0].pad;
            uint32 outcode = poly_scr_outcode(prev_scr[0], width, height);
            uint32 *bucket;

            outcode &= poly_scr_outcode(prev_scr[1], width, height);
            outcode &= poly_scr_outcode(prev_scr[2], width, height);
            if (outcode == 0 && relative_depth >= 0 && relative_depth < 0xC60)
            {
                bucket = ot + 1 + (relative_depth >> 5);
                sint32 subdiv = poly_needs_subdiv(prev_scr, 3, relative_depth, 0x280);
                uint32 packets = subdiv ? 4u : 1u;
                POLY_G3 *poly = render_packet_at(packet_offset, packets * sizeof(*poly));

                setPolyG3(poly);
                poly->p[0].rgbc.rgbc = prim->p[0].rgbc.rgbc;
                poly->p[0].xy.xy = prev_scr[0];
                poly->p[1].rgbc.rgbc = prim->p[1].rgbc.rgbc;
                poly->p[1].xy.xy = prev_scr[1];
                poly->p[2].rgbc.rgbc = prim->p[2].rgbc.rgbc;
                poly->p[2].xy.xy = prev_scr[2];
                AddPrim(bucket, poly);
                if (subdiv)
                {
                    poly_subdiv_g3(poly, poly + 1);
                    height = disp_state.view_height;
                    width = disp_state.view_width;
                }
                packet_offset += packets * sizeof(*poly);
            }
        }
        prev_valid = 1;
        prev_scr[0] = cur_scr[0];
        prev_scr[1] = cur_scr[1];
        prev_scr[2] = cur_scr[2];
        prev_avg = AverageZ3(cur_depth[0], cur_depth[1], cur_depth[2]);
    }
    render_packet_publish(packet_offset);
}

void poly_dispatch_prim_grps(const TRACK_PRIM_GROUP *src, uint32 slot, uint32 *ot, sint32 adjusted, sint32 alternate)
{
    sint32 count;
    const TRACK_PRIM_FACE *vertices;
    const void *prims;

    if (slot >= 2)
        abort();
    if (adjusted != 0)
    {
        count = src->sets[0].count;
        vertices = src->sets[0].faces;
        prims = count ? src->prims[slot][0].f3 + count : NULL;
        (alternate != 0 ? poly_render_f3_alt_strip : poly_render_f3_strip)(vertices, prims, count, ot);
        count = src->sets[1].count;
        vertices = src->sets[1].faces;
        prims = count ? src->prims[slot][1].f4 + count : NULL;
        (alternate != 0 ? poly_render_f4_alt_strip : poly_render_f4_strip)(vertices, prims, count, ot);
        count = src->sets[4].count;
        vertices = src->sets[4].faces;
        prims = count ? src->prims[slot][4].g3 + count : NULL;
        (alternate != 0 ? poly_render_g3_alt_strip : poly_render_g3_strip)(vertices, prims, count, ot);
        count = src->sets[5].count;
        vertices = src->sets[5].faces;
        prims = count ? src->prims[slot][5].g4 + count : NULL;
        (alternate != 0 ? poly_render_g4_alt_strip : poly_render_g4_strip)(vertices, prims, count, ot);
        vertices = src->sets[2].faces;
        count = src->sets[2].count;
        prims = count ? src->prims[slot][2].ft3 + count : NULL;
        (alternate != 0 ? poly_render_ft3_alt_strip : poly_render_ft3_strip)(vertices, prims, count, ot);
        if (alternate != 0)
        {
            count = src->sets[3].count;
            vertices = src->sets[3].faces;
            prims = count ? src->prims[slot][3].ft4 + count : NULL;
            poly_render_ft4_alt_strip(vertices, prims, count, ot);
        }
        else
        {
            uint32 mode = race_cfg.course;

            count = src->sets[3].count;
            vertices = src->sets[3].faces;
            prims = count ? src->prims[slot][3].ft4 + count : NULL;
            if (mode == 1)
                poly_render_ft4_clipped_strip(vertices, prims, count, ot);
            else
                poly_render_ft4_strip(vertices, prims, count, ot);
        }
        count = src->sets[6].count;
        vertices = src->sets[6].faces;
        prims = count ? src->prims[slot][6].gt3 + count : NULL;
        (alternate != 0 ? poly_render_gt3_mixed_strip : poly_render_gt3_strip)(vertices, prims, count, ot);
        count = src->sets[7].count;
        vertices = src->sets[7].faces;
        prims = count ? src->prims[slot][7].gt4 + count : NULL;
        (alternate != 0 ? poly_render_gt4_mixed_strip : poly_render_gt4_strip)(vertices, prims, count, ot);
        return;
    }

    vertices = src->sets[0].faces;
    prims = src->prims[slot][0].f3;
    count = src->sets[0].count;
    (alternate != 0 ? poly_render_f3_alt_strip : poly_render_f3_strip)(vertices, prims, count, ot);
    vertices = src->sets[1].faces;
    prims = src->prims[slot][1].f4;
    count = src->sets[1].count;
    (alternate != 0 ? poly_render_f4_alt_strip : poly_render_f4_strip)(vertices, prims, count, ot);
    vertices = src->sets[4].faces;
    prims = src->prims[slot][4].g3;
    count = src->sets[4].count;
    (alternate != 0 ? poly_render_g3_alt_strip : poly_render_g3_strip)(vertices, prims, count, ot);
    vertices = src->sets[5].faces;
    prims = src->prims[slot][5].g4;
    count = src->sets[5].count;
    (alternate != 0 ? poly_render_g4_alt_strip : poly_render_g4_strip)(vertices, prims, count, ot);
    vertices = src->sets[2].faces;
    prims = src->prims[slot][2].ft3;
    count = src->sets[2].count;
    (alternate != 0 ? poly_render_ft3_alt_strip : poly_render_ft3_strip)(vertices, prims, count, ot);
    if (alternate != 0)
    {
        vertices = src->sets[3].faces;
        prims = src->prims[slot][3].ft4;
        count = src->sets[3].count;
        poly_render_ft4_alt_strip(vertices, prims, count, ot);
    }
    else
    {
        uint32 mode = race_cfg.course;

        vertices = src->sets[3].faces;
        prims = src->prims[slot][3].ft4;
        count = src->sets[3].count;
        if (mode == 1)
            poly_render_ft4_clipped_strip(vertices, prims, count, ot);
        else
            poly_render_ft4_strip(vertices, prims, count, ot);
    }
    vertices = src->sets[6].faces;
    prims = src->prims[slot][6].gt3;
    count = src->sets[6].count;
    (alternate != 0 ? poly_render_gt3_mixed_strip : poly_render_gt3_strip)(vertices, prims, count, ot);
    vertices = src->sets[7].faces;
    prims = src->prims[slot][7].gt4;
    count = src->sets[7].count;
    (alternate != 0 ? poly_render_gt4_mixed_strip : poly_render_gt4_strip)(vertices, prims, count, ot);
}
