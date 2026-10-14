#ifndef CAMERA_H
#define CAMERA_H

#include "psx.h"

struct RACE_VIEW;

struct ROUTE_SEGMENT;

struct BOAT;

enum
{
    CAMERA_DEFAULT_HEIGHT = 200
};

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
    VEC3F pos;
    float distance;
    float edge[2];
    uint32 mode;
    uint32 ready;
} HD_CAMERA;

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
void hd_camera_sync(const CAMERA *camera, HD_CAMERA *state);
void hd_camera_update(const CAMERA *camera, const struct BOAT *boat, const MATRIXF *pose, HD_CAMERA *state);
void hd_camera_matrix(const CAMERA *camera, const struct BOAT *boat, const MATRIXF *pose, const HD_CAMERA *state, MATRIXF *rotation, VEC3F *pos);

void camera_config_lighting(const struct BOAT *boat, const MATRIX *input);
void camera_get_lighting(const struct BOAT *boat, const MATRIX *input, MATRIX *light, MATRIX *color);
void camera_bias_matrix(const CAMERA *camera, const MATRIX *src, sint32 unbiased);

#endif /* CAMERA_H */
