#ifndef PROFILE_H
#define PROFILE_H

#include "input.h"
#include "sprite.h"

#include "psx.h"
#include <stddef.h>

typedef struct
{
    uint8 rank;
    uint8 profile;
    uint16 lap_time[3];
    uint16 race_time[3];
} PROFILE_SUMMARY;

extern PROFILE_SUMMARY profile_summaries[18];

typedef struct
{
    uint16 boat;
    uint16 time[3];
    char name[5];
} PROFILE_BEST;

typedef struct
{
    PROFILE_BEST lap;
    PROFILE_BEST trial;
    uint16 boats[11];
    uint16 times[11][3];
    char names[11][5];
    uint8 reserved[3];
} PROFILE_RECORD;

extern PROFILE_RECORD profile_recs[18];
extern uint16 race_lap_times[3][3];

typedef struct
{
    uint8 choice[4];
    uint8 limit[4];
    uint8 offset[4];
} PROFILE_GRID;

typedef struct
{
    uint16 completed;
    uint16 grid_row;
    uint16 phase;
    uint8 points[16];
} PROFILE_SERIES;

typedef struct
{
    uint8 attempts;
    uint8 result;
    uint8 state;
} PROFILE_COURSE;

typedef struct
{
    PROFILE_COURSE courses[4][6];
} PROFILE_PROGRESS;

typedef struct
{
    uint8 reset_pending;
    uint8 level_result;
    uint8 max_level;
    uint8 reward_flags;
    uint8 course;
    uint8 level;
    PROFILE_PROGRESS progress;
    uint8 availability[10];
    PROFILE_SERIES series[3];
    PROFILE_GRID grid[10];
} PLAYER_PROFILE;

typedef struct
{
    uint16 rules_slot;
    uint16 menu_slot;
    uint8 mode_choice;
    uint8 pad;
    uint8 slot;

    struct
    {
        uint16 racers;
        uint16 leaders;
        uint16 trailers;
        uint16 menu_slot;
    } saved;
} PROFILE_SELECTION;

extern PROFILE_SELECTION profile_selection;

sint32 profile_format_parts(const uint16 parts[3], char *text);

uint32 profile_time_tenths(const uint16 parts[3]);
sint32 time_equal(const uint16 a[3], const uint16 b[3]);
void profile_reset_grid_flags(void);
uint8 profile_is_unlocked(sint16 idx);
uint32 profile_update_best_times(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 profile_store_best_lap(sint32 name_idx, uint32 unused2, uint32 unused3, uint32 unused4);

PLAYER_PROFILE *profile_at(uint32 slot);
PLAYER_PROFILE *profile_cur(void);
PROFILE_SERIES *profile_get_series(void);
void profile_reset_series(uint32 slot);
uint8 profile_save_read_byte(uint32 offset);
void profile_save_write_byte(uint32 offset, uint8 val);

sint32 profile_unlock(sint16 idx);
sint32 profile_generate_opponents(void);
sint32 profile_merge_unlocks(PLAYER_PROFILE *profile);
sint32 profile_init_ranking_grid(void);

#endif /* PROFILE_H */
