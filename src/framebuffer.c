#include "framebuffer.h"
#include "display.h"
#include "callbacks.h"
#include "global.h"
#include "xport_trace.h"
#include <stdlib.h>

// Native capture storage is independent of transient primitive pools
static uint32 fb_capture_pixels[0x20000u / sizeof(uint32)];
static uint32 fb_capture_offsets[3];
static PSX_RECT fb_capture_areas[2][3] = {{{0, 140, 368, 50}, {0, 290, 368, 50}, {0, 340, 368, 50}}, {{0, 74, 512, 25}, {0, 146, 512, 25}, {0, 170, 512, 25}}};

static uint32 fb_capture_size(const PSX_RECT *area, uint32 offset)
{
    uint32 size;
    if (area->w < 0 || area->h < 0)
        abort();
    size = (uint32)area->w * (uint32)area->h * sizeof(uint16);
    if ((offset & 3u) != 0u || offset > sizeof(fb_capture_pixels) || size > sizeof(fb_capture_pixels) - offset)
        abort();
    return size;
}

uint32 fb_capture_strips(void)
{
    PSX_RECT *areas = fb_capture_areas[r_u32(0x80083478u) == 2u];
    uint32 offset = 0u;
    uint32 index;

    FUNCTION_MARKER(0x8001121Cu, "MAIN.EXE");
    DrawSync(0);
    for (index = 0; index < 3u; ++index)
    {
        uint32 frame = display_state.scene_buffer;
        uint32 frame_y = (uint16)display_scene_frame(&camera_views[0], (sint32)frame)->viewport.y;
        PSX_RECT *area = &areas[index];
        uint32 size;
        area->y = (sint16)(uint16)((uint16)area->y + frame_y);
        fb_capture_offsets[index] = offset;
        size = fb_capture_size(area, offset);
        StoreImage(area, fb_capture_pixels + offset / sizeof(uint32));
        offset += size;
    }
    return 0;
}

uint32 fb_restore_strips(void)
{
    PSX_RECT *areas = fb_capture_areas[r_u32(0x80083478u) == 2u];
    uint32 index;

    FUNCTION_MARKER(0x80011308u, "MAIN.EXE");
    for (index = 0; index < 3u; ++index)
    {
        uint32 offset = fb_capture_offsets[index];
        fb_capture_size(&areas[index], offset);
        LoadImagePSX(&areas[index], fb_capture_pixels + offset / sizeof(uint32));
    }
    return 0;
}

uint32 fb_remove_vert_offset(void)
{
    PSX_RECT *areas = fb_capture_areas[r_u32(0x80083478u) == 2u];
    uint32 frame;
    uint32 index;

    FUNCTION_MARKER(0x8001138Cu, "MAIN.EXE");
    frame = display_state.scene_buffer;
    for (index = 0; index < 3u; ++index)
    {
        uint32 offset = (uint16)display_scene_frame(&camera_views[0], (sint32)frame)->viewport.y;
        areas[index].y = (sint16)(uint16)((uint16)areas[index].y - offset);
    }
    return 0;
}

sint32 fb_submit_strips(void)
{
    static DR_AREA area;
    static DR_OFFSET offset;
    static DR_TPAGE mode;
    static POLY_F4 strip;
    PSX_RECT *viewport = &display_scene_frame(&camera_views[0], (sint32)display_state.scene_buffer)->viewport;
    sint16 origin[2] = {viewport->x, viewport->y};
    sint32 y;

    FUNCTION_MARKER(0x800113FCu, "MAIN.EXE");
    DrawSync(0);
    SetDrawArea(&area, viewport);
    DrawPrim(&area);
    SetDrawOffset(&offset, origin);
    DrawPrim(&offset);
    SetDrawMode1(&mode, 1, 0, 0);
    DrawPrim(&mode);
    cb_wait_vblank();
    strip.tag = 0x05FFFFFFu;
    strip.color0 = 0x2A000000u;
    for (y = 0; y < viewport->h; y += 16)
    {
        strip.x0 = strip.x2 = 0;
        strip.x1 = strip.x3 = viewport->w;
        strip.y0 = strip.y1 = (sint16)y;
        strip.y2 = strip.y3 = (sint16)(y + 16);
        DrawPrim(&strip);
    }
    return DrawSync(0);
}
