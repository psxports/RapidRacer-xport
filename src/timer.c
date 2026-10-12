#include "menu_select.h"
#include "hud.h"
#include "render.h"
#include "sound.h"
#include "game.h"
#include "menu.h"
#include "name.h"
#include "upgrades.h"
#include "timer.h"
#include "race.h"
#include "utils.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

TIME_REC race_time;
TIME_REC race_bonus_time;
TIME_REC race_freeze_time;
TIME_REC race_end_time;

sint32 time_format(TIME_REC *dst, sint32 val)
{
    FUNCTION_MARKER(0x80034D3C, "MAIN.EXE");
    sint32 remainder;
    sint32 quotient;
    uint32 digit;

    dst->text[0] = val / 60000;
    digit = dst->text[0];
    dst->ticks = val;
    remainder = val - 60000 * digit;
    dst->text[1] = remainder / 6000;
    digit = dst->text[1];
    remainder = remainder - 6000 * digit;
    dst->text[3] = remainder / 1000;
    digit = dst->text[3];
    remainder = remainder - 1000 * digit;
    dst->text[4] = remainder / 100;
    digit = dst->text[4];
    remainder = remainder - 100 * digit;
    digit = dst->text[0];
    dst->text[2] = ':';
    dst->text[5] = ':';

    dst->text[0] = digit + '0';
    digit = dst->text[3];
    dst->text[1] += '0';
    dst->text[3] = digit + '0';
    dst->text[6] = remainder / 10;
    dst->text[4] += '0';
    remainder -= 10 * dst->text[6];
    quotient = dst->text[6] + '0';
    dst->parameter = 0;
    dst->text[8] = 0;
    dst->text[7] = remainder;
    digit = dst->text[7];
    dst->text[6] = quotient;
    dst->text[7] = digit + '0';
    return quotient;
}

sint32 time_init(TIME_REC *dst)
{
    FUNCTION_MARKER(0x80034EE4, "MAIN.EXE");
    dst->ticks = 0;
    dst->parameter = 0;
    dst->text[0] = '0';
    dst->text[1] = '0';
    dst->text[2] = ':';
    dst->text[3] = '0';
    dst->text[4] = '0';
    dst->text[5] = ':';
    dst->text[6] = '0';
    dst->text[7] = '0';
    dst->text[8] = 0;
    return '0';
}

sint32 time_copy(TIME_REC *dst, const TIME_REC *src)
{
    FUNCTION_MARKER(0x80034F1C, "MAIN.EXE");
    uint32 idx;
    uint32 val;

    {
        idx = 0;
        while (idx < 10)
        {
            val = src->text[idx];
            dst->text[idx] = val;
            ++idx;
            if (val == 0)
                break;
        }
        while (idx < 10)
        {
            dst->text[idx] = 0;
            ++idx;
        }
    }
    dst->parameter = src->parameter;
    val = src->ticks;
    dst->ticks = val;
    return val;
}

sint32 time_add(TIME_REC *dst, const TIME_REC *src)
{
    static const uint8 digit_pos[5] = {7, 6, 4, 3, 1};
    static const uint8 bases[5] = {10, 10, 10, 6, 10};
    uint32 carry = 0;
    uint32 val;
    uint32 left;
    uint32 right;
    sint32 idx;

    FUNCTION_MARKER(0x80034F6C, "MAIN.EXE");

    left = dst->ticks;
    right = src->ticks;
    dst->ticks = left + right;
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 pos = digit_pos[idx];

        right = src->text[pos];
        left = dst->text[pos];
        dst->text[pos] = left + right + 208 + carry;
        val = dst->text[pos];
        carry = val >= (uint32)('0' + bases[idx]);
        if (carry)
            dst->text[pos] = val - bases[idx];
    }
    val = carry + 208;
    right = src->text[0];
    left = dst->text[0];
    dst->text[0] = left + right + val;
    return val;
}

sint32 time_sub(TIME_REC *dst, const TIME_REC *src)
{
    static const uint8 digit_pos[5] = {7, 6, 4, 3, 1};
    static const uint8 bases[5] = {10, 10, 10, 6, 10};
    sint32 borrow = 0;
    sint32 idx;

    FUNCTION_MARKER(0x800350A0, "MAIN.EXE");

    dst->ticks -= src->ticks;
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 pos = digit_pos[idx];
        uint8 val = dst->text[pos] - (src->text[pos] - '0') - borrow;
        borrow = val < '0';
        if (borrow)
            val += bases[idx];
        dst->text[pos] = val;
    }
    if (borrow)
    {
        dst->text[0] = '0';
        dst->text[1] = '0';
        dst->text[3] = '0';
        dst->text[4] = '0';
        dst->text[6] = '0';
        dst->text[7] = '0';
        dst->ticks = 0;
    }
    return '0';
}

sint32 time_inc(TIME_REC *dst)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800351C4, "MAIN.EXE");
    digit = dst->text[7] + 2u;
    dst->text[7] = digit;
    counter = dst->ticks;
    digit = dst->text[7];
    dst->ticks = counter + 2;
    if (digit >= '9' + 1)
    {
        next = dst->text[6];
        dst->text[7] = digit - 10u;
        dst->text[6] = next + 1u;
    }
    digit = dst->text[6];
    if (digit >= '9' + 1)
    {
        next = dst->text[4];
        dst->text[6] -= 10u;
        dst->text[4] = next + 1u;
    }
    digit = dst->text[4];
    if (digit >= '9' + 1)
    {
        next = dst->text[3];
        dst->text[4] -= 10u;
        dst->text[3] = next + 1u;
    }
    digit = dst->text[3];
    if (digit >= '5' + 1)
    {
        next = dst->text[1];
        dst->text[3] -= 6u;
        dst->text[1] = next + 1u;
    }
    if (dst->text[1] >= '9' + 1)
    {
        dst->text[0] = '0';
        dst->text[3] = '5';
        dst->text[1] = '9';
        dst->text[4] = '9';
        dst->text[6] = '9';
        dst->text[7] = '9';
        dst->ticks = 59999;
    }
    return 0;
}

sint32 time_dec(TIME_REC *dst)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800352C8, "MAIN.EXE");
    digit = dst->text[7] - 2u;
    dst->text[7] = digit;
    counter = dst->ticks;
    digit = dst->text[7];
    dst->ticks = counter - 2;
    if (digit < '0')
    {
        next = dst->text[6];
        dst->text[7] = digit + 10u;
        dst->text[6] = next - 1u;
    }
    digit = dst->text[6];
    if (digit < '0')
    {
        next = dst->text[4];
        dst->text[6] += 10u;
        dst->text[4] = next - 1u;
    }
    digit = dst->text[4];
    if (digit < '0')
    {
        next = dst->text[3];
        dst->text[4] += 10u;
        dst->text[3] = next - 1u;
    }
    digit = dst->text[3];
    if (digit < '0')
    {
        next = dst->text[1];
        dst->text[3] += 6u;
        dst->text[1] = next - 1u;
    }
    digit = dst->text[1];
    if (digit < '0')
    {
        next = dst->text[0];
        dst->text[1] += 10u;
        dst->text[0] = next - 1u;
    }
    if (dst->text[0] >= '0')
        return 0;
    dst->ticks = 0;
    dst->parameter = 0;
    dst->text[0] = '0';
    dst->text[1] = '0';
    dst->text[3] = '0';
    dst->text[4] = '0';
    dst->text[6] = '0';
    dst->text[7] = '0';
    return 1;
}

static void timer_format_text(uint8 *output, sint32 val)
{
    uint8 digit;
    sint32 remainder;

    digit = val / 60000;
    output[0] = digit + '0';
    remainder = val - 60000u * digit;
    digit = remainder / 6000;
    output[1] = digit + '0';
    remainder = remainder - 6000u * digit;
    output[2] = ':';
    digit = remainder / 1000;
    output[3] = digit + '0';
    remainder = remainder - 1000u * digit;
    digit = remainder / 100;
    output[4] = digit + '0';
    remainder = remainder - 100u * digit;
    output[5] = ':';
    digit = remainder / 10;
    output[6] = digit + '0';
    remainder = remainder - 10u * digit;
    output[7] = remainder + '0';
    output[8] = 0;
}

sint32 time_init_disp(void)
{
    sint32 row;
    uint32 column;
    sint16 selection;
    const uint16 *rec;
    uint32 val;
    uint8 text[9];

    FUNCTION_MARKER(0x80053CEC, "MAIN.EXE");
    game_selection.ready = 1;
    result_state.pickups = 0;
    menu_build_player_mode_table();
    row = menu_course_select.level;
    column = menu_course_select.course;
    selection = 6u * row + column;
    rec = profile_recs[selection].trial.time;

    sound_mix.music = sound_options.music;
    sound_mix.effects = sound_options.effects;
    val = profile_time_tenths(rec);
    result_state.rec_ticks = val;
    if (val != 0)
    {
        timer_format_text(text, val);
        time_copy_chars7((char *)text, hud_text.rec);
    }
    else
    {
        memcpy(hud_text.rec, "n/a", 4u);
        result_state.rec_ticks = -1;
    }
    if (game_selection.mode == 3)
        return upgrades_refresh_unlocks();
    return 3;
}
