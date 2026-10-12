#include "hud.h"
#include "track.h"
#include "mesh.h"
#include "route.h"
#include "game.h"
#include "profile.h"
#include "camera.h"
#include "arena.h"
#include "effects.h"
#include "render.h"
#include "race.h"
#include "river.h"
#include "horizon.h"
#include "polygon.h"
#include "disp.h"
#include "utils.h"
#include "boat.h"
#include "sound.h"
#include "callbacks.h"
#include "xport_trace.h"
#include "psx_gpu.h"
#include <stdlib.h>

typedef struct
{
    uint32 color;
    sint32 scr;
    sint32 alternate_scr;
    sint16 depth;
    uint16 flags;
} ANIM_VERTEX;

// Shared billboard vertex colors from both original polygon passes
static const uint32 river_billboard_colors[8] = {
    0x3C000000, 0x3CE0F0F0, 0x3C0000FF, 0x3C10E0E0, 0x3C80FF80, 0x3CFF8080, 0x3C000000, 0x3C000000,
};

static sint32 river_project_vertex(const ROUTE_VERTEX *rec, sint32 ox, sint32 oy, sint32 oz, sint32 *scr)
{
    SVECTOR point;
    sint32 flags;

    point.vx = ox + 16 * rec->pos[0];
    point.vy = oy + 16 * rec->pos[1];
    point.vz = oz + 16 * rec->pos[2];
    point.pad = 0;
    return gte_project_full_depth(&point, scr, &flags);
}

static uint16 river_tex_coord(const ROUTE_VERTEX *rec, uint32 phase)
{
    uint32 idx = 2u * rec->tex_phase + phase & 0x1FE;

    return render_curve[2][idx] + 0x40u;
}

static uint32 river_vertex_color(uint32 pal, sint32 factor)
{
    uint32 result = 0x3C000000;
    uint32 shift;

    for (shift = 0; shift < 24; shift += 8)
    {
        sint32 val = factor * (sint32)(pal >> shift & 0xFF) >> 12;

        val = clamp(val, 0, 255);
        result |= (uint32)val << shift;
    }
    return result;
}

// Encode registered packets through the shared PsyQ boundary
static uint32 river_packet_link(void *packet)
{
    uint32 *tag = packet;
    uint32 saved = *tag;
    uint32 link = 0;

    AddPrim(&link, packet);
    *tag = saved;
    return link;
}

static const ROUTE_SEGMENT *river_render_pass(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3], sint32 generated_course)
{
    ANIM_VERTEX proj[2][32];
    uint32 *ordering;
    uint32 *pending_bucket;
    uint32 packet_offset;
    const ROUTE_SEGMENT *desc;
    uint32 remaining_passes = pass_count;
    sint32 scr_limit_x;
    sint32 scr_limit_y;
    sint32 prev_buffer = -1;
    sint32 cur_buffer = 0;

    if (remaining_passes == 0)
        return mesh;
    ordering = render_order.route_ot;
    pending_bucket = ordering;
    packet_offset = render_packet_offset();
    render_billboards.count = 0;
    scr_limit_y = (sint16)disp_state.view_height;
    scr_limit_x = (sint16)disp_state.view_width;
    desc = mesh->prev.seg;
    while (remaining_passes != 0)
    {
        sint32 phase = game_timing.ticks;
        sint32 ox = desc->origin[0] + transform[0];
        sint32 oy = desc->origin[1] + transform[1];
        sint32 oz = desc->origin[2] + transform[2];
        sint32 vertex_count = desc->vertex_count;
        const uint32 *pal_entry = render_pal(desc->pal);
        uint32 pal = pal_entry[0];
        sint32 vertex;

        for (vertex = 0; vertex < vertex_count; ++vertex)
        {
            const ROUTE_VERTEX *src = &desc->vertices[vertex];
            SVECTOR point;
            sint32 flags;
            XY scr;
            sint32 depth;
            sint32 x;
            sint32 y;
            sint32 deform0;
            sint32 deform1;
            sint32 vertical_deform;
            sint32 shading;
            uint16 deform;
            uint8 low_deform;
            uint8 high_deform;
            uint8 outcode;
            uint8 tex;
            uint8 type;

            point.vx = ox + 16 * src->pos[0];
            point.vy = oy + 16 * src->pos[1];
            point.vz = oz + 16 * src->pos[2];
            point.pad = 0;
            low_deform = src->curve & 15u;
            high_deform = src->curve >> 4;
            deform0 = render_curve[low_deform][(sint8)src->phase[0] * 2 + phase & 0x1FF];
            deform1 = render_curve[high_deform][(sint8)src->phase[1] * 2 + phase & 0x1FF];
            vertical_deform = -(deform0 + deform1);
            shading = 2 * (low_deform + high_deform);
            if ((uint16)(phase - src->impact_tick) < 256u)
            {
                sint32 adjust = 4 * render_curve[10][(uint16)(phase - src->impact_tick)] - 80;

                shading -= adjust;
                vertical_deform -= adjust >> 3;
            }
            deform = (uint8)src->deform[0] | (uint8)src->deform[1] << 8;
            proj[cur_buffer][vertex].alternate_scr = 0;
            if (deform != 0)
            {
                SVECTOR base = point;
                sint32 unused_flags;

                gte_project_full_depth(&base, &proj[cur_buffer][vertex].alternate_scr, &unused_flags);
                point.vx = base.vx + (vertical_deform * ((sint8)deform * 128) >> 12);
                point.vy = base.vy - vertical_deform;
                point.vz = base.vz + (vertical_deform * ((sint8)(deform >> 8) * 128) >> 12);
                proj[cur_buffer][vertex].color = 0x3C3F3F3F;
                outcode = 0;
            }
            else
            {
                point.vy = point.vy - vertical_deform;
                shading = -shading - vertical_deform + src->shade;
                if (shading < 3)
                    shading = 3;
                proj[cur_buffer][vertex].color = river_vertex_color(pal, shading << 5);
                outcode = 0x10;
            }
            depth = gte_project_full_depth(&point, (sint32 *)&scr.xy, &flags);
            if (deform == 0)
                proj[cur_buffer][vertex].alternate_scr = scr.xy;
            type = src->flags >> 5;
            if (type != 0 && render_billboards.count < 64)
            {
                render_add_billboard(point.vx, point.vy, point.vz, type);
                proj[cur_buffer][vertex].color = river_billboard_colors[type];
            }
            x = scr.x;
            y = scr.y;
            if (y < 0)
                outcode |= 8;
            if (y >= scr_limit_y)
                outcode |= 4;
            if (x < 0)
                outcode |= 2;
            if (x >= scr_limit_x)
                outcode |= 1;
            if (generated_course)
                tex = render_curve[8][2u * src->tex_phase + phase & 0x1FFu] + 32u;
            else
                tex = render_curve[15][2u * src->tex_phase + 2u * phase & 0x1FFu];
            proj[cur_buffer][vertex].scr = scr.xy;
            proj[cur_buffer][vertex].depth = depth;
            proj[cur_buffer][vertex].flags = tex << 8 | outcode;
        }
        if (prev_buffer >= 0)
        {
            const uint32 *colors = pal_entry + 2;
            sint32 idx0 = desc->join[1];
            sint32 idx1 = desc->join[0];
            uint32 pending_head = 0;
            uint32 *pending_patch = NULL;
            uint32 *link_bucket = NULL;
            sint32 link_mode = 1;
            sint32 prim;

            for (prim = 0; prim < 14; ++prim)
            {
                uint8 control = desc->commands[prim];
                sint32 next0;
                sint32 next1;
                ANIM_VERTEX *a;
                ANIM_VERTEX *b;
                ANIM_VERTEX *c;
                ANIM_VERTEX *d;
                uint16 uv0;
                uint16 uv1;
                uint16 uv2;
                uint16 uv3;
                sint32 bucket_idx;
                uint32 *bucket;

                if (control == 255)
                    break;
                if ((control & 3u) != 0)
                {
                    uint8 kind = control & 3u;
                    uint32 tex[3];
                    uint16 shared;
                    uint32 color;
                    sint32 scr[4];
                    sint32 depth[4];
                    sint32 cross0;
                    sint32 cross1 = 0;
                    sint32 bucket_idx;
                    uint32 *bucket;
                    uint32 link_val;

                    if ((control & 0xF0u) == 0xF0)
                    {
                        ++idx0;
                        if ((control & 6u) != 2)
                            ++idx1;
                        ++colors;
                        link_mode = 1;
                        continue;
                    }
                    poly_material_words(2 * (control >> 4), &tex[0], &tex[1], &tex[2]);
                    shared = tex[0] ^ tex[1];
                    color = *colors & 0x00FFFFFFu | (kind == 3 ? 0x2C000000u : 0x24000000u);
                    ++colors;
                    if (kind == 1)
                    {
                        ANIM_VERTEX *a = &proj[cur_buffer][idx1];
                        ANIM_VERTEX *b = &proj[cur_buffer][idx1 + 1];
                        ANIM_VERTEX *c = &proj[prev_buffer][idx0];

                        scr[0] = a->alternate_scr;
                        scr[1] = b->alternate_scr;
                        scr[2] = c->alternate_scr;
                        depth[0] = a->depth;
                        depth[1] = b->depth;
                        depth[2] = c->depth;
                        ++idx1;
                        cross0 = NormalClip(scr[0], scr[1], scr[2]);
                        bucket_idx = AverageZ3(depth[0], depth[1], depth[2]) - 1;
                        if (cross0 < 0 || bucket_idx < 0 || bucket_idx >= 2000)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_idx >>= 3;
                        bucket = ordering + bucket_idx;
                        POLY_FT3 *poly = render_packet_at(packet_offset, (bucket_idx < 20 ? 4 : 1) * sizeof(*poly));
                        poly->rgbc.rgbc = color;
                        poly->p[0].xy.xy = scr[0];
                        poly->p[0].uv.uv = tex[0] ^ shared;
                        poly->p[0].attr = tex[0] >> 16;
                        poly->p[1].xy.xy = scr[1];
                        poly->p[1].uv.uv = tex[1] >> 16;
                        poly->p[1].attr = tex[1] ^ shared;
                        poly->p[2].xy.xy = scr[2];
                        poly->p[2].uv.uv = tex[2];
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_val = link_mode > 0 ? pending_head : *link_bucket;
                        if (link_val == 0)
                            pending_patch = &poly[bucket_idx < 20 ? 1 : 0].tag;
                        poly->tag = link_val;
                        setlen(poly, getlen(poly) | 7);
                        if (link_mode > 0)
                            AddPrim(&pending_head, poly);
                        else
                            AddPrim(link_bucket, poly);
                        packet_offset += sizeof(*poly);
                        if (bucket_idx < 20)
                        {
                            poly_subdiv_ft3(poly, poly + 1);
                            packet_offset += 3u * sizeof(*poly);
                        }
                        link_mode = 1;
                        continue;
                    }
                    if (kind == 2)
                    {
                        ANIM_VERTEX *a = &proj[cur_buffer][idx1];
                        ANIM_VERTEX *c = &proj[prev_buffer][idx0];
                        ANIM_VERTEX *d = &proj[prev_buffer][idx0 + 1];

                        scr[0] = a->alternate_scr;
                        scr[1] = c->alternate_scr;
                        scr[2] = d->alternate_scr;
                        depth[0] = a->depth;
                        depth[1] = c->depth;
                        depth[2] = d->depth;
                        ++idx0;
                        cross0 = NormalClip(scr[0], scr[2], scr[1]);
                        bucket_idx = AverageZ3(depth[0], depth[1], depth[2]) - 1;
                        if (cross0 < 0 || bucket_idx < 0 || bucket_idx >= 2000)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_idx >>= 3;
                        bucket = ordering + bucket_idx;
                        POLY_FT3 *poly = render_packet_at(packet_offset, (bucket_idx < 20 ? 4 : 1) * sizeof(*poly));
                        poly->rgbc.rgbc = color;
                        poly->p[0].xy.xy = scr[0];
                        poly->p[0].uv.uv = tex[0] ^ shared;
                        poly->p[0].attr = tex[0] >> 16;
                        poly->p[1].xy.xy = scr[2];
                        poly->p[1].uv.uv = tex[2] >> 16;
                        poly->p[1].attr = tex[0];
                        poly->p[2].xy.xy = scr[1];
                        poly->p[2].uv.uv = tex[2];
                        if (link_mode < 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        link_val = link_mode > 0 ? pending_head : *link_bucket;
                        if (link_val == 0)
                            pending_patch = &poly[bucket_idx < 20 ? 1 : 0].tag;
                        poly->tag = link_val;
                        setlen(poly, getlen(poly) | 7);
                        if (link_mode > 0)
                            AddPrim(&pending_head, poly);
                        else
                            AddPrim(link_bucket, poly);
                        packet_offset += sizeof(*poly);
                        if (bucket_idx < 20)
                        {
                            poly_subdiv_ft3(poly, poly + 1);
                            packet_offset += 3u * sizeof(*poly);
                        }
                        link_mode = 1;
                        continue;
                    }
                    {
                        ANIM_VERTEX *a = &proj[cur_buffer][idx1];
                        ANIM_VERTEX *b = &proj[cur_buffer][idx1 + 1];
                        ANIM_VERTEX *c = &proj[prev_buffer][idx0];
                        ANIM_VERTEX *d = &proj[prev_buffer][idx0 + 1];

                        scr[0] = a->alternate_scr;
                        scr[1] = b->alternate_scr;
                        scr[2] = c->alternate_scr;
                        scr[3] = d->alternate_scr;
                        depth[0] = a->depth;
                        depth[1] = b->depth;
                        depth[2] = c->depth;
                        depth[3] = d->depth;
                        ++idx0;
                        ++idx1;
                        if ((a->flags & b->flags & c->flags & d->flags & 15u) != 0)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_idx = (depth[0] + depth[1] + depth[2] + depth[3] & 0xFFFF) - 1;
                        if (bucket_idx < 0 || bucket_idx >= 0x7D00)
                        {
                            link_mode = 1;
                            continue;
                        }
                        bucket_idx >>= 7;
                        cross0 = NormalClip(scr[0], scr[1], scr[2]);
                        cross1 = NormalClip(scr[1], scr[2], scr[3]);
                        bucket = ordering + bucket_idx;
                        if (cross0 >= 0 && cross1 <= 0)
                        {
                            POLY_FT4 *poly = render_packet_at(packet_offset, (bucket_idx < 20 ? 16 : 1) * sizeof(*poly));
                            poly->rgbc.rgbc = color;
                            poly->p[0].xy.xy = scr[0];
                            poly->p[0].uv.uv = tex[0] ^ shared;
                            poly->p[0].attr = tex[0] >> 16;
                            poly->p[1].xy.xy = scr[1];
                            poly->p[1].uv.uv = tex[1] >> 16;
                            poly->p[1].attr = tex[1] ^ shared;
                            poly->p[2].xy.xy = scr[2];
                            poly->p[2].uv.uv = tex[2];
                            poly->p[3].xy.xy = scr[3];
                            poly->p[3].uv.uv = tex[2] >> 16;
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_val = link_mode > 0 ? pending_head : *link_bucket;
                            if (link_val == 0)
                                pending_patch = &poly[bucket_idx < 20 ? 7 : 0].tag;
                            poly->tag = link_val;
                            setlen(poly, getlen(poly) | 9);
                            if (link_mode > 0)
                                AddPrim(&pending_head, poly);
                            else
                                AddPrim(link_bucket, poly);
                            packet_offset += sizeof(*poly);
                            if (bucket_idx < 20)
                            {
                                POLY_FT4 *src = poly;
                                POLY_FT4 *next_poly = poly + 1;

                                next_poly = poly_subdiv_ft4(src, next_poly);
                                next_poly = poly_subdiv_ft4(src, next_poly);
                                next_poly = poly_subdiv_ft4(src + 1, next_poly);
                                next_poly = poly_subdiv_ft4(src + 2, next_poly);
                                next_poly = poly_subdiv_ft4(src + 3, next_poly);
                                packet_offset += 15u * sizeof(*poly);
                            }
                        }
                        else if (cross0 >= 0 || cross1 <= 0)
                        {
                            sint32 start = cross0 >= 0 ? 0 : 1;
                            sint32 middle = start + 1;
                            sint32 last = start + 2;

                            POLY_FT3 *poly = render_packet_at(packet_offset, (bucket_idx < 20 ? 4 : 1) * sizeof(*poly));
                            poly->rgbc.rgbc = color & 0x00FFFFFF | 0x24000000;
                            poly->p[0].xy.xy = scr[start];
                            poly->p[0].uv.uv = cross0 >= 0 ? tex[0] ^ shared : tex[1] >> 16;
                            poly->p[0].attr = tex[0] >> 16;
                            poly->p[1].xy.xy = scr[middle];
                            poly->p[1].uv.uv = cross0 >= 0 ? tex[1] >> 16 : tex[2];
                            poly->p[1].attr = cross0 >= 0 ? tex[1] ^ shared : tex[0];
                            poly->p[2].xy.xy = scr[last];
                            poly->p[2].uv.uv = cross0 >= 0 ? tex[2] : tex[2] >> 16;
                            if (link_mode < 0)
                            {
                                link_mode = 1;
                                continue;
                            }
                            link_val = link_mode > 0 ? pending_head : *link_bucket;
                            if (link_val == 0)
                                pending_patch = &poly[bucket_idx < 20 ? 1 : 0].tag;
                            poly->tag = link_val;
                            setlen(poly, getlen(poly) | 7);
                            if (link_mode > 0)
                                AddPrim(&pending_head, poly);
                            else
                                AddPrim(link_bucket, poly);
                            packet_offset += sizeof(*poly);
                            if (bucket_idx < 20)
                            {
                                poly_subdiv_ft3(poly, poly + 1);
                                packet_offset += 3u * sizeof(*poly);
                            }
                        }
                    }
                    link_mode = 1;
                    continue;
                }
                next0 = idx0 + ((control & 6u) != 1);
                next1 = idx1 + ((control & 6u) != 2);
                a = &proj[cur_buffer][idx1];
                b = &proj[cur_buffer][next1];
                c = &proj[prev_buffer][idx0];
                d = &proj[prev_buffer][next0];
                idx0 = next0;
                idx1 = next1;
                POLY_GT4 *poly = render_packet_at(packet_offset, sizeof(*poly));
                poly->p[0].rgbc.rgbc = a->color;
                poly->p[0].xy.xy = a->scr;
                poly->p[1].rgbc.rgbc = b->color;
                poly->p[1].xy.xy = b->scr;
                poly->p[2].rgbc.rgbc = c->color;
                poly->p[2].xy.xy = c->scr;
                poly->p[3].rgbc.rgbc = d->color;
                poly->p[3].xy.xy = d->scr;
                if ((a->flags & b->flags & c->flags & d->flags & 15u) != 0)
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
                if (((a->flags | c->flags) & 0xF0u) == 0)
                {
                    uv0 = a->flags & 0xFF00u | (generated_course ? 0x2Fu : 0u);
                    uv1 = b->flags & 0xFF00u | (generated_course ? 0u : 0x3Fu);
                    uv2 = c->flags & 0xFF00u | (generated_course ? 0x2Fu : 0u);
                    uv3 = d->flags & 0xFF00u | (generated_course ? 0u : 0x3Fu);
                }
                else if (((b->flags | d->flags) & 0xF0u) == 0)
                {
                    uv0 = a->flags & 0xFF00u | (generated_course ? 0u : 0x3Fu);
                    uv1 = b->flags & 0xFF00u | (generated_course ? 0x2Fu : 0u);
                    uv2 = c->flags & 0xFF00u | (generated_course ? 0u : 0x3Fu);
                    uv3 = d->flags & 0xFF00u | (generated_course ? 0x2Fu : 0u);
                }
                else
                {
                    uv0 = a->flags & 0xFF00u | (generated_course ? 0u : 0x10u);
                    uv1 = b->flags & 0xFF00u | (generated_course ? 0x27u : 0x3Fu);
                    uv2 = c->flags & 0xFF00u | (generated_course ? 0u : 0x10u);
                    uv3 = d->flags & 0xFF00u | (generated_course ? 0x27u : 0x3Fu);
                }
                poly->p[0].uv.uv = uv0;
                poly->p[0].attr = render_material(1)->clut;
                poly->p[1].uv.uv = uv1;
                poly->p[1].attr = render_material(1)->tpage;
                poly->p[2].uv.uv = uv2;
                poly->p[3].uv.uv = uv3;
                {
                    sint32 rejected_depth = a->depth + b->depth + c->depth + d->depth - 1;

                    if (rejected_depth < 0 || rejected_depth >= 0x7D00)
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
                    bucket_idx = rejected_depth >> 7;
                }
                bucket = ordering + bucket_idx;
                pending_bucket = bucket;
                poly->tag = *bucket;
                setlen(poly, getlen(poly) | 12);
                if (pending_head != 0)
                {
                    uint32 head = pending_head;

                    *pending_patch |= river_packet_link(poly);
                    pending_head = 0;
                    *bucket = head;
                }
                else
                {
                    AddPrim(bucket, poly);
                }
                link_bucket = bucket;
                link_mode = 0;
                packet_offset += sizeof(*poly);
                if (bucket_idx < 20)
                {
                    render_packet_at(packet_offset, 3u * sizeof(*poly));
                    poly_subdiv_gt4(poly, poly + 1);
                    packet_offset += 3u * sizeof(*poly);
                }
            }
            if (pending_head != 0 && pending_bucket != NULL)
            {
                uint32 old_bucket = *pending_bucket;

                *pending_bucket = pending_head;
                *pending_patch |= old_bucket;
            }
        }
        if (prev_buffer >= 0)
            --remaining_passes;
        prev_buffer = cur_buffer;
        cur_buffer ^= 1;
        desc = desc->next.seg;
    }
    render_packet_publish(packet_offset);
    return desc;
}

static const ROUTE_SEGMENT *river_render_surface(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007E850, "MAIN.EXE");
    return river_render_pass(mesh, pass_count, transform, 0);
}

static const ROUTE_SEGMENT *river_render_animated(const ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3])
{
    const ROUTE_SEGMENT *cur = mesh;
    uint32 remaining_passes = pass_count;
    uint32 *ordering;
    uint32 packet_offset;
    uint32 phase;
    const ROUTE_SEGMENT *partner;
    sint32 partner_x;
    sint32 partner_y;
    sint32 partner_z;
    sint32 tx;
    sint32 ty;
    sint32 tz;

    FUNCTION_MARKER(0x8007F490, "MAIN.EXE");
    if (remaining_passes == 0)
        return cur;
    phase = game_timing.ticks;
    partner = cur->prev.seg;
    ordering = render_order.route_ot;
    packet_offset = render_packet_offset();
    tx = transform[0];
    ty = transform[1];
    tz = transform[2];

    partner_x = tx + partner->origin[0];
    partner_y = ty + partner->origin[1];
    partner_z = tz + partner->origin[2];
    while (remaining_passes != 0)
    {
        const uint8 *control = cur->commands;
        uint32 pal_idx = cur->pal;
        const uint32 *pal = render_pal(pal_idx);
        uint32 pal_color = pal[1] ^ 0x10000000;
        const uint32 *colors = pal + 2;
        const ROUTE_VERTEX *cur_rec = &cur->vertices[cur->join[0]];
        const ROUTE_VERTEX *partner_rec = &partner->vertices[cur->join[1]];
        const ROUTE_VERTEX *prev_cur = cur_rec;
        const ROUTE_VERTEX *prev_partner = partner_rec;
        sint32 cur_x = tx + cur->origin[0];
        sint32 cur_y = ty + cur->origin[1];
        sint32 cur_z = tz + cur->origin[2];
        sint32 joined = 1;

        for (;;)
        {
            uint8 command = *control++;
            uint8 kind = command & 3u;
            sint32 scr[4];
            sint32 depth[4];
            sint32 avgZ;
            sint32 cross0;
            sint32 cross1;
            uint32 *bucket;
            uint32 tex[3];
            uint16 shared;
            uint32 prim_color;

            if (kind == 0)
            {
                ++cur_rec;
                ++partner_rec;
                if (joined == 0)
                    continue;
                prev_cur = cur_rec - 1;
                prev_partner = partner_rec - 1;
                joined = 0;
                continue;
            }

            if (joined == 0)
            {
                joined = 1;
                depth[0] = river_project_vertex(prev_cur, cur_x, cur_y, cur_z, &scr[0]);
                depth[1] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[1]);
                depth[2] = river_project_vertex(prev_partner, partner_x, partner_y, partner_z, &scr[2]);
                depth[3] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[3]);
                avgZ = AverageZ4(depth[0], depth[1], depth[2], depth[3]) - 1;
                {
                    const RENDER_MATERIAL *material = render_material(1);

                    POLY_FT4 *poly = render_packet_at(packet_offset, sizeof(*poly));
                    poly->rgbc.rgbc = pal_color;
                    poly->p[0].xy.xy = scr[0];
                    poly->p[0].uv.uv = river_tex_coord(cur_rec, phase) << 8;
                    poly->p[0].attr = material->clut;
                    poly->p[1].xy.xy = scr[1];
                    poly->p[1].uv.uv = river_tex_coord(prev_cur, phase) << 8 | 0x3F;
                    poly->p[1].attr = material->tpage;
                    poly->p[2].xy.xy = scr[2];
                    poly->p[2].uv.uv = river_tex_coord(partner_rec, phase) << 8;
                    poly->p[3].xy.xy = scr[3];
                    poly->p[3].uv.uv = river_tex_coord(prev_partner, phase) << 8 | 0x3F;
                    if (avgZ >= 0 && avgZ < 0x7D0)
                    {
                        bucket = ordering + (avgZ >> 3);
                        setlen(poly, 9);
                        AddPrim(bucket, poly);
                        packet_offset += sizeof(*poly);
                    }
                }
            }

            if (command == 0xFF)
                break;

            if (kind == 3)
            {
                depth[0] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[0]);
                ++cur_rec;
                depth[1] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[1]);
                depth[2] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[2]);
                ++partner_rec;
                depth[3] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[3]);
                cross0 = NormalClip(scr[0], scr[1], scr[2]);
                cross1 = NormalClip(scr[1], scr[2], scr[3]);
                avgZ = AverageZ4(depth[0], depth[1], depth[2], depth[3]) - 1;
                prim_color = *colors;
                poly_material_words(2 * (command >> 4), &tex[0], &tex[1], &tex[2]);
                shared = tex[0] ^ tex[1];
                POLY_FT4 *poly = render_packet_at(packet_offset, sizeof(*poly));
                poly->rgbc.rgbc = prim_color | 0x2C000000;
                poly->p[0].xy.xy = scr[0];
                poly->p[0].uv.uv = tex[0] ^ shared;
                poly->p[0].attr = tex[0] >> 16;
                poly->p[1].xy.xy = scr[1];
                poly->p[1].uv.uv = tex[1] >> 16;
                poly->p[1].attr = tex[1] ^ shared;
                poly->p[2].xy.xy = scr[2];
                poly->p[2].uv.uv = tex[2];
                poly->p[3].xy.xy = scr[3];
                poly->p[3].uv.uv = tex[2] >> 16;
                if ((cross1 <= 0 || cross0 >= 0) && avgZ >= 0 && avgZ < 0x7D0)
                {
                    bucket = ordering + (avgZ >> 3);
                    setlen(poly, 9);
                    AddPrim(bucket, poly);
                    packet_offset += sizeof(*poly);
                }
                ++colors;
            }
            else if (kind == 1)
            {
                depth[0] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[0]);
                ++cur_rec;
                depth[1] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[1]);
                depth[2] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[2]);
                avgZ = AverageZ3(depth[0], depth[1], depth[2]) - 1;
                prim_color = *colors;
                poly_material_words(2 * (command >> 4), &tex[0], &tex[1], &tex[2]);
                shared = tex[0] ^ tex[1];
                POLY_FT3 *poly = render_packet_at(packet_offset, sizeof(*poly));
                poly->rgbc.rgbc = prim_color | 0x24000000;
                poly->p[0].xy.xy = scr[0];
                poly->p[0].uv.uv = tex[0] ^ shared;
                poly->p[0].attr = tex[0] >> 16;
                poly->p[1].xy.xy = scr[1];
                poly->p[1].uv.uv = tex[1] >> 16;
                poly->p[1].attr = tex[1] ^ shared;
                poly->p[2].xy.xy = scr[2];
                poly->p[2].uv.uv = tex[2];
                if (NormalClip(scr[0], scr[1], scr[2]) >= 0 && avgZ >= 0 && avgZ < 0x7D0)
                {
                    bucket = ordering + (avgZ >> 3);
                    setlen(poly, 7);
                    AddPrim(bucket, poly);
                    packet_offset += sizeof(*poly);
                }
                ++colors;
            }
            else
            {
                depth[0] = river_project_vertex(cur_rec, cur_x, cur_y, cur_z, &scr[0]);
                depth[1] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[1]);
                ++partner_rec;
                depth[2] = river_project_vertex(partner_rec, partner_x, partner_y, partner_z, &scr[2]);
                avgZ = AverageZ3(depth[0], depth[1], depth[2]) - 1;
                prim_color = *colors;
                poly_material_words(2 * (command >> 4), &tex[0], &tex[1], &tex[2]);
                shared = tex[0] ^ tex[1];
                POLY_FT3 *poly = render_packet_at(packet_offset, sizeof(*poly));
                poly->rgbc.rgbc = prim_color | 0x24000000;
                poly->p[0].xy.xy = scr[0];
                poly->p[0].uv.uv = tex[0] ^ shared;
                poly->p[0].attr = tex[0] >> 16;
                poly->p[1].xy.xy = scr[2];
                poly->p[1].uv.uv = tex[2] >> 16;
                poly->p[1].attr = tex[0];
                poly->p[2].xy.xy = scr[1];
                poly->p[2].uv.uv = tex[2];
                if (NormalClip(scr[0], scr[1], scr[2]) <= 0 && avgZ >= 0 && avgZ < 0x7D0)
                {
                    bucket = ordering + (avgZ >> 3);
                    setlen(poly, 7);
                    AddPrim(bucket, poly);
                    packet_offset += sizeof(*poly);
                }
                ++colors;
            }
        }
        partner = cur;
        partner_x = cur_x;
        partner_y = cur_y;
        partner_z = cur_z;
        cur = cur->next.seg;
        --remaining_passes;
    }
    render_packet_publish(packet_offset);
    return cur;
}

static const ROUTE_SEGMENT *river_render_surface_fractal(const ROUTE_SEGMENT *object, sint32 enabled, const sint32 transform[3])
{
    FUNCTION_MARKER(0x8007FE50, "MAIN.EXE");
    return river_render_pass(object, enabled, transform, 1);
}

void river_render(const ROUTE_SEGMENT *sequence, const RACE_VIEW *state_base, sint32 state_idx)
{
    const RACE_VIEW *state = state_base;
    const ROUTE_SEGMENT *sel = sequence;
    const ROUTE_SEGMENT *supplemental = NULL;
    sint32 available = -1;
    sint32 frame;

    FUNCTION_MARKER(0x80018C60, "MAIN.EXE");
    render_order.route_ot = render_route_ot(render_frame(camera_for_view(state), state_idx));
    render_publish_pal();
    SetBackColor(255, 255, 255);
    PushMatrix();
    render_scale_proj_transform(2);
    if (camera_for_view(state)->dir != 1)
    {
        const ROUTE_SEGMENT *animation;
        sint32 width0;
        sint32 width1;
        sint32 height;

        frame = sequence->next.idx - 1u;
        if (frame < 0)
            frame = sequence->prev.idx + 1u;
        width0 = (sint16)camera_for_view(state)->visible[0];
        width1 = (sint16)camera_for_view(state)->visible[1];
        frame = frame - width0;
        frame = frame - width1;
        frame = frame + 2u;
        if (frame < 0)
            frame = frame + route_resources.count;
        else
        {
            sint32 frame_count = route_resources.count;

            if (frame_count < frame)
                frame = frame - frame_count;
        }

        height = (sint16)camera_for_view(state)->visible[1];
        if ((uint32)frame >= route_resources.count)
            abort();
        animation = route_resources.segs[frame];
        sel = river_render_animated(animation, height, camera_for_view(state)->pos);
        sel = sel->prev.seg;
        width0 = (sint16)camera_for_view(state)->visible[0];
        available = 25u - width0;
        sel = sel->prev.seg;
        if (available > 0)
        {
            sint32 step;

            supplemental = sel->prev.seg;
            for (step = 0; step < available; ++step)
            {
                supplemental = supplemental->prev.seg;
            }
        }
    }
    {
        uint32 mode = race_cfg.course;
        sint16 width = camera_for_view(state)->visible[0];

        if (mode == 6)
            sel = river_render_surface_fractal(sel, width, camera_for_view(state)->pos);
        else
            sel = river_render_surface(sel, width, camera_for_view(state)->pos);
    }
    if (camera_for_view(state)->dir == 1)
    {
        const ROUTE_SEGMENT *next;
        sint32 height;
        sint32 width;

        supplemental = sel;
        next = sel->prev.seg;
        height = (sint16)camera_for_view(state)->visible[1];
        next = next->prev.seg;
        width = (sint16)camera_for_view(state)->visible[0];
        available = 25u - width;
        river_render_animated(next, height, camera_for_view(state)->pos);
    }
    render_scale_proj_transform(0);
    PopMatrix();
    sint16 limit = camera_for_view(state)->visible[1];

    if (limit < available)
        available = limit;
    if (available > 0)
        render_generate_entries(camera_for_view(state)->pos, supplemental, available);
    const RENDER_BILLBOARD *entries = render_billboards.entries;
    sint32 entry_count = render_billboards.count;

    render_billboard(render_order.route_ot, entries, entry_count, 0);
}

// Transient event billboard owner formerly allocated as800 bytes
static RENDER_BILLBOARD river_billboards[100];

enum
{
    RIVER_COURSE_COUNT = 24,
    RIVER_BORDER_COUNT = 12 * (RIVER_COURSE_COUNT - 1) + 3
};

static RIVER_REC river_recs[RIVER_COURSE_COUNT];
static RENDER_BILLBOARD river_border[RIVER_BORDER_COUNT];
RIVER_LIST river_course, river_markers;
sint32 river_selection;

typedef struct
{
    uint32 ot[2][251];
} RIVER_FRAME;

static RIVER_FRAME river_frames[2][2];
static uint32 *river_ot[2];
static uint32 river_outside;

void river_select_frame(sint32 buffer, sint32 view)
{
    const BOAT *boat = boat_players[0];
    RIVER_FRAME *frame;

    FUNCTION_MARKER(0x800389F4, "MAIN.EXE");
    if ((uint32)buffer >= 2 || (uint32)view >= 2)
        abort();
    frame = &river_frames[view][buffer];
    river_outside = (uint32)boat->pos.x != (uint32)(sint16)boat->pos.x;
    if ((uint32)boat->pos.y != (uint32)(sint16)boat->pos.y || boat->pos.z < -30000 || boat->pos.z > 34000)
        river_outside = 1;
    horizon_select_showcase(buffer, view);
    river_ot[0] = frame->ot[0];
    river_ot[1] = frame->ot[1];
}

static void river_draw_model(uint32 *ot, uint32 bucket_count, const MESH_MODEL *model, uint32 color)
{
    uint32 remaining;
    const MESH_FACE *face;
    uint32 packet_offset;
    POLY_FT4 *poly;
    sint32 prev_cross = -1;
    sint32 prev_depth = 0;

    FUNCTION_MARKER(0x8003A100, "MAIN.EXE");
    if (model == NULL)
        return;
    remaining = model->sets[3].count - 1;
    face = model->sets[3].faces;
    if (remaining == UINT32_MAX)
        return;
    packet_offset = render_packet_offset();
    do
    {
        SVECTOR vertices[4];
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross = -1;
        sint32 vertex;

        for (vertex = 0; vertex < 4; ++vertex)
            vertices[vertex] = face->v[vertex];
        gte_project3_full_depth(vertices, scr, depth, &flags);
        if (prev_cross > 0 && (uint32)(prev_depth >> 3) < bucket_count)
        {
            uint32 *bucket = ot + (prev_depth >> 3);

            poly = render_packet_at(packet_offset, sizeof(*poly));
            setlen(poly, 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        if (flags >= 0)
            cross = NormalClip(scr[0], scr[1], scr[2]);
        prev_cross = cross;
        if (cross > 0)
        {
            uint32 lit;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            depth[3] = gte_project_full_depth(&vertices[3], &scr[3], &flags);
            poly->p[3].xy.xy = scr[3];
            lit = gte_normal_color_col_depth(&vertices[3], color);
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = face->v[0].pad;
            poly->p[0].attr = face->clut;
            poly->p[1].uv.uv = face->v[1].pad;
            poly->p[1].attr = face->tpage;
            poly->p[2].uv.uv = face->v[2].pad;
            poly->p[3].uv.uv = face->v[3].pad;
            poly->rgbc.rgbc = lit;
            prev_depth = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        ++face;
    } while (--remaining != UINT32_MAX);
    if (prev_cross > 0 && (uint32)(prev_depth >> 3) < bucket_count)
    {
        uint32 *bucket = ot + (prev_depth >> 3);

        poly = render_packet_at(packet_offset, sizeof(*poly));
        setlen(poly, 9);
        AddPrim(bucket, poly);
        packet_offset += sizeof(*poly);
    }
    render_packet_publish(packet_offset);
}

static sint32 river_place_model(const CAMERA *camera, const BOAT *boat, const MESH_MODEL *model, sint16 x, sint16 z, sint16 angle, const uint32 colors[2], uint32 *const ot[2])
{
    MATRIX matrix = {{{0}}};
    PsxGteSnapshot controls, proj;
    SVECTOR origin = {0};
    XY scr;
    sint32 flags, radius;
    sint16 depth;
    sint32 row;

    FUNCTION_MARKER(0x80039E10, "MAIN.EXE");
    matrix.m[0][0] = ONE;
    matrix.m[1][1] = ONE;
    matrix.m[2][2] = ONE;
    matrix.t[0] = x;
    matrix.t[1] = track_horizon->ground;
    matrix.t[2] = z;
    RotMatrixZ(1024 - angle, &matrix);
    psx_gte_snapshot(&controls);
    camera_bias_matrix(camera, &matrix, 0);
    // Original bounding projection uses its own depth cue
    xport_gte_write_control(27, 65);
    xport_gte_write_control(28, 0);
    gte_project_full_depth(&origin, (sint32 *)&scr.xy, &flags);
    psx_gte_snapshot(&proj);
    xport_gte_write_control(27, controls.dqa);
    xport_gte_write_control(28, controls.dqb);
    radius = proj.ir0;
    depth = proj.ir[2];
    if (scr.x + radius < 0 || scr.x - radius > disp_state.view_width || scr.y + radius < 0 || scr.y - radius > disp_state.view_height)
        return 0;
    if (depth + 1035 < 0 || depth - 1035 >= 8001)
        return 0;
    camera_config_lighting(boat, &matrix);
    camera_bias_matrix(camera, &matrix, 0);
    render_order.bias = 0;
    river_draw_model(ot[0], 250, model, colors[0]);
    for (row = 0; row < 3; ++row)
    {
        matrix.m[row][0] = -matrix.m[row][0];
        matrix.m[row][1] = -matrix.m[row][1];
    }
    matrix.t[1] = matrix.t[1] + 10u;
    camera_config_lighting(boat, &matrix);
    camera_bias_matrix(camera, &matrix, 0);
    render_order.bias = 0;
    river_draw_model(ot[1], 250, model, colors[1]);
    return 1;
}

static void river_hsv_to_rgb(uint8 output[3], sint32 hue, sint32 sat, sint32 val)
{
    sint32 sector;
    sint32 fraction;
    sint32 low;
    sint32 falling;
    sint32 rising;

    if (hue == 1536)
        hue = 0;
    sector = hue / 256;
    fraction = hue % 256;
    low = val * (256 - sat) / 256;
    falling = val * (256 - sat * fraction / 256) / 256;
    rising = val * (256 - sat * (256 - fraction) / 256) / 256;
    val = clamp(val, 0, 255);
    low = clamp(low, 0, 255);
    falling = clamp(falling, 0, 255);
    rising = clamp(rising, 0, 255);
    switch (sector)
    {
        case 0:
            output[0] = val;
            output[1] = rising;
            output[2] = low;
            break;
        case 1:
            output[0] = falling;
            output[1] = val;
            output[2] = low;
            break;
        case 2:
            output[0] = low;
            output[1] = val;
            output[2] = rising;
            break;
        case 3:
            output[0] = low;
            output[1] = falling;
            output[2] = val;
            break;
        case 4:
            output[0] = rising;
            output[1] = low;
            output[2] = val;
            break;
        default:
            output[0] = val;
            output[1] = low;
            output[2] = falling;
            break;
    }
}

void river_render_showcase(const RACE_VIEW *state, uint32 *overlay_ot, sint32 buffer_idx)
{
    RIVER_REC *route = river_course.recs;
    sint32 route_count = river_course.count;
    RENDER_BILLBOARD *collisions = river_billboards;
    sint32 col_count = 0;
    sint32 rendered = 0;
    sint32 idx;

    FUNCTION_MARKER(0x80039728, "MAIN.EXE");
    ClearOTagR(river_ot[0], 251);
    ClearOTagR(river_ot[1], 251);
    {
        uint32 *chain = horizon_ot_entry(12);
        for (uint32 side = 0; side < 2; ++side)
        {
            uint32 *ot = river_ot[side];
            ot[0] = *chain;
            AddPrims(chain, &ot[250], ot);
        }
    }
    river_ot[0] = render_route_ot(render_frame(camera_for_view(state), buffer_idx));
    if (boat_player_menu(&race_views[0])->mode == 5 || boat_player_menu(&race_views[0])->mode == 1)
    {
        horizon_add_backdrop(overlay_ot);
    }
    if (river_outside != 0)
    {
        return;
    }

    {
        PsxGteSnapshot gte;
        RIVER_REC *recs = river_markers.recs;
        sint32 rec_count = river_markers.count;
        sint32 sel = river_selection;
        static const uint32 model_colors[4][2] = {{0x2CE820E8, 0x2C601060}, {0x2C2020FF, 0x2C101060}, {0x2C20E8E8, 0x2C106060}, {0x2C303030, 0x2C201010}};
        uint32 *ot[2] = {river_ot[0], river_ot[1]};

        SetFarColor(160, 208, 224);
        psx_gte_snapshot(&gte);
        SetFogNear(0, gte.h);
        if (gte.h == 0)
            abort();
        xport_gte_write_control(27, -1536000 / gte.h);
        xport_gte_write_control(28, 0x01400000);
        for (idx = 0; idx < rec_count; ++idx)
        {
            const RIVER_REC *rec = &recs[idx];
            uint32 type = sel == -1 || sel == idx ? rec->angle >> 12 : 3u;

            if (idx >= 4 || type >= 4)
                abort();
            if (river_place_model(camera_for_view(state), boat_player(state), &mesh_event_models[idx], rec->pos[0], rec->pos[2], rec->angle, model_colors[type], ot))
                rendered += 2;
        }
        for (idx = 0; idx < route_count; ++idx)
        {
            const RIVER_REC *seg = &route[idx];

            if (seg->angle >> 12 != 0)
            {
                uint32 colors[2];
                uint8 rgb[3];
                sint32 model = seg->kind == 3 && sel != -1 ? sel : 3;

                if (idx == route_count - 1)
                {
                    colors[0] = 0x2CC0C0C0;
                    colors[1] = 0x2C605050;
                }
                else
                {
                    sint16 hue = (sint32)((uint32)(route_count - idx) << 9) / route_count;
                    river_hsv_to_rgb(rgb, hue, 228, 255);
                    colors[0] = 0x2C000000u | rgb[0] | rgb[1] << 8 | rgb[2] << 16;
                    river_hsv_to_rgb(rgb, hue, 192, 96);
                    colors[1] = 0x2C000000u | rgb[0] | rgb[1] << 8 | rgb[2] << 16;
                }
                if (model < 0 || model >= 4)
                    abort();
                if (river_place_model(camera_for_view(state), boat_player(state), &mesh_event_models[model], seg->pos[0], seg->pos[2], seg->angle, colors, ot))
                {
                    sint32 point;
                    if (seg->kind != 0 && col_count < 100 && sel != -1)
                    {
                        RENDER_BILLBOARD *entry = &collisions[col_count++];
                        entry->x = seg->item_pos[0] + camera_for_view(state)->pos[0];
                        entry->y = seg->item_pos[1] + camera_for_view(state)->pos[1];
                        entry->z = seg->item_pos[2] + camera_for_view(state)->pos[2];
                        entry->type = seg->kind;
                    }
                    for (point = 0; point < seg->border_count && col_count < 100; ++point)
                    {
                        const RENDER_BILLBOARD *src;
                        sint32 idx = seg->border_start + point;
                        if (idx < 0 || idx >= RIVER_BORDER_COUNT)
                            abort();
                        src = &river_border[idx];
                        RENDER_BILLBOARD *entry = &collisions[col_count++];
                        entry->x = src->x + camera_for_view(state)->pos[0];
                        entry->y = src->y + camera_for_view(state)->pos[1];
                        entry->z = src->z + camera_for_view(state)->pos[2];
                        entry->type = src->type;
                    }
                    rendered += 2;
                }
            }
            if ((sint16)rendered >= 14)
                break;
        }
        MATRIX rotation = camera_for_view(state)->rotation;
        MATRIX translation = {{{0}}};
        SetRotMatrix(&rotation);
        SetTransMatrix(&translation);
        render_billboard(ot[0], collisions, col_count, 0);
        SetRotMatrix(&rotation);
        SetTransMatrix(&translation);
        render_billboard(ot[1], collisions, col_count, 1);
    }
}

sint32 river_nearby_event_check(const BOAT *boat)
{
    sint32 relative;

    FUNCTION_MARKER(0x80039CCC, "MAIN.EXE");
    if (river_selection == -1)
        return 0;
    for (relative = -1; relative < 2; ++relative)
    {
        sint32 idx = relative + river_course.idx;
        RIVER_REC *entry;
        sint32 dx;
        sint32 dz;
        sint32 result;
        VECTOR delta;
        VECTOR sq;

        if (idx < 0 || idx >= river_course.count)
            continue;
        entry = &river_course.recs[idx];
        dx = (sint16)(boat->contacts.points[0].pos.x - entry->item_pos[0]);
        dz = (sint16)(boat->contacts.points[0].pos.z - entry->item_pos[2]);
        delta.vx = dx;
        delta.vy = 0;
        delta.vz = dz;
        Square0(&delta, &sq);
        if (sq.vx + sq.vy + sq.vz >= 20000)
            continue;
        result = entry->kind;
        entry->kind = 0;
        if (result == 3)
            sound_queue_command(sound_for_context(&race_views[0]), boat_player_menu(&race_views[0])->pickups[2].count + 10, 7);
        return result;
    }
    return 0;
}

static sint32 terrain_collide_capsule(BOAT *boat, const RIVER_REC *seg)
{
    MATRIX *pose = &boat->pose;
    BOAT_CAPSULE *capsule = &boat->capsule;
    VECTOR *start = &capsule->world[0];
    VECTOR *end = &capsule->world[1];
    VECTOR dir;
    SVECTOR n;
    SVECTOR points[2] = {{0}, {0}};
    sint32 len;
    sint32 idx;
    sint32 s;
    sint32 c;

    FUNCTION_MARKER(0x8003A310, "MAIN.EXE");
    SetRotMatrix(pose);
    SetTransMatrix(pose);
    gte_transform_raw(&capsule->local[0], start);
    gte_transform_raw(&capsule->local[1], end);
    start->vy = 0;
    end->vy = 0;
    dir.vx = end->vx - start->vx;
    dir.vy = end->vy - start->vy;
    dir.vz = end->vz - start->vz;
    VectorNormalS(&dir, &n);
    len = (sint16)trunc12(mul32(n.vx, dir.vx) + mul32(n.vy, dir.vy) + mul32(n.vz, dir.vz));
    c = sra(mul32(470, rcos(1024 - seg->angle)), 12u);
    s = sra(mul32(470, rsin(1024 - seg->angle)), 12u);
    points[0].vx = seg->pos[0] - c;
    points[0].vz = seg->pos[2] + s;
    points[1].vx = seg->pos[0] + c;
    points[1].vz = seg->pos[2] - s;
    for (idx = 0; idx < 2; ++idx)
    {
        VECTOR relative;
        VECTOR nearest;
        VECTOR offset;
        VECTOR sq;
        SVECTOR contact_n;
        sint32 proj;
        sint32 distance_sq;
        sint32 radius = capsule->radius;
        sint32 limit = radius + 80;

        const SVECTOR *point = points + idx;
        relative.vx = point->vx - start->vx;
        relative.vy = point->vy - start->vy;
        relative.vz = point->vz - start->vz;
        proj = sra(mul32(n.vx, relative.vx) + mul32(n.vy, relative.vy) + mul32(n.vz, relative.vz), 12u);
        if (proj < 0)
            proj = 0;
        else if (proj > len)
            proj = len;
        nearest.vx = sra(mul32(n.vx, proj), 12u) + start->vx;
        nearest.vy = sra(mul32(n.vy, proj), 12u) + start->vy;
        nearest.vz = sra(mul32(n.vz, proj), 12u) + start->vz;
        offset.vx = point->vx - nearest.vx;
        offset.vy = point->vy - nearest.vy;
        offset.vz = point->vz - nearest.vz;
        Square0(&offset, &sq);
        distance_sq = sq.vx + sq.vy + sq.vz;
        if (distance_sq < mul32(limit, limit))
        {
            sint32 penetration = radius - (SquareRoot0(distance_sq) - 80);

            nearest.vx = (point->vx + nearest.vx) / 2;
            nearest.vy = (point->vy + nearest.vy) / 2;
            nearest.vz = (point->vz + nearest.vz) / 2;
            VectorNormalS(&offset, &contact_n);
            boat_resolve_col(boat, NULL, &contact_n, &nearest, penetration);
        }
    }
    return 0;
}

static sint32 river_cross_marker(const BOAT *boat, RIVER_LIST *list)
{
    const RIVER_REC *seg = &list->recs[list->idx];
    sint16 type = seg->angle >> 12;
    sint32 angle;
    sint32 s;
    sint32 c;
    sint32 x;
    sint32 z;
    sint32 proj;
    sint32 prev_proj;
    sint32 result;

    FUNCTION_MARKER(0x8003A7E0, "MAIN.EXE");
    if (type == 3)
        return type;
    angle = 1024 - seg->angle;
    s = rsin(angle);
    c = rcos(angle);
    x = seg->pos[0] - boat->pos.x;
    z = seg->pos[2] - boat->pos.z;
    result = mul32(c, z);
    proj = sra(mul32(s, x) + result, 12u);
    prev_proj = list->prev_proj;
    list->prev_proj = proj;
    if (proj <= 0 && prev_proj >= 0)
    {
        sint32 dx = x - sra(mul32(proj, s), 12u);
        sint32 dz = z - sra(mul32(proj, c), 12u);
        sint32 distance = SquareRoot0(mul32(dx, dx) + mul32(dz, dz));

        result = distance < 471;
        if (result)
        {
            result = -1;
            if (river_selection == -1)
            {
                river_selection = type;
                sound_queue_command(sound_for_context(&race_views[0]), 1, 1);
                result_state.column = race_grid_columns[river_selection];
                result = result_state.column;
            }
        }
    }
    else if (proj <= 0)
        result = prev_proj;
    return result;
}

static sint32 river_dist_sq(const BOAT *boat, const RIVER_REC *seg)
{
    VECTOR delta = {(sint16)((uint16)boat->contacts.points[0].pos.x - (uint16)seg->pos[0]), 0, (sint16)((uint16)boat->contacts.points[0].pos.z - (uint16)seg->pos[2]), 0};
    VECTOR sq;

    Square0(&delta, &sq);
    return sq.vx + sq.vy + sq.vz;
}

static sint32 river_find_nearest(const RACE_VIEW *state, RIVER_LIST *list)
{
    BOAT *boat;
    sint32 cur;
    sint32 best;
    sint32 relative;
    sint32 radius;
    sint32 result;

    FUNCTION_MARKER(0x8003AAE4, "MAIN.EXE");
    if (river_outside != 0)
        return river_outside;
    boat = boat_player(state);
    cur = list->idx;
    {
        const RIVER_REC *seg = &list->recs[cur];
        best = river_dist_sq(boat, seg);
    }
    for (relative = -1; relative < 2; relative += 2)
    {
        sint32 candidate = cur + relative;
        if (candidate >= 0 && candidate < list->count)
        {
            const RIVER_REC *seg = &list->recs[candidate];
            sint32 distance = river_dist_sq(boat, seg);
            if (distance < best)
            {
                best = distance;
                cur = candidate;
                list->idx = cur;
                break;
            }
        }
    }
    list->dist_sq = best;
    radius = boat->capsule.bound_radius + 1020;
    result = mul32(radius, radius);
    if (best < result)
    {
        const RIVER_REC *seg = &list->recs[cur];
        terrain_collide_capsule(boat, seg);
        result = -1;
        if (river_selection == -1)
            return river_cross_marker(boat, list);
    }
    return result;
}

sint32 boat_update_route_segs(const RACE_VIEW *state)
{
    FUNCTION_MARKER(0x8003AAA4, "MAIN.EXE");
    river_find_nearest(state, &river_markers);
    return river_find_nearest(state, &river_course);
}

void terrain_init_contact(ROUTE_CONTACT *point)
{
    FUNCTION_MARKER(0x8003AD04, "MAIN.EXE");
    point->seg = &route_showcase_seg;
    point->entry = 0;
    point->right = 0;
    point->left = 0;
    point->pos = (VEC3){{0, 0, 0}};
    point->height = 0;
    point->penetration = 0;
}

sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground)
{
    VECTOR world;
    VECTOR up = {0, ONE, 0, 0};
    sint32 flags;
    sint32 delta[3];
    sint32 shift;

    FUNCTION_MARKER(0x8003AD44, "MAIN.EXE");
    RotTrans(&point->sample, &world, &flags);
    delta[0] = world.vx - point->pos.x;
    delta[1] = world.vy - point->pos.y;
    delta[2] = world.vz - point->pos.z;
    shift = 18 - Lzc(4096);
    if (shift > 0)
    {
        up.vx >>= shift;
        up.vy >>= shift;
        up.vz >>= shift;
    }
    VectorNormalS(&up, &point->n);
    point->surface = 0;
    point->height = ground - point->pos.y;
    for (sint32 idx = 0; idx < 3; ++idx)
    {
        point->pos.v[idx] += delta[idx];
        point->delta.v[idx] = delta[idx];
        point->diagonal.v[idx] = diagonal[idx];
    }
    point->weight = 48;
    return 0;
}

static sint32 river_init_course(void)
{
    sint32 count = RIVER_COURSE_COUNT;
    RIVER_REC *recs = river_recs;
    RENDER_BILLBOARD *border = river_border;
    sint16 checkpoints[7];
    sint16 angle = -512;
    sint16 border_idx = 0;
    sint16 checkpoint = 0;
    sint16 x = route_showcase_seg.origin[0];
    sint16 z = route_showcase_seg.origin[2] + 10096u;
    sint16 y = track_horizon->ground;
    sint32 lanes = profile_selection.slot == 1 ? 5 : profile_selection.slot == 0 ? 6 : 4;
    sint32 idx;

    FUNCTION_MARKER(0x8003AF00, "MAIN.EXE");
    river_course.recs = recs;
    river_course.count = count;
    river_course.idx = 0;
    recs[0].border_start = 0;
    recs[0].border_count = 0;
    checkpoints[0] = (count - 1) / (lanes + 1);
    for (idx = 1; idx < lanes; ++idx)
        checkpoints[idx] = (idx + 1) * checkpoints[0] + (random_next() & 1);
    checkpoints[lanes] = count - 2u;
    for (idx = 0; idx < count; ++idx)
    {
        RIVER_REC *rec = &recs[idx];
        sint32 curve = angle > 0 ? rsin(4 * angle) / 4 : 0;
        sint32 dir = angle > 0 ? rcos(4 * angle) >> 3 : 0;
        sint32 item_x;

        rec->pos[0] = x + curve;
        rec->pos[1] = y;
        rec->pos[2] = z + 8 * angle;
        rec->angle = 1024 - dir & 0x0FFF | 0x3000;
        item_x = random_next() % 470;
        item_x = abs(item_x);
        item_x = x + item_x - 235;
        if (angle > 0)
            item_x += rsin(4 * angle) / 4;
        rec->item_pos[0] = item_x;
        rec->item_pos[1] = y;
        rec->item_pos[2] = rec->pos[2];
        if ((sint16)idx == checkpoints[checkpoint])
        {
            rec->kind = 3;
            checkpoint++;
            if (checkpoint > lanes)
                checkpoint = lanes;
        }
        else if ((sint16)idx == count - 1)
            rec->kind = 0;
        else
        {
            sint32 kind = random_next() & 3;
            if (kind < 2)
                rec->kind = kind == 0 ? 4 : 5;
            else
            {
                rec->kind = 2;
                rec->item_pos[1] = y - 8;
            }
        }
        if ((sint16)idx < count - 1)
        {
            sint32 point;

            (rec + 1)->border_start = border_idx;
            (rec + 1)->border_count = 12;
            for (point = 0; point < 12; ++point)
            {
                angle = angle + 16;
                border->x = x + (point & 1 ? 470 : -470) + (angle > 0 ? rsin(4 * angle) / 4 : 0);
                border->y = y;
                border->z = z + 8 * angle;
                border->type = 1;
                ++border;
                border_idx++;
            }
        }
        angle = angle + 16;
        if ((sint16)idx == count - 2)
        {
            sint32 point;

            (rec + 1)->border_count = (rec + 1)->border_count + 3u;
            for (point = 1; point < 4; ++point)
            {
                border->x = x + rsin(4 * angle) / 4 - 470 + 235 * point;
                border->y = y;
                border->z = z + 8 * angle;
                border->type = 1;
                ++border;
            }
        }
    }
    return count > 0 ? 0 : count;
}

sint32 river_init_showcase(void)
{
    uint32 players = race_cfg.players;

    FUNCTION_MARKER(0x8003B508, "MAIN.EXE");
    river_selection = -1;
    track_control = NULL;
    track_clear_start();
    result_state.hud_vals[2] = UINT16_MAX;
    result_state.hud_vals[1] = UINT16_MAX;
    track_clear_sections();
    result_state.hud_vals[0] = 0;
    route_init_showcase();
    river_outside = 0;
    track_showcase_horizon(race_cfg.variant);
    track_showcase_reverb();
    track_showcase_race();
    render_select_lighting(RENDER_LIGHT_SHOWCASE);
    horizon_init_showcase(players, race_cfg.variant);
    if (!gpu_register_packet_range(river_frames, sizeof(river_frames)))
        abort();
    river_init_course();
    return race_init_markers();
}

static void river_copy_rgb(CVECTOR *dst, const uint8 src[3])
{
    dst->r = src[0];
    dst->g = src[1];
    dst->b = src[2];
}

void river_init_light_fractal(const RACE_CFG *cfg, sint32 count)
{
    uint8 ambient[3] = {50, 30, 40};
    uint8 light_colors[2][3];
    uint8 hsv_rgb[3];
    RGBC light_a[14];
    RGBC light_b[14];
    VECTOR light_vectors[2];
    MATRIX rotation = {{{ONE, 0, 0}, {0, ONE, 0}, {0, 0, ONE}}, {0, 0, 0}};
    SVECTOR src = {0, 0, ONE, 0};
    VECTOR t;
    ROUTE_SEGMENT *prev_rec = route_resources.segs[0];
    RENDER_LIGHTING *lighting = render_lighting(0);
    uint32 *colors;
    uint32 *output;
    sint32 random;
    sint32 course;
    sint32 idx;

    FUNCTION_MARKER(0x8003F7D4, "MAIN.EXE");
    random = random_next() & 3;
    if (random < 2)
    {
        sint32 hue_offset = random_next() % 128;
        sint32 secondary_sat = (hue_offset / 2 + 64) / 2;

        river_hsv_to_rgb(hsv_rgb, hue_offset + 672, hue_offset / 2 + 64, 128);
        river_copy_rgb(&track_colors(0, 6)->colors[0].rgb, hsv_rgb);
        river_hsv_to_rgb(light_colors[1], hue_offset + 672, secondary_sat, 64);
        river_copy_rgb(&track_colors(0, 6)->colors[1].rgb, hsv_rgb);
        secondary_sat = secondary_sat * 2 + 128;
        river_hsv_to_rgb(hsv_rgb, hue_offset + 800, secondary_sat, 96);
        river_copy_rgb(&track_colors(0, 6)->colors[2].rgb, hsv_rgb);
        river_hsv_to_rgb(ambient, hue_offset + 800, 128, 48);
        light_colors[0][0] = 0xE6;
        light_colors[0][1] = 0xB9;
        light_colors[0][2] = 0x8C;
        course = 1;
    }
    else if (random == 2)
    {
        sint32 hue_offset = random_next() % 128;

        river_hsv_to_rgb(hsv_rgb, hue_offset + 162, 135, 255);
        river_copy_rgb(&track_colors(0, 6)->colors[0].rgb, hsv_rgb);
        river_hsv_to_rgb(light_colors[0], hue_offset + 162, 135, 127);
        river_copy_rgb(&track_colors(0, 6)->colors[1].rgb, hsv_rgb);
        river_hsv_to_rgb(hsv_rgb, 2 * (hue_offset + 82) + 1100, 150, 150);
        river_copy_rgb(&track_colors(0, 6)->colors[2].rgb, hsv_rgb);
        river_hsv_to_rgb(ambient, 2 * (hue_offset + 82) + 1100, 150, 75);
        light_colors[1][0] = 70;
        light_colors[1][1] = 80;
        light_colors[1][2] = 90;
        course = 1;
    }
    else
    {
        sint32 hue_offset = random_next() % 64;

        river_hsv_to_rgb(hsv_rgb, 934, 196 - hue_offset, 112 - hue_offset);
        river_copy_rgb(&track_colors(1, 6)->colors[0].rgb, hsv_rgb);
        river_copy_rgb(&track_colors(1, 6)->colors[1].rgb, hsv_rgb);
        river_hsv_to_rgb(ambient, 998, 148 - hue_offset, 80 - hue_offset);
        river_copy_rgb(&track_colors(1, 6)->colors[2].rgb, ambient);
        light_colors[0][0] = 96 - hue_offset / 2;
        light_colors[0][1] = light_colors[0][0];
        light_colors[0][2] = 48;
        light_colors[1][0] = 32;
        light_colors[1][1] = 32;
        light_colors[1][2] = 32;
        course = 2;
    }
    race_cfg.variant = course;

    idx = cfg->course;
    RotMatrixX(track_light_angles[idx].pitch * 4096 / 360, &rotation);
    RotMatrixY(track_light_angles[idx].yaw * 4096 / 360, &rotation);
    SetRotMatrix(&rotation);
    ApplyRotMatrix(&src, &t);
    light_vectors[0].vx = -t.vx;
    light_vectors[0].vy = -t.vy;
    light_vectors[0].vz = -t.vz;
    light_vectors[0].pad = 0;
    light_vectors[1].vx = t.vx;
    light_vectors[1].vy = -2 * t.vy;
    light_vectors[1].vz = -t.vz;
    light_vectors[1].pad = 0;
    VectorNormal(&light_vectors[0], &light_vectors[0]);
    VectorNormal(&light_vectors[1], &light_vectors[1]);

    lighting->lights[0].vector[0] = -light_vectors[0].vx;
    lighting->lights[0].vector[1] = -light_vectors[0].vy;
    lighting->lights[0].vector[2] = -light_vectors[0].vz;
    lighting->lights[0].scale = 0;
    lighting->lights[1].vector[0] = -light_vectors[1].vx;
    lighting->lights[1].vector[1] = -light_vectors[1].vy;
    lighting->lights[1].vector[2] = -light_vectors[1].vz;
    lighting->lights[1].scale = 0;
    for (idx = 0; idx < 3; ++idx)
    {
        lighting->lights[2].vector[idx] = 0;
        lighting->color.m[idx][0] = 32u * light_colors[0][idx];
        lighting->color.m[idx][1] = 32u * light_colors[1][idx];
        lighting->color.m[idx][2] = 0;
        lighting->color.t[idx] = ambient[idx];
    }
    lighting->lights[2].scale = 0;

    colors = render_begin_pal(count);
    prev_rec = prev_rec->prev.seg;
    output = colors;
    for (idx = 0; idx <= count; ++idx)
    {
        RGBC *cur_light = (idx & 1) != 0 ? light_b : light_a;
        const RGBC *prev_light = idx == 0 ? NULL : (idx & 1) != 0 ? light_a : light_b;
        ROUTE_SEGMENT *rec = prev_rec->next.seg;
        uint32 selector = rec->join[0] << 4 | rec->join[1];
        ROUTE_VERTEX *cur_vertex = &rec->vertices[selector >> 4];
        const ROUTE_VERTEX *prev_vertex = &prev_rec->vertices[selector & 15];
        sint32 point;
        sint32 sum_r = 0;
        sint32 sum_g = 0;
        sint32 sum_b = 0;
        sint32 sum_max;
        sint32 scale;

        rec->lighting = 0;
        for (point = 0; point < 14; ++point)
        {
            VECTOR along;
            VECTOR across;
            VECTOR n;
            sint32 channel;
            sint32 light;
            RGBC *color = cur_light + point;

            along.vx = prev_rec->origin[0] + 16 * prev_vertex->pos[0] - (rec->origin[0] + 16 * cur_vertex->pos[0]);
            along.vy = prev_rec->origin[1] + 16 * prev_vertex->pos[1] - (rec->origin[1] + 16 * cur_vertex->pos[1]);
            along.vz = prev_rec->origin[2] + 16 * prev_vertex->pos[2] - (rec->origin[2] + 16 * cur_vertex->pos[2]);
            along.pad = 0;
            if (point >= 12)
            {
                across.vx = 16 * (cur_vertex[-1].pos[0] - cur_vertex->pos[0]);
                across.vy = 16 * (cur_vertex[-1].pos[1] - cur_vertex->pos[1]);
                across.vz = 16 * (cur_vertex[-1].pos[2] - cur_vertex->pos[2]);
                OuterProduct12(&across, &along, &n);
            }
            else
            {
                across.vx = 16 * (cur_vertex[1].pos[0] - cur_vertex->pos[0]);
                across.vy = 16 * (cur_vertex[1].pos[1] - cur_vertex->pos[1]);
                across.vz = 16 * (cur_vertex[1].pos[2] - cur_vertex->pos[2]);
                OuterProduct12(&along, &across, &n);
            }
            across.pad = 0;
            VectorNormal(&n, &n);
            color->rgbc = 0;
            for (channel = 0; channel < 3; ++channel)
            {
                sint32 total = 0;
                sint32 src_channel = channel;

                for (light = 0; light < 2; ++light)
                {
                    const VECTOR *light_dir = light_vectors + light;
                    sint32 dot = light_dir->vx * n.vx + light_dir->vy * n.vy + light_dir->vz * n.vz >> 12;
                    if (dot >= 0)
                        total += dot * light_colors[light][src_channel] >> 12;
                }
                color->rgbc |= clamp(total, 0, 255) << (8 * channel);
            }
            ++cur_vertex;
            ++prev_vertex;
        }

        cur_vertex = &rec->vertices[(selector >> 4) + 3];
        rec->pal = output - colors;
        for (point = 3; point < 10; ++point)
        {
            const RGBC *color = cur_light + point;
            sint32 avg = (color->r + color->g + color->b + ambient[0] + ambient[1] + ambient[2]) / 3;

            cur_vertex->shade = avg;
            sum_r += color->r;
            sum_g += color->g;
            sum_b += color->b;
            ++cur_vertex;
        }
        sum_r += 7 * ambient[0];
        sum_g += 7 * ambient[1];
        sum_b += 7 * ambient[2];
        if (sum_r < 33 && sum_g < 33 && sum_b < 33)
            sum_r = sum_g = sum_b = 1;
        sum_max = sum_r >= sum_g && sum_r >= sum_b ? sum_r : sum_g >= sum_b ? sum_g : sum_b;
        // Q12 scale normalizes the largest RGB component to 255
        scale = (255 << 12) / sum_max;
        cur_vertex = &rec->vertices[(selector >> 4) + 3];
        for (point = 3; point < 10; ++point)
        {
            const RGBC *color = cur_light + point;
            sint32 val = (color->r + color->g + color->b + ambient[0] + ambient[1] + ambient[2]) << 12;
            sint32 shade = val / (21 * scale);

            cur_vertex->shade = clamp(shade, 0, 255);
            ++cur_vertex;
        }

        if (prev_light != NULL)
        {
            uint8 r = clamp(sum_r * scale >> 12, 0, 255);
            uint8 g = clamp(sum_g * scale >> 12, 0, 255);
            uint8 b = clamp(sum_b * scale >> 12, 0, 255);

            *output++ = r | g << 8 | b << 16 | 0x3C000000;
            *output++ = r / 2 | (g / 2) << 8 | (b / 2) << 16 | 0x3C000000;
            for (point = 0; point < 14; ++point)
            {
                uint8 flags = rec->commands[point];

                if ((flags & 6u) == 6)
                    break;
                if ((flags & 8u) == 0)
                {
                    const RGBC *color = cur_light + point;
                    sint32 r_sum = (color->r + cur_light[point + 1].r + prev_light[point].r + prev_light[point + 1].r) / 4 + ambient[0];
                    sint32 g_sum = (color->g + cur_light[point + 1].g + prev_light[point].g + prev_light[point + 1].g) / 4 + ambient[1];
                    sint32 b_sum = (color->b + cur_light[point + 1].b + prev_light[point].b + prev_light[point + 1].b) / 4 + ambient[2];
                    *output++ = clamp(r_sum, 0, 255) | clamp(g_sum, 0, 255) << 8 | clamp(b_sum, 0, 255) << 16 | 0x2C000000;
                }
            }
        }
        prev_rec = rec;
    }
    render_end_pal(output - colors);
}
