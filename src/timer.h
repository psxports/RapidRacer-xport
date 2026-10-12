#ifndef TIMER_H
#define TIMER_H

#include "psx.h"

typedef struct
{
    uint8 text[10];
    uint16 parameter;
    uint32 ticks;
} TIME_REC;

extern TIME_REC race_time;
extern TIME_REC race_bonus_time;
extern TIME_REC race_freeze_time;
extern TIME_REC race_end_time;

sint32 time_inc(TIME_REC *dst);
sint32 time_dec(TIME_REC *dst);

sint32 time_format(TIME_REC *dst, sint32 val);
sint32 time_init(TIME_REC *dst);
sint32 time_copy(TIME_REC *dst, const TIME_REC *src);
sint32 time_add(TIME_REC *dst, const TIME_REC *src);
sint32 time_sub(TIME_REC *dst, const TIME_REC *src);

sint32 time_init_disp(void);

#endif /* TIMER_H */
