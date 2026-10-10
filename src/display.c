#include "game.h"
#include "menu.h"
#include "scene.h"
#include "callbacks.h"
#include "runtime.h"
#include "display.h"
#include "psx_gpu.h"
#include "global.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

// Scene dimensions from MAIN.EXE 800B3DF8/800B3DFC
DISPLAY_STATE display_state = {0u, 368u, 480u};

// Scene GPU environments and viewport owners from MAIN.EXE 800DE0F0/800DF098
static DISPLAY_SCENE_FRAME display_scene_frames[2][2];

DISPLAY_SCENE_FRAME *display_scene_frame(const CAMERA_STATE *view, sint32 buffer)
{
    sint32 player;
    if (view == &camera_views[0])
        player = 0;
    else if (view == &camera_views[1])
        player = 1;
    else
        abort();
    if (buffer < 0 || buffer >= 2 || !gpu_register_packet_range(display_scene_frames, sizeof(display_scene_frames)))
        abort();
    return &display_scene_frames[player][buffer];
}

// Line packet owner from MAIN.EXE 800E1C78
static LINE_F2 display_lines[2][200];
static uint8 display_lines_registered;

LINE_F2 *display_line_buffer(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    if (!display_lines_registered)
    {
        if (!gpu_register_packet_range(display_lines, sizeof(display_lines)))
            abort();
        display_lines_registered = 1;
    }
    return display_lines[buffer];
}

// Menu ordering owner from MAIN.EXE 800DD210 and PsyQ terminal 800A0CF4
static uint32 display_menu_orders[2][40];
static uint32 display_menu_terminal[5] = {0x04ffffffu, 0u, 0u, 0u, 0u};
static uint8 display_menu_registered;

uint32 *display_order_slot(sint32 buffer, sint32 slot)
{
    if (buffer < 0 || buffer >= 2 || slot < 0 || slot >= 40)
        abort();
    if (!display_menu_registered)
    {
        if (!gpu_register_packet_range(display_menu_orders, sizeof(display_menu_orders)) || !gpu_register_packet_range(display_menu_terminal, sizeof(display_menu_terminal)))
            abort();
        display_menu_registered = 1;
    }
    return &display_menu_orders[buffer][slot];
}

uint32 *display_begin_menu_frame(void)
{
    uint32 *orders = display_order_slot((sint16)display_state.buffer, 0);
    ClearOTag(orders, 40);
    AddPrim(orders + 39, display_menu_terminal);
    return orders;
}

// Menu environment owners from MAIN.EXE 800DDE18/800DDE74, stride112
static DRAWENV display_menu_draw[2];
static DISPENV display_menu_disp[2];

DRAWENV *display_draw_env(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    return &display_menu_draw[buffer];
}

DISPENV *display_disp_env(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    return &display_menu_disp[buffer];
}

sint32 display_sync_swap_buf(void)
{
    sint32 pass;

    FUNCTION_MARKER(0x8001116Cu, "MAIN.EXE");
    DrawSync(0);
    for (pass = 0; pass < 2; ++pass)
    {
        sint32 buffer = (sint32)display_state.scene_buffer;

        render_select_prim_pool(buffer);
        buffer = (sint32)display_state.scene_buffer;
        scene_render_player(0x800DE0F0u, buffer);
        if (r_u32(0x80083478u) == 2u)
        {
            buffer = (sint32)display_state.scene_buffer;
            scene_render_player(0x800DF098u, buffer);
        }
        game_render_frame();
        buffer = (sint32)display_state.scene_buffer;
        display_state.scene_buffer = 1u - (uint32)buffer;
    }
    return 0;
}

sint32 display_init_envs(void)
{
    // Original page configurations from MAIN.EXE 80080AB4
    static const PSX_RECT pages[2][2] = {{{0, 0, 368, 512}, {0, 0, 368, 512}}, {{0, 0, 512, 256}, {0, 256, 512, 256}}};
    uint32 mode = r_u32(0x80083478u);
    sint32 compact = mode == 2u || game_timing.base_rate == 25u;
    const PSX_RECT *page = pages[compact];
    sint32 width = page[0].w;
    sint32 height = page[0].h;
    sint32 half_width = width;
    sint32 half_height = height;
    sint32 player, buffer;

    FUNCTION_MARKER(0x80014D54u, "MAIN.EXE");
    if (mode == 2u)
    {
        if (r_u32(0x80083488u) == mode)
            half_width /= 2;
        else
            half_height /= 2;
    }
    display_publish_dimensions((uint32)half_width, (uint32)half_height);
    for (buffer = 0; buffer < 2; ++buffer)
    {
        DISPENV *disp = &display_scene_frame(&camera_views[0], buffer)->disp;
        SetDefDispEnv(disp, page[buffer].x, page[buffer].y, page[buffer].w, page[buffer].h);
        disp->screen.y = 20;
        disp->screen.h = 256;
        disp->isinter = (uint8)!compact;
    }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DISPLAY_SCENE_FRAME *frame = display_scene_frame(&camera_views[player], buffer);
            const PSX_RECT *draw_page = &page[1 - buffer];
            sint32 x = draw_page->x;
            sint32 y = draw_page->y;
            frame->viewport = *draw_page;
            if (player != 0)
            {
                if (r_u32(0x80083488u) == 2u)
                    x += half_width;
                else
                    y += half_height;
            }
            SetDefDrawEnv(&frame->draw, x, y, half_width, half_height);
        }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DISPLAY_SCENE_FRAME *frame = display_scene_frame(&camera_views[player], buffer);
            SetDrawEnv(&frame->packet, &frame->draw);
        }
    for (player = 0; player < 2; ++player)
        for (buffer = 0; buffer < 2; ++buffer)
        {
            LINE_F2 *line = &display_scene_frame(&camera_views[player], buffer)->divider;
            line->tag = (line->tag & 0xFFFFFFu) | 0x03000000u;
            line->r0 = line->g0 = line->b0 = 0;
            line->code = 0x40u;
            if (player == 0)
            {
                line->x0 = r_u32(0x80083488u) == 2u ? (sint16)(half_width - 1) : 0;
                line->y0 = r_u32(0x80083488u) == 2u ? 0 : (sint16)(half_height - 1);
                line->x1 = (sint16)(half_width - 1);
                line->y1 = (sint16)(half_height - 1);
            }
            else
            {
                line->x0 = line->y0 = 0;
                line->x1 = r_u32(0x80083488u) == 2u ? 0 : (sint16)(half_width - 1);
                line->y1 = r_u32(0x80083488u) == 2u ? (sint16)(half_height - 1) : 0;
            }
        }
    render_set_geom_offset(half_width / 2, half_height / 2);
    return 0;
}

sint32 display_publish_dimensions(uint32 width, uint32 height)
{
    FUNCTION_MARKER(0x80016144u, "MAIN.EXE");
    menu_fn_8006776c();
    display_state.scene_width = width;
    display_state.scene_height = height;
    FlushCache();
    global_fn_8006777c();
    return 0;
}

void display_fn_800440ec(void)
{
    FUNCTION_MARKER(0x800440ECu, "MAIN.EXE");
}

void menu_reset_counts(void)
{
    FUNCTION_MARKER(0x8004420Cu, "MAIN.EXE");
    menu_asset_counts.labels = menu_asset_defaults.labels;
    menu_asset_counts.sprites = menu_asset_defaults.sprites;
    menu_notice.lines = menu_notice.default_lines;
    menu_asset_counts.extra = menu_asset_defaults.extra;
}

sint32 display_clear_page_region(sint32 page, sint16 x, sint16 y, sint16 width, sint16 height, uint8 red, uint8 green, uint8 blue)
{
    // Original zero seed at MAIN.EXE 800B4090, overridden at 800442D4/800442DC
    static const sint16 origins[3][2] = {{0, 0}, {0, 256}, {512, 0}};
    PSX_RECT rectangle;
    sint32 index = (sint16)page;

    FUNCTION_MARKER(0x80044280u, "MAIN.EXE");
    if (index < 0 || index >= 3)
        abort();
    rectangle.x = (sint16)((uint16)origins[index][0] + (uint16)x);
    rectangle.y = (sint16)((uint16)origins[index][1] + (uint16)y);
    rectangle.w = width;
    rectangle.h = height;
    ClearImage(&rectangle, red, green, blue);
    return DrawSync(0);
}

sint32 display_clear_region(sint16 x, sint16 y)
{
    PSX_RECT rectangle;

    FUNCTION_MARKER(0x80044338u, "MAIN.EXE");
    rectangle.x = x;
    rectangle.y = y;
    rectangle.w = (sint16)display_state.width;
    rectangle.h = (sint16)display_state.height;
    ClearImage(&rectangle, 0u, 0u, 0u);
    return DrawSync(0);
}

sint32 display_move_rect(sint16 x, sint16 y, sint16 destination_x, sint16 destination_y)
{
    PSX_RECT rectangle;

    FUNCTION_MARKER(0x8004438Cu, "MAIN.EXE");
    rectangle.x = x;
    rectangle.y = y;
    rectangle.w = (sint16)display_state.width;
    rectangle.h = (sint16)display_state.height;
    MoveImage(&rectangle, destination_x, destination_y);
    return DrawSync(0);
}

sint32 display_move_page_rect(sint32 source_page, sint32 destination_page)
{
    // Original zero seed at MAIN.EXE 800B4090 with menu width/height overrides
    sint16 horizontal[3] = {0, 0, (sint16)display_state.width};
    sint16 vertical[3] = {0, (sint16)display_state.height, 0};
    sint32 source = (sint16)source_page;
    sint32 destination = (sint16)destination_page;

    FUNCTION_MARKER(0x800443E4u, "MAIN.EXE");
    if (source < 0 || source >= 3 || destination < 0 || destination >= 3)
        abort();
    return display_move_rect(horizontal[source], vertical[source], horizontal[destination], vertical[destination]);
}

sint32 display_clear_quadrants(void)
{
    sint16 width = (sint16)display_state.width;
    sint16 height = (sint16)display_state.height;

    FUNCTION_MARKER(0x80044564u, "MAIN.EXE");
    display_clear_region(0, 0);
    display_clear_region(0, height);
    display_clear_region(width, 0);
    return display_clear_region(width, height);
}

DRAWENV *display_swap_envs(void)
{
    FUNCTION_MARKER(0x8004533Cu, "MAIN.EXE");
    VSync(0);
    return display_swap_after_vsync();
}

void display_set_line_color(sint8 red, sint8 green, sint8 blue)
{
    FUNCTION_MARKER(0x800453B0u, "MAIN.EXE");
    display_state.line_red = (uint8)red;
    display_state.line_green = (uint8)green;
    display_state.line_blue = (uint8)blue;
}

sint32 display_queue_line_segment(sint16 x0, sint16 y0, sint16 x1, sint16 y1)
{
    uint16 count = display_state.line_count;
    LINE_F2 *packet;

    FUNCTION_MARKER(0x800453C4u, "MAIN.EXE");
    if (count >= 200u)
    {
        if (count != 200u)
            abort();
        return 200;
    }
    packet = &display_line_buffer((sint16)display_state.buffer)[count];
    packet->r0 = display_state.line_red;
    packet->g0 = display_state.line_green;
    packet->b0 = display_state.line_blue;
    packet->x0 = x0;
    packet->y0 = y0;
    packet->x1 = x1;
    packet->y1 = y1;
    packet->tag = (packet->tag & 0x00FFFFFFu) | 0x03000000u;
    packet->code = 0x40u;
    display_state.line_count = (uint16)(count + 1u);
    return count + 1;
}

sint32 display_queue_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom)
{
    FUNCTION_MARKER(0x8004545Cu, "MAIN.EXE");
    display_queue_line_segment(left, top, right, top);
    display_queue_line_segment(left, bottom, right, bottom);
    display_queue_line_segment(left, top, left, bottom);
    return display_queue_line_segment(right, top, right, bottom);
}

sint32 display_queue_beveled_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom)
{
    FUNCTION_MARKER(0x80045508u, "MAIN.EXE");
    display_set_line_color((sint8)192, (sint8)192, (sint8)192);
    display_queue_rect_outline(left, top, right, bottom);
    display_set_line_color(32, 32, 32);
    return display_queue_rect_outline((sint16)(left + 1), (sint16)(top + 1), (sint16)(right + 1), (sint16)(bottom + 1));
}

DRAWENV *display_swap_after_vsync(void)
{
    sint32 next;
    DRAWENV *draw_environment;
    DrawSync(0);
    next = display_state.buffer == 0u;
    display_state.buffer = (uint16)next;
    PutDispEnv(display_disp_env(next));
    draw_environment = display_draw_env(next);
    PutDrawEnv(draw_environment);
    return draw_environment;
}
