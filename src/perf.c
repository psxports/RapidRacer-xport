#include "perf.h"
#include "xport.h"
#include "xport_trace.h"
#include "psx_gpu.h"
#include <stdlib.h>

enum
{
    PERF_MAX_SAMPLES = 20,
    PERF_MAX_MARKERS = 5
};

typedef struct
{
    sint16 spacing;
    uint8 shift, markers;
} PERF_CONFIG;

typedef struct
{
    sint32 r, g, b;
} PERF_COLOR;

typedef struct
{
    POLY_G4 cur[2];
    POLY_G4 graph[PERF_MAX_SAMPLES][2];
    POLY_F3 markers[2][PERF_MAX_MARKERS];
} PERF_FRAME;

typedef struct
{
    uint64 frame_start;
    uint16 samples[PERF_MAX_SAMPLES];
    uint16 cur, scale_shift, marker_count;
    uint8 sample_count;
} PERF;

static PERF perf_state;
static PERF_FRAME perf_frames[2];

// Configs and effective palette reads from reviewed MAIN.EXE
static const PERF_CONFIG perf_configs[3] = {
    {260, 0, 2},
    {130, 1, 3},
    {65, 2, 5},
};

// Original 20 reads include constants following the seven named colors
static const PERF_COLOR perf_colors[PERF_MAX_SAMPLES] = {
    {200, 0, 0}, {0, 200, 0}, {0, 0, 200}, {200, 200, 0}, {0, 200, 200}, {200, 0, 200}, {200, 200, 200}, {0, 0, 0}, {200, 0, 0}, {0, 200, 0}, {200, 0, 8192}, {200, 200, 2048}, {0, 200, 7680}, {200, 200, 1536}, {0, 0, 7373}, {0, 0, 1229}, {200, 0, 7168}, {0, 8192, 1024}, {200, 2048, 7021}, {200, 7680, 878},
};

static uint16 perf_elapsed(void)
{
    uint64 elapsed = xport_timer_get() - perf_state.frame_start;
    uint64 counter = elapsed / 64;
    uint32 shift = perf_state.scale_shift;

    counter >>= shift;
    if (counter > 0xFFFF)
        counter = 0xFFFF;
    return counter;
}

static sint16 perf_sample_store(void)
{
    uint8 count = perf_state.sample_count;
    sint16 val = perf_elapsed();

    if (count < PERF_MAX_SAMPLES)
    {
        perf_state.samples[count] = val;
        perf_state.sample_count = count + 1u;
    }
    return val;
}

static void perf_init_marker(POLY_F3 *prim, sint16 x, sint16 y)
{
    SetPolyF3(prim);
    setRGB0(prim, 50, 50, 150);
    prim->p[0].x = x - 4;
    prim->p[0].y = y;
    prim->p[1].x = x + 4;
    prim->p[1].y = y;
    prim->p[2].x = x;
    prim->p[2].y = y + 6;
}

static void perf_init_bar_colors(POLY_G4 *prim, uint32 color, sint32 reverse)
{
    sint32 r = perf_colors[color].r;
    sint32 g = perf_colors[color].g;
    sint32 b = perf_colors[color].b;
    uint8 dark_r = r / 2;
    uint8 dark_g = g / 2;
    uint8 dark_b = b / 2;
    uint8 bright_r = r;
    uint8 bright_g = g;
    uint8 bright_b = b;

    SetPolyG4(prim);
    setRGB0(prim, reverse ? bright_r : dark_r, reverse ? bright_g : dark_g, reverse ? bright_b : dark_b);
    setRGB1(prim, reverse ? bright_r : dark_r, reverse ? bright_g : dark_g, reverse ? bright_b : dark_b);
    setRGB2(prim, reverse ? dark_r : bright_r, reverse ? dark_g : bright_g, reverse ? dark_b : bright_b);
    setRGB3(prim, reverse ? dark_r : bright_r, reverse ? dark_g : bright_g, reverse ? dark_b : bright_b);
}

static void perf_sample_bar_init(POLY_G4 *prim, uint32 color, sint32 reverse)
{
    perf_init_bar_colors(prim, color, reverse);
    setXY0(prim, 25, reverse ? 27u : 25u);
    prim->p[1].xy.x = 100;
    prim->p[1].xy.y = reverse ? 26u : 25u;
    prim->p[2].xy.x = 25;
    prim->p[2].xy.y = reverse ? 29u : 26u;
    prim->p[3].xy.x = 100;
    prim->p[3].xy.y = reverse ? 29u : 27u;
}

static void perf_init_cur_bar(POLY_G4 *prim, sint32 reverse)
{
    SetPolyG4(prim);
    if (!reverse)
    {
        setRGB0(prim, 0, 100, 0);
        setRGB1(prim, 100, 0, 0);
        setRGB2(prim, 100, 255, 100);
        setRGB3(prim, 255, 100, 100);
        setXY0(prim, 25, 30);
        prim->p[1].xy.x = 100;
        prim->p[1].xy.y = 30;
        prim->p[2].xy.x = 25;
        prim->p[2].xy.y = 31;
        prim->p[3].xy.x = 100;
        prim->p[3].xy.y = 32;
    }
    else
    {
        setRGB0(prim, 100, 255, 100);
        setRGB1(prim, 255, 100, 100);
        setRGB2(prim, 0, 100, 0);
        setRGB3(prim, 100, 0, 0);
        setXY0(prim, 25, 31);
        prim->p[1].xy.x = 100;
        prim->p[1].xy.y = 32;
        prim->p[2].xy.x = 25;
        prim->p[2].xy.y = 34;
        prim->p[3].xy.x = 100;
        prim->p[3].xy.y = 34;
    }
}

void perf_init(sint32 scale)
{
    const PERF_CONFIG *cfg;
    sint16 spacing;
    uint32 buffer;
    uint32 marker;
    uint32 color;

    FUNCTION_MARKER(0x80022288, "MAIN.EXE");
    if (scale < 0 || scale >= (sint32)(sizeof(perf_configs) / sizeof(perf_configs[0])))
        abort();
    cfg = &perf_configs[scale];
    spacing = cfg->spacing;
    perf_state.scale_shift = cfg->shift;
    perf_state.marker_count = cfg->markers;
    if (!gpu_register_packet_range(perf_frames, sizeof(perf_frames)))
        abort();
    for (buffer = 0; buffer < 2; ++buffer)
    {
        for (marker = 0; marker < perf_state.marker_count; ++marker)
        {
            sint16 x = 21 + marker * spacing;

            perf_init_marker(&perf_frames[buffer].markers[0][marker], x, 22);
            perf_init_marker(&perf_frames[buffer].markers[1][marker], x, 36);
        }
        for (color = 0; color < PERF_MAX_SAMPLES; ++color)
        {
            perf_sample_bar_init(&perf_frames[buffer].graph[color][0], color, 0);
            perf_sample_bar_init(&perf_frames[buffer].graph[color][1], color, 1);
        }
        perf_init_cur_bar(&perf_frames[buffer].cur[0], 0);
        perf_init_cur_bar(&perf_frames[buffer].cur[1], 1);
    }
    perf_begin_frame();
}

void perf_begin_frame(void)
{
    FUNCTION_MARKER(0x8002284C, "MAIN.EXE");
    perf_state.frame_start = xport_timer_get();
    perf_state.sample_count = 0;
}

sint16 perf_mark_frame(void)
{
    FUNCTION_MARKER(0x800228E8, "MAIN.EXE");
    return perf_sample_store();
}

sint16 perf_frame_end(void)
{
    sint16 val;

    FUNCTION_MARKER(0x80022880, "MAIN.EXE");
    val = perf_sample_store();
    perf_state.cur = val;
    return val;
}

static void perf_render_markers(uint32 *ot_entry, POLY_F3 *markers)
{
    uint32 marker;

    for (marker = 0; marker < perf_state.marker_count; ++marker)
        AddPrim(ot_entry, &markers[marker]);
}

static void perf_render_samples(uint32 *ot_entry, sint16 buffer)
{
    uint32 count = perf_state.sample_count;
    uint32 idx;

    for (idx = 0; idx < count; ++idx)
    {
        uint16 right = perf_state.samples[idx];
        uint16 left = idx == 0 ? 0u : perf_state.samples[idx - 1];
        POLY_G4 *upper = &perf_frames[buffer].graph[idx][0];
        POLY_G4 *lower = &perf_frames[buffer].graph[idx][1];

        upper->p[0].xy.x = left + 25u;
        upper->p[2].xy.x = left + 25u;
        upper->p[1].xy.x = right + 25u;
        upper->p[3].xy.x = right + 25u;
        lower->p[0].xy.x = left + 25u;
        lower->p[2].xy.x = left + 25u;
        lower->p[1].xy.x = right + 25u;
        lower->p[3].xy.x = right + 25u;
        AddPrim(ot_entry, upper);
        AddPrim(ot_entry, lower);
    }
}

void perf_render_cur(uint32 *ot_entry, sint16 buffer)
{
    uint16 x = perf_state.cur + 25u;
    POLY_G4 *upper;
    POLY_G4 *lower;

    FUNCTION_MARKER(0x80022B84, "MAIN.EXE");
    if (!ot_entry || buffer < 0 || buffer >= 2)
        abort();
    upper = &perf_frames[buffer].cur[0];
    lower = &perf_frames[buffer].cur[1];
    upper->p[1].xy.x = x;
    upper->p[3].xy.x = x;
    lower->p[1].xy.x = x;
    lower->p[3].xy.x = x;
    perf_render_markers(ot_entry, perf_frames[buffer].markers[1]);
    AddPrim(ot_entry, upper);
    AddPrim(ot_entry, lower);
}

void perf_render_graph(uint32 *ot_entry, sint16 buffer)
{
    FUNCTION_MARKER(0x80022948, "MAIN.EXE");
    if (!ot_entry || buffer < 0 || buffer >= 2)
        abort();
    perf_render_markers(ot_entry, perf_frames[buffer].markers[0]);
    perf_render_samples(ot_entry, buffer);
}
