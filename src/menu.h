#ifndef MENU_H
#define MENU_H
#include "movie.h"

#include "input.h"
#include "render.h"
#include "sprite.h"

#include "psx.h"

struct RACE_VIEW;
struct GpuTimImage;

void menu_img_upload_pad(uint32 layout);
sint32 menu_img_decode_startup(struct GpuTimImage *img);

struct BOAT_MENU;

typedef enum
{
    MENU_LANGUAGE_ENGLISH = 0,
    MENU_LANGUAGE_FRENCH = 1,
    MENU_LANGUAGE_GERMAN = 2,
    MENU_LANGUAGE_ITALIAN = 3,
    MENU_LANGUAGE_SPANISH = 4
} MENU_LANGUAGE;

typedef struct
{
    uint32 frame;
    // Lifetime fields from MAIN.EXE 800B6A4C/6A6E/6A8C/6C28
    uint16 workspace_active;
    uint16 running;
    // Race transition guard from MAIN.EXE 800B6B52
    uint16 race_trans_paused;
    uint8 input_enabled;
    uint8 refresh;
    sint16 selection;
    sint16 scr;
    sint16 phase;
    uint8 language;
    // Pending sound from MAIN.EXE 800B6AFA, FFFF means idle
    uint16 sound;
    uint32 state_ticks;
    uint16 series_mode;
    uint16 series_confirm;
    uint16 exit_wait;
    uint16 pad_missing;
    sint16 prev_scr;
    sint16 from_scr;
    sint16 saved_choice;
    uint16 reward_notice;
    uint16 level_notice;
    uint16 course_cleared;
    uint16 trophy;
    uint16 cutscene;
    uint16 movie_idx;
} MENU;

typedef struct
{
    uint8 brightness;
    uint8 falling;
} MENU_PULSE;

extern MENU_PULSE menu_pulse;

extern MENU menu_state;

typedef struct
{
    uint16 type;
    uint16 val;
    uint16 boat;
    uint16 course;
    uint16 lines;
    // Published notice count from MAIN.EXE 800B408A
    uint16 default_lines;
} MENU_NOTICE;

extern MENU_NOTICE menu_notice;

typedef struct
{
    uint16 rebuild;
    uint16 sel;
    uint16 loaded;
    uint16 heading;
    uint16 load;
    uint16 requested;
} MENU_TEXTURES;

extern MENU_TEXTURES menu_tex;

typedef struct
{
    sint16 labels;
    sint16 sprs;
    sint16 extra;
} MENU_ASSET_COUNTS;

extern MENU_ASSET_COUNTS menu_asset_defaults;
extern MENU_ASSET_COUNTS menu_asset_counts;

typedef struct
{
    sint16 first;
    sint16 last;
    sint16 sel;
    const uint16 *recs;
} MENU_CHOICE;

typedef struct
{
    uint16 x;
    uint16 y;
    uint16 active;
    uint16 count;
    const uint16 *recs;
} MENU_VISIBILITY;

typedef struct
{
    uint32 type;
    uint16 enabled;
    uint16 visible;
    uint16 rec;
    MENU_CHOICE *choice;
    const MENU_VISIBILITY *visibility;
} MENU_DESC;

typedef struct
{
    uint32 selection;
    uint32 count;
    MENU_DESC *descs;
    const uint16 *commands;
} MENU_CONFIGURATION;

extern MENU_CONFIGURATION menu_cfg[53];

void menu_update_select_mode(void);
sint32 menu_set_resource_prefix(void);

sint32 menu_startup_tim_disp(void);
sint32 menu_load_tex(SPRITE_IMAGE *state, const char *prefix);

sint32 menu_clear_notice(void);

sint32 menu_tex_refresh_if_needed(void);

sint32 menu_save_desc_payloads(sint16 cfg_idx);

void menu_update_grp_desc_select(sint16 cfg_idx, sint16 desc_idx, sint16 val);

void menu_set_grp_desc_val(sint16 cfg_idx, sint16 desc_idx, sint16 val);

sint32 menu_set_selection(sint16 cfg_idx, sint16 selection);
sint32 menu_inc_grp_offset(sint16 idx, sint16 grp);
sint16 menu_query_grp_val(sint16 cfg_idx, sint16 desc_idx);

sint32 menu_update_accel_range(void);
sint32 menu_update_brake_range(void);

sint32 menu_build_player_mode_table(void);

#endif /* MENU_H */
