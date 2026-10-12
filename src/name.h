#ifndef NAME_H
#define NAME_H

#include "input.h"

#include "psx.h"

typedef enum
{
    NAME_EDITOR_IDLE = 0,
    NAME_EDITOR_STEP_PREV = 1,
    NAME_EDITOR_STEP_NEXT = 2,
    NAME_EDITOR_COMMIT_STEP = 3,
    NAME_EDITOR_SEEK_PREV = 4,
    NAME_EDITOR_COMMIT_PREV = 5,
    NAME_EDITOR_SEEK_NEXT = 6,
    NAME_EDITOR_COMMIT_NEXT = 7
} NAME_EDITOR_PHASE;

typedef struct
{
    sint16 player_slot;
    sint16 phase;
    sint16 glyph;
    sint16 pending_glyph;
    sint16 sel_char;
    sint16 target_char;
    uint16 wheel_angle;
    uint16 wheel_step;
    uint16 prev_angle;
} NAME_EDITOR;

extern NAME_EDITOR name_editor;

typedef enum
{
    NAME_REEL_IDLE = 0,
    NAME_REEL_SPIN = 1,
    NAME_REEL_SELECTED = 2,
    NAME_REEL_STEP = 3,
    NAME_REEL_STOPPING = 4
} NAME_REEL_MODE;

typedef struct
{
    sint16 mode;
    uint16 counter;
    sint16 selection;
    uint16 step;
    sint16 prev;
    sint16 target;
} NAME_REEL;

typedef struct
{
    uint32 code;
    uint16 input_gate;
    NAME_REEL slots[5];
} NAME_REELS;

extern NAME_REELS name_reels;

extern const char name_code_frac[5];
extern const char name_code_winr[5];
extern const char name_code_hurr[5];

char *name_player_text(uint32 slot);
void name_reset_players(void);
uint8 *name_save_byte(uint32 offset);

void name_apply_player_name_cheats(void);

sint32 name_matches(sint16 idx, const char *code);
sint32 name_copy_player_name_bytes(uint16 idx, char *output);

sint32 name_reset_wheel(void);

sint32 name_update_wheel(PAD *input, uint32 unused2, uint32 unused3, uint32 unused4);

sint32 name_update_reels(PAD *input);
uint8 name_check_reels(void);
sint32 name_reset_reels(void);
sint32 time_copy_chars7(char *src, char *dst);

sint32 name_bytes_copy8(char *src, char *dst);

#endif /* NAME_H */
