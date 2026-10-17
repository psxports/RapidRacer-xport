#include "hud.h"
#include "horizon.h"
#include "pickup.h"
#include "track.h"
#include "ai.h"
#include "game.h"
#include "boat.h"
#include "menu.h"
#include "route.h"
#include "text.h"
#include "polygon.h"
#include "psx_gpu.h"
#include "render.h"
#include "mesh.h"
#include "input.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

uint32 render_capture_enabled;
sint32 render_proj_speed[2];

RENDER_SAMPLE render_save_sample(void)
{
    RENDER_SAMPLE sample = {0};
    if (boat_racer_count < 0 || boat_racer_count > 32)
        abort();
    sample.seed = random_seed;
    sample.rand_bit = mesh_render_state.rand_bit;
    sample.rand_pair = mesh_render_state.rand_pair;
    for (sint32 idx = 0; idx < boat_racer_count; ++idx)
        sample.phases[idx] = boat_racers[idx]->trail.phase;
    return sample;
}

void render_restore_sample(const RENDER_SAMPLE *sample)
{
    random_seed = sample->seed;
    mesh_render_state.rand_bit = sample->rand_bit;
    mesh_render_state.rand_pair = sample->rand_pair;
    for (sint32 idx = 0; idx < boat_racer_count; ++idx)
        boat_racers[idx]->trail.phase = sample->phases[idx];
}

// Mutable lighting sets from MAIN.EXE 80089580/800895B8/800914CC
static RENDER_LIGHTING default_lighting[3] = {{{{{-4782, 1353, 2345}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{1704, 3060, 0}, {3060, 2376, 0}, {2616, 1248, 0}}, {124, 90, 127}}}, {{{{-4782, 1353, 2345}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{852, 1530, 0}, {1530, 1188, 0}, {1308, 624, 0}}, {62, 45, 64}}}, {{{{1313, 1398, 3615}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{1704, 3060, 0}, {3060, 2376, 0}, {2616, 1248, 0}}, {124, 90, 127}}}};

// The original root starts at zero-filled memory before a course is loaded
static RENDER_LIGHTING initial_lighting[256];
static RENDER_LIGHTING archive_lighting[256];
static RENDER_LIGHTING *active_lighting = initial_lighting;
static uint32 lighting_count = 256;

RENDER_LIGHTING *render_lighting(uint32 idx)
{
    if (idx >= lighting_count)
        abort();
    return &active_lighting[idx];
}

void render_select_lighting(RENDER_LIGHT_SET set)
{
    if ((uint32)set >= 3)
        abort();
    active_lighting = &default_lighting[set];
    lighting_count = 1;
}

void render_load_lighting(const uint8 *src, uint32 size)
{
    uint32 count;
    uint32 idx;

    // Disk entries contain three eight-byte lights followed by a PsyQ matrix
    if (!src || size % 56 != 0 || size / 56 > 256)
        abort();
    count = size / 56;
    if (count == 0 || count > 256)
        abort();
    for (idx = 0; idx < count; ++idx)
    {
        RENDER_LIGHTING *entry = &archive_lighting[idx];
        uint32 row;
        uint32 axis;

        for (row = 0; row < 3; ++row)
        {
            for (axis = 0; axis < 3; ++axis)
            {
                entry->lights[row].vector[axis] = xport_load_le16(src + row * 8 + axis * 2);
                entry->color.m[row][axis] = xport_load_le16(src + 24u + row * 6 + axis * 2);
            }
            entry->lights[row].scale = xport_load_le16(src + row * 8 + 6u);
            entry->color.t[row] = xport_load_le32(src + 44u + row * 4);
        }
        src += 56;
    }
    active_lighting = archive_lighting;
    lighting_count = count;
}

// Palette selectors are uint16 word offsets followed by at most fourteen colors
enum
{
    RENDER_PALETTE_CAPACITY = 65536 + 16
};

typedef struct
{
    const uint32 *colors;
    uint32 count;
} RENDER_PALETTE;

// Immutable defaults from MAIN.EXE 80089538/8008955C
static const uint32 default_pals[2][9] = {{0x3C808080, 0x2C808080, 0x2C808080, 0x2C808080, 0x2C808080, 0x2C808080, 0x2C808080, 0x2C808080, 0x2C808080}, {0x3C323232, 0x2C323232, 0x2C323232, 0x2C323232, 0x2C323232, 0x2C323232, 0x2C323232, 0x2C323232, 0x2C323232}};

static uint32 initial_pal[RENDER_PALETTE_CAPACITY];
static uint32 archive_pal[RENDER_PALETTE_CAPACITY];
static uint32 generated_pal[RENDER_PALETTE_CAPACITY];
static RENDER_PALETTE active_pal = {initial_pal, RENDER_PALETTE_CAPACITY};
static RENDER_PALETTE frame_pal = {initial_pal, RENDER_PALETTE_CAPACITY};

const uint32 *render_pal(uint32 idx)
{
    if (idx >= frame_pal.count)
        abort();
    return &frame_pal.colors[idx];
}

void render_publish_pal(void)
{
    frame_pal = active_pal;
}

void render_select_pal(sint32 mirror)
{
    if ((uint32)mirror > 1)
        abort();
    active_pal.colors = default_pals[mirror];
    active_pal.count = 9;
}

void render_load_pal(const uint8 *src, uint32 size)
{
    uint32 words = 0;
    uint32 idx;

    // Resource flags are converted to render commands later by scene processing
    for (idx = 0; idx < route_resources.count; ++idx)
    {
        const ROUTE_SEGMENT *seg = route_resources.segs[idx];
        uint32 end = seg->pal + 2u;
        uint32 command;

        for (command = 0; command < 14; ++command)
        {
            uint8 flags = seg->commands[command];
            if ((flags & 6u) == 6)
                break;
            if ((flags & 8u) == 0)
                ++end;
        }
        if (command == 14 || end > RENDER_PALETTE_CAPACITY)
            abort();
        if (end > words)
            words = end;
    }
    if (!src && words != 0 || words > size / 4)
        abort();
    for (idx = 0; idx < words; ++idx)
        archive_pal[idx] = xport_load_le32(src + 4 * idx);
    active_pal.colors = archive_pal;
    active_pal.count = words;
}

uint32 *render_begin_pal(uint32 segs)
{
    if (segs > RENDER_PALETTE_CAPACITY / 16u)
        abort();
    active_pal.colors = generated_pal;
    active_pal.count = 0;
    return generated_pal;
}

void render_end_pal(uint32 words)
{
    if (words > RENDER_PALETTE_CAPACITY)
        abort();
    active_pal.count = words;
}

// Projection state from MAIN.EXE 800B6BDC/800B6AC4/800B6AC8
static uint32 proj_dist;
static sint32 proj_offset[2];

static BOAT *indicator_target;
static sint32 indicator_distance;
static uint32 indicator_ticks;
static sint32 indicator_ready;
static sint32 indicator_dir[4];

// Indicator packet owners from MAIN.EXE 800B7410/800B73F0
static LINE_F3 indicator_polylines[2][3];
static LINE_F2 indicator_lines[2];
// Original immutable geometry at MAIN.EXE 80080AE4/80080B24
static const SVECTOR indicator_vertices[8] = {{-22, 0, 0, 0}, {-48, 0, 0, 0}, {0, 48, 0, 0}, {48, 0, 0, 0}, {22, 0, 0, 0}, {22, -64, 0, 0}, {-22, -64, 0, 0}, {-22, 0, 0, 0}};
static const MATRIX indicator_matrix = {{{2048, 0, 0}, {0, 2048, 0}, {0, 0, ONE}}, {0, 90, 0}};

static void render_indicator_points(LINE_F3 *line, const sint32 points[3])
{
    for (uint32 i = 0; i < 3; ++i)
        line->p[i].xy = points[i];
}

// Native player/frame owners retain PsyQ packets only at the GPU boundary
struct RENDER_FRAME
{
    uint32 initialized;
    POLY_FT4 strips[40];
    POLY_F4 edges[20];
    POLY_G4 gradients[10];
    LINE_F2 lines[8];
    DR_TWIN tex_windows[4];
    uint32 object_ot[101];
    uint32 route_ot[251];
    uint32 track_ot[15];
    DR_MODE ui_modes[2];
    HUD_PRIM hud_prims[HUD_PRIM_CAPACITY];
    HUD hud;
};

static RENDER_FRAME frames[2][2];

RENDER_FRAME *render_frame(const CAMERA *view, sint32 frame)
{
    sint32 player;
    if (view == &camera_views[0])
        player = 0;
    else if (view == &camera_views[1])
        player = 1;
    else
        abort();
    if (frame < 0 || frame >= 2 || !gpu_register_packet_range(frames, sizeof(frames)))
        abort();
    return &frames[player][frame];
}

static void render_check_frame(const RENDER_FRAME *frame)
{
    uint32 player, buffer;
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
            if (frame == &frames[player][buffer])
                return;
    abort();
}

HORIZON_PRIMS render_horizon_prims(RENDER_FRAME *frame)
{
    HORIZON_PRIMS prims;
    render_check_frame(frame);
    if (!frame->initialized)
        abort();
    prims.strips = frame->strips;
    prims.edges = frame->edges;
    prims.gradients = frame->gradients;
    prims.tex_windows = frame->tex_windows;
    return prims;
}

HUD *render_frame_hud(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return &frame->hud;
}

uint32 *render_route_ot(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return frame->route_ot;
}

uint32 *render_object_ot(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return frame->object_ot;
}

uint32 *render_scene_ot(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return frame->track_ot;
}

DR_MODE *render_ui_modes(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return frame->ui_modes;
}

sint32 render_init_prims(RENDER_FRAME *buffer)
{
    TRACK_HORIZON *cfg = track_horizon;
    uint32 idx;
    PSX_RECT window;

    FUNCTION_MARKER(0x8001483C, "MAIN.EXE");
    render_check_frame(buffer);
    for (idx = 0; idx < 40; ++idx)
    {
        POLY_FT4 *poly = &buffer->strips[idx];
        setPolyFT4(poly);
        setShadeTex(poly, 1);
        poly->p[1].attr = getTPage(0, 0, 896, 256);
        poly->p[0].attr = getClut(896, 511);
    }
    for (idx = 0; idx < 20; ++idx)
    {
        setPolyF4(&buffer->edges[idx]);
    }
    for (idx = 0; idx < 10; ++idx)
    {
        POLY_G4 *poly = &buffer->gradients[idx];
        setPolyG4(poly);
        poly->p[0].rgbc.rgbc = cfg->colors[2].word;
        poly->p[1].rgbc.rgbc = cfg->colors[2].word;
        poly->p[2].rgbc.rgbc = cfg->colors[1].word;
        poly->p[3].rgbc.rgbc = cfg->colors[1].word;
    }
    if (cfg->tex_width != 64 && cfg->tex_width != 128 && cfg->tex_width != 256)
    {
        cfg->tex_width = 256;
    }
    setRECT(&window, 16, 0, 48, 64);
    SetTexWindow(&buffer->tex_windows[0], &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(&buffer->tex_windows[1], &window);
    window.w = cfg->tex_width;
    SetTexWindow(&buffer->tex_windows[2], &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(&buffer->tex_windows[3], &window);
    for (idx = 0; idx < 8; ++idx)
    {
        LINE_F2 *line = &buffer->lines[idx];
        setLineF2(line);
        setRGB0(line, 255, 255, 255);
    }
    buffer->initialized = 1;
    return 0;
}

// Curve owner from MAIN.EXE 800F0508; rows 2/8/10/15 also serve texture and light animation
uint8 render_curve[16][512];

// Original 80083774 curve includes the extra byte read by the third quadrant
static const uint8 curve_src[129] = {
    63, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 73, 74, 74, 75, 76, 77, 77, 78, 79, 80, 80, 81, 82, 83, 83, 84, 85, 86, 86, 87, 88, 89, 89, 90, 91, 91, 92, 93, 93, 94, 95, 95, 96, 97, 97, 98, 99, 99, 100, 101, 101, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108, 108, 109, 109, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 115, 116, 116, 117, 117, 118, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 208,
};

sint32 render_build_curve_scaled_lut(void)
{
    sint32 scale;

    FUNCTION_MARKER(0x80014A68, "MAIN.EXE");
    for (scale = 0; scale < 16; ++scale)
    {
        uint8 *output = render_curve[scale];
        sint32 idx;

        for (idx = 0; idx < 128; ++idx)
            output[idx] = scale * curve_src[idx] >> 5;
        for (idx = 0; idx < 128; ++idx)
            output[128 + idx] = scale * curve_src[127 - idx] >> 5;
        for (idx = 0; idx < 128; ++idx)
            output[256 + idx] = scale * (128 - curve_src[1 + idx]) >> 5;
        for (idx = 0; idx < 128; ++idx)
            output[384 + idx] = scale * (128 - curve_src[127 - idx]) >> 5;
    }
    return 0;
}

// Original material sources from MAIN.EXE 800922C0/80092520/800925D0/80092430/800928D0/800926D0
typedef struct
{
    uint16 clut_x, clut_y;
    RENDER_TEXCOORD uv[4];
    uint32 surface;
} RENDER_MATERIAL_SRC;

static const RENDER_MATERIAL_SRC material_src_0[23] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 508, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 508, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 509, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 509, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0},
    {768, 507, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 507, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 506, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 506, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 505, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 505, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1},
};

static const RENDER_MATERIAL_SRC material_src_1[11] = {
    {768, 500, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 510, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 501, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 502, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 509, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 500, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 510, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 501, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 502, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1},
};

static const RENDER_MATERIAL_SRC material_src_2[16] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 500, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 500, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 501, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 501, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 502, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 502, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0},
};

static const RENDER_MATERIAL_SRC material_src_3[15] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 508, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 508, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 507, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 507, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 505, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 505, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1},
};

static const RENDER_MATERIAL_SRC material_src_4[32] = {
    {768, 495, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 495, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 510, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 494, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 509, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 494, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 508, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 493, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 507, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 493, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 506, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 492, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 505, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 492, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 504, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0},
    {768, 491, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 503, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 491, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 502, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 490, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 501, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 490, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 500, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 489, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 499, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 489, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 498, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {768, 488, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1}, {768, 497, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {768, 488, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1}, {768, 496, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0},
};

static const RENDER_MATERIAL_SRC material_src_5[32] = {
    {512, 197, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 195, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 190, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1},     {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 198, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1},     {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 193, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1},   {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 193, {{0, 0}, {0, 31}, {39, 0}, {39, 31}}, 1},   {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 193, {{39, 31}, {39, 0}, {0, 31}, {0, 0}}, 1},   {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 193, {{39, 0}, {39, 31}, {0, 0}, {0, 31}}, 1},   {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0},
    {512, 196, {{39, 0}, {39, 31}, {0, 0}, {0, 31}}, 1}, {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 196, {{39, 31}, {39, 0}, {0, 31}, {0, 0}}, 1}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 191, {{63, 32}, {48, 32}, {63, 63}, {48, 63}}, 1}, {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 191, {{48, 32}, {63, 32}, {48, 63}, {63, 63}}, 1}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 192, {{40, 31}, {40, 0}, {47, 31}, {47, 0}}, 1}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 192, {{40, 31}, {40, 0}, {47, 31}, {47, 0}}, 1}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0}, {512, 194, {{48, 0}, {48, 31}, {63, 0}, {63, 31}}, 1}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0}, {512, 194, {{48, 31}, {48, 0}, {63, 31}, {63, 0}}, 1}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0},
};

static RENDER_MATERIAL materials[32];

const RENDER_MATERIAL *render_material(uint32 idx)
{
    if (idx >= 32)
        abort();
    return &materials[idx];
}

sint32 render_init_materials(sint32 grp)
{
    const RENDER_MATERIAL_SRC *src;
    sint32 count;
    sint32 idx;
    uint16 tpage;

    FUNCTION_MARKER(0x80014B7C, "MAIN.EXE");
    switch (grp)
    {
        case 0:
            src = material_src_0;
            count = 23;
            break;
        case 1:
            src = material_src_1;
            count = 11;
            break;
        case 2:
            src = material_src_2;
            count = 16;
            break;
        case 3:
        case 5:
            src = material_src_4;
            count = 32;
            break;
        case 4:
            src = material_src_3;
            count = 15;
            break;
        case 6:
            src = material_src_5;
            count = 32;
            break;
        default:
            input_stop_exit();
            return 0;
    }
    tpage = GetTPage(0, 0, 768, 256);
    for (idx = 0; idx < count; ++idx)
    {
        RENDER_MATERIAL *material = &materials[idx];
        uint32 vertex;
        material->tpage = tpage;
        material->clut = GetClut(src[idx].clut_x, src[idx].clut_y);
        for (vertex = 0; vertex < 4; ++vertex)
            material->uv[vertex] = src[idx].uv[vertex];
        material->surface = src[idx].surface;
    }
    return 0;
}

sint32 render_init_ui_prims(void)
{
    uint32 buffer, idx;

    FUNCTION_MARKER(0x80015A78, "MAIN.EXE");
    if (!gpu_register_packet_range(indicator_polylines, sizeof(indicator_polylines)) || !gpu_register_packet_range(indicator_lines, sizeof(indicator_lines)))
        abort();
    for (buffer = 0; buffer < 2; ++buffer)
    {
        for (idx = 0; idx < 3; ++idx)
        {
            LINE_F3 *line = &indicator_polylines[buffer][idx];
            setLineF3(line);
            setRGB0(line, 255, 255, 16);
        }
        setLineF2(&indicator_lines[buffer]);
        setRGB0(&indicator_lines[buffer], 255, 255, 16);
    }
    indicator_target = NULL;
    indicator_distance = 0;
    indicator_ticks = 0;
    indicator_ready = 0;
    return 0;
}

static uint32 render_update_indicator(const RACE_VIEW *owner)
{
    BOAT *candidate = NULL;
    const BOAT_MENU *menu;
    sint32 idx;

    indicator_ready = 0;
    if (race_cfg.mode == 5)
        return 2;
    if (race_cfg.players == 2)
        return 4;
    menu = boat_player_menu(owner);
    if (menu->mode != 4)
        return 4;
    for (idx = menu->racer_num + 1u; idx < boat_racer_count; ++idx)
    {
        BOAT *entry = boat_racers[idx];

        if (entry->race.mode != 4)
            break;
        if (entry->route.distance < 0x1000000 && (candidate == 0 || entry->route.distance < candidate->route.distance))
            candidate = entry;
    }
    if (indicator_target == 0)
    {
        if (candidate == 0 || candidate->race.mode != 4)
            return 4;
        indicator_target = candidate;
        indicator_ticks = 0;
    }
    else
    {
        BOAT *cur = indicator_target;

        sint32 gap = cur->route.distance;

        if (candidate)
            gap = cur->route.distance - candidate->route.distance;
        if (candidate != cur && gap < 2500)
            candidate = cur;
        if (candidate == cur)
        {
            if (candidate->race.mode != 4)
            {
                indicator_target = NULL;
                return 4;
            }
            if (indicator_distance >= candidate->route.distance && (sint32)indicator_ticks < 240)
                indicator_ticks += 4;
        }
        else
        {
            if (candidate == 0 || candidate->race.mode != 4)
            {
                indicator_target = NULL;
                return 4;
            }
            indicator_target = candidate;
        }
    }
    indicator_distance = candidate->route.distance;
    if ((sint32)indicator_ticks < 120)
        return 1;
    const BOAT *camera = boat_player(owner);
    indicator_dir[0] = candidate->pos.x - camera->pos.x;
    indicator_dir[1] = candidate->pos.z - camera->pos.z;
    indicator_dir[2] = camera->pose.m[0][2];
    indicator_dir[3] = camera->pose.m[2][2];
    indicator_ready = 1;
    return 0;
}

uint32 render_read_indicator(MATRIX *transform, const SVECTOR **points)
{
    if (!indicator_ready)
        return 1;
    MATRIX matrix = indicator_matrix;
    sint32 angle = 0, steering = 0;
    {
        sint32 target_angle = ratan2(indicator_dir[0], indicator_dir[1]);
        sint32 camera_angle = ratan2(indicator_dir[2], indicator_dir[3]);
        uint32 adjusted;

        angle = target_angle - ((camera_angle & 0xFFF) - 0x800) & 0xFFF;
        adjusted = angle - 1u;
        if ((uint32)(angle - 0x201) < 0x5FF)
        {
            angle = 0x200;
            adjusted = 0x1FF;
        }
        else if ((uint32)(angle - 0x801) < 0x5FF)
        {
            angle = 0xE00;
            adjusted = 0xDFF;
        }
        if (adjusted < 0x200)
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
    *transform = matrix;
    *points = indicator_vertices;
    return 0;
}

uint32 render_prepare_indicator(const RACE_VIEW *owner, MATRIX *transform, const SVECTOR **points)
{
    uint32 result = render_update_indicator(owner);
    return result ? result : render_read_indicator(transform, points);
}

uint32 hd_render_read_indicator(MATRIXF *transform, const SVECTOR **points)
{
    if (!indicator_ready)
        return 1;
    float angle = (atan2f((float)indicator_dir[0], (float)indicator_dir[1]) - atan2f((float)indicator_dir[2], (float)indicator_dir[3])) / RAD12 + 2048;
    if (angle < 0)
        angle += 4096;
    else if (angle >= 4096)
        angle -= 4096;
    if (angle >= 513 && angle < 2048)
        angle = 512;
    else if (angle >= 2049 && angle < 3584)
        angle = 3584;
    float steering = angle >= 1 && angle < 513 ? fmaxf(-40, -angle / 16) : fminf(40, (4096 - angle) / 16);
    IdentityMatrixf(transform);
    transform->m[0][0] = transform->m[1][1] = 1000.0f / 4096;
    RotMatrixZf(angle * RAD12, transform);
    transform->t[0] = steering;
    transform->t[1] = 90;
    *points = indicator_vertices;
    return 0;
}

uint32 hd_render_prepare_indicator(const RACE_VIEW *owner, MATRIXF *transform, const SVECTOR **points)
{
    uint32 result = render_update_indicator(owner);
    return result ? result : hd_render_read_indicator(transform, points);
}

uint32 render_target_indicator(const RACE_VIEW *owner, uint32 *ot, sint32 alternate)
{
    const SVECTOR *vertices;
    MATRIX matrix;
    sint32 scr0[3];
    sint32 scr1[3];
    sint32 scr2[3];
    sint32 depth[3];
    sint32 flags;
    LINE_F3 *prim;
    LINE_F2 *final_prim;
    uint32 result;

    FUNCTION_MARKER(0x80015B4C, "MAIN.EXE");
    if (alternate < 0 || alternate >= 2 || !ot)
        abort();
    prim = indicator_polylines[alternate];
    final_prim = &indicator_lines[alternate];
    result = race_paused ? render_read_indicator(&matrix, &vertices) : render_prepare_indicator(owner, &matrix, &vertices);
    if (result)
        return result;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    gte_project3_full_depth(&vertices[0], scr0, depth, &flags);
    gte_project3_full_depth(&vertices[2], scr1, depth, &flags);
    render_indicator_points(&prim[0], scr0);
    AddPrim(ot, &prim[0]);
    gte_project3_full_depth(&vertices[5], scr2, depth, &flags);
    render_indicator_points(&prim[1], scr1);
    AddPrim(ot, &prim[1]);
    sint32 tail[3] = {scr1[2], scr2[0], scr2[1]};
    render_indicator_points(&prim[2], tail);
    AddPrim(ot, &prim[2]);
    final_prim->p[0].x = scr2[1];
    final_prim->p[0].y = (uint32)scr2[1] >> 16;
    final_prim->p[1].x = scr2[2];
    final_prim->p[1].y = (uint32)scr2[2] >> 16;
    AddPrim(ot, final_prim);
    return *ot;
}

// Render order owner from MAIN.EXE 800B6AB4/800B6A08
RENDER_ORDER render_order;

void render_set_ot(uint32 *ot, uint32 depth_limit)
{
    FUNCTION_MARKER(0x80016190, "MAIN.EXE");
    render_order.ot = ot;
    render_order.depth_limit = depth_limit;
}

sint32 render_scale_proj_transform(uint32 shift)
{
    MATRIX matrix;
    sint16 *vals = &matrix.m[0][0];
    uint32 shift_bits = shift & 31;
    uint32 idx;
    sint32 proj;

    FUNCTION_MARKER(0x800161A0, "MAIN.EXE");
    ReadRotMatrix(&matrix);
    for (idx = 0; idx < 6; ++idx)
    {
        sint32 val = (uint32)vals[idx] << shift_bits;

        if (shift == 3)
        {
            if (val < INT16_MIN)
                val = INT16_MIN;
            if (val > INT16_MAX)
                val = INT16_MAX;
        }
        vals[idx] = val;
    }
    matrix.t[0] = (uint32)matrix.t[0] << shift_bits;
    matrix.t[1] = (uint32)matrix.t[1] << shift_bits;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    proj = sra(proj_dist, shift);
    gte_write_h(proj);
    return proj;
}

uint32 render_current_proj(const RACE_VIEW *view)
{
    return race_cfg.players == 2 ? 200u : game_timing.base_rate == game_timing.frame_rate ? 290u - render_proj_speed[view->idx] : 290u;
}

uint32 render_update_proj(const RACE_VIEW *view)
{
    if (race_paused)
        return render_current_proj(view);
    sint32 speed = render_proj_speed[view->idx];
    sint32 target = (sint32)(3u * boat_player(view)->velocity.speed) / 256;
    speed -= (speed - target) >> 5;
    if (speed > 130)
        speed = 130;
    render_proj_speed[view->idx] = speed;
    return render_current_proj(view);
}

void render_set_proj_dist(uint32 proj)
{
    FUNCTION_MARKER(0x80016310, "MAIN.EXE");
    gte_write_h(proj);
    proj_dist = proj;
}

void render_set_geom_offset(sint32 x, sint32 y)
{
    FUNCTION_MARKER(0x80016320, "MAIN.EXE");
    SetGeomOffset(x, y);
    proj_offset[0] = x;
    proj_offset[1] = y;
}

// Native packet pools own their storage and write cursors
typedef struct
{
    uint8 *data;
    uint32 offset;
    uint32 capacity;
} RENDER_PACKET_POOL;

static RENDER_PACKET_POOL packet_pools[2];
static uint32 packet_storage[2][0x20000u / sizeof(uint32)];
static uint32 packet_buffer;

static void render_packet_pool_init(uint32 capacity)
{
    uint32 idx;
    if (capacity > 0x20000 || (capacity & 3) != 0)
        abort();
    for (idx = 0; idx < 2; ++idx)
    {
        RENDER_PACKET_POOL *pool = &packet_pools[idx];
        if (!pool->data)
        {
            pool->data = (uint8 *)packet_storage[idx];
            if (!gpu_register_packet_range(pool->data, sizeof(packet_storage[idx])))
                abort();
        }
        pool->offset = 0;
        pool->capacity = capacity;
    }
    packet_buffer = 0;
}

static void render_packet_pool_select(sint32 alternate)
{
    packet_buffer = alternate != 0;
    if (!packet_pools[packet_buffer].data)
        abort();
}

uint32 render_packet_offset(void)
{
    const RENDER_PACKET_POOL *pool = &packet_pools[packet_buffer];
    uint32 offset = pool->offset;
    if (!pool->data || offset > pool->capacity || (offset & 3) != 0)
        abort();
    return offset;
}

void *render_packet_at(uint32 offset, uint32 size)
{
    RENDER_PACKET_POOL *pool = &packet_pools[packet_buffer];
    if (!pool->data || offset > pool->capacity || size > pool->capacity - offset || (offset & 3) != 0)
        abort();
    return pool->data + offset;
}

void render_packet_publish(uint32 offset)
{
    RENDER_PACKET_POOL *pool = &packet_pools[packet_buffer];
    if (!pool->data || offset > pool->capacity || (offset & 3) != 0)
        abort();
    pool->offset = offset;
}

void render_init_packet_pools(void)
{
    uint32 size = race_selection.special != 0 ? 0x20000u : race_cfg.players == 2 ? 0x16000u : 0x10000u;

    FUNCTION_MARKER(0x80016340, "MAIN.EXE");
    render_packet_pool_init(size);
}

void render_select_prim_pool(sint32 alternate)
{
    FUNCTION_MARKER(0x800163B0, "MAIN.EXE");
    render_packet_pool_select(alternate);
    render_packet_publish(0);
}

// Transient scene billboard owner from MAIN.EXE 800B7680/800B3E14
RENDER_BILLBOARD_LIST render_billboards;

sint32 render_add_billboard(sint32 x, sint32 y, sint32 z, uint16 type)
{
    RENDER_BILLBOARD *entry;
    if (render_billboards.count >= 64)
        return 0;
    entry = &render_billboards.entries[render_billboards.count++];
    entry->x = x;
    entry->y = y;
    entry->z = z;
    entry->type = type;
    return 1;
}

void render_generate_entries(const sint32 transform[3], const ROUTE_SEGMENT *seg, sint32 remaining)
{
    sint32 count;

    FUNCTION_MARKER(0x80018F48, "MAIN.EXE");
    while (remaining != 0)
    {
        sint32 idx;
        sint32 vals[4];

        count = render_billboards.count;
        if (count >= 64)
            return;
        for (idx = 0; idx < seg->vertex_count; ++idx)
        {
            const ROUTE_VERTEX *vtx = &seg->vertices[idx];
            uint8 type = vtx->flags >> 5;

            if (type == 0)
                continue;
            if (render_billboards.count >= 64)
                return;
            route_sample_geom(seg, idx, 1, vals);
            type = vtx->flags >> 5;
            render_add_billboard(vals[0] + transform[0], vals[1] + transform[1], vals[2] + transform[2], type);
        }
        --remaining;
        seg = seg->next.seg;
    }
}

// Billboard material owners from MAIN.EXE 800D6A48/800F2508/800DCC30
static RENDER_MATERIAL billboard_animation[16];
static RENDER_MATERIAL billboard_images[2];

static void render_billboard_rect(RENDER_MATERIAL *material, sint32 x, sint32 y, sint32 clut_x, sint32 clut_y, uint8 u, uint8 v)
{
    material->tpage = GetTPage(0, 1, x, y);
    material->clut = GetClut(clut_x, clut_y);
    material->uv[0].u = material->uv[2].u = u;
    material->uv[1].u = material->uv[3].u = u + 31u;
    material->uv[0].v = material->uv[1].v = v;
    material->uv[2].v = material->uv[3].v = v + 31u;
}

const RENDER_MATERIAL *render_billboard_tex(sint32 type, uint32 animation, uint16 *clut)
{
    const RENDER_MATERIAL *material;
    if (type <= 0 || type >= 16 || animation >= 16)
        abort();
    if (type <= 2)
    {
        material = &billboard_images[type - 1];
        *clut = material->clut;
    }
    else
    {
        material = &billboard_animation[animation];
        *clut = billboard_animation[type].clut;
    }
    return material;
}

sint32 render_init_billboards(void)
{
    sint32 x = 816, y = 256, pal_y = 503;
    uint32 idx;
    FUNCTION_MARKER(0x8001909C, "MAIN.EXE");
    for (idx = 0; idx < 16; ++idx)
    {
        render_billboard_rect(&billboard_animation[idx], x, y, 784, pal_y, x % 64 * 4, y % 256);
        y += 32;
        ++pal_y;
        if (y == 512)
        {
            y = 256;
            x += 8;
        }
    }
    pickup_reset();
    render_billboard_rect(&billboard_images[1], 808, 448, 800, 481, 160, 192);
    render_billboard_rect(&billboard_images[0], 800, 448, 800, 480, 128, 192);
    return 159;
}

void render_billboard(uint32 *ot, const RENDER_BILLBOARD *entries, sint32 count, sint32 lower)
{
    MATRIX saved;
    MATRIX identity = {0};
    uint32 packet;
    uint32 animation;
    uint32 animation_phase;
    sint32 width3;
    sint32 height3;
    sint32 idx;

    FUNCTION_MARKER(0x80019340, "MAIN.EXE");
    if (race_cfg.mode == 8)
    {
        sint32 phase = game_timing.ticks & 0x7Fu;

        if (phase >= 64u)
            phase = 127u - phase;
        width3 = phase + 64;
        height3 = (phase * 4 + 256) / 3;
    }
    else
    {
        width3 = 64;
        height3 = 84;
    }
    animation_phase = game_timing.ticks;
    packet = render_packet_offset();
    animation = (animation_phase >> 2) % 15;
    ReadRotMatrix(&saved);
    SetLightMatrix(&saved);
    gte_set_back_color_raw(saved.t[0], saved.t[1], saved.t[2]);
    identity.m[0][0] = ONE;
    identity.m[1][1] = ONE;
    identity.m[2][2] = ONE;
    SetRotMatrix(&identity);
    SetTransMatrix(&identity);
    for (idx = 0; idx < count; ++idx)
    {
        sint32 type = (sint16)entries[idx].type;
        sint32 half_width;
        sint32 height;
        SVECTOR center;
        VECTOR t;
        SVECTOR vertices[4];
        sint32 proj[4];
        sint32 depth[3];
        sint32 flags;
        sint32 vertex_flags;
        sint32 avgZ;
        const RENDER_MATERIAL *tex;
        uint16 clut;
        uint32 *bucket;
        POLY_FT4 *prim;

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
        if (race_cfg.players == 2)
            height /= 2;
        center.vx = entries[idx].x;
        center.vy = entries[idx].y;
        center.vz = entries[idx].z;
        center.pad = 0;
        gte_transform_light(&center, &t, &vertex_flags);
        vertices[0].vx = t.vx - half_width;
        vertices[0].vy = t.vy + (lower ? height : -height);
        vertices[0].vz = t.vz;
        vertices[0].pad = 0;
        vertices[1].vx = t.vx + half_width;
        vertices[1].vy = vertices[0].vy;
        vertices[1].vz = vertices[0].vz;
        vertices[1].pad = 0;
        vertices[2].vx = vertices[0].vx;
        vertices[2].vy = t.vy;
        vertices[2].vz = vertices[0].vz;
        vertices[2].pad = 0;
        vertices[3].vx = vertices[1].vx;
        vertices[3].vy = vertices[2].vy;
        vertices[3].vz = vertices[0].vz;
        vertices[3].pad = 0;
        gte_project3_full_depth(vertices, proj, depth, &flags);
        if (flags < 0)
            continue;
        prim = render_packet_at(packet, sizeof(*prim));
        prim->p[0].xy.xy = proj[0];
        prim->p[1].xy.xy = proj[1];
        prim->p[2].xy.xy = proj[2];
        avgZ = AverageZ3(depth[0], depth[1], depth[2]);
        --avgZ;
        if (avgZ < 0 || avgZ >= 2000)
            continue;
        avgZ >>= 3;
        gte_project(&vertices[3], &proj[3], &vertex_flags);
        prim->p[3].xy.xy = proj[3];
        type = (sint16)entries[idx].type;
        tex = render_billboard_tex(type, animation, &clut);
        prim->p[1].attr = tex->tpage;
        prim->p[0].attr = clut;
        setRGB0(prim, lower ? 48 : 128, lower ? 48 : 128, lower ? 69 : 128);
        prim->rgbc.code = 0x2C;
        setUV4(prim, tex->uv[0].u, tex->uv[0].v, tex->uv[1].u, tex->uv[1].v, tex->uv[2].u, tex->uv[2].v, tex->uv[3].u, tex->uv[3].v);

        bucket = ot + avgZ;
        prim->tag = *bucket & 0xFF000000;
        setlen(prim, getlen(prim) | 9);
        AddPrim(bucket, prim);
        packet += sizeof(*prim);
    }
    SetRotMatrix(&saved);
    SetTransMatrix(&saved);
    render_packet_publish(packet);
}

/* Implements each RTPT group from MAIN.EXE:0x800198AC */

sint32 render_dispatch_prim_grps(const OBJECT_GROUP *object, uint32 table, sint32 adjusted, uint32 *ot)
{
    const OBJECT_GROUP_ENTRY *entries;
    sint32 entry_idx;
    sint32 result;

    FUNCTION_MARKER(0x8001DA94, "MAIN.EXE");
    if (object->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    render_set_ot(ot, 100);
    result = object->capacity;
    entries = object->slots;
    if (result == 0)
        return result;
    entry_idx = 0;
    do
    {
        if (entries->status == 2)
        {
            uint32 item = entries->section;
            SECTION *section = track_section(item);
            if (table >= 2)
                abort();
            for (uint32 list = 0; list < 2; ++list)
                for (TRACK_GROUP_NODE *node = section->grps[list]; node; node = node->next)
                    poly_dispatch_prim_grps(node->grp, table, ot, adjusted, list);
        }
        ++entry_idx;
        result = entry_idx < object->capacity;
        ++entries;
    } while (result != 0);
    return result;
}

void render_init_hud_frame(RENDER_FRAME *frame, HUD_LAYOUT layout, sint32 buf)
{
    hud_init_prims(&frame->hud, frame->hud_prims, frame->ui_modes, hud_sprs(layout), 38, 19, 19, buf);
}
