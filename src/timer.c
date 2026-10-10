#include "render.h"
#include "sound.h"
#include "game.h"
#include "menu.h"
#include "name.h"
#include "vehicle_select.h"
#include "runtime.h"
#include "timer.h"
#include "results.h"
#include "global.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

TIME_REC race_time;
TIME_REC race_bonus_time;
TIME_REC race_freeze_time;
TIME_REC race_end_time;

sint32 time_format(TIME_REC *dst, sint32 value)
{
    FUNCTION_MARKER(0x80034D3Cu, "MAIN.EXE");
    sint32 remainder;
    sint32 quotient;
    uint32 digit;

    quotient = value / 60000;
    dst->text[0] = (uint8)quotient;
    digit = dst->text[0];
    dst->ticks = (uint32)value;
    remainder = (sint32)((uint32)value - 60000u * digit);
    quotient = remainder / 6000;
    dst->text[1] = (uint8)quotient;
    digit = dst->text[1];
    remainder = (sint32)((uint32)remainder - 6000u * digit);
    quotient = remainder / 1000;
    dst->text[3] = (uint8)quotient;
    digit = dst->text[3];
    remainder = (sint32)((uint32)remainder - 1000u * digit);
    quotient = remainder / 100;
    dst->text[4] = (uint8)quotient;
    digit = dst->text[4];
    remainder = (sint32)((uint32)remainder - 100u * digit);
    digit = dst->text[0];
    dst->text[2] = ':';
    dst->text[5] = ':';
    {
        uint32 second = dst->text[1];

        dst->text[0] = (uint8)(digit + '0');
        digit = dst->text[3];
        dst->text[1] = (uint8)(second + '0');
        dst->text[3] = (uint8)(digit + '0');
    }
    quotient = remainder / 10;
    dst->text[6] = (uint8)quotient;
    {
        uint32 fourth = dst->text[4];
        uint32 sixth = dst->text[6];

        dst->text[4] = (uint8)(fourth + '0');
        remainder = (sint32)((uint32)remainder - 10u * sixth);
    }
    quotient = (sint32)dst->text[6] + '0';
    dst->parameter = 0u;
    dst->text[8] = 0u;
    dst->text[7] = (uint8)remainder;
    digit = dst->text[7];
    dst->text[6] = (uint8)quotient;
    dst->text[7] = (uint8)(digit + '0');
    return quotient;
}

sint32 time_init(TIME_REC *dst)
{
    FUNCTION_MARKER(0x80034EE4u, "MAIN.EXE");
    dst->ticks = 0u;
    dst->parameter = 0u;
    dst->text[0] = '0';
    dst->text[1] = '0';
    dst->text[2] = ':';
    dst->text[3] = '0';
    dst->text[4] = '0';
    dst->text[5] = ':';
    dst->text[6] = '0';
    dst->text[7] = '0';
    dst->text[8] = 0u;
    return '0';
}

sint32 time_copy(TIME_REC *dst, const TIME_REC *src)
{
    FUNCTION_MARKER(0x80034F1Cu, "MAIN.EXE");
    uint32 idx;
    uint32 value;

    {
        idx = 0u;
        while (idx < 10u)
        {
            value = src->text[idx];
            dst->text[idx] = (uint8)value;
            ++idx;
            if (value == 0u)
                break;
        }
        while (idx < 10u)
        {
            dst->text[idx] = 0u;
            ++idx;
        }
    }
    dst->parameter = src->parameter;
    value = src->ticks;
    dst->ticks = value;
    return (sint32)value;
}

sint32 time_add(TIME_REC *dst, const TIME_REC *src)
{
    static const uint8 positions[5] = {7u, 6u, 4u, 3u, 1u};
    static const uint8 bases[5] = {10u, 10u, 10u, 6u, 10u};
    uint32 carry = 0u;
    uint32 value;
    uint32 left;
    uint32 right;
    sint32 idx;

    FUNCTION_MARKER(0x80034F6Cu, "MAIN.EXE");

    left = dst->ticks;
    right = src->ticks;
    dst->ticks = left + right;
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 pos = positions[idx];

        right = src->text[pos];
        left = dst->text[pos];
        dst->text[pos] = (uint8)(left + right + 208u + carry);
        value = dst->text[pos];
        carry = value >= (uint32)('0' + bases[idx]);
        if (carry)
            dst->text[pos] = (uint8)(value - bases[idx]);
    }
    value = carry + 208u;
    right = src->text[0];
    left = dst->text[0];
    dst->text[0] = (uint8)(left + right + value);
    return (sint32)value;
}

sint32 time_sub(TIME_REC *dst, const TIME_REC *src)
{
    static const uint8 positions[5] = {7u, 6u, 4u, 3u, 1u};
    static const uint8 bases[5] = {10u, 10u, 10u, 6u, 10u};
    sint32 borrow = 0;
    sint32 idx;

    FUNCTION_MARKER(0x800350A0u, "MAIN.EXE");

    dst->ticks -= src->ticks;
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 pos = positions[idx];
        uint8 value = (uint8)(dst->text[pos] - (src->text[pos] - '0') - borrow);
        borrow = value < '0';
        if (borrow)
            value += bases[idx];
        dst->text[pos] = (uint8)value;
    }
    if (borrow)
    {
        dst->text[0] = '0';
        dst->text[1] = '0';
        dst->text[3] = '0';
        dst->text[4] = '0';
        dst->text[6] = '0';
        dst->text[7] = '0';
        dst->ticks = 0u;
    }
    return '0';
}

sint32 time_increment(TIME_REC *dst)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800351C4u, "MAIN.EXE");
    digit = (uint8)(dst->text[7] + 2u);
    dst->text[7] = digit;
    counter = dst->ticks;
    digit = dst->text[7];
    dst->ticks = counter + 2u;
    if (digit >= '9' + 1)
    {
        next = dst->text[6];
        dst->text[7] = (uint8)(digit - 10u);
        dst->text[6] = (uint8)(next + 1u);
    }
    digit = dst->text[6];
    if (digit >= '9' + 1)
    {
        next = dst->text[4];
        digit = dst->text[6];
        dst->text[6] = (uint8)(digit - 10u);
        dst->text[4] = (uint8)(next + 1u);
    }
    digit = dst->text[4];
    if (digit >= '9' + 1)
    {
        next = dst->text[3];
        digit = dst->text[4];
        dst->text[4] = (uint8)(digit - 10u);
        dst->text[3] = (uint8)(next + 1u);
    }
    digit = dst->text[3];
    if (digit >= '5' + 1)
    {
        next = dst->text[1];
        digit = dst->text[3];
        dst->text[3] = (uint8)(digit - 6u);
        dst->text[1] = (uint8)(next + 1u);
    }
    if (dst->text[1] >= '9' + 1)
    {
        dst->text[0] = '0';
        dst->text[3] = '5';
        dst->text[1] = '9';
        dst->text[4] = '9';
        dst->text[6] = '9';
        dst->text[7] = '9';
        dst->ticks = 59999u;
    }
    return 0;
}

sint32 time_decrement(TIME_REC *dst)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800352C8u, "MAIN.EXE");
    digit = (uint8)(dst->text[7] - 2u);
    dst->text[7] = digit;
    counter = dst->ticks;
    digit = dst->text[7];
    dst->ticks = counter - 2u;
    if (digit < '0')
    {
        next = dst->text[6];
        dst->text[7] = (uint8)(digit + 10u);
        dst->text[6] = (uint8)(next - 1u);
    }
    digit = dst->text[6];
    if (digit < '0')
    {
        next = dst->text[4];
        digit = dst->text[6];
        dst->text[6] = (uint8)(digit + 10u);
        dst->text[4] = (uint8)(next - 1u);
    }
    digit = dst->text[4];
    if (digit < '0')
    {
        next = dst->text[3];
        digit = dst->text[4];
        dst->text[4] = (uint8)(digit + 10u);
        dst->text[3] = (uint8)(next - 1u);
    }
    digit = dst->text[3];
    if (digit < '0')
    {
        next = dst->text[1];
        digit = dst->text[3];
        dst->text[3] = (uint8)(digit + 6u);
        dst->text[1] = (uint8)(next - 1u);
    }
    digit = dst->text[1];
    if (digit < '0')
    {
        next = dst->text[0];
        digit = dst->text[1];
        dst->text[1] = (uint8)(digit + 10u);
        dst->text[0] = (uint8)(next - 1u);
    }
    if (dst->text[0] >= '0')
        return 0;
    dst->ticks = 0u;
    dst->parameter = 0u;
    dst->text[0] = '0';
    dst->text[1] = '0';
    dst->text[3] = '0';
    dst->text[4] = '0';
    dst->text[6] = '0';
    dst->text[7] = '0';
    return 1;
}

void timer_format_text(uint8 *output, sint32 value)
{
    uint8 digit;
    sint32 remainder;

    digit = (uint8)(value / 60000);
    output[0] = (uint8)(digit + '0');
    remainder = (sint32)((uint32)value - 60000u * digit);
    digit = (uint8)(remainder / 6000);
    output[1] = (uint8)(digit + '0');
    remainder = (sint32)((uint32)remainder - 6000u * digit);
    output[2] = ':';
    digit = (uint8)(remainder / 1000);
    output[3] = (uint8)(digit + '0');
    remainder = (sint32)((uint32)remainder - 1000u * digit);
    digit = (uint8)(remainder / 100);
    output[4] = (uint8)(digit + '0');
    remainder = (sint32)((uint32)remainder - 100u * digit);
    output[5] = ':';
    digit = (uint8)(remainder / 10);
    output[6] = (uint8)(digit + '0');
    remainder = (sint32)((uint32)remainder - 10u * digit);
    output[7] = (uint8)((uint8)remainder + '0');
    output[8] = 0u;
}

sint32 time_init_display(void)
{
    sint32 row;
    uint32 column;
    sint16 selection;
    const uint16 *record;
    uint32 value;
    uint8 text[9];

    FUNCTION_MARKER(0x80053CECu, "MAIN.EXE");
    game_selection.ready = 1u;
    result_state.pickups = (uint8)(0u);
    menu_build_player_mode_table();
    row = (sint16)menu_course_select.level;
    column = menu_course_select.course;
    selection = (sint16)(6u * (uint32)row + column);
    record = profile_records[selection].trial.time;
    {
        uint16 first = sound_options.music;
        uint16 second = sound_options.effects;

        sound_mix.music = (uint16)(first);
        sound_mix.effects = (uint16)(second);
    }
    value = profile_time_tenths(record);
    result_state.record_ticks = value;
    if (value != 0u)
    {
        timer_format_text(text, (sint32)value);
        time_copy_chars7((char *)text, hud_text.record);
    }
    else
    {
        memcpy(hud_text.record, "n/a", 4u);
        result_state.record_ticks = (uint32)-1;
    }
    if ((sint16)game_selection.mode == 3)
        return vehicle_select_fn_80061a90();
    return 3;
}
