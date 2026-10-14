#include "hud.h"
#include <string.h>
#include "sprite.h"
#include <stdlib.h>
#include "text.h"
#include "utils.h"
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

sint32 text_parse_int(char *text)
{
    sint32 val = 0;
    sint32 negative = 0;
    uint8 character;

    FUNCTION_MARKER(0x800159D8, "MAIN.EXE");
    character = text[0];
    while (character == ' ' || character == '\n' || character == '\t')
    {
        ++text;
        character = text[0];
    }
    if (character == '-')
    {
        negative = 1;
        character = *++text;
    }
    while ((uint32)(character - '0') < 10)
    {
        val = mul32(val, 10) + (character - '0');
        character = *++text;
    }
    if (negative)
        return -val;
    return val;
}

sint16 text_find_char_idx(const char *val, const char *vals)
{
    sint16 idx = 0;
    uint8 target;
    uint8 cur;

    FUNCTION_MARKER(0x8001F438, "MAIN.EXE");
    cur = vals[0];
    if (cur == 0)
        return 0;
    target = val[0];
    cur = vals[0];
    for (;;)
    {
        if (cur == target)
            return idx;
        ++vals;
        cur = vals[0];
        idx = idx + 1;
        if (cur == 0)
            return 0;
    }
}

sint32 text_format_digits(sint32 val, char *output, sint16 digits)
{
    sint16 number = val;
    sint32 result;

    FUNCTION_MARKER(0x8001F484, "MAIN.EXE");
    result = number % 10 + '0';
    if (digits == 2)
    {
        output[0] = number / 10 + '0';
        output[1] = result;
    }
    else
        output[0] = result;
    return result;
}

TEXT_FONT text_fonts[TEXT_FONT_COUNT];
static uint32 text_font_idx;

static const uint8 *const text_alphabets[TEXT_FONT_COUNT] = {text_alphabet_0, text_alphabet_1, text_alphabet_2};
static const TEXT_METRIC *const text_metrics[TEXT_FONT_COUNT] = {text_metrics_0, text_metrics_1, text_metrics_2};
static const uint32 text_counts[TEXT_FONT_COUNT] = {sizeof(text_alphabet_0), sizeof(text_alphabet_1), sizeof(text_alphabet_2)};

uint16 text_char_width(uint32 font_idx, uint8 ch)
{
    uint32 idx = 0;

    if (font_idx >= TEXT_FONT_COUNT)
        abort();
    while (idx + 1 < text_counts[font_idx] && text_alphabets[font_idx][idx] != ch)
        ++idx;
    return text_metrics[font_idx][idx].width;
}

void text_init_fonts(void)
{
    FUNCTION_MARKER(0x80050E98, "MAIN.EXE");
    static const uint16 tex_x[3] = {960, 910, 922};
    static const uint16 tex_y[3] = {256, 256, 352};
    static const uint16 clut_y[3] = {450, 445, 444};
    static const uint8 adjust[3] = {5, 2, 2};
    uint32 grp, row, column;
    memset(text_fonts, 0, sizeof(text_fonts));
    text_font_idx = 0;
    for (grp = 0; grp < TEXT_FONT_COUNT; ++grp)
    {
        TEXT_FONT *font = &text_fonts[grp];
        font->count = text_counts[grp];
        font->tex_x = tex_x[grp];
        font->tex_y = tex_y[grp];
        font->clut_y = clut_y[grp];
        font->space = text_spaces[grp];
        for (row = 0; row < 256; ++row)
            font->map[row] = -1;
        for (row = 0; row < font->count; ++row)
            font->map[text_alphabets[grp][row]] = row;
        for (row = 0; row < 26; ++row)
            font->map['A' + row] = font->map['a' + row];
        memcpy(font->metrics, text_metrics[grp], font->count * sizeof(TEXT_METRIC));
        for (row = 0; row < font->count; ++row)
            for (column = 0; column < font->count; ++column)
                font->kerning[row * font->count + column] = font->metrics[row].width - adjust[grp];
    }
}

void text_load_kerning(uint32 grp, const uint8 *data, uint32 count)
{
    TEXT_FONT *font;
    if (grp >= TEXT_FONT_COUNT)
        abort();
    font = &text_fonts[grp];
    if (count != font->count || count > TEXT_FONT_CAPACITY)
        abort();
    memcpy(font->kerning, data, count * count);
}

static sint32 text_emit(uint8 character, sint16 x, sint16 y, sint8 adjust)
{
    TEXT_FONT *font = &text_fonts[text_font_idx];
    sint16 glyph = font->map[character];
    uint32 buffer = spr_text_buffer();
    const TEXT_METRIC *metric;
    TEXT_GLYPH *output;
    sint32 height;
    if (glyph < 0 || (uint32)glyph >= font->count || font->pending_count[buffer] >= TEXT_GLYPH_CAPACITY)
        return -1;
    metric = &font->metrics[glyph];
    height = (sint16)metric->height + (adjust > 0 ? -adjust : adjust);
    if (height < 0)
        return height;
    output = &font->pending[buffer][font->pending_count[buffer]++];
    output->x = x;
    output->y = y + (adjust > 0 ? adjust : 0);
    output->u = 4 * (font->tex_x % 64) + metric->u;
    output->v = font->tex_y + metric->v + (adjust > 0 ? adjust : 0);
    output->width = metric->width;
    output->height = height;
    return font->pending_count[buffer];
}

static sint32 text_emit_glyph(uint8 character, sint16 x, sint16 y)
{
    FUNCTION_MARKER(0x80051470, "MAIN.EXE");
    return text_emit(character, x, y, 0);
}

static sint32 text_draw(char *text, sint32 x, sint16 y, sint8 adjust, sint32 clipped)
{
    TEXT_FONT *font = &text_fonts[text_font_idx];
    if (!text)
        return 0;
    while (*text)
    {
        uint8 cur = *text++;
        uint8 next = *text;
        sint16 glyph = font->map[cur];
        sint16 next_glyph = font->map[next];
        if (cur == ' ' || cur == '_')
            x = x + font->space;
        else if (glyph >= 0 && (uint32)glyph < font->count)
        {
            text_emit(cur, x, y, adjust);
            if (next == 0)
                return 0;
            if (next == ' ' || next == '_')
                x = x + (clipped ? font->space : font->metrics[glyph].width);
            else if (next_glyph >= 0 && (uint32)next_glyph < font->count)
                x = x + font->kerning[glyph * font->count + next_glyph];
        }
    }
    return 0;
}

static sint32 text_render_glyphs(char *text, sint32 x, sint16 y)
{
    FUNCTION_MARKER(0x80051664, "MAIN.EXE");
    return text_draw(text, x, y, 0, 0);
}

static sint32 text_emit_clipped(uint8 character, sint16 x, sint16 y, sint8 adjust)
{
    FUNCTION_MARKER(0x8005183C, "MAIN.EXE");
    return text_emit(character, x, y, adjust);
}

static sint32 text_render_clipped(char *text, sint32 x, sint16 y, sint8 adjust)
{
    FUNCTION_MARKER(0x80051A7C, "MAIN.EXE");
    return text_draw(text, x, y, adjust, 1);
}

static sint32 text_measure_width(char *text)
{
    FUNCTION_MARKER(0x80052600, "MAIN.EXE");
    TEXT_FONT *font = &text_fonts[text_font_idx];
    uint32 width = 0;
    if (!text)
        return 0;
    while (*text)
    {
        uint8 cur = *text++;
        uint8 next = *text;
        sint16 glyph = font->map[cur];
        sint16 next_glyph = font->map[next];
        if (cur == ' ' || cur == '_')
            width += font->space;
        else if (glyph >= 0 && (uint32)glyph < font->count)
        {
            if (!next || next == ' ' || next == '_')
                width += font->metrics[glyph].width;
            else if (next_glyph >= 0 && (uint32)next_glyph < font->count)
                width += font->kerning[glyph * font->count + next_glyph];
        }
    }
    return (sint16)width;
}

static void text_render_rec(TEXT_RECORD *rec)
{
    sint32 x = rec->x;
    if (rec->visible != 1 || rec->font >= TEXT_FONT_COUNT || !rec->text)
        return;
    text_font_idx = rec->font;
    if (rec->centered == 1)
    {
        sint32 width = text_measure_width(rec->text);
        x -= width + (width < 0) >> 1;
    }
    text_draw(rec->text, (sint16)x, rec->y, rec->style, rec->style != 0);
}

sint32 text_render_recs(void)
{
    FUNCTION_MARKER(0x80051F48, "MAIN.EXE");
    sint32 idx;
    if (text_hud_count > TEXT_HUD_CAPACITY || text_menu_count > TEXT_MENU_CAPACITY)
        abort();
    for (idx = text_hud_count; idx-- > 0;)
        text_render_rec(&text_hud[idx]);
    for (idx = 0; idx < text_menu_count; ++idx)
        text_render_rec(&text_menu[idx]);
    return 0;
}

// HUD labels use writable storage for the existing text-record interface
static char text_hud_status[] = "loading";
static char text_hud_time[] = "beta 008";
static char text_hud_paused[] = "Sep 22 1997";
static char text_hud_names[10][8] = {
    "aaaa", "aaaa", "aaaa", "aaaa", "aaaa", "aaaa", "aaaa", "aaaa", "aaaa", "aaaa",
};
static char text_boats[3][9][14] = {
    {
        "WATERHAWK",
        "WAVEHAMMER",
        "AKULA",
        "AMERICANA",
        "MARLIN",
        "T550",
        "RADIOACTIVE",
        "BLACK WIDOW",
        "HURRICANE",
    },
    {
        "WATERHAWK A",
        "WAVEHAMMER A",
        "AKULA A",
        "AMERICANA A",
        "MARLIN A",
        "T550 A",
        "RADIOACTIVE A",
        "BLACK WIDOW A",
        "HURRICANE A",
    },
    {
        "WATERHAWK A",
        "WAVEHAMMER A",
        "AKULA A",
        "AMERICANA A",
        "MARLIN A",
        "T550 A",
        "RADIOACTIVE A",
        "BLACK WIDOW A",
        "HURRICANE A",
    },
};
static char text_pause_msgs[5][45] = {
    "game paused. controller removed", "jeu stopp\351 car manette retir\351e", "spiel unterbrochen. controller entfernt", "gioco in pausa. controller eliminato", "juego detenido porque se ha quitado el mando",
};

const char *text_boat_name(uint32 idx)
{
    if (idx >= 9)
        abort();
    return text_boats[0][idx];
}

void text_init_hud(void)
{
    TEXT_RECORD *recs;
    sint32 row;
    sint32 column;

    FUNCTION_MARKER(0x80052144, "MAIN.EXE");
    text_hud_count = 121;
    recs = text_hud;
    recs[0].font = 0;
    recs[0].centered = 0;
    recs[0].visible = 0;
    recs[0].text = text_hud_status;
    recs[0].x = 190;
    recs[0].y = 100;
    recs = text_hud;
    recs[1].text = text_hud_time;
    recs[1].x = 300;
    recs[1].font = 1;
    recs[1].centered = 0;
    recs[1].visible = 0;
    recs[1].y = 168;
    recs = text_hud;
    recs[2].text = text_hud_paused;
    recs[2].font = 1;
    recs[2].centered = 0;
    recs[2].visible = 0;
    recs[2].x = 256;
    recs[2].y = 100;
    for (row = 0; row < 10; ++row)
    {
        TEXT_RECORD *rec = &text_hud[row + 3];
        rec[0].text = text_hud_names[row];
        rec[0].font = 1;
        rec[0].centered = 0;
        rec[0].x = 124;
        rec[0].y = 12 * row + 50;
        rec[0].visible = 0;
    }
    for (row = 0; row < 9; ++row)
    {
        for (column = 0; column < 10; ++column)
        {
            TEXT_RECORD *rec = &text_hud[10 * row + column + 31];
            rec[0].font = 1;
            rec[0].x = 124;
            rec[0].y = 12 * column + 50;
            rec[0].text = text_boats[0][row];
            rec[0].centered = 0;
            rec[0].visible = 0;
        }
    }
    for (row = 0; row < 9; ++row)
    {
        TEXT_RECORD *rec = &text_hud[row + 13];
        rec[0].text = text_boats[1][row];
        rec[0].font = 1;
        rec[0].centered = 0;
        rec[0].x = 124;
        rec[0].y = 170;
        rec[0].visible = 0;
    }
    for (row = 0; row < 9; ++row)
    {
        TEXT_RECORD *rec = &text_hud[row + 22];
        rec[0].text = text_boats[2][row];
        rec[0].font = 1;
        rec[0].centered = 0;
        rec[0].x = 124;
        rec[0].y = 170;
        rec[0].visible = 0;
    }
}

void text_show_pause(uint8 language)
{
    char *message = language < 5u ? text_pause_msgs[language] : NULL;
    uint8 font = language < 5u ? language == 0 ? 1u : 2u : 0u;
    TEXT_RECORD *rec = &text_hud[2];
    FUNCTION_MARKER(0x80052418, "MAIN.EXE");
    if (message)
        rec->text = message;
    rec->font = font;
    rec->visible = 1;
    rec->centered = 1;
    rec->x = 256;
    rec->y = 230;
}

uint32 text_set_menu_visible(sint32 idx, sint8 val)
{
    TEXT_RECORD *recs = text_menu;

    FUNCTION_MARKER(0x800524C8, "MAIN.EXE");
    if (idx < 0 || idx >= TEXT_MENU_CAPACITY)
        abort();
    recs[idx].visible = val;
    return 1;
}

uint32 text_set_hud_visible(sint32 idx, sint8 val)
{
    TEXT_RECORD *recs = text_hud;

    FUNCTION_MARKER(0x800524E4, "MAIN.EXE");
    if (idx < 0 || idx >= TEXT_HUD_CAPACITY)
        abort();
    recs[idx].visible = val;
    return 1;
}

sint32 text_clear_hud(void)
{
    sint16 count = text_hud_count;
    TEXT_RECORD *recs = text_hud;
    sint32 idx;

    FUNCTION_MARKER(0x80052500, "MAIN.EXE");
    if (count > TEXT_HUD_CAPACITY)
        abort();
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx)
    {
        recs[idx].visible = 0;
        recs[idx].style = 0;
    }
    return 0;
}

sint32 text_clear_names(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x80052568, "MAIN.EXE");
    for (idx = 0; idx < 10; ++idx)
    {
        TEXT_RECORD *recs = text_hud;
        recs[idx + 3].visible = 0;
        recs = text_hud;
        recs[idx + 3].style = 0;
    }
    return 0;
}

sint32 text_clear_boat_names(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x800525B4, "MAIN.EXE");
    for (idx = 0; idx < 90; ++idx)
    {
        TEXT_RECORD *recs = text_hud;
        recs[idx + 31].visible = 0;
        recs = text_hud;
        recs[idx + 31].style = 0;
    }
    return 0;
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

void text_load_menu(const uint8 *descs, char *strings, uint32 count)
{
    uint32 idx;
    if (count > TEXT_MENU_CAPACITY)
        abort();
    for (idx = 0; idx < count; ++idx)
    {
        TEXT_RECORD *rec = &text_menu[idx];
        const uint8 *src = descs + idx * 16;
        rec->text = strings;
        rec->x = xport_load_le16(src + 8u);
        rec->y = xport_load_le16(src + 10u);
        rec->font = src[12];
        rec->visible = src[13];
        rec->centered = src[14];
        rec->style = src[15];
        while (*strings != 0)
            ++strings;
        ++strings;
    }
    text_menu_count = count;
}

uint32 text_clear_menu_range(sint16 start, sint32 count)
{
    sint32 idx;
    FUNCTION_MARKER(0x80050524, "MAIN.EXE");
    if (count <= 0)
        return 0;
    if (start < 0 || start >= TEXT_MENU_CAPACITY || count > TEXT_MENU_CAPACITY - start)
        abort();
    for (idx = count; idx-- > 0;)
        text_menu[start + idx].visible = 0;
    return 1;
}

sint32 text_parse_digits(const uint8 *text, sint32 len)
{
    sint32 result = 0;
    sint16 idx = 0;
    sint16 remaining = len;

    while (--remaining != -1)
    {
        sint32 digit = text[idx] - '0';

        if (remaining == 2)
            result += 100 * digit;
        else if (remaining == 1)
            result += 10 * digit;
        else if (remaining == 0)
            result += digit;
        ++idx;
    }
    return result;
}
