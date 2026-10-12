#include "game.h"
#include "framebuffer.h"
#include "disp.h"
#include "callbacks.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdlib.h>

// Native capture storage is independent of transient primitive pools
static uint32 fb_capture_pix[0x20000u / sizeof(uint32)];
static uint32 fb_capture_offsets[3];
static PSX_RECT fb_capture_areas[2][3] = {{{0, 140, 368, 50}, {0, 290, 368, 50}, {0, 340, 368, 50}}, {{0, 74, 512, 25}, {0, 146, 512, 25}, {0, 170, 512, 25}}};

static uint32 fb_capture_size(const PSX_RECT *area, uint32 offset)
{
    uint32 size;
    if (area->w < 0 || area->h < 0)
        abort();
    size = area->w * area->h * sizeof(uint16);
    if ((offset & 3) != 0 || offset > sizeof(fb_capture_pix) || size > sizeof(fb_capture_pix) - offset)
        abort();
    return size;
}

uint32 fb_capture_strips(void)
{
    PSX_RECT *areas = fb_capture_areas[race_cfg.players == 2];
    uint32 offset = 0;
    uint32 idx;

    FUNCTION_MARKER(0x8001121C, "MAIN.EXE");
    DrawSync(0);
    for (idx = 0; idx < 3; ++idx)
    {
        uint32 frame = disp_state.scene_buffer;
        uint32 frame_y = (uint16)disp_scene_frame(&camera_views[0], frame)->viewport.y;
        PSX_RECT *area = &areas[idx];
        uint32 size;
        area->y = area->y + frame_y;
        fb_capture_offsets[idx] = offset;
        size = fb_capture_size(area, offset);
        StoreImage(area, fb_capture_pix + offset / sizeof(uint32));
        offset += size;
    }
    return 0;
}

uint32 fb_restore_strips(void)
{
    PSX_RECT *areas = fb_capture_areas[race_cfg.players == 2];
    uint32 idx;

    FUNCTION_MARKER(0x80011308, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
    {
        uint32 offset = fb_capture_offsets[idx];
        fb_capture_size(&areas[idx], offset);
        LoadImagePSX(&areas[idx], fb_capture_pix + offset / sizeof(uint32));
    }
    return 0;
}

uint32 fb_remove_vert_offset(void)
{
    PSX_RECT *areas = fb_capture_areas[race_cfg.players == 2];
    uint32 frame;
    uint32 idx;

    FUNCTION_MARKER(0x8001138C, "MAIN.EXE");
    frame = disp_state.scene_buffer;
    for (idx = 0; idx < 3; ++idx)
    {
        uint32 offset = (uint16)disp_scene_frame(&camera_views[0], frame)->viewport.y;
        areas[idx].y = areas[idx].y - offset;
    }
    return 0;
}

sint32 fb_submit_strips(void)
{
    static DR_AREA area;
    static DR_OFFSET offset;
    static DR_TPAGE mode;
    static POLY_F4 strip;
    PSX_RECT *viewport = &disp_scene_frame(&camera_views[0], disp_state.scene_buffer)->viewport;
    sint16 origin[2] = {viewport->x, viewport->y};
    sint32 y;

    FUNCTION_MARKER(0x800113FC, "MAIN.EXE");
    DrawSync(0);
    SetDrawArea(&area, viewport);
    DrawPrim(&area);
    SetDrawOffset(&offset, origin);
    DrawPrim(&offset);
    SetDrawMode1(&mode, 1, 0, 0);
    DrawPrim(&mode);
    cb_wait_vblank();
    setPolyF4(&strip);
    termPrim(&strip);
    setSemiTrans(&strip, 1);
    setRGB0(&strip, 0, 0, 0);
    for (y = 0; y < viewport->h; y += 16)
    {
        setXY4(&strip, 0, y, viewport->w, y, 0, y + 16, viewport->w, y + 16);
        DrawPrim(&strip);
    }
    return DrawSync(0);
}
