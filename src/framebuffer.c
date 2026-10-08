#include "framebuffer.h"
#include "callbacks.h"
#include "global.h"
#include "xport_trace.h"

uint32 fb_capture_strips(void)
{
    uint32 rectangle = 0x80083744u;
    uint32 buffer_offset;
    sint32 index;

    FUNCTION_MARKER(0x8001121Cu, "MAIN.EXE");
    if (r_u32(0x80083478u) == 2u)
        rectangle = 0x8008375Cu;
    buffer_offset = 0u;
    DrawSync(0);
    for (index = 0; index < 3; ++index)
    {
        uint32 frame = r_u32(0x800B3DA8u);
        uint32 buffer = r_u32(0x800B6A5Cu);
        uint32 rectangle_y = r_u16(rectangle + 2u);
        uint32 frame_y = r_u16(0x800DE84Au + frame * 1784u);
        uint32 destination = buffer_offset + buffer;
        sint32 width;
        sint32 height;
        sint32 pixels;
        uint32 byte_count;

        w_u16(rectangle + 2u, rectangle_y + frame_y);
        w_u32(0x800B6E40u + (uint32)index * 4u, destination);
        width = (sint16)r_u16(rectangle + 4u);
        height = (sint16)r_u16(rectangle + 6u);
        pixels = math_mul_lo_s32(width, height);
        byte_count = (uint32)pixels << 1;
        buffer_offset += byte_count;
        StoreImage((PSX_RECT *)psx_addr(rectangle, sizeof(PSX_RECT)), (uint32 *)psx_addr(destination, byte_count));
        rectangle += sizeof(PSX_RECT);
    }
    return 0;
}

uint32 fb_restore_strips(void)
{
    uint32 rectangle = r_u32(0x80083478u) == 2u ? 0x8008375Cu : 0x80083744u;
    uint32 index;

    FUNCTION_MARKER(0x80011308u, "MAIN.EXE");
    for (index = 0; index < 3u; ++index)
    {
        PSX_RECT *area = (PSX_RECT *)psx_addr(rectangle, sizeof(PSX_RECT));
        uint32 buffer = r_u32(0x800B6E40u + index * 4u);
        sint32 pixels = (sint32)area->w * (sint32)area->h;

        LoadImagePSX(area, (uint32 *)psx_addr(buffer, (size_t)pixels * sizeof(uint16)));
        rectangle += sizeof(PSX_RECT);
    }
    return 0;
}

uint32 fb_remove_vert_offset(void)
{
    uint32 rectangle = 0x80083744u;
    uint32 frame;
    uint32 index;

    FUNCTION_MARKER(0x8001138Cu, "MAIN.EXE");
    if (r_u32(0x80083478u) == 2u)
        rectangle = 0x8008375Cu;
    frame = r_u32(0x800B3DA8u);
    for (index = 0; index < 3u; ++index)
    {
        uint32 current = r_u16(rectangle + 2u);
        uint32 offset = r_u16(0x800DE84Au + frame * 1784u);

        w_u16(rectangle + 2u, current - offset);
        rectangle += sizeof(PSX_RECT);
    }
    return 0;
}

sint32 fb_submit_strips(void)
{
    uint32 descriptor;
    uint32 packet = 0x800B6C30u;
    uint32 value;
    sint32 y;

    FUNCTION_MARKER(0x800113FCu, "MAIN.EXE");
    descriptor = 0x800DE848u + 1784u * r_u32(0x800B3DA8u);
    DrawSync(0);
    SetDrawArea(psx_addr(0x800B6C48u, 12u), (PSX_RECT *)psx_addr(descriptor, sizeof(PSX_RECT)));
    DrawPrim(psx_addr(0x800B6C48u, 12u));
    SetDrawOffset(psx_addr(0x800B6C54u, 12u), (const sint16 *)psx_addr(descriptor, 4u));
    DrawPrim(psx_addr(0x800B6C54u, 12u));
    SetDrawMode1(psx_addr(0x800B6788u, sizeof(DR_TPAGE)), 1, 0, 0);
    DrawPrim(psx_addr(0x800B6788u, sizeof(DR_TPAGE)));
    cb_wait_vblank();
    w_u32(packet, 0x05FFFFFFu);
    value = (uint32)(sint32)(sint16)r_u16(descriptor + 6u);
    w_u32(0x800B6C34u, 0x2A000000u);
    if ((sint32)value > 0)
    {
        y = 0;
        do
        {
            w_u16(packet + 8u, 0u);
            w_u16(packet + 10u, (uint32)y);
            value = r_u16(descriptor + 4u);
            w_u16(packet + 14u, (uint32)y);
            y += 16;
            w_u16(packet + 16u, 0u);
            w_u16(packet + 18u, (uint32)y);
            w_u16(packet + 12u, value);
            value = r_u16(descriptor + 4u);
            w_u16(packet + 22u, (uint32)y);
            w_u16(packet + 20u, value);
            DrawPrim(psx_addr(packet, 24u));
        } while (y < (sint16)r_u16(descriptor + 6u));
    }
    return DrawSync(0);
}
