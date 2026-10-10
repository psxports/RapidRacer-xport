#ifndef RR_TRAIL_H
#define RR_TRAIL_H

#include "psx.h"

struct BOAT;

sint32 trail_init_quad_templates5(void);
sint32 trail_render_marker(struct BOAT *boat, uint32 *ot);
sint32 trail_transform(struct BOAT *boat);
sint32 trail_update(struct BOAT *boat);
sint32 trail_emit(struct BOAT *boat);
uint32 trail_render(struct BOAT *boat, uint32 *ot, sint32 bucket);
sint32 trail_rebase(struct BOAT *boat);
uint32 trail_render_spray(struct BOAT *boat, uint32 *ot, sint32 bucket);
sint32 trail_init_tex_templates8(void);
sint32 trail_render_flare(struct BOAT *boat, uint32 *ot, uint32 depth_limit);
sint32 trail_init(struct BOAT *boat);

#endif /* RR_TRAIL_H */
