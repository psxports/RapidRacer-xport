#ifndef RR_DISPLAY_H
#define RR_DISPLAY_H

#include "psx.h"

sint32 display_sync_swap_buf(void);
sint32 display_init_envs(void);
sint32 display_publish_dimensions(uint32 first, uint32 second);
void display_fn_800440ec(void);
uint32 menu_publish_resource_ptrs(void);
sint32 display_clear_page_region(sint32 page, sint16 x, sint16 y, sint16 width, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint16 height, uint8 red, uint8 green, uint8 blue);
sint32 display_clear_region(sint16 x, sint16 y);
sint32 display_move_rect(sint16 x, sint16 y, sint16 destination_x, sint16 destination_y);
sint32 display_move_page_rect(sint32 source_page, sint32 destination_page);
sint32 display_clear_quadrants(void);
sint32 display_swap_envs(void);
void display_set_line_color(sint8 red, sint8 green, sint8 blue);

sint32 display_queue_line_segment(sint16 x0, sint16 y0, sint16 x1, sint16 y1);
sint32 display_queue_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

sint32 display_queue_beveled_rect_outline(sint16 left, sint16 top, sint16 right, sint16 bottom);

sint32 display_swap_after_vsync(void);

#endif /* RR_DISPLAY_H */
