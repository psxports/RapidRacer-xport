#include "game.h"
#include "menu.h"
#include "track.h"
#include "callbacks.h"
#include "disp.h"
#include "psx_gpu.h"
#include "utils.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

// Scene dimensions from MAIN.EXE 800B3DF8/800B3DFC
DISP disp_state = {0, 368, 480};

// Scene GPU environments and viewport owners from MAIN.EXE 800DE0F0/800DF098
static DISP_SCENE_FRAME disp_scene_frames[2][2];

DISP_SCENE_FRAME *disp_scene_frame(const CAMERA *view, sint32 buffer)
{
    sint32 player;
    if (view == &camera_views[0])
        player = 0;
    else if (view == &camera_views[1])
        player = 1;
    else
        abort();
    if (buffer < 0 || buffer >= 2 || !gpu_register_packet_range(disp_scene_frames, sizeof(disp_scene_frames)))
        abort();
    return &disp_scene_frames[player][buffer];
}

// Line packet owner from MAIN.EXE 800E1C78
static LINE_F2 disp_lines[2][200];
static uint8 disp_lines_registered;

LINE_F2 *disp_line_buffer(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    if (!disp_lines_registered)
    {
        if (!gpu_register_packet_range(disp_lines, sizeof(disp_lines)))
            abort();
        disp_lines_registered = 1;
    }
    return disp_lines[buffer];
}

// Menu ordering owner from MAIN.EXE 800DD210 and PsyQ terminal 800A0CF4
static uint32 disp_menu_orders[2][40];
static uint32 disp_menu_terminal[5] = {0x04ffffff, 0, 0, 0, 0};
static uint8 disp_menu_registered;

uint32 *disp_order_slot(sint32 buffer, sint32 slot)
{
    if (buffer < 0 || buffer >= 2 || slot < 0 || slot >= 40)
        abort();
    if (!disp_menu_registered)
    {
        if (!gpu_register_packet_range(disp_menu_orders, sizeof(disp_menu_orders)) || !gpu_register_packet_range(disp_menu_terminal, sizeof(disp_menu_terminal)))
            abort();
        disp_menu_registered = 1;
    }
    return &disp_menu_orders[buffer][slot];
}

uint32 *disp_begin_menu_frame(void)
{
    uint32 *orders = disp_order_slot(disp_state.buffer, 0);
    ClearOTag(orders, 40);
    AddPrim(orders + 39, disp_menu_terminal);
    return orders;
}

// Menu environment owners from MAIN.EXE 800DDE18/800DDE74, stride112
static DRAWENV disp_menu_draw[2];
static DISPENV disp_menu_disp[2];

DRAWENV *disp_draw_env(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    return &disp_menu_draw[buffer];
}

DISPENV *disp_disp_env(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    return &disp_menu_disp[buffer];
}

sint32 disp_sync_swap_buf(void)
{
    sint32 pass;

    FUNCTION_MARKER(0x8001116C, "MAIN.EXE");
    gpu_set_raster(1);
    DrawSync(0);
    for (pass = 0; pass < 2; ++pass)
    {
        sint32 buffer = disp_state.scene_buffer;

        render_select_prim_pool(buffer);
        buffer = disp_state.scene_buffer;
        track_render_player(&race_views[0], buffer);
        if (race_cfg.players == 2)
        {
            buffer = disp_state.scene_buffer;
            track_render_player(&race_views[1], buffer);
        }
        game_frame_og();
        disp_state.scene_buffer = 1 - disp_state.scene_buffer;
    }
    return 0;
}

static sint32 disp_publish_dimensions(uint32 width, uint32 height)
{
    FUNCTION_MARKER(0x80016144, "MAIN.EXE");
    disp_state.view_width = width;
    disp_state.view_height = height;
    FlushCache();
    return 0;
}

sint32 disp_init_envs(void)
{
    // Original page configurations from MAIN.EXE 80080AB4
    static const PSX_RECT pages[2][2] = {{{0, 0, 368, 512}, {0, 0, 368, 512}}, {{0, 0, 512, 256}, {0, 256, 512, 256}}};
    uint32 mode = race_cfg.players;
    sint32 compact = mode == 2 || game_timing.base_rate == 25;
    const PSX_RECT *page = pages[compact];
    sint32 width = page[0].w;
    sint32 height = page[0].h;
    sint32 half_width = width;
    sint32 half_height = height;
    sint32 player, buffer;

    FUNCTION_MARKER(0x80014D54, "MAIN.EXE");
    if (mode == 2)
    {
        if (race_cfg.layout == mode)
            half_width /= 2;
        else
            half_height /= 2;
    }
    disp_publish_dimensions(half_width, half_height);
    for (buffer = 0; buffer < 2; ++buffer)
    {
        DISPENV *disp = &disp_scene_frame(&camera_views[0], buffer)->disp;
        const PSX_RECT *rect = page + buffer;
        SetDefDispEnv(disp, rect->x, rect->y, rect->w, rect->h);
        disp->screen.y = 20;
        disp->screen.h = 256;
        disp->isinter = !compact;
    }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DISP_SCENE_FRAME *frame = disp_scene_frame(&camera_views[player], buffer);
            const PSX_RECT *draw_page = &page[1 - buffer];
            sint32 x = draw_page->x;
            sint32 y = draw_page->y;
            frame->viewport = *draw_page;
            if (player != 0)
            {
                if (race_cfg.layout == 2)
                    x += half_width;
                else
                    y += half_height;
            }
            SetDefDrawEnv(&frame->draw, x, y, half_width, half_height);
        }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DISP_SCENE_FRAME *frame = disp_scene_frame(&camera_views[player], buffer);
            SetDrawEnv(&frame->packet, &frame->draw);
        }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            LINE_F2 *line = &disp_scene_frame(&camera_views[player], buffer)->divider;
            setLineF2(line);
            setRGB0(line, 0, 0, 0);
            if (player == 0)
            {
                line->p[0].x = race_cfg.layout == 2 ? (half_width - 1) : 0;
                line->p[0].y = race_cfg.layout == 2 ? 0 : (half_height - 1);
                line->p[1].x = half_width - 1;
                line->p[1].y = half_height - 1;
            }
            else
            {
                line->p[0].x = line->p[0].y = 0;
                line->p[1].x = race_cfg.layout == 2 ? 0 : (half_width - 1);
                line->p[1].y = race_cfg.layout == 2 ? (half_height - 1) : 0;
            }
        }
    render_set_geom_offset(half_width / 2, half_height / 2);
    return 0;
}

void menu_reset_counts(void)
{
    FUNCTION_MARKER(0x8004420C, "MAIN.EXE");
    menu_asset_counts.labels = menu_asset_defaults.labels;
    menu_asset_counts.sprs = menu_asset_defaults.sprs;
    menu_notice.lines = menu_notice.default_lines;
    menu_asset_counts.extra = menu_asset_defaults.extra;
}

static sint32 disp_clear_page_region(sint32 page, sint16 x, sint16 y, sint16 width, sint16 height, uint8 r, uint8 g, uint8 b)
{
    // Original zero seed at MAIN.EXE 800B4090, overridden at 800442D4/800442DC
    static const sint16 origins[3][2] = {{0, 0}, {0, 256}, {512, 0}};
    PSX_RECT rect;
    sint16 idx = page;

    FUNCTION_MARKER(0x80044280, "MAIN.EXE");
    if (idx < 0 || idx >= 3)
        abort();
    rect.x = origins[idx][0] + x;
    rect.y = origins[idx][1] + y;
    rect.w = width;
    rect.h = height;
    ClearImage(&rect, r, g, b);
    return DrawSync(0);
}

sint32 disp_clear_region(sint16 x, sint16 y)
{
    PSX_RECT rect;

    FUNCTION_MARKER(0x80044338, "MAIN.EXE");
    rect.x = x;
    rect.y = y;
    rect.w = disp_state.width;
    rect.h = disp_state.height;
    ClearImage(&rect, 0, 0, 0);
    return DrawSync(0);
}

static sint32 disp_move_rect(sint16 x, sint16 y, sint16 dst_x, sint16 dst_y)
{
    PSX_RECT rect;

    FUNCTION_MARKER(0x8004438C, "MAIN.EXE");
    rect.x = x;
    rect.y = y;
    rect.w = disp_state.width;
    rect.h = disp_state.height;
    MoveImage(&rect, dst_x, dst_y);
    return DrawSync(0);
}

sint32 disp_move_page_rect(sint32 src_page, sint32 dst_page)
{
    // Original zero seed at MAIN.EXE 800B4090 with menu width/height overrides
    sint16 horizontal[3] = {0, 0, (sint16)disp_state.width};
    sint16 vertical[3] = {0, (sint16)disp_state.height, 0};
    sint16 src = src_page;
    sint16 dst = dst_page;

    FUNCTION_MARKER(0x800443E4, "MAIN.EXE");
    if (src < 0 || src >= 3 || dst < 0 || dst >= 3)
        abort();
    return disp_move_rect(horizontal[src], vertical[src], horizontal[dst], vertical[dst]);
}

sint32 disp_clear_quadrants(void)
{
    sint16 width = disp_state.width;
    sint16 height = disp_state.height;

    FUNCTION_MARKER(0x80044564, "MAIN.EXE");
    disp_clear_region(0, 0);
    disp_clear_region(0, height);
    disp_clear_region(width, 0);
    return disp_clear_region(width, height);
}

DRAWENV *disp_swap_after_vsync(void)
{
    sint32 next;
    DRAWENV *draw_environment;
    DrawSync(0);
    next = disp_state.buffer == 0;
    disp_state.buffer = next;
    PutDispEnv(disp_disp_env(next));
    draw_environment = disp_draw_env(next);
    PutDrawEnv(draw_environment);
    return draw_environment;
}

DRAWENV *disp_swap_envs(void)
{
    FUNCTION_MARKER(0x8004533C, "MAIN.EXE");
    VSync(0);
    return disp_swap_after_vsync();
}

void disp_set_line_color(sint8 r, sint8 g, sint8 b)
{
    FUNCTION_MARKER(0x800453B0, "MAIN.EXE");
    disp_state.line_r = r;
    disp_state.line_g = g;
    disp_state.line_b = b;
}

sint32 disp_queue_line_seg(sint16 x0, sint16 y0, sint16 x1, sint16 y1)
{
    uint16 count = disp_state.line_count;
    LINE_F2 *line;

    FUNCTION_MARKER(0x800453C4, "MAIN.EXE");
    if (count >= 200u)
    {
        if (count != 200)
            abort();
        return 200;
    }
    line = &disp_line_buffer(disp_state.buffer)[count];
    setRGB0(line, disp_state.line_r, disp_state.line_g, disp_state.line_b);
    line->p[0].x = x0;
    line->p[0].y = y0;
    line->p[1].x = x1;
    line->p[1].y = y1;
    setLineF2(line);
    disp_state.line_count = count + 1u;
    return count + 1;
}

sint32 disp_queue_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom)
{
    FUNCTION_MARKER(0x8004545C, "MAIN.EXE");
    disp_queue_line_seg(left, top, right, top);
    disp_queue_line_seg(left, bottom, right, bottom);
    disp_queue_line_seg(left, top, left, bottom);
    return disp_queue_line_seg(right, top, right, bottom);
}

sint32 disp_queue_beveled_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom)
{
    FUNCTION_MARKER(0x80045508, "MAIN.EXE");
    disp_set_line_color(192, 192, 192);
    disp_queue_rect_outline(left, top, right, bottom);
    disp_set_line_color(32, 32, 32);
    return disp_queue_rect_outline(left + 1, top + 1, right + 1, bottom + 1);
}
