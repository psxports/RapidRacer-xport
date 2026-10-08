#ifndef RR_CAMERA_H
#define RR_CAMERA_H

#include "psx.h"

struct BOAT;

typedef struct
{
    sint32 height;
    sint32 pitch;
    sint32 target_height;
    sint32 min_segments;
    sint16 up_offset;
    sint16 follow_offset;
} CAMERA_SETTINGS;

typedef struct
{
    uint32 route;
    uint32 mode;
    MATRIX rotation;
    sint32 position[3];
    uint32 blend;
    uint16 direction;
    uint16 visible[3];
    uint32 distance;
    uint32 orbit_phase;
    sint16 edge_offset[2];
} CAMERA_STATE;

typedef struct
{
    sint32 position[3];
    sint32 direction[3];
    uint32 route;
    uint32 index;
    sint16 segment;
    sint16 timer;
    uint16 steps;
    uint16 cursor;
    uint32 phase;
    uint32 scale;
} CAMERA_ATTRACT;

extern CAMERA_ATTRACT camera_attract;
extern CAMERA_STATE camera_views[2];
CAMERA_STATE *camera_for_view(uint32 context);
sint32 camera_init(CAMERA_STATE *camera);

sint32 camera_update_trans(const struct BOAT *boat, CAMERA_STATE *control, CAMERA_ATTRACT *state);
sint32 camera_update_target(sint32 index, uint32 descriptor, sint32 position[3], sint32 direction[3]);
sint32 camera_update(CAMERA_STATE *camera, struct BOAT *boat);
sint32 camera_smooth_target_state(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear);
void camera_smooth_edges(sint32 position[3], const struct BOAT *boat, sint16 edge_offset[2]);
sint32 camera_build_xform(sint32 output[3], MATRIX *rotation, const CAMERA_SETTINGS *settings, const MATRIX *pose);
MATRIX *camera_build_target_rot(const sint32 first[3], const sint32 second[3], MATRIX *rotation, const CAMERA_SETTINGS *settings);
sint32 camera_classify_env_dir(CAMERA_STATE *camera);
void camera_config_lighting(const struct BOAT *boat, const MATRIX *input);
sint32 camera_bias_matrix(const CAMERA_STATE *camera, const MATRIX *source, sint32 unbiased);
sint32 camera_bias(const CAMERA_STATE *camera, uint32 source_address);
sint32 camera_mul_lo(sint32 left, sint32 right);

#endif /* RR_CAMERA_H */
