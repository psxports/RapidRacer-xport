#include <string.h>
#include "motion.h"
#include "motion_data.h"
#include "global.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stdio.h>

sint32 motion_half_period = 25;
sint32 motion_horizontal_locked;
const sint32 motion_gravity = 512;

static uint64 motion_abs_s64_bits(uint64 value)
{
    if (value & ((uint64)1u << 63))
        return (~value) + 1u;
    return value;
}

static void quat_mul_values(sint32 output[4], const sint32 left[4], const sint32 right[4])
{
    sint32 result[4];
    uint64 value;

    value = math_mul_s32_bits(left[3], right[3]) - math_mul_s32_bits(left[0], right[0]) - math_mul_s32_bits(left[1], right[1]) - math_mul_s32_bits(left[2], right[2]);
    result[3] = math_shift28_s64(value);
    value = math_mul_s32_bits(left[3], right[0]) + math_mul_s32_bits(right[3], left[0]) + math_mul_s32_bits(left[1], right[2]) - math_mul_s32_bits(left[2], right[1]);
    result[0] = math_shift28_s64(value);
    value = math_mul_s32_bits(left[3], right[1]) + math_mul_s32_bits(right[3], left[1]) + math_mul_s32_bits(left[2], right[0]) - math_mul_s32_bits(left[0], right[2]);
    result[1] = math_shift28_s64(value);
    value = math_mul_s32_bits(left[3], right[2]) + math_mul_s32_bits(right[3], left[2]) + math_mul_s32_bits(left[0], right[1]) - math_mul_s32_bits(left[1], right[0]);
    result[2] = math_shift28_s64(value);
    memcpy(output, result, sizeof(result));
}

void quat_matrix(const sint32 quaternion[4], MATRIX *matrix)
{
    sint32 x = (sint16)((uint32)quaternion[0] >> 16);
    sint32 y = (sint16)((uint32)quaternion[1] >> 16);
    sint32 z = (sint16)((uint32)quaternion[2] >> 16);
    sint32 w = (sint16)((uint32)quaternion[3] >> 16);
    sint32 xx2 = math_add_wrap_s32(math_mul_lo_s32(x, x), math_mul_lo_s32(x, x));
    sint32 yy2 = math_add_wrap_s32(math_mul_lo_s32(y, y), math_mul_lo_s32(y, y));
    sint32 zz2 = math_add_wrap_s32(math_mul_lo_s32(z, z), math_mul_lo_s32(z, z));
    sint32 xy2 = math_add_wrap_s32(math_mul_lo_s32(x, y), math_mul_lo_s32(x, y));
    sint32 xz2 = math_add_wrap_s32(math_mul_lo_s32(x, z), math_mul_lo_s32(x, z));
    sint32 yz2 = math_add_wrap_s32(math_mul_lo_s32(y, z), math_mul_lo_s32(y, z));
    sint32 wx2 = math_add_wrap_s32(math_mul_lo_s32(w, x), math_mul_lo_s32(w, x));
    sint32 wy2 = math_add_wrap_s32(math_mul_lo_s32(w, y), math_mul_lo_s32(w, y));
    sint32 wz2 = math_add_wrap_s32(math_mul_lo_s32(w, z), math_mul_lo_s32(w, z));

    matrix->m[0][0] = (uint16)math_sra_signed(math_sub_wrap_s32(math_sub_wrap_s32(0x1000000, zz2), yy2), 12);
    matrix->m[1][1] = (uint16)math_sra_signed(math_sub_wrap_s32(math_sub_wrap_s32(0x1000000, zz2), xx2), 12);
    matrix->m[2][2] = (uint16)math_sra_signed(math_sub_wrap_s32(math_sub_wrap_s32(0x1000000, yy2), xx2), 12);
    matrix->m[1][0] = (uint16)math_sra_signed(math_add_wrap_s32(xy2, wz2), 12);
    matrix->m[0][1] = (uint16)math_sra_signed(math_sub_wrap_s32(xy2, wz2), 12);
    matrix->m[2][0] = (uint16)math_sra_signed(math_sub_wrap_s32(xz2, wy2), 12);
    matrix->m[0][2] = (uint16)math_sra_signed(math_add_wrap_s32(xz2, wy2), 12);
    matrix->m[2][1] = (uint16)math_sra_signed(math_add_wrap_s32(yz2, wx2), 12);
    matrix->m[1][2] = (uint16)math_sra_signed(math_sub_wrap_s32(yz2, wx2), 12);
}

sint32 motion_apply_force(MOTION_STATE *object, const sint32 input[3], const sint16 axis[3], sint32 *a2_out)
{
    MATRIX transposed = {0};
    VECTOR scaled;
    VECTOR rotated;
    sint32 divisor = (sint32)object->forces.mass;
    sint32 negative_x;
    sint32 negative_y;
    sint32 negative_z;
    sint32 cross_x;
    sint32 cross_y;
    sint32 cross_z;

    FUNCTION_MARKER(0x80012048u, "MAIN.EXE");
    scaled.vx = input[0] / divisor;
    scaled.vy = input[1] / divisor;
    scaled.vz = input[2] / divisor;
    scaled.pad = 0;
    PushMatrix();
    TransposeMatrix(&object->transform.pose, &transposed);
    ApplyMatrixLV(&transposed, &scaled, &rotated);
    PopMatrix();
    object->forces.force[0] = math_add_wrap_s32(object->forces.force[0], (sint32)((uint32)scaled.vx));
    object->forces.force[1] = math_add_wrap_s32(object->forces.force[1], (sint32)((uint32)scaled.vy));
    object->forces.force[2] = math_add_wrap_s32(object->forces.force[2], (sint32)((uint32)scaled.vz));

    negative_x = (sint32)(0u - (uint32)rotated.vx);
    negative_y = (sint32)(0u - (uint32)rotated.vy);
    negative_z = (sint32)(0u - (uint32)rotated.vz);
    cross_x = math_sub_wrap_s32(math_mul_lo_s32(axis[2], negative_y), math_mul_lo_s32(axis[1], negative_z));
    cross_y = math_sub_wrap_s32(math_mul_lo_s32(axis[0], negative_z), math_mul_lo_s32(axis[2], negative_x));
    cross_z = math_sub_wrap_s32(math_mul_lo_s32(axis[1], negative_x), math_mul_lo_s32(axis[0], negative_y));
    if (a2_out != NULL)
        *a2_out = cross_x;
    object->forces.torque[0] = math_add_wrap_s32(object->forces.torque[0], (sint32)((uint32)cross_x));
    object->forces.torque[1] = math_add_wrap_s32(object->forces.torque[1], (sint32)((uint32)cross_y));
    object->forces.torque[2] = math_add_wrap_s32(object->forces.torque[2], (sint32)((uint32)cross_z));
    return (sint32)object->forces.torque[2];
}

static sint32 motion_finish_inertia(MOTION_STATE *object, sint32 third)
{
    sint32 first = (sint32)object->forces.inertia[0];
    sint32 first_scaled = math_mul_lo_s32(652, first) / 4096;
    sint32 second;
    sint32 second_scaled;
    sint32 result;

    object->forces.inertia[2] = (sint32)((uint32)third);
    second = (sint32)object->forces.inertia[1];
    object->forces.inertia[0] = (sint32)((uint32)first_scaled);
    second_scaled = math_mul_lo_s32(652, second) / 4096;
    third = (sint32)object->forces.inertia[2];
    object->forces.inertia[1] = (sint32)((uint32)second_scaled);
    result = math_mul_lo_s32(652, third) / 4096;
    object->forces.inertia[2] = (sint32)((uint32)result);
    return result;
}

sint32 motion_init_inertia(MOTION_STATE *object, sint32 divisor, const sint16 axis[3])
{
    sint32 x;
    sint32 y;
    sint32 z;
    sint32 value;

    FUNCTION_MARKER(0x80012408u, "MAIN.EXE");
    object->forces.mass = (sint32)((uint32)divisor);
    z = axis[2];
    y = axis[1];
    value = math_add_wrap_s32(math_mul_lo_s32(z, z), math_mul_lo_s32(y, y)) / 12;
    object->forces.inertia[0] = (sint32)((uint32)value);
    z = axis[2];
    x = axis[0];
    value = math_add_wrap_s32(math_mul_lo_s32(z, z), math_mul_lo_s32(x, x)) / 12;
    object->forces.inertia[1] = (sint32)((uint32)value);
    y = axis[1];
    x = axis[0];
    value = math_add_wrap_s32(math_mul_lo_s32(y, y), math_mul_lo_s32(x, x)) / 12;
    return motion_finish_inertia(object, value);
}

static void motion_angle_values(uint32 angle, sint32 *sine_value, sint32 *cosine_value)
{
    sint32 signed_angle = (sint32)angle;
    sint32 rounded = math_add_wrap_s32(signed_angle, (sint32)(angle >> 31));
    uint32 half = (uint16)(rounded >> 1);
    uint32 table_offset = (uint32)(rounded >> 15) & 0x3FFCu;
    sint32 cosine = (sint32)motion_sine[(table_offset / 4u + 1024u) & 4095u];
    sint32 sine = (sint32)motion_sine[table_offset / 4u];
    uint32 curve = 0x10000000u - (uint32)(((uint64)half * half) / 0xCF817Bu);
    sint32 scale = math_mul_lo_s32(25736, (sint32)half) >> 12;
    uint64 cosine_bits = math_mul_s32_bits(cosine, (sint32)curve) - math_mul_s32_bits(scale, sine);
    uint64 sine_bits = math_mul_s32_bits(scale, cosine) + math_mul_s32_bits(sine, (sint32)curve);

    *cosine_value = math_shift28_s64(cosine_bits);
    *sine_value = math_shift28_s64(sine_bits);
}

static sint32 motion_angle_cos(uint32 angle)
{
    sint32 signed_angle = (sint32)angle;
    sint32 rounded = math_add_wrap_s32(signed_angle, (sint32)(angle >> 31));
    uint32 half = (uint16)(rounded >> 1);
    uint32 table_offset = (uint32)(rounded >> 15) & 0x3FFCu;
    sint32 cosine = (sint32)motion_sine[(table_offset / 4u + 1024u) & 4095u];
    uint32 curve = 0x10000000u - (uint32)(((uint64)half * half) / 0xCF817Bu);
    sint32 scale = math_mul_lo_s32(25736, (sint32)half) >> 12;
    sint32 sine = (sint32)motion_sine[table_offset / 4u];

    return math_shift28_s64(math_mul_s32_bits(cosine, (sint32)curve) - math_mul_s32_bits(scale, sine));
}

static sint32 motion_angle_sin(uint32 angle)
{
    sint32 signed_angle = (sint32)angle;
    sint32 rounded = math_add_wrap_s32(signed_angle, (sint32)(angle >> 31));
    uint32 half = (uint16)(rounded >> 1);
    uint32 table_offset = (uint32)(rounded >> 15) & 0x3FFCu;
    sint32 cosine = (sint32)motion_sine[(table_offset / 4u + 1024u) & 4095u];
    uint32 curve = 0x10000000u - (uint32)(((uint64)half * half) / 0xCF817Bu);
    sint32 scale = math_mul_lo_s32(25736, (sint32)half) >> 12;
    sint32 sine = (sint32)motion_sine[table_offset / 4u];

    return math_shift28_s64(math_mul_s32_bits(scale, cosine) + math_mul_s32_bits(sine, (sint32)curve));
}

sint32 motion_init_pose(MOTION_STATE *object, const sint32 position[3], const sint16 rotation[3])
{
    sint32 angles[3];
    sint32 combined[4];
    uint32 index;

    FUNCTION_MARKER(0x80012278u, "MAIN.EXE");
    for (index = 0; index < 3u; ++index)
    {
        angles[index] = (sint32)((uint32)(sint32)rotation[index] << 16);
        object->position[index] = position[index];
        object->velocity.vector[index] = object->forces.force[index] = object->forces.torque[index] = 0;
        object->forces.inertia[index] = 1;
    }
    object->forces.mass = 1;
    quat_from_angles(object->orientation.quaternion, angles);
    quat_set_identity(object->orientation.attitude);
    quat_set_identity(object->orientation.rotation);
    quat_matrix(object->orientation.quaternion, &object->transform.pose);
    quat_mul_values(combined, object->orientation.attitude, object->orientation.quaternion);
    quat_matrix(combined, &object->transform.matrix);
    for (index = 0; index < 3u; ++index)
        object->transform.matrix.t[index] = object->transform.pose.t[index] = object->position[index];
    return object->position[0];
}

sint32 motion_integrate_pose(MOTION_STATE *object)
{
    sint32 velocity[3];
    sint32 orientation[4];
    sint32 base[4];
    sint32 combined[4];
    sint32 squared_length;
    uint32 index;

    FUNCTION_MARKER(0x80012574u, "MAIN.EXE");
    if (motion_horizontal_locked != 0)
    {
        object->velocity.vector[0] = (sint32)(0);
        object->velocity.vector[2] = (sint32)(0);
    }
    for (index = 0; index < 3u; ++index)
    {
        velocity[index] = (sint32)object->velocity.vector[index];
        object->position[index] = (sint32)((uint32)math_add_wrap_s32((sint32)object->position[index], math_sra_signed(velocity[index], 8)));
    }
    squared_length = math_add_wrap_s32(math_mul_lo_s32(velocity[0], velocity[0]), math_mul_lo_s32(velocity[1], velocity[1]));
    squared_length = math_add_wrap_s32(squared_length, math_mul_lo_s32(velocity[2], velocity[2]));
    object->velocity.speed = (sint32)(SquareRoot0(squared_length));
    quat_mul(object->orientation.quaternion, object->orientation.rotation);
    quat_norm(object->orientation.quaternion);
    for (index = 0; index < 4u; ++index)
    {
        orientation[index] = (sint32)object->orientation.quaternion[index];
        base[index] = (sint32)object->orientation.attitude[index];
    }
    quat_matrix(orientation, &object->transform.pose);
    quat_mul_values(combined, orientation, base);
    quat_matrix(combined, &object->transform.matrix);
    for (index = 0; index < 3u; ++index)
    {
        uint32 value = object->position[index];

        object->transform.matrix.t[index] = (sint32)(value);
        object->transform.pose.t[index] = (sint32)(value);
    }
    return (sint32)object->position[0];
}

sint32 motion_integrate(MOTION_STATE *object)
{
    sint16 angular_velocity[3];
    sint32 angles[3];
    sint32 rotation[4];
    sint32 sine;
    sint32 cosine;
    sint32 temporary[4];
    sint32 product[4];
    uint32 index;
    sint32 result;

    FUNCTION_MARKER(0x800126B8u, "MAIN.EXE");
    SetRotMatrix(&object->transform.pose);
    SetTransMatrix(&object->transform.pose);

    angular_velocity[0] = (sint16)(uint16)object->forces.force[0];
    angular_velocity[1] = (sint16)(uint16)object->forces.force[1];
    angular_velocity[2] = (sint16)(uint16)object->forces.force[2];
    for (index = 0; index < 3u; ++index)
    {
        sint32 torque = (sint32)object->forces.torque[index];
        sint32 factor = (sint32)motion_half_period;
        sint32 inertia = (sint32)object->forces.inertia[index];
        uint32 divisor = (uint32)math_mul_lo_s32(factor, inertia);
        uint32 reciprocal;

        if (divisor == 0u)
            abort();
        reciprocal = UINT32_MAX / divisor;
        angles[index] = (sint32)((sint64)torque * reciprocal >> 16);
    }
    angular_velocity[1] = (sint16)((uint16)angular_velocity[1] - (uint16)motion_gravity);
    cosine = motion_angle_cos((uint32)angles[0]);
    sine = motion_angle_sin((uint32)angles[0]);
    rotation[0] = sine;
    rotation[1] = 0;
    rotation[2] = 0;
    rotation[3] = cosine;
    cosine = motion_angle_cos((uint32)angles[1]);
    sine = motion_angle_sin((uint32)angles[1]);
    temporary[0] = 0;
    temporary[1] = sine;
    temporary[2] = 0;
    temporary[3] = cosine;
    quat_mul_values(product, rotation, temporary);
    for (index = 0; index < 4u; ++index)
        rotation[index] = product[index];
    cosine = motion_angle_cos((uint32)angles[2]);
    sine = motion_angle_sin((uint32)angles[2]);
    temporary[0] = 0;
    temporary[1] = 0;
    temporary[2] = sine;
    temporary[3] = cosine;
    quat_mul_values(product, rotation, temporary);
    for (index = 0; index < 4u; ++index)
        rotation[index] = product[index];
    for (index = 0; index < 3u; ++index)
        object->velocity.vector[index] = math_add_wrap_s32(object->velocity.vector[index], (sint32)((uint32)(sint32)angular_velocity[index]));
    quat_mul(object->orientation.rotation, rotation);
    quat_norm_component(object->orientation.rotation);
    result = motion_integrate_pose(object);
    for (index = 0; index < 3u; ++index)
    {
        object->forces.force[index] = (sint32)(0);
        object->forces.torque[index] = (sint32)(0);
    }
    return result;
}

sint32 *quat_mul(sint32 output[4], const sint32 right[4])
{
    FUNCTION_MARKER(0x8001298Cu, "MAIN.EXE");
    quat_mul_values(output, output, right);
    return output;
}

sint32 *quat_premul(sint32 output[4], const sint32 left[4])
{
    FUNCTION_MARKER(0x80012C90u, "MAIN.EXE");
    quat_mul_values(output, left, output);
    return output;
}

/* Implements the arithmetic from MAIN.EXE:0x80012FC0 */
MATRIX *quat_to_mat(const sint32 quaternion[4], MATRIX *matrix)
{
    FUNCTION_MARKER(0x80012FC0u, "MAIN.EXE");
    quat_matrix(quaternion, matrix);
    return matrix;
}

sint32 *quat_from_angles(sint32 output[4], const sint32 angles[3])
{
    sint32 temporary[4];
    sint32 sine;
    sint32 cosine;

    FUNCTION_MARKER(0x800131ACu, "MAIN.EXE");
    motion_angle_values((uint32)angles[0], &sine, &cosine);
    output[0] = sine;
    output[1] = output[2] = 0;
    output[3] = cosine;
    motion_angle_values((uint32)angles[1], &sine, &cosine);
    temporary[0] = temporary[2] = 0;
    temporary[1] = sine;
    temporary[3] = cosine;
    quat_mul(output, temporary);
    motion_angle_values((uint32)angles[2], &sine, &cosine);
    temporary[0] = temporary[1] = 0;
    temporary[2] = sine;
    temporary[3] = cosine;
    quat_mul(output, temporary);
    return output;
}

sint32 *quat_from_axis_angle(sint32 output[4], uint32 angle)
{
    sint32 sine;
    sint32 cosine;

    FUNCTION_MARKER(0x80013B10u, "MAIN.EXE");
    motion_angle_values(angle, &sine, &cosine);
    output[0] = output[1] = 0;
    output[2] = sine;
    output[3] = cosine;
    return output;
}

sint32 *quat_premul_angle(sint32 output[4], uint32 angle)
{
    sint32 rounded = math_add_wrap_s32((sint32)angle, (sint32)(angle >> 31));
    uint32 half = (uint16)(rounded >> 1);
    uint32 index = ((uint32)(rounded >> 15) & 0x3FFCu) / 4u;
    uint32 half_square = (uint32)math_mul_lo_s32((sint32)half, (sint32)half);
    sint32 cosine = motion_sine[(index + 1024u) & 4095u];
    sint32 sine = motion_sine[index];
    uint32 curve = 0x10000000u - (uint32)(((uint64)half_square * 0x9DE9E65Bu) >> 55);
    sint32 scale = math_mul_lo_s32(25736, (sint32)half) >> 12;
    sint32 temporary[4];

    FUNCTION_MARKER(0x80013CA0u, "MAIN.EXE");
    temporary[3] = math_shift28_s64(math_mul_s32_bits(cosine, (sint32)curve) - math_mul_s32_bits(scale, sine));
    temporary[0] = math_shift28_s64(math_mul_s32_bits(scale, cosine) + math_mul_s32_bits(sine, (sint32)curve));
    temporary[1] = temporary[2] = 0;
    quat_premul(output, temporary);
    return output;
}

sint32 *quat_rotate_y(sint32 output[4], uint32 angle)
{
    sint32 sine;
    sint32 cosine;
    sint32 temporary[4];

    FUNCTION_MARKER(0x80013E50u, "MAIN.EXE");
    motion_angle_values(angle, &sine, &cosine);
    temporary[0] = temporary[2] = 0;
    temporary[1] = sine;
    temporary[3] = cosine;
    quat_premul(output, temporary);
    return output;
}

sint32 *quat_norm_component(sint32 value_address[4])
{
    sint32 x = (sint32)value_address[0];
    sint32 y = (sint32)value_address[1];
    sint32 z = (sint32)value_address[2];
    sint32 w = (sint32)value_address[3];
    uint64 residual = math_mul_s32_bits(x, x);
    uint64 magnitude;

    FUNCTION_MARKER(0x80014294u, "MAIN.EXE");
    residual += math_mul_s32_bits(y, y);
    residual += math_mul_s32_bits(z, z);
    residual += math_mul_s32_bits(w, w);
    residual += UINT64_C(0xFF00000000000000);
    magnitude = motion_abs_s64_bits(residual);
    for (;;)
    {
        sint32 doubled = (sint32)((uint32)w << 1);
        uint64 candidate = residual - (uint64)(sint64)doubled + 1u;

        if (motion_abs_s64_bits(candidate) < magnitude)
            w = (sint32)((uint32)w - 1u);
        else
        {
            candidate = residual + (uint64)(sint64)doubled + 1u;
            if (motion_abs_s64_bits(candidate) >= magnitude)
                break;
            w = (sint32)((uint32)w + 1u);
        }
        residual = candidate;
        magnitude = motion_abs_s64_bits(candidate);
    }
    value_address[3] = (uint32)w;
    return value_address;
}

uint32 motion_isqrt_round(uint64 value)
{
    uint64 bit = UINT64_C(0x4000000000000000);
    uint64 root = 0;

    FUNCTION_MARKER(0x80014480u, "MAIN.EXE");
    while (bit != 0)
    {
        uint64 trial = root + bit;

        if (value >= trial)
        {
            value -= trial;
            root = (root >> 1) + bit;
        }
        else
        {
            root >>= 1;
        }
        bit >>= 2;
    }
    if (root < value * 2u)
        ++root;
    return (uint32)root;
}

sint32 *quat_set_identity(sint32 output[4])
{
    FUNCTION_MARKER(0x80014554u, "MAIN.EXE");
    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    output[3] = 0x10000000u;
    return output;
}

sint32 *quat_norm(sint32 quaternion[4])
{
    sint32 values[4];
    uint64 squared_length = 0;
    uint32 length;
    uint32 index;

    FUNCTION_MARKER(0x80014570u, "MAIN.EXE");
    for (index = 0; index < 4u; ++index)
    {
        values[index] = (sint32)quaternion[index];
        squared_length += math_mul_s32_bits(values[index], values[index]);
    }
    length = motion_isqrt_round(squared_length);
    for (index = 0; index < 4u; ++index)
    {
        sint64 numerator = (sint64)values[index] * 0x10000000;

        quaternion[index] = (sint32)(numerator / (sint64)length);
    }
    return quaternion;
}
