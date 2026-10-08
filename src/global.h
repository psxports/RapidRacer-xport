#ifndef RR_GLOBAL_H
#define RR_GLOBAL_H

#include "psx.h"

uint32 guest_stack_push(uint32 size);
void guest_stack_pop(uint32 size);

void game_random_seed(uint32 seed);
sint32 game_random_next(void);
sint32 math_mul_lo_s32(sint32 left, sint32 right);
sint32 math_add_wrap_s32(sint32 left, sint32 right);
sint32 math_sub_wrap_s32(sint32 left, sint32 right);
sint32 math_sra_signed(sint32 value, uint32 shift);
uint64 math_mul_s32_bits(sint32 left, sint32 right);
sint32 math_shift28_s64(uint64 value);

sint32 math_div_s32(sint32 numerator, sint32 denominator);
sint32 math_rem_s32(sint32 numerator, sint32 denominator);
sint32 math_abs_s32(sint32 value);
sint32 math_sra_s32(uint32 value, uint32 shift);
sint32 math_trunc_shift12_s32(sint32 value);

sint32 vec_dist_sq(const sint32 first[3], const sint32 second[3]);

sint32 vec_normalize(const VECTOR *input, SVECTOR *output);
sint32 vec_norm(uint32 input, uint32 output);
sint32 math_hsv_to_rgb(uint32 output, uint32 hsv);

void global_fn_8006777c(void);

uint32 guest_swap_stack_ptr(uint32 new_stack);
uint32 global_fn_80068900(uint32 callback);
uint32 global_fn_80068918(void);
sint32 global_fn_8006e9d8(void);

#endif /* RR_GLOBAL_H */
