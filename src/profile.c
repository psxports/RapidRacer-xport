#include "profile.h"
#include "arena.h"
#include "boat.h"
#include "disp.h"
#include "game.h"
#include "input.h"
#include "menu.h"
#include "menu_select.h"
#include "name.h"
#include "race.h"
#include "ranking.h"
#include "render.h"
#include "save.h"
#include "sound.h"
#include "sprite.h"
#include "text.h"
#include "timer.h"
#include "tournament.h"
#include "upgrades.h"
#include "utils.h"
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
PROFILE_RECORD profile_recs[18];

// Captured lap parts from MAIN.EXE 800E18D4
uint16 race_lap_times[3][3];

PLAYER_PROFILE *profile_at(uint32 slot)
{
    return &player_profiles[slot];
}

void profile_reset_series(uint32 slot)
{
    uint32 row;
    uint32 racer;

    for (row = 0; row < 3; ++row)
    {
        PROFILE_SERIES *series = &player_profiles[slot].series[row];

        series->completed = 0;
        series->phase = 0;
        for (racer = 0; racer < 16; ++racer)
            series->points[racer] = 0;
    }
}

// Decode only the series region of the compatible save format
static PROFILE_SERIES *profile_save_series(uint32 offset, uint32 *field)
{
    uint32 relative;
    uint32 slot;
    uint32 within;

    if (offset < 88)
        return NULL;
    relative = offset - 88;
    slot = relative / 274;
    within = relative % 274;
    if (slot >= 5 || within < 208)
        return NULL;
    within -= 208;
    *field = within % 22;
    return &player_profiles[slot].series[within / 22];
}

static uint8 *profile_save_availability(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;

    if (offset < 88)
        return NULL;
    relative = offset - 88;
    slot = relative / 274;
    field = relative % 274;
    if (slot >= 5 || field < 78 || field >= 88)
        return NULL;
    return &player_profiles[slot].availability[field - 78];
}

// Decode the grid region at the compatible file boundary
static uint8 *profile_save_grid(uint32 offset)
{
    uint32 relative;
    uint32 slot;
    uint32 field;
    PROFILE_GRID *row;

    if (offset < 88)
        return NULL;
    relative = offset - 88;
    slot = relative / 274;
    field = relative % 274;
    if (slot >= 5 || field < 88 || field >= 208)
        return NULL;
    field -= 88;
    row = &player_profiles[slot].grid[field / 12];
    field %= 12;
    if (field < 4)
        return &row->choice[field];
    if (field < 8)
        return &row->limit[field - 4];
    return &row->offset[field - 8];
}

// Locate native header fields in the compatible file format
static uint8 *profile_save_header(uint32 offset)
{
    uint32 relative;

    if (offset < 88)
        return NULL;
    relative = offset - 88;
    if (relative / 274 >= 5)
        return NULL;
    switch (relative % 274)
    {
        case 0:
            return &player_profiles[relative / 274].reset_pending;
        case 1:
            return &player_profiles[relative / 274].course;
        case 2:
            return &player_profiles[relative / 274].level;
        case 3:
            return &player_profiles[relative / 274].max_level;
        case 4:
            return &player_profiles[relative / 274].level_result;
        case 5:
            return &player_profiles[relative / 274].reward_flags;
        default:
            return NULL;
    }
}

// Map selection backups only at the compatible file boundary

// Map profile progression only at the compatible file boundary
static uint8 *profile_save_progress(uint32 offset)
{
    uint32 relative;
    uint32 field;
    PROFILE_COURSE *course;

    if (offset < 88)
        return NULL;
    relative = offset - 88;
    field = relative % 274;
    if (relative / 274 >= 5 || field < 6 || field >= 78)
        return NULL;
    field -= 6;
    course = &player_profiles[relative / 274].progress.courses[field % 24 / 6][field % 6];
    if (field < 24)
        return &course->attempts;
    if (field < 48)
        return &course->result;
    return &course->state;
}

// Map audio options only at the compatible file boundary
static uint16 *profile_save_sound(uint32 offset)
{
    if (offset < 78 || offset >= 86)
        return NULL;
    switch ((offset - 78) / 2)
    {
        case 0:
            return &sound_options.music;
        case 1:
            return &sound_options.effects;
        case 2:
            return &sound_options.mode;
        default:
            return &sound_options.mono;
    }
}

// Map profile choices only at the compatible file boundary
static uint8 *profile_save_choice_byte(uint32 offset)
{
    if (offset == 21)
        return &profile_selection.slot;
    if (offset == 5640)
        return &profile_selection.mode_choice;
    if (offset == 5642)
        return &profile_selection.pad;
    return NULL;
}

static uint16 *profile_save_choice_word(uint32 offset)
{
    if (offset < 5636 || offset >= 5640)
        return NULL;
    return offset < 5638 ? &profile_selection.rules_slot : &profile_selection.menu_slot;
}

// Map race options only at the compatible file boundary
static uint8 *profile_save_game_option(uint32 offset)
{
    if (offset == 20)
        return &game_options.unlock_courses;
    if (offset == 22)
        return &race_selection.ai_variant;
    if (offset == 50)
        return &game_options.result_filter;
    if (offset == 24)
        return &game_options.no_cur;
    if (offset == 25)
        return &game_options.split_layout;
    if (offset == 26)
        return &game_options.players_only;
    if (offset >= 27 && offset <= 28)
        return &game_options.handicap[offset - 27];
    return NULL;
}

// Map remaining menu parameter words at the compatible file boundary
static uint16 *profile_save_menu_word(uint32 offset)
{
    if (offset >= 16 && offset < 18)
        return &game_options.round_limit;
    if (offset >= 18 && offset < 20)
        return &game_options.level;
    if (offset >= 76 && offset < 78)
        return &sound_options.slider_val;
    if (offset >= 86 && offset < 88)
        return &race_selection.format;
    return NULL;
}

// Map result bytes at the compatible file boundary
static uint8 *profile_save_result_byte(uint32 offset)
{
    if (offset == 51)
        return &result_state.status;
    if (offset == 52)
        return &result_state.column;
    if (offset == 56)
        return &result_state.pickups;
    if (offset == 5643)
        return &game_selection.attract;
    return NULL;
}

// Map participant arrays at the compatible file boundary
static uint8 *profile_save_participant_byte(uint32 offset)
{
    if (offset >= 29 && offset < 45)
        return &race_selection.ranks[offset - 29];
    if (offset >= 45 && offset < 50)
        return &race_selection.boats[offset - 45];
    return NULL;
}

// Map track choices at the compatible file boundary
static uint16 *profile_save_track(uint32 offset)
{
    if (offset >= 60 && offset < 76)
        return &sound_options.tracks[(offset - 60) / 2];
    return NULL;
}

// Map summary fields only at the compatible file boundary
static uint8 *profile_save_summary_byte(uint32 offset)
{
    uint32 field;
    PROFILE_SUMMARY *summary;

    if (offset < 1600 || offset >= 1852)
        return NULL;
    field = (offset - 1600) % 14;
    summary = &profile_summaries[(offset - 1600) / 14];
    if (field == 0)
        return &summary->rank;
    if (field == 1)
        return &summary->profile;
    return NULL;
}

static uint16 *profile_save_summary_word(uint32 offset)
{
    uint32 field;
    PROFILE_SUMMARY *summary;

    if (offset < 1600 || offset >= 1852)
        return NULL;
    field = (offset - 1600) % 14;
    summary = &profile_summaries[(offset - 1600) / 14];
    if (field >= 2 && field < 8)
        return &summary->lap_time[(field - 2) / 2];
    if (field >= 8)
        return &summary->race_time[(field - 8) / 2];
    return NULL;
}

// Map best records only at the compatible file boundary
static uint8 *profile_save_rec_byte(uint32 offset)
{
    uint32 field;
    PROFILE_RECORD *rec;

    if (offset < 1852 || offset >= 4948)
        return NULL;
    field = (offset - 1852) % 172;
    rec = &profile_recs[(offset - 1852) / 172];
    if (field >= 8 && field < 13)
        return (uint8 *)&rec->lap.name[field - 8];
    if (field >= 22 && field < 27)
        return (uint8 *)&rec->trial.name[field - 22];
    if (field >= 116 && field < 171)
        return (uint8 *)&rec->names[(field - 116) / 5][(field - 116) % 5];
    if (field == 13)
        return &rec->reserved[0];
    if (field == 27)
        return &rec->reserved[1];
    if (field == 171)
        return &rec->reserved[2];
    return NULL;
}

static uint16 *profile_save_rec_word(uint32 offset)
{
    uint32 field;
    PROFILE_RECORD *rec;

    if (offset < 1852 || offset >= 4948)
        return NULL;
    field = (offset - 1852) % 172;
    rec = &profile_recs[(offset - 1852) / 172];
    if (field < 2)
        return &rec->lap.boat;
    if (field < 8)
        return &rec->lap.time[(field - 2) / 2];
    if (field >= 14 && field < 16)
        return &rec->trial.boat;
    if (field >= 16 && field < 22)
        return &rec->trial.time[(field - 16) / 2];
    if (field >= 28 && field < 50)
        return &rec->boats[(field - 28) / 2];
    if (field >= 50 && field < 116)
        return &rec->times[(field - 50) / 6][(field - 50) % 6 / 2];
    return NULL;
}

// Map captured lap times only at the compatible file boundary
static uint16 *profile_save_lap_word(uint32 offset)
{
    if (offset < 4948 || offset >= 4966)
        return NULL;
    return &race_lap_times[(offset - 4948) / 6][(offset - 4948) % 6 / 2];
}

// Map participant fields only at the compatible file boundary
static uint8 *profile_save_race_participant_byte(uint32 offset)
{
    RACE_PARTICIPANT *participant;
    uint32 field;

    if (offset < 1472 || offset >= 1600)
        return NULL;
    participant = &race_participants[(offset - 1472) / 8];
    field = (offset - 1472) % 8;
    if (field == 0)
        return &participant->boat;
    if (field == 1)
        return &participant->variant;
    if (field < 4)
        return &participant->reserved[field - 2];
    return NULL;
}

static uint32 *profile_save_race_participant_word(uint32 offset)
{
    if (offset < 1472 || offset >= 1600 || (offset - 1472) % 8 < 4)
        return NULL;
    return &race_participants[(offset - 1472) / 8].mode;
}

// Map tournament matches only at the compatible file boundary
static uint8 *profile_save_tournament_match(uint32 offset)
{
    if (offset == 5356)
        return &tournament_matches.round;
    if (offset == 5357)
        return &tournament_matches.match;
    if (offset >= 5358 && offset < 5363)
        return &tournament_matches.winners[offset - 5358];
    if (offset >= 5363 && offset < 5368)
        return &tournament_matches.opponents[offset - 5363];
    if (offset >= 5368 && offset < 5373)
        return &tournament_matches.points[offset - 5368];
    return NULL;
}

// Preserve the first twenty grid rounds in the compatible save format
static uint8 *profile_save_tournament_grid_byte(uint32 offset)
{
    if (offset == 5014)
        return &tournament_grid.round;
    if (offset == 5015)
        return &tournament_grid.player;
    if (offset >= 5016 && offset < 5116)
        return &tournament_grid.status[(offset - 5016) / 5][(offset - 5016) % 5];
    return NULL;
}

static uint16 *profile_save_tournament_grid_word(uint32 offset)
{
    if (offset < 5116 || offset >= 5316)
        return NULL;
    return &tournament_grid.seconds[(offset - 5116) / 10][(offset - 5116) % 10 / 2];
}

// Map tournament times only at the compatible file boundary
static uint16 *profile_save_tournament_time(uint32 offset)
{
    TOURNAMENT_TIME *time;
    uint32 field;

    if (offset < 5316 || offset >= 5356)
        return NULL;
    time = &tournament_times[(offset - 5316) / 8];
    field = (offset - 5316) % 8 / 2;
    return field < 3 ? &time->parts[field] : &time->parameter;
}

// Map championship state only at the compatible file boundary
static uint8 *profile_save_tournament_champ(uint32 offset)
{
    if (offset == 5386)
        return &tournament_champ.course;
    if (offset == 5387)
        return &tournament_champ.player;
    if (offset >= 5388 && offset < 5404)
        return &tournament_champ.races[offset - 5388];
    if (offset >= 5404 && offset < 5420)
        return &tournament_champ.points[offset - 5404];
    return NULL;
}

// Map selection and result words at the compatible file boundary
static uint16 *profile_save_selection_tail(uint32 offset)
{
    switch (offset & ~1u)
    {
        case 5660:
            return &race_selection.special;
        case 5668:
            return &race_selection.resource;
        case 5678:
            return &result_state.grid_count;
        case 5680:
            return &result_state.count;
        case 5682:
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
        uint8 len;
        uint8 first;
    } spans[] = {
        {23, 1, 0}, {53, 2, 1}, {57, 3, 3}, {1470, 2, 6}, {4966, 18, 8}, {5373, 13, 26}, {5641, 1, 39}, {5644, 16, 40}, {5662, 2, 56}, {5670, 8, 58},
    };

    uint32 idx;

    for (idx = 0; idx < sizeof(spans) / sizeof(spans[0]); ++idx)
    {
        if (offset >= spans[idx].offset && offset - spans[idx].offset < spans[idx].len)
            return &profile_save_opaque[spans[idx].first + offset - spans[idx].offset];
    }
    return NULL;
}

// Native field encoder from MAIN.EXE 0x8004B644
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
    uint8 *rec_byte = profile_save_rec_byte(offset);
    uint16 *rec_word = profile_save_rec_word(offset);
    uint8 *summary_byte = profile_save_summary_byte(offset);
    uint16 *summary_word = profile_save_summary_word(offset);
    uint8 *participant = profile_save_participant_byte(offset);
    uint16 *track = profile_save_track(offset);
    uint8 *result_byte = profile_save_result_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = menu_select_save_backup(offset);
    uint8 *header = profile_save_header(offset);
    uint8 *grid = profile_save_grid(offset);
    uint8 *availability = profile_save_availability(offset);
    uint32 field;
    PROFILE_SERIES *series = profile_save_series(offset, &field);
    uint16 val;
    uint16 *menu_word = profile_save_menu_word(offset);
    uint8 *option = profile_save_game_option(offset);
    uint8 *choice = profile_save_choice_byte(offset);
    uint16 *sel = profile_save_choice_word(offset);
    uint16 *audio = profile_save_sound(offset);

    if (opaque_byte != NULL)
        return *opaque_byte;
    if (tail_word != NULL)
        return *tail_word >> 8 * (offset & 1);
    if (champ_byte != NULL)
        return *champ_byte;
    if (time_word != NULL)
        return *time_word >> 8 * (offset & 1);
    if (grid_byte != NULL)
        return *grid_byte;
    if (grid_word != NULL)
        return *grid_word >> 8 * (offset & 1);
    if (match_byte != NULL)
        return *match_byte;
    if (participant_byte != NULL)
        return *participant_byte;
    if (participant_word != NULL)
        return *participant_word >> 8 * (offset & 3);
    if (lap_word != NULL)
        return *lap_word >> 8 * (offset & 1);
    if (rec_byte != NULL)
        return *rec_byte;
    if (rec_word != NULL)
        return *rec_word >> 8 * (offset & 1);
    if (summary_byte != NULL)
        return *summary_byte;
    if (summary_word != NULL)
        return *summary_word >> 8 * (offset & 1);
    if (track != NULL)
        return *track >> 8 * (offset & 1);
    if (participant != NULL)
        return *participant;
    if (result_byte != NULL)
        return *result_byte;
    if (menu_word != NULL)
        return *menu_word >> 8 * (offset & 1);
    if (option != NULL)
        return *option;
    if (choice != NULL)
        return *choice;
    if (sel != NULL)
        return *sel >> 8 * (offset & 1);
    if (audio != NULL)
        return *audio >> 8 * (offset & 1);
    if (offset < 8)
    {
        switch (offset / 2)
        {
            case 0:
                val = game_selection.selection;
                break;
            case 1:
                val = game_selection.mode;
                break;
            case 2:
                val = game_selection.rules;
                break;
            default:
                val = game_selection.prev_mode;
                break;
        }
        return val >> 8 * (offset & 1);
    }
    if (offset >= 8 && offset < 16)
    {
        switch (offset)
        {
            case 8:
                return game_selection.flags;
            case 9:
                return game_selection.flags >> 8;
            case 10:
                return game_selection.ready;
            case 11:
                return game_selection.event;
            case 12:
                return game_selection.event_arg;
            case 13:
                return game_selection.menu_variant;
            case 14:
                return game_selection.players;
            default:
                return game_selection.players >> 8;
        }
    }
    if (offset == 55)
        return menu_state.language;
    if (offset >= 5664 && offset < 5668)
        return name_reels.code >> 8 * (offset - 5664);
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
        return 0;
    if (field >= 6)
        return series->points[field - 6];
    val = field < 2 ? series->completed : field < 4 ? series->grid_row : series->phase;
    return val >> 8 * (field & 1);
}

// Native field decoder from MAIN.EXE 0x8004B6BC
void profile_save_write_byte(uint32 offset, uint8 val)
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
    uint8 *rec_byte = profile_save_rec_byte(offset);
    uint16 *rec_word = profile_save_rec_word(offset);
    uint8 *summary_byte = profile_save_summary_byte(offset);
    uint16 *summary_word = profile_save_summary_word(offset);
    uint8 *participant = profile_save_participant_byte(offset);
    uint16 *track = profile_save_track(offset);
    uint8 *result_byte = profile_save_result_byte(offset);
    uint8 *progress = profile_save_progress(offset);
    uint8 *backup = menu_select_save_backup(offset);
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
        *opaque_byte = val;
        return;
    }
    if (tail_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *tail_word = *tail_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (champ_byte != NULL)
    {
        *champ_byte = val;
        return;
    }
    if (time_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *time_word = *time_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (grid_byte != NULL)
    {
        *grid_byte = val;
        return;
    }
    if (grid_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *grid_word = *grid_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (match_byte != NULL)
    {
        *match_byte = val;
        return;
    }
    if (participant_byte != NULL)
    {
        *participant_byte = val;
        return;
    }
    if (participant_word != NULL)
    {
        uint32 shift = 8 * (offset & 3);
        *participant_word = *participant_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (lap_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *lap_word = *lap_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (rec_byte != NULL)
    {
        *rec_byte = val;
        return;
    }
    if (rec_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *rec_word = *rec_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (summary_byte != NULL)
    {
        *summary_byte = val;
        return;
    }
    if (summary_word != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *summary_word = *summary_word & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (track != NULL)
    {
        uint32 shift = 8 * (offset & 1);
        *track = *track & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (participant != NULL)
    {
        *participant = val;
        return;
    }
    if (result_byte != NULL)
    {
        *result_byte = val;
        return;
    }
    target = profile_save_menu_word(offset);
    if (target != NULL)
    {
        shift = 8 * (offset & 1);
        *target = *target & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (option != NULL)
    {
        *option = val;
        return;
    }
    if (choice != NULL)
    {
        *choice = val;
        return;
    }
    target = profile_save_choice_word(offset);
    if (target != NULL)
    {
        shift = 8 * (offset & 1);
        *target = *target & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    target = profile_save_sound(offset);
    if (target != NULL)
    {
        shift = 8 * (offset & 1);
        *target = *target & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (offset < 8)
    {
        switch (offset / 2)
        {
            case 0:
                target = &game_selection.selection;
                break;
            case 1:
                target = &game_selection.mode;
                break;
            case 2:
                target = &game_selection.rules;
                break;
            default:
                target = &game_selection.prev_mode;
                break;
        }
        shift = 8 * (offset & 1);
        *target = *target & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (offset >= 8 && offset < 16)
    {
        switch (offset)
        {
            case 8:
                game_selection.flags = game_selection.flags & 0xFF00u | val;
                break;
            case 9:
                game_selection.flags = game_selection.flags & 0xFFu | val << 8;
                break;
            case 10:
                game_selection.ready = val;
                break;
            case 11:
                game_selection.event = val;
                break;
            case 12:
                game_selection.event_arg = val;
                break;
            case 13:
                game_selection.menu_variant = val;
                break;
            case 14:
                game_selection.players = game_selection.players & 0xFF00u | val;
                break;
            default:
                game_selection.players = game_selection.players & 0xFFu | val << 8;
                break;
        }
        return;
    }
    if (offset == 55)
    {
        menu_state.language = val;
        return;
    }
    if (offset >= 5664 && offset < 5668)
    {
        uint32 shift = 8 * (offset - 5664);

        name_reels.code = name_reels.code & ~(0xFFu << shift) | (uint32)val << shift;
        return;
    }
    if (name != NULL)
    {
        *name = val;
        return;
    }
    if (progress != NULL)
    {
        *progress = val;
        return;
    }
    if (backup != NULL)
    {
        *backup = val;
        return;
    }
    if (header != NULL)
    {
        *header = val;
        return;
    }
    if (grid != NULL)
    {
        *grid = val;
        return;
    }
    if (availability != NULL)
    {
        *availability = val;
        return;
    }
    if (series == NULL)
    {
        return;
    }
    if (field >= 6)
    {
        series->points[field - 6] = val;
        return;
    }
    target = field < 2 ? &series->completed : field < 4 ? &series->grid_row : &series->phase;
    shift = 8 * (field & 1);
    *target = *target & ~(0xFFu << shift) | (uint32)val << shift;
}

// Native formatting shared with MAIN.EXE 80053898
sint32 profile_format_parts(const uint16 parts[3], char *text)
{
    uint32 middle = parts[1];
    uint32 last = parts[2];
    uint32 minutes = parts[0];
    uint32 remainder;
    sint32 result = last;

    FUNCTION_MARKER(0x80053898, "MAIN.EXE");
    text[0] = 48;
    text[2u] = 48;
    text[3u] = 48;
    text[5u] = 48;
    text[6u] = 48;
    if (minutes != 0)
        text[0] = minutes + 48;
    if (middle != 0)
    {
        remainder = middle % 100;
        text[2u] = remainder / 10 + 48;
        text[3u] = remainder % 10 + 48;
    }
    if (last != 0)
    {
        uint32 tens = last / 10 % 100 / 10;
        text[5u] = tens + 48;
        result = 10 * tens;
        text[6u] = last / 10 % 100 % 10 + 48;
    }
    return result;
}

sint32 time_equal(const uint16 a[3], const uint16 b[3])
{
    FUNCTION_MARKER(0x800539BC, "MAIN.EXE");
    if (a[2] != b[2])
        return 0;
    if (a[1] != b[1])
        return 0;
    return a[0] == b[0];
}

// Native arithmetic from MAIN.EXE 80053A00
static uint32 profile_time_ticks(const uint16 parts[3])
{
    FUNCTION_MARKER(0x80053A00, "MAIN.EXE");
    return 60000u * parts[0] + 100u * parts[1] + parts[2] / 10u;
}

// Native display arithmetic from MAIN.EXE 80059C70
uint32 profile_time_tenths(const uint16 parts[3])
{
    FUNCTION_MARKER(0x80059C70, "MAIN.EXE");
    return 6000u * parts[0] + 100u * parts[1] + parts[2] / 10u;
}

void profile_reset_grid_flags(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 row;
    sint32 idx;

    FUNCTION_MARKER(0x80053E14, "MAIN.EXE");
    if ((game_selection.flags & 2u) != 0)
    {
        uint8 flags = profile_data->reward_flags;

        profile_data->max_level = 3;
        profile_data->reward_flags = flags | 0x80u;
        for (row = 0; row < 3; ++row)
        {
            for (idx = 5; idx >= 0; --idx)
                profile_data->progress.courses[row][idx].state = 6;
            for (idx = 5; idx >= 0; --idx)
                profile_data->progress.courses[row][idx].result = 1;
        }
        for (idx = 9; idx >= 0; --idx)
            profile_cur()->availability[idx] = 1;
    }
}

uint8 profile_is_unlocked(sint16 idx)
{
    FUNCTION_MARKER(0x80055EC8, "MAIN.EXE");
    return profile_cur()->availability[idx] == 1;
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
    BOAT_MENU *peer;
    PROFILE_RECORD *rec;
    char *src;
    uint32 candidate;
    uint32 cur;
    sint32 checked;
    sint32 idx;

    FUNCTION_MARKER(0x80057610, "MAIN.EXE");
    result = game_selection.menu_variant;
    if (result != 0)
        goto done;
    mode = (sint16)game_selection.mode;
    selection = game_selection.selection;
    if (mode != 2 && (selection == -1 || selection == -3))
    {
        result = -3;
        goto done;
    }
    result = 3;
    if (mode == 3)
        goto done;
    profile_data = profile_cur();
    profile_level = profile_data->level;
    peer = boat_player_menu(&race_views[0]);
    profile_row = profile_data->course;
    rec = &profile_recs[profile_row + 6 * profile_level];
    if (mode == 2)
    {
        parts[0] = text_parse_int((char *)peer->best_time.text);
        parts[1] = text_parse_int((char *)peer->best_time.text + 3u);
        parts[2] = 10 * text_parse_int((char *)peer->best_time.text + 6u);
        candidate = profile_time_ticks(parts);
        cur = profile_time_ticks(rec->trial.time);
        if (candidate != 0 && (candidate < cur || cur == 0))
        {
            rec->trial.time[0] = parts[0];
            rec->trial.time[1] = parts[1];
            rec->trial.time[2] = parts[2];
            rec->trial.boat = race_participants[0].boat;
            result = name_copy_player_name_bytes(0, rec->trial.name);
            goto done;
        }
        result = cur;
        goto done;
    }

    src = (char *)boat_lap_time(peer, peer->best_lap_idx)->text;
    parts[0] = text_parse_int(src);
    parts[1] = text_parse_int(src + 3u);
    parts[2] = 10 * text_parse_int(src + 6u);
    candidate = profile_time_ticks(parts);
    cur = profile_time_ticks(rec->lap.time);
    if (candidate < cur || cur == 0)
    {
        rec->lap.time[0] = parts[0];
        rec->lap.time[1] = parts[1];
        rec->lap.time[2] = parts[2];
        rec->lap.boat = race_participants[0].boat;
        name_copy_player_name_bytes(0, rec->lap.name);
    }

    parts[0] = text_parse_int((char *)peer->total_time.text);
    parts[1] = text_parse_int((char *)peer->total_time.text + 3u);
    parts[2] = 10 * text_parse_int((char *)peer->total_time.text + 6u);
    candidate = profile_time_ticks(parts);
    checked = 0;
    do
    {
        cur = profile_time_ticks(rec->times[checked]);
        ++checked;
        if (cur >= candidate || cur == 0)
            break;
    } while ((sint16)checked < 10);
    if ((sint16)checked == 10)
    {
        result = 9;
        goto done;
    }
    idx = (sint16)(checked - 1);
    for (cur = 10; (sint32)cur > idx; --cur)
    {
        sint32 part;
        sint32 character;

        for (part = 0; part < 3; ++part)
            rec->times[cur][part] = rec->times[cur - 1][part];
        rec->boats[cur] = rec->boats[cur - 1];
        for (character = 0; character < 4; ++character)
            rec->names[cur][character] = rec->names[cur - 1][character];
    }
    rec->times[idx][0] = parts[0];
    rec->times[idx][1] = parts[1];
    rec->times[idx][2] = parts[2];
    rec->boats[idx] = race_participants[0].boat;
    result = name_copy_player_name_bytes(0, rec->names[idx]);

done:
    return result;
}

sint32 profile_store_best_lap(sint32 name_idx, uint32 unused2, uint32 unused3, uint32 unused4)
{
    PLAYER_PROFILE *profile_data;
    BOAT_MENU *peer;
    char *src;
    PROFILE_RECORD *rec;
    uint32 minutes;
    uint32 seconds;
    uint32 fraction;
    uint32 candidate;
    uint32 cur;

    FUNCTION_MARKER(0x800579D4, "MAIN.EXE");
    if (game_selection.menu_variant != 0)
        return game_selection.menu_variant;
    if (game_selection.mode != 2 && (game_selection.selection == -1 || game_selection.selection == -3))
        return -3;
    if (game_selection.mode == 3)
        return 3;
    profile_data = profile_cur();
    peer = boat_player_menu(&race_views[0]);
    src = (char *)boat_lap_time(peer, peer->best_lap_idx)->text;
    rec = &profile_recs[profile_data->course + 6u * profile_data->level];
    minutes = (uint16)text_parse_int(src);
    seconds = (uint16)text_parse_int(src + 3u);
    fraction = (uint16)(10 * text_parse_int(src + 6u));
    candidate = 60000 * minutes + 100 * seconds + fraction / 10;
    cur = profile_time_ticks(rec->lap.time);
    if (candidate < cur || cur == 0)
    {
        rec->lap.time[0] = minutes;
        rec->lap.time[1] = seconds;
        rec->lap.time[2] = fraction;
        rec->lap.boat = race_participants[0].boat;
    }
    return name_copy_player_name_bytes(name_idx, rec->lap.name);
}

PLAYER_PROFILE *profile_cur(void)
{
    uint32 mode = (uint8)game_selection.players;

    FUNCTION_MARKER(0x80059CD0, "MAIN.EXE");
    if (mode == 1)
        return profile_at(profile_selection.slot);
    return profile_at(mode == 2 ? 3u : 4u);
}

PROFILE_SERIES *profile_get_series(void)
{
    PLAYER_PROFILE *profile = profile_cur();

    FUNCTION_MARKER(0x80059D28, "MAIN.EXE");
    return &profile->series[profile->level];
}

sint32 profile_unlock(sint16 idx)
{
    PLAYER_PROFILE *profile = profile_cur();

    FUNCTION_MARKER(0x80064064, "MAIN.EXE");
    // Fourth-column completion unlocks the final reserved course state
    if (idx == -1)
    {
        uint8 *tail = &profile->progress.courses[3][5].state;

        if (*tail != 0)
            return 0;
        *tail = 1;
        save_flush();
        return 1;
    }
    if (profile->availability[idx] == 0)
    {
        profile->availability[idx] = 1;
        save_flush();
        return 1;
    }
    return 0;
}

sint32 profile_generate_opponents(void)
{
    sint32 rec_idx;

    FUNCTION_MARKER(0x8006414C, "MAIN.EXE");
    random_seed = 0;
    for (rec_idx = 0; rec_idx < 18; ++rec_idx)
    {
        PROFILE_RECORD *rec = &profile_recs[rec_idx];
        sint32 base_time = profile_default_times[rec_idx];
        sint32 generated[10];
        sint32 minutes = base_time / 6000;
        sint32 seconds = base_time % 6000 / 100;
        sint32 remainder = (base_time - 6000 * minutes - 100 * seconds) / 2;
        sint32 random_val;
        sint32 val;
        sint32 idx;

        rec->lap.time[0] = minutes;
        rec->lap.time[1] = seconds;
        rec->lap.time[2] = 20 * remainder;
        random_val = random_next();
        val = 3 * base_time + 194 + random_val % 500;
        for (idx = 0; idx < 10; ++idx)
        {
            generated[idx] = val;
            random_val = random_next();
            val += 20 + random_val % 105;
        }
        random_val = random_next();
        val = base_time - 44 - random_val % 120;
        minutes = val / 6000;
        seconds = val % 6000 / 100;
        rec->trial.time[0] = minutes;
        rec->trial.time[1] = seconds;
        rec->trial.time[2] = 20 * ((val - 6000 * minutes - 100 * seconds) / 2);
        for (idx = 0; idx < 5; ++idx)
        {
            rec->lap.name[idx] = profile_default_names[rec_idx][1][idx];
            rec->trial.name[idx] = profile_default_names[rec_idx][0][idx];
        }
        for (idx = 0; idx < 10; ++idx)
        {
            sint32 generated_val = generated[idx];
            sint32 generated_minutes = generated_val / 6000;
            sint32 generated_seconds = generated_val % 6000 / 100;
            sint32 character;
            rec->times[idx][0] = generated_minutes;
            rec->times[idx][1] = generated_seconds;
            rec->times[idx][2] = 20 * ((generated_val - 6000 * generated_minutes - 100 * generated_seconds) / 2);
            for (character = 0; character < 5; ++character)
                rec->names[idx][character] = profile_default_names[rec_idx][idx][character];
        }
    }
    return 0;
}

sint32 profile_merge_unlocks(PLAYER_PROFILE *profile)
{
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80064584, "MAIN.EXE");
    for (column = 0; column < 10; ++column)
        profile->availability[column] = 0;
    for (row = 0; row < 3; ++row)
    {
        PLAYER_PROFILE *src = profile_at(row);
        for (column = 0; column < 10; ++column)
        {
            if (src->availability[column] == 1)
                profile->availability[column] = 1;
        }
    }
    return 0;
}

sint32 profile_init_ranking_grid(void)
{
    PLAYER_PROFILE *profile_data = profile_cur();
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x8006464C, "MAIN.EXE");
    profile_data->course = 0;
    profile_data->level = 0;
    profile_data->reward_flags = 0;
    profile_data->max_level = profile_at(profile_selection.rules_slot)->max_level;
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[row][column].state = 5;
    }
    if (profile_data->max_level != 0xFF)
    {
        for (row = 0; row < profile_data->max_level + 1; ++row)
        {
            for (column = 0; column < 6; ++column)
                profile_data->progress.courses[row][column].state = 4;
        }
    }
    for (row = 0; row < 3; ++row)
    {
        for (column = 0; column < 6; ++column)
            profile_data->progress.courses[row][column].result = 0;
    }
    return profile_merge_unlocks(profile_cur());
}
