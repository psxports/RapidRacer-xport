#ifndef TRAIL_H
#define TRAIL_H

#include "psx.h"
#include "render.h"

struct BOAT;

sint32 trail_init_quad_templates5(void);
void trail_marker_material(uint32 idx, RENDER_MATERIAL *dst);
sint32 trail_render_marker(struct BOAT *boat, uint32 *ot);
sint32 trail_transform(struct BOAT *boat);
sint32 trail_update(struct BOAT *boat);
sint32 trail_emit(struct BOAT *boat);
uint32 trail_render(struct BOAT *boat, uint32 *ot, sint32 bucket);
sint32 trail_rebase(struct BOAT *boat);
uint32 trail_render_spray(struct BOAT *boat, uint32 *ot, sint32 bucket);
sint32 trail_init_tex_templates8(void);
void hd_trail_prepare_flare(const struct BOAT *boat, VEC3F points[2][4], RENDER_MATERIAL *material, sint32 update);
void trail_prepare_flare(const struct BOAT *boat, SVECTOR points[2][4], RENDER_MATERIAL *material);
sint32 trail_render_flare(struct BOAT *boat, uint32 *ot, uint32 depth_limit);
sint32 trail_init(struct BOAT *boat);

#endif /* TRAIL_H */
