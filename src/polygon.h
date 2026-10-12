#ifndef POLYGON_H
#define POLYGON_H
#include "track.h"

#include "psx.h"

struct ROUTE_SEGMENT;

// Packed coordinate values retain GPU midpoint arithmetic

// Packed RGB and command byte retain original Gouraud averaging

void poly_dispatch_prim_grps(const TRACK_PRIM_GROUP *src, uint32 slot, uint32 *ot, sint32 adjusted, sint32 alternate);

void poly_material_words(uint32 idx, uint32 *page, uint32 *uv0, uint32 *uv1);
POLY_FT4 *poly_subdiv_ft4(POLY_FT4 *prim, POLY_FT4 *dst);
POLY_FT3 *poly_subdiv_ft3(POLY_FT3 *prim, POLY_FT3 *dst);
POLY_GT4 *poly_subdiv_gt4(POLY_GT4 *prim, POLY_GT4 *dst);

#endif /* POLYGON_H */
