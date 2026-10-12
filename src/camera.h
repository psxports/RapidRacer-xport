#ifndef CAMERA_H
#define CAMERA_H

#include "psx.h"

struct RACE_VIEW;

struct ROUTE_SEGMENT;

struct BOAT;

typedef struct
{
    const struct ROUTE_SEGMENT *route;
    uint32 mode;
    MATRIX rotation;
    sint32 pos[3];
    uint32 blend;
    uint16 dir;
    uint16 visible[3];
    uint32 distance;
    uint32 orbit_phase;
    sint16 edge_offset[2];
} CAMERA;

typedef struct
{
    sint32 pos[3];
    sint32 dir[3];
    const struct ROUTE_SEGMENT *route;
    uint32 idx;
    sint16 seg;
    sint16 timer;
    uint16 steps;
    uint16 cursor;
    uint32 phase;
    uint32 scale;
} CAMERA_ATTRACT;

extern CAMERA_ATTRACT camera_attract;
extern CAMERA camera_views[2];
extern const MATRIX camera_identity;
CAMERA *camera_for_view(const struct RACE_VIEW *context);
sint32 camera_init(CAMERA *camera);

sint32 camera_update(CAMERA *camera, struct BOAT *boat);

void camera_config_lighting(const struct BOAT *boat, const MATRIX *input);
void camera_bias_matrix(const CAMERA *camera, const MATRIX *src, sint32 unbiased);

#endif /* CAMERA_H */
