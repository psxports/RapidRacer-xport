#include <stdlib.h>
#include "global.h"
#include "xport_trace.h"

static uint32 rr_guest_stack_pointer = 0x801FFFD0u;

uint32 guest_stack_push(uint32 size)
{
    rr_guest_stack_pointer -= size;
    return rr_guest_stack_pointer;
}

void guest_stack_pop(uint32 size)
{
    rr_guest_stack_pointer += size;
}

void game_random_seed(uint32 seed)
{
    w_u32(0x800D2498u, seed);
}

sint32 game_random_next(void)
{
    w_u32(0x800D2498u, 1103515245u * r_u32(0x800D2498u) + 12345u);
    return (sint32)((r_u32(0x800D2498u) >> 16) & 0x7FFFu);
}

sint32 math_mul_lo_s32(sint32 left, sint32 right)
{
    return (sint32)((uint32)left * (uint32)right);
}

sint32 math_add_wrap_s32(sint32 left, sint32 right)
{
    return (sint32)((uint32)left + (uint32)right);
}

sint32 math_sub_wrap_s32(sint32 left, sint32 right)
{
    return (sint32)((uint32)left - (uint32)right);
}

sint32 math_sra_signed(sint32 value, uint32 shift)
{
    uint32 bits;

    shift &= 31u;
    if (shift == 0u)
        return value;
    bits = (uint32)value >> shift;

    if (value < 0)
        bits |= ~0u << (32u - shift);
    return (sint32)bits;
}

uint64 math_mul_s32_bits(sint32 left, sint32 right)
{
    return (uint64)((sint64)left * (sint64)right);
}

sint32 math_shift28_s64(uint64 value)
{
    uint64 shifted = value >> 28;

    if (value & ((uint64)1u << 63))
        shifted |= ~(((uint64)1u << 36) - 1u);
    return (sint32)(uint32)shifted;
}

sint32 math_div_s32(sint32 numerator, sint32 denominator)
{
    if (denominator == 0 || (numerator == (sint32)0x80000000u && denominator == -1))
        abort();
    return numerator / denominator;
}

sint32 math_rem_s32(sint32 numerator, sint32 denominator)
{
    if (denominator == 0 || (numerator == (sint32)0x80000000u && denominator == -1))
        abort();
    return numerator % denominator;
}

sint32 math_abs_s32(sint32 value)
{
    return value < 0 ? (sint32)(0u - (uint32)value) : value;
}

sint32 math_sra_s32(uint32 value, uint32 shift)
{
    return (sint32)((value >> shift) | ((value & 0x80000000u) != 0u ? (~0u << (32u - shift)) : 0u));
}

sint32 math_trunc_shift12_s32(sint32 value)
{
    uint32 adjusted = (uint32)value;

    if (value < 0)
        adjusted += 0xFFFu;
    return math_sra_s32(adjusted, 12u);
}

sint32 vec_dist_sq(const sint32 first[3], const sint32 second[3])
{
    VECTOR input;
    VECTOR squared;

    FUNCTION_MARKER(0x8002CF14u, "MAIN.EXE");
    input.vx = (sint16)((uint16)first[0] - (uint16)second[0]);
    input.vy = (sint16)((uint16)first[1] - (uint16)second[1]);
    input.vz = (sint16)((uint16)first[2] - (uint16)second[2]);
    input.pad = 0;
    Square0(&input, &squared);
    return (sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz);
}

sint32 vec_normalize(const VECTOR *input, SVECTOR *output)
{
    VECTOR source;
    source.vx = input->vx;
    source.vy = input->vy;
    source.vz = input->vz;
    source.pad = 0;
    sint32 maximum = source.vx < 0 ? (sint32)(0u - (uint32)source.vx) : source.vx;
    sint32 magnitude = source.vy < 0 ? (sint32)(0u - (uint32)source.vy) : source.vy;
    sint32 shift;

    FUNCTION_MARKER(0x80032C40u, "MAIN.EXE");
    if (maximum < magnitude)
        maximum = magnitude;
    magnitude = source.vz < 0 ? (sint32)(0u - (uint32)source.vz) : source.vz;
    if (maximum < magnitude)
        maximum = magnitude;
    shift = (sint32)(18u - (uint32)Lzc(maximum));
    if (shift > 0)
    {
        uint32 amount = (uint32)shift & 31u;
        source.vx >>= amount;
        source.vy >>= amount;
        source.vz >>= amount;
    }
    return VectorNormalS(&source, output);
}

sint32 vec_norm(uint32 input, uint32 output)
{
    VECTOR source;
    SVECTOR normalized;
    sint32 result;

    source.vx = (sint32)r_u32(input);
    source.vy = (sint32)r_u32(input + 4u);
    source.vz = (sint32)r_u32(input + 8u);
    source.pad = 0;
    result = vec_normalize(&source, &normalized);
    w_u16(output, (uint16)normalized.vx);
    w_u16(output + 2u, (uint16)normalized.vy);
    w_u16(output + 4u, (uint16)normalized.vz);
    return result;
}

sint32 math_hsv_to_rgb(uint32 output, uint32 hsv)
{
    sint32 hue = (sint16)r_u16(hsv);
    sint32 saturation = (sint16)r_u16(hsv + 2u);
    sint32 value = (sint16)r_u16(hsv + 4u);
    sint32 sector;
    sint32 fraction;
    sint32 low;
    sint32 falling;
    sint32 rising;

    FUNCTION_MARKER(0x800408B4u, "MAIN.EXE");
    if (hue == 1536)
        hue = 0;
    sector = hue / 256;
    fraction = hue % 256;
    low = value * (256 - saturation) / 256;
    falling = value * (256 - saturation * fraction / 256) / 256;
    rising = value * (256 - saturation * (256 - fraction) / 256) / 256;
    value = value < 0 ? 0 : value > 255 ? 255 : value;
    low = low < 0 ? 0 : low > 255 ? 255 : low;
    falling = falling < 0 ? 0 : falling > 255 ? 255 : falling;
    rising = rising < 0 ? 0 : rising > 255 ? 255 : rising;
    if (sector == 0)
    {
        w_u8(output, (uint8)value);
        w_u8(output + 1u, (uint8)rising);
        w_u8(output + 2u, (uint8)low);
        return 1;
    }
    if (sector == 1)
    {
        w_u8(output, (uint8)falling);
        w_u8(output + 1u, (uint8)value);
        w_u8(output + 2u, (uint8)low);
        return 2;
    }
    if (sector == 2)
    {
        w_u8(output, (uint8)low);
        w_u8(output + 1u, (uint8)value);
        w_u8(output + 2u, (uint8)rising);
        return 3;
    }
    if (sector == 3)
    {
        w_u8(output, (uint8)low);
        w_u8(output + 1u, (uint8)falling);
        w_u8(output + 2u, (uint8)value);
        return 4;
    }
    if (sector == 4)
    {
        w_u8(output, (uint8)rising);
        w_u8(output + 1u, (uint8)low);
        w_u8(output + 2u, (uint8)value);
        return 4;
    }
    w_u8(output, (uint8)value);
    w_u8(output + 1u, (uint8)low);
    w_u8(output + 2u, (uint8)falling);
    return 4;
}

void global_fn_8006777c(void)
{
    FUNCTION_MARKER(0x8006777Cu, "MAIN.EXE");
}

uint32 guest_swap_stack_ptr(uint32 new_stack)
{
    uint32 old_stack = rr_guest_stack_pointer;

    FUNCTION_MARKER(0x8006778Cu, "MAIN.EXE");
    rr_guest_stack_pointer = new_stack;
    return old_stack;
}

uint32 global_fn_80068900(uint32 callback)
{
    uint32 previous = r_u32(0x8009AA0Cu);

    FUNCTION_MARKER(0x80068900u, "MAIN.EXE");
    w_u32(0x8009AA0Cu, callback);
    return previous;
}

uint32 global_fn_80068918(void)
{
    FUNCTION_MARKER(0x80068918u, "MAIN.EXE");
    return r_u32(0x8009AA0Cu);
}

sint32 global_fn_8006e9d8(void)
{
    uint32 value;

    FUNCTION_MARKER(0x8006E9D8u, "MAIN.EXE");
    value = r_u32(0x800D2498u) * 0x41C64E6Du + 0x3039u;
    w_u32(0x800D2498u, value);
    return (sint32)((value >> 16) & 0x7FFFu);
}
