#include "perf.h"
#include "xport.h"
#include "xport_trace.h"

enum
{
    PERF_SAMPLES = 0x800B8028u,
    PERF_CURRENT = 0x800B67D8u,
    PERF_SCALE_SHIFT = 0x800B67CCu,
    PERF_MARKER_COUNT = 0x800B67D0u,
    PERF_SAMPLE_COUNT = 0x800B67E0u,
    PERF_CURRENT_BAR_A = 0x800B8050u,
    PERF_CURRENT_BAR_B = 0x800B8098u,
    PERF_GRAPH_BAR_A = 0x800B80E0u,
    PERF_GRAPH_BAR_B = 0x800B8128u,
    PERF_MARKERS_A = 0x800B8C20u,
    PERF_MARKERS_B = 0x800B8D10u,
    PERF_CONFIG = 0x80084340u,
    PERF_COLORS_R = 0x8008434Cu,
    PERF_COLORS_G = 0x80084368u,
    PERF_COLORS_B = 0x80084384u,
    PERF_MAX_SAMPLES = 20u
};

static uint64 perf_frame_start;

static void perf_add_prim(uint32 ot_entry, uint32 primitive)
{
    AddPrim(psx_addr(ot_entry, sizeof(uint32)), psx_addr(primitive, sizeof(uint32)));
}

static uint16 perf_elapsed(void)
{
    uint64 elapsed = xport_timer_get() - perf_frame_start;
    uint64 counter = elapsed / 64u;
    uint32 shift = r_u16(PERF_SCALE_SHIFT);

    counter >>= shift;
    if (counter > 0xFFFFu)
        counter = 0xFFFFu;
    return (uint16)counter;
}

static sint16 perf_sample_store(void)
{
    uint8 count = r_u8(PERF_SAMPLE_COUNT);
    sint16 value = (sint16)perf_elapsed();

    if (count < PERF_MAX_SAMPLES)
    {
        w_u16(PERF_SAMPLES + (uint32)count * 2u, (uint16)value);
        w_u8(PERF_SAMPLE_COUNT, (uint8)(count + 1u));
    }
    return value;
}

static void perf_init_marker(uint32 primitive, sint16 x, sint16 y)
{
    SetPolyF3(psx_addr(primitive, 20u));
    w_u8(primitive + 4u, 50u);
    w_u8(primitive + 5u, 50u);
    w_u8(primitive + 6u, 150u);
    w_u16(primitive + 8u, (uint16)(x - 4));
    w_u16(primitive + 10u, (uint16)y);
    w_u16(primitive + 12u, (uint16)(x + 4));
    w_u16(primitive + 14u, (uint16)y);
    w_u16(primitive + 16u, (uint16)x);
    w_u16(primitive + 18u, (uint16)(y + 6));
}

static void perf_init_bar_colors(uint32 primitive, uint32 color, sint32 reverse)
{
    sint32 red = (sint32)r_u32(PERF_COLORS_R + color * 4u);
    sint32 green = (sint32)r_u32(PERF_COLORS_G + color * 4u);
    sint32 blue = (sint32)r_u32(PERF_COLORS_B + color * 4u);
    uint8 dark_red = (uint8)(red / 2);
    uint8 dark_green = (uint8)(green / 2);
    uint8 dark_blue = (uint8)(blue / 2);
    uint8 bright_red = (uint8)red;
    uint8 bright_green = (uint8)green;
    uint8 bright_blue = (uint8)blue;

    SetPolyG4(psx_addr(primitive, 36u));
    w_u8(primitive + 4u, reverse ? bright_red : dark_red);
    w_u8(primitive + 5u, reverse ? bright_green : dark_green);
    w_u8(primitive + 6u, reverse ? bright_blue : dark_blue);
    w_u8(primitive + 12u, reverse ? bright_red : dark_red);
    w_u8(primitive + 13u, reverse ? bright_green : dark_green);
    w_u8(primitive + 14u, reverse ? bright_blue : dark_blue);
    w_u8(primitive + 20u, reverse ? dark_red : bright_red);
    w_u8(primitive + 21u, reverse ? dark_green : bright_green);
    w_u8(primitive + 22u, reverse ? dark_blue : bright_blue);
    w_u8(primitive + 28u, reverse ? dark_red : bright_red);
    w_u8(primitive + 29u, reverse ? dark_green : bright_green);
    w_u8(primitive + 30u, reverse ? dark_blue : bright_blue);
}

static void perf_sample_bar_init(uint32 primitive, uint32 color, sint32 reverse)
{
    perf_init_bar_colors(primitive, color, reverse);
    w_u16(primitive + 8u, 25u);
    w_u16(primitive + 10u, reverse ? 27u : 25u);
    w_u16(primitive + 16u, 100u);
    w_u16(primitive + 18u, reverse ? 26u : 25u);
    w_u16(primitive + 24u, 25u);
    w_u16(primitive + 26u, reverse ? 29u : 26u);
    w_u16(primitive + 32u, 100u);
    w_u16(primitive + 34u, reverse ? 29u : 27u);
}

static void perf_init_current_bar(uint32 primitive, sint32 reverse)
{
    SetPolyG4(psx_addr(primitive, 36u));
    if (!reverse)
    {
        w_u8(primitive + 4u, 0u);
        w_u8(primitive + 5u, 100u);
        w_u8(primitive + 6u, 0u);
        w_u8(primitive + 12u, 100u);
        w_u8(primitive + 13u, 0u);
        w_u8(primitive + 14u, 0u);
        w_u8(primitive + 20u, 100u);
        w_u8(primitive + 21u, 255u);
        w_u8(primitive + 22u, 100u);
        w_u8(primitive + 28u, 255u);
        w_u8(primitive + 29u, 100u);
        w_u8(primitive + 30u, 100u);
        w_u16(primitive + 8u, 25u);
        w_u16(primitive + 10u, 30u);
        w_u16(primitive + 16u, 100u);
        w_u16(primitive + 18u, 30u);
        w_u16(primitive + 24u, 25u);
        w_u16(primitive + 26u, 31u);
        w_u16(primitive + 32u, 100u);
        w_u16(primitive + 34u, 32u);
    }
    else
    {
        w_u8(primitive + 4u, 100u);
        w_u8(primitive + 5u, 255u);
        w_u8(primitive + 6u, 100u);
        w_u8(primitive + 12u, 255u);
        w_u8(primitive + 13u, 100u);
        w_u8(primitive + 14u, 100u);
        w_u8(primitive + 20u, 0u);
        w_u8(primitive + 21u, 100u);
        w_u8(primitive + 22u, 0u);
        w_u8(primitive + 28u, 100u);
        w_u8(primitive + 29u, 0u);
        w_u8(primitive + 30u, 0u);
        w_u16(primitive + 8u, 25u);
        w_u16(primitive + 10u, 31u);
        w_u16(primitive + 16u, 100u);
        w_u16(primitive + 18u, 32u);
        w_u16(primitive + 24u, 25u);
        w_u16(primitive + 26u, 34u);
        w_u16(primitive + 32u, 100u);
        w_u16(primitive + 34u, 34u);
    }
}

void perf_init(sint32 scale)
{
    uint32 config = PERF_CONFIG + (uint32)scale * 4u;
    sint16 spacing = (sint16)r_u16(config);
    uint32 buffer;
    uint32 marker;
    uint32 color;

    FUNCTION_MARKER_ARGS(0x80022288u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 1u, XPORT_CALL_SCALAR((uint32)scale));
    w_u16(PERF_SCALE_SHIFT, r_u8(config + 2u));
    w_u16(PERF_MARKER_COUNT, r_u8(config + 3u));
    for (buffer = 0u; buffer < 2u; ++buffer)
    {
        for (marker = 0u; marker < r_u16(PERF_MARKER_COUNT); ++marker)
        {
            sint16 x = (sint16)(21 + (sint32)marker * spacing);

            perf_init_marker(PERF_MARKERS_A + buffer * 20u + marker * 40u, x, 22);
            perf_init_marker(PERF_MARKERS_B + buffer * 20u + marker * 40u, x, 36);
        }
        for (color = 0u; color < PERF_MAX_SAMPLES; ++color)
        {
            perf_sample_bar_init(PERF_GRAPH_BAR_A + buffer * 36u + color * 144u, color, 0);
            perf_sample_bar_init(PERF_GRAPH_BAR_B + buffer * 36u + color * 144u, color, 1);
        }
        perf_init_current_bar(PERF_CURRENT_BAR_A + buffer * 36u, 0);
        perf_init_current_bar(PERF_CURRENT_BAR_B + buffer * 36u, 1);
    }
    perf_begin_frame();
}

void perf_begin_frame(void)
{
    FUNCTION_MARKER(0x8002284Cu, "MAIN.EXE");
    perf_frame_start = xport_timer_get();
    w_u8(PERF_SAMPLE_COUNT, 0u);
}

sint16 perf_mark_frame(void)
{
    FUNCTION_MARKER(0x800228E8u, "MAIN.EXE");
    return perf_sample_store();
}

sint16 perf_frame_end(void)
{
    sint16 value;

    FUNCTION_MARKER(0x80022880u, "MAIN.EXE");
    value = perf_sample_store();
    w_u16(PERF_CURRENT, (uint16)value);
    return value;
}

static void perf_render_markers(uint32 ot_entry, sint16 buffer, uint32 base)
{
    uint32 marker;

    for (marker = 0u; marker < r_u16(PERF_MARKER_COUNT); ++marker)
        perf_add_prim(ot_entry, base + (uint32)(sint32)buffer * 20u + marker * 40u);
}

static void perf_render_samples(uint32 ot_entry, sint16 buffer)
{
    uint32 buffer_offset = (uint32)(sint32)buffer * 36u;
    uint32 count = r_u8(PERF_SAMPLE_COUNT);
    uint32 index;

    for (index = 0u; index < count; ++index)
    {
        uint16 right = r_u16(PERF_SAMPLES + index * 2u);
        uint16 left = index == 0u ? 0u : r_u16(PERF_SAMPLES + (index - 1u) * 2u);
        uint32 first = PERF_GRAPH_BAR_A + buffer_offset + index * 144u;
        uint32 second = PERF_GRAPH_BAR_B + buffer_offset + index * 144u;

        w_u16(first + 8u, (uint16)(left + 25u));
        w_u16(first + 24u, (uint16)(left + 25u));
        w_u16(first + 16u, (uint16)(right + 25u));
        w_u16(first + 32u, (uint16)(right + 25u));
        w_u16(second + 8u, (uint16)(left + 25u));
        w_u16(second + 24u, (uint16)(left + 25u));
        w_u16(second + 16u, (uint16)(right + 25u));
        w_u16(second + 32u, (uint16)(right + 25u));
        perf_add_prim(ot_entry, first);
        perf_add_prim(ot_entry, second);
    }
}

void perf_render_current(uint32 ot_entry, sint16 buffer)
{
    uint32 offset = (uint32)(sint32)buffer * 36u;
    uint16 x = (uint16)(r_u16(PERF_CURRENT) + 25u);
    uint32 first = PERF_CURRENT_BAR_A + offset;
    uint32 second = PERF_CURRENT_BAR_B + offset;

    FUNCTION_MARKER_ARGS(0x80022B84u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 2u, XPORT_CALL_GUEST_POINTER(ot_entry, 4u), XPORT_CALL_SCALAR((uint32)(sint32)buffer));
    w_u16(first + 16u, x);
    w_u16(first + 32u, x);
    w_u16(second + 16u, x);
    w_u16(second + 32u, x);
    perf_render_markers(ot_entry, buffer, PERF_MARKERS_B);
    perf_add_prim(ot_entry, first);
    perf_add_prim(ot_entry, second);
}

void perf_render_graph(uint32 ot_entry, sint16 buffer)
{
    FUNCTION_MARKER_ARGS(0x80022948u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 2u, XPORT_CALL_GUEST_POINTER(ot_entry, 4u), XPORT_CALL_SCALAR((uint32)(sint32)buffer));
    perf_render_markers(ot_entry, buffer, PERF_MARKERS_A);
    perf_render_samples(ot_entry, buffer);
}
