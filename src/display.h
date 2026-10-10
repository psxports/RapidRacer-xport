#ifndef RR_DISPLAY_H
#define RR_DISPLAY_H

#include "psx.h"
#include "camera.h"

// Frame selector and line state from MAIN.EXE 800B6B40/6B68/6B84/6B66/6B5C
typedef struct
{
    // Scene frame selector from MAIN.EXE 800B3DA8
    uint32 scene_buffer;
    uint32 scene_width, scene_height;
    uint16 buffer;
    uint16 line_count;
    // Menu dimensions from MAIN.EXE 800B69E4/800B69E6
    uint16 width, height;
    uint8 line_red, line_green, line_blue;
} DISPLAY_STATE;

extern DISPLAY_STATE display_state;

typedef struct
{
    DRAWENV draw;
    DISPENV disp;
    DR_ENV packet;
    LINE_F2 divider;
    PSX_RECT viewport;
} DISPLAY_SCENE_FRAME;

DISPLAY_SCENE_FRAME *display_scene_frame(const CAMERA_STATE *view, sint32 buffer);

LINE_F2 *display_line_buffer(sint32 buffer);
uint32 *display_order_slot(sint32 buffer, sint32 slot);
uint32 *display_begin_menu_frame(void);
DRAWENV *display_draw_env(sint32 buffer);
DISPENV *display_disp_env(sint32 buffer);

sint32 display_sync_swap_buf(void);
sint32 display_init_envs(void);
sint32 display_publish_dimensions(uint32 width, uint32 height);
void display_fn_800440ec(void);
void menu_reset_counts(void);
sint32 display_clear_page_region(sint32 page, sint16 x, sint16 y, sint16 width, sint16 height, uint8 red, uint8 green, uint8 blue);
sint32 display_clear_region(sint16 x, sint16 y);
sint32 display_move_rect(sint16 x, sint16 y, sint16 destination_x, sint16 destination_y);
sint32 display_move_page_rect(sint32 source_page, sint32 destination_page);
sint32 display_clear_quadrants(void);
DRAWENV *display_swap_envs(void);
void display_set_line_color(sint8 red, sint8 green, sint8 blue);

sint32 display_queue_line_segment(sint16 x0, sint16 y0, sint16 x1, sint16 y1);
sint32 display_queue_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

sint32 display_queue_beveled_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

DRAWENV *display_swap_after_vsync(void);

#endif /* RR_DISPLAY_H */
