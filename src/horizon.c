#include "horizon.h"
#include "track.h"
#include "game.h"
#include "utils.h"
#include "xport_trace.h"
#include "psx_gpu.h"
#include <stdlib.h>

static POLY_F4 backdrops[2];
static POLY_F4 *backdrop;

// Owners cover both frame buffers and at most two player views
#define HORIZON_VIEWS 2u
#define HORIZON_COLS 12u
#define HORIZON_GRID_CAP 198u
#define HORIZON_BACKDROP_CAP 108u

typedef struct
{
    uint32 xy[HORIZON_COLS];
    uint32 color[HORIZON_COLS];
    uint16 uv[HORIZON_COLS];
} HORIZON_RING;

typedef struct
{
    POLY_GT4 grid[HORIZON_GRID_CAP];
    POLY_FT4 backdrop[HORIZON_BACKDROP_CAP];
    DR_TWIN windows[4];
    uint32 grid_count, backdrop_count, window_count;
    uint32 view_count;
} HORIZON_BUFFER;

typedef struct
{
    uint32 layers[15];
} HORIZON_FRAME;

static HORIZON_BUFFER horizon_buffers[2];
static HORIZON_FRAME horizon_frames[2][2];
static HORIZON_BUFFER *horizon_buffer;
static HORIZON_FRAME *horizon_frame;

// Twelve radial samples from MAIN.EXE 80080EA8..80080EBF
static const sint16 horizon_radii[HORIZON_COLS] = {4096, 3072, 2048, 1280, 900, 668, 512, 324, 206, 158, 76, 32};

sint16 horizon_showcase_radius(uint32 idx)
{
    if (idx >= HORIZON_COLS)
        abort();
    return horizon_radii[idx];
}

static void horizon_init(uint32 views)
{
    if (views == 0 || views > HORIZON_VIEWS)
        abort();
    if (!gpu_register_packet_range(horizon_buffers, sizeof(horizon_buffers)) || !gpu_register_packet_range(horizon_frames, sizeof(horizon_frames)))
        abort();
    for (uint32 buffer = 0; buffer < 2; ++buffer)
    {
        HORIZON_BUFFER *dst = &horizon_buffers[buffer];
        dst->view_count = views;
        for (uint32 idx = 0; idx < 99 * views; ++idx)
        {
            POLY_GT4 *poly = &dst->grid[idx];
            SetPolyGT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 0);
            setUV4(poly, 0, 0, 63, 0, 0, 63, 63, 63);
            poly->p[0].attr = getClut(768, 511);
            poly->p[1].attr = getTPage(0, 0, 768, 256);
        }
        for (uint32 idx = 0; idx < 54 * views; ++idx)
        {
            POLY_FT4 *poly = &dst->backdrop[idx];
            SetPolyFT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 1);
            setRGB0(poly, 108, 108, 108);
            poly->p[0].attr = getClut(912, 511);
            poly->p[1].attr = getTPage(0, 3, 896, 0);
        }
        for (uint32 idx = 0; idx < 2 * views; ++idx)
        {
            PSX_RECT window = {0, 0, 256, 256};
            if ((idx & 1) != 0)
                window.w = window.h = 64;
            SetTexWindow(&dst->windows[idx], &window);
        }
    }
}

void horizon_select_showcase(uint32 buffer, uint32 view)
{
    if (buffer >= 2 || view >= horizon_buffers[buffer].view_count)
        abort();
    horizon_buffer = &horizon_buffers[buffer];
    horizon_frame = &horizon_frames[view][buffer];
    if (view == 0)
    {
        backdrop = &backdrops[buffer];
        horizon_buffer->grid_count = 0;
        horizon_buffer->backdrop_count = 0;
        horizon_buffer->window_count = 0;
    }
}

static void horizon_project_ring(HORIZON_RING *dst, sint16 angle, const CAMERA *view)
{
    sint16 c = rcos(angle);
    sint16 s = rsin(angle);
    uint32 ax = abs(view->pos[0]);
    uint32 az = abs(view->pos[2]);
    sint32 scroll = (sint32)(ax + az) / 4 % 64;
    sint32 cue = 4095;
    for (uint32 idx = 0; idx < HORIZON_COLS; ++idx)
    {
        SVECTOR pos = {0};
        pos.vx = s * horizon_radii[idx] >> 12;
        pos.vz = c * horizon_radii[idx] >> 12;
        pos.vy = track_horizon->ground;
        if (idx > 0)
            pos.vy = pos.vy + view->pos[1];
        // Original terrain-height helper 8003A2FC returns zero on every call
        xport_gte_write_data(8, cue > 0 ? cue : 0);
        cue -= 512;
        xport_gte_execute(0x780010);
        dst->color[idx] = xport_gte_read_data(22);
        xport_gte_write_data(0, (uint16)pos.vx | (uint32)(uint16)pos.vy << 16);
        xport_gte_write_data(1, (uint16)pos.vz);
        xport_gte_execute(0x180001);
        dst->xy[idx] = xport_gte_read_data(14);
        dst->uv[idx] = scroll;
    }
}

static void horizon_grid_quad(POLY_GT4 *dst, const HORIZON_RING *prev, const HORIZON_RING *cur, uint32 idx)
{
    dst->p[0].xy.xy = prev->xy[idx + 1];
    dst->p[1].xy.xy = cur->xy[idx + 1];
    dst->p[2].xy.xy = prev->xy[idx];
    dst->p[3].xy.xy = cur->xy[idx];
    dst->p[0].rgbc.rgbc = prev->color[idx + 1];
    dst->p[1].rgbc.rgbc = cur->color[idx + 1];
    dst->p[2].rgbc.rgbc = prev->color[idx];
    dst->p[3].rgbc.rgbc = cur->color[idx];
    dst->p[0].uv.uv = (uint8)prev->uv[idx + 1] << 8;
    dst->p[1].uv.uv = 63 | (uint8)cur->uv[idx + 1] << 8;
    dst->p[2].uv.uv = (uint8)(prev->uv[idx] + 64u) << 8;
    dst->p[3].uv.uv = 63 | (uint8)(cur->uv[idx] + 64u) << 8;
    dst->p[0].rgbc.code = 0x3C;
}

void horizon_render_showcase(uint32 *track_ot, const CAMERA *view, uint32 mode, uint32 layout, uint32 players)
{
    FUNCTION_MARKER(0x80038B04, "MAIN.EXE");
    HORIZON_BUFFER *buffer = horizon_buffer;
    HORIZON_FRAME *frame = horizon_frame;
    HORIZON_RING rings[2];
    sint16 angle, shade, boundary;
    sint32 ascending = 1;
    sint32 remainder, heading;
    uint32 connections = players == 1 || layout == 1 ? 9u : 7u;
    uint32 grid_need = connections * 11;
    uint32 backdrop_need = connections * 6;
    if (!buffer || !frame || buffer->grid_count + grid_need > 99 * buffer->view_count || buffer->backdrop_count + backdrop_need > 54 * buffer->view_count || buffer->window_count + 2 > 2 * buffer->view_count)
        abort();
    if (mode == 2)
    {
        gte_set_far_color_raw(640, 0, 512);
        xport_gte_write_data(6, 0x00701060);
    }
    else
    {
        gte_set_far_color_raw(2560, 3328, 3584);
        xport_gte_write_data(6, 0x004B4018);
    }
    ClearOTagR(frame->layers, 15);
    frame->layers[0] = *track_ot;
    AddPrims(track_ot, &frame->layers[14], frame->layers);
    AddPrim(&frame->layers[13], &buffer->windows[buffer->window_count++]);
    heading = 1024 - (ratan2(view->rotation.m[2][2], view->rotation.m[0][2]) & 0xFFF);
    if (heading < 0)
        heading += 4096;
    angle = (-heading - ratan2(connections == 9 ? 256 : 128, (sint16)gte_read_h()) - 171);
    remainder = angle % 684;
    if (remainder < 0)
        remainder = track_horizon->alternating ? -remainder : remainder + 683;
    shade = 255 * remainder / 683;
    if (track_horizon->alternating)
    {
        if ((angle / 684 & 1) != 0)
            shade = 255 - shade;
        else
            ascending = 0;
        shade = clamp(shade, 0, 255);
    }
    horizon_project_ring(&rings[0], angle, view);
    if (ascending)
    {
        boundary = (shade + (shade < 0 ? 63 : 0) | 63);
        angle = (angle + 171 * (boundary - shade) / 63);
    }
    else
    {
        boundary = shade / 64 * 64;
        angle = (angle + 171 * (shade - boundary) / 63);
    }
    for (uint32 ring = 0; ring < connections; ++ring)
    {
        HORIZON_RING *prev = &rings[ring & 1];
        HORIZON_RING *cur = &rings[ring + 1 & 1];
        sint32 bucket_base = 5 + (ascending ? shade : boundary) / 64;
        sint16 bucket_step = track_horizon->tex_width / 64;
        horizon_project_ring(cur, angle, view);
        for (sint32 idx = 0; idx < 11u; ++idx)
        {
            POLY_GT4 *grid = &buffer->grid[buffer->grid_count++];
            horizon_grid_quad(grid, prev, cur, idx);
            if (idx < 6u)
            {
                sint32 bucket = bucket_base + idx / 4u * bucket_step;
                POLY_FT4 *dst = &buffer->backdrop[buffer->backdrop_count++];
                if (bucket < 0 || bucket >= 15)
                    abort();
                dst->p[0].xy.xy = prev->xy[idx + 1u];
                dst->p[1].xy.xy = cur->xy[idx + 1u];
                dst->p[2].xy.xy = prev->xy[idx];
                dst->p[3].xy.xy = cur->xy[idx];
                dst->p[0].uv.uv = (uint8)shade | (uint8)(-2 - 16 * (sint32)(idx + 1u)) << 8;
                dst->p[1].uv.uv = (uint8)boundary | (uint8)(-2 - 16 * (sint32)(idx + 1u)) << 8;
                dst->p[2].uv.uv = (uint8)shade | (uint8)(-2 - 16 * idx) << 8;
                dst->p[3].uv.uv = (uint8)boundary | (uint8)(-2 - 16 * idx) << 8;
                AddPrim(&frame->layers[bucket], dst);
            }
            AddPrim(&frame->layers[13], grid);
        }
        if (ascending)
        {
            shade = boundary + 1;
            boundary = (boundary + 64);
            if (shade >= 256)
            {
                shade = track_horizon->alternating ? 255 : 0;
                boundary = track_horizon->alternating ? 192 : 63;
                if (track_horizon->alternating)
                    ascending = 0;
            }
        }
        else
        {
            shade = boundary - 1;
            boundary = (boundary - 64);
            if (shade < 0)
            {
                shade = track_horizon->alternating ? 0 : 255;
                boundary = track_horizon->alternating ? 63 : 192;
                if (track_horizon->alternating)
                    ascending = 1;
            }
        }
        angle = (angle + 171);
    }
    AddPrim(&frame->layers[13], &buffer->windows[buffer->window_count++]);
}

static void horizon_init_backdrop(uint32 mode)
{
    if (!gpu_register_packet_range(backdrops, sizeof(backdrops)))
        abort();
    for (uint32 buffer = 0; buffer < 2; ++buffer)
    {
        POLY_F4 *dst = &backdrops[buffer];
        SetPolyF4(dst);
        dst->rgbc.rgbc = mode == 2 ? 0x28700820u : 0x28403808u;
        dst->p[0].xy = 344u << 16;
        dst->p[1].xy = 368u | 344u << 16;
        dst->p[2].xy = 512u << 16;
        dst->p[3].xy = 368u | 512u << 16;
    }
}

void horizon_init_showcase(uint32 views, uint32 mode)
{
    horizon_init(views);
    horizon_init_backdrop(mode);
}

uint32 *horizon_ot_entry(uint32 idx)
{
    if (!horizon_frame || idx >= 15)
        abort();
    return &horizon_frame->layers[idx];
}

void horizon_add_backdrop(uint32 *ot)
{
    AddPrim(ot, backdrop);
}

static void horizon_project(sint16 s, sint16 c, const sint16 heights[3], uint32 proj[3])
{
    SVECTOR vertices[3];
    sint32 scr[3];
    sint32 depth[3];
    sint32 flags;
    uint32 idx;

    for (idx = 0; idx < 3; ++idx)
    {
        vertices[idx].vx = s;
        vertices[idx].vy = heights[idx];
        vertices[idx].vz = c;
        vertices[idx].pad = 0;
    }
    gte_project3_full_depth(vertices, scr, depth, &flags);
    for (idx = 0; idx < 3; ++idx)
        proj[idx] = scr[idx];
}

uint32 horizon_render(RENDER_FRAME *buffer, const CAMERA *view)
{
    POLY_FT4 *strip_pool;
    POLY_F4 *edge_pool;
    POLY_G4 *triangle_pool;
    const TRACK_HORIZON *cfg = track_horizon;
    sint16 heights[12] = {0};
    uint32 proj_a[9] = {0};
    uint32 proj_b[9] = {0};
    uint32 *cur = proj_a;
    uint32 *next = proj_b;
    sint16 angle;
    sint32 pos;
    sint16 shade;
    sint16 boundary;
    sint32 ascending = 1;
    sint32 height_count;
    sint32 grp_count;
    sint32 radius_scale;
    sint32 ring_count;
    sint32 stripe_bucket_step;
    sint32 ring;
    sint32 grp;
    uint32 *track_ot = render_scene_ot(buffer);
    HORIZON_PRIMS prims;

    FUNCTION_MARKER(0x800198AC, "MAIN.EXE");
    prims = render_horizon_prims(buffer);
    strip_pool = prims.strips;
    edge_pool = prims.edges;
    triangle_pool = prims.gradients;

    angle = 1024 - (ratan2(view->rotation.m[2][2], view->rotation.m[0][2]) & 0xFFF);
    if (angle < 0)
    {
        angle += 4096;
    }

    {
        sint16 proj = gte_read_h();
        uint32 mode = race_cfg.players;
        sint32 numerator;

        if (mode == 1)
        {
            numerator = 256;
            ring_count = 10;
        }
        else
        {
            mode = race_cfg.layout;
            if (mode == 1)
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
        angle = (-angle - ratan2(numerator, proj) - 135);
    }
    pos = angle % 684;
    if (pos < 0)
    {
        if (cfg->alternating != 0)
            pos = -pos;
        else
            pos += 683;
    }
    shade = 255 * pos / 683;
    if (cfg->alternating != 0)
    {
        if ((angle / 684 & 1) != 0)
            shade = 255 - shade;
        else
            ascending = 0;
        shade = clamp(shade, 0, 255);
    }
    if (cfg->strips > 4u)
        abort();
    heights[0] = cfg->ground;
    height_count = 1;
    if (cfg->edge != 0)
    {
        heights[height_count] = heights[height_count - 1] + cfg->edge_step;
        ++height_count;
    }
    for (grp = 0; grp < cfg->strips; ++grp)
    {
        heights[height_count] = heights[height_count - 1] + cfg->strip_step;
        ++height_count;
    }
    if (cfg->gradient != 0)
    {
        heights[height_count] = heights[height_count - 1] - cfg->gradient_step;
        ++height_count;
        heights[height_count] = heights[height_count - 1] + cfg->top_step;
        ++height_count;
    }
    else
    {
        heights[height_count] = heights[height_count - 1] - cfg->gradient_step;
        ++height_count;
    }
    if (height_count > 9 || ring_count * cfg->strips > 40 || ring_count * (1 + (cfg->edge != 0)) > 20 || ring_count > 10)
        abort();
    grp_count = (height_count + 2) / 3;
    radius_scale = race_cfg.mode == 8 ? 1 : 2;
    for (grp = 0; grp < grp_count; ++grp)
    {
        sint16 grp_heights[3] = {heights[grp * 3], heights[grp * 3 + 1], heights[grp * 3 + 2]};
        sint16 s = rsin(angle) * radius_scale;
        sint16 c = rcos(angle) * radius_scale;

        horizon_project(s, c, grp_heights, &proj_a[grp * 3]);
    }
    if (ascending)
    {
        boundary = (shade + (shade < 0 ? 63 : 0) | 63);
        angle = (angle + 171 * (boundary - shade) / 63);
    }
    else
    {
        boundary = shade / 64 * 64;
        angle = (angle + 171 * (shade - boundary) / 63);
    }
    stripe_bucket_step = (sint16)cfg->tex_width / 64;
    for (ring = 0; ring < ring_count; ++ring)
    {
        POLY_F4 *edge_poly = edge_pool;
        uint32 edge_present;

        for (grp = 0; grp < grp_count; ++grp)
        {
            sint16 grp_heights[3] = {heights[grp * 3], heights[grp * 3 + 1], heights[grp * 3 + 2]};
            sint16 s = rsin(angle) * radius_scale;
            sint16 c = rcos(angle) * radius_scale;

            horizon_project(s, c, grp_heights, &next[grp * 3]);
        }
        edge_present = cfg->edge != 0;
        if (edge_present)
        {
            POLY_F4 *poly = edge_poly;
            poly->p[2].xy = cur[0];
            poly->p[0].xy = cur[1];
            poly->p[3].xy = next[0];
            poly->p[1].xy = next[1];
            poly->rgbc.rgbc = cfg->colors[0].word;
            AddPrim(&track_ot[13], poly);
            ++edge_pool;
            edge_poly = edge_pool;
        }
        for (grp = 0; grp < cfg->strips; ++grp)
        {
            POLY_FT4 *poly = strip_pool++;
            uint32 point = edge_present + grp;
            sint32 tex_left = -2 - 32 * (grp + 1);
            sint32 tex_right = -2 - 32 * grp;
            sint32 bucket_offset = grp / 2 * stripe_bucket_step;
            sint32 shade_bucket = (ascending ? shade : boundary) / 64;
            uint32 bucket = shade_bucket + 5 + bucket_offset;
            if (bucket >= 15)
                abort();

            poly->p[2].xy.xy = cur[point];
            poly->p[0].xy.xy = cur[point + 1];
            poly->p[3].xy.xy = next[point];
            poly->p[1].xy.xy = next[point + 1];
            setUV4(poly, shade, tex_left, boundary, tex_left, shade, tex_right, boundary, tex_right);
            AddPrim(&track_ot[bucket], poly);
        }
        // Original 8001A0A8 underflows the strip pool when the resource has no strips
        if (cfg->strips != 0)
        {
            ++strip_pool[-1].p[0].uv.v;
            ++strip_pool[-1].p[1].uv.v;
        }
        if (cfg->gradient != 0)
        {
            POLY_G4 *poly = triangle_pool++;
            poly->p[2].xy.xy = cur[height_count - 2];
            poly->p[0].xy.xy = cur[height_count - 1];
            poly->p[3].xy.xy = next[height_count - 2];
            poly->p[1].xy.xy = next[height_count - 1];
            AddPrim(&track_ot[13], poly);
        }
        {
            POLY_F4 *poly = edge_poly;
            poly->p[0].xy = cur[height_count - 1];
            poly->p[2].xy = cur[height_count - 1];
            poly->p[1].xy = next[height_count - 1];
            poly->p[3].xy = next[height_count - 1];
            poly->p[0].y = poly->p[1].y = 0;
            poly->rgbc.rgbc = cfg->colors[2].word;
            poly->rgbc.code = 0x28;
            AddPrim(&track_ot[13], poly);
            ++edge_pool;
        }
        if (ascending)
        {
            shade = boundary + 1;
            boundary += 64;
            if (shade >= 256)
            {
                shade = 255;
                if (cfg->alternating != 0)
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
            if (shade < 0)
            {
                shade = 0;
                if (cfg->alternating != 0)
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
        angle = (angle + 171);
        if ((ring & 1) != 0)
        {
            cur = proj_a;
            next = proj_b;
        }
        else
        {
            cur = proj_b;
            next = proj_a;
        }
    }
    AddPrim(&track_ot[13], &prims.tex_windows[2]);
    AddPrim(&track_ot[5], &prims.tex_windows[3]);
    return getaddr(&track_ot[5]);
}
