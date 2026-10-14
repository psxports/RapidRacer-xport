#ifndef MENU_SELECT_H
#define MENU_SELECT_H
#include "psx.h"
#include "input.h"
#include "sprite.h"
#include "text.h"

typedef enum
{
    MENU_BOAT_IDLE = 0,
    MENU_BOAT_MOVING = 3,
    MENU_BOAT_SETTLING = 4
} MENU_BOAT_PHASE;

typedef struct
{
    sint16 boat;
    sint16 player;
    uint16 phase;
    uint16 rotation;
} MENU_BOAT_SELECT;

extern MENU_BOAT_SELECT menu_boat_select;

typedef enum
{
    MENU_COURSE_IDLE = 0,
    MENU_COURSE_MOVING = 3,
    MENU_COURSE_SETTLING = 4,
    MENU_COURSE_NEXT_WAIT = 5,
    MENU_COURSE_NEXT_ENTER = 6,
    MENU_COURSE_PREV_WAIT = 7,
    MENU_COURSE_PREV_ENTER = 8
} MENU_COURSE_PHASE;

typedef struct
{
    uint16 blocked;
    sint16 rec;
    sint16 course;
    sint16 level;
    uint16 phase;
    sint16 dir;
} MENU_COURSE_SELECT;

extern MENU_COURSE_SELECT menu_course_select;

float hd_menu_course_step(void);
const UI_RECORD *hd_menu_spr_parent(const UI_RECORD *rec);

sint32 menu_select_advance_boat(sint16 action, PAD *input);
sint32 menu_select_advance(void);
sint32 menu_select_commit(void);
void menu_select_complete(void);
sint32 menu_select_dispatch_results(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 menu_select_input(PAD *input, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 menu_select_init(sint32 initialize, uint32 unused2, uint32 unused3, uint32 unused4);
void menu_select_init_series_menu(void);
sint32 menu_select_leave_course_select(void);
sint32 menu_select_populate_recs(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 menu_select_prepare_series_race(void);
void menu_select_process_results(void);
void menu_select_refresh_series_prompt(void);
sint32 menu_select_reset(void);
uint32 menu_select_reset_rec_styles(void);
void menu_select_restore_backup(void);
sint32 menu_select_restore_grid(void);
sint32 menu_select_restore(sint16 selection);
uint8 *menu_select_save_backup(uint32 offset);
void menu_select_course(void);
sint32 menu_select_start_dir_trans(sint16 dir);
sint32 menu_select_update_boat(void);
void menu_select_update_carousel(sint16 dir, PAD *input);
void menu_select_update_limits(void);
sint32 menu_select_update_trans(PAD *input);
void menu_select_update_course_info(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4);
void menu_select_update_series_menu(PAD *input);
void menu_select_apply_trans(void);
uint8 menu_select_level_available(sint32 requested, uint32 unused2, uint32 unused3, uint32 unused4);
sint32 menu_select_grid_cell(sint32 requested, sint32 column, uint32 unused3, uint32 unused4);
#endif
