#ifndef RR_FRAMEBUFFER_H
#define RR_FRAMEBUFFER_H

#include "psx.h"

uint32 fb_capture_strips(void);
uint32 fb_restore_strips(void);
uint32 fb_remove_vert_offset(void);

sint32 fb_submit_strips(void);

#endif /* RR_FRAMEBUFFER_H */
