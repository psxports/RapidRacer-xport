#ifndef RACE_STATS_H
#define RACE_STATS_H

#include "psx.h"

typedef struct
{
    uint32 ticks, samples, speed_sum;
    sint32 speed, top_speed;
    uint16 lap_pos[3], turbos_used, turbos_collected, bonus_seconds;
    uint8 active, autopilot;
    char lines[10][48];
} RACE_STATS;

typedef struct
{
    uint16 y, duration, clut;
} RACE_STATS_ROW;

extern RACE_STATS race_stats;
extern const RACE_STATS_ROW race_stats_rows[10];

void race_stats_reset(void);
void race_stats_begin(void);
void race_stats_update(void);
sint32 race_stats_hide_hud(void);
float race_stats_x(uint32 row, float fract);
float race_stats_bar_height(float fract);

#endif
