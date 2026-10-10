#include "pickup.h"
#include "scene.h"
#include "ai.h"
#include "game.h"
#include "vehicle.h"
#include "menu.h"
#include "route.h"
#include "text.h"
#include "polygon.h"
#include "psx_gpu.h"
#include "render.h"
#include "input.h"
#include "global.h"
#include "motion.h"
#include <stdlib.h>
#include <stdio.h>

uint32 render_capture_enabled;
sint32 render_proj_speed[2];

typedef struct
{
    uint8 u, v;
    uint16 w, h;
} HUD_METRIC;

struct HUD_FONT
{
    const HUD_METRIC *metrics;
    const char *chars;
    uint16 count, glyph;
};


// Mutable HUD sprite templates from MAIN.EXE 8009365C/8009422C/80094BF4
static HUD_SPRITE render_hud_templates[3][38] = {
    {
        {44, 456, 894, 339, 2, 9, 832, 424, 0},
        {32, 450, 893, 326, 3, 13, 832, 424, 0},
        {21, 434, 892, 256, 4, 18, 832, 424, 0},
        {16, 426, 877, 352, 4, 18, 832, 413, 0},
        {63, 463, 892, 339, 2, 11, 832, 416, 0},
        {54, 465, 890, 334, 2, 14, 832, 416, 0},
        {42, 468, 890, 319, 3, 15, 832, 416, 0},
        {31, 466, 893, 311, 3, 15, 832, 416, 0},
        {17, 453, 892, 274, 4, 19, 832, 416, 0},
        {16, 426, 881, 397, 15, 58, 832, 431, 1},
        {262, 47, 881, 455, 10, 33, 832, 430, 1},
        {32, 52, 877, 360, 12, 27, 832, 415, 1},
        {0, 0, 849, 413, 32, 74, 832, 426, 0},
        {0, 0, 960, 361, 57, 31, 832, 427, 0},
        {0, 0, 832, 256, 45, 157, 832, 428, 0},
        {0, 0, 832, 256, 45, 157, 832, 428, 0},
        {303, 428, 877, 319, 13, 41, 832, 420, 1},
        {300, 0, 873, 488, 22, 12, 832, 421, 0},
        {300, 421, 877, 256, 15, 63, 832, 422, 1},
        {70, 90, 704, 331, 23, 35, 752, 454, 2},
        {70, 140, 712, 439, 22, 35, 704, 475, 2},
        {70, 190, 734, 440, 34, 35, 704, 475, 2},
        {70, 240, 704, 296, 35, 35, 704, 475, 2},
        {70, 290, 704, 406, 31, 33, 704, 475, 2},
        {70, 340, 745, 256, 13, 40, 704, 475, 2},
        {210, 195, 735, 411, 31, 29, 704, 475, 2},
        {214, 245, 735, 411, 31, 29, 704, 475, 2},
        {126, 342, 727, 331, 11, 35, 720, 475, 3},
        {126, 342, 960, 480, 9, 30, 720, 475, 3},
        {170, 140, 704, 366, 34, 40, 704, 475, 2},
        {170, 140, 739, 296, 27, 35, 704, 475, 0},
        {170, 140, 738, 331, 29, 40, 704, 475, 0},
        {170, 140, 704, 256, 41, 40, 704, 475, 0},
        {170, 140, 738, 371, 28, 40, 704, 475, 0},
        {170, 140, 988, 475, 24, 35, 704, 475, 0},
        {170, 140, 969, 475, 19, 35, 704, 475, 0},
        {240, 204, 976, 411, 20, 11, 976, 422, 2},
        {244, 254, 976, 411, 20, 11, 976, 422, 2},
    },
    {
        {76, 103, 874, 256, 3, 7, 880, 263, 0},
        {55, 102, 870, 256, 4, 10, 880, 263, 0},
        {36, 93, 864, 256, 6, 13, 880, 263, 0},
        {30, 86, 854, 267, 6, 13, 880, 267, 0},
        {77, 104, 870, 413, 2, 11, 752, 449, 0},
        {68, 105, 868, 413, 2, 14, 752, 449, 0},
        {56, 107, 877, 290, 3, 15, 752, 449, 0},
        {45, 106, 877, 275, 3, 15, 752, 449, 0},
        {31, 99, 877, 256, 4, 19, 752, 449, 0},
        {30, 86, 850, 432, 18, 29, 880, 266, 1},
        {33, 19, 864, 270, 11, 14, 880, 273, 1},
        {33, 36, 859, 419, 13, 12, 880, 327, 1},
        {0, 0, 832, 284, 44, 40, 880, 268, 0},
        {0, 0, 832, 256, 32, 28, 880, 269, 0},
        {0, 0, 832, 324, 48, 95, 880, 270, 0},
        {0, 0, 832, 324, 48, 95, 880, 270, 0},
        {434, 90, 880, 280, 16, 19, 880, 261, 1},
        {300, 0, 873, 488, 22, 12, 0, 0, 0},
        {432, 86, 832, 432, 18, 29, 880, 260, 1},
        {120, 20, 859, 461, 18, 18, 880, 272, 2},
        {120, 35, 878, 491, 17, 18, 880, 331, 2},
        {120, 50, 832, 458, 27, 18, 880, 331, 2},
        {120, 65, 832, 440, 27, 18, 880, 331, 2},
        {120, 80, 832, 476, 24, 17, 880, 331, 2},
        {120, 95, 859, 440, 9, 20, 880, 331, 2},
        {262, 58, 880, 319, 14, 4, 880, 330, 2},
        {262, 73, 880, 319, 14, 4, 880, 330, 2},
        {0, 0, 887, 299, 12, 35, 880, 331, 0},
        {0, 0, 880, 299, 7, 16, 880, 331, 0},
        {230, 35, 864, 392, 28, 20, 880, 331, 2},
        {230, 35, 856, 479, 21, 18, 880, 331, 0},
        {230, 35, 872, 441, 23, 20, 880, 331, 0},
        {230, 35, 832, 408, 22, 20, 880, 331, 0},
        {230, 35, 863, 421, 32, 20, 880, 331, 0},
        {230, 35, 832, 493, 18, 18, 880, 331, 0},
        {230, 35, 832, 391, 25, 17, 880, 331, 0},
        {250, 52, 854, 498, 20, 12, 880, 331, 2},
        {250, 67, 854, 498, 20, 12, 880, 331, 2},
    },
    {
        {72, 207, 874, 256, 3, 7, 880, 263, 0},
        {51, 206, 870, 256, 4, 10, 880, 263, 0},
        {32, 197, 864, 256, 6, 13, 880, 263, 0},
        {30, 86, 854, 267, 6, 13, 880, 267, 0},
        {77, 104, 870, 413, 2, 11, 752, 449, 0},
        {68, 105, 868, 413, 2, 14, 752, 449, 0},
        {56, 107, 877, 290, 3, 15, 752, 449, 0},
        {45, 106, 877, 275, 3, 15, 752, 449, 0},
        {31, 99, 877, 256, 4, 19, 752, 449, 0},
        {26, 190, 850, 432, 18, 29, 880, 266, 1},
        {26, 19, 864, 270, 11, 14, 880, 273, 1},
        {26, 34, 859, 419, 13, 12, 880, 327, 1},
        {0, 0, 832, 284, 44, 40, 880, 268, 0},
        {0, 0, 832, 256, 32, 28, 880, 269, 0},
        {0, 0, 832, 324, 48, 95, 880, 270, 0},
        {0, 0, 832, 324, 48, 95, 880, 270, 0},
        {177, 194, 880, 280, 16, 19, 880, 261, 1},
        {300, 0, 873, 488, 22, 12, 0, 0, 0},
        {175, 190, 832, 432, 18, 29, 880, 260, 1},
        {40, 60, 859, 461, 18, 18, 880, 272, 2},
        {40, 80, 878, 491, 17, 18, 880, 331, 2},
        {40, 100, 832, 458, 27, 18, 880, 331, 2},
        {40, 120, 832, 440, 27, 18, 880, 331, 2},
        {40, 140, 832, 476, 24, 17, 880, 331, 2},
        {40, 160, 859, 440, 9, 20, 880, 331, 2},
        {280, 60, 854, 498, 20, 12, 880, 331, 2},
        {280, 75, 854, 498, 20, 12, 880, 331, 2},
        {0, 0, 887, 299, 12, 35, 880, 331, 0},
        {0, 0, 880, 299, 7, 16, 880, 331, 0},
        {120, 80, 864, 392, 28, 20, 880, 331, 2},
        {120, 80, 856, 479, 21, 18, 880, 331, 0},
        {120, 80, 872, 441, 23, 20, 880, 331, 0},
        {120, 80, 832, 408, 22, 20, 880, 331, 0},
        {120, 80, 863, 421, 32, 20, 880, 331, 0},
        {120, 80, 832, 493, 18, 18, 880, 331, 0},
        {120, 80, 832, 391, 25, 17, 880, 331, 0},
        {283, 63, 880, 319, 14, 4, 880, 330, 2},
        {283, 78, 880, 319, 14, 4, 880, 330, 2},
    },
};

const HUD_SPRITE *render_hud_geom;

HUD_SPRITE *render_hud_sprites(HUD_LAYOUT layout)
{
    if ((uint32)layout > HUD_LAYOUT_VERTICAL)
        abort();
    return render_hud_templates[layout];
}

static const HUD_METRIC render_hud_metrics_0[] = {
    {0,2,16,34},
    {16,2,24,34},
    {40,2,24,34},
    {64,2,24,34},
    {88,2,24,34},
    {0,39,24,34},
    {24,39,24,34},
    {48,39,24,34},
    {76,39,24,34},
    {104,39,24,34},
    {116,2,12,34}
};
static const HUD_METRIC render_hud_metrics_1[] = {
    {1,1,12,29},
    {15,1,21,29},
    {37,1,20,29},
    {60,1,20,29},
    {82,1,21,29},
    {105,1,21,29},
    {127,1,19,29},
    {147,1,21,29},
    {169,1,20,29},
    {191,1,21,29},
    {213,1,11,29}
};
static const HUD_METRIC render_hud_metrics_2[] = {
    {0,0,10,19},
    {12,0,16,19},
    {28,0,16,19},
    {44,0,16,19},
    {60,0,16,19},
    {76,0,16,19},
    {92,0,16,19},
    {108,0,16,19},
    {124,0,16,19},
    {140,0,16,19},
    {156,0,8,19},
    {0,20,8,22},
    {8,19,12,19},
    {20,19,14,19},
    {36,19,15,19},
    {52,19,12,19},
    {64,19,16,19},
    {80,19,15,19},
    {96,19,13,19},
    {112,19,16,24},
    {128,19,16,19},
    {144,19,11,19},
    {156,19,14,23},
    {0,42,15,19},
    {16,42,10,19},
    {28,42,20,19},
    {48,42,15,19},
    {64,42,15,19},
    {80,42,16,23},
    {96,42,15,23},
    {112,42,12,19},
    {124,42,14,19},
    {140,42,12,19},
    {0,65,15,19},
    {16,65,13,19},
    {32,65,18,19},
    {52,65,16,19},
    {68,65,16,23},
    {84,65,15,19},
    {160,135,16,19},
    {32,64,14,2},
    {132,135,10,19},
    {144,135,8,19}
};
static const HUD_METRIC render_hud_metrics_3[] = {
    {0,1,20,18},
    {20,1,36,18},
    {56,1,32,18},
    {88,1,32,18},
    {120,1,32,18},
    {0,21,32,18},
    {32,21,32,18},
    {64,21,36,18},
    {100,21,32,18},
    {132,21,36,18},
    {152,1,16,18}
};
static const HUD_METRIC render_hud_metrics_4[] = {
    {1,1,12,29},
    {15,1,21,29},
    {37,1,20,29},
    {60,1,20,29},
    {82,1,21,29},
    {105,1,21,29},
    {127,1,19,29},
    {147,1,21,29},
    {169,1,20,29},
    {191,1,21,29},
    {213,1,11,29}
};
static const HUD_METRIC render_hud_metrics_5[] = {
    {0,1,12,11},
    {12,1,18,11},
    {32,1,18,11},
    {52,1,18,11},
    {72,1,18,11},
    {92,1,19,11},
    {172,1,17,11},
    {112,1,18,11},
    {132,1,18,11},
    {152,1,19,11},
    {176,27,10,11},
    {0,13,10,13},
    {12,13,12,11},
    {24,13,16,11},
    {40,13,16,11},
    {56,13,16,11},
    {72,13,16,11},
    {88,13,16,11},
    {106,13,15,11},
    {120,13,16,14},
    {136,13,16,11},
    {152,13,12,11},
    {0,27,16,14},
    {16,27,15,11},
    {32,27,11,11},
    {44,27,22,11},
    {68,27,16,11},
    {84,27,16,11},
    {100,27,16,14},
    {116,27,16,14},
    {132,27,15,11},
    {148,27,15,11},
    {164,27,12,11},
    {0,41,16,11},
    {16,41,16,11},
    {32,41,20,11},
    {52,41,16,11},
    {68,41,16,14},
    {84,41,16,11},
    {176,82,16,11},
    {136,27,13,2},
    {124,82,11,11},
    {136,82,10,11}
};
static const HUD_FONT render_hud_fonts[6]={
{render_hud_metrics_0,"1234567890:",11,12},
{render_hud_metrics_1,"1234567890/",11,13},
{render_hud_metrics_2,"1234567890:;#abcdefghijklmnopqrstuvwxyz/ +-",43,15},
{render_hud_metrics_3,"1234567890:",11,12},
{render_hud_metrics_4,"1234567890/",11,13},
{render_hud_metrics_5,"1234567890:;#abcdefghijklmnopqrstuvwxyz/ +-",43,15},
};

// Mutable lighting sets from MAIN.EXE 80089580/800895B8/800914CC
static RENDER_LIGHTING render_default_lighting[3] = {
    {{{{-4782, 1353, 2345}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{1704, 3060, 0}, {3060, 2376, 0}, {2616, 1248, 0}}, {124, 90, 127}}},
    {{{{-4782, 1353, 2345}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{852, 1530, 0}, {1530, 1188, 0}, {1308, 624, 0}}, {62, 45, 64}}},
    {{{{1313, 1398, 3615}, 0}, {{3749, 4161, 1964}, 0}, {{0, 0, 0}, 0}}, {{{1704, 3060, 0}, {3060, 2376, 0}, {2616, 1248, 0}}, {124, 90, 127}}}};

// The original root starts at zero-filled memory before a course is loaded
static RENDER_LIGHTING render_initial_lighting[256];
static RENDER_LIGHTING render_archive_lighting[256];
static RENDER_LIGHTING *render_active_lighting = render_initial_lighting;
static uint32 render_lighting_count = 256u;

RENDER_LIGHTING *render_lighting(uint32 index)
{
    if (index >= render_lighting_count)
        abort();
    return &render_active_lighting[index];
}

void render_select_lighting(RENDER_LIGHT_SET set)
{
    if ((uint32)set >= 3u)
        abort();
    render_active_lighting = &render_default_lighting[set];
    render_lighting_count = 1u;
}

void render_load_lighting(const uint8 *src, size_t size)
{
    uint32 count;
    uint32 index;

    // Disk entries contain three eight-byte lights followed by a PsyQ matrix
    if (!src || size % 56u != 0u || size / 56u > 256u)
        abort();
    count = (uint32)(size / 56u);
    if (count == 0u || count > 256u)
        abort();
    for (index = 0u; index < count; ++index)
    {
        RENDER_LIGHTING *entry = &render_archive_lighting[index];
        uint32 row;
        uint32 axis;

        for (row = 0u; row < 3u; ++row)
        {
            for (axis = 0u; axis < 3u; ++axis)
            {
                entry->lights[row].vector[axis] = (sint16)xport_load_le16(src + row * 8u + axis * 2u);
                entry->color.m[row][axis] = (sint16)xport_load_le16(src + 24u + row * 6u + axis * 2u);
            }
            entry->lights[row].scale = (sint16)xport_load_le16(src + row * 8u + 6u);
            entry->color.t[row] = (sint32)xport_load_le32(src + 44u + row * 4u);
        }
        src += 56u;
    }
    render_active_lighting = render_archive_lighting;
    render_lighting_count = count;
}

// Palette selectors are uint16 word offsets followed by at most fourteen colors
enum {RENDER_PALETTE_CAPACITY = 65536 + 16};
typedef struct
{
    const uint32 *colors;
    uint32 count;
} RENDER_PALETTE;

// Immutable defaults from MAIN.EXE 80089538/8008955C
static const uint32 render_default_palettes[2][9] = {
    {0x3C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u, 0x2C808080u},
    {0x3C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u, 0x2C323232u}};

static uint32 render_initial_palette[RENDER_PALETTE_CAPACITY];
static uint32 render_archive_palette[RENDER_PALETTE_CAPACITY];
static uint32 render_generated_palette[RENDER_PALETTE_CAPACITY];
static RENDER_PALETTE render_active_palette = {render_initial_palette, RENDER_PALETTE_CAPACITY};
static RENDER_PALETTE render_frame_palette = {render_initial_palette, RENDER_PALETTE_CAPACITY};

const uint32 *render_palette(uint32 index)
{
    if (index >= render_frame_palette.count)
        abort();
    return &render_frame_palette.colors[index];
}

void render_publish_palette(void)
{
    render_frame_palette = render_active_palette;
}

void render_select_palette(sint32 mirror)
{
    if ((uint32)mirror > 1u)
        abort();
    render_active_palette.colors = render_default_palettes[mirror];
    render_active_palette.count = 9u;
}

void render_load_palette(const uint8 *src, size_t size)
{
    uint32 words = 0u;
    uint32 index;

    // Resource flags are converted to render commands later by scene processing
    for (index = 0u; index < route_resources.count; ++index)
    {
        const ROUTE_SEGMENT *seg = route_resources.segments[index];
        uint32 end = (uint32)seg->palette + 2u;
        uint32 command;

        for (command = 0u; command < 14u; ++command)
        {
            uint8 flags = seg->commands[command];
            if ((flags & 6u) == 6u)
                break;
            if ((flags & 8u) == 0u)
                ++end;
        }
        if (command == 14u || end > RENDER_PALETTE_CAPACITY)
            abort();
        if (end > words)
            words = end;
    }
    if ((!src && words != 0u) || words > size / 4u)
        abort();
    for (index = 0u; index < words; ++index)
        render_archive_palette[index] = xport_load_le32(src + 4u * index);
    render_active_palette.colors = render_archive_palette;
    render_active_palette.count = words;
}

uint32 *render_begin_palette(uint32 segments)
{
    if (segments > RENDER_PALETTE_CAPACITY / 16u)
        abort();
    render_active_palette.colors = render_generated_palette;
    render_active_palette.count = 0u;
    return render_generated_palette;
}

void render_end_palette(uint32 words)
{
    if (words > RENDER_PALETTE_CAPACITY)
        abort();
    render_active_palette.count = words;
}

// Projection state from MAIN.EXE 800B6BDC/800B6AC4/800B6AC8
typedef struct
{
    uint32 distance;
    sint32 offset[2];
} RENDER_PROJECTION;

static RENDER_PROJECTION render_projection;

static BOAT *render_indicator_target;
static sint32 render_indicator_distance;
static uint32 render_indicator_ticks;

// Indicator packet owners from MAIN.EXE 800B7410/800B73F0
static LINE_F3 render_indicator_polylines[2][3];
static LINE_F2 render_indicator_lines[2];
// Original immutable geometry at MAIN.EXE 80080AE4/80080B24
static const SVECTOR render_indicator_vertices[8] = {{-22, 0, 0, 0}, {-48, 0, 0, 0}, {0, 48, 0, 0}, {48, 0, 0, 0}, {22, 0, 0, 0}, {22, -64, 0, 0}, {-22, -64, 0, 0}, {-22, 0, 0, 0}};
static const MATRIX render_indicator_matrix = {{{2048, 0, 0}, {0, 2048, 0}, {0, 0, 4096}}, {0, 90, 0}};

static void render_indicator_points(LINE_F3 *packet, const sint32 points[3])
{
    packet->x0 = (sint16)points[0];
    packet->y0 = (sint16)((uint32)points[0] >> 16);
    packet->x1 = (sint16)points[1];
    packet->y1 = (sint16)((uint32)points[1] >> 16);
    packet->x2 = (sint16)points[2];
    packet->y2 = (sint16)((uint32)points[2] >> 16);
}

// Native player/frame owners retain PsyQ packets only at the GPU boundary
struct RENDER_FRAME
{
    uint32 initialized;
    POLY_FT4 strips[40];
    POLY_F4 edges[20];
    POLY_G4 gradients[10];
    LINE_F2 lines[8];
    DR_TWIN texture_windows[4];
    uint32 object_ot[101];
    uint32 route_ot[251];
    uint32 scene_ot[15];
    DR_MODE ui_modes[2];
    HUD_PRIM hud_prims[HUD_PRIM_CAPACITY];
    HUD_STATE hud;
};

static RENDER_FRAME render_frames[2][2];

RENDER_FRAME *render_frame(const CAMERA_STATE *view, sint32 frame)
{
    sint32 player;
    if (view == &camera_views[0])
        player = 0;
    else if (view == &camera_views[1])
        player = 1;
    else
        abort();
    if (frame < 0 || frame >= 2 || !gpu_register_packet_range(render_frames, sizeof(render_frames)))
        abort();
    return &render_frames[player][frame];
}

static void render_check_frame(const RENDER_FRAME *frame)
{
    uint32 player, buffer;
    for (player = 0; player < 2u; ++player)
        for (buffer = 0; buffer < 2u; ++buffer)
            if (frame == &render_frames[player][buffer])
                return;
    abort();
}

// Shared mutable HUD strings
HUD_TEXT hud_text =
{
    "0:00",
    "1/3",
    "16/16",
    { "0:00:00 ", "0:00:00 ", "0:00:00 ", "0:00:00 " },
    "0:00:00 ",
    "record",
    "best",
    "0:00:00 ",
    "0:00:00 ",
    { "00", "00", "00" },
    "0:00",
    { "paul", "will" },
    "time",
    "extended",
    "wrong way",
    "test",
    "-0:00:00",
};

// Mutable descriptor banks from MAIN.EXE 800839E8/80083D58/80083EC0/80084028/80084190
static HUD_DESC render_hud_single[21] = {
    {0, 202, 36, 0, 1, 27316, NULL, NULL, NULL, NULL},
    {2, 117, 51, 0, 1, 27444, NULL, NULL, NULL, NULL},
    {2, 330, 51, 0, 1, 27444, NULL, NULL, NULL, NULL},
    {2, 30, 90, 1, 1, 27444, NULL, NULL, NULL, NULL},
    {2, 30, 120, 1, 1, 27700, NULL, NULL, NULL, NULL},
    {2, 30, 150, 1, 1, 27764, NULL, NULL, NULL, NULL},
    {2, 30, 180, 1, 1, 27956, NULL, NULL, NULL, NULL},
    {2, 260, 90, 1, 1, 27444, NULL, NULL, NULL, NULL},
    {2, 184, 140, 0, 0, 27828, NULL, NULL, NULL, NULL},
    {2, 184, 160, 0, 0, 27828, NULL, NULL, NULL, NULL},
    {2, 184, 210, 0, 0, 27892, NULL, NULL, NULL, NULL},
    {2, 30, 120, 1, 0, 27444, NULL, NULL, NULL, NULL},
    {2, 260, 82, 1, 0, 27892, NULL, NULL, NULL, NULL},
    {2, 260, 142, 1, 0, 27828, NULL, NULL, NULL, NULL},
    {2, 260, 52, 1, 0, 27892, NULL, NULL, NULL, NULL},
    {2, 260, 112, 1, 0, 27828, NULL, NULL, NULL, NULL},
    {2, 184, 71, 0, 0, 27444, NULL, NULL, NULL, NULL},
    {0, 192, 36, 0, 1, 27316, NULL, NULL, NULL, NULL},
    {0, 96, 416, 0, 1, 27316, NULL, NULL, NULL, NULL},
    {0, 192, 416, 0, 1, 27316, NULL, NULL, NULL, NULL},
    {0, 288, 416, 0, 1, 27316, NULL, NULL, NULL, NULL},
};
static HUD_DESC render_hud_horizontal0[8] = {
    {3, 420, 19, 0, 1, 17207, NULL, NULL, NULL, NULL},
    {5, 115, 36, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 120, 19, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 33, 53, 1, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 256, 53, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 256, 63, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 256, 53, 0, 0, 17783, NULL, NULL, NULL, NULL},
    {5, 256, 83, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC render_hud_horizontal1[8] = {
    {3, 420, 9, 0, 1, 17207, NULL, NULL, NULL, NULL},
    {5, 115, 26, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 120, 9, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 33, 43, 1, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 256, 43, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 256, 53, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 256, 43, 0, 0, 17783, NULL, NULL, NULL, NULL},
    {5, 256, 73, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC render_hud_vertical0[8] = {
    {3, 187, 19, 0, 1, 17207, NULL, NULL, NULL, NULL},
    {5, 102, 34, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 98, 19, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 26, 50, 1, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 128, 50, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 128, 60, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 128, 70, 0, 0, 17783, NULL, NULL, NULL, NULL},
    {5, 128, 88, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC render_hud_vertical1[8] = {
    {3, 187, 19, 0, 1, 17207, NULL, NULL, NULL, NULL},
    {5, 102, 34, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 98, 19, 0, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 26, 50, 1, 1, 17335, NULL, NULL, NULL, NULL},
    {5, 128, 50, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 128, 60, 0, 0, 17719, NULL, NULL, NULL, NULL},
    {5, 128, 70, 0, 0, 17783, NULL, NULL, NULL, NULL},
    {5, 128, 88, 0, 0, 17783, NULL, NULL, NULL, NULL},
};

HUD_DESC *render_hud_desc_bank(HUD_LAYOUT layout, uint32 view)
{
    static uint8 initialized;
    static HUD_DESC *const banks[5] = {render_hud_single, render_hud_horizontal0, render_hud_horizontal1, render_hud_vertical0, render_hud_vertical1};
    uint32 idx;

    if (view >= 2u || layout > HUD_LAYOUT_VERTICAL || (layout == HUD_LAYOUT_SINGLE && view != 0u))
        abort();
    if (!initialized)
    {
        // All layouts share the same string owner
        banks[0][0].text = hud_text.time;
        banks[0][1].text = hud_text.rank;
        banks[0][2].text = hud_text.lap;
        banks[0][3].text = hud_text.laps[0];
        banks[0][4].text = hud_text.laps[1];
        banks[0][5].text = hud_text.laps[2];
        banks[0][6].text = hud_text.laps[3];
        banks[0][7].text = hud_text.best_lap;
        banks[0][8].text = hud_text.time_label;
        banks[0][9].text = hud_text.extended;
        banks[0][10].text = hud_text.wrong_way;
        banks[0][11].text = hud_text.names[0];
        banks[0][12].text = hud_text.record;
        banks[0][13].text = hud_text.best;
        banks[0][14].text = hud_text.record_label;
        banks[0][15].text = hud_text.best_label;
        banks[0][16].text = hud_text.test;
        banks[0][17].text = hud_text.trial_time;
        banks[0][18].text = hud_text.markers[0];
        banks[0][19].text = hud_text.markers[1];
        banks[0][20].text = hud_text.markers[2];
        banks[1][0].text = hud_text.time;
        banks[1][1].text = hud_text.rank;
        banks[1][2].text = hud_text.lap;
        banks[1][3].text = hud_text.names[0];
        banks[1][4].text = hud_text.time_label;
        banks[1][5].text = hud_text.extended;
        banks[1][6].text = hud_text.wrong_way;
        banks[1][7].text = hud_text.delta;
        banks[2][0].text = hud_text.time;
        banks[2][1].text = hud_text.rank;
        banks[2][2].text = hud_text.lap;
        banks[2][3].text = hud_text.names[1];
        banks[2][4].text = hud_text.time_label;
        banks[2][5].text = hud_text.extended;
        banks[2][6].text = hud_text.wrong_way;
        banks[2][7].text = hud_text.delta;
        banks[3][0].text = hud_text.time;
        banks[3][1].text = hud_text.rank;
        banks[3][2].text = hud_text.lap;
        banks[3][3].text = hud_text.names[0];
        banks[3][4].text = hud_text.time_label;
        banks[3][5].text = hud_text.extended;
        banks[3][6].text = hud_text.wrong_way;
        banks[3][7].text = hud_text.delta;
        banks[4][0].text = hud_text.time;
        banks[4][1].text = hud_text.rank;
        banks[4][2].text = hud_text.lap;
        banks[4][3].text = hud_text.names[1];
        banks[4][4].text = hud_text.time_label;
        banks[4][5].text = hud_text.extended;
        banks[4][6].text = hud_text.wrong_way;
        banks[4][7].text = hud_text.delta;
        initialized = 1;
    }
    idx = layout == HUD_LAYOUT_SINGLE ? 0u : 1u + 2u * (layout - 1u) + view;
    return banks[idx];
}

HUD_STATE *render_frame_hud(RENDER_FRAME *frame)
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
    return frame->scene_ot;
}

DR_MODE *render_ui_modes(RENDER_FRAME *frame)
{
    render_check_frame(frame);
    return frame->ui_modes;
}

static void render_project_horizon(sint16 sine, sint16 cosine, const sint16 heights[3], uint32 proj[3])
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
        proj[index] = (uint32)screens[index];
}

sint32 render_init_prims(RENDER_FRAME *buffer)
{
    SCENE_HORIZON *cfg = scene_horizon;
    uint32 index;
    PSX_RECT window;

    FUNCTION_MARKER(0x8001483Cu, "MAIN.EXE");
    render_check_frame(buffer);
    for (index = 0u; index < 40u; ++index)
    {
        POLY_FT4 *packet = &buffer->strips[index];
        setlen(packet, 9);
        packet->code = 0x2D;
        packet->tpage = (uint16)getTPage(0, 0, 896, 256);
        packet->clut = (uint16)getClut(896, 511);
    }
    for (index = 0u; index < 20u; ++index)
    {
        setlen(&buffer->edges[index], 5);
        buffer->edges[index].code = 0x28;
    }
    for (index = 0u; index < 10u; ++index)
    {
        POLY_G4 *packet = &buffer->gradients[index];
        setlen(packet, 8);
        packet->color0 = cfg->colors[2].word;
        packet->color1 = cfg->colors[2].word;
        packet->color2 = cfg->colors[1].word;
        packet->color3 = cfg->colors[1].word;
    }
    if (cfg->tex_width != 64u && cfg->tex_width != 128u && cfg->tex_width != 256u)
    {
        cfg->tex_width = 256u;
    }
    setRECT(&window, 16, 0, 48, 64);
    SetTexWindow(&buffer->texture_windows[0], &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(&buffer->texture_windows[1], &window);
    window.w = (sint16)cfg->tex_width;
    SetTexWindow(&buffer->texture_windows[2], &window);
    setRECT(&window, 0, 0, 256, 256);
    SetTexWindow(&buffer->texture_windows[3], &window);
    for (index = 0u; index < 8u; ++index)
    {
        LINE_F2 *packet = &buffer->lines[index];
        setlen(packet, 3);
        packet->r0 = packet->g0 = packet->b0 = 255;
        packet->code = 0x40;
    }
    buffer->initialized = 1;
    return 0;
}

// Curve owner from MAIN.EXE 800F0508; rows 2/8/10/15 also serve texture and light animation
uint8 render_curve[16][512];

// Original 80083774 curve includes the extra byte read by the third quadrant
static const uint8 render_curve_source[129] = {
    63, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 73, 74, 74, 75, 76, 77, 77, 78, 79, 80, 80, 81, 82, 83, 83, 84, 85, 86, 86, 87, 88, 89, 89, 90, 91, 91, 92, 93, 93, 94, 95, 95, 96, 97, 97, 98, 99, 99, 100, 101, 101, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108, 108, 109, 109, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 115, 116, 116, 117, 117, 118, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 208,
};

sint32 render_build_curve_scaled_lut(void)
{
    sint32 scale;

    FUNCTION_MARKER(0x80014A68u, "MAIN.EXE");
    for (scale = 0; scale < 16; ++scale)
    {
        uint8 *output = render_curve[scale];
        sint32 index;

        for (index = 0; index < 128; ++index)
            output[index] = (uint8)((scale * render_curve_source[index]) >> 5);
        for (index = 0; index < 128; ++index)
            output[128 + index] = (uint8)((scale * render_curve_source[127 - index]) >> 5);
        for (index = 0; index < 128; ++index)
            output[256 + index] = (uint8)((scale * (128 - render_curve_source[1 + index])) >> 5);
        for (index = 0; index < 128; ++index)
            output[384 + index] = (uint8)((scale * (128 - render_curve_source[127 - index])) >> 5);
    }
    return 0;
}

// Original material sources from MAIN.EXE 800922C0/80092520/800925D0/80092430/800928D0/800926D0
typedef struct
{
    uint16 clut_x, clut_y;
    RENDER_TEXCOORD uv[4];
    uint32 surface;
} RENDER_MATERIAL_SOURCE;

static const RENDER_MATERIAL_SOURCE render_material_source_0[23] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 508, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 508, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 509, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 509, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u},
    {768, 507, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 507, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 506, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 506, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 505, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 505, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u},
};

static const RENDER_MATERIAL_SOURCE render_material_source_1[11] = {
    {768, 500, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 510, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 501, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 502, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 509, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 500, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 510, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 501, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 502, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u},
};

static const RENDER_MATERIAL_SOURCE render_material_source_2[16] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 500, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 500, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 501, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 501, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 502, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 502, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u},
};

static const RENDER_MATERIAL_SOURCE render_material_source_3[15] = {
    {768, 510, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 510, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 508, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 508, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 507, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 507, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 505, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 505, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u},
};

static const RENDER_MATERIAL_SOURCE render_material_source_4[32] = {
    {768, 495, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 495, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 510, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 494, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 509, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 494, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 508, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 493, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 507, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 493, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 506, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 492, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 505, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 492, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 504, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u},
    {768, 491, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 503, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 491, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 502, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 490, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 501, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 490, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 500, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 489, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 499, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 489, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 498, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {768, 488, {{16, 0}, {16, 63}, {63, 0}, {63, 63}}, 1u}, {768, 497, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {768, 488, {{16, 63}, {16, 0}, {63, 63}, {63, 0}}, 1u}, {768, 496, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u},
};

static const RENDER_MATERIAL_SOURCE render_material_source_5[32] = {
    {512, 197, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1u}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 195, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1u}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 190, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1u},     {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 198, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1u},     {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 193, {{0, 31}, {0, 0}, {39, 31}, {39, 0}}, 1u},   {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 193, {{0, 0}, {0, 31}, {39, 0}, {39, 31}}, 1u},   {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 193, {{39, 31}, {39, 0}, {0, 31}, {0, 0}}, 1u},   {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 193, {{39, 0}, {39, 31}, {0, 0}, {0, 31}}, 1u},   {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u},
    {512, 196, {{39, 0}, {39, 31}, {0, 0}, {0, 31}}, 1u}, {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 196, {{39, 31}, {39, 0}, {0, 31}, {0, 0}}, 1u}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 191, {{63, 32}, {48, 32}, {63, 63}, {48, 63}}, 1u}, {512, 511, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 191, {{48, 32}, {63, 32}, {48, 63}, {63, 63}}, 1u}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 192, {{40, 31}, {40, 0}, {47, 31}, {47, 0}}, 1u}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 192, {{40, 31}, {40, 0}, {47, 31}, {47, 0}}, 1u}, {512, 199, {{0, 0}, {63, 0}, {0, 63}, {63, 63}}, 0u}, {512, 194, {{48, 0}, {48, 31}, {63, 0}, {63, 31}}, 1u}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u}, {512, 194, {{48, 31}, {48, 0}, {63, 31}, {63, 0}}, 1u}, {512, 509, {{0, 0}, {63, 0}, {63, 0}, {63, 63}}, 0u},
};

static RENDER_MATERIAL render_materials[32];

const RENDER_MATERIAL *render_material(uint32 index)
{
    if (index >= 32u)
        abort();
    return &render_materials[index];
}

sint32 render_init_materials(sint32 group)
{
    const RENDER_MATERIAL_SOURCE *source;
    sint32 count;
    sint32 index;
    uint16 tpage;

    FUNCTION_MARKER(0x80014B7Cu, "MAIN.EXE");
    switch (group)
    {
        case 0:
            source = render_material_source_0;
            count = 23;
            break;
        case 1:
            source = render_material_source_1;
            count = 11;
            break;
        case 2:
            source = render_material_source_2;
            count = 16;
            break;
        case 3:
        case 5:
            source = render_material_source_4;
            count = 32;
            break;
        case 4:
            source = render_material_source_3;
            count = 15;
            break;
        case 6:
            source = render_material_source_5;
            count = 32;
            break;
        default:
            input_stop_exit();
            return 0;
    }
    tpage = GetTPage(0, 0, 768, 256);
    for (index = 0; index < count; ++index)
    {
        RENDER_MATERIAL *material = &render_materials[index];
        uint32 vertex;
        material->tpage = tpage;
        material->clut = GetClut(source[index].clut_x, source[index].clut_y);
        for (vertex = 0; vertex < 4u; ++vertex)
            material->uv[vertex] = source[index].uv[vertex];
        material->surface = source[index].surface;
    }
    return 0;
}

sint32 render_init_ui_prims(void)
{
    uint32 buffer, index;

    FUNCTION_MARKER(0x80015A78u, "MAIN.EXE");
    if (!gpu_register_packet_range(render_indicator_polylines, sizeof(render_indicator_polylines)) || !gpu_register_packet_range(render_indicator_lines, sizeof(render_indicator_lines)))
        abort();
    for (buffer = 0; buffer < 2; ++buffer)
    {
        for (index = 0; index < 3; ++index)
        {
            LINE_F3 *packet = &render_indicator_polylines[buffer][index];
            setlen(packet, 5);
            packet->code = 0x48;
            packet->pad = 0x55555555u;
            packet->r0 = packet->g0 = 255;
            packet->b0 = 16;
        }
        setlen(&render_indicator_lines[buffer], 3);
        render_indicator_lines[buffer].code = 0x40;
        render_indicator_lines[buffer].r0 = render_indicator_lines[buffer].g0 = 255;
        render_indicator_lines[buffer].b0 = 16;
    }
    render_indicator_target = NULL;
    render_indicator_distance = 0;
    render_indicator_ticks = 0;
    return 0;
}

uint32 render_target_indicator(uint32 owner, uint32 *ot, sint32 alternate)
{
    const SVECTOR *vertices = render_indicator_vertices;
    MATRIX matrix = render_indicator_matrix;
    sint32 first[3];
    sint32 second[3];
    sint32 third[3];
    sint32 depths[3];
    sint32 flags;
    LINE_F3 *primitive;
    LINE_F2 *final_primitive;
    BOAT *candidate = NULL;
    const BOAT_MENU *menu;
    sint32 angle = 0;
    sint32 steering = 0;
    uint32 index;

    FUNCTION_MARKER(0x80015B4Cu, "MAIN.EXE");
    if (alternate < 0 || alternate >= 2 || !ot)
        abort();
    primitive = render_indicator_polylines[alternate];
    final_primitive = &render_indicator_lines[alternate];
    if (r_u32(0x80083484u) == 5u)
        return 2;
    if (r_u32(0x80083478u) == 2u)
        return 4;
    menu = vehicle_menu(r_u32(owner + 0x64u));
    if (menu->mode != 4u)
        return 4;
    for (index = (uint32)menu->racer_num + 1u; index < vehicle_racer_count; ++index)
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

        sint32 gap = (sint32)current->route.distance;

        if (candidate)
            gap = (sint32)((uint32)current->route.distance - (uint32)candidate->route.distance);
        if (candidate != current && gap < 2500)
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
    render_indicator_points(&primitive[0], first);
    AddPrim(ot, &primitive[0]);
    gte_project3_full_depth(&vertices[5], third, depths, &flags);
    render_indicator_points(&primitive[1], second);
    AddPrim(ot, &primitive[1]);
    {
        sint32 tail[3] = {second[2], third[0], third[1]};
        render_indicator_points(&primitive[2], tail);
        AddPrim(ot, &primitive[2]);
    }
    final_primitive->x0 = (sint16)third[1];
    final_primitive->y0 = (sint16)((uint32)third[1] >> 16);
    final_primitive->x1 = (sint16)third[2];
    final_primitive->y1 = (sint16)((uint32)third[2] >> 16);
    AddPrim(ot, final_primitive);
    return *ot;
}

// Render order owner from MAIN.EXE 800B6AB4/800B6A08
RENDER_ORDER render_order;

void render_set_order_depth(uint32 *ot, uint32 depth_limit)
{
    FUNCTION_MARKER(0x80016190u, "MAIN.EXE");
    render_order.ot = ot;
    render_order.depth_limit = depth_limit;
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
    projection = math_sra_signed((sint32)render_projection.distance, shift);
    gte_write_h((uint16)projection);
    return projection;
}

void render_set_proj_dist(uint32 projection)
{
    FUNCTION_MARKER(0x80016310u, "MAIN.EXE");
    gte_write_h((uint16)projection);
    render_projection.distance = projection;
}

void render_set_geom_offset(sint32 x, sint32 y)
{
    FUNCTION_MARKER(0x80016320u, "MAIN.EXE");
    SetGeomOffset(x, y);
    render_projection.offset[0] = x;
    render_projection.offset[1] = y;
}

// Native packet pools own their storage and write cursors
typedef struct
{
    uint8 *data;
    uint32 offset;
    uint32 capacity;
} RENDER_PACKET_POOL;

static RENDER_PACKET_POOL render_packet_pools[2];
static uint32 render_packet_storage[2][0x20000u / sizeof(uint32)];
static uint32 render_packet_buffer;

static void render_packet_pool_init(uint32 capacity)
{
    uint32 index;
    if (capacity > 0x20000u || (capacity & 3u) != 0u)
        abort();
    for (index = 0; index < 2u; ++index)
    {
        RENDER_PACKET_POOL *pool = &render_packet_pools[index];
        if (!pool->data)
        {
            pool->data = (uint8 *)render_packet_storage[index];
            if (!gpu_register_packet_range(pool->data, sizeof(render_packet_storage[index])))
                abort();
        }
        pool->offset = 0u;
        pool->capacity = capacity;
    }
    render_packet_buffer = 0u;
}

static void render_packet_pool_select(sint32 alternate)
{
    render_packet_buffer = alternate != 0;
    if (!render_packet_pools[render_packet_buffer].data)
        abort();
}

uint32 render_packet_offset(void)
{
    const RENDER_PACKET_POOL *pool = &render_packet_pools[render_packet_buffer];
    uint32 offset = pool->offset;
    if (!pool->data || offset > pool->capacity || (offset & 3u) != 0u)
        abort();
    return offset;
}

void *render_packet_at(uint32 offset, uint32 size)
{
    RENDER_PACKET_POOL *pool = &render_packet_pools[render_packet_buffer];
    if (!pool->data || offset > pool->capacity || size > pool->capacity - offset || (offset & 3u) != 0u)
        abort();
    return pool->data + offset;
}

void render_packet_publish(uint32 offset)
{
    RENDER_PACKET_POOL *pool = &render_packet_pools[render_packet_buffer];
    if (!pool->data || offset > pool->capacity || (offset & 3u) != 0u)
        abort();
    pool->offset = offset;
}

void render_init_packet_pools(void)
{
    uint32 size = race_selection.special != 0u ? 0x20000u : (r_u32(0x80083478u) == 2u ? 0x16000u : 0x10000u);

    FUNCTION_MARKER(0x80016340u, "MAIN.EXE");
    render_packet_pool_init(size);
}

void render_select_prim_pool(sint32 alternate)
{
    FUNCTION_MARKER(0x800163B0u, "MAIN.EXE");
    render_packet_pool_select(alternate);
    render_packet_publish(0u);
}

// Transient scene billboard owner from MAIN.EXE 800B7680/800B3E14
RENDER_BILLBOARD_LIST render_billboards;

sint32 render_add_billboard(sint32 x, sint32 y, sint32 z, uint16 type)
{
    RENDER_BILLBOARD *entry;
    if (render_billboards.count >= 64u)
        return 0;
    entry = &render_billboards.entries[render_billboards.count++];
    entry->x = (sint16)x;
    entry->y = (sint16)y;
    entry->z = (sint16)z;
    entry->type = type;
    return 1;
}

void render_generate_entries(const sint32 transform[3], const ROUTE_SEGMENT *seg, sint32 remaining)
{
    uint32 count;

    FUNCTION_MARKER_ARGS(0x80018F48u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 3u, XPORT_CALL_HOST_POINTER(transform, 3u * sizeof(*transform)), XPORT_CALL_HOST_POINTER(seg, sizeof(*seg)), XPORT_CALL_SCALAR((uint32)remaining));
    while (remaining != 0)
    {
        sint32 idx;
        sint32 values[4];

        count = render_billboards.count;
        if ((sint32)count >= 64)
            return;
        for (idx = 0; idx < (sint32)seg->vertex_count; ++idx)
        {
            const ROUTE_VERTEX *vtx = &seg->vertices[idx];
            uint8 type = vtx->flags >> 5;

            if (type == 0u)
                continue;
            if (render_billboards.count >= 64u)
                return;
            route_sample_geom(seg, idx, 1, values);
            type = vtx->flags >> 5;
            render_add_billboard((sint32)((uint32)values[0] + (uint32)transform[0]), (sint32)((uint32)values[1] + (uint32)transform[1]), (sint32)((uint32)values[2] + (uint32)transform[2]), type);
        }
        --remaining;
        seg = seg->next.seg;
    }
}

// Billboard material owners from MAIN.EXE 800D6A48/800F2508/800DCC30
static RENDER_MATERIAL render_billboard_animation[16];
static RENDER_MATERIAL render_billboard_images[2];

static void render_billboard_rect(RENDER_MATERIAL *material, sint32 x, sint32 y, sint32 clut_x, sint32 clut_y, uint8 u, uint8 v)
{
    material->tpage = GetTPage(0, 1, x, y);
    material->clut = GetClut(clut_x, clut_y);
    material->uv[0].u = material->uv[2].u = u;
    material->uv[1].u = material->uv[3].u = (uint8)(u + 31u);
    material->uv[0].v = material->uv[1].v = v;
    material->uv[2].v = material->uv[3].v = (uint8)(v + 31u);
}

static const RENDER_MATERIAL *render_billboard_texture(sint32 type, uint32 animation, uint16 *clut)
{
    const RENDER_MATERIAL *material;
    if (type <= 0 || type >= 16 || animation >= 16u)
        abort();
    if (type <= 2)
    {
        material = &render_billboard_images[type - 1];
        *clut = material->clut;
    }
    else
    {
        material = &render_billboard_animation[animation];
        *clut = render_billboard_animation[type].clut;
    }
    return material;
}

sint32 render_init_billboards(void)
{
    sint32 x = 816, y = 256, palette_y = 503;
    uint32 index;
    FUNCTION_MARKER(0x8001909Cu, "MAIN.EXE");
    for (index = 0; index < 16u; ++index)
    {
        render_billboard_rect(&render_billboard_animation[index], x, y, 784, palette_y, (uint8)((x % 64) * 4), (uint8)(y % 256));
        y += 32;
        ++palette_y;
        if (y == 512)
        {
            y = 256;
            x += 8;
        }
    }
    pickup_reset();
    render_billboard_rect(&render_billboard_images[1], 808, 448, 800, 481, 160, 192);
    render_billboard_rect(&render_billboard_images[0], 800, 448, 800, 480, 128, 192);
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
    sint32 index;

    FUNCTION_MARKER(0x80019340u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 8u)
    {
        uint32 phase = game_timing.ticks & 0x7Fu;

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
    animation_phase = (uint32)game_timing.ticks;
    packet = render_packet_offset();
    animation = (animation_phase >> 2) % 15u;
    ReadRotMatrix(&saved);
    SetLightMatrix(&saved);
    gte_set_back_color_raw(saved.t[0], saved.t[1], saved.t[2]);
    identity.m[0][0] = 4096;
    identity.m[1][1] = 4096;
    identity.m[2][2] = 4096;
    SetRotMatrix(&identity);
    SetTransMatrix(&identity);
    for (index = 0; index < count; ++index)
    {
        sint32 type = (sint16)entries[index].type;
        sint32 half_width;
        sint32 height;
        SVECTOR center;
        VECTOR transformed;
        SVECTOR vertices[4];
        sint32 proj[4];
        sint32 depths[3];
        sint32 flags;
        sint32 vertex_flags;
        sint32 depth;
        const RENDER_MATERIAL *texture;
        uint16 clut;
        uint32 *bucket;
        POLY_FT4 *primitive;

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
        center.vx = entries[index].x;
        center.vy = entries[index].y;
        center.vz = entries[index].z;
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
        gte_project3_full_depth(vertices, proj, depths, &flags);
        if (flags < 0)
            continue;
        primitive = render_packet_at(packet, sizeof(*primitive));
        primitive->x0 = (sint16)proj[0];
        primitive->y0 = (sint16)((uint32)proj[0] >> 16);
        primitive->x1 = (sint16)proj[1];
        primitive->y1 = (sint16)((uint32)proj[1] >> 16);
        primitive->x2 = (sint16)proj[2];
        primitive->y2 = (sint16)((uint32)proj[2] >> 16);
        depth = AverageZ3(depths[0], depths[1], depths[2]);
        --depth;
        if ((uint32)depth >= 2000u)
            continue;
        depth = (sint32)((uint32)depth >> 3);
        gte_project(&vertices[3], &proj[3], &vertex_flags);
        primitive->x3 = (sint16)proj[3];
        primitive->y3 = (sint16)((uint32)proj[3] >> 16);
        type = (sint16)entries[index].type;
        texture = render_billboard_texture(type, animation, &clut);
        primitive->tpage = texture->tpage;
        primitive->clut = clut;
        primitive->r0 = primitive->g0 = lower ? 48 : 128;
        primitive->b0 = lower ? 69 : 128;
        primitive->code = 0x2C;
        primitive->u0 = texture->uv[0].u;
        primitive->v0 = texture->uv[0].v;
        primitive->u1 = texture->uv[1].u;
        primitive->v1 = texture->uv[1].v;
        primitive->u2 = texture->uv[2].u;
        primitive->v2 = texture->uv[2].v;
        primitive->u3 = texture->uv[3].u;
        primitive->v3 = texture->uv[3].v;
        bucket = ot + (uint32)depth;
        primitive->tag = (*bucket & 0xFF000000u) | 0x09000000u;
        AddPrim(bucket, primitive);
        packet += sizeof(*primitive);
    }
    SetRotMatrix(&saved);
    SetTransMatrix(&saved);
    render_packet_publish(packet);
}

/* Implements each RTPT group from MAIN.EXE:0x800198AC */
uint32 render_horizon(RENDER_FRAME *buffer, const CAMERA_STATE *view)
{
    POLY_FT4 *strip_pool;
    POLY_F4 *edge_pool;
    POLY_G4 *triangle_pool;
    const SCENE_HORIZON *cfg = scene_horizon;
    sint16 heights[12] = {0};
    uint32 proj_a[9] = {0};
    uint32 proj_b[9] = {0};
    uint32 *current = proj_a;
    uint32 *next = proj_b;
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
    uint32 *scene_ot = render_scene_ot(buffer);

    FUNCTION_MARKER(0x800198ACu, "MAIN.EXE");
    render_check_frame(buffer);
    if (!buffer->initialized)
        abort();
    strip_pool = buffer->strips;
    edge_pool = buffer->edges;
    triangle_pool = buffer->gradients;
    {
        sint32 first = view->rotation.m[2][2];
        sint32 second = view->rotation.m[0][2];

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
        if (cfg->alternating != 0u)
            position = -position;
        else
            position += 683;
    }
    shade = 255 * position / 683;
    if (cfg->alternating != 0u)
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
    if (cfg->strips > 4u)
        abort();
    heights[0] = (sint16)cfg->ground;
    height_count = 1;
    if (cfg->edge != 0u)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)cfg->edge_step);
        ++height_count;
    }
    for (group = 0; group < (sint32)cfg->strips; ++group)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)cfg->strip_step);
        ++height_count;
    }
    if (cfg->gradient != 0u)
    {
        heights[height_count] = (sint16)(heights[height_count - 1] - (sint16)cfg->gradient_step);
        ++height_count;
        heights[height_count] = (sint16)(heights[height_count - 1] + (sint16)cfg->top_step);
        ++height_count;
    }
    else
    {
        heights[height_count] = (sint16)(heights[height_count - 1] - (sint16)cfg->gradient_step);
        ++height_count;
    }
    if (height_count > 9 || ring_count * cfg->strips > 40 || ring_count * (1 + (cfg->edge != 0u)) > 20 || ring_count > 10)
        abort();
    group_count = (height_count + 2) / 3;
    radius_scale = r_u32(0x80083484u) == 8u ? 1 : 2;
    for (group = 0; group < group_count; ++group)
    {
        sint16 group_heights[3] = {heights[group * 3], heights[group * 3 + 1], heights[group * 3 + 2]};
        sint16 sine = (sint16)(rsin((sint16)angle) * radius_scale);
        sint16 cosine = (sint16)(rcos((sint16)angle) * radius_scale);

        render_project_horizon(sine, cosine, group_heights, &proj_a[group * 3]);
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
    stripe_bucket_step = (sint16)cfg->tex_width / 64;
    for (ring = 0; ring < ring_count; ++ring)
    {
        POLY_F4 *edge_packet = edge_pool;
        uint32 edge_present;

        for (group = 0; group < group_count; ++group)
        {
            sint16 group_heights[3] = {heights[group * 3], heights[group * 3 + 1], heights[group * 3 + 2]};
            sint16 sine = (sint16)(rsin((sint16)angle) * radius_scale);
            sint16 cosine = (sint16)(rcos((sint16)angle) * radius_scale);

            render_project_horizon(sine, cosine, group_heights, &next[group * 3]);
        }
        edge_present = cfg->edge != 0u;
        if (edge_present)
        {
            POLY_F4 *packet = edge_packet;
            packet->xy2 = current[0];
            packet->xy0 = current[1];
            packet->xy3 = next[0];
            packet->xy1 = next[1];
            packet->color0 = cfg->colors[0].word;
            AddPrim(&scene_ot[13], packet);
            ++edge_pool;
            edge_packet = edge_pool;
        }
        for (group = 0; group < (sint32)cfg->strips; ++group)
        {
            POLY_FT4 *packet = strip_pool++;
            uint32 point = edge_present + (uint32)group;
            sint32 texture_left = -2 - 32 * (group + 1);
            sint32 texture_right = -2 - 32 * group;
            sint32 bucket_offset = (group / 2) * stripe_bucket_step;
            sint32 shade_bucket = (ascending ? (sint16)shade : (sint16)boundary) / 64;
            uint32 bucket = (uint32)(shade_bucket + 5 + bucket_offset);
            if (bucket >= 15u)
                abort();

            packet->xy2 = current[point];
            packet->xy0 = current[point + 1u];
            packet->xy3 = next[point];
            packet->xy1 = next[point + 1u];
            packet->u0 = packet->u2 = (uint8)shade;
            packet->u1 = packet->u3 = (uint8)boundary;
            packet->v0 = packet->v1 = (uint8)texture_left;
            packet->v2 = packet->v3 = (uint8)texture_right;
            AddPrim(&scene_ot[bucket], packet);
        }
        // Original 8001A0A8 underflows the strip pool when the resource has no strips
        if (cfg->strips != 0u)
        {
            ++strip_pool[-1].v0;
            ++strip_pool[-1].v1;
        }
        if (cfg->gradient != 0u)
        {
            POLY_G4 *packet = triangle_pool++;
            packet->xy2 = current[height_count - 2];
            packet->xy0 = current[height_count - 1];
            packet->xy3 = next[height_count - 2];
            packet->xy1 = next[height_count - 1];
            AddPrim(&scene_ot[13], packet);
        }
        {
            POLY_F4 *packet = edge_packet;
            packet->xy0 = current[height_count - 1];
            packet->xy2 = current[height_count - 1];
            packet->xy1 = next[height_count - 1];
            packet->xy3 = next[height_count - 1];
            packet->y0 = packet->y1 = 0;
            packet->color0 = cfg->colors[2].word;
            packet->code = 0x28;
            AddPrim(&scene_ot[13], packet);
            ++edge_pool;
        }
        if (ascending)
        {
            shade = boundary + 1;
            boundary += 64;
            if ((sint16)shade >= 256)
            {
                shade = 255;
                if (cfg->alternating != 0u)
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
                if (cfg->alternating != 0u)
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
            current = proj_a;
            next = proj_b;
        }
        else
        {
            current = proj_b;
            next = proj_a;
        }
    }
    AddPrim(&scene_ot[13], &buffer->texture_windows[2]);
    AddPrim(&scene_ot[5], &buffer->texture_windows[3]);
    result = getaddr(&scene_ot[5]);
    return result;
}

sint32 render_dispatch_prim_groups(const OBJECT_GROUP_STATE *object, uint32 table, sint32 adjusted, uint32 *ot)
{
    const OBJECT_GROUP_ENTRY *entries;
    sint32 entry_index;
    sint32 result;

    FUNCTION_MARKER(0x8001DA94u, "MAIN.EXE");
    if (object->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    render_set_order_depth(ot, 100u);
    result = (sint32)object->capacity;
    entries = object->slots;
    if (result == 0)
        return result;
    entry_index = 0;
    do
    {
        if (entries->status == 2u)
        {
            uint32 item = entries->section;
            SCENE_SECTION *section = scene_section(item);
            if (table >= 2u)
                abort();
            for (uint32 list = 0; list < 2u; ++list)
                for (SCENE_GROUP_NODE *node = section->groups[list]; node; node = node->next)
                    poly_dispatch_prim_groups(node->group, table, ot, adjusted, (sint32)list);
        }
        ++entry_index;
        result = entry_index < (sint32)object->capacity;
        ++entries;
    } while (result != 0);
    return result;
}





HUD_PRIM *render_publish_hud(const HUD_STATE *hud)
{
    uint16 text_count = render_hud.text_count;

    FUNCTION_MARKER(0x8002085Cu, "MAIN.EXE");
    render_hud = *hud;
    render_hud.text_count = text_count;
    return render_hud.prims;
}

HUD_STATE render_hud;

static sint32 render_init_hud_prims(HUD_STATE *hud, HUD_PRIM *prims, DR_MODE modes[2],
                             const HUD_SPRITE *src, uint16 count,
                             uint16 hud_count, uint16 menu_count, sint32 buf)
{
    uint16 idx;

    FUNCTION_MARKER(0x80020894u, "MAIN.EXE");
    if (count > HUD_PRIM_CAPACITY || buf < 0 || buf >= 8)
        abort();
    hud->prims = prims;
    for (idx = 0; idx < count; ++idx)
    {
        HUD_PRIM *dst = &prims[idx];
        const HUD_SPRITE *sprite = &src[idx];
        uint16 clut = (uint16)getClut(sprite->clut_x, sprite->clut_y);
        uint16 tpage = (uint16)getTPage(0, 0, sprite->tex_x, sprite->tex_y);

        dst->kind = 4;
        dst->state = sprite->state;
        dst->sprite.r0 = dst->sprite.g0 = dst->sprite.b0 = 0x80;
        dst->sprite.x0 = sprite->x;
        dst->sprite.y0 = (sint16)(sprite->y - ((uint32)(buf - 2) < 2u ? 10 : 0));
        dst->sprite.u0 = (uint8)((sprite->tex_x % 64) * 4);
        dst->sprite.v0 = (uint8)sprite->tex_y;
        dst->sprite.clut = clut;
        dst->sprite.w = (uint16)(4 * sprite->w);
        dst->sprite.h = (uint16)sprite->h;
        setSprt(&dst->sprite);
        SetDrawMode(&dst->mode, 1, 0, tpage, NULL);
        if (sprite->state >= 2)
            dst->sprite.y0 = sprite->y;
    }
    hud->prim_count = count;
    hud->hud_count = hud_count;
    hud->menu_count = menu_count;
    SetDrawMode(&modes[0], 1, 0, getTPage(0, 0, 832, 256), NULL);
    SetDrawMode(&modes[1], 1, 0, getTPage(0, 0, 832, 256), NULL);
    return 0;
}

sint32 render_submit_active_recs(HUD_PRIM *prims, uint32 *ot)
{
    sint32 count = (sint16)render_hud.prim_count;
    sint32 idx = 0;

    FUNCTION_MARKER(0x80020D48u, "MAIN.EXE");
    render_order.ot = ot;
    if (count <= 0)
        return count;
    do
    {
        if (prims[idx].state == 1)
        {
            AddPrim(render_order.ot, &prims[idx].sprite);
            if (prims[idx].kind == 4)
                AddPrim(render_order.ot, &prims[idx].mode);
        }
        idx = (sint16)(idx + 1);
        count = (sint16)render_hud.prim_count;
    } while (idx < count);
    return 0;
}

sint32 render_activate_recs(HUD_PRIM *prims)
{
    sint32 count = (sint16)render_hud.prim_count;
    sint32 idx;

    FUNCTION_MARKER(0x800210CCu, "MAIN.EXE");
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx)
        if (prims[idx].state == 1)
            prims[idx].state = 2;
    return idx < count;
}

static sint32 render_init_hud_text(HUD_STATE *hud, HUD_DESC *descs, uint16 count)
{
    uint16 idx;

    FUNCTION_MARKER(0x8001F374u, "MAIN.EXE");
    hud->descs = descs;
    for (idx = 0; idx < count; ++idx)
    {
        HUD_DESC *desc = &descs[idx];
        sint16 font_id = desc->font_id;

        if (font_id < 0 || font_id >= 6)
            abort();
        desc->font = &render_hud_fonts[font_id];
        desc->shape = &hud->prims[desc->font->glyph].sprite;
        desc->mode = &hud->prims[desc->font->glyph].mode;
    }
    hud->desc_count = count;
    return -1;
}

void render_build_text_packets(void)
{
    sint32 desc_idx;
    HUD_PRIM *last = NULL;

    FUNCTION_MARKER(0x800205ACu, "MAIN.EXE");
    render_hud.text_count = 0;
    for (desc_idx = 0; desc_idx < (sint16)render_hud.desc_count; ++desc_idx)
    {
        HUD_DESC *desc = &render_hud.descs[desc_idx];
        if (desc->visible == 1)
        {
            uint32 advance = 0;
            char *text = desc->text;
            uint16 start = render_hud.prim_count;
            sint32 count = 0;

            while ((uint8)*text != 0)
            {
                sint32 metric_idx;
                const HUD_METRIC *metric;
                HUD_PRIM *dst;

                if (render_hud.prim_count >= HUD_PRIM_CAPACITY)
                    abort();
                dst = &render_hud.prims[render_hud.prim_count];
                metric_idx = (sint16)text_find_char_index(text, (char *)desc->font->chars);
                if ((uint32)metric_idx >= desc->font->count)
                    abort();
                metric = &desc->font->metrics[metric_idx];
                dst->kind = 2;
                dst->state = 1;
                dst->sprite = *desc->shape;
                dst->sprite.x0 = (sint16)((uint16)desc->x + advance);
                dst->sprite.y0 = desc->y;
                dst->sprite.u0 = (uint8)(dst->sprite.u0 + metric->u);
                dst->sprite.v0 = (uint8)(dst->sprite.v0 + metric->v);
                dst->sprite.w = metric->w;
                dst->sprite.h = metric->h;
                dst->sprite.clut = desc->clut;
                dst->mode = *desc->mode;
                advance += metric->w - 2u;
                last = dst;
                ++render_hud.prim_count;
                ++count;
                ++text;
            }
            render_hud.text_count = (uint16)(render_hud.text_count + count);
            if (desc->align == 0)
            {
                sint32 half = (sint16)advance;
                half += (sint32)((advance << 16) >> 31);
                advance = (uint32)half >> 1;
            }
            else if (desc->align == 1)
                advance = 0;
            if (!last)
                abort();
            last->kind = 4;
            for (sint32 idx = 0; idx < count; ++idx)
            {
                SPRT *sprite = &render_hud.prims[start + idx].sprite;
                sprite->x0 = (sint16)((uint16)sprite->x0 - advance);
            }
        }
    }
}

static void render_init_hud_frame(RENDER_FRAME *frame, HUD_LAYOUT layout, sint32 buf)
{
    render_init_hud_prims(&frame->hud, frame->hud_prims, frame->ui_modes,
                          render_hud_sprites(layout), 38, 19, 19, buf);
}

sint32 render_init_context(sint32 mode, uint32 unused)
{
    FUNCTION_MARKER(0x8001F0FCu, "MAIN.EXE");
    menu_init_layout();
    if (mode == 1)
    {
        for (sint32 buf = 0; buf < 2; ++buf)
            render_init_hud_frame(render_frame(&camera_views[0], buf), HUD_LAYOUT_SINGLE, buf);
        for (sint32 buf = 0; buf < 2; ++buf)
            render_init_hud_text(render_frame_hud(render_frame(&camera_views[0], buf)), render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0), 21);
        render_hud_geom = render_hud_sprites(HUD_LAYOUT_SINGLE);
        return 0x8009365C;
    }
    if (mode == 2)
    {
        HUD_LAYOUT layout = game_options.split_layout ? HUD_LAYOUT_VERTICAL : HUD_LAYOUT_HORIZONTAL;
        sint32 base_idx = game_options.split_layout ? 4 : 0;
        for (sint32 idx = 0; idx < 4; ++idx)
            render_init_hud_frame(render_frame(&camera_views[idx / 2], idx % 2), layout, base_idx + idx);
        for (sint32 idx = 0; idx < 4; ++idx)
            render_init_hud_text(render_frame_hud(render_frame(&camera_views[idx / 2], idx % 2)), render_hud_desc_bank(layout, idx / 2), 8);
        render_hud_geom = render_hud_sprites(layout);
        return 33;
    }
    return 2;
}
