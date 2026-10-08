#include "title.h"
#include "callbacks.h"
#include "global.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 title_init_cb(uint32 state)
{
    uint32 callback_record;
    uint32 title;
    sint32 block;
    sint32 index;

    FUNCTION_MARKER(0x800343F4u, "MAIN.EXE");
    title_relocate_palette_lut();
    callback_record = cb_alloc_node(0x80034C28u);
    w_u32(callback_record + 12u, state);
    w_u32(0x800B6850u, callback_record);
    w_u32(state, r_u16(0x800E05D6u) == 2u ? 0x80080E78u : 0x80080E8Cu);
    title = r_u32(state);
    w_u32(state + 28u, 4u);
    w_u32(state + 24u, 0u);
    w_u32(state + 8u, 0x80034638u);
    w_u32(state + 20u, 0u);
    w_u32(state + 16u, title);
    for (block = 0; block < 4; ++block)
    {
        uint32 destination = state + (uint32)block * 512u + 542u;
        for (index = 0; index < 256; ++index)
            w_u16(destination - (uint32)index * 2u, (uint16)-30654);
    }
    w_u32(state + 4u, 1u);
    return 1;
}

sint32 title_relocate_palette_lut(void)
{
    uint32 source;
    sint32 index;

    FUNCTION_MARKER(0x800344C4u, "MAIN.EXE");
    source = 0x8008C608u;
    if ((r_u32(0x8008C604u) & 8u) != 0u)
    {
        uint32 offset;

        w_u32(0x800B6848u, 0x8008C614u);
        offset = r_u32(source);
        if ((sint32)offset < 0)
            offset += 3u;
        source += (uint32)math_sra_s32(offset, 2u) << 2;
    }
    w_u32(0x800B683Cu, source + 12u);
    source = 0x8008CB48u;
    if ((r_u32(0x8008CB44u) & 8u) != 0u)
    {
        uint32 offset;

        w_u32(0x800B684Cu, 0x8008CB54u);
        offset = r_u32(source);
        if ((sint32)offset < 0)
            offset += 3u;
        source += (uint32)math_sra_s32(offset, 2u) << 2;
    }
    w_u32(0x800B6840u, source + 12u);
    source = 0x8008A048u;
    if ((r_u32(0x8008A044u) & 8u) != 0u)
    {
        uint32 offset;

        w_u32(0x800B6844u, 0x8008A054u);
        offset = r_u32(source);
        if ((sint32)offset < 0)
            offset += 3u;
        source += (uint32)math_sra_s32(offset, 2u) << 2;
    }
    w_u32(0x800B6838u, source + 12u);
    w_u16(r_u32(0x800B6844u), (uint16)-30654);
    for (index = 0; index < 172; index += 4)
    {
        uint32 target = 0x800CA6C0u + 4u * r_u8(0x80091040u + (uint32)index * 2u);
        sint32 first = (sint16)r_u16(0x80091042u + (uint32)index * 2u);
        sint32 second = (sint16)r_u16(0x80091044u + (uint32)index * 2u);

        w_u16(target, (uint16)(200u * (uint32)first + (uint32)second));
        w_u16(target + 2u, r_u16(0x80091046u + (uint32)index * 2u));
    }
    return 0;
}

static sint32 title_glyph_rows_scroll(uint32 node)
{
    uint32 state = r_u32(node + 12u);
    sint32 row = (sint32)r_u32(state + 24u);
    sint32 result = row < (sint32)r_u32(state + 28u);

    FUNCTION_MARKER(0x80034638u, "MAIN.EXE");
    while (result != 0)
    {
        uint32 destination = state + 32u + (uint32)row * 512u;
        sint32 columns = (sint32)r_u32(state + 4u);
        uint32 source = destination + (uint32)columns * 32u;
        sint32 index;

        index = 0;
        if ((sint32)(15u - (uint32)columns) > 0)
        {
            do
            {
                sint32 word;

                for (word = 0; word < 8; ++word)
                    w_u32(destination + (uint32)word * 4u, r_u32(source + (uint32)word * 4u));
                source += 32u;
                destination += 32u;
                index = (sint32)((uint32)index + 1u);
            } while (index < (sint32)(15u - r_u32(state + 4u)));
        }
        if (row == (sint32)(r_u32(state + 28u) - 1u))
        {
            index = 0;
            if ((sint32)r_u32(state + 4u) > 0)
            {
                do
                {
                    sint32 position = (sint32)r_u32(state + 20u);
                    sint32 word;

                    if (position >= 0)
                    {
                        uint32 sequence = r_u32(state + 16u);
                        uint32 character = r_u8(sequence);
                        sint32 pixel = (sint32)((uint32)(sint32)(sint16)r_u16(0x800CA6C0u + character * 4u) + (uint32)position);
                        uint32 glyph = r_u32(0x800B6838u) + (uint32)(pixel >> 1);

                        w_u32(destination, 0x88428842u);
                        for (word = 0; word < 6; ++word)
                        {
                            uint8 second = r_u8(glyph + 100u);
                            uint32 palette = r_u32(0x800B6844u);
                            uint8 first = r_u8(glyph);
                            uint32 high = (pixel & 1) != 0 ? second >> 4 : second & 15u;
                            uint32 low = (pixel & 1) != 0 ? first >> 4 : first & 15u;
                            uint16 high_color = r_u16(palette + high * 2u);
                            uint16 low_color = r_u16(palette + low * 2u);

                            w_u32(destination + (uint32)(word + 1) * 4u, ((uint32)high_color << 16) | low_color);
                            glyph += 200u;
                        }
                        w_u32(destination + 28u, 0x88428842u);
                    }
                    else
                    {
                        for (word = 0; word < 8; ++word)
                            w_u32(destination + (uint32)word * 4u, 0x88428842u);
                    }
                    destination += 32u;
                    position = (sint32)(r_u32(state + 20u) + 1u);
                    {
                        uint32 sequence = r_u32(state + 16u);

                        w_u32(state + 20u, (uint32)position);
                        if (position >= (sint16)r_u16(0x800CA6C2u + (uint32)r_u8(sequence) * 4u))
                        {
                            uint32 next_sequence = sequence + 1u;

                            w_u32(state + 16u, next_sequence);
                            if (r_u8(next_sequence) == 0u)
                            {
                                uint32 first_sequence = r_u32(state);

                                w_u32(state + 16u, first_sequence);
                                xport_update_u32(node + 8u, XPORT_MEMORY_UPDATE_OR, 1u);
                            }
                            sequence = r_u32(state + 16u);
                            w_u32(state + 20u, r_u8(sequence) == 32u ? (uint32)-16 : (uint32)-2);
                        }
                    }
                    index = (sint32)((uint32)index + 1u);
                } while (index < (sint32)r_u32(state + 4u));
            }
        }
        else
        {
            source = state + 544u + (uint32)row * 512u;

            index = 0;
            if ((sint32)r_u32(state + 4u) > 0)
            {
                do
                {
                    sint32 word;

                    for (word = 0; word < 8; ++word)
                        w_u32(destination + (uint32)word * 4u, r_u32(source + (uint32)word * 4u));
                    source += 32u;
                    destination += 32u;
                    index = (sint32)((uint32)index + 1u);
                } while (index < (sint32)r_u32(state + 4u));
            }
        }
        row = (sint32)((uint32)row + 1u);
        result = row < (sint32)r_u32(state + 28u);
    }
    return result;
}

static sint32 display_draw_glyph_pixels(uint32 node)
{
    uint32 state;
    sint32 frame;
    sint32 glyph;
    uint32 output;
    uint32 source;
    sint32 row;

    FUNCTION_MARKER(0x80034994u, "MAIN.EXE");
    state = r_u32(node + 12u);
    frame = (sint32)r_u32(state + 12u);
    glyph = frame / 8;
    w_u32(state + 12u, (uint32)frame + 1u);
    if (glyph == 15)
    {
        glyph = 14;
        xport_update_u32(node + 8u, XPORT_MEMORY_UPDATE_OR, 4u);
    }
    output = state + 1056u;
    {
        uint32 glyph_offset = (uint32)r_u8(0x80091198u + (uint32)glyph) << 7;
        uint32 source_base = r_u32(0x800B683Cu);

        source = source_base + glyph_offset;
    }
    for (row = 0; row < 8; ++row)
    {
        sint32 column;
        uint32 first = 0u;
        uint32 second = 0u;

        for (column = 0; column < 15; ++column)
        {
            if ((column & 7) == 0)
            {
                first = r_u32(source);
                second = r_u32(source + 8u);
                source += 4u;
            }
            {
                uint32 first_index = (first & 15u) * 2u;
                uint32 second_index = (second & 15u) * 2u;
                uint32 palette = r_u32(0x800B6848u);
                uint32 second_color = r_u16(palette + second_index);
                uint32 first_color = r_u16(palette + first_index);

                w_u32(output, first_color | (second_color << 16));
            }
            first >>= 4;
            second >>= 4;
            output += 32u;
        }
        output -= 476u;
        source += 8u;
    }
    return row < 8;
}

static sint32 display_update_mask_rows(uint32 node)
{
    uint32 state;
    sint32 phase;
    sint32 column;
    uint32 fill;
    uint32 mask;
    sint32 row;

    FUNCTION_MARKER(0x80034A70u, "MAIN.EXE");
    state = r_u32(node + 12u);
    phase = (sint32)r_u32(state + 12u);
    column = phase / 2;
    if ((phase & 1) == 0)
        column = 15 - column;
    ++phase;
    w_u32(state + 12u, (uint32)phase);
    if (phase == 16)
        xport_update_u32(node + 8u, XPORT_MEMORY_UPDATE_OR, 2u);
    fill = column < 5 ? 28288u : (column == 5 ? 22976u : 20864u);
    mask = 0xFFFF0000u;
    if ((column & 1) != 0)
    {
        mask = 0x0000FFFFu;
        fill <<= 16;
    }
    for (row = 0; row < 4; ++row)
    {
        uint32 output = state + 32u + (uint32)row * 512u + (uint32)(column / 2) * 4u;
        uint32 pending = r_u32(output);
        sint32 entry;

        for (entry = 0; entry < 15; ++entry)
        {
            uint32 next = entry < 14 ? r_u32(output + 32u) : 0u;

            w_u32(output, (pending & mask) | fill);
            pending = next;
            output += 32u;
        }
    }
    return row < 4;
}

sint32 title_dispatch_state_cb(uint32 node)
{
    uint32 state = r_u32(node + 12u);
    uint32 callback = r_u32(state + 8u);
    uint32 flags;

    FUNCTION_MARKER(0x80034C28u, "MAIN.EXE");
    if (callback == 0x80034638u)
        title_glyph_rows_scroll(node);
    else if (callback == 0x80034994u)
        display_draw_glyph_pixels(node);
    else if (callback == 0x80034A70u)
        display_update_mask_rows(node);
    flags = r_u32(node + 8u);
    if ((flags & 1u) != 0u)
    {
        flags ^= 1u;
        w_u32(node + 8u, flags);
        w_u32(state + 12u, 0u);
        if ((flags & 8u) != 0u)
        {
            w_u32(state + 16u, r_u32(state));
            w_u32(state + 20u, (uint32)-16);
        }
        else
            w_u32(state + 8u, 0x80034A70u);
    }
    flags = r_u32(node + 8u);
    if ((flags & 2u) != 0u)
    {
        w_u32(node + 8u, flags ^ 2u);
        w_u32(state + 12u, 0u);
        w_u32(state + 8u, 0x80034994u);
    }
    flags = r_u32(node + 8u);
    if ((flags & 4u) != 0u)
    {
        w_u32(node + 8u, flags ^ 4u);
        w_u32(state + 28u, 4u);
        w_u32(state, 0x80080E8Cu);
        w_u32(state + 20u, (uint32)-16);
        w_u32(state + 24u, 0u);
        w_u32(state + 12u, 0u);
        w_u32(state + 8u, 0x80034638u);
        w_u32(state + 16u, r_u32(state));
    }
    return LoadImagePSX((PSX_RECT *)psx_addr(0x800B3F48u, sizeof(PSX_RECT)), (uint32 *)psx_addr(state + 32u, 1u));
}
