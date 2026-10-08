#ifndef RR_TIMER_H
#define RR_TIMER_H

#include "psx.h"

sint32 race_format_time(uint32 output, sint32 value);
sint32 time_get_rec_ticks(uint32 value);
sint32 time_init_rec_zero(uint32 output);
sint32 time_copy_rec(uint32 output, uint32 input);
sint32 time_packed_add(uint32 output, uint32 input);
sint32 time_packed_sub(uint32 output, uint32 input);
sint32 race_timer_increment(uint32 value);
sint32 race_timer_decrement(uint32 value);
void timer_format_text(uint8 *output, sint32 value);

sint32 time_init_display(void);
sint32 race_decode_time_bcd(uint32 time);

#endif /* RR_TIMER_H */
