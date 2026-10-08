#include "menu.h"
#include "name.h"
#include "vehicle_select.h"
#include "runtime.h"
#include "timer.h"
#include "global.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 race_format_time(uint32 output, sint32 value)
{
    sint32 remainder;
    sint32 quotient;
    uint32 digit;

    FUNCTION_MARKER(0x80034D3Cu, "MAIN.EXE");
    quotient = value / 60000;
    w_u8(output, (uint8)quotient);
    digit = r_u8(output);
    w_u32(output + 12u, (uint32)value);
    remainder = (sint32)((uint32)value - 60000u * digit);
    quotient = remainder / 6000;
    w_u8(output + 1u, (uint8)quotient);
    digit = r_u8(output + 1u);
    remainder = (sint32)((uint32)remainder - 6000u * digit);
    quotient = remainder / 1000;
    w_u8(output + 3u, (uint8)quotient);
    digit = r_u8(output + 3u);
    remainder = (sint32)((uint32)remainder - 1000u * digit);
    quotient = remainder / 100;
    w_u8(output + 4u, (uint8)quotient);
    digit = r_u8(output + 4u);
    remainder = (sint32)((uint32)remainder - 100u * digit);
    digit = r_u8(output);
    w_u8(output + 2u, ':');
    w_u8(output + 5u, ':');
    {
        uint32 second = r_u8(output + 1u);

        w_u8(output, (uint8)(digit + '0'));
        digit = r_u8(output + 3u);
        w_u8(output + 1u, (uint8)(second + '0'));
        w_u8(output + 3u, (uint8)(digit + '0'));
    }
    quotient = remainder / 10;
    w_u8(output + 6u, (uint8)quotient);
    {
        uint32 fourth = r_u8(output + 4u);
        uint32 sixth = r_u8(output + 6u);

        w_u8(output + 4u, (uint8)(fourth + '0'));
        remainder = (sint32)((uint32)remainder - 10u * sixth);
    }
    quotient = (sint32)r_u8(output + 6u) + '0';
    w_u16(output + 10u, 0u);
    w_u8(output + 8u, 0u);
    w_u8(output + 7u, (uint8)remainder);
    digit = r_u8(output + 7u);
    w_u8(output + 6u, (uint8)quotient);
    w_u8(output + 7u, (uint8)(digit + '0'));
    return quotient;
}

sint32 time_get_rec_ticks(uint32 value)
{
    FUNCTION_MARKER(0x80034ED8u, "MAIN.EXE");
    return r_s32(value + 12u);
}

sint32 time_init_rec_zero(uint32 output)
{
    FUNCTION_MARKER(0x80034EE4u, "MAIN.EXE");
    w_u32(output + 12u, 0u);
    w_u16(output + 10u, 0u);
    w_u8(output, '0');
    w_u8(output + 1u, '0');
    w_u8(output + 2u, ':');
    w_u8(output + 3u, '0');
    w_u8(output + 4u, '0');
    w_u8(output + 5u, ':');
    w_u8(output + 6u, '0');
    w_u8(output + 7u, '0');
    w_u8(output + 8u, 0u);
    return '0';
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

sint32 race_timer_increment(uint32 value)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800351C4u, "MAIN.EXE");
    digit = (uint8)(r_u8(value + 7u) + 2u);
    w_u8(value + 7u, digit);
    counter = r_u32(value + 12u);
    digit = r_u8(value + 7u);
    w_u32(value + 12u, counter + 2u);
    if (digit >= '9' + 1)
    {
        next = r_u8(value + 6u);
        w_u8(value + 7u, (uint8)(digit - 10u));
        w_u8(value + 6u, (uint8)(next + 1u));
    }
    digit = r_u8(value + 6u);
    if (digit >= '9' + 1)
    {
        next = r_u8(value + 4u);
        digit = r_u8(value + 6u);
        w_u8(value + 6u, (uint8)(digit - 10u));
        w_u8(value + 4u, (uint8)(next + 1u));
    }
    digit = r_u8(value + 4u);
    if (digit >= '9' + 1)
    {
        next = r_u8(value + 3u);
        digit = r_u8(value + 4u);
        w_u8(value + 4u, (uint8)(digit - 10u));
        w_u8(value + 3u, (uint8)(next + 1u));
    }
    digit = r_u8(value + 3u);
    if (digit >= '5' + 1)
    {
        next = r_u8(value + 1u);
        digit = r_u8(value + 3u);
        w_u8(value + 3u, (uint8)(digit - 6u));
        w_u8(value + 1u, (uint8)(next + 1u));
    }
    if (r_u8(value + 1u) >= '9' + 1)
    {
        w_u8(value, '0');
        w_u8(value + 3u, '5');
        w_u8(value + 1u, '9');
        w_u8(value + 4u, '9');
        w_u8(value + 6u, '9');
        w_u8(value + 7u, '9');
        w_u32(value + 12u, 59999u);
    }
    return 0;
}

sint32 race_timer_decrement(uint32 value)
{
    uint8 digit;
    uint8 next;
    uint32 counter;

    FUNCTION_MARKER(0x800352C8u, "MAIN.EXE");
    digit = (uint8)(r_u8(value + 7u) - 2u);
    w_u8(value + 7u, digit);
    counter = r_u32(value + 12u);
    digit = r_u8(value + 7u);
    w_u32(value + 12u, counter - 2u);
    if (digit < '0')
    {
        next = r_u8(value + 6u);
        w_u8(value + 7u, (uint8)(digit + 10u));
        w_u8(value + 6u, (uint8)(next - 1u));
    }
    digit = r_u8(value + 6u);
    if (digit < '0')
    {
        next = r_u8(value + 4u);
        digit = r_u8(value + 6u);
        w_u8(value + 6u, (uint8)(digit + 10u));
        w_u8(value + 4u, (uint8)(next - 1u));
    }
    digit = r_u8(value + 4u);
    if (digit < '0')
    {
        next = r_u8(value + 3u);
        digit = r_u8(value + 4u);
        w_u8(value + 4u, (uint8)(digit + 10u));
        w_u8(value + 3u, (uint8)(next - 1u));
    }
    digit = r_u8(value + 3u);
    if (digit < '0')
    {
        next = r_u8(value + 1u);
        digit = r_u8(value + 3u);
        w_u8(value + 3u, (uint8)(digit + 6u));
        w_u8(value + 1u, (uint8)(next - 1u));
    }
    digit = r_u8(value + 1u);
    if (digit < '0')
    {
        next = r_u8(value);
        digit = r_u8(value + 1u);
        w_u8(value + 1u, (uint8)(digit + 10u));
        w_u8(value, (uint8)(next - 1u));
    }
    if (r_u8(value) >= '0')
        return 0;
    w_u32(value + 12u, 0u);
    w_u16(value + 10u, 0u);
    w_u8(value, '0');
    w_u8(value + 1u, '0');
    w_u8(value + 3u, '0');
    w_u8(value + 4u, '0');
    w_u8(value + 6u, '0');
    w_u8(value + 7u, '0');
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
    uint32 record;
    uint32 value;
    uint8 text[9];

    FUNCTION_MARKER(0x80053CECu, "MAIN.EXE");
    w_u8(0x800E058Au, 1u);
    w_u8(0x800E05B8u, 0u);
    menu_build_player_mode_table();
    row = r_s16(0x800B4246u);
    column = r_u16(0x800B4244u);
    selection = (sint16)(6u * (uint32)row + column);
    record = 0x800E0CCCu + 172u * (uint32)(sint32)selection;
    {
        uint16 first = r_u16(0x800E05CEu);
        uint16 second = r_u16(0x800E05D0u);

        w_u16(0x80083494u, first);
        w_u16(0x80083490u, second);
    }
    value = time_parts_to_tenths(record);
    w_u32(0x800B6A54u, value);
    if (value != 0u)
    {
        timer_format_text(text, (sint32)value);
        runtime_copy_host_text(0x80083968u, (const char *)&text[1]);
    }
    else
    {
        uint32 unavailable = r_u32(0x800B424Cu);

        w_u32(0x80083968u, unavailable);
        w_u32(0x800B6A54u, (uint32)-1);
    }
    if (r_s16(0x800E0582u) == 3)
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
