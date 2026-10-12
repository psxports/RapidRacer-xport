#ifndef HORIZON_H
#define HORIZON_H
#include "psx.h"
#include "render.h"
#include "camera.h"

uint32 horizon_render(RENDER_FRAME *frame, const CAMERA *view);
void horizon_init_showcase(uint32 views, uint32 mode);
void horizon_select_showcase(uint32 buffer, uint32 view);
void horizon_render_showcase(uint32 *ot, const CAMERA *view, uint32 mode, uint32 layout, uint32 players);
uint32 *horizon_ot_entry(uint32 idx);
void horizon_add_backdrop(uint32 *ot);
#endif /* HORIZON_H */
