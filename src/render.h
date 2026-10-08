#ifndef RR_RENDER_H
#define RR_RENDER_H

#include "psx.h"
#include "camera.h"

uint32 render_select_tex_desc(uint32 descriptor);
sint32 render_init_prims(uint32 state, uint32 context);
sint32 render_build_curve_scaled_lut(void);
sint32 render_build_tex_desc_table(sint32 group);
sint32 render_init_ui_prims(void);
uint32 render_target_indicator(uint32 owner, uint32 ordering_table, sint32 alternate);
void render_set_order_depth(uint32 first, uint32 second);
sint32 render_scale_proj_transform(uint32 shift);
void render_set_proj_dist(uint32 projection);
void render_set_geom_offset(sint32 x, sint32 y);
uint32 render_alloc_mode_bufs(void);
uint32 render_select_prim_pool(sint32 alternate);
sint32 render_generate_entries(const sint32 transform[3], uint32 object, sint32 remaining);

sint32 render_build_tex_descs(void);
uint32 render_billboard(uint32 ordering_table, uint32 entries, sint32 count, sint32 lower);
uint32 render_horizon(uint32 owner, const CAMERA_STATE *view);
sint32 render_dispatch_prim_groups(uint32 object, uint32 table, sint32 adjusted, uint32 ordering_table);
sint32 render_init_context(sint32 mode, uint32 unused);
sint32 render_publish_state(uint32 state);
sint32 sprite_build_rotating_pkt(uint32 source, sint32 buffer_index, uint32 state);

sint32 render_submit_active_recs(uint32 primitives, uint32 ordering_table);

sint32 render_activate_recs(uint32 records);

uint32 render_fn_8006c284(uint32 ordering_table, sint32 count);

void render_fn_8006c434(uint32 ordering_table);

sint32 render_build_text_records(uint32 records, uint32 unused, uint32 context);
void render_build_text_packets(void);

#endif /* RR_RENDER_H */
