#include "render.h"
#include "game.h"
#include "vehicle.h"
#include "input.h"
#include "tournament.h"
#include "display.h"
#include "sound.h"
#include "ranking.h"
#include "name.h"
#include "profile.h"
#include "vehicle_select.h"
#include "arena.h"
#include "global.h"
#include "menu.h"
#include "results.h"
#include "sprite.h"
#include "text.h"
#include "timer.h"
#include "xport_trace.h"
#include <stdlib.h>

// Initial record times from MAIN.EXE 800995F8
static const sint32 profile_default_times[18] = {
    7926, 9944, 9944, 10202, 9164, 12240, 8032, 8748, 7120, 10232, 8248, 12116, 8336, 9392, 7408, 10406, 9128, 12012,
};

// Initial record names from MAIN.EXE 800991C0; each original entry has six bytes
static const char profile_default_names[18][10][6] = {
    {"DANB", "PAUL", "WILL", "RIKS", "WESZ", "NIKI", "SAM ", "MARK", "PASC", "JUAN"}, {"PAUL", "DANB", "WILL", "MORT", "CMD ", "WESZ", "SAM ", "RIKS", "PASC", "JUAN"}, {"WESZ", "JULE", "DANB", "PAUL", "MORT", "WILL", "SAM ", "RIKS", "PASC", "JUAN"}, {"CMD ", "NIKI", "MULI", "MIKE", "LEON", "MICH", "MULI", "RIKS", "PASC", "JUAN"}, {"WILL", "MORT", "DANB", "WESZ", "PAUL", "JULE", "NIK ", "SAM ", "PASC", "JUAN"}, {"DANB", "PAUL", "RIKS", "WESZ", "MORT", "WILL", "SAM ", "MARK", "PASC", "JUAN"}, {"DANB", "PAUL", "WILL", "RIKS", "WESZ", "NIKI", "SAM ", "MARK", "PASC", "JUAN"}, {"PAUL", "DANB", "WILL", "MORT", "CMD ", "WESZ", "SAM ", "RIKS", "PASC", "JUAN"}, {"WESZ", "JULE", "DANB", "PAUL", "MORT", "WILL", "SAM ", "RIKS", "PASC", "JUAN"},
    {"MULI", "MULI", "MULI", "MULI", "MULI", "MULI", "MULI", "RIKS", "PASC", "JUAN"}, {"WILL", "MORT", "DANB", "WESZ", "PAUL", "JULE", "NIK ", "SAM ", "PASC", "JUAN"}, {"DANB", "PAUL", "RIKS", "WESZ", "MORT", "WILL", "SAM ", "MARK", "PASC", "JUAN"}, {"MIKE", "PAUL", "WILL", "RIKS", "WESZ", "NIKI", "SAM ", "MARK", "PASC", "JUAN"}, {"PAUL", "DANB", "WILL", "MORT", "RWD ", "WESZ", "SAM ", "RIKS", "PASC", "JUAN"}, {"WESZ", "JULE", "DANB", "PAUL", "MORT", "WILL", "SAM ", "RIKS", "PASC", "JUAN"}, {"CMD ", "RWD ", "MULI", "MULI", "MULI", "MULI", "MULI", "RIKS", "PASC", "JUAN"}, {"WILL", "MORT", "DANB", "WESZ", "PAUL", "JULE", "NIK ", "SAM ", "PASC", "JUAN"}, {"DANB", "PAUL", "RIKS", "WESZ", "MORT", "WILL", "SAM ", "MARK", "PASC", "JUAN"},
};

// Selection fields from MAIN.EXE 800E0595 and 800E1B84/86/88/8A
PROFILE_SELECTION profile_selection;

static PLAYER_PROFILE player_profiles[5];

// Summary pool from MAIN.EXE 800E0BC0
PROFILE_SUMMARY profile_summaries[18];

// Best-record pool from MAIN.EXE 800E0CBC; slot ten retains the insertion spill
PROFILE_RECORD profile_records[18];

// Captured lap parts from MAIN.EXE 800E18D4
uint16 race_lap_times[3][3];

typedef struct
{
    uint16 x;
    uint16 y;
    uint16 record;
    uint16 rank;
} PROFILE_COURSE_SLOT;

static PROFILE_COURSE_SLOT profile_course_slots[6];

typedef struct
{
    uint16 order;
    uint16 record;
    uint16 label;
    uint16 x;
    uint16 y;
} PROFILE_COURSE_UI;

// Original MAIN.EXE course maps at 80098D60/6C/8C/98/A4
static const PROFILE_COURSE_UI profile_course_ui[6] = {
    {0, 23, 4, 85, 107}, {1, 24, 5, 125, 83}, {2, 25, 6, 171, 71}, {3, 26, 7, 205, 65}, {4, 27, 8, 252, 60}, {5, 28, 9, 322, 56},
};

// Original MAIN.EXE course text map at 80098D3C
static const uint16 profile_course_text[3][6] = {
    {0, 1, 1, 2, 2, 3},
    {3, 4, 4, 5, 5, 6},
    {7, 7, 8, 8, 9, 9},
};

// Original MAIN.EXE level records at 80098D84
static const uint16 profile_level_records[3] = {18, 19, 20};

typedef struct
{
    uint16 x;
    uint16 y;
    uint16 first;
    uint16 second;
} VEHICLE_CAROUSEL_SLOT;

static VEHICLE_CAROUSEL_SLOT vehicle_carousel[8];

// Original MAIN.EXE record order at 80098D2C
static const uint16 vehicle_carousel_order[8] = {3, 1, 0, 2, 4, 6, 7, 5};

typedef struct
{
    uint8 active;
    uint8 course;
    uint8 level;
    uint8 grid_row;
    PROFILE_PROGRESS progress;
} PROFILE_SELECTION_BACKUP;

static PROFILE_SELECTION_BACKUP profile_selection_backups[3];

PLAYER_PROFILE *profile_at(uint32 slot)
{
    return &player_profiles[slot];
}

void profile_reset_series(uint32 slot)
{
    uint32 row;
    uint32 racer;

    for (row = 0u; row < 3u; ++row)
    {
        PROFILE_SERIES *series = &player_profiles[slot].series[row];

        series->completed = 0u;
        series->phase = 0u;
        for (racer = 0u; racer < 16u; ++racer)
            series->points[racer] = 0u;
    }
}

// Decode only the series region of the compatible save format
static PROFILE_SERIES *profile_save_series(uint32 offset, uint32 *field)
{
    uint32 relative;
    uint32 slot;
    uint32 within;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    within = relative % 274u;
    if (slot >= 5u || within < 208u)
        return NULL;
    within -= 208u;
    *field = within % 22u;
    return &player_profiles[slot].series[within / 22u];
}

static uint8 *profile_save_availability(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    field = relative % 274u;
    if (slot >= 5u || field < 78u || field >= 88u)
        return NULL;
    return &player_profiles[slot].availability[field - 78u];
}

// Decode the grid region at the compatible file boundary
static uint8 *profile_save_grid(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;
    PROFILE_GRID *row;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    slot = relative / 274u;
    field = relative % 274u;
    if (slot >= 5u || field < 88u || field >= 208u)
        return NULL;
    field -= 88u;
    row = &player_profiles[slot].grid[field / 12u];
    field %= 12u;
    if (field < 4u)
        return &row->choice[field];
    if (field < 8u)
        return &row->limit[field - 4u];
    return &row->offset[field - 8u];
}

// Locate native header fields in the compatible file format
static uint8 *profile_save_header(uint32 offset)
{
    uint32 relative;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    if (relative / 274u >= 5u)
        return NULL;
    switch (relative % 274u)
    {
        case 0u:
            return &player_profiles[relative / 274u].reset_pending;
        case 1u:
            return &player_profiles[relative / 274u].course;
        case 2u:
            return &player_profiles[relative / 274u].level;
        case 3u:
            return &player_profiles[relative / 274u].max_level;
        case 4u:
            return &player_profiles[relative / 274u].level_result;
        case 5u:
            return &player_profiles[relative / 274u].reward_flags;
        default:
            return NULL;
    }
}

// Map selection backups only at the compatible file boundary
static uint8 *profile_save_backup(uint32 offset)
{
    uint32 relative;
    uint32 field;
    PROFILE_SELECTION_BACKUP *backup;

    if (offset >= 1458u && offset < 1470u)
    {
        relative = offset - 1458u;
        backup = &profile_selection_backups[relative / 4u];
        switch (relative % 4u)
        {
            case 0u:
                return &backup->active;
            case 1u:
                return &backup->course;
            case 2u:
                return &backup->level;
            default:
                return &backup->grid_row;
        }
    }
    if (offset < 5420u || offset >= 5636u)
        return NULL;
    relative = offset - 5420u;
    backup = &profile_selection_backups[relative / 72u];
    field = relative % 24u;
    if (relative % 72u < 24u)
        return &backup->progress.courses[field / 6u][field % 6u].attempts;
    if (relative % 72u < 48u)
        return &backup->progress.courses[field / 6u][field % 6u].result;
    return &backup->progress.courses[field / 6u][field % 6u].state;
}

// Map profile progression only at the compatible file boundary
static uint8 *profile_save_progress(uint32 offset)
{
    uint32 relative;
    uint32 field;
    PROFILE_COURSE *course;

    if (offset < 88u)
        return NULL;
    relative = offset - 88u;
    field = relative % 274u;
    if (relative / 274u >= 5u || field < 6u || field >= 78u)
        return NULL;
    field -= 6u;
    course = &player_profiles[relative / 274u].progress.courses[(field % 24u) / 6u][field % 6u];
    if (field < 24u)
        return &course->attempts;
    if (field < 48u)
        return &course->result;
    return &course->state;
}

// Map audio options only at the compatible file boundary
static uint16 *profile_save_sound(uint32 offset)
{
    if (offset < 78u || offset >= 86u)
        return NULL;
    switch ((offset - 78u) / 2u)
    {
        case 0u:
            return &sound_options.music;
        case 1u:
            return &sound_options.effects;
        case 2u:
            return &sound_options.mode;
        default:
            return &sound_options.mono;
    }
}

// Map profile choices only at the compatible file boundary
static uint8 *profile_save_choice_byte(uint32 offset)
{
    if (offset == 21u)
        return &profile_selection.slot;
    if (offset == 5640u)
        return &profile_selection.mode_choice;
    if (offset == 5642u)
        return &profile_selection.controller;
    return NULL;
}

static uint16 *profile_save_choice_word(uint32 offset)
{
    if (offset < 5636u || offset >= 5640u)
        return NULL;
    return offset < 5638u ? &profile_selection.rules_slot : &profile_selection.menu_slot;
}

// Map race options only at the compatible file boundary
static uint8 *profile_save_game_option(uint32 offset)
{
    if (offset == 20u)
        return &game_options.unlock_courses;
    if (offset == 22u)
        return &race_selection.ai_variant;
    if (offset == 50u)
        return &game_options.result_filter;
    if (offset == 24u)
        return &game_options.no_current;
    if (offset == 25u)
        return &game_options.split_layout;
    if (offset == 26u)
        return &game_options.players_only;
    if (offset >= 27u && offset <= 28u)
        return &game_options.handicap[offset - 27u];
    return NULL;
}

// Map remaining menu parameter words at the compatible file boundary
static uint16 *profile_save_menu_word(uint32 offset)
{
    if (offset >= 16u && offset < 18u)
        return &game_options.round_limit;
    if (offset >= 18u && offset < 20u)
        return &game_options.level;
    if (offset >= 76u && offset < 78u)
        return &sound_options.slider_value;
    if (offset >= 86u && offset < 88u)
        return &race_selection.format;
    return NULL;
}

// Map result bytes at the compatible file boundary
static uint8 *profile_save_result_byte(uint32 offset)
{
    if (offset == 51u)
        return &result_state.status;
    if (offset == 52u)
        return &result_state.column;
    if (offset == 56u)
        return &result_state.pickups;
    if (offset == 5643u)
        return &game_selection.attract;
    return NULL;
}

// Map participant arrays at the compatible file boundary
static uint8 *profile_save_participant_byte(uint32 offset)
{
    if (offset >= 29u && offset < 45u)
        return &race_selection.ranks[offset - 29u];
    if (offset >= 45u && offset < 50u)
        return &race_selection.boats[offset - 45u];
    return NULL;
}

// Map track choices at the compatible file boundary
static uint16 *profile_save_track(uint32 offset)
{
    if (offset >= 60u && offset < 76u)
        return &sound_options.tracks[(offset - 60u) / 2u];
    return NULL;
}

// Map summary fields only at the compatible file boundary
static uint8 *profile_save_summary_byte(uint32 offset)
{
    uint32 field;
    PROFILE_SUMMARY *summary;

    if (offset < 1600u || offset >= 1852u)
        return NULL;
    field = (offset - 1600u) % 14u;
    summary = &profile_summaries[(offset - 1600u) / 14u];
    if (field == 0u)
        return &summary->rank;
    if (field == 1u)
        return &summary->profile;
    return NULL;
}

static uint16 *profile_save_summary_word(uint32 offset)
{
    uint32 field;
    PROFILE_SUMMARY *summary;

    if (offset < 1600u || offset >= 1852u)
        return NULL;
    field = (offset - 1600u) % 14u;
    summary = &profile_summaries[(offset - 1600u) / 14u];
    if (field >= 2u && field < 8u)
        return &summary->lap_time[(field - 2u) / 2u];
    if (field >= 8u)
        return &summary->race_time[(field - 8u) / 2u];
    return NULL;
}

// Map best records only at the compatible file boundary
static uint8 *profile_save_record_byte(uint32 offset)
{
    uint32 field;
    PROFILE_RECORD *record;

    if (offset < 1852u || offset >= 4948u)
        return NULL;
    field = (offset - 1852u) % 172u;
    record = &profile_records[(offset - 1852u) / 172u];
    if (field >= 8u && field < 13u)
        return (uint8 *)&record->lap.name[field - 8u];
    if (field >= 22u && field < 27u)
        return (uint8 *)&record->trial.name[field - 22u];
    if (field >= 116u && field < 171u)
        return (uint8 *)&record->names[(field - 116u) / 5u][(field - 116u) % 5u];
    if (field == 13u)
        return &record->reserved[0];
    if (field == 27u)
        return &record->reserved[1];
    if (field == 171u)
        return &record->reserved[2];
    return NULL;
}

static uint16 *profile_save_record_word(uint32 offset)
{
    uint32 field;
    PROFILE_RECORD *record;

    if (offset < 1852u || offset >= 4948u)
        return NULL;
    field = (offset - 1852u) % 172u;
    record = &profile_records[(offset - 1852u) / 172u];
    if (field < 2u)
        return &record->lap.boat;
    if (field < 8u)
        return &record->lap.time[(field - 2u) / 2u];
    if (field >= 14u && field < 16u)
        return &record->trial.boat;
    if (field >= 16u && field < 22u)
        return &record->trial.time[(field - 16u) / 2u];
    if (field >= 28u && field < 50u)
        return &record->boats[(field - 28u) / 2u];
    if (field >= 50u && field < 116u)
        return &record->times[(field - 50u) / 6u][((field - 50u) % 6u) / 2u];
    return NULL;
}

// Map captured lap times only at the compatible file boundary
static uint16 *profile_save_lap_word(uint32 offset)
{
    if (offset < 4948u || offset >= 4966u)
        return NULL;
    return &race_lap_times[(offset - 4948u) / 6u][((offset - 4948u) % 6u) / 2u];
}

// Map participant fields only at the compatible file boundary
static uint8 *profile_save_race_participant_byte(uint32 offset)
{
    RACE_PARTICIPANT *participant;
    uint32 field;

    if (offset < 1472u || offset >= 1600u)
        return NULL;
    participant = &race_participants[(offset - 1472u) / 8u];
    field = (offset - 1472u) % 8u;
    if (field == 0u)
        return &participant->boat;
    if (field == 1u)
        return &participant->variant;
    if (field < 4u)
        return &participant->reserved[field - 2u];
    return NULL;
}

static uint32 *profile_save_race_participant_word(uint32 offset)
{
    if (offset < 1472u || offset >= 1600u || (offset - 1472u) % 8u < 4u)
        return NULL;
    return &race_participants[(offset - 1472u) / 8u].mode;
}

// Map tournament matches only at the compatible file boundary
static uint8 *profile_save_tournament_match(uint32 offset)
{
    if (offset == 5356u)
        return &tournament_matches.round;
    if (offset == 5357u)
        return &tournament_matches.match;
    if (offset >= 5358u && offset < 5363u)
        return &tournament_matches.winners[offset - 5358u];
    if (offset >= 5363u && offset < 5368u)
        return &tournament_matches.opponents[offset - 5363u];
    if (offset >= 5368u && offset < 5373u)
        return &tournament_matches.points[offset - 5368u];
    return NULL;
}

// Preserve the first twenty grid rounds in the compatible save format
static uint8 *profile_save_tournament_grid_byte(uint32 offset)
{
    if (offset == 5014u)
        return &tournament_grid.round;
    if (offset == 5015u)
        return &tournament_grid.player;
    if (offset >= 5016u && offset < 5116u)
        return &tournament_grid.status[(offset - 5016u) / 5u][(offset - 5016u) % 5u];
    return NULL;
}

static uint16 *profile_save_tournament_grid_word(uint32 offset)
{
    if (offset < 5116u || offset >= 5316u)
        return NULL;
    return &tournament_grid.seconds[(offset - 5116u) / 10u][((offset - 5116u) % 10u) / 2u];
}

// Map tournament times only at the compatible file boundary
static uint16 *profile_save_tournament_time(uint32 offset)
{
    TOURNAMENT_TIME *time;
    uint32 field;

    if (offset < 5316u || offset >= 5356u)
        return NULL;
    time = &tournament_times[(offset - 5316u) / 8u];
    field = ((offset - 5316u) % 8u) / 2u;
    return field < 3u ? &time->parts[field] : &time->parameter;
}

// Map championship state only at the compatible file boundary
static uint8 *profile_save_tournament_champ(uint32 offset)
{
    if (offset == 5386u)
        return &tournament_champ.course;
    if (offset == 5387u)
        return &tournament_champ.player;
    if (offset >= 5388u && offset < 5404u)
        return &tournament_champ.races[offset - 5388u];
    if (offset >= 5404u && offset < 5420u)
        return &tournament_champ.points[offset - 5404u];
    return NULL;
}

// Map selection and result words at the compatible file boundary
static uint16 *profile_save_selection_tail(uint32 offset)
{
    switch (offset & ~1u)
    {
        case 5660u:
            return &race_selection.special;
        case 5668u:
            return &race_selection.resource;
        case 5678u:
            return &result_state.grid_count;
        case 5680u:
            return &result_state.count;
        case 5682u:
            return &result_state.quit_player;
        default:
            return NULL;
    }
}

// Preserve opaque gaps from the original compatible save layout
static uint8 profile_save_opaque[66];

static uint8 *profile_save_opaque_byte(uint32 offset)
{
    static const struct
    {
        uint16 offset;
        uint8 length;
        uint8 first;
    } spans[] = {
        {23u, 1u, 0u}, {53u, 2u, 1u}, {57u, 3u, 3u}, {1470u, 2u, 6u}, {4966u, 18u, 8u}, {5373u, 13u, 26u}, {5641u, 1u, 39u}, {5644u, 16u, 40u}, {5662u, 2u, 56u}, {5670u, 8u, 58u},
    };

    uint32 index;

    for (index = 0u; index < sizeof(spans) / sizeof(spans[0]); ++index)
    {
        if (offset >= spans[index].offset && offset - spans[index].offset < spans[index].length)
            return &profile_save_opaque[spans[index].first + offset - spans[index].offset];
    }
    return NULL;
}

// Native save encoder used by mc_save_segments_gather (MAIN.EXE 0x8004B644)
uint8 profile_save_read_byte(uint32 offset)
{
    uint8 *name = name_save_byte(offset);
    uint8 *opaque_byte = profile_save_opaque_byte(offset);
    uint16 *tail_word = profile_save_selection_tail(offset);
    uint8 *champ_byte = profile_save_tournament_champ(offset);
    uint16 *time_word = profile_save_tournament_time(offset);
    uint8 *grid_byte = profile_save_tournament_grid_byte(offset);
    uint16 *grid_word = profile_save_tournament_grid_word(offset);
    uint8 *match_byte = profile_save_tournament_match(offset);
    uint8 *participant_byte = profile_save_race_participant_byte(offset);
    uint32 *participant_word = profile_save_race_participant_word(offset);
    uint16 *lap_word = profile_save_lap_word(offset);
    uint8 *record_byte = profile_save_record_byte(offset);
    uint16 *record_word = profile_save_record_word(offset);
    uint8 *summary_byte = profile_save_summary_byte(offset);
    uint16 *summary_word = profile_save_summary_word(offset);
    uint8 *participant = profile_save_participant_byte(offset);
    uint16 *track = profile_save_track(offset);
    uint8 *result_byte = profile_save_result_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = profile_save_backup(offset);
    uint8 *header = profile_save_header(offset);
    uint8 *grid = profile_save_grid(offset);
    uint8 *availability = profile_save_availability(offset);
    uint32 field;
    PROFILE_SERIES *series = profile_save_series(offset, &field);
    uint16 value;
    uint16 *menu_word = profile_save_menu_word(offset);
    uint8 *option = profile_save_game_option(offset);
    uint8 *choice = profile_save_choice_byte(offset);
    uint16 *selected = profile_save_choice_word(offset);
    uint16 *audio = profile_save_sound(offset);

    if (opaque_byte != NULL)
        return *opaque_byte;
    if (tail_word != NULL)
        return (uint8)(*tail_word >> (8u * (offset & 1u)));
    if (champ_byte != NULL)
        return *champ_byte;
    if (time_word != NULL)
        return (uint8)(*time_word >> (8u * (offset & 1u)));
    if (grid_byte != NULL)
        return *grid_byte;
    if (grid_word != NULL)
        return (uint8)(*grid_word >> (8u * (offset & 1u)));
    if (match_byte != NULL)
        return *match_byte;
    if (participant_byte != NULL)
        return *participant_byte;
    if (participant_word != NULL)
        return (uint8)(*participant_word >> (8u * (offset & 3u)));
    if (lap_word != NULL)
        return (uint8)(*lap_word >> (8u * (offset & 1u)));
    if (record_byte != NULL)
        return *record_byte;
    if (record_word != NULL)
        return (uint8)(*record_word >> (8u * (offset & 1u)));
    if (summary_byte != NULL)
        return *summary_byte;
    if (summary_word != NULL)
        return (uint8)(*summary_word >> (8u * (offset & 1u)));
    if (track != NULL)
        return (uint8)(*track >> (8u * (offset & 1u)));
    if (participant != NULL)
        return *participant;
    if (result_byte != NULL)
        return *result_byte;
    if (menu_word != NULL)
        return (uint8)(*menu_word >> (8u * (offset & 1u)));
    if (option != NULL)
        return *option;
    if (choice != NULL)
        return *choice;
    if (selected != NULL)
        return (uint8)(*selected >> (8u * (offset & 1u)));
    if (audio != NULL)
        return (uint8)(*audio >> (8u * (offset & 1u)));
    if (offset < 8u)
    {
        switch (offset / 2u)
        {
            case 0u:
                value = game_selection.selection;
                break;
            case 1u:
                value = game_selection.mode;
                break;
            case 2u:
                value = game_selection.rules;
                break;
            default:
                value = game_selection.previous_mode;
                break;
        }
        return (uint8)(value >> (8u * (offset & 1u)));
    }
    if (offset >= 8u && offset < 16u)
    {
        switch (offset)
        {
            case 8u:
                return (uint8)game_selection.flags;
            case 9u:
                return (uint8)(game_selection.flags >> 8);
            case 10u:
                return game_selection.ready;
            case 11u:
                return game_selection.event;
            case 12u:
                return game_selection.event_arg;
            case 13u:
                return game_selection.menu_variant;
            case 14u:
                return (uint8)game_selection.players;
            default:
                return (uint8)(game_selection.players >> 8);
        }
    }
    if (offset == 55u)
        return menu_state.language;
    if (offset >= 5664u && offset < 5668u)
        return (uint8)(name_reels.code >> (8u * (offset - 5664u)));
    if (name != NULL)
        return *name;
    if (progress != NULL)
        return *progress;
    if (backup != NULL)
        return *backup;
    if (header != NULL)
        return *header;
    if (grid != NULL)
        return *grid;
    if (availability != NULL)
        return *availability;
    if (series == NULL)
        return 0u;
    if (field >= 6u)
        return series->points[field - 6u];
    value = field < 2u ? series->completed : field < 4u ? series->grid_row : series->phase;
    return (uint8)(value >> (8u * (field & 1u)));
}

// Native save decoder used by mc_save_segments_scatter (MAIN.EXE 0x8004B6BC)
void profile_save_write_byte(uint32 offset, uint8 value)
{
    uint8 *name = name_save_byte(offset);
    uint8 *opaque_byte = profile_save_opaque_byte(offset);
    uint16 *tail_word = profile_save_selection_tail(offset);
    uint8 *champ_byte = profile_save_tournament_champ(offset);
    uint16 *time_word = profile_save_tournament_time(offset);
    uint8 *grid_byte = profile_save_tournament_grid_byte(offset);
    uint16 *grid_word = profile_save_tournament_grid_word(offset);
    uint8 *match_byte = profile_save_tournament_match(offset);
    uint8 *participant_byte = profile_save_race_participant_byte(offset);
    uint32 *participant_word = profile_save_race_participant_word(offset);
    uint16 *lap_word = profile_save_lap_word(offset);
    uint8 *record_byte = profile_save_record_byte(offset);
    uint16 *record_word = profile_save_record_word(offset);
    uint8 *summary_byte = profile_save_summary_byte(offset);
    uint16 *summary_word = profile_save_summary_word(offset);
    uint8 *participant = profile_save_participant_byte(offset);
    uint16 *track = profile_save_track(offset);
    uint8 *result_byte = profile_save_result_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = profile_save_backup(offset);
    uint8 *header = profile_save_header(offset);
    uint8 *grid = profile_save_grid(offset);
    uint8 *availability = profile_save_availability(offset);
    uint32 field;
    PROFILE_SERIES *series = profile_save_series(offset, &field);
    uint16 *target;
    uint8 *option = profile_save_game_option(offset);
    uint8 *choice = profile_save_choice_byte(offset);
    uint32 shift;

    if (opaque_byte != NULL)
    {
        *opaque_byte = value;
        return;
    }
    if (tail_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *tail_word = (uint16)((*tail_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (champ_byte != NULL)
    {
        *champ_byte = value;
        return;
    }
    if (time_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *time_word = (uint16)((*time_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (grid_byte != NULL)
    {
        *grid_byte = value;
        return;
    }
    if (grid_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *grid_word = (uint16)((*grid_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (match_byte != NULL)
    {
        *match_byte = value;
        return;
    }
    if (participant_byte != NULL)
    {
        *participant_byte = value;
        return;
    }
    if (participant_word != NULL)
    {
        uint32 shift = 8u * (offset & 3u);
        *participant_word = (*participant_word & ~(0xFFu << shift)) | ((uint32)value << shift);
        return;
    }
    if (lap_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *lap_word = (uint16)((*lap_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (record_byte != NULL)
    {
        *record_byte = value;
        return;
    }
    if (record_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *record_word = (uint16)((*record_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (summary_byte != NULL)
    {
        *summary_byte = value;
        return;
    }
    if (summary_word != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *summary_word = (uint16)((*summary_word & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (track != NULL)
    {
        uint32 shift = 8u * (offset & 1u);
        *track = (uint16)((*track & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (participant != NULL)
    {
        *participant = value;
        return;
    }
    if (result_byte != NULL)
    {
        *result_byte = value;
        return;
    }
    target = profile_save_menu_word(offset);
    if (target != NULL)
    {
        shift = 8u * (offset & 1u);
        *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (option != NULL)
    {
        *option = value;
        return;
    }
    if (choice != NULL)
    {
        *choice = value;
        return;
    }
    target = profile_save_choice_word(offset);
    if (target != NULL)
    {
        shift = 8u * (offset & 1u);
        *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    target = profile_save_sound(offset);
    if (target != NULL)
    {
        shift = 8u * (offset & 1u);
        *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (offset < 8u)
    {
        switch (offset / 2u)
        {
            case 0u:
                target = &game_selection.selection;
                break;
            case 1u:
                target = &game_selection.mode;
                break;
            case 2u:
                target = &game_selection.rules;
                break;
            default:
                target = &game_selection.previous_mode;
                break;
        }
        shift = 8u * (offset & 1u);
        *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
        return;
    }
    if (offset >= 8u && offset < 16u)
    {
        switch (offset)
        {
            case 8u:
                game_selection.flags = (uint16)((game_selection.flags & 0xFF00u) | value);
                break;
            case 9u:
                game_selection.flags = (uint16)((game_selection.flags & 0xFFu) | ((uint16)value << 8));
                break;
            case 10u:
                game_selection.ready = value;
                break;
            case 11u:
                game_selection.event = value;
                break;
            case 12u:
                game_selection.event_arg = value;
                break;
            case 13u:
                game_selection.menu_variant = value;
                break;
            case 14u:
                game_selection.players = (uint16)((game_selection.players & 0xFF00u) | value);
                break;
            default:
                game_selection.players = (uint16)((game_selection.players & 0xFFu) | ((uint16)value << 8));
                break;
        }
        return;
    }
    if (offset == 55u)
    {
        menu_state.language = value;
        return;
    }
    if (offset >= 5664u && offset < 5668u)
    {
        uint32 shift = 8u * (offset - 5664u);

        name_reels.code = (name_reels.code & ~(0xFFu << shift)) | ((uint32)value << shift);
        return;
    }
    if (name != NULL)
    {
        *name = value;
        return;
    }
    if (progress != NULL)
    {
        *progress = value;
        return;
    }
    if (backup != NULL)
    {
        *backup = value;
        return;
    }
    if (header != NULL)
    {
        *header = value;
        return;
    }
    if (grid != NULL)
    {
        *grid = value;
        return;
    }
    if (availability != NULL)
    {
        *availability = value;
        return;
    }
    if (series == NULL)
    {
        return;
    }
    if (field >= 6u)
    {
        series->points[field - 6u] = value;
        return;
    }
    target = field < 2u ? &series->completed : field < 4u ? &series->grid_row : &series->phase;
    shift = 8u * (field & 1u);
    *target = (uint16)((*target & ~(0xFFu << shift)) | ((uint32)value << shift));
}

sint32 profile_fn_80052f30(void)
{
    FUNCTION_MARKER(0x80052F30u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return menu_fn_80052f58();
}

sint32 profile_fn_800531c0(void)
{
    FUNCTION_MARKER(0x800531C0u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return menu_fn_800531e8();
}

sint32 profile_fn_80053458(void)
{
    FUNCTION_MARKER(0x80053458u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    return profile_fn_8005347c();
}

sint32 profile_fn_8005347c(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x8005347Cu, "MAIN.EXE");
    if ((sint16)controller->type == 7)
    {
        UI_RECORD *records = sprite_records;
        sint32 delta = 127 - (sint32)controller->packet[6];
        records[3].x = (uint16)(252 - delta / 2);
        display_set_line_color(192, 192, 192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        input_calibrations[selected].stick_center = (uint16)((uint16)delta);
        result = 1;
        if ((controller->pressed & 0x800u) == 0u)
            return result;
        menu_state.sound = 1u;
        result = 46;
    }
    menu_state.phase = (uint16)result;
    return result;
}

sint32 profile_fn_80053588(void)
{
    FUNCTION_MARKER(0x80053588u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return profile_fn_800535b0();
}

sint32 profile_fn_800535b0(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x800535B0u, "MAIN.EXE");
    if ((sint16)controller->type == 7)
    {
        UI_RECORD *records;
        SPRITE_RENDER *sprite;
        char *text;
        uint32 raw;
        sint32 value;
        sint32 percent;
        sint32 scale;
        sint16 record_index;

        display_set_line_color(192, 192, 192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        raw = controller->packet[6];
        value = (127 - (sint32)raw) / 2;
        records = sprite_records;
        record_index = (sint16)records[5].value;
        records[3].x = (uint16)(252 - value);
        sprite = sprite_render_at(records[5].data, record_index);
        if (value < 0)
            value = -value;
        if (400 * (sint16)value / 252 < 9)
        {
            value = (sint16)input_calibrations[selected].stick_range >> 6;
            if (value < 0)
                value = -value;
            w_u16(0x800B4234u, 0u);
        }
        else if ((sint16)r_u16(0x800B4234u) >= (sint16)value)
            value = (sint16)r_u16(0x800B4234u);
        else
        {
            sint32 magnitude = 127 - (sint32)raw;
            w_u16(0x800B4234u, (uint16)value);
            if (magnitude < 0)
                magnitude = -magnitude;
            input_calibrations[selected].stick_range = (uint16)((uint16)(32 * magnitude));
        }
        percent = 400 * (sint16)value / 252;
        scale = 2 * (sint16)value + 1;
        if (percent >= 101)
            percent = 100;
        if (scale >= 129)
            scale = 128;
        sprite->width = (uint32)scale;
        sprite->u = (uint32)(1024 - scale);
        text = text_menu[3].text;
        text[0] = (uint8)(percent / 100 + 48);
        text[1u] = (uint8)(percent % 100 / 10 + 48);
        text[2u] = (uint8)(percent % 10 + 48);
        if ((controller->pressed & 0x800u) != 0u)
        {
            menu_state.phase = 2u;
            result = 1;
            menu_state.sound = 1u;
        }
    }
    else
        menu_state.phase = 2u;
    return result;
}

// Native formatting shared with MAIN.EXE 80053898
sint32 profile_format_parts(const uint16 parts[3], char *text)
{
    uint32 middle = parts[1];
    uint32 last = parts[2];
    uint32 first = parts[0];
    uint32 remainder;
    sint32 result = (sint32)last;

    text[0] = 48u;
    text[2u] = 48u;
    text[3u] = 48u;
    text[5u] = 48u;
    text[6u] = 48u;
    if (first != 0u)
        text[0] = (uint8)(first + 48u);
    if (middle != 0u)
    {
        remainder = middle % 100u;
        text[2u] = (uint8)(remainder / 10u + 48u);
        text[3u] = (uint8)(remainder % 10u + 48u);
    }
    if (last != 0u)
    {
        uint32 tens = (last / 10u) % 100u / 10u;
        text[5u] = (uint8)(tens + 48u);
        result = (sint32)(10u * tens);
        text[6u] = (uint8)((last / 10u) % 100u % 10u + 48u);
    }
    return result;
}

sint32 profile_format_value_parts3(uint32 values, char *text)
{
    uint16 parts[3];

    parts[1] = r_u16(values + 2u);
    parts[2] = r_u16(values + 4u);
    parts[0] = r_u16(values);
    FUNCTION_MARKER(0x80053898u, "MAIN.EXE");
    return profile_format_parts(parts, text);
}

sint32 time_equal(const uint16 first[3], const uint16 second[3])
{
    FUNCTION_MARKER(0x800539BCu, "MAIN.EXE");
    if (first[2] != second[2])
        return 0;
    if (first[1] != second[1])
        return 0;
    return first[0] == second[0];
}

// Native arithmetic from MAIN.EXE 80053A00
uint32 profile_time_ticks(const uint16 parts[3])
{
    return 60000u * parts[0] + 100u * parts[1] + parts[2] / 10u;
}

// Native display arithmetic from MAIN.EXE 80059C70
uint32 profile_time_tenths(const uint16 parts[3])
{
    return 6000u * parts[0] + 100u * parts[1] + parts[2] / 10u;
}

uint32 time_fields_convert(uint32 value)
{
    uint32 first = r_u16(value);
    uint32 second = r_u16(value + 2u);
    uint32 third = r_u16(value + 4u);

    FUNCTION_MARKER(0x80053A00u, "MAIN.EXE");
    return 60000u * first + 100u * second + third / 10u;
}

void profile_reset_grid_flags(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 row;
    sint32 index;

    FUNCTION_MARKER(0x80053E14u, "MAIN.EXE");
    if ((game_selection.flags & 2u) != 0u)
    {
        uint8 flags = profile_data->reward_flags;

        profile_data->max_level = 3u;
        profile_data->reward_flags = (uint8)(flags | 0x80u);
        for (row = 0; row < 3; ++row)
        {
            for (index = 5; index >= 0; --index)
                profile_data->progress.courses[(uint32)row][(uint32)index].state = (uint8)(6u);
            for (index = 5; index >= 0; --index)
                profile_data->progress.courses[(uint32)row][(uint32)index].result = (uint8)(1u);
        }
        for (index = 9; index >= 0; --index)
            profile_current()->availability[index] = 1u;
    }
}

void profile_update_trans(void)
{
    PLAYER_PROFILE *profile = profile_current();
    sint32 result;

    FUNCTION_MARKER(0x80053ECCu, "MAIN.EXE");
    menu_save_desc_payloads((sint16)menu_state.screen);
    if ((sint16)menu_state.screen == 3)
    {
        PROFILE_SERIES *series;

        profile_init_mode_rec();
        profile_reset_grid_flags();
        profile->reset_pending = 0u;
        series = profile_get_series();
        if ((sint16)menu_state.phase == 5)
        {
            result = (sint16)game_selection.mode;
            if (result != 0 && result != 2)
            {
                result = (sint16)series->phase;
                if (result == 2)
                {
                    menu_state.phase = 18u;
                }
            }
        }
    }
    else if ((sint16)menu_state.screen == 4)
    {
        profile_select_mode();
        profile_reset_grid_flags();
        profile->reset_pending = 0u;
    }
}

sint32 profile_update_mode_select_vis(void)
{
    sint16 track = (sint16)menu_state.screen;
    MENU_DESC *table = menu_configurations[track].descs;
    sint32 index;
    sint32 visible;
    sint16 selection;
    uint32 primitive;
    UI_RECORD *records;

    FUNCTION_MARKER(0x80053FA4u, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        MENU_DESC *entry = table + (sint32)index;
        sint16 record_index = (sint16)entry->record;
        const MENU_VISIBILITY *list;
        sint32 child;
        visible = index < 5;
        records = sprite_records;
        records[(sint32)record_index].type = (uint8)visible;
        list = entry->visibility;
        for (child = 0; child < (sint16)list->count; ++child)
        {
            record_index = (sint16)list->records[child];
            records = sprite_records;
            records[(sint32)record_index].type = (uint8)visible;
        }
    }
    profile_build_vehicle_grid(8, (sint16)menu_boat_select.boat, 344, 130);
    visible = (uint8)profile_is_unlocked((sint16)((sint16)menu_boat_select.boat)) != 0u;
    text_clear_boat_names();
    selection = (sint16)menu_boat_select.boat;
    primitive = 496u + 160u * (uint32)(sint32)selection;
    text_hud[31 + 10 * selection].visible = 1;
    text_hud[31 + 10 * selection].font = 1;
    records = sprite_records;
    text_hud[31 + 10 * selection].x = 120;
    text_hud[31 + 10 * selection].y = 160;
    records[40].type = (uint8)visible;
    text_menu[4].visible = (uint8)visible;
    return 1;
}

void profile_update_carousel(sint16 direction, CONTROLLER_STATE *input)
{
    sint32 index;

    FUNCTION_MARKER(0x80054148u, "MAIN.EXE");
    if (direction == 0)
    {
        sint32 result;
        if ((input->current & 0x10u) != 0u)
        {
            uint8 gate = menu_state.input_enabled;
            if (gate != 0u)
            {
                sint16 state = (sint16)menu_boat_select.phase;
                if (state == 0)
                {
                    uint8 pages = (uint8)game_selection.players;
                    menu_state.sound = 2u;
                    if (pages != 0u)
                    {
                        sint16 page = (sint16)menu_boat_select.player;
                        if (page != 0)
                        {
                            page = (sint16)(page - 1);
                            menu_state.phase = 5u;
                            menu_boat_select.player = (uint16)page;
                            menu_state.refresh = 1u;
                            sprite_clear_anim_recs();
                            profile_update_trans();
                            page = (sint16)menu_boat_select.player;
                            menu_boat_select.boat = race_selection.boats[(sint32)page];
                        }
                        else
                        {
                            uint8 mode = game_selection.menu_variant;
                            menu_state.phase = 25u;
                            if (mode == 0u && (sint16)game_selection.mode == 0)
                                profile_backup_selection();
                        }
                    }
                    else
                    {
                        uint8 mode = game_selection.menu_variant;
                        menu_state.phase = 25u;
                        if (mode == 0u && (sint16)game_selection.mode == 0)
                            profile_backup_selection();
                    }
                }
            }
        }
        result = input->current & 0x40u;
        if (result == 0)
            return;
        result = menu_state.input_enabled;
        if (result == 0)
            return;
        result = (sint16)menu_boat_select.phase;
        if (result != 0)
            return;
        {
            sint16 selection = (sint16)menu_boat_select.boat;
            menu_state.sound = 1u;
            result = (uint8)profile_is_unlocked((sint16)(selection));
            if (result == 0)
            {
                menu_state.sound = 5u;
                return;
            }
            else
            {
                sint16 page = (sint16)menu_boat_select.player;
                uint8 selected = (uint8)menu_boat_select.boat;
                sint16 page_snapshot;
                uint8 pages;

                race_selection.boats[(sint32)page] = selected;
                page_snapshot = (sint16)menu_boat_select.player;
                pages = (uint8)game_selection.players;
                if (page_snapshot + 1 == pages)
                {
                    sint16 mode = (sint16)game_selection.mode;
                    if (mode == 0 || mode == 2 || page_snapshot != 0)
                        menu_state.phase = 10u;
                    else
                        menu_state.phase = 18u;
                }
                else
                {
                    TEXT_RECORD *menu;
                    char *output;

                    page_snapshot = (sint16)(page_snapshot + 1);
                    menu = text_menu;
                    menu_state.refresh = 1u;
                    output = menu[5].text;
                    menu_boat_select.player = (uint16)page_snapshot;
                    menu_state.phase = 5u;
                    name_copy_player_name_bytes((uint16)page_snapshot, output);
                    page_snapshot = (sint16)menu_boat_select.player;
                    menu_boat_select.boat = race_selection.boats[(sint32)page_snapshot];
                }
                sprite_clear_anim_recs();
                {
                    profile_update_trans();
                    return;
                }
            }
        }
    }
    if (direction == 1 || direction == 2)
    {
        sint32 table_index = direction == 1 ? 7 : 1;
        for (index = 0; index < 8; ++index)
        {
            UI_RECORD *record;
            sint16 record_index;
            sint16 target_x;
            sint16 target_y;
            sint16 target_first;
            sint16 target_second;
            UI_RECORD *records;
            if (table_index == 8)
                table_index = 0;
            record_index = (sint16)vehicle_carousel_order[index];
            target_x = (sint16)vehicle_carousel[table_index].x;
            target_y = (sint16)vehicle_carousel[table_index].y;
            target_first = (sint16)vehicle_carousel[table_index].first;
            target_second = (sint16)vehicle_carousel[table_index].second;
            records = sprite_records;
            record = (records + (sint32)record_index);
            sprite_init_trans(record, target_x, target_y, target_first, target_second, 10);
            ++table_index;
        }
        if (direction == 1)
        {
            sint16 selection = (sint16)(menu_boat_select.boat + 1u);
            menu_boat_select.boat = (uint16)selection;
            if (selection == 8)
                menu_boat_select.boat = 0u;
            menu_boat_select.rotation = 1u;
        }
        else
        {
            sint16 selection = (sint16)(uint16)(menu_boat_select.boat - 1u);
            menu_boat_select.boat = (uint16)selection;
            if (selection < 0)
                menu_boat_select.boat = 7u;
            menu_boat_select.rotation = 2u;
        }
        menu_boat_select.phase = MENU_BOAT_MOVING;
        return;
    }
    if (direction == 3)
    {
        UI_RECORD saved_record;
        UI_RECORD *saved = &saved_record;
        sint16 first_index = (sint16)vehicle_carousel_order[0];
        UI_RECORD *records = sprite_records;
        UI_RECORD *first = (records + (sint32)first_index);
        uint8 state = first->transition_ticks;
        if (state == 5u)
        {
            sint16 rotation = (sint16)menu_boat_select.rotation;
            if (rotation == 1)
            {
                sprite_copy_anim_rec_bytes(first, saved);
                for (index = 0; index < 7; ++index)
                {
                    sint16 source_index = (sint16)vehicle_carousel_order[index + 1];
                    sint16 destination_index;
                    UI_RECORD *source;
                    UI_RECORD *destination;

                    records = sprite_records;
                    destination_index = (sint16)vehicle_carousel_order[index];
                    source = (records + (sint32)source_index);
                    destination = (records + (sint32)destination_index);
                    sprite_copy_anim_rec_bytes(source, destination);
                }
                first_index = (sint16)vehicle_carousel_order[7];
                records = sprite_records;
                sprite_copy_anim_rec_bytes(saved, (records + (sint32)first_index));
                first_index = (sint16)vehicle_carousel_order[0];
                records = sprite_records;
                sprite_apply_prim_vram_mask((records + (sint32)first_index));
                first_index = (sint16)vehicle_carousel_order[3];
                records = sprite_records;
                sprite_disable_semitransparency((records + (sint32)first_index));
            }
            rotation = (sint16)menu_boat_select.rotation;
            if (rotation == 2)
            {
                UI_RECORD *last;

                first_index = (sint16)vehicle_carousel_order[7];
                records = sprite_records;
                last = (records + (sint32)first_index);
                sprite_copy_anim_rec_bytes(last, saved);
                for (index = 7; index > 0; --index)
                {
                    sint16 source_index = (sint16)vehicle_carousel_order[index - 1];
                    sint16 destination_index;
                    UI_RECORD *source;
                    UI_RECORD *destination;

                    records = sprite_records;
                    destination_index = (sint16)vehicle_carousel_order[index];
                    source = (records + (sint32)source_index);
                    destination = (records + (sint32)destination_index);
                    sprite_copy_anim_rec_bytes(source, destination);
                }
                first_index = (sint16)vehicle_carousel_order[0];
                records = sprite_records;
                sprite_copy_anim_rec_bytes(saved, (records + (sint32)first_index));
                first_index = (sint16)vehicle_carousel_order[4];
                records = sprite_records;
                sprite_apply_prim_vram_mask((records + (sint32)first_index));
                first_index = (sint16)vehicle_carousel_order[1];
                records = sprite_records;
                sprite_disable_semitransparency((records + (sint32)first_index));
            }
            {
                uint16 selection_bits = menu_boat_select.boat;
                sint32 selection = (sint16)(uint16)(selection_bits - 2u);
                UI_RECORD *records_snapshot;
                if (selection < 0)
                    selection = (sint16)(uint16)(selection_bits + 6u);
                records_snapshot = sprite_records;
                for (index = 0; index < 8; ++index)
                {
                    sint16 record_index = (sint16)vehicle_carousel_order[index];
                    uint16 selected_record = vehicle_carousel_order[selection];
                    UI_RECORD *record = (records_snapshot + (sint32)record_index);
                    record->value = selected_record;
                    selection = (sint16)(uint16)(selection + 1);
                    if (selection == 8)
                        selection = 0;
                }
            }
            profile_update_mode_select_vis();
        }
        first_index = (sint16)vehicle_carousel_order[0];
        records = sprite_records;
        first = (records + (sint32)first_index);
        state = first->transition_ticks;
        if (state == 0u)
        {
            sint16 record_index = (sint16)vehicle_carousel_order[2];
            sint32 result;
            menu_boat_select.phase = MENU_BOAT_SETTLING;
            result = menu_move_sprite_rec((records + (sint32)record_index), 1, 8, 6, 1, 1, 0, 0, 0, 0);
            return;
        }
        return;
    }
    if (direction == 4)
    {
        sint16 record_index = (sint16)vehicle_carousel_order[2];
        UI_RECORD *records = sprite_records;
        UI_RECORD *record = (records + (sint32)record_index);
        uint8 state = record->command;
        if (state == 0u)
        {
            menu_boat_select.phase = MENU_BOAT_IDLE;
            record->command = 2u;
        }
        return;
    }
}

sint32 vehicle_update_carousel(void)
{
    sint16 track = (sint16)menu_state.screen;
    UI_RECORD *records = sprite_records;
    MENU_DESC *table = menu_configurations[track].descs;
    PLAYER_PROFILE *profile;
    sint32 selection;
    sint32 index;
    sint32 result = 1;

    FUNCTION_MARKER(0x800549ECu, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        sint16 record_index = (sint16)vehicle_carousel_order[index];
        UI_RECORD *record = (records + (sint32)record_index);
        uint16 value = record->x;
        const MENU_VISIBILITY *list;

        vehicle_carousel[index].x = value;
        value = record->y;
        vehicle_carousel[index].y = value;
        list = table[(sint32)index].visibility;
        value = list->x;
        vehicle_carousel[index].first = value;
        value = list->y;
        vehicle_carousel[index].second = value;
    }
    selection = (sint16)menu_boat_select.boat - 2;
    if (selection < 0)
        selection += 8;
    profile = profile_current();
    for (index = 0; index < 8; ++index)
    {
        UI_RECORD *record;
        SPRITE_RENDER *primitive;
        sint32 enabled;
        sint16 record_index;
        uint16 selected_record;
        if (selection == 8)
            selection = 0;
        record_index = (sint16)vehicle_carousel_order[index];
        selected_record = vehicle_carousel_order[selection];
        records = sprite_records;
        record = (records + (sint32)record_index);
        record->value = selected_record;
        record->type = 1u;
        primitive = sprite_render_at(record->data, (sint16)record->value);
        enabled = profile->availability[selection] != 0u;
        primitive->quad.b0 = enabled != 0 ? 128u : 32u;
        primitive->quad.g0 = enabled != 0 ? 128u : 32u;
        primitive->quad.r0 = enabled != 0 ? 128u : 32u;
        vehicle_select_enable_prim(&primitive->quad, enabled);
        ++selection;
    }
    profile_update_mode_select_vis();
    if ((uint8)game_selection.players == 1u)
    {
        for (index = 4; index > 0; --index)
            text_menu[5].visible = 0u;
    }
    else
    {
        TEXT_RECORD *menu = text_menu;
        uint16 active = menu_boat_select.player;
        char *output = menu[5].text;
        name_copy_player_name_bytes(active, output);
    }
    if ((sint16)game_selection.mode == 1 || (uint8)game_selection.players >= 2u)
    {
        TEXT_RECORD *menu = text_menu;
        UI_RECORD *records_now;
        uint16 value;

        menu[3].visible = 0u;
        menu = text_menu;
        records_now = sprite_records;
        value = menu[3].x;
        menu[4].x = value;
        records_now[41].type = 0u;
        records_now = sprite_records;
        result = records_now[41].x;
        records_now[40].x = (uint16)result;
    }
    return result;
}

sint32 vehicle_select_assign_palettes(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    MENU_DESC *table = menu_configurations[10].descs;
    PLAYER_PROFILE *profile_data = profile_current();
    UI_RECORD *records = sprite_records;
    sint32 group = 0;
    sint32 index;

    FUNCTION_MARKER(0x80054CB4u, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 screen = (sint16)menu_course_select.level;
        sint32 type = profile_data->progress.courses[(uint32)(sint32)screen][(uint32)index].state;
        sint16 selection;
        sint16 palette = 0;
        SPRITE_RENDER *output;
        MENU_DESC *entry;
        uint16 clut;

        if ((sint16)game_selection.mode == 1)
        {
            selection = (sint16)profile_course_slots[index].record;
            sint32 split = (sint16)menu_course_select.course;
            if (index < split)
                type = 6;
            else
                type = index == split ? 4 : 5;
        }
        else
        {
            selection = (sint16)profile_course_slots[index].record;
        }
        screen = (sint16)menu_course_select.level;
        if (screen == 1)
        {
            group = 23;
            if (type == 5)
                palette = selection != 0 ? 504 : 506;
            else if (type == 6)
                palette = selection != 0 ? 235 : 234;
            else
                palette = selection != 0 ? 507 : 505;
        }
        else if (screen == 0 || screen == 2)
        {
            if (type == 5)
                palette = selection != 0 ? 508 : 510;
            else if (type == 6)
                palette = selection != 0 ? 229 : 228;
            else
                palette = selection != 0 ? 511 : 509;
        }
        else if (screen == 3)
        {
            palette = (sint16)(index + 229);
            group = (uint16)(index - 1) < 4u ? 36 : 0;
        }
        output = sprite_render_at(records->data, group + index);
        clut = getClut(768, palette);
        screen = (sint16)menu_course_select.level;
        output->quad.clut = clut;
        output->orientation = (uint8)(screen == 2);
        entry = table + (sint32)selection;
        screen = (sint16)menu_course_select.level;
        entry->record = (uint16)((uint16)(group + index));
        if (screen == 3)
            entry->visible = (uint16)(group != 0);
    }
    return index << 16;
}

sint32 vehicle_advance_carousel(sint16 action, CONTROLLER_STATE *input)
{
    uint32 menu_words[9];
    uint16 menu_last;
    sint32 apply_offset = 0;
    sint16 base_offset;
    sint32 index;

    FUNCTION_MARKER(0x80054FC0u, "MAIN.EXE");
    for (index = 0; index < 9; ++index)
        menu_words[index] = r_u32(0x80082388u + 4u * (uint32)index);
    menu_last = r_u16(0x800823ACu);
    if ((sint16)menu_course_select.blocked != 0)
    {
        sint16 record_index = (sint16)menu_course_select.record;
        UI_RECORD *record = (sprite_records + (sint32)record_index);
        if (record->command == 0u)
            menu_move_sprite_rec(record, 1, 8, 6, 1, 1, (sint32)menu_words[0], (sint32)menu_words[1], (sint16)menu_words[2], (sint16)menu_words[3]);
    }
    if (action == 0)
    {
        sint32 result = input->current & 0x40u;
        sint32 enabled = 0;

        if (result != 0)
        {
            result = menu_state.input_enabled;
            if (result != 0)
            {
                result = (sint16)r_u16(0x800B6AA0u) == 0;
                enabled = result;
            }
        }
        if (enabled == 0)
            return result;
        {
            uint8 row = (uint8)menu_course_select.level;
            uint8 column = (uint8)menu_course_select.course;

            if ((uint8)profile_grid_cell_is_available(row, column, 0u, 0u) != 0u)
            {
                if ((sint16)game_selection.mode == 6)
                {
                    sint16 mode = (sint16)game_selection.rules;

                    menu_state.sound = 1u;
                    if (mode == 1)
                    {
                        menu_state.phase = 30u;
                        return tournament_reset_grid();
                    }
                    if (mode < 2)
                    {
                        if (mode == 0)
                            menu_state.phase = 38u;
                        return 38;
                    }
                    if (mode == 2)
                    {
                        menu_state.phase = 31u;
                        return tournament_init_round();
                    }
                    return 31;
                }
                voice_start_scaled((sint16)r_u16(0x800834B4u), 1);
                sound_fn_8007741c(0, 128u);
                return profile_commit_selection();
            }
        }
        menu_state.sound = 5u;
        return 5;
    }
    base_offset = (sint16)(uint16)(sprite_records->transition_step_x >> 16);
    switch ((sint16)(action - 1))
    {
        case 0:
        case 1:
        {
            uint16 old_column;
            sint16 column;
            sint16 record_index;
            UI_RECORD *record;
            sint16 target_x;
            sint16 target_y;

            if (action == 1)
            {
                profile_sort_recs_desc(0u, 0u);
                old_column = menu_course_select.course;
                menu_course_select.course = (uint16)(old_column + 1u);
                column = (sint16)(uint16)(old_column + 1u);
                w_u16(0x800B6BECu, old_column);
                if (column >= 6)
                {
                    column = 5;
                    menu_course_select.course = 5u;
                }
            }
            else
            {
                profile_sort_recs_asc(0u, 0u, 0u);
                old_column = menu_course_select.course;
                menu_course_select.course = (uint16)(old_column - 1u);
                w_u16(0x800B6BECu, old_column);
                column = (sint16)(uint16)(old_column - 1u);
                if (column < 0)
                {
                    column = 0;
                    menu_course_select.course = 0u;
                }
            }
            column = (sint16)menu_course_select.course;
            record_index = (sint16)profile_course_slots[column].record;
            record = (sprite_records + (sint32)record_index);
            record->x = profile_course_slots[column].x;
            record->y = profile_course_slots[column].y;
            target_y = (sint16)profile_course_slots[column].y;
            target_x = (sint16)((sint16)profile_course_slots[(sint16)old_column].x + (action == 1 ? 134 : -134));
            sprite_init_trans(record, target_x, target_y, 0, 0, 6);
            menu_course_select.direction = (uint16)action;
            menu_course_select.phase = MENU_COURSE_MOVING;
            break;
        }
        case 2:
        {
            sint16 column = (sint16)menu_course_select.course;
            sint16 record_index = (sint16)profile_course_slots[column].record;
            UI_RECORD *record = (sprite_records + (sint32)record_index);
            if (record->transition_ticks == 0u)
            {
                sint16 x = (sint16)record->x;
                sint16 y = (sint16)record->y;
                UI_RECORD *records;
                uint8 visible;
                sint16 target_x;
                sint16 target_y;
                uint8 row;
                uint8 selected_column;

                if ((sint16)menu_course_select.direction == 1)
                    profile_sort_recs_desc(0u, 0u);
                else
                    profile_sort_recs_asc(0u, 0u, 0u);
                column = (sint16)menu_course_select.course;
                records = sprite_records;
                records->x = (uint16)x;
                records->y = (uint16)y;
                target_x = (sint16)profile_course_slots[column].x;
                target_y = (sint16)profile_course_slots[column].y;
                sprite_init_trans(records, target_x, target_y, 0, 0, 6);
                row = (uint8)menu_course_select.level;
                selected_column = (uint8)menu_course_select.course;
                visible = (uint8)profile_grid_cell_is_available(row, selected_column, 0u, 0u);
                sprite_records[8].type = visible;
                text_menu[3].visible = visible;
                profile_update_select_prims(0u, 0u, 0u, 0u);
                vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
                w_u16(0x800B411Au, 2u);
                menu_course_select.phase = MENU_COURSE_SETTLING;
            }
            break;
        }
        case 3:
            if (sprite_records->transition_ticks == 0u)
                menu_course_select.phase = MENU_COURSE_IDLE;
            break;
        case 4:
            if (sprite_records->transition_ticks != 0u)
            {
                apply_offset = 1;
                break;
            }
            profile_init_menu(1, 0u, 0u, 0u);
            menu_start_dir_trans(1);
            {
                TEXT_RECORD *menu = text_menu;
                UI_RECORD *records;
                uint16 value;
                uint16 last_value;

                menu_course_select.phase = MENU_COURSE_NEXT_ENTER;
                for (index = 0; index < 19; ++index)
                {
                    sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                    TEXT_RECORD *address = &menu[menu_index];
                    address[0].x = (uint16)(address[0].x + 1029u);
                }
                records = sprite_records;
                value = records[18].x;
                last_value = records[20].x;
                records[18].x = (uint16)(value + 1029u);
                value = records[19].x;
                records[20].x = (uint16)(last_value + 1029u);
                records[19].x = (uint16)(value + 1029u);
            }
            break;
        case 5:
        case 7:
            if (sprite_records->transition_ticks == 0u)
                menu_course_select.phase = MENU_COURSE_IDLE;
            else
                apply_offset = 1;
            break;
        case 6:
            if (sprite_records->transition_ticks != 0u)
            {
                apply_offset = 1;
                break;
            }
            profile_init_menu(3, 0u, 0u, 0u);
            menu_start_dir_trans(3);
            {
                TEXT_RECORD *menu = text_menu;
                UI_RECORD *records;
                uint16 value;
                uint16 last_value;

                menu_course_select.phase = MENU_COURSE_PREV_ENTER;
                for (index = 0; index < 19; ++index)
                {
                    sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                    TEXT_RECORD *address = &menu[menu_index];
                    address[0].x = (uint16)(address[0].x - 980u);
                }
                records = sprite_records;
                value = records[18].x;
                last_value = records[20].x;
                records[18].x = (uint16)(value - 980u);
                value = records[19].x;
                records[20].x = (uint16)(last_value - 980u);
                records[19].x = (uint16)(value - 980u);
            }
            break;
        default:
            break;
    }
    if (apply_offset != 0)
    {
        {
            TEXT_RECORD *menu = text_menu;
            UI_RECORD *records;
            uint16 value;
            uint16 last_value;

            for (index = 0; index < 19; ++index)
            {
                sint16 menu_index = index == 18 ? (sint16)menu_last : (sint16)(uint16)(menu_words[index / 2] >> (16 * (index & 1)));
                TEXT_RECORD *address = &menu[menu_index];
                address[0].x = (uint16)(address[0].x + (uint32)(sint32)base_offset);
            }
            records = sprite_records;
            value = records[18].x;
            last_value = records[20].x;
            records[18].x = (uint16)(value + (uint32)(sint32)base_offset);
            value = records[19].x;
            records[20].x = (uint16)(last_value + (uint32)(sint32)base_offset);
            records[19].x = (uint16)(value + (uint32)(sint32)base_offset);
        }
    }
    return vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
}

sint32 profile_populate_select_recs(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 index;

    FUNCTION_MARKER(0x800557ACu, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 offset = (sint16)(2 * (sint16)profile_course_slots[index].rank);
        sint16 source_index = (sint16)profile_course_slots[index].record;
        UI_RECORD *source = (sprite_records + (sint32)source_index);
        UI_RECORD *output;
        uint16 coordinate;
        sint16 screen;
        uint8 visible = 0u;

        source->layer = (uint8)(offset + 1);
        output = (sprite_records + index + 49);
        coordinate = source->x;
        output->x = (uint16)(coordinate + 100u);
        coordinate = source->y;
        output->layer = (uint8)offset;
        screen = (sint16)menu_course_select.level;
        output->y = (uint16)(coordinate + 40u);
        if (profile_data->progress.courses[(uint32)(sint32)screen][(uint32)index].state == 6u && screen != 3)
        {
            if ((sint16)game_selection.mode != 1 || index < (sint16)menu_course_select.course)
                visible = 1u;
        }
        output->type = visible;
    }
    return index << 16;
}

sint32 profile_sort_recs_desc(uint32 unused1, uint32 unused2)
{
    sint16 split = (sint16)menu_course_select.course;
    sint16 track = (sint16)menu_state.screen;
    sint32 output_index = 0;
    sint32 secondary = 1;
    MENU_DESC *table = menu_configurations[track].descs;

    FUNCTION_MARKER(0x800558ECu, "MAIN.EXE");
    if (split >= 0)
    {
        UI_RECORD *records = sprite_records;
        sint16 value = split;

        do
        {
            PROFILE_COURSE_SLOT *slot = &profile_course_slots[(sint16)value];
            UI_RECORD *record = (records + (sint16)output_index);
            uint16 x = slot->x;
            uint16 y;
            MENU_DESC *entry;

            record->x = x;
            y = slot->y;
            entry = table + (sint16)output_index;
            record->value = (uint16)value;
            record->y = y;
            entry->record = (uint16)((uint16)value);
            value = (sint16)(value - 1);
            slot->record = (uint16)output_index;
            slot->rank = (uint16)output_index;
            output_index = (sint16)(output_index + 1);
        } while (value >= 0);
    }
    split = (sint16)menu_course_select.course;
    if (split < 5)
    {
        sint16 value = (sint16)(split + 1);

        if (value < 6)
        {
            UI_RECORD *records = sprite_records;

            do
            {
                PROFILE_COURSE_SLOT *slot = &profile_course_slots[(sint16)value];
                UI_RECORD *record = (records + (sint16)output_index);
                uint16 x = slot->x;
                uint16 y;
                MENU_DESC *entry;

                record->x = x;
                y = slot->y;
                entry = table + (sint16)output_index;
                record->value = (uint16)value;
                record->y = y;
                entry->record = (uint16)((uint16)value);
                value = (sint16)(value + 1);
                slot->record = (uint16)output_index;
                output_index = (sint16)(output_index + 1);
                slot->rank = (uint16)secondary;
                secondary = (sint16)(secondary + 1);
            } while (value < 6);
        }
    }
    return profile_populate_select_recs(0u, 0u, 0u, 0u);
}

sint32 profile_sort_recs_asc(uint32 unused1, uint32 unused2, uint32 unused3)
{
    sint32 split = (sint16)menu_course_select.course;
    sint16 track = (sint16)menu_state.screen;
    MENU_DESC *table = menu_configurations[track].descs;
    sint32 value = split;
    sint32 output_index = 0;
    sint32 secondary = 1;

    FUNCTION_MARKER(0x80055AA8u, "MAIN.EXE");
    while (value < 6)
    {
        PROFILE_COURSE_SLOT *slot = &profile_course_slots[(sint16)value];
        UI_RECORD *record = (sprite_records + (sint16)output_index);
        MENU_DESC *entry = table + (sint32)output_index;
        uint16 first = slot->x;
        uint16 second;

        record->x = first;
        second = slot->y;
        record->value = (uint16)value;
        record->layer = 0u;
        record->y = second;
        entry->record = (uint16)((uint16)value);
        ++value;
        slot->record = (uint16)output_index;
        slot->rank = (uint16)output_index;
        ++output_index;
    }
    split = (sint16)menu_course_select.course;
    if (split > 0)
    {
        value = (sint16)(split - 1);
        while ((sint16)value >= 0)
        {
            PROFILE_COURSE_SLOT *slot = &profile_course_slots[(sint16)value];
            UI_RECORD *record = (sprite_records + (sint16)output_index);
            MENU_DESC *entry = table + (sint32)output_index;
            uint16 first = slot->x;
            uint16 second;

            record->x = first;
            second = slot->y;
            record->value = (uint16)value;
            record->layer = 0u;
            record->y = second;
            entry->record = (uint16)((uint16)value);
            --value;
            slot->record = (uint16)output_index;
            ++output_index;
            slot->rank = (uint16)secondary;
            ++secondary;
        }
    }
    return profile_populate_select_recs(0u, 0u, 0u, 0u);
}

sint32 profile_fn_80055cfc(sint16 direction, sint16 index, UI_RECORD *state)
{
    sint32 current = state->x;
    sint32 target = 0;

    FUNCTION_MARKER(0x80055CFCu, "MAIN.EXE");
    if (direction == 0)
        target = current - 512;
    else if (direction == 1)
    {
        if (index < 6)
        {
            target = current;
            state->x = (uint16)(current + 512);
        }
        else
        {
            target = current + 512;
            state->x = (uint16)(current + 1024);
        }
    }
    else if (direction == 2)
        target = current + 512;
    else if (direction == 3)
    {
        if (index < 6)
        {
            target = current;
            state->x = (uint16)(current - 512);
        }
        else
        {
            target = current - 512;
            state->x = (uint16)(current - 1024);
        }
    }
    return sprite_init_trans(state, (sint16)target, (sint16)state->y, 0, 0, 25);
}

sint32 menu_start_dir_trans(sint16 direction)
{
    UI_RECORD *records = sprite_records;
    sint32 index;

    FUNCTION_MARKER(0x80055DE4u, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
        profile_fn_80055cfc(direction, (sint16)index, (records + index));
    for (index = 4; index < 10; ++index)
        profile_fn_80055cfc(direction, 6, (records + index + 37));
    return 0;
}

sint32 profile_is_unlocked(sint16 index)
{
    FUNCTION_MARKER(0x80055EC8u, "MAIN.EXE");
    return profile_current()->availability[index] == 1u;
}

sint32 profile_level_is_available(sint32 requested, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 value = (uint8)requested;

    FUNCTION_MARKER(0x80055F0Cu, "MAIN.EXE");
    if ((sint16)r_u16(0x800B69CAu) != 0)
        return 0;
    if ((sint16)game_selection.mode == 2)
    {
        if (value == 3u)
            return 0;
    }
    else if (value == 3u && (uint8)game_selection.players != 1u)
    {
        if (name_player_name_matches(0, 0x800B4254u) != 0 && (uint8)game_selection.players == 2u)
            return (sint16)game_selection.mode != 6;
        return 0;
    }
    if (game_options.unlock_courses != 0u)
        return 1;
    return profile_data->max_level >= value;
}

sint32 profile_grid_cell_is_available(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x8005600Cu, "MAIN.EXE");
    if (game_options.unlock_courses != 0u)
        return 1;
    if (level == 3u)
        return (uint8)name_check_reels();
    if (profile_data->max_level < level)
        return 0;
    return profile_data->progress.courses[level][(uint8)column].state != 5u;
}

sint32 profile_get_grid_cell(sint32 requested, sint32 column, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 level = (uint8)requested;

    FUNCTION_MARKER(0x800560B8u, "MAIN.EXE");
    if ((uint32)((sint16)game_selection.mode - 1) < 2u || level == 3u)
        return 0;
    if (game_options.result_filter == 0u)
        return profile_data->progress.courses[level][(uint8)column].result;
    if (profile_data->progress.courses[level][(uint8)column].result == 2u)
        return profile_data->progress.courses[level][(uint8)column].result;
    return 1;
}

sint32 profile_update_select_prims(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    UI_RECORD *records;
    SPRITE_RENDER *row;
    uint32 availability;
    sint32 index;

    FUNCTION_MARKER(0x80056178u, "MAIN.EXE");
    for (index = 0; index < 6; ++index)
    {
        sint16 mapped = (sint16)profile_course_ui[index].label;
        TEXT_RECORD *desc = &text_menu[mapped];
        uint8 visible = 0u;

        if ((sint16)menu_course_select.course == index && (sint16)menu_course_select.level < 3)
            visible = 1u;
        desc[0].visible = visible;
    }
    records = sprite_records;
    {
        uint8 level = (uint8)menu_course_select.level;
        sint16 primitive_index = (sint16)records[12].value;
        uint8 column = (uint8)menu_course_select.course;

        row = sprite_render_at(records[12].data, primitive_index);
        availability = (uint8)profile_get_grid_cell(level, column, 0u, 1u);
    }
    if (availability == 1u)
    {
        row->quad.b0 = 0x80u;
        row->quad.g0 = 0x80u;
        row->quad.r0 = 0x80u;
        row->quad.code |= 1u;
        records[12].type = 1u;
    }
    else if (availability == 2u)
    {
        row->quad.r0 = 0x80u;
        row->quad.b0 = 0x40u;
        row->quad.g0 = 0x40u;
        row->quad.code &= 0xFEu;
        records[12].type = 1u;
    }
    else if (availability == 0u)
    {
        row->quad.g0 = 0x40u;
        row->quad.r0 = 0x40u;
        row->quad.b0 = 0x80u;
        row->quad.code &= 0xFEu;
        records[12].type = 0u;
    }
    for (index = 0; index < 10; ++index)
        text_menu[index + 11].visible = 0u;
    {
        sint16 level = (sint16)menu_course_select.level;
        if (level < 3)
        {
            sint16 column = (sint16)menu_course_select.course;
            sint16 mapped = (sint16)profile_course_text[level][column];
            TEXT_RECORD *desc = &text_menu[mapped];

            desc[11].visible = 1u;
            return (sint32)(desc + 189u);
        }
        return 2 * level;
    }
}

sint32 profile_init_menu(sint32 initialize, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint16 track = (sint16)menu_state.screen;
    MENU_CONFIGURATION *configuration = &menu_configurations[track];
    PLAYER_PROFILE *profile_data;
    sint32 initial_level;
    sint32 index;

    FUNCTION_MARKER(0x80056368u, "MAIN.EXE");
    profile_data = profile_current();
    menu_course_select.course = profile_data->course;
    initial_level = profile_data->level;
    menu_course_select.level = (uint16)initial_level;
    if (initial_level == 3)
    {
        if ((sint16)game_selection.mode == 2)
        {
            menu_course_select.level = 0u;
            menu_course_select.course = 0u;
            profile_data->level = (uint8)(0u);
            profile_data->course = (uint8)(0u);
        }
        if ((sint16)menu_course_select.level == initial_level && (sint16)menu_course_select.course == 0)
            menu_course_select.course = 1u;
    }
    if ((sint16)game_selection.mode == 6 && (sint16)game_selection.rules == 0)
        menu_course_select.level = game_options.level;
    for (index = 0; index < 6; ++index)
    {
        sint32 record_index = (sint16)profile_course_ui[index].record;
        sint16 ordering;
        UI_RECORD *records;

        sprite_records[record_index].type = 0u;
        ordering = (sint16)profile_course_ui[index].order;
        records = sprite_records;
        records->value = (uint16)ordering;
        if ((sint16)initialize == 0)
        {
            uint16 first = profile_course_ui[index].x;
            UI_RECORD *record = (records + (sint32)ordering);
            uint16 second;

            record->x = first;
            second = profile_course_ui[index].y;
            profile_course_slots[index].x = first;
            record->y = second;
            profile_course_slots[index].y = second;
        }
    }
    profile_sort_recs_asc(0u, 0u, 0u);
    {
        UI_RECORD *records = sprite_records;
        sint16 column = (sint16)menu_course_select.course;

        configuration->selection = (uint32)(sint32)column;
        menu_state.selection = (uint16)column;
        sprite_set_orient((records + 7), 1);
    }
    sprite_records[7].layer = 12u;
    sprite_records[6].layer = 12u;
    {
        uint8 visible = 0u;
        if ((sint16)r_u16(0x800B69CAu) == 0)
            visible = (uint8)((sint16)menu_course_select.level != 0);
        sprite_records[7].type = visible;
    }
    {
        uint8 level = (uint8)menu_course_select.level;
        uint8 visible = (uint8)profile_level_is_available((uint8)(level + 1u), 0u, 0u, 0u) != 0u;
        sprite_records[6].type = visible;
    }
    {
        uint8 level = (uint8)menu_course_select.level;
        uint8 column = (uint8)menu_course_select.course;
        uint8 visible = (uint8)profile_grid_cell_is_available(level, column, 0u, 0u);
        sprite_records[8].type = visible;
        text_menu[3].visible = visible;
    }
    w_u16(0x800B411Au, 2u);
    for (index = 0; index < 3; ++index)
    {
        sint16 level = (sint16)menu_course_select.level;
        sint32 record_index = (sint16)profile_level_records[index];
        sprite_records[record_index].type = (uint8)(level == index);
    }
    sprite_records[13].type = 0u;
    sprite_records[15].type = 0u;
    sprite_records[14].type = 0u;
    sprite_records[16].type = 0u;
    sprite_records[22].type = 0u;
    sprite_records[21].type = 0u;
    sprite_records[34].type = 0u;
    sprite_records[33].type = 0u;
    sprite_records[36].type = 0u;
    sprite_records[35].type = 0u;
    profile_update_select_prims(0u, 0u, 0u, 0u);
    vehicle_select_assign_palettes(0u, 0u, 0u, 0u);
    sprite_records[17].type = 0u;
    if ((sint16)menu_course_select.level == 3)
    {
        TEXT_RECORD *first_menu;
        uint8 selector;
        uint16 coordinate;

        sprite_records[12].type = 0u;
        first_menu = text_menu;
        menu_course_select.record = 17u;
        menu_course_select.blocked = 0u;
        first_menu[10].visible = 0u;
        text_menu[28].visible = 1u;
        text_menu[29].visible = 1u;
        text_menu[30].visible = 1u;
        text_menu[31].visible = 1u;
        sprite_records[47].type = 1u;
        sprite_records[48].type = 1u;
        text_menu[2].visible = 0u;
        sprite_records[10].type = 0u;
        selector = menu_state.language;
        {
            TEXT_RECORD *menu = text_menu;
            coordinate = r_u16(0x80098DB0u + 2u * selector);
            {
                UI_RECORD *records = sprite_records;
                menu[3].x = (uint16)(coordinate + 15u);
                records[8].x = coordinate;
            }
        }
        for (index = 0; index < 4; ++index)
            sprite_records[index + 37].type = 0u;
        for (index = 4; index < 10; ++index)
            sprite_records[index + 37].type = 1u;
        for (index = 0; index < 6; ++index)
            sprite_records[index + 49].type = 0u;
        name_reset_reels();
    }
    else
    {
        MENU_DESC *descs = configuration->descs;
        TEXT_RECORD *state_menu;

        for (index = 0; index < 6; ++index)
            descs[(sint32)index].visible = (uint16)(1u);
        text_menu[10].visible = 1u;
        state_menu = text_menu;
        menu_course_select.record = 12u;
        menu_course_select.blocked = 0u;
        state_menu[28].visible = 0u;
        text_menu[29].visible = 0u;
        text_menu[30].visible = 0u;
        text_menu[31].visible = 0u;
        sprite_records[47].type = 0u;
        sprite_records[48].type = 0u;
        text_menu[2].visible = 1u;
        sprite_records[10].type = 1u;
        text_menu[3].x = 227u;
        sprite_records[8].x = 212u;
        for (index = 0; index < 10; ++index)
            sprite_records[index + 37].type = 0u;
        for (index = 0; index < 6; ++index)
        {
            sint16 level = (sint16)menu_course_select.level;
            UI_RECORD *records = sprite_records;
            uint8 selected = profile_data->progress.courses[(uint32)(sint32)level][(uint32)index].state;

            records[index + 49].layer = 4u;
            sprite_records[index + 49].type = (uint8)(selected == 6u);
        }
    }
    for (index = 0; index < 6; ++index)
        text_menu[index + 22].visible = 0u;
    sprite_records[55].type = 0u;
    text_menu[32].visible = 0u;
    if ((sint16)r_u16(0x800B69CAu) != 0)
    {
        sint16 column;

        text_menu[0].visible = 0u;
        text_menu[21].visible = 1u;
        column = (sint16)menu_course_select.course;
        text_menu[column + 22].visible = 1u;
        if ((sint16)menu_course_select.course != 0 && (sint16)game_selection.mode != 6)
        {
            sprite_records[55].type = 1u;
            text_menu[32].visible = 1u;
        }
    }
    else
    {
        text_menu[0].visible = 1u;
        text_menu[21].visible = 0u;
    }
    return profile_populate_select_recs(1u, 0u, 0u, 0u);
}

void profile_select_course(void)
{
    PLAYER_PROFILE *profile_data = profile_current();

    FUNCTION_MARKER(0x80056C04u, "MAIN.EXE");
    profile_data->course = (uint8)((uint8)menu_course_select.course);
}

uint32 text_reset_rec_styles(void)
{
    FUNCTION_MARKER(0x80056C68u, "MAIN.EXE");
    text_set_hud_visible(1, 0);
    return text_set_hud_visible(2, 0);
}

sint32 profile_fn_80056c98(void)
{
    FUNCTION_MARKER(0x80056C98u, "MAIN.EXE");
    return 10;
}

sint32 profile_handle_select_input(CONTROLLER_STATE *input, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint32 result = menu_state.input_enabled;

    FUNCTION_MARKER(0x80056CA0u, "MAIN.EXE");
    if (result != 0)
    {
        result = (sint16)menu_course_select.phase;
        if (result == 0)
        {
            if ((input->current & 0x20u) != 0u && (sint16)menu_course_select.level != 3)
            {
                menu_state.sound = 1u;
                menu_state.phase = 21u;
            }
            result = 1;
            if ((input->current & 0x80u) != 0u && (sint16)game_selection.mode == 1)
            {
                result = 1;
                if ((sint16)menu_course_select.course != 0)
                {
                    menu_state.sound = 1u;
                    result = 20;
                    menu_state.phase = 20u;
                }
            }
        }
    }
    return result;
}

sint32 profile_update_selection_vis_time(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    PROFILE_RECORD *source;
    PROFILE_SUMMARY *summary;
    uint32 enabled;
    sint32 index;

    FUNCTION_MARKER(0x80056D6Cu, "MAIN.EXE");
    profile_data = profile_current();
    for (index = 0; index < 12; ++index)
        sprite_records[index + 2].type = 0u;
    for (index = 0; index < 6; ++index)
    {
        uint8 active = (uint8)(index == (sint16)menu_course_select.course);
        uint32 offset = 32u * (uint32)index;

        text_menu[index].visible = active;
        text_menu[index + 1].visible = active;
    }
    {
        sint32 level = (sint16)menu_course_select.level;

        if (level == 1)
        {
            sint32 column = (sint16)menu_course_select.course;
            sprite_records[column + 8].type = 1u;
        }
        else if (level == 2)
        {
            sint32 column = (sint16)menu_course_select.course;
            UI_RECORD *owner = (sprite_records + column + 2);

            owner->type = 1u;
            sprite_set_orient(owner, 1);
        }
        else if (level == 0)
        {
            sint32 column = (sint16)menu_course_select.course;
            sprite_records[column + 2].type = 1u;
        }
    }

    if ((sint16)game_selection.mode == 4)
    {
        sint32 level = (sint16)menu_course_select.level;
        selection = (sint16)(menu_course_select.course + 6u * (uint32)level);
    }
    else
    {
        uint32 profile_level = profile_data->level;
        selection = (sint16)(menu_course_select.course + 6u * profile_level);
    }
    sprite_records[16].type = 0u;
    sprite_records[17].type = 0u;
    sprite_records[18].type = 0u;
    sprite_records[19].type = 0u;
    if (r_s16(0x800B6AA0u) != 0 && (sint16)game_selection.mode != 4)
    {
        if (selection == 0)
        {
            sprite_records[16].type = 1u;
            sprite_records[19].type = 1u;
        }
        else if (selection == 1)
            sprite_records[18].type = 1u;
        else if (selection == 2 && (sint16)game_selection.selection == 0 && (uint8)game_selection.players == 1u)
            sprite_records[17].type = 1u;
    }

    summary = &profile_summaries[(sint16)selection];
    enabled = summary->lap_time[0] != 0u || summary->lap_time[1] != 0u || summary->lap_time[2] != 0u;
    for (index = 0; index < 16; ++index)
    {
        uint8 selected = (uint8)(index == summary->rank ? enabled : 0u);
        TEXT_RECORD *menu = text_menu;

        menu[index + 24].visible = selected;
    }
    for (index = 0; index < 3; ++index)
    {
        uint8 selected = (uint8)(index == summary->profile ? enabled : 0u);
        TEXT_RECORD *menu = text_menu;

        menu[index + 40].visible = selected;
    }
    for (index = 0; index < 4; ++index)
        text_menu[index + 13].visible = (uint8)enabled;
    for (index = 0; index < 2; ++index)
    {
        uint32 offset = 16u * (uint32)index;

        text_menu[index + 43].visible = (uint8)(1u - enabled);
        text_menu[index + 20].visible = (uint8)enabled;
        text_menu[index + 22].visible = (uint8)enabled;
    }
    for (index = 0; index < 2; ++index)
    {
        uint32 offset = 16u * (uint32)index;

        text_menu[index + 18].visible = 1u;
        text_menu[index + 45].visible = 0u;
        text_menu[index + 22].visible = 1u;
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[20].text;
        profile_format_parts(summary->lap_time, destination);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[21].text;
        profile_format_parts(summary->race_time, destination);
    }
    source = &profile_records[(sint16)selection];
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[22].text;
        profile_format_parts(source->lap.time, destination);
    }
    {
        TEXT_RECORD *menu = text_menu;
        char *destination = menu[23].text;
        profile_format_parts(source->times[0], destination);
    }
    if (r_s16(0x800B6AA0u) != 0)
    {
        TEXT_RECORD *menu;

        sprite_records[14].type = 0u;
        text_menu[47].visible = 0u;
        sprite_records[15].type = 0u;
        menu = text_menu;
        menu[48].visible = 0u;
        return (sint32)menu;
    }
    return 0;
}

sint32 profile_compact_rankings(void)
{
    sint16 state = (sint16)game_selection.selection;
    uint32 total;
    uint32 first;
    sint16 limit;
    sint16 index;

    FUNCTION_MARKER(0x8005737Cu, "MAIN.EXE");
    if (state != -1 && state != -3)
        return -3;
    total = (uint32)(uint16)vehicle_racer_count + (uint32)vehicle_leader_count + (uint32)vehicle_trailer_count;
    limit = (sint16)total;
    first = race_selection.ranks[0];
    if (limit > 1)
    {
        for (index = 1; index < limit; ++index)
        {
            uint32 value = race_selection.ranks[(sint32)index];
            if (first < value)
                race_selection.ranks[(sint32)index] = (uint8)(value - 1u);
        }
    }
    race_selection.ranks[0] = (uint8)(total - 1u);
    return (sint32)(total - 1u);
}

sint32 race_capture_time_summaries(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    uint32 profile_level;
    uint32 profile_row;
    uint32 peer;
    PROFILE_SUMMARY *record;
    uint32 peer_index;
    sint32 index;
    sint32 result = -1;
    sint32 selection;

    FUNCTION_MARKER(0x80057448u, "MAIN.EXE");
    if (game_selection.menu_variant != 0u)
        return result;
    result = -3;
    selection = (sint16)game_selection.selection;
    if (selection == -1 || selection == -3)
        return result;
    result = 3;
    if ((sint16)game_selection.mode == 3)
        return result;
    profile_data = profile_current();
    profile_level = profile_data->level;
    profile_row = profile_data->course;
    peer = r_u32(0x800DE154u);
    record = &profile_summaries[profile_row + 6u * profile_level];
    record->rank = race_selection.ranks[0];
    record->profile = profile_selection.slot;
    peer_index = r_u8(peer + 1u);
    record->race_time[0] = (uint16)text_parse_signed_decimal(text_bind(peer + 516u));
    record->race_time[1] = (uint16)text_parse_signed_decimal(text_bind(peer + 519u));
    record->race_time[2] = (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 522u)));
    if (peer_index < 10u)
    {
        uint32 source = peer + 20u + 16u * peer_index;
        record->lap_time[0] = (uint16)text_parse_signed_decimal(text_bind(source));
        record->lap_time[1] = (uint16)text_parse_signed_decimal(text_bind(source + 3u));
        record->lap_time[2] = (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u)));
    }
    for (index = 0; index < 3; ++index)
    {
        uint32 source = peer + 20u + 16u * (uint32)index;
        uint16 *output = race_lap_times[index];
        output[0] = (uint16)text_parse_signed_decimal(text_bind(source));
        output[1] = (uint16)text_parse_signed_decimal(text_bind(source + 3u));
        result = text_parse_signed_decimal(text_bind(source + 6u));
        output[2] = (uint16)(10 * result);
    }
    return result;
}

uint32 profile_update_best_times(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    uint16 parts[3];
    uint32 result;
    sint32 mode;
    sint32 selection;
    PLAYER_PROFILE *profile_data;
    uint32 profile_level;
    uint32 profile_row;
    uint32 peer;
    PROFILE_RECORD *record;
    uint32 source;
    uint32 candidate;
    uint32 current;
    sint32 checked;
    sint32 index;

    FUNCTION_MARKER(0x80057610u, "MAIN.EXE");
    result = game_selection.menu_variant;
    if (result != 0u)
        goto done;
    mode = (sint16)game_selection.mode;
    selection = (sint16)game_selection.selection;
    if (mode != 2 && (selection == -1 || selection == -3))
    {
        result = (uint32)-3;
        goto done;
    }
    result = 3u;
    if (mode == 3)
        goto done;
    profile_data = profile_current();
    profile_level = profile_data->level;
    peer = r_u32(0x800DE154u);
    profile_row = profile_data->course;
    record = &profile_records[profile_row + 6u * profile_level];
    if (mode == 2)
    {
        parts[0] = (uint16)text_parse_signed_decimal(text_bind(peer + 532u));
        parts[1] = (uint16)text_parse_signed_decimal(text_bind(peer + 535u));
        parts[2] = (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 538u)));
        candidate = profile_time_ticks(parts);
        current = profile_time_ticks(record->trial.time);
        if (candidate != 0u && (candidate < current || current == 0u))
        {
            record->trial.time[0] = parts[0];
            record->trial.time[1] = parts[1];
            record->trial.time[2] = parts[2];
            record->trial.boat = race_participants[0].boat;
            result = (uint32)name_copy_player_name_bytes(0u, record->trial.name);
            goto done;
        }
        result = current;
        goto done;
    }

    source = peer + 20u + 16u * r_u8(peer + 1u);
    parts[0] = (uint16)text_parse_signed_decimal(text_bind(source));
    parts[1] = (uint16)text_parse_signed_decimal(text_bind(source + 3u));
    parts[2] = (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u)));
    candidate = profile_time_ticks(parts);
    current = profile_time_ticks(record->lap.time);
    if (candidate < current || current == 0u)
    {
        record->lap.time[0] = parts[0];
        record->lap.time[1] = parts[1];
        record->lap.time[2] = parts[2];
        record->lap.boat = race_participants[0].boat;
        name_copy_player_name_bytes(0u, record->lap.name);
    }

    parts[0] = (uint16)text_parse_signed_decimal(text_bind(peer + 516u));
    parts[1] = (uint16)text_parse_signed_decimal(text_bind(peer + 519u));
    parts[2] = (uint16)(10 * text_parse_signed_decimal(text_bind(peer + 522u)));
    candidate = profile_time_ticks(parts);
    checked = 0;
    do
    {
        current = profile_time_ticks(record->times[checked]);
        ++checked;
        if (current >= candidate || current == 0u)
            break;
    } while ((sint16)checked < 10);
    if ((sint16)checked == 10)
    {
        result = 9u;
        goto done;
    }
    index = (sint16)(checked - 1);
    for (current = 10u; (sint32)current > index; --current)
    {
        sint32 part;
        sint32 character;

        for (part = 0; part < 3; ++part)
            record->times[current][part] = record->times[current - 1u][part];
        record->boats[current] = record->boats[current - 1u];
        for (character = 0; character < 4; ++character)
            record->names[current][character] = record->names[current - 1u][character];
    }
    record->times[index][0] = parts[0];
    record->times[index][1] = parts[1];
    record->times[index][2] = parts[2];
    record->boats[index] = race_participants[0].boat;
    result = (uint32)name_copy_player_name_bytes(0u, record->names[index]);

done:
    return result;
}

sint32 profile_fn_800579d4(sint32 name_index, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    uint32 peer;
    uint32 source;
    PROFILE_RECORD *record;
    uint32 first;
    uint32 second;
    uint32 third;
    uint32 candidate;
    uint32 current;

    FUNCTION_MARKER(0x800579D4u, "MAIN.EXE");
    if (game_selection.menu_variant != 0u)
        return game_selection.menu_variant;
    if ((sint16)game_selection.mode != 2 && ((sint16)game_selection.selection == -1 || (sint16)game_selection.selection == -3))
        return -3;
    if ((sint16)game_selection.mode == 3)
        return 3;
    profile_data = profile_current();
    peer = r_u32(0x800DE154u);
    source = peer + 20u + 16u * r_u8(peer + 1u);
    record = &profile_records[profile_data->course + 6u * profile_data->level];
    first = (uint16)text_parse_signed_decimal(text_bind(source));
    second = (uint16)text_parse_signed_decimal(text_bind(source + 3u));
    third = (uint16)(10 * text_parse_signed_decimal(text_bind(source + 6u)));
    candidate = 60000u * first + 100u * second + third / 10u;
    current = profile_time_ticks(record->lap.time);
    if (candidate < current || current == 0u)
    {
        record->lap.time[0] = (uint16)first;
        record->lap.time[1] = (uint16)second;
        record->lap.time[2] = (uint16)third;
        record->lap.boat = race_participants[0].boat;
    }
    return name_copy_player_name_bytes((uint16)name_index, record->lap.name);
}

PLAYER_PROFILE *profile_current(void)
{
    uint32 mode = (uint8)game_selection.players;

    FUNCTION_MARKER(0x80059CD0u, "MAIN.EXE");
    if (mode == 1u)
        return profile_at(profile_selection.slot);
    return profile_at(mode == 2u ? 3u : 4u);
}

PROFILE_SERIES *profile_get_series(void)
{
    PLAYER_PROFILE *profile = profile_current();

    FUNCTION_MARKER(0x80059D28u, "MAIN.EXE");
    return &profile->series[profile->level];
}

void profile_init_mode_rec(void)
{
    PLAYER_PROFILE *profile_data;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x80059D8Cu, "MAIN.EXE");
    game_selection.menu_variant = 0u;
    vehicle_racer_count = (uint32)(8u);
    profile_data = profile_current();
    selection = (sint16)menu_query_group_value(3, 0);
    w_u16(0x800B69CAu, 0u);
    result = selection < 2;
    if (selection == 1)
    {
        if ((sint16)game_selection.previous_mode == 1)
            profile_restore_backup();
        game_selection.mode = 2u;
        return;
    }
    if (selection >= 2)
    {
        uint32 level = (uint32)(selection - 2);
        result = (sint32)level;
        if (selection < 5)
        {
            PROFILE_SERIES *series;
            sint32 index;

            game_selection.mode = 1u;
            w_u16(0x800B69CAu, 1u);
            profile_data->level = (uint8)((uint8)level);
            profile_at(profile_selection.slot)->course = 0u;
            series = profile_get_series();
            if (series->phase != 0u)
            {
                race_selection.boats[0] = (uint8)series->grid_row;
                result = (uint8)series->completed;
                profile_data->course = (uint8)((uint8)result);
            }
            else
            {
                result = (uint8)menu_boat_select.boat;
                series->completed = (uint16)(0u);
                series->phase = (uint16)(1u);
                race_selection.boats[0] = (uint8)result;
                series->grid_row = (uint16)result;
                for (index = 15; index >= 0; --index)
                    series->points[(uint32)index] = (uint8)(0u);
            }
        }
        return;
    }
    if (selection == 0)
    {
        result = (sint16)game_selection.previous_mode;
        if (result == 1)
            profile_restore_backup();
        game_selection.mode = 0u;
    }
}

sint32 profile_backup_selection(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SELECTION_BACKUP *backup = &profile_selection_backups[profile_selection.slot];
    sint32 group;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80059F38u, "MAIN.EXE");
    backup->active = 1u;
    profile_data->course = (uint8)((uint8)menu_course_select.course);
    backup->course = (uint8)menu_course_select.course;
    backup->level = profile_data->level;
    race_selection.boats[0] = (uint8)menu_boat_select.boat;
    backup->grid_row = (uint8)menu_boat_select.boat;
    for (group = 0; group < 3; ++group)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                profile_selection_backups[group].progress.courses[row][column].attempts = profile_at((uint32)group)->progress.courses[row][column].attempts;
                profile_selection_backups[group].progress.courses[row][column].result = profile_at((uint32)group)->progress.courses[row][column].result;
                profile_selection_backups[group].progress.courses[row][column].state = profile_at((uint32)group)->progress.courses[row][column].state;
            }
        }
    }
    return 0;
}

void profile_restore_backup(void)
{
    uint32 selected = profile_selection.slot;
    PROFILE_SELECTION_BACKUP *backup = &profile_selection_backups[selected];
    PLAYER_PROFILE *profile_data = profile_at(selected);
    sint32 group;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8005A0C4u, "MAIN.EXE");
    if (backup->active == 0u)
        return;
    profile_data->course = (uint8)(backup->course);
    menu_course_select.course = backup->course;
    w_u16(0x800B6C04u, profile_data->level);
    profile_data->level = (uint8)(backup->level);
    race_selection.boats[0] = backup->grid_row;
    menu_boat_select.boat = backup->grid_row;
    for (group = 0; group < 3; ++group)
    {
        for (row = 0; row < 3; ++row)
        {
            for (column = 0; column < 6; ++column)
            {
                profile_at((uint32)group)->progress.courses[row][column].attempts = (uint8)(profile_selection_backups[group].progress.courses[row][column].attempts);
                profile_at((uint32)group)->progress.courses[row][column].result = (uint8)(profile_selection_backups[group].progress.courses[row][column].result);
                profile_at((uint32)group)->progress.courses[row][column].state = (uint8)(profile_selection_backups[group].progress.courses[row][column].state);
            }
        }
    }
}

sint32 profile_reset_recs(void)
{
    sint32 profile_index;
    sint32 result = 0;

    FUNCTION_MARKER(0x8005A258u, "MAIN.EXE");
    for (profile_index = 0; profile_index < 3; ++profile_index)
    {
        PLAYER_PROFILE *profile_data = profile_at((uint32)profile_index);
        if (profile_at((uint32)profile_index)->reset_pending != 0u)
        {
            sint32 group;
            sint32 index;

            profile_at((uint32)profile_index)->reset_pending = 0u;
            profile_copy_templates((sint16)profile_index);
            profile_reset_series((uint32)profile_index);
            profile_selection_backups[profile_index].active = 0u;
            profile_data->course = (uint8)(0u);
            profile_data->level = (uint8)(0u);
            profile_data->max_level = 0u;
            profile_data->reward_flags = (uint8)(0u);
            for (group = 0; group < 3; ++group)
            {
                for (index = 0; index < 3; ++index)
                    profile_data->progress.courses[(uint32)group][2u - (uint32)index].state = (uint8)(4u);
                for (index = 0; index < 3; ++index)
                    profile_data->progress.courses[(uint32)group][3u + (uint32)index].state = (uint8)(5u);
            }
            for (group = 0; group < 3; ++group)
            {
                for (index = 0; index < 6; ++index)
                {
                    profile_data->progress.courses[(uint32)group][(uint32)index].result = (uint8)(0u);
                    profile_data->progress.courses[(uint32)group][(uint32)index].attempts = (uint8)(0u);
                }
            }
            for (index = 9; index >= 0; --index)
                profile_at((uint32)profile_index)->availability[index] = 0u;
            profile_at((uint32)profile_index)->availability[0] = 1u;
            profile_at((uint32)profile_index)->availability[1] = 1u;
            profile_at((uint32)profile_index)->availability[2] = 1u;
        }
        result = profile_index + 1 < 3;
    }
    return result;
}

sint32 profile_advance_menu_select(void)
{
    uint32 frame;
    sint32 result;

    FUNCTION_MARKER(0x8005A3ECu, "MAIN.EXE");
    frame = guest_stack_push(0x28u);
    game_selection.players = (uint16)((game_selection.players & 0xFF00u) | (uint8)(1u));
    if ((sint16)r_u16(0x800B4142u) == 1)
    {
        game_selection.mode = 0u;
        game_selection.previous_mode = 0u;
    }
    else if ((sint16)game_selection.mode == 1)
    {
        menu_update_group_desc_select(3, 0, (sint16)(uint16)(r_u16(0x800B6C04u) + 2u));
    }
    {
        CONTROLLER_STATE no_buttons = {0};
        no_buttons.current = r_u16(frame + 0x10u);
        profile_update_menu_trans(&no_buttons);
    }
    result = profile_reset_recs();
    guest_stack_pop(0x28u);
    return result;
}

sint32 profile_update_menu_trans(CONTROLLER_STATE *input)
{
    sint16 count;
    sint16 selected;
    sint16 next;
    sint16 record_index = 0;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x8005A480u, "MAIN.EXE");
    if (menu_state.input_enabled != 0u && (input->current & 0x40u) != 0u)
    {
        menu_state.sound = 1u;
        menu_state.phase = 25u;
        ranking_noop();
        sprite_clear_anim_recs();
        profile_update_trans();
    }
    count = (sint16)menu_query_group_value(3, 0);
    selected = (sint16)menu_query_group_value(3, 1);
    next = (sint16)(profile_at((uint32)selected)->max_level + 1u);
    profile_selection.slot = (uint8)selected;
    menu_set_group_desc_value(3, 0, next);
    if (next < count)
    {
        menu_update_group_desc_select(3, 0, next);
        count = next;
    }
    w_u16(0x800B6C04u, (uint16)(count - 2));
    if (count == 2)
        record_index = 10;
    else if (count == 3)
        record_index = 11;
    else if (count == 4)
        record_index = 12;
    sprite_records->type = 1u;
    for (index = 10; index < 13; ++index)
        sprite_records[index].type = 0u;
    if (record_index != 0)
    {
        UI_RECORD *record = (sprite_records + (sint32)record_index);
        record->type = 1u;
        if ((sint16)menu_state.selection != 0)
            sprite_apply_prim_vram_mask(record);
        else
            sprite_disable_semitransparency(record);
    }
    else
    {
        sprite_records[4].type = 0u;
        sprite_records[8].type = 0u;
        sprite_records[9].type = 0u;
    }
    for (index = 0; index < 3; ++index)
    {
        text_menu[index + 3].visible = 0;
        result = 1;
    }
    return result;
}

void profile_dispatch_menu_state(void)
{
    sint32 state;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x8005A714u, "MAIN.EXE");
    result = game_selection.ready;
    if (result == 0)
        return;

    state = (sint16)game_selection.mode;
    if (state == 1)
    {
        uint32 first = (uint16)vehicle_racer_count;
        uint32 second = vehicle_leader_count;
        uint32 third = vehicle_trailer_count;

        result_state.grid_count = (uint16)(first + second + third);
        {
            profile_update_row_completion();
            return;
        }
    }
    if (state >= 2)
    {
        if (state == 3)
        {
            vehicle_select_fn_8005e624();
            return;
        }
        if (state == 4)
        {
            game_selection.event = 1u;
            game_selection.mode = 0u;
            game_selection.event_arg = 0u;
            game_selection.selection = (uint16)-1;
            return;
        }
        return;
    }
    if (state != 0)
        return;
    selection = (sint16)game_selection.selection;
    if (selection == -3)
    {
        game_selection.event = 48u;
        game_selection.event_arg = 0u;
        return;
    }
    if (selection == -4)
    {
        profile_confirm_row_select();
        return;
    }
    if (selection < -2)
        return;
    if (selection == -2)
    {
        profile_fn_8005aca8();
        return;
    }
    if (selection == -1)
    {
        if (name_player_name_matches(0, 0x800B425Cu) == 0)
        {
            profile_refresh_menu_ack();
            return;
        }
        {
            profile_confirm_row_select();
            return;
        }
    }
}

void profile_update_row_completion(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    uint32 row = profile_data->level;
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005A8A0u, "MAIN.EXE");
    profile_data->progress.courses[row][column].state = (uint8)(6u);
    menu_notice.type = 3u;
    menu_notice.value = (uint16)(column + 1u);
    if (row == 2u)
        menu_notice.value = (uint16)(column + 7u);
    game_selection.event = 19u;
    game_selection.event_arg = 0u;
    profile_data->course = (uint8)((uint8)(column + 1u));
    series->completed = (uint16)(series->completed + 1u);
    menu_course_select.course = profile_data->course;
    if (profile_data->course != 6u)
        return;
    if ((uint16)ranking_player_is_leading() == 0u)
    {
        profile_apply_action(12);
        return;
    }
    profile_data->reward_flags |= 0x80u;
    if (row == 0u)
    {
        profile_apply_action(4);
        return;
    }
    if (row == 1u)
    {
        profile_apply_action(5);
        return;
    }
    if (row == 2u)
    {
        profile_apply_action(6);
        return;
    }
}

sint32 profile_row_is_complete(sint16 row)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 column;

    FUNCTION_MARKER(0x8005A9F8u, "MAIN.EXE");
    for (column = 0; column < 6; ++column)
    {
        if (profile_data->progress.courses[(uint32)(sint32)row][(uint32)column].state != 6u)
            return 0;
    }
    return 1;
}

void profile_confirm_row_select(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    uint32 column = profile_data->course;
    uint32 row = profile_data->level;
    PROFILE_COURSE *row_data;
    PROFILE_COURSE *cell;
    sint32 result;

    FUNCTION_MARKER(0x8005AA88u, "MAIN.EXE");
    profile_data->level_result = 0u;
    game_selection.event = 11u;
    w_u8(0x800B6C24u, 0u);
    if (row == 3u)
    {
        profile_data->progress.courses[3u][column].attempts = (uint8)(0u);
        return;
    }
    row_data = profile_data->progress.courses[row];
    cell = &row_data[column];
    if (cell->state == 4u)
    {
        cell->state = 6u;
        w_u16(0x800B69E0u, 1u);
        if (row_data[3].state == 5u)
            row_data[3].state = 4u;
        else if (row_data[4].state == 5u)
            row_data[4].state = 4u;
        else if (row_data[5].state == 5u)
            row_data[5].state = 4u;
    }
    if (profile_row_is_complete((sint16)row) != 0)
    {
        if (profile_data->level <= 2u)
            profile_apply_action((sint16)(profile_data->level + 1u));
        if (profile_data->level == profile_data->max_level)
        {
            profile_data->course = (uint8)(0u);
            profile_data->level_result = 1u;
            profile_data->level = (uint8)((uint8)(profile_data->level + 1u));
        }
        if (profile_data->max_level < profile_data->level)
            profile_data->max_level = profile_data->level;
    }
    result = (sint32)(2u * row);
    if (result_state.pickups == 5u)
    {
        result_state.pickups = (uint8)(0u);
        cell = &row_data[column];
        result = 2;
        if (cell->result != 2u)
        {
            profile_data->course = (uint8)((uint8)column);
            profile_data->level = (uint8)((uint8)row);
            cell->result = 1u;
            result = profile_index_map();
            row_data[column].attempts = (uint8)result;
            w_u8(0x800B6C24u, 1u);
        }
    }
}

sint32 profile_fn_8005aca8(void)
{
    FUNCTION_MARKER(0x8005ACA8u, "MAIN.EXE");
    game_selection.event = 17u;
    return 17;
}

sint32 profile_refresh_menu_ack(void)
{
    uint32 state;

    FUNCTION_MARKER(0x8005ACBCu, "MAIN.EXE");

    state = game_selection.menu_variant;
    game_selection.event = 10u;
    if (state == 0u)
        game_selection.event_arg = 0u;
    else if (state == 1u)
        game_selection.event_arg = 1u;
    return 1;
}

sint32 profile_fn_8005ad20(void)
{
    PROFILE_SERIES *series = profile_get_series();
    TEXT_RECORD *menu = text_menu;
    UI_RECORD *records = sprite_records;

    FUNCTION_MARKER(0x8005AD20u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B6B0Cu) != 0)
    {
        menu[0].visible = 0u;
        menu[29].visible = 0u;
        menu[1].visible = 0u;
        menu[2].visible = 0u;
        records[48].type = 0u;
        records[49].type = 0u;
        records[50].type = 0u;
        records[51].type = 1u;
        records[52].type = 1u;
        menu[30].visible = 1u;
        menu[31].visible = 1u;
        menu[32].visible = 1u;
        return (sint32)menu;
    }
    menu[30].visible = 0u;
    menu[31].visible = 0u;
    menu[32].visible = 0u;
    menu[0].visible = 1u;
    records[48].type = 1u;
    records[50].type = 1u;
    records[51].type = 0u;
    if (series->phase == 1u)
    {
        menu[29].visible = 1u;
        menu[1].visible = 0u;
        menu[2].visible = 0u;
        records[49].type = 0u;
    }
    else
    {
        menu[29].visible = 0u;
        menu[1].visible = 1u;
        menu[2].visible = 1u;
        records[49].type = 1u;
    }
    records[51].type = 0u;
    records[52].type = 0u;
    if ((uint32)series->phase - 1u >= 2u)
    {
        sint16 position = menu_state.language == 1u ? 262 : 242;
        records[50].x = (uint16)position;
        return (sint32)records;
    }
    records[50].x = 136u;
    return 136;
}

sint32 profile_fn_8005afa0(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    UI_RECORD *records = sprite_records;
    TEXT_RECORD *menu = text_menu;
    char *text;
    sint32 index;
    sint32 value;

    FUNCTION_MARKER(0x8005AFA0u, "MAIN.EXE");
    menu_course_select.course = series->completed;
    race_selection.boats[0] = (uint8)series->grid_row;
    menu_boat_select.boat = race_selection.boats[0];
    for (index = 0; index < 6; ++index)
    {
        uint8 selected = index == (sint16)menu_course_select.course;
        menu[(2 * (index)) + 4].visible = selected;
        menu[(2 * (index)) + 5].visible = selected;
        records[index + 39].type = selected;
    }
    for (index = 0; index < 9; ++index)
    {
        uint8 selected = index == (sint16)menu_boat_select.boat;
        records[index + 30].type = selected;
        menu[index + 16].visible = selected;
    }
    for (index = 0; index < 3; ++index)
        records[index + 45].type = index == profile_data->level;
    if (series->phase == 2u)
    {
        records[50].x = 136u;
        records[49].type = 0u;
    }
    else
    {
        records[50].x = menu_state.language == 1u ? 262u : 242u;
        records[49].type = 1u;
    }
    profile_build_vehicle_grid(1, (sint16)menu_boat_select.boat, 388, 94);
    ranking_build_order();
    value = ranking_find_empty_slot() + 1;
    text = menu[26].text;
    if (series->completed != 0u)
    {
        text[0] = (uint8)(value / 10 + 48);
        text[1u] = (uint8)(value % 10 + 48);
        text[2u] = 32u;
        text[3u] = 32u;
    }
    else
    {
        for (index = 0; index < 4; ++index)
            text[(uint32)index] = 46u;
    }
    text = menu[28].text;
    if (series->completed == 0u)
    {
        for (index = 0; index < 4; ++index)
            text[(uint32)index] = 46u;
    }
    else if (series->points[0] == 0u)
    {
        text[0] = 48u;
        text[1u] = 48u;
        text[2u] = 32u;
        text[3u] = 32u;
    }
    else
    {
        value = series->points[0];
        text[0] = (uint8)(value / 10 + 48);
        text[1u] = (uint8)(value % 10 + 48);
        text[2u] = 32u;
        text[3u] = 32u;
    }
    return profile_fn_8005ad20();
}

sint32 profile_fn_8005b350(CONTROLLER_STATE *input)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    sint32 index;

    FUNCTION_MARKER(0x8005B350u, "MAIN.EXE");
    if ((sint16)r_u16(0x800B6B0Cu) == 0 && menu_state.input_enabled != 0u)
    {
        if ((input->current & 0x10u) != 0u)
        {
            menu_state.sound = 2u;
            menu_state.phase = 3u;
            profile_restore_backup();
        }
        if ((input->current & 0x40u) != 0u)
        {
            if (series->phase == 1u)
                series->phase = (uint16)(2u);
            menu_state.sound = 1u;
            menu_state.phase = 10u;
        }
    }
    if ((uint32)series->phase - 2u < 2u && menu_state.input_enabled != 0u)
    {
        if ((input->current & 0x80u) != 0u)
        {
            if ((sint16)r_u16(0x800B6B0Cu) == 0 && sprite_records[49].type == 1u && (input->current & 0x40u) == 0u)
            {
                menu_state.sound = 4u;
                w_u16(0x800B6B0Cu, 1u);
            }
            else if ((sint16)r_u16(0x800B6B0Cu) != 0)
            {
                menu_state.sound = 1u;
                w_u16(0x800B6B0Cu, 0u);
            }
        }
        if ((sint16)r_u16(0x800B6B0Cu) != 0 && (input->current & 0x20u) != 0u)
        {
            w_u16(0x800B6B0Cu, 0u);
            series->phase = (uint16)(1u);
            menu_state.phase = 5u;
            menu_state.sound = 1u;
            series->completed = (uint16)(0u);
            profile_data->course = (uint8)(0u);
            for (index = 0; index < 16; ++index)
                series->points[(uint32)index] = (uint8)(0u);
            w_u16(0x800B4140u, (uint16)-1);
        }
    }
    return profile_fn_8005ad20();
}

sint32 profile_fn_8005b570(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    sint32 count;
    sint32 index;

    FUNCTION_MARKER(0x8005B570u, "MAIN.EXE");
    if (series->phase == 2u)
    {
        series->completed = (uint16)(0u);
        profile_data->course = (uint8)(0u);
        series->phase = (uint16)(3u);
        for (index = 0; index < 16; ++index)
            series->points[(uint32)index] = (uint8)(0u);
    }
    if ((sint16)menu_state.phase != 10)
        return 10;
    count = (sint16)series->completed;
    for (index = 0; index < count; ++index)
        profile_data->progress.courses[profile_data->level][(uint32)index].state = (uint8)(6u);
    for (index = count; index < 6; ++index)
        profile_data->progress.courses[profile_data->level][(uint32)index].state = (uint8)(4u);
    return index;
}

sint32 profile_restore_selection(sint16 selection)
{
    PLAYER_PROFILE *profile_data;

    FUNCTION_MARKER(0x8005BE50u, "MAIN.EXE");
    menu_clear_notice();
    profile_data = profile_current();
    profile_get_series();
    menu_textures.selected = UINT16_C(0xFFFF);
    menu_course_select.course = profile_data->course;
    return selection;
}

void profile_complete_selection(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    PROFILE_SERIES *series = profile_get_series();
    uint32 column = profile_data->course;

    FUNCTION_MARKER(0x8005D804u, "MAIN.EXE");
    profile_data->progress.courses[profile_data->level][column].state = (uint8)(6u);
    menu_notice.type = 3u;
    menu_notice.value = (uint16)(column + 1u);
    if (profile_data->level == 2u)
        menu_notice.value = (uint16)(column + 7u);
    game_selection.event = 44u;
    profile_data->course = (uint8)((uint8)(column + 1u));
    series->completed = (uint16)(series->completed + 1u);
    menu_course_select.course = profile_data->course;
    if (profile_data->course == 6u)
    {
        profile_apply_action(10);
        return;
    }
}

void profile_apply_action(sint16 action)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 result;

    FUNCTION_MARKER(0x800637F0u, "MAIN.EXE");
    if (action == 3)
    {
        if ((sint16)r_u16(0x800B69E0u) != 0)
            w_u16(0x800B6BF0u, 5u);
        profile_data->reward_flags |= 0x88u;
        action = profile_selection.slot == 0u ? 7 : (profile_selection.slot == 1u ? 8 : 9);
    }
    switch (action)
    {
        case 1:
            profile_data->reward_flags |= 2u;
            return;
        case 2:
            profile_data->reward_flags |= 4u;
            return;
        case 4:
            profile_data->reward_flags |= 0x10u;
            w_u16(0x800B6BF0u, 2u);
            w_u32(0x800B6AE4u, 1u);
            return;
        case 5:
            profile_data->reward_flags |= 0x20u;
            w_u16(0x800B6BF0u, 3u);
            w_u32(0x800B6AE4u, 2u);
            return;
        case 6:
            profile_data->reward_flags |= 0x40u;
            w_u16(0x800B6BF0u, 4u);
            w_u32(0x800B6AE4u, 3u);
            return;
        case 7:
            profile_data->reward_flags |= 1u;
            return;
        case 8:
        case 9:
            profile_data->reward_flags |= 1u;
            if (profile_unlock(7) != 0)
            {
                w_u32(0x800B6A58u, 1u);
                menu_notice.value = 7u;
                menu_notice.boat = 7u;
                menu_notice.type = 2u;
                game_selection.event = 14u;
            }
            return;
        case 12:
            result = (profile_data->level_result & 7u) + 6;
            w_u16(0x800B6BF0u, (uint16)result);
            return;
        default:
            return;
    }
}

sint32 profile_reset_menu_state(void)
{
    FUNCTION_MARKER(0x80063A4Cu, "MAIN.EXE");
    game_selection.mode = 0u;
    game_selection.event = 8u;
    game_selection.event_arg = 0u;
    return 8;
}

sint32 profile_dispatch_mode_result_handler(uint32 first, uint32 second, uint32 third, uint32 fourth)
{
    sint16 mode;

    FUNCTION_MARKER(0x80063A70u, "MAIN.EXE");
    race_capture_time_summaries(first, second, third, fourth);
    mode = (sint16)game_selection.rules;
    if (mode == 0)
    {
        profile_update_best_times(first, second, third, fourth);
        return tournament_record_champ_result();
    }
    if (mode == 1)
        return tournament_record_result(first, second, third, fourth);
    if (mode == 2)
        return tournament_record_match_result();
    return 2;
}

sint32 menu_save_select_state(void)
{
    sint32 first = (sint16)r_u16(0x800B6AD6u);
    uint16 second = r_u16(0x800B6BF2u);
    uint16 third = r_u16(0x800B6B34u);

    FUNCTION_MARKER(0x80063B00u, "MAIN.EXE");
    game_selection.event = 1u;
    game_selection.mode = 0u;
    game_selection.selection = (uint16)-1;
    vehicle_racer_count = (uint32)((uint32)first);
    vehicle_leader_count = (uint16)(second);
    vehicle_trailer_count = (uint16)(third);
    return -1;
}

void profile_process_select_results(void)
{
    PLAYER_PROFILE *profile_data = profile_current();

    FUNCTION_MARKER(0x80063B4Cu, "MAIN.EXE");
    profile_compact_rankings();
    if (profile_data->level < 3u)
    {
        profile_update_best_times(0u, 0u, 0u, 0u);
        race_capture_time_summaries(0u, 0u, 0u, 0u);
    }
    {
        profile_dispatch_menu_state();
        return;
    }
}

void results_handle_2p(void)
{
    FUNCTION_MARKER(0x80063BA4u, "MAIN.EXE");
    {
        results_2p_complete_route();
        return;
    }
}

sint32 profile_commit_selection(void)
{
    PLAYER_PROFILE *profile_data;
    sint16 initial_state;
    sint16 state;
    sint32 gate;

    FUNCTION_MARKER(0x80063D44u, "MAIN.EXE");
    menu_save_desc_payloads((sint16)menu_state.screen);
    profile_data = profile_current();
    profile_data->level_result = 0u;
    profile_data->course = (uint8)((uint8)menu_course_select.course);
    game_selection.event = 10u;
    initial_state = (sint16)game_selection.mode;
    game_selection.event_arg = (uint8)menu_course_select.course;
    if (initial_state == 4)
    {
        uint16 first = (uint16)vehicle_racer_count;
        uint16 second = vehicle_leader_count;
        uint16 third = vehicle_trailer_count;

        w_u16(0x800B6AD6u, first);
        w_u16(0x800B6BF2u, second);
        w_u16(0x800B6B34u, third);
    }
    gate = (sint16)menu_course_select.blocked;
    result_state.status = (uint8)(0u);
    if (gate != 0)
    {
        game_selection.previous_mode = (uint16)initial_state;
        if ((sint16)menu_course_select.record == 12)
        {
            sint32 row = (sint16)menu_course_select.level;
            uint8 *value;

            game_selection.mode = 3u;
            value = &profile_data->progress.courses[(uint32)row][(uint32)(sint16)menu_course_select.course].attempts;
            if (*value != 0u)
                *value = (uint8)(*value - 1u);
        }
    }
    if ((sint16)menu_course_select.level == 3)
    {
        uint16 selection;
        uint16 saved;

        race_selection.special = 1u;
        selection = menu_course_select.course;
        saved = profile_selection.menu_slot;
        race_selection.resource = (uint16)(selection - 1u);
        w_u16(0x800B6BF6u, saved);
    }
    w_u16(0x800B6AA0u, 5u);
    text_set_hud_visible(0, 1);
    state = (sint16)game_selection.mode;
    if (state != 3 && (sint16)race_selection.special == 0)
    {
        TEXT_RECORD *record;
        sint32 first = (sint16)menu_course_select.course;
        sint32 second = (sint16)menu_course_select.level;

        menu_state.phase = 21u;
        menu_increment_group_offset(first, second);
        record = text_hud;
        record[0].x = 332u;
        record[0].y = 145u;
    }
    if (state != 6 && (uint8)game_selection.players == 2u)
    {
        name_copy_player_name_bytes(0u, hud_text.names[0]);
        return name_copy_player_name_bytes(1u, hud_text.names[1]);
    }
    return 2;
}

void profile_update_limits(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint16 maximum = 0;
    sint16 value;
    sint32 index;

    FUNCTION_MARKER(0x80063F68u, "MAIN.EXE");
    menu_set_group_desc_value(3, 0, (sint16)profile_data->max_level + 1);
    for (index = 0; index < 3; ++index)
    {
        value = profile_at((uint32)index)->max_level;
        if (value > maximum)
            maximum = value;
    }
    if (maximum >= 3)
        maximum = 2;
    menu_set_group_desc_value(4, 0, maximum + 1);
    value = profile_at((uint32)(sint32)(sint16)profile_selection.rules_slot)->max_level;
    if (value >= 3)
        value = 2;
    menu_set_group_desc_value(35, 0, value);
}

sint32 profile_unlock(sint16 index)
{
    PLAYER_PROFILE *profile = profile_current();

    FUNCTION_MARKER(0x80064064u, "MAIN.EXE");
    // Fourth-column completion unlocks the final reserved course state
    if (index == -1)
    {
        uint8 *tail = &profile->progress.courses[3][5].state;

        if (*tail != 0u)
            return 0;
        *tail = 1u;
        return 1;
    }
    if (profile->availability[index] == 0u)
    {
        profile->availability[index] = 1u;
        return 1;
    }
    return 0;
}

sint32 profile_generate_opponents(void)
{
    sint32 record_index;

    FUNCTION_MARKER(0x8006414Cu, "MAIN.EXE");
    game_random_seed(0u);
    for (record_index = 0; record_index < 18; ++record_index)
    {
        PROFILE_RECORD *record = &profile_records[record_index];
        sint32 base_time = profile_default_times[record_index];
        sint32 generated[10];
        sint32 minutes = base_time / 6000;
        sint32 seconds = (base_time % 6000) / 100;
        sint32 remainder = (base_time - 6000 * minutes - 100 * seconds) / 2;
        sint32 random_value;
        sint32 value;
        sint32 index;

        record->lap.time[0] = (uint16)minutes;
        record->lap.time[1] = (uint16)seconds;
        record->lap.time[2] = (uint16)(20 * remainder);
        random_value = game_random_next();
        value = 3 * base_time + 194 + random_value % 500;
        for (index = 0; index < 10; ++index)
        {
            generated[index] = value;
            random_value = game_random_next();
            value += 20 + random_value % 105;
        }
        random_value = game_random_next();
        value = base_time - 44 - random_value % 120;
        minutes = value / 6000;
        seconds = (value % 6000) / 100;
        record->trial.time[0] = (uint16)minutes;
        record->trial.time[1] = (uint16)seconds;
        record->trial.time[2] = (uint16)(20 * ((value - 6000 * minutes - 100 * seconds) / 2));
        for (index = 0; index < 5; ++index)
        {
            record->lap.name[index] = profile_default_names[record_index][1][index];
            record->trial.name[index] = profile_default_names[record_index][0][index];
        }
        for (index = 0; index < 10; ++index)
        {
            sint32 generated_value = generated[index];
            sint32 generated_minutes = generated_value / 6000;
            sint32 generated_seconds = (generated_value % 6000) / 100;
            sint32 character;
            record->times[index][0] = (uint16)generated_minutes;
            record->times[index][1] = (uint16)generated_seconds;
            record->times[index][2] = (uint16)(20 * ((generated_value - 6000 * generated_minutes - 100 * generated_seconds) / 2));
            for (character = 0; character < 5; ++character)
                record->names[index][character] = profile_default_names[record_index][index][character];
        }
    }
    return 0;
}

sint32 profile_merge_unlocks(PLAYER_PROFILE *profile)
{
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80064584u, "MAIN.EXE");
    for (column = 0; column < 10; ++column)
        profile->availability[column] = 0u;
    for (row = 0; row < 3; ++row)
    {
        PLAYER_PROFILE *source = profile_at((uint32)row);
        for (column = 0; column < 10; ++column)
        {
            if (source->availability[column] == 1u)
                profile->availability[column] = 1u;
        }
    }
    return 0;
}

sint32 profile_init_ranking_grid(void)
{
    PLAYER_PROFILE *profile_data = profile_current();
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8006464Cu, "MAIN.EXE");
    profile_data->course = (uint8)(0u);
    profile_data->level = (uint8)(0u);
    profile_data->reward_flags = (uint8)(0u);
    profile_data->max_level = profile_at(profile_selection.rules_slot)->max_level;
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(5u);
    }
    if (profile_data->max_level != 0xFFu)
    {
        for (row = 0; row < (sint32)profile_data->max_level + 1; ++row)
        {
            for (column = 0; column < 6; ++column)
                profile_data->progress.courses[(uint32)row][(uint32)column].state = (uint8)(4u);
        }
    }
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[(uint32)row][(uint32)column].result = (uint8)(0u);
    }
    return profile_merge_unlocks(profile_current());
}
