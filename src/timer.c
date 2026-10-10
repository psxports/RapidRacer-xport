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

sint32 time_format(TIME_REC *dst, sint32 value)
{
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

    dst->ticks -= src->ticks;
    for (idx = 0; idx < 5; ++idx)
    {
        uint32 pos = positions[idx];
        sint32 value = dst->text[pos] - (src->text[pos] - '0') - borrow;
        borrow = value < '0';
        if (borrow)
            value += bases[idx];
        dst->text[pos] = (uint8)value;
    }
    if (borrow)
    {
        time_init(dst);
        return '0';
    }
    dst->text[0] = (uint8)(dst->text[0] - (src->text[0] - '0'));
    return '0';
}

// Temporary codecs for timer owners migrated in stage6
static void timer_load_rec(uint32 src, TIME_REC *dst)
{
    for (uint32 idx = 0; idx < 10u; ++idx)
        dst->text[idx] = r_u8(src + idx);
    dst->parameter = r_u16(src + 10u);
    dst->ticks = r_u32(src + 12u);
}

static void timer_store_rec(uint32 dst, const TIME_REC *src)
{
    for (uint32 idx = 0; idx < 10u; ++idx)
        w_u8(dst + idx, src->text[idx]);
    w_u16(dst + 10u, src->parameter);
    w_u32(dst + 12u, src->ticks);
}

sint32 time_sub_from_legacy(TIME_REC *dst, uint32 src)
{
    TIME_REC rec;

    timer_load_rec(src, &rec);
    return time_sub(dst, &rec);
}

sint32 time_copy_to_legacy(uint32 dst, const TIME_REC *src)
{
    TIME_REC rec;
    sint32 result;

    timer_load_rec(dst, &rec);
    result = time_copy(&rec, src);
    timer_store_rec(dst, &rec);
    return result;
}

sint32 time_add_to_legacy(uint32 dst, const TIME_REC *src)
{
    TIME_REC rec;
    sint32 result;

    timer_load_rec(dst, &rec);
    result = time_add(&rec, src);
    timer_store_rec(dst, &rec);
    return result;
}

sint32 race_format_time(uint32 dst, sint32 value)
{
    TIME_REC rec;
    sint32 result;

    FUNCTION_MARKER(0x80034D3Cu, "MAIN.EXE");
    timer_load_rec(dst, &rec);
    result = time_format(&rec, value);
    timer_store_rec(dst, &rec);
    return result;
}

sint32 time_get_rec_ticks(uint32 value)
{
    FUNCTION_MARKER(0x80034ED8u, "MAIN.EXE");
    return r_s32(value + 12u);
}

sint32 time_init_rec_zero(uint32 dst)
{
    TIME_REC rec;
    sint32 result;

    FUNCTION_MARKER(0x80034EE4u, "MAIN.EXE");
    timer_load_rec(dst, &rec);
    result = time_init(&rec);
    timer_store_rec(dst, &rec);
    return result;
}

sint32 time_copy_rec(uint32 output, uint32 input)
{
    uint32 index;
    uint32 value;

    FUNCTION_MARKER(0x80034F1Cu, "MAIN.EXE");
    if (output != 0u && input != 0u)
    {
        index = 0u;
        while (index < 10u)
        {
            value = r_u8(input + index);
            w_u8(output + index, (uint8)value);
            ++index;
            if (value == 0u)
                break;
        }
        while (index < 10u)
        {
            w_u8(output + index, 0u);
            ++index;
        }
    }
    w_u16(output + 10u, r_u16(input + 10u));
    value = r_u32(input + 12u);
    w_u32(output + 12u, value);
    return (sint32)value;
}

sint32 time_packed_add(uint32 output, uint32 input)
{
    static const uint8 positions[5] = {7u, 6u, 4u, 3u, 1u};
    static const uint8 bases[5] = {10u, 10u, 10u, 6u, 10u};
    uint32 carry = 0u;
    uint32 value;
    uint32 left;
    uint32 right;
    sint32 index;

    FUNCTION_MARKER(0x80034F6Cu, "MAIN.EXE");
    left = r_u32(output + 12u);
    right = r_u32(input + 12u);
    w_u32(output + 12u, left + right);
    for (index = 0; index < 5; ++index)
    {
        uint32 position = positions[index];

        right = r_u8(input + position);
        left = r_u8(output + position);
        w_u8(output + position, (uint8)(left + right + 208u + carry));
        value = r_u8(output + position);
        carry = value >= (uint32)('0' + bases[index]);
        if (carry)
            w_u8(output + position, (uint8)(value - bases[index]));
    }
    value = carry + 208u;
    right = r_u8(input);
    left = r_u8(output);
    w_u8(output, (uint8)(left + right + value));
    return (sint32)value;
}

sint32 time_packed_sub(uint32 output, uint32 input)
{
    static const uint8 positions[5] = {7u, 6u, 4u, 3u, 1u};
    static const uint8 bases[5] = {10u, 10u, 10u, 6u, 10u};
    sint32 borrow = 0;
    sint32 index;

    FUNCTION_MARKER(0x800350A0u, "MAIN.EXE");
    xport_update_u32(output + 12u, XPORT_MEMORY_UPDATE_SUBTRACT, r_u32(input + 12u));
    for (index = 0; index < 5; ++index)
    {
        uint32 position = positions[index];
        sint32 value = r_u8(output + position) - (r_u8(input + position) - '0') - borrow;
        borrow = value < '0';
        if (borrow)
            value += bases[index];
        w_u8(output + position, (uint8)value);
    }
    if (borrow)
    {
        time_init_rec_zero(output);
        return '0';
    }
    w_u8(output, (uint8)(r_u8(output) - (r_u8(input) - '0')));
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

sint32 race_timer_increment(uint32 value)
{
    TIME_REC rec;
    sint32 result;

    timer_load_rec(value, &rec);
    result = time_increment(&rec);
    timer_store_rec(value, &rec);
    return result;
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

sint32 race_timer_decrement(uint32 value)
{
    TIME_REC rec;
    sint32 result;

    timer_load_rec(value, &rec);
    result = time_decrement(&rec);
    timer_store_rec(value, &rec);
    return result;
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

        w_u16(0x80083494u, first);
        w_u16(0x80083490u, second);
    }
    value = profile_time_tenths(record);
    w_u32(0x800B6A54u, value);
    if (value != 0u)
    {
        timer_format_text(text, (sint32)value);
        time_copy_chars7((char *)text, hud_text.record);
    }
    else
    {
        memcpy(hud_text.record, "n/a", 4u);
        w_u32(0x800B6A54u, (uint32)-1);
    }
    if ((sint16)game_selection.mode == 3)
        return vehicle_select_fn_80061a90();
    return 3;
}

sint32 race_decode_time_bcd(uint32 time)
{
    sint32 minutes = 10 * (r_u8(time) >> 4) + (r_u8(time) & 15u);
    sint32 seconds = 10 * (r_u8(time + 1u) >> 4) + (r_u8(time + 1u) & 15u);
    sint32 fraction = 10 * (r_u8(time + 2u) >> 4) + (r_u8(time + 2u) & 15u);

    FUNCTION_MARKER(0x800707E0u, "MAIN.EXE");
    return 75 * (60 * minutes + seconds) + fraction - 150;
}
