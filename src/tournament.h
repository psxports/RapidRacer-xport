#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "input.h"

#include "psx.h"

typedef struct
{
    uint8 round;
    uint8 match;
    uint8 winners[5];
    uint8 opponents[5];
    uint8 points[5];
    uint16 leader;
    uint16 match_count;
} TOURNAMENT_MATCH;

extern TOURNAMENT_MATCH tournament_matches;

enum
{
    TOURNAMENT_GRID_ROUNDS = 256,
    TOURNAMENT_GRID_PLAYERS = 5,
    TOURNAMENT_GRID_EMPTY = 0,
    TOURNAMENT_GRID_PASSED = 1,
    TOURNAMENT_GRID_FAILED = 2
};

typedef struct
{
    uint8 round;
    uint8 player;
    uint8 status[TOURNAMENT_GRID_ROUNDS][TOURNAMENT_GRID_PLAYERS];
    uint16 seconds[TOURNAMENT_GRID_ROUNDS][TOURNAMENT_GRID_PLAYERS];
} TOURNAMENT_GRID;

extern TOURNAMENT_GRID tournament_grid;

typedef struct
{
    uint16 parts[3];
    uint16 parameter;
} TOURNAMENT_TIME;

extern TOURNAMENT_TIME tournament_times[5];

typedef struct
{
    uint8 course;
    uint8 player;
    uint8 races[16];
    uint8 points[16];
} TOURNAMENT_CHAMP;

extern TOURNAMENT_CHAMP tournament_champ;

void tournament_clear_extra_rounds(void);

sint32 tournament_reset_grid(void);

sint32 tournament_refresh_grid(void);
sint32 tournament_update_grid_input(PAD *input);
sint32 tournament_sync_grid(void);

sint32 tournament_refresh_player_name(void);
sint32 tournament_rec_result(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);

sint32 tournament_init_round(void);

sint32 tournament_refresh_matches(void);
sint32 tournament_update_match_input(PAD *input);
sint32 tournament_sync_match(void);
sint32 tournament_rec_match_result(void);
sint32 tournament_refresh_standings(void);
sint32 tournament_advance_round(PAD *input);
sint32 tournament_refresh_leader(void);
sint32 tournament_reset_result_rows(void);

sint32 tournament_init_table(void);
sint32 tournament_update_mode_input(PAD *input);
sint32 tournament_reset_text(void);
sint32 tournament_rec_champ_result(void);

#endif /* TOURNAMENT_H */
