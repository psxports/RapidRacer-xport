#ifndef RR_MESH_H
#define RR_MESH_H

#include "psx.h"

struct BOAT;

void mesh_render_tri_flat_lit(uint32 faces, sint32 count);
void mesh_render_quad_flat_lit(uint32 faces, sint32 count);
void mesh_render_tri_flat_lit_tex(uint32 faces, sint32 count);
void mesh_render_quad_flat_lit_tex(uint32 faces, sint32 count);
void mesh_render_tri_gouraud_lit(uint32 faces, sint32 count);
void mesh_render_quad_gouraud_lit(uint32 faces, sint32 count);
void mesh_render_tri_gouraud_lit_tex(uint32 faces, sint32 count);
void mesh_render_quad_gouraud_lit_tex(uint32 faces, sint32 count);
void mesh_dispatch_renderers(uint32 first, uint32 second, uint32 groups);
void mesh_render_quad_faces(uint32 faces, sint32 count, uint32 stride);
void mesh_render_tri_faces(uint32 faces, sint32 count, uint32 stride);
void mesh_dispatch_face_render_groups(uint32 first, uint32 second, uint32 groups);

sint32 mesh_face_groups_project(uint32 descriptor);
uint32 mesh_prepare_player_ot_packet(uint32 state, sint32 player);
sint32 mesh_vis_vert_project(const struct BOAT *boat);
uint32 mesh_prepare_racer_ot_packet(uint32 state, uint32 bucket, sint32 player);
sint32 route_segment_is_in_window(uint32 entry, sint32 offset);
sint32 mesh_order_bucket(uint32 view, const sint32 position[3], sint16 *depth, sint32 *visible);
sint32 mesh_calc_order_bucket(uint32 view, const struct BOAT *boat, uint32 output, sint16 *host_depth);
void mesh_render_racer_model(uint32 view, sint32 player);
void mesh_render_lit_quads(uint32 ordering, uint32 descriptor, const SVECTOR *normal);
sint32 mesh_init_tex_templates(void);
void mesh_fn_80034180(void);
sint32 mesh_fn_8003429c(uint32 state);

#endif /* RR_MESH_H */
