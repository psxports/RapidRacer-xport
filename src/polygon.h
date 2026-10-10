#include "scene.h"
#ifndef RR_POLYGON_H
#define RR_POLYGON_H

#include "psx.h"

struct ROUTE_SEGMENT;


typedef struct
{
    uint32 color;
    sint32 screen;
    sint32 alternate_screen;
    sint16 depth;
    uint16 flags;
} RR_ANIMATED_VERTEX;

// Packed coordinate values retain GPU midpoint arithmetic
typedef struct
{
    uint32 xy;
    uint16 uv;
} POLY_TEX_VERTEX;

// Packed RGB and command byte retain original Gouraud averaging
typedef struct
{
    uint32 color;
    uint32 xy;
} POLY_SHADED_VERTEX;

typedef struct
{
    uint32 color;
    uint32 xy;
    uint16 uv;
} POLY_TEX_SHADED_VERTEX;

void poly_dispatch_prim_groups(const SCENE_PRIM_GROUP *src, uint32 slot, uint32 *ot, sint32 adjusted, sint32 alternate);
POLY_G3 *poly_subdiv_tri_shaded(POLY_G3 *prim, POLY_G3 *dst);

void poly_copy_tri_shaded_pkt_planes(POLY_SHADED_VERTEX v0, POLY_G3 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2);
POLY_SHADED_VERTEX poly_tri_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask);
POLY_GT3 *poly_subdiv_tri_tex_shaded(POLY_GT3 *prim, POLY_GT3 *dst);
void poly_copy_tri_tex_shaded_pkt_planes(POLY_TEX_SHADED_VERTEX v0, POLY_GT3 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, uint32 clut_word, uint32 tpage_word);
POLY_TEX_SHADED_VERTEX poly_tri_tex_shaded_verts_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask);
POLY_F3 *poly_subdiv_tri_flat(POLY_F3 *prim, POLY_F3 *dst);
void poly_copy_tri_flat_verts(uint32 color, uint32 v0, POLY_F3 *dst, uint32 tag, uint32 v1, uint32 v2);
uint32 poly_tri_flat_verts_avg(uint32 v0, uint32 v1, uint32 mask);
POLY_FT3 *poly_subdiv_tri_tex(POLY_FT3 *prim, POLY_FT3 *dst);
void poly_copy_tri_tex_verts(uint32 color, POLY_TEX_VERTEX v0, POLY_FT3 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, uint32 clut_word, uint32 tpage_word);
POLY_TEX_VERTEX poly_tri_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 word_mask, uint32 half_mask);
POLY_G4 *poly_subdiv_quad_shaded(POLY_G4 *prim, POLY_G4 *dst);
void poly_copy_quad_shaded_pkt_planes(POLY_SHADED_VERTEX v0, POLY_G4 *dst, uint32 tag, POLY_SHADED_VERTEX v1, POLY_SHADED_VERTEX v2, POLY_SHADED_VERTEX v3);
POLY_SHADED_VERTEX poly_quad_shaded_verts_avg(POLY_SHADED_VERTEX v0, POLY_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask);
POLY_GT4 *poly_subdiv_quad_tex_shaded(POLY_GT4 *prim, POLY_GT4 *dst);
void poly_copy_pkt_subdiv_attrs(POLY_TEX_SHADED_VERTEX v0, POLY_GT4 *dst, uint32 tag, POLY_TEX_SHADED_VERTEX v1, POLY_TEX_SHADED_VERTEX v2, POLY_TEX_SHADED_VERTEX v3, uint32 clut_word, uint32 tpage_word);
POLY_TEX_SHADED_VERTEX poly_masked_vert_pairs_avg(POLY_TEX_SHADED_VERTEX v0, POLY_TEX_SHADED_VERTEX v1, uint32 first_mask, uint32 second_mask);
POLY_F4 *poly_subdiv_quad_flat(POLY_F4 *prim, POLY_F4 *dst);
void poly_copy_prim_tex_verts(uint32 color, uint32 v0, POLY_F4 *dst, uint32 tag, uint32 v1, uint32 v2, uint32 v3);
uint32 poly_coord_packed_avg(uint32 v0, uint32 v1, uint32 mask);
POLY_FT4 *poly_subdiv_quad_tex(POLY_FT4 *prim, POLY_FT4 *dst);
void poly_copy_quad_tex_verts(uint32 color, POLY_TEX_VERTEX v0, POLY_FT4 *dst, uint32 tag, POLY_TEX_VERTEX v1, POLY_TEX_VERTEX v2, POLY_TEX_VERTEX v3, uint32 clut_word, uint32 tpage_word);
POLY_TEX_VERTEX poly_quad_tex_verts_avg(POLY_TEX_VERTEX v0, POLY_TEX_VERTEX v1, uint32 word_mask, uint32 half_mask);
void poly_render_quad_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_tex_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_tex_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_clipped_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot);
void poly_render_quad_alt_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_alt_tex_strip(const SCENE_PRIM_FACE *faces, const POLY_FT3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_mixed_strip(const SCENE_PRIM_FACE *faces, const POLY_GT4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_mixed_strip(const SCENE_PRIM_FACE *faces, const POLY_GT3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_alt_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_alt_flat_strip(const SCENE_PRIM_FACE *faces, const POLY_F3 *prims, sint32 count, uint32 *ot);
void poly_render_quad_alt_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G4 *prims, sint32 count, uint32 *ot);
void poly_render_tri_alt_shaded_strip(const SCENE_PRIM_FACE *faces, const POLY_G3 *prims, sint32 count, uint32 *ot);
const struct ROUTE_SEGMENT *mesh_render_pass_packets(const struct ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3]);
const struct ROUTE_SEGMENT *mesh_render_pass(const struct ROUTE_SEGMENT *mesh, sint32 pass_count, const sint32 transform[3]);
const struct ROUTE_SEGMENT *poly_fn_8007fe50(const struct ROUTE_SEGMENT *object, sint32 enabled, const sint32 transform[3]);

#endif /* RR_POLYGON_H */
