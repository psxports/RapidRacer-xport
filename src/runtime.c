#include "runtime.h"
#include <string.h>
#include "game_main.h"
#include "global.h"
#include "callbacks.h"
#include "mdec.h"
#include "vehicle_select.h"
#include "ranking.h"
#include "name.h"
#include "profile.h"
#include "text.h"
#include "mc.h"
#include "sprite.h"
#include "menu.h"
#include "display.h"
#include "effects.h"
#include "pickup.h"
#include "race_events.h"
#include "sound.h"
#include "timer.h"
#include "mesh.h"
#include "object.h"
#include "ai.h"
#include "cd.h"
#include "vehicle.h"
#include "route.h"
#include "arena.h"
#include "render.h"
#include "game.h"
#include "scene.h"
#include "input.h"
#include "psx.h"
#include "xport_trace.h"
#include "psx_gpu.h"
#include <stdio.h>
#include <stdlib.h>

static uint32 rr_cd_data_callback_address;

void cd_set_data_cb(uint32 callback)
{
    rr_cd_data_callback_address = callback;
}

static uint32 rr_vsync_guest_callback;

static void runtime_dispatch_vsync(void)
{
    if (rr_vsync_guest_callback == 0x800387CCu)
        cb_vblank_timer();
}

void runtime_set_vsync_cb(uint32 callback)
{
    rr_vsync_guest_callback = callback;
    VSyncCallback(callback ? (void *)runtime_dispatch_vsync : NULL);
}

typedef struct
{
    sint32 x;
    sint32 y;
    sint32 width;
    sint32 height;
    sint32 background;
    sint32 max_characters;
} RR_FONT_COMPAT;

static RR_FONT_COMPAT rr_font;
static sint32 rr_dump_font;
static sint32 rr_font_texture_x;
static sint32 rr_font_texture_y;
static uint32 rr_cd_ready_callback_address;

static void cd_dispatch_ready(uint8 status, uint8 *result)
{
    uint32 frame;
    uint32 response;
    uint32 index;

    if (rr_cd_ready_callback_address != 0x80038950u || !result)
        return;
    frame = guest_stack_push(8u);
    response = frame;
    for (index = 0u; index < 5u; ++index)
        w_u8(response + index, result[index]);
    cd_handle_ready_cb(status, response);
    guest_stack_pop(8u);
}

void runtime_load_font(sint32 x, sint32 y)
{
    // Image binding for MAIN.EXE:8006A63C, using the original resident font data
    w_u16(0x800D234Cu, LoadClut(psx_addr(0x800A01E0u, 32u), x, y + 128));
    w_u16(0x800D2348u, LoadTPage(psx_addr(0x800A03E0u, 2048u), 0, 0, x, y, 128, 32));
    w_u32(0x800A01D8u, 0u);
    xport_guest_fill(0x800A0058u, (uint8)(0), 384u);
    rr_font_texture_x = x;
    rr_font_texture_y = y;
}

sint32 runtime_open_font(sint32 x, sint32 y, sint32 width, sint32 height, sint32 background, sint32 max_characters)
{
    uint32 font = r_u32(0x800A01D8u);
    uint32 character_offset;
    uint32 record;
    uint32 character_buffer;
    uint32 primitive_buffer;
    uint32 draw_mode;
    sint32 index;

    if (font >= 8u || max_characters < 0)
        return -1;
    if (font == 0u)
        w_u32(0x800A0BE0u, 0u);
    character_offset = r_u32(0x800A0BE0u);
    if ((uint32)max_characters > 1024u - character_offset)
        max_characters = (sint32)(1024u - character_offset);
    record = 0x800A0058u + font * 48u;
    character_buffer = 0x800CDF48u + character_offset;
    primitive_buffer = 0x800CE348u + character_offset * 16u;

    draw_mode = 0xE1000000u;
    if (r_u8(0x800A0C38u) == 1u || r_u8(0x800A0C38u) == 2u)
        draw_mode |= (uint32)r_u16(0x800D2348u) & 0x27FFu;
    else
        draw_mode |= (uint32)r_u16(0x800D2348u) & 0x09FFu;
    w_u8(record + 19u, 2u);
    w_u32(record + 20u, draw_mode);
    w_u32(record + 24u, 0xE2000000u);
    w_u32(record + 44u, width == 0 ? 1u : 0u);
    if (background != 0)
    {
        w_u8(record + 3u, 3u);
        w_u8(record + 7u, background == 2 ? 0x62u : 0x60u);
    }
    w_u16(record + 8u, (uint16)x);
    w_u16(record + 10u, (uint16)y);
    w_u16(record + 12u, (uint16)width);
    w_u16(record + 14u, (uint16)height);
    w_u32(record + 28u, (uint32)max_characters);
    w_u32(record + 40u, 0u);
    w_u32(record + 36u, character_buffer);
    w_u32(record + 32u, primitive_buffer);
    w_u8(character_buffer, 0u);
    for (index = 0; index < max_characters; ++index)
    {
        uint32 primitive = primitive_buffer + (uint32)index * 16u;
        w_u8(primitive + 3u, 3u);
        w_u8(primitive + 7u, 0x74u);
        w_u16(primitive + 14u, r_u16(0x800D234Cu));
    }
    w_u32(0x800A0BE0u, character_offset + (uint32)max_characters);
    w_u32(0x800A01D8u, font + 1u);

    rr_font.x = x;
    rr_font.y = y;
    rr_font.width = width;
    rr_font.height = height;
    rr_font.background = background;
    rr_font.max_characters = max_characters;
    return (sint32)font;
}

void runtime_set_font_dump(sint32 font)
{
    if (font >= 0 && (uint32)font <= r_u32(0x800A01D8u))
    {
        w_u32(0x800A01DCu, (uint32)font);
        w_u32(0x800A0C34u, 0x8006ACBCu);
    }
    rr_dump_font = font;
}

uint32 cd_set_ready_cb(uint32 callback)
{
    uint32 previous = rr_cd_ready_callback_address;
    rr_cd_ready_callback_address = callback;
    CdReadyCallback(callback ? cd_dispatch_ready : NULL);
    return previous;
}

uint32 runtime_build_cd_path_at(uint32 output, uint32 input)
{
    uint32 cursor = output;
    uint32 source = 0x800D6958u;

    while (r_u8(source) != 0u && cursor - output < 125u)
        w_u8(cursor++, r_u8(source++));
    source = input;
    while (r_u8(source) != 0u && cursor - output < 125u)
        w_u8(cursor++, r_u8(source++));
    w_u8(cursor++, ';');
    w_u8(cursor++, '1');
    w_u8(cursor, 0u);
    return output;
}

uint32 runtime_build_cd_path(uint32 input)
{
    return runtime_build_cd_path_at(0x801FFE08u, input);
}

uint32 runtime_join_paths(uint32 prefix, uint32 suffix)
{
    uint32 output = 0x801FFDB0u;
    uint32 cursor = output;

    while (r_u8(prefix) != 0u && cursor - output < 62u)
        w_u8(cursor++, r_u8(prefix++));
    while (r_u8(suffix) != 0u && cursor - output < 62u)
        w_u8(cursor++, r_u8(suffix++));
    w_u8(cursor, 0u);
    return output;
}

sint32 runtime_parse_decimal(uint32 text, sint32 length)
{
    sint32 result = 0;
    sint16 index = 0;
    sint16 remaining = (sint16)length;

    while (--remaining != -1)
    {
        sint32 digit = (sint32)r_u8(text + (uint32)(sint32)index) - '0';

        if (remaining == 2)
            result += 100 * digit;
        else if (remaining == 1)
            result += 10 * digit;
        else if (remaining == 0)
            result += digit;
        ++index;
    }
    return result;
}

void runtime_copy_guest_text(uint32 destination, uint32 source)
{
    uint8 value;

    do
    {
        value = r_u8(source++);
        w_u8(destination++, value);
    } while (value != 0u);
}

void runtime_copy_host_text(uint32 destination, const char *source)
{
    do
    {
        w_u8(destination++, (uint8)*source);
    } while (*source++ != '\0');
}

void runtime_join_guest_text(uint32 destination, uint32 first, uint32 second)
{
    uint8 value;

    while ((value = r_u8(first++)) != 0u)
        w_u8(destination++, value);
    runtime_copy_guest_text(destination, second);
}

void runtime_copy_guest_5(uint32 destination, uint32 source)
{
    uint32 first = r_u32(source);
    uint8 last = r_u8(source + 4u);

    w_u32(destination, first);
    w_u8(destination + 4u, last);
}

void runtime_copy_guest_7(uint32 destination, uint32 source)
{
    uint32 first = r_u32(source);
    uint16 middle = r_u16(source + 4u);
    uint8 last = r_u8(source + 6u);

    w_u32(destination, first);
    w_u16(destination + 4u, middle);
    w_u8(destination + 6u, last);
}

void runtime_copy_guest_8(uint32 destination, uint32 source)
{
    uint32 first = r_u32(source);
    uint32 second = r_u32(source + 4u);

    w_u32(destination, first);
    w_u32(destination + 4u, second);
}

void runtime_copy_guest_9(uint32 destination, uint32 source)
{
    uint32 first = r_u32(source);
    uint32 second = r_u32(source + 4u);
    uint8 last = r_u8(source + 8u);

    w_u32(destination, first);
    w_u32(destination + 4u, second);
    w_u8(destination + 8u, last);
}

void runtime_format_vram_filename(uint32 destination, sint32 selection, sint32 two_player)
{
    char text[24];

    snprintf(text, sizeof(text), two_player != 0 ? "VRAM%d_2P.IFF" : "VRAM%d.IFF", selection);
    runtime_copy_host_text(destination, text);
}
