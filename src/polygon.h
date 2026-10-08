#ifndef RR_POLYGON_H
#define RR_POLYGON_H

#include "psx.h"

typedef struct
{
    uint32 count_offset;
    uint32 stride;
    uint32 command;
    uint32 screen_offset[3];
    uint32 fourth_offset;
} RR_OBJECT_PACKET_LAYOUT;

typedef struct
{
    uint32 color;
    sint32 screen;
    sint32 alternate_screen;
    sint16 depth;
    uint16 flags;
} RR_ANIMATED_VERTEX;

void poly_dispatch_prim_groups(uint32 groups, uint32 ordering_table, sint32 adjusted, sint32 alternate);
void poly_emit_object_packet_group(uint32 descriptor, uint32 projected, uint32 ordering_entry, uint32 *indices, uint32 *primitive, const RR_OBJECT_PACKET_LAYOUT *layout);
sint32 poly_subdivide_tri_shaded(uint32 primitive, uint32 destination);

void poly_copy_tri_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third);
void poly_tri_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask);
sint32 poly_subdivide_tri_tex_shaded(uint32 primitive, uint32 destination);
void poly_copy_tri_tex_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 first_flag, uint32 second_flag);
void poly_tri_tex_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask);
sint32 poly_subdivide_tri_flat(uint32 primitive, uint32 destination);
void poly_copy_tri_flat_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third);
void poly_tri_flat_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 mask);
sint32 poly_subdivide_tri_tex(uint32 primitive, uint32 destination);
void poly_copy_tri_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 first_flag, uint32 second_flag);
void poly_tri_tex_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 word_mask, uint32 half_mask);
sint32 poly_subdivide_quad_shaded(uint32 primitive, uint32 destination);
void poly_copy_quad_shaded_pkt_planes(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth);
void poly_quad_shaded_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask);
sint32 poly_subdivide_quad_tex_shaded(uint32 primitive, uint32 destination);
void poly_copy_pkt_subdiv_attrs(uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth, uint32 first_flag, uint32 second_flag);
void poly_masked_vert_pairs_avg(uint32 first, uint32 second, uint32 destination, uint32 first_mask, uint32 second_mask);
uint32 poly_subdivide_quad_flat(uint32 primitive, uint32 destination);
void poly_copy_prim_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth);
void poly_coord_packed_avg(uint32 first, uint32 second, uint32 destination, uint32 mask);
uint32 poly_subdivide_quad_tex(uint32 primitive, uint32 destination);
void poly_copy_quad_tex_verts(uint32 primitive, uint32 first, uint32 destination, uint32 tag, uint32 second, uint32 third, uint32 fourth, uint32 first_flag, uint32 second_flag);
void poly_quad_tex_verts_avg(uint32 first, uint32 second, uint32 destination, uint32 word_mask, uint32 half_mask);
void poly_render_quad_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_tex_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_tex_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_clipped_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_alt_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_alt_tex_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_mixed_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_mixed_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_alt_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_alt_flat_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_quad_alt_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_render_tri_alt_shaded_strip(uint32 vertices, uint32 primitives, sint32 count, uint32 ordering_table);
void poly_interp_anim_verts(uint32 first, uint32 second, uint32 destination, sint32 count);
uint32 mesh_render_pass_packets(uint32 mesh, sint32 pass_count, const sint32 transform[3]);
uint32 mesh_render_pass(uint32 mesh, sint32 pass_count, const sint32 transform[3]);
uint32 poly_fn_8007fe50(uint32 object, sint32 enabled, const sint32 transform[3]);

#endif /* RR_POLYGON_H */
