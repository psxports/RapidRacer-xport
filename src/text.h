#ifndef TEXT_H
#define TEXT_H

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
void text_reset(void);
void text_load_menu(const uint8 *descs, char *strings, uint32 count);

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
    uint32 count;
    uint16 tex_x, tex_y, clut_y, space;
    TEXT_GLYPH pending[2][TEXT_GLYPH_CAPACITY];
    uint32 pending_count[2];
} TEXT_FONT;

extern TEXT_FONT text_fonts[TEXT_FONT_COUNT];
void text_init_fonts(void);
uint16 text_char_width(uint32 font_idx, uint8 ch);
void text_load_kerning(uint32 font, const uint8 *data, uint32 count);

sint32 text_parse_int(char *text);

sint16 text_find_char_idx(const char *val, const char *vals);
sint32 text_format_digits(sint32 val, char *output, sint16 digits);

sint32 text_render_recs(void);

const char *text_boat_name(uint32 idx);
void text_init_hud(void);
void text_show_pause(uint8 language);
uint32 text_set_menu_visible(sint32 idx, sint8 val);
uint32 text_set_hud_visible(sint32 idx, sint8 val);
sint32 text_clear_hud(void);
sint32 text_clear_names(void);
sint32 text_clear_boat_names(void);

uint32 text_clear_menu_range(sint16 start, sint32 count);

sint32 text_parse_digits(const uint8 *text, sint32 len);
#endif /* TEXT_H */
