#include <string.h>
#include "sprite.h"
#include <stdlib.h>
#include "text.h"
#include "global.h"
#include "xport_trace.h"

// Immutable font metrics extracted from the reviewed MAIN.EXE
static const uint8 text_alphabet_0[] = {49, 50, 51, 52, 53, 54, 55, 56, 57, 48, 58, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 46, 59, 32, 35, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251, 169, 252, 253, 254, 255, 223, 222, 156, 149, 43, 45};
static const TEXT_METRIC text_metrics_0[] = {
    {0, 0, 20, 16},    {20, 0, 32, 16}, {52, 0, 32, 16},  {84, 0, 32, 16},  {116, 0, 32, 16}, {148, 0, 32, 16},  {180, 0, 28, 16},  {0, 19, 32, 16},   {32, 19, 32, 16}, {64, 19, 32, 16},  {96, 19, 20, 16},  {140, 19, 28, 16}, {168, 19, 28, 16},  {196, 19, 24, 16},  {0, 35, 32, 16},    {32, 35, 28, 16},   {60, 36, 28, 16},   {88, 35, 32, 19},   {120, 35, 28, 16}, {148, 35, 20, 16}, {168, 35, 28, 20}, {196, 35, 28, 16}, {0, 55, 20, 16},    {20, 55, 36, 16},   {56, 55, 28, 16},   {84, 55, 28, 16}, {112, 55, 32, 19}, {144, 55, 28, 19}, {172, 55, 24, 16}, {196, 55, 28, 16}, {0, 72, 24, 16},    {24, 72, 32, 16},   {56, 72, 24, 16},   {80, 72, 36, 16}, {168, 72, 28, 16}, {224, 72, 32, 19}, {196, 72, 28, 16},  {200, 167, 12, 16}, {208, 0, 16, 18}, {132, 75, 20, 16},
    {116, 19, 24, 16}, {0, 92, 28, 16}, {28, 92, 28, 16}, {56, 92, 28, 16}, {84, 92, 28, 16}, {112, 92, 28, 16}, {140, 92, 28, 16}, {168, 92, 40, 16}, {0, 109, 24, 19}, {24, 109, 28, 16}, {52, 109, 28, 16}, {80, 109, 28, 16}, {108, 109, 28, 16}, {136, 109, 20, 16}, {156, 109, 20, 16}, {176, 109, 20, 16}, {196, 109, 20, 16}, {134, 167, 30, 16}, {0, 130, 28, 16},  {28, 130, 28, 16}, {56, 130, 28, 16}, {84, 130, 28, 16}, {112, 130, 28, 16}, {140, 130, 28, 16}, {168, 130, 32, 17}, {0, 148, 28, 16}, {28, 148, 28, 16}, {56, 148, 28, 16}, {132, 75, 20, 16}, {84, 148, 28, 16}, {112, 148, 32, 19}, {144, 148, 32, 19}, {176, 148, 32, 19}, {0, 167, 32, 16}, {32, 167, 32, 16}, {64, 167, 40, 16}, {115, 167, 16, 16}, {0, 1, 12, 11},     {12, 1, 20, 11},
};
static const uint8 text_alphabet_1[] = {49, 50, 51, 52, 53, 54, 55, 56, 57, 48, 59, 58, 35, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 47, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251, 169, 252, 253, 254, 255, 223, 222, 156, 149, 43, 45, 46, 37};
static const TEXT_METRIC text_metrics_1[] = {
    {0, 1, 12, 11},    {12, 1, 20, 11},   {32, 1, 20, 11},   {52, 1, 20, 11},   {72, 1, 20, 11},   {92, 1, 20, 11}, {172, 1, 20, 11}, {112, 1, 20, 11}, {132, 1, 20, 11}, {152, 1, 20, 11}, {164, 1, 12, 11}, {164, 13, 12, 11}, {12, 13, 12, 11},  {24, 13, 16, 11},  {40, 13, 16, 11},  {56, 13, 16, 11},  {72, 13, 16, 11},  {88, 13, 16, 11}, {106, 13, 16, 11}, {120, 13, 16, 14}, {136, 13, 16, 11}, {152, 13, 12, 11}, {0, 27, 16, 14},  {16, 27, 16, 11}, {32, 27, 12, 11},  {44, 27, 24, 11},  {68, 27, 16, 11},  {84, 27, 16, 11},  {100, 27, 16, 14}, {116, 27, 16, 14}, {132, 27, 16, 11}, {148, 27, 16, 11}, {164, 27, 12, 11}, {0, 41, 16, 11},  {16, 41, 16, 11}, {32, 41, 20, 11}, {52, 41, 16, 11},  {68, 41, 16, 14},  {84, 41, 16, 11}, {178, 82, 16, 11},
    {100, 41, 16, 11}, {116, 41, 16, 11}, {132, 41, 16, 11}, {148, 41, 16, 11}, {164, 41, 16, 11}, {0, 55, 16, 11}, {16, 55, 24, 11}, {40, 55, 16, 14}, {56, 55, 16, 11}, {72, 55, 16, 11}, {88, 55, 16, 11}, {104, 55, 16, 11}, {120, 55, 12, 11}, {132, 55, 12, 11}, {144, 55, 12, 11}, {156, 55, 12, 11}, {168, 55, 16, 11}, {0, 69, 16, 11},  {16, 69, 16, 11},  {32, 69, 16, 11},  {48, 69, 16, 11},  {64, 69, 16, 11},  {80, 69, 16, 11}, {96, 69, 20, 13}, {116, 69, 16, 11}, {132, 69, 16, 11}, {148, 69, 16, 11}, {180, 69, 16, 11}, {164, 69, 16, 11}, {0, 82, 16, 14},   {16, 82, 16, 14},  {32, 82, 20, 11},  {52, 82, 20, 11},  {72, 82, 20, 11}, {92, 82, 24, 11}, {116, 82, 8, 11}, {124, 82, 12, 11}, {136, 82, 12, 11}, {148, 82, 8, 11}, {180, 13, 20, 11},
};
static const uint8 text_alphabet_2[] = {49, 50, 51, 52, 53, 54, 55, 56, 57, 48, 59, 58, 35, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 37, 38, 43, 45, 46, 47, 42, 224, 225, 226, 227, 228, 229, 230, 231, 233, 232, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251, 169, 252, 253, 254, 255, 223, 222, 156, 149, 43, 45};
static const TEXT_METRIC text_metrics_2[] = {
    {0, 0, 8, 8},   {8, 0, 16, 8},  {24, 0, 16, 8},  {40, 0, 16, 8}, {56, 0, 16, 8},  {72, 0, 16, 8},  {88, 0, 16, 8},   {104, 0, 16, 8},  {120, 0, 16, 8},  {136, 0, 16, 8}, {0, 9, 8, 9},    {128, 9, 8, 8},    {8, 9, 12, 8},   {20, 9, 12, 8},  {32, 9, 12, 8},  {44, 9, 12, 8},  {56, 9, 16, 8}, {72, 9, 12, 8}, {84, 9, 12, 8}, {96, 9, 12, 10}, {108, 9, 12, 8},  {120, 9, 8, 8}, {0, 19, 12, 10}, {12, 19, 12, 8}, {24, 19, 8, 8},  {32, 19, 16, 8}, {48, 19, 12, 8}, {60, 19, 12, 8}, {72, 19, 12, 10}, {84, 19, 12, 10}, {96, 19, 12, 8},  {108, 19, 12, 8}, {120, 19, 10, 8}, {0, 29, 12, 8},   {12, 29, 12, 8},  {24, 29, 16, 8},  {40, 29, 12, 8}, {52, 29, 12, 10}, {64, 29, 12, 8}, {136, 9, 16, 8}, {136, 19, 16, 8}, {76, 57, 8, 8},
    {84, 57, 7, 8}, {92, 57, 4, 8}, {96, 57, 12, 9}, {84, 57, 7, 8}, {76, 29, 12, 8}, {88, 29, 12, 8}, {100, 29, 12, 8}, {112, 29, 12, 8}, {124, 29, 12, 8}, {0, 39, 12, 8},  {12, 39, 16, 8}, {136, 29, 12, 10}, {28, 39, 12, 8}, {40, 39, 12, 8}, {52, 39, 12, 8}, {64, 39, 12, 8}, {76, 39, 8, 8}, {84, 39, 8, 8}, {92, 39, 8, 8}, {100, 39, 8, 8}, {108, 39, 12, 8}, {0, 48, 12, 8}, {12, 48, 12, 8}, {24, 48, 12, 8}, {36, 48, 12, 8}, {48, 48, 12, 8}, {60, 48, 12, 8}, {72, 48, 12, 9}, {84, 48, 12, 8},  {96, 48, 12, 8},  {108, 48, 12, 8}, {120, 48, 12, 9}, {0, 57, 12, 8},   {12, 57, 12, 10}, {24, 57, 12, 10}, {36, 57, 12, 10}, {48, 57, 12, 8}, {60, 57, 16, 8},  {8, 8, 6, 0},    {2, 3, 4, 5},    {6, 7, 8, 8},     {3, 1, 0, 2},
};
static const uint16 text_spaces[3] = {8, 8, 6};

sint32 text_parse_signed_decimal(char *text)
{
    sint32 value = 0;
    sint32 negative = 0;
    uint8 character;

    FUNCTION_MARKER(0x800159D8u, "MAIN.EXE");
    character = (uint8)text[0];
    while (character == ' ' || character == '\n' || character == '\t')
    {
        ++text;
        character = (uint8)text[0];
    }
    if (character == '-')
    {
        negative = 1;
        character = (uint8) * ++text;
    }
    while ((uint32)(character - '0') < 10u)
    {
        value = math_add_wrap_s32(math_mul_lo_s32(value, 10), (sint32)character - '0');
        character = (uint8) * ++text;
    }
    if (negative)
        return (sint32)(0u - (uint32)value);
    return value;
}

sint32 text_find_char_index(char *value, char *values)
{
    sint32 index = 0;
    uint8 target;
    uint8 current;

    FUNCTION_MARKER(0x8001F438u, "MAIN.EXE");
    current = (uint8)values[0];
    if (current == 0u)
        return 0;
    target = (uint8)value[0];
    current = (uint8)values[0];
    for (;;)
    {
        if (current == target)
            return (sint16)index;
        ++values;
        current = (uint8)values[0];
        index = (sint16)(index + 1);
        if (current == 0u)
            return 0;
    }
}

sint32 text_format_decimal_digits(sint32 value, char *output, sint16 digits)
{
    sint32 number = (sint16)value;
    sint32 result;

    FUNCTION_MARKER(0x8001F484u, "MAIN.EXE");
    if (digits == 2)
    {
        if (number != 0)
        {
            output[0] = (uint8)(number / 10 + '0');
            result = number % 10 + '0';
            output[1u] = (uint8)result;
        }
        else
        {
            result = '0';
            output[0] = '0';
            output[1u] = '0';
        }
    }
    else
    {
        result = number != 0 ? number % 10 + '0' : '0';
        output[0] = (uint8)result;
    }
    return result;
}

TEXT_FONT text_fonts[TEXT_FONT_COUNT];
static size_t text_font_index;

void text_init_fonts(void)
{
    FUNCTION_MARKER(0x80050E98u, "MAIN.EXE");
    static const uint8 *alphabets[3] = {text_alphabet_0, text_alphabet_1, text_alphabet_2};
    static const TEXT_METRIC *metrics[3] = {text_metrics_0, text_metrics_1, text_metrics_2};
    static const size_t counts[3] = {sizeof(text_alphabet_0), sizeof(text_alphabet_1), sizeof(text_alphabet_2)};
    static const uint16 texture_x[3] = {960, 910, 922};
    static const uint16 texture_y[3] = {256, 256, 352};
    static const uint16 clut_y[3] = {450, 445, 444};
    static const uint8 adjustment[3] = {5, 2, 2};
    size_t group, row, column;
    memset(text_fonts, 0, sizeof(text_fonts));
    text_font_index = 0;
    for (group = 0; group < TEXT_FONT_COUNT; ++group)
    {
        TEXT_FONT *font = &text_fonts[group];
        font->count = counts[group];
        font->texture_x = texture_x[group];
        font->texture_y = texture_y[group];
        font->clut_y = clut_y[group];
        font->space = text_spaces[group];
        for (row = 0; row < 256; ++row)
            font->map[row] = -1;
        for (row = 0; row < font->count; ++row)
            font->map[alphabets[group][row]] = (sint16)row;
        for (row = 0; row < 26; ++row)
            font->map['A' + row] = font->map['a' + row];
        memcpy(font->metrics, metrics[group], font->count * sizeof(TEXT_METRIC));
        for (row = 0; row < font->count; ++row)
            for (column = 0; column < font->count; ++column)
                font->kerning[row * font->count + column] = (uint8)(font->metrics[row].width - adjustment[group]);
    }
}

void text_load_kerning(size_t group, const uint8 *data, size_t count)
{
    TEXT_FONT *font;
    if (group >= TEXT_FONT_COUNT)
        abort();
    font = &text_fonts[group];
    if (count != font->count || count > TEXT_FONT_CAPACITY)
        abort();
    memcpy(font->kerning, data, count * count);
}

static sint32 text_emit(uint8 character, sint16 x, sint16 y, sint8 adjustment)
{
    TEXT_FONT *font = &text_fonts[text_font_index];
    sint16 glyph = font->map[character];
    size_t buffer = (size_t)sprite_text_buffer();
    const TEXT_METRIC *metric;
    TEXT_GLYPH *output;
    sint32 height;
    if (glyph < 0 || (size_t)glyph >= font->count || font->pending_count[buffer] >= TEXT_GLYPH_CAPACITY)
        return -1;
    metric = &font->metrics[glyph];
    height = (sint16)metric->height + (adjustment > 0 ? -adjustment : adjustment);
    if (height < 0)
        return height;
    output = &font->pending[buffer][font->pending_count[buffer]++];
    output->x = x;
    output->y = (sint16)(y + (adjustment > 0 ? adjustment : 0));
    output->u = (uint8)(4 * (font->texture_x % 64) + metric->u);
    output->v = (uint8)(font->texture_y + metric->v + (adjustment > 0 ? adjustment : 0));
    output->width = metric->width;
    output->height = (uint16)height;
    return (sint32)font->pending_count[buffer];
}

sint32 text_emit_glyph(uint8 character, sint16 x, sint16 y)
{
    FUNCTION_MARKER(0x80051470u, "MAIN.EXE");
    return text_emit(character, x, y, 0);
}

static sint32 text_draw(char *text, sint32 x, sint16 y, sint8 adjustment, sint32 clipped)
{
    TEXT_FONT *font = &text_fonts[text_font_index];
    if (!text)
        return 0;
    while (*text)
    {
        uint8 current = (uint8)*text++;
        uint8 next = (uint8)*text;
        sint16 first = font->map[current];
        sint16 second = font->map[next];
        if (current == ' ' || current == '_')
            x = (sint32)((uint32)x + font->space);
        else if (first >= 0 && (size_t)first < font->count)
        {
            text_emit(current, (sint16)x, y, adjustment);
            if (next == 0)
                return 0;
            if (next == ' ' || next == '_')
                x = (sint32)((uint32)x + (clipped ? font->space : font->metrics[first].width));
            else if (second >= 0 && (size_t)second < font->count)
                x = (sint32)((uint32)x + font->kerning[(size_t)first * font->count + second]);
        }
    }
    return 0;
}

sint32 text_render_glyphs(char *text, sint32 x, sint16 y)
{
    FUNCTION_MARKER(0x80051664u, "MAIN.EXE");
    return text_draw(text, x, y, 0, 0);
}

sint32 text_emit_clipped(uint8 character, sint16 x, sint16 y, sint8 adjustment)
{
    FUNCTION_MARKER(0x8005183Cu, "MAIN.EXE");
    return text_emit(character, x, y, adjustment);
}

sint32 text_render_clipped(char *text, sint32 x, sint16 y, sint8 adjustment)
{
    FUNCTION_MARKER(0x80051A7Cu, "MAIN.EXE");
    return text_draw(text, x, y, adjustment, 1);
}

static void text_render_record(TEXT_RECORD *record)
{
    sint32 x = record->x;
    if (record->visible != 1 || record->font >= TEXT_FONT_COUNT || !record->text)
        return;
    text_font_index = record->font;
    if (record->centered == 1)
    {
        sint32 width = text_measure_width(record->text);
        x -= (width + (width < 0)) >> 1;
    }
    text_draw(record->text, (sint16)x, (sint16)record->y, record->style, record->style != 0);
}

sint32 text_render_recs(void)
{
    FUNCTION_MARKER(0x80051F48u, "MAIN.EXE");
    sint32 index;
    if (text_hud_count > TEXT_HUD_CAPACITY || text_menu_count > TEXT_MENU_CAPACITY)
        abort();
    for (index = text_hud_count; index-- > 0;)
        text_render_record(&text_hud[index]);
    for (index = 0; index < text_menu_count; ++index)
        text_render_record(&text_menu[index]);
    return 0;
}

void text_init_hud(const TEXT_HUD_STRINGS *strings)
{
    TEXT_RECORD *records;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80052144u, "MAIN.EXE");
    text_hud_count = 121u;
    records = text_hud;
    records[0].font = 0u;
    records[0].centered = 0u;
    records[0].visible = 0u;
    records[0].text = strings->status;
    records[0].x = 190u;
    records[0].y = 100u;
    records = text_hud;
    records[1].text = strings->time;
    records[1].x = 300u;
    records[1].font = 1u;
    records[1].centered = 0u;
    records[1].visible = 0u;
    records[1].y = 168u;
    records = text_hud;
    records[2].text = strings->paused;
    records[2].font = 1u;
    records[2].centered = 0u;
    records[2].visible = 0u;
    records[2].x = 256u;
    records[2].y = 100u;
    for (row = 0; row < 10; ++row)
    {
        TEXT_RECORD *record = &text_hud[row + 3];
        record[0].text = strings->names[row];
        record[0].font = 1u;
        record[0].centered = 0u;
        record[0].x = 124u;
        record[0].y = (uint16)(12 * row + 50);
        record[0].visible = 0u;
    }
    for (row = 0; row < 9; ++row)
    {
        for (column = 0; column < 10; ++column)
        {
            TEXT_RECORD *record = &text_hud[(10 * (row)) + column + 31];
            record[0].font = 1u;
            record[0].x = 124u;
            record[0].y = (uint16)(12 * column + 50);
            record[0].text = strings->boats[0][row];
            record[0].centered = 0u;
            record[0].visible = 0u;
        }
    }
    for (row = 0; row < 9; ++row)
    {
        TEXT_RECORD *record = &text_hud[row + 13];
        record[0].text = strings->boats[1][row];
        record[0].font = 1u;
        record[0].centered = 0u;
        record[0].x = 124u;
        record[0].y = 170u;
        record[0].visible = 0u;
    }
    for (row = 0; row < 9; ++row)
    {
        TEXT_RECORD *record = &text_hud[row + 22];
        record[0].text = strings->boats[2][row];
        record[0].font = 1u;
        record[0].centered = 0u;
        record[0].x = 124u;
        record[0].y = 170u;
        record[0].visible = 0u;
    }
}

void text_show_pause(char *message, uint8 font)
{
    TEXT_RECORD *record = &text_hud[2];
    FUNCTION_MARKER(0x80052418u, "MAIN.EXE");
    if (message)
        record->text = message;
    record->font = font;
    record->visible = 1;
    record->centered = 1;
    record->x = 256;
    record->y = 230;
}

uint32 text_set_menu_visible(sint32 index, sint8 value)
{
    TEXT_RECORD *records = text_menu;

    FUNCTION_MARKER(0x800524C8u, "MAIN.EXE");
    if (index < 0 || index >= TEXT_MENU_CAPACITY)
        abort();
    records[index].visible = (uint8)value;
    return 1;
}

uint32 text_set_hud_visible(sint32 index, sint8 value)
{
    TEXT_RECORD *records = text_hud;

    FUNCTION_MARKER(0x800524E4u, "MAIN.EXE");
    if (index < 0 || index >= TEXT_HUD_CAPACITY)
        abort();
    records[index].visible = (uint8)value;
    return 1;
}

sint32 text_clear_hud(void)
{
    sint16 count = (sint16)text_hud_count;
    TEXT_RECORD *records = text_hud;
    sint32 index;

    FUNCTION_MARKER(0x80052500u, "MAIN.EXE");
    if (count > TEXT_HUD_CAPACITY)
        abort();
    if (count <= 0)
        return count;
    for (index = 0; index < count; ++index)
    {
        records[index].visible = 0u;
        records[index].style = 0u;
    }
    return 0;
}

sint32 text_clear_names(void)
{
    sint32 index;

    FUNCTION_MARKER(0x80052568u, "MAIN.EXE");
    for (index = 0; index < 10; ++index)
    {
        TEXT_RECORD *records = text_hud;
        records[index + 3].visible = 0u;
        records = text_hud;
        records[index + 3].style = 0u;
    }
    return 0;
}

sint32 text_clear_boat_names(void)
{
    sint32 index;

    FUNCTION_MARKER(0x800525B4u, "MAIN.EXE");
    for (index = 0; index < 90; ++index)
    {
        TEXT_RECORD *records = text_hud;
        records[index + 31].visible = 0u;
        records = text_hud;
        records[index + 31].style = 0u;
    }
    return 0;
}

sint32 text_measure_width(char *text)
{
    FUNCTION_MARKER(0x80052600u, "MAIN.EXE");
    TEXT_FONT *font = &text_fonts[text_font_index];
    uint32 width = 0;
    if (!text)
        return 0;
    while (*text)
    {
        uint8 current = (uint8)*text++;
        uint8 next = (uint8)*text;
        sint16 first = font->map[current];
        sint16 second = font->map[next];
        if (current == ' ' || current == '_')
            width += font->space;
        else if (first >= 0 && (size_t)first < font->count)
        {
            if (!next || next == ' ' || next == '_')
                width += font->metrics[first].width;
            else if (second >= 0 && (size_t)second < font->count)
                width += font->kerning[(size_t)first * font->count + second];
        }
    }
    return (sint16)width;
}

TEXT_RECORD text_menu[TEXT_MENU_CAPACITY];
TEXT_RECORD text_hud[TEXT_HUD_CAPACITY];
sint16 text_menu_count;
sint16 text_hud_count;

void text_reset(void)
{
    memset(text_menu, 0, sizeof(text_menu));
    memset(text_hud, 0, sizeof(text_hud));
    text_menu_count = text_hud_count = 0;
}

void text_load_menu(const uint8 *descs, char *strings, size_t count)
{
    size_t index;
    if (count > TEXT_MENU_CAPACITY)
        abort();
    for (index = 0; index < count; ++index)
    {
        TEXT_RECORD *record = &text_menu[index];
        const uint8 *source = descs + index * 16u;
        record->text = strings;
        record->x = (uint16)(source[8] | ((uint16)source[9] << 8));
        record->y = (uint16)(source[10] | ((uint16)source[11] << 8));
        record->font = source[12];
        record->visible = source[13];
        record->centered = source[14];
        record->style = source[15];
        while ((uint8)*strings != 0u)
            ++strings;
        ++strings;
    }
    text_menu_count = (sint16)count;
}

char *text_bind(uint32 address)
{
    // Strings remain owned by legacy resource/profile storage until stages 4 and 5
    return address ? (char *)psx_addr(address, 1u) : NULL;
}

uint32 text_clear_menu_range(sint16 first, sint32 count)
{
    sint32 index;
    FUNCTION_MARKER(0x80050524u, "MAIN.EXE");
    if (count <= 0)
        return 0;
    if (first < 0 || first >= TEXT_MENU_CAPACITY || count > TEXT_MENU_CAPACITY - first)
        abort();
    for (index = count; index-- > 0;)
        text_menu[first + index].visible = 0;
    return 1;
}
