#ifndef RR_NAME_H
#define RR_NAME_H

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
    uint16 player_slot;
    uint16 phase;
    uint16 glyph;
    uint16 pending_glyph;
    uint16 selected_char;
    uint16 target_char;
    uint16 wheel_angle;
    uint16 wheel_step;
    uint16 previous_angle;
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
    uint16 mode;
    uint16 counter;
    uint16 selection;
    uint16 step;
    uint16 previous;
    uint16 target;
} NAME_REEL;

typedef struct
{
    uint32 code;
    uint16 input_gate;
    NAME_REEL slots[5];
} NAME_REELS;

extern NAME_REELS name_reels;

char *name_player_text(uint32 slot);
void name_reset_players(void);
uint8 *name_save_byte(uint32 offset);

void name_apply_player_name_cheats(void);
sint32 name_refresh_menu_text(void);
sint32 name_player_name_matches(sint16 index, uint32 value);
sint32 name_copy_player_name_bytes(uint16 index, char *output);
void name_commit_chars(void);
sint32 name_reset_wheel(void);
sint32 name_find_glyph(sint8 value);
sint32 name_layout_wheel(sint16 offset_x, sint16 offset_y);
sint32 name_scroll_char(void);
sint32 name_update_wheel(CONTROLLER_STATE *input, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 name_draw_entry(void);
sint32 name_build_glyphs(void);
void name_fn_800591bc(void);
sint32 name_update_reels(CONTROLLER_STATE *input);
sint32 name_check_reels(void);
sint32 name_reset_reels(void);
sint32 time_copy_chars7(char *source, char *destination);

sint32 name_bytes_copy8(char *source, char *destination);
uint32 time_parts_to_tenths(uint32 value);

#endif /* RR_NAME_H */
