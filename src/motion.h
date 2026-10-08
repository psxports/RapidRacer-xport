#ifndef RR_MOTION_H
#define RR_MOTION_H

#include "psx.h"

extern sint32 motion_half_period;
extern sint32 motion_horizontal_locked;
extern const sint32 motion_gravity;

typedef struct
{
    sint32 quaternion[4];
    sint32 attitude[4];
    sint32 rotation[4];
} BOAT_ORIENTATION;

typedef struct
{
    sint32 vector[3];
    sint32 speed;
} BOAT_VELOCITY;

typedef struct
{
    sint32 mass;
    sint32 inertia[3];
    sint32 force[3];
    sint32 torque[3];
} BOAT_FORCES;

typedef struct
{
    MATRIX matrix;
    MATRIX pose;
} BOAT_TRANSFORM;

typedef struct
{
    BOAT_TRANSFORM transform;
    sint32 position[3];
    BOAT_ORIENTATION orientation;
    BOAT_VELOCITY velocity;
    BOAT_FORCES forces;
} MOTION_STATE;

sint32 motion_apply_force(MOTION_STATE *object, const sint32 input[3], const sint16 axis[3], sint32 *cross_x);
sint32 motion_init_inertia(MOTION_STATE *object, sint32 divisor, const sint16 axis[3]);
sint32 motion_init_pose(MOTION_STATE *object, const sint32 position[3], const sint16 rotation[3]);
sint32 motion_integrate_pose(MOTION_STATE *object);
sint32 motion_integrate(MOTION_STATE *object);

sint32 *quat_mul(sint32 output[4], const sint32 right[4]);
sint32 *quat_premul(sint32 output[4], const sint32 left[4]);
void quat_matrix(const sint32 quaternion[4], MATRIX *matrix);
MATRIX *quat_to_mat(const sint32 quaternion[4], MATRIX *matrix);
sint32 *quat_from_angles(sint32 output[4], const sint32 angles[3]);
sint32 *quat_from_axis_angle(sint32 output[4], uint32 angle);
sint32 *quat_premul_angle(sint32 output[4], uint32 angle);
sint32 *quat_rotate_y(sint32 output[4], uint32 angle);
sint32 *quat_norm_component(sint32 values[4]);
uint32 motion_isqrt_round(uint64 value);
sint32 *quat_set_identity(sint32 output[4]);
sint32 *quat_norm(sint32 quaternion[4]);

#endif /* RR_MOTION_H */
