#ifndef DISP_H
#define DISP_H

#include "psx.h"
#include "camera.h"

// Frame selector and line state from MAIN.EXE 800B6B40/6B68/6B84/6B66/6B5C
typedef struct
{
    // Scene frame selector from MAIN.EXE 800B3DA8
    uint32 scene_buffer;
    sint32 view_width, view_height;
    uint16 buffer;
    uint16 line_count;
    // Menu dimensions from MAIN.EXE 800B69E4/800B69E6
    uint16 width, height;
    uint8 line_r, line_g, line_b;
} DISP;

extern DISP disp_state;

typedef struct
{
    DRAWENV draw;
    DISPENV disp;
    DR_ENV packet;
    LINE_F2 divider;
    PSX_RECT viewport;
} DISP_SCENE_FRAME;

DISP_SCENE_FRAME *disp_scene_frame(const CAMERA *view, sint32 buffer);

LINE_F2 *disp_line_buffer(sint32 buffer);
uint32 *disp_order_slot(sint32 buffer, sint32 slot);
uint32 *disp_begin_menu_frame(void);
DRAWENV *disp_draw_env(sint32 buffer);
DISPENV *disp_disp_env(sint32 buffer);

sint32 disp_sync_swap_buf(void);
sint32 disp_init_envs(void);

void menu_reset_counts(void);

sint32 disp_clear_region(sint16 x, sint16 y);

sint32 disp_move_page_rect(sint32 src_page, sint32 dst_page);
sint32 disp_clear_quadrants(void);
DRAWENV *disp_swap_envs(void);
void disp_set_line_color(sint8 r, sint8 g, sint8 b);

sint32 disp_queue_line_seg(sint16 x0, sint16 y0, sint16 x1, sint16 y1);
sint32 disp_queue_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

sint32 disp_queue_beveled_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

#endif /* DISP_H */
