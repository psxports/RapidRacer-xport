#ifndef RR_TEXT_H
#define RR_TEXT_H

#include "psx.h"
#include <stddef.h>

typedef struct
{
    char *text;
    uint16 x;
    uint16 y;
    uint8 font;
    uint8 visible;
    uint8 centered;
    sint8 style;
} TEXT_RECORD;

enum
{
    TEXT_MENU_CAPACITY = 110,
    TEXT_HUD_CAPACITY = 140
};

extern TEXT_RECORD text_menu[TEXT_MENU_CAPACITY];
extern TEXT_RECORD text_hud[TEXT_HUD_CAPACITY];
extern sint16 text_menu_count;
extern sint16 text_hud_count;
char *text_bind(uint32 address);
void text_reset(void);
void text_load_menu(const uint8 *descs, char *strings, size_t count);

enum
{
    TEXT_FONT_COUNT = 3,
    TEXT_GLYPH_CAPACITY = 200,
    TEXT_FONT_CAPACITY = 128
};

typedef struct
{
    uint8 u, v;
    uint16 width, height;
} TEXT_METRIC;

typedef struct
{
    sint16 x, y;
    uint8 u, v;
    uint16 width, height;
} TEXT_GLYPH;

typedef struct
{
    sint16 map[256];
    TEXT_METRIC metrics[TEXT_FONT_CAPACITY];
    uint8 kerning[TEXT_FONT_CAPACITY * TEXT_FONT_CAPACITY];
    size_t count;
    uint16 texture_x, texture_y, clut_y, space;
    TEXT_GLYPH pending[2][TEXT_GLYPH_CAPACITY];
    size_t pending_count[2];
} TEXT_FONT;

extern TEXT_FONT text_fonts[TEXT_FONT_COUNT];
void text_init_fonts(void);
void text_load_kerning(size_t font, const uint8 *data, size_t count);

sint32 text_parse_signed_decimal(char *text);

sint32 text_find_char_index(char *value, char *values);
sint32 text_format_decimal_digits(sint32 value, char *output, sint16 digits);

sint32 text_emit_glyph(uint8 character, sint16 x, sint16 y);
sint32 text_render_glyphs(char *text, sint32 x, sint16 y);
sint32 text_emit_clipped(uint8 character, sint16 x, sint16 y, sint8 adjustment);
sint32 text_render_clipped(char *text, sint32 x, sint16 y, sint8 adjustment);
sint32 text_render_recs(void);

typedef struct
{
    char *status;
    char *time;
    char *paused;
    char *names[10];
    char *boats[3][9];
} TEXT_HUD_STRINGS;

void text_init_hud(const TEXT_HUD_STRINGS *strings);
void text_show_pause(char *message, uint8 font);
uint32 text_set_menu_visible(sint32 index, sint8 value);
uint32 text_set_hud_visible(sint32 index, sint8 value);
sint32 text_clear_hud(void);
sint32 text_clear_names(void);
sint32 text_clear_boat_names(void);
sint32 text_measure_width(char *text);

uint32 text_clear_menu_range(sint16 first, sint32 count);

#endif /* RR_TEXT_H */
