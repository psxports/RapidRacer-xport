#include "race_stats.h"
#include "boat.h"
#include "camera.h"
#include "game.h"
#include "input.h"
#include "race.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

RACE_STATS race_stats;

// NTSC MAIN.EXE 8008DDA8, with shared PAL HUD palette coordinates
const RACE_STATS_ROW race_stats_rows[10] = {
    {90, 60, 27892}, {130, 40, 27828}, {155, 30, 27444}, {195, 40, 27828},
    {220, 20, 27444}, {245, 20, 27444}, {270, 20, 27444}, {310, 40, 27828},
    {335, 30, 27444}, {360, 30, 27444}
};

void race_stats_reset(void)
{
    memset(&race_stats, 0, sizeof(race_stats));
}

void race_stats_begin(void)
{
    const BOAT_MENU *menu = boat_player_menu(&race_views[0]);
    const char *suffix = menu->racer_num == 1 ? "st" : menu->racer_num == 2 ? "nd" : menu->racer_num == 3 ? "rd" : "th";
    uint32 best = 0;
    race_stats.active = 1;
    race_stats.ticks = 0;
    sprintf(race_stats.lines[0], "%d%s place", menu->racer_num, suffix);
    sprintf(race_stats.lines[1], "top speed: %d", race_stats.top_speed);
    sprintf(race_stats.lines[2], "average speed: %u", race_stats.samples ? race_stats.speed_sum / race_stats.samples : 0);
    for (uint32 idx = 0; idx < 3; ++idx)
    {
        if (menu->lap_times[idx].ticks < menu->lap_times[best].ticks)
            best = idx;
        sprintf(race_stats.lines[4 + idx], "lap %u: %s pos: %u", idx + 1, menu->lap_times[idx].text + 1, race_stats.lap_pos[idx]);
    }
    sprintf(race_stats.lines[3], "%s lap: %s", result_state.best_lap_ticks == result_state.rec_ticks ? "record" : "best", menu->lap_times[best].text + 1);
    sprintf(race_stats.lines[7], "turbos used: %u", race_stats.turbos_used);
    sprintf(race_stats.lines[8], "turbos collected: %u", race_stats.turbos_collected);
    sprintf(race_stats.lines[9], "time bonuses: %usec", race_stats.bonus_seconds);
}

sint32 race_stats_hide_hud(void)
{
    return race_stats.active && race_stats.ticks * 60 >= 32 * FIELD_RATE;
}

float race_stats_bar_height(float fract)
{
    if (!race_stats.active)
        return 0;
    float height = ((float)race_stats.ticks - 1 + fract) * (60.0f / FIELD_RATE);
    if (height < 0)
        height = 0;
    if (height > 80)
        height = 80;
    return height;
}

float race_stats_x(uint32 row, float fract)
{
    float tick = ((float)race_stats.ticks - 1 + fract) * (60.0f / FIELD_RATE);
    sint32 start = 80;
    for (uint32 idx = 0; idx < row; ++idx)
        start += race_stats_rows[idx].duration + 1;
    // The original counter retains the first completed line for one extra tick
    if (row)
        ++start;
    float step = (tick - start) / race_stats_rows[row].duration;
    if (step < 0)
        step = 0;
    if (step > 1)
        step = 1;
    return 368 - 348 * step;
}

void race_stats_update(void)
{
    if (race_paused || race_cfg.players != 1)
        return;
    BOAT_MENU *menu = boat_player_menu(&race_views[0]);
    if (!race_stats.active)
    {
        if (menu->mode == 4)
        {
            sint32 speed = (sint32)(boat_players[0]->velocity.speed * 256u) / 16000;
            if (speed > 999)
                speed = 999;
            race_stats.speed += (speed - race_stats.speed) / 8;
            if (race_stats.speed > 0 && race_stats.speed < 8)
                --race_stats.speed;
            if (race_stats.speed > race_stats.top_speed)
                race_stats.top_speed = race_stats.speed;
            race_stats.speed_sum += race_stats.speed;
            ++race_stats.samples;
        }
        return;
    }
    ++race_stats.ticks;
    if (race_stats.ticks * 60 >= 80 * FIELD_RATE)
        camera_views[0].mode = 1;
    sint32 complete = 81;
    for (uint32 idx = 0; idx < 10; ++idx)
        complete += race_stats_rows[idx].duration + 1;
    if (race_stats.ticks * 60 >= (uint32)complete * FIELD_RATE &&
        (input_pads[0].pressed || race_stats.ticks * 60 >= (uint32)(complete + 900) * FIELD_RATE))
        menu->mode = 7;
}
