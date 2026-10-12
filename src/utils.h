#ifndef UTILS_H
#define UTILS_H

#include "psx.h"

typedef union
{
    struct
    {
        sint32 x, y, z;
    };

    sint32 v[3];
} VEC3;

typedef union
{
    struct
    {
        sint32 x, y, z, w;
    };

    sint32 v[4];
} QUAT;

// Arguments must have no side effects
#define clamp(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))

// Preserve the low 32 bits using unsigned arithmetic
#define mul32(left, right) ((sint32)((uint32)(left) * (uint32)(right)))
#define shr28(val) ((sint32)((val) >> 28))
#define sra(val, shift) ((sint32)(val) >> ((shift) & 31))
#define mul64(left, right) ((uint64)((sint64)(sint32)(left) * (sint64)(sint32)(right)))
#define trunc12(val) ((sint32)(val) / (1 << 12))

uint32 swap32(uint32 val);
sint32 abs_max3(sint32 x, sint32 y, sint32 z);

extern uint32 random_seed;
sint32 random_next(void);

sint32 vec_dist_sq(const VEC3 *v0, const VEC3 *v1);

sint32 vec_normalize(const VEC3 *input, SVECTOR *output);

QUAT *quat_mul(QUAT *output, const QUAT *right);

void quat_matrix(const QUAT *quat, MATRIX *matrix);
MATRIX *quat_to_mat(const QUAT *quat, MATRIX *matrix);
QUAT *quat_from_angles(QUAT *output, const VEC3 *angles);
QUAT *quat_from_axis_angle(QUAT *output, uint32 angle);
QUAT *quat_premul_angle(QUAT *output, uint32 angle);
QUAT *quat_rotate_y(QUAT *output, uint32 angle);
QUAT *quat_norm_component(QUAT *vals);
QUAT *quat_set_identity(QUAT *output);
QUAT *quat_norm(QUAT *quat);

sint32 quat_cos(uint32 angle);
sint32 quat_sin(uint32 angle);

#endif /* UTILS_H */
