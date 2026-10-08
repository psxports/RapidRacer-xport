#include "game.h"
#include "menu.h"
#include "scene.h"
#include "callbacks.h"
#include "runtime.h"
#include "display.h"
#include "global.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 display_sync_swap_buf(void)
{
    sint32 pass;

    FUNCTION_MARKER(0x8001116Cu, "MAIN.EXE");
    DrawSync(0);
    for (pass = 0; pass < 2; ++pass)
    {
        sint32 buffer = (sint32)r_u32(0x800B3DA8u);

        render_select_prim_pool(buffer);
        buffer = (sint32)r_u32(0x800B3DA8u);
        scene_render_player_scene(0x800DE0F0u, buffer);
        if (r_u32(0x80083478u) == 2u)
        {
            buffer = (sint32)r_u32(0x800B3DA8u);
            scene_render_player_scene(0x800DF098u, buffer);
        }
        game_render_frame();
        buffer = (sint32)r_u32(0x800B3DA8u);
        w_u32(0x800B3DA8u, 1u - (uint32)buffer);
    }
    return 0;
}

sint32 display_init_envs(void)
{
    uint32 configurations[8];
    uint32 selected;
    uint32 mode;
    sint32 flag;
    sint32 x;
    sint32 y;
    sint32 width;
    sint32 height;
    sint32 second_x;
    sint32 second_y;
    sint32 second_width;
    sint32 second_height;
    sint32 half_width;
    sint32 half_height;
    uint32 index;

    FUNCTION_MARKER(0x80014D54u, "MAIN.EXE");
    configurations[0] = r_u32(0x80080AB4u);
    configurations[1] = r_u32(0x80080AB8u);
    configurations[2] = r_u32(0x80080ABCu);
    configurations[3] = r_u32(0x80080AC0u);
    configurations[4] = r_u32(0x80080AC4u);
    configurations[5] = r_u32(0x80080AC8u);
    configurations[6] = r_u32(0x80080ACCu);
    configurations[7] = r_u32(0x80080AD0u);
    mode = r_u32(0x80083478u);
    if (mode == 2u)
    {
        selected = 4u;
        flag = 0;
    }
    else if (r_u32(0x800B3D88u) == 25u)
    {
        selected = 4u;
        flag = 0;
    }
    else
    {
        selected = 0u;
        flag = 1;
    }
    w_u32(0x800DE848u, configurations[selected + 2u]);
    w_u32(0x800DE84Cu, configurations[selected + 3u]);
    w_u32(0x800DEF40u, configurations[selected]);
    w_u32(0x800DEF44u, configurations[selected + 1u]);
    w_u32(0x800DF7F0u, configurations[selected + 2u]);
    w_u32(0x800DF7F4u, configurations[selected + 3u]);
    w_u32(0x800DFEE8u, configurations[selected]);
    w_u32(0x800DFEECu, configurations[selected + 1u]);
    mode = r_u32(0x80083478u);
    width = (sint16)(uint16)configurations[selected + 1u];
    height = (sint16)(uint16)(configurations[selected + 1u] >> 16);
    if (mode == 2u)
    {
        if (r_u32(0x80083488u) == mode)
        {
            half_width = width / 2;
            half_height = height;
        }
        else
        {
            half_width = width;
            half_height = height / 2;
        }
    }
    else
    {
        half_width = width;
        half_height = height;
    }
    x = (sint16)(uint16)configurations[selected];
    y = (sint16)(uint16)(configurations[selected] >> 16);
    second_x = (sint16)(uint16)configurations[selected + 2u];
    second_y = (sint16)(uint16)(configurations[selected + 2u] >> 16);
    second_width = (sint16)(uint16)configurations[selected + 3u];
    second_height = (sint16)(uint16)(configurations[selected + 3u] >> 16);
    display_publish_dimensions((uint32)half_width, (uint32)half_height);
    SetDefDispEnv((DISPENV *)psx_addr(0x800DE1B4u, sizeof(DISPENV)), x, y, width, height);
    SetDefDispEnv((DISPENV *)psx_addr(0x800DE8ACu, sizeof(DISPENV)), second_x, second_y, second_width, second_height);
    w_u16(0x800DE1BEu, 20u);
    w_u16(0x800DE8B6u, 20u);
    w_u8(0x800DE1C4u, (uint8)flag);
    w_u8(0x800DE8BCu, (uint8)flag);
    w_u16(0x800DE1C2u, 256u);
    w_u16(0x800DE8BAu, 256u);
    SetDefDrawEnv((DRAWENV *)psx_addr(0x800DE158u, sizeof(DRAWENV)), second_x, second_y, half_width, half_height);
    SetDefDrawEnv((DRAWENV *)psx_addr(0x800DE850u, sizeof(DRAWENV)), x, y, half_width, half_height);
    mode = r_u32(0x80083488u);
    if (mode == 2u)
    {
        SetDefDrawEnv((DRAWENV *)psx_addr(0x800DF100u, sizeof(DRAWENV)), second_x + half_width, second_y, half_width, half_height);
        SetDefDrawEnv((DRAWENV *)psx_addr(0x800DF7F8u, sizeof(DRAWENV)), x + half_width, y, half_width, half_height);
    }
    else
    {
        SetDefDrawEnv((DRAWENV *)psx_addr(0x800DF100u, sizeof(DRAWENV)), second_x, second_y + half_height, half_width, half_height);
        SetDefDrawEnv((DRAWENV *)psx_addr(0x800DF7F8u, sizeof(DRAWENV)), x, y + half_height, half_width, half_height);
    }
    SetDrawEnv(psx_addr(0x800DE1C8u, 64u), (DRAWENV *)psx_addr(0x800DE158u, sizeof(DRAWENV)));
    SetDrawEnv(psx_addr(0x800DE8C0u, 64u), (DRAWENV *)psx_addr(0x800DE850u, sizeof(DRAWENV)));
    SetDrawEnv(psx_addr(0x800DF170u, 64u), (DRAWENV *)psx_addr(0x800DF100u, sizeof(DRAWENV)));
    SetDrawEnv(psx_addr(0x800DF868u, 64u), (DRAWENV *)psx_addr(0x800DF7F8u, sizeof(DRAWENV)));
    mode = r_u32(0x80083488u);
    if (mode == 2u)
    {
        w_u16(0x800DEEF4u, (uint16)(half_width - 1));
        w_u16(0x800DE7FCu, (uint16)(half_width - 1));
        w_u16(0x800DEEF6u, 0u);
        w_u16(0x800DE7FEu, 0u);
    }
    else
    {
        w_u16(0x800DEEF4u, 0u);
        w_u16(0x800DE7FCu, 0u);
        w_u16(0x800DEEF6u, (uint16)(half_height - 1));
        w_u16(0x800DE7FEu, (uint16)(half_height - 1));
    }
    w_u16(0x800DEEF8u, (uint16)(half_width - 1));
    w_u16(0x800DE800u, (uint16)(half_width - 1));
    w_u16(0x800DEEFAu, (uint16)(half_height - 1));
    w_u16(0x800DE802u, (uint16)(half_height - 1));
    w_u16(0x800DFE9Cu, 0u);
    w_u16(0x800DF7A4u, 0u);
    w_u16(0x800DFE9Eu, 0u);
    w_u16(0x800DF7A6u, 0u);
    mode = r_u32(0x80083488u);
    if (mode == 2u)
    {
        w_u16(0x800DFEA0u, 0u);
        w_u16(0x800DF7A8u, 0u);
        w_u16(0x800DFEA2u, (uint16)(half_height - 1));
        w_u16(0x800DF7AAu, (uint16)(half_height - 1));
    }
    else
    {
        w_u16(0x800DFEA0u, (uint16)(half_width - 1));
        w_u16(0x800DF7A8u, (uint16)(half_width - 1));
        w_u16(0x800DFEA2u, 0u);
        w_u16(0x800DF7AAu, 0u);
    }
    for (index = 0u; index < 2u; ++index)
    {
        uint32 first = 0x800DE7F4u + 1784u * index;
        uint32 second = 0x800DF79Cu + 1784u * index;
        w_u8(first + 3u, 3u);
        w_u8(first + 7u, 0x40u);
        w_u8(second + 3u, 3u);
        w_u8(second + 7u, 0x40u);
        w_u8(first + 4u, 0u);
        w_u8(first + 5u, 0u);
        w_u8(first + 6u, 0u);
        w_u8(second + 4u, 0u);
        w_u8(second + 5u, 0u);
        w_u8(second + 6u, 0u);
    }
    render_set_geom_offset(half_width / 2, half_height / 2);
    return 0;
}

sint32 display_publish_dimensions(uint32 first, uint32 second)
{
    FUNCTION_MARKER(0x80016144u, "MAIN.EXE");
    menu_fn_8006776c();
    w_u32(0x800B3DF8u, first);
    w_u32(0x800B3DFCu, second);
    FlushCache();
    global_fn_8006777c();
    return 0;
}

void display_fn_800440ec(void)
{
    FUNCTION_MARKER(0x800440ECu, "MAIN.EXE");
}

uint32 menu_publish_resource_ptrs(void)
{
    FUNCTION_MARKER(0x8004420Cu, "MAIN.EXE");
    w_u32(0x800EA680u, 0x800957C4u);
    w_u32(0x800EA684u, 0x800E35C0u);
    w_u32(0x800EA688u, 0x800E5B48u);
    w_u32(0x800EA68Cu, 0x800E5DA0u);
    w_u16(0x800B6BA8u, r_u16(0x800B4086u));
    w_u16(0x800B6BAAu, r_u16(0x800B4088u));
    w_u16(0x800B6BACu, r_u16(0x800B408Au));
    w_u16(0x800B6BAEu, r_u16(0x800B408Cu));
    return 0x800E5DA0u;
}

sint32 display_clear_page_region(sint32 page, sint16 x, sint16 y, sint16 width, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint16 height, uint8 red, uint8 green, uint8 blue)
{
    PSX_RECT rectangle;
    sint16 horizontal[3];
    sint16 vertical[3];

    FUNCTION_MARKER(0x80044280u, "MAIN.EXE");
    horizontal[0] = (sint16)r_u16(0x800B4090u);
    horizontal[1] = (sint16)r_u16(0x800B4092u);
    horizontal[2] = (sint16)r_u16(0x800B4094u);
    vertical[0] = (sint16)r_u16(0x800B4090u);
    vertical[1] = 512;
    vertical[2] = 256;
    rectangle.x = (sint16)(horizontal[page] + x);
    rectangle.y = (sint16)(vertical[page] + y);
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
    rectangle.w = (sint16)r_u16(0x800B69E4u);
    rectangle.h = (sint16)r_u16(0x800B69E6u);
    ClearImage(&rectangle, 0u, 0u, 0u);
    return DrawSync(0);
}

sint32 display_move_rect(sint16 x, sint16 y, sint16 destination_x, sint16 destination_y)
{
    PSX_RECT rectangle;

    FUNCTION_MARKER(0x8004438Cu, "MAIN.EXE");
    rectangle.x = x;
    rectangle.y = y;
    rectangle.w = (sint16)r_u16(0x800B69E4u);
    rectangle.h = (sint16)r_u16(0x800B69E6u);
    MoveImage(&rectangle, destination_x, destination_y);
    return DrawSync(0);
}

sint32 display_move_page_rect(sint32 source_page, sint32 destination_page)
{
    uint32 frame;
    uint32 rectangle;
    uint16 extent;
    uint32 source_offset;
    uint32 destination_offset;
    sint16 source_x;
    sint16 source_y;
    sint16 destination_x;
    sint16 destination_y;
    sint32 result;

    FUNCTION_MARKER(0x800443E4u, "MAIN.EXE");
    frame = guest_stack_push(0x28u);
    rectangle = r_u32(0x800B4090u);
    extent = r_u16(0x800B4094u);
    w_u32(frame + 0x10u, rectangle);
    w_u16(frame + 0x14u, extent);
    rectangle = r_u32(0x800B4090u);
    extent = r_u16(0x800B4094u);
    w_u32(frame + 0x18u, rectangle);
    w_u16(frame + 0x1Cu, extent);
    destination_offset = 2u * (uint32)(sint32)(sint16)destination_page;
    rectangle = r_u16(0x800B69E4u);
    extent = r_u16(0x800B69E6u);
    w_u16(frame + 0x1Au, extent);
    w_u16(frame + 0x14u, (uint16)rectangle);
    source_offset = 2u * (uint32)(sint32)(sint16)source_page;
    source_x = (sint16)r_u16(frame + 0x10u + source_offset);
    source_y = (sint16)r_u16(frame + 0x18u + source_offset);
    destination_x = (sint16)r_u16(frame + 0x10u + destination_offset);
    destination_y = (sint16)r_u16(frame + 0x18u + destination_offset);
    result = display_move_rect(source_x, source_y, destination_x, destination_y);
    guest_stack_pop(0x28u);
    return result;
}

sint32 display_clear_quadrants(void)
{
    sint16 width = (sint16)r_u16(0x800B69E4u);
    sint16 height = (sint16)r_u16(0x800B69E6u);

    FUNCTION_MARKER(0x80044564u, "MAIN.EXE");
    display_clear_region(0, 0);
    display_clear_region(0, height);
    display_clear_region(width, 0);
    return display_clear_region(width, height);
}

sint32 display_swap_envs(void)
{
    FUNCTION_MARKER(0x8004533Cu, "MAIN.EXE");
    VSync(0);
    return display_swap_after_vsync();
}

void display_set_line_color(sint8 red, sint8 green, sint8 blue)
{
    FUNCTION_MARKER(0x800453B0u, "MAIN.EXE");
    w_u8(0x800B6B84u, (uint8)red);
    w_u8(0x800B6B66u, (uint8)green);
    w_u8(0x800B6B5Cu, (uint8)blue);
}

sint32 display_queue_line_segment(sint16 x0, sint16 y0, sint16 x1, sint16 y1)
{
    uint16 count = r_u16(0x800B6B68u);
    uint32 packet;

    FUNCTION_MARKER(0x800453C4u, "MAIN.EXE");
    if (count == 200u)
        return 200;
    packet = 0x800E1C78u + 3200u * (uint32)(sint32)(sint16)r_u16(0x800B6B40u) + 16u * count;
    w_u8(packet + 4u, r_u8(0x800B6B84u));
    w_u8(packet + 5u, r_u8(0x800B6B66u));
    {
        uint8 blue = r_u8(0x800B6B5Cu);

        w_u16(packet + 8u, (uint16)x0);
        w_u16(packet + 10u, (uint16)y0);
        w_u16(packet + 12u, (uint16)x1);
        w_u16(packet + 14u, (uint16)y1);
        w_u8(packet + 6u, blue);
    }
    w_u8(packet + 3u, 3u);
    w_u8(packet + 7u, 0x40u);
    count = r_u16(0x800B6B68u);
    w_u16(0x800B6B68u, (uint16)(count + 1u));
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

sint32 display_swap_after_vsync(void)
{
    sint32 next;
    uint32 draw_environment;
    DrawSync(0);
    next = r_u16(0x800B6B40u) == 0u;
    w_u16(0x800B6B40u, (uint16)next);
    PutDispEnv((DISPENV *)psx_addr(0x800DDE74u + 112u * (uint32)next, sizeof(DISPENV)));
    draw_environment = 0x800DDE18u + 112u * (uint32)(sint32)(sint16)r_u16(0x800B6B40u);
    PutDrawEnv((DRAWENV *)psx_addr(draw_environment, sizeof(DRAWENV)));
    return (sint32)draw_environment;
}
