#include "text.h"
#include "sprite.h"
#include "psx_gpu.h"
#include "arena.h"
#include "display.h"
#include "global.h"
#include "menu.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Clipped sprite state from MAIN.EXE 800B6A48/800B40B0
typedef struct
{
    uint32 u, v;
    uint32 offset_x, offset_y;
    uint32 width, height;
    POLY_FT4 quad;
} SPRITE_CLIP;

enum
{
    SPRITE_CLIP_CAPACITY = 5
};

static SPRITE_CLIP sprite_clips[SPRITE_CLIP_CAPACITY];
static uint16 sprite_clip_count;

// Sprite packet owners keep stable addresses for the shared GPU registry
static SPRITE_RENDER sprite_label_packets[10];
static SPRITE_RENDER sprite_image_packets[160];
static SPRITE_RENDER sprite_notice_packets[10];
SPRITE_RENDER sprite_glyphs[52];
uint16 sprite_buffers_active;

// Menu text packets from MAIN.EXE 800EA690/800EC5D0/800EE518 and 800D71F0
static SPRT sprite_text_packets[TEXT_FONT_COUNT][2][TEXT_GLYPH_CAPACITY];
static DR_TPAGE sprite_text_modes[TEXT_FONT_COUNT][2];
// Notice background owner from MAIN.EXE 800D6BC8
static TILE sprite_notice_tiles[2];

void sprite_register_packets(void)
{
    if (!gpu_register_packet_range(sprite_label_packets, sizeof(sprite_label_packets)) || !gpu_register_packet_range(sprite_image_packets, sizeof(sprite_image_packets)) || !gpu_register_packet_range(sprite_notice_packets, sizeof(sprite_notice_packets)) || !gpu_register_packet_range(sprite_glyphs, sizeof(sprite_glyphs)) || !gpu_register_packet_range(sprite_text_packets, sizeof(sprite_text_packets)) || !gpu_register_packet_range(sprite_text_modes, sizeof(sprite_text_modes)) || !gpu_register_packet_range(sprite_notice_tiles, sizeof(sprite_notice_tiles)))
        abort();
}

SPRITE_RENDER *sprite_render_at(SPRITE_RENDER *base, sint32 index)
{
    size_t count;
    if (base == sprite_label_packets)
        count = 10;
    else if (base == sprite_image_packets)
        count = 160;
    else if (base == sprite_notice_packets)
        count = 10;
    else if (base == sprite_glyphs)
        count = 52;
    else
        abort();
    if (index < 0 || (size_t)index >= count)
        abort();
    return base + index;
}

// UI array from MAIN.EXE 800B6A74
UI_RECORD sprite_records[180];

// Label descriptors from MAIN.EXE 800957C4
SPRITE_IMAGE sprite_labels[8] = {{.path = "BACK\\F01.TIM"}, {.path = "BACK\\F02.TIM"}, {.path = "BACK\\F03.TIM"}, {.path = "BACK\\F04.TIM"}, {.path = "BACK\\F05.TIM"}, {.path = "BACK\\F06.TIM"}, {.path = "BACK\\F07.TIM"}, {.path = "BACK\\F08.TIM"}};

// Working descriptors from MAIN.EXE 800E35C0/800E5B48
SPRITE_IMAGE sprite_images[160];
SPRITE_IMAGE sprite_notices[10];

static uint16 sprite_image_u16(const uint8 *source)
{
    return (uint16)(source[0] | ((uint16)source[1] << 8));
}

void sprite_decode_image(SPRITE_IMAGE *image, const uint8 *source)
{
    memcpy(image->path, source, 60);
    image->path[60] = 0;
    image->mode = source[30];
    image->reserved = source[31];
    image->u = sprite_image_u16(source + 32);
    image->v = sprite_image_u16(source + 34);
    image->width = sprite_image_u16(source + 36);
    image->height = sprite_image_u16(source + 38);
    image->tpage_x = sprite_image_u16(source + 40);
    image->tpage_y = sprite_image_u16(source + 42);
    image->clut_x = sprite_image_u16(source + 44);
    image->clut_y = sprite_image_u16(source + 46);
    image->x = sprite_image_u16(source + 48);
    image->y = sprite_image_u16(source + 50);
    image->tpage = sprite_image_u16(source + 52);
    image->clut = sprite_image_u16(source + 54);
}

void sprite_encode_image(const SPRITE_IMAGE *image, uint8 *target)
{
    memcpy(target, image->path, 30);
    target[30] = (uint8)(image->mode);
    target[31] = (uint8)(image->reserved);
    target[32] = (uint8)(image->u);
    target[33] = (uint8)(image->u >> 8);
    target[34] = (uint8)(image->v);
    target[35] = (uint8)(image->v >> 8);
    target[36] = (uint8)(image->width);
    target[37] = (uint8)(image->width >> 8);
    target[38] = (uint8)(image->height);
    target[39] = (uint8)(image->height >> 8);
    target[40] = (uint8)(image->tpage_x);
    target[41] = (uint8)(image->tpage_x >> 8);
    target[42] = (uint8)(image->tpage_y);
    target[43] = (uint8)(image->tpage_y >> 8);
    target[44] = (uint8)(image->clut_x);
    target[45] = (uint8)(image->clut_x >> 8);
    target[46] = (uint8)(image->clut_y);
    target[47] = (uint8)(image->clut_y >> 8);
    target[48] = (uint8)(image->x);
    target[49] = (uint8)(image->x >> 8);
    target[50] = (uint8)(image->y);
    target[51] = (uint8)(image->y >> 8);
    target[52] = (uint8)(image->tpage);
    target[53] = (uint8)(image->tpage >> 8);
    target[54] = (uint8)(image->clut);
    target[55] = (uint8)(image->clut >> 8);
    // Preserve the unused disk tail without interpreting it as a runtime pointer
    memcpy(target + 56, image->path + 56, 4);
}

void sprite_refresh_image_path(SPRITE_IMAGE *image)
{
    uint8 packed[60];
    sprite_encode_image(image, packed);
    memcpy(image->path, packed, 60);
    image->path[60] = 0;
}

void sprite_set_image_path(SPRITE_IMAGE *image, const char *path)
{
    uint8 packed[60];
    size_t length = strlen(path);
    if (length >= sizeof(packed))
        abort();
    // Long names overlay metadata in the compatible file format
    sprite_encode_image(image, packed);
    memcpy(packed, path, length + 1);
    sprite_decode_image(image, packed);
}

// Pixel scale from MAIN.EXE 80081008/81810 and page width from 80081816
static const struct
{
    sint16 scale;
    sint16 page_width;
} sprite_pixel_modes[4] = {{4, 256}, {2, 128}, {1, 64}, {256, 0}};

sint32 sprite_pixel_scale(uint8 mode)
{
    if (mode >= 4u)
        abort();
    return sprite_pixel_modes[mode].scale;
}

static const SPRITE_IMAGE *sprite_image_source(sint16 kind, sint16 index)
{
    static const struct
    {
        const SPRITE_IMAGE *images;
        size_t count;
    } groups[3] = {{sprite_labels, 8}, {sprite_images, 160}, {sprite_notices, 10}};

    if (kind < 0 || kind >= 3 || index < 0 || (size_t)index >= groups[kind].count)
        abort();
    return &groups[kind].images[index];
}

sint32 sprite_flush_pending(sint32 ordering_index)
{
    sint32 selector = (sint16)display_state.buffer;
    LINE_F2 *packet;
    uint32 *table;

    FUNCTION_MARKER(0x800455C8u, "MAIN.EXE");
    if (selector < 0 || selector >= 2 || ordering_index < 0 || ordering_index >= 40 || display_state.line_count > 200u)
        abort();
    table = display_order_slot(selector, ordering_index);
    if (display_state.line_count != 0u)
    {
        packet = display_line_buffer(selector);
        do
        {
            AddPrim(table, packet++);
            --display_state.line_count;
        } while (display_state.line_count != 0u);
    }
    return ordering_index;
}

typedef struct
{
    uint16 phase;
    uint16 ticks;
    uint16 frames[4];
} SPRITE_LABEL_ANIMATION;

// Label animation from MAIN.EXE 800B40B6/40B8/F0478
static SPRITE_LABEL_ANIMATION sprite_label_anim;

// Label positions from MAIN.EXE 800967A4/967AC
static const sint16 sprite_label_positions[4][2] = {{0, 0}, {256, 0}, {0, 120}, {256, 120}};

sint32 sprite_reset_labels(void)
{
    FUNCTION_MARKER(0x80046654u, "MAIN.EXE");
    sprite_label_anim.phase = 0u;
    sprite_label_anim.ticks = 0u;
    return sprite_animate_labels();
}

sint32 sprite_animate_labels(void)
{
    sint32 phase = (uint16)sprite_label_anim.phase;
    UI_RECORD *base = sprite_records;
    sint32 index;

    FUNCTION_MARKER(0x8004667Cu, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        ++phase;
        sprite_label_anim.frames[index] = (uint16)phase;
        if (phase == 8)
            phase = 0;
    }
    sprite_label_anim.ticks = (uint16)(sprite_label_anim.ticks + 1u);
    if ((sint16)menu_asset_counts.labels == 8)
    {
        for (index = 0; index < 8; ++index)
            base[index + 160].type = 0u;
        for (index = 0; index < 4; ++index)
        {
            sint16 source_index = (sint16)sprite_label_anim.frames[index] - 1;
            render_dispatch_rec(sprite_render_at(sprite_label_packets, index), source_index, 0);
            sprite_init_anim_rec((sint16)(index + 160), sprite_label_packets, 1, (sint16)index, sprite_label_positions[index][0], sprite_label_positions[index][1], 1);
        }
    }
    if ((sint16)sprite_label_anim.ticks == 6)
    {
        sprite_label_anim.ticks = 0u;
        phase = (sint16)sprite_label_anim.phase + 1;
        if (phase == 8)
            phase = 0;
        sprite_label_anim.phase = (uint16)phase;
        return phase;
    }
    return 6;
}

sint32 render_alloc_rec_bufs(void)
{
    FUNCTION_MARKER(0x8004684Cu, "MAIN.EXE");
    sprite_register_packets();
    game_push_checkpoint();
    sprite_buffers_active = 1u;
    display_state.line_count = 0u;
    return 1;
}

sint32 render_release_rec_bufs(void)
{
    sint32 enabled;

    FUNCTION_MARKER(0x800468ACu, "MAIN.EXE");
    enabled = (sint16)sprite_buffers_active;
    if (enabled != 0)
        return game_pop_checkpoint();
    return enabled;
}

sint32 menu_build_sprite_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    uint32 index;
    sint32 divisor;
    sint32 limit;
    sint32 visible_width;
    sint32 overflow_width;
    sint32 source_height;
    sint32 right_edge;
    SPRITE_CLIP *record;
    uint16 selector;

    FUNCTION_MARKER(0x800468D4u, "MAIN.EXE");
    output->kind = 0u;
    index = source->mode;
    divisor = sprite_pixel_scale(source->mode);
    limit = sprite_pixel_modes[index].page_width;
    visible_width = (sint32)((uint32)width - 1u);
    right_edge = divisor != 0 ? (sint32)((uint32)x + (uint32)(sint32)(sint16)(width / divisor)) : 0;
    if (divisor != 0 && right_edge >= limit)
    {
        sint32 clipped_width = (sint32)((uint32)limit - (uint32)x);

        overflow_width = (sint32)((uint32)visible_width - (uint32)clipped_width);
        visible_width = clipped_width;
        selector = sprite_clip_count;
        if (selector >= SPRITE_CLIP_CAPACITY)
            abort();
        record = &sprite_clips[selector];
        record->quad.tag = (record->quad.tag & 0x00FFFFFFu) | 0x09000000u;
        record->quad.code = 46u;
        record->quad.r0 = 0x80u;
        record->quad.g0 = 0x80u;
        record->quad.b0 = 0x80u;
        record->quad.tpage = (uint16)getTPage((sint32)index, 0, (sint16)source->tpage_x + visible_width, (sint16)source->tpage_y);
        record->quad.clut = (uint16)getClut((sint16)source->clut_x, (sint16)source->clut_y);
        record->offset_y = 0u;
        record->offset_x = 0u;
        record->u = 0u;
        record->v = (uint32)y;
        record->width = (uint32)(sint16)overflow_width;
        record->height = (uint32)height;
        selector = sprite_clip_count;
        sprite_clip_count = (uint16)(selector + 1u);
        output->clip_index = selector;
        record->quad.u0 = 0u;
        record->quad.v0 = (uint8)y;
        record->quad.u1 = 0u;
        record->quad.v1 = (uint8)((uint32)y + (uint32)height);
        record->quad.u2 = (uint8)overflow_width;
        record->quad.v2 = (uint8)y;
        record->quad.u3 = (uint8)overflow_width;
        record->quad.v3 = (uint8)((uint32)y + (uint32)height);
    }
    else
    {
        output->clip_index = 0u;
    }

    output->quad.tag = (output->quad.tag & 0x00FFFFFFu) | ((uint32)(9u) << 24);
    output->quad.code = 46u;
    output->quad.r0 = 0x80u;
    output->quad.g0 = 0x80u;
    output->quad.b0 = 0x80u;
    output->quad.tpage = source->tpage;
    output->quad.clut = source->clut;
    output->clut_x = (uint32)(sint16)source->clut_x;
    output->clut_y = (uint32)(sint16)source->clut_y;
    output->tpage_x = (uint32)(sint16)source->tpage_x;
    source_height = (sint16)source->tpage_y;
    output->offset_y = 0u;
    output->offset_x = 0u;
    output->u = (uint32)x;
    output->v = (uint32)y;
    output->width = (uint32)visible_width;
    output->height = (uint32)height;
    output->tpage_y = (uint32)source_height;
    output->quad.u0 = (uint8)x;
    output->quad.v0 = (uint8)y;
    output->quad.u1 = (uint8)x;
    output->quad.v1 = (uint8)((uint32)y + (uint32)height);
    output->quad.u2 = (uint8)((uint32)x + (uint32)visible_width);
    output->quad.v2 = (uint8)y;
    output->quad.u3 = (uint8)((uint32)x + (uint32)visible_width);
    output->quad.v3 = (uint8)((uint32)y + (uint32)height);
    output->last_orientation = 0u;
    output->orientation = 0u;
    output->mode = (uint8)index;
    return (sint32)((uint32)x + (uint32)visible_width);
}

sint32 sprite_build_draw_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    sint32 source_height;

    FUNCTION_MARKER(0x80046B80u, "MAIN.EXE");
    output->kind = 1u;
    output->simple.tag = (output->simple.tag & 0x00FFFFFFu) | ((uint32)(4u) << 24);
    output->simple.code = 100u;
    output->simple.r0 = 0x80u;
    output->simple.g0 = 0x80u;
    output->simple.b0 = 0x80u;
    output->simple.clut = source->clut;
    output->clut_x = (uint32)(sint16)source->clut_x;
    output->clut_y = (uint32)(sint16)source->clut_y;
    output->tpage_x = (uint32)(sint16)source->tpage_x;
    source_height = (sint16)source->tpage_y;
    output->offset_y = 0u;
    output->offset_x = 0u;
    output->u = (uint32)x;
    output->v = (uint32)y;
    output->width = (uint32)width;
    output->height = (uint32)height;
    output->simple.u0 = (uint8)x;
    output->simple.v0 = (uint8)y;
    output->simple.w = (uint16)width;
    output->simple.h = (uint16)height;
    output->tpage_y = (uint32)source_height;
    output->last_orientation = 0u;
    output->orientation = 0u;
    output->mode = source->mode;
    SetDrawMode1(&output->draw_mode, 1, 0, source->tpage);
    return (sint32)(uint32)output->draw_mode.code[0];
}

sint32 render_dispatch_rec(SPRITE_RENDER *output, sint16 source_index, sint16 kind)
{
    const SPRITE_IMAGE *source;

    FUNCTION_MARKER(0x80046C84u, "MAIN.EXE");
    source = sprite_image_source(kind, source_index);
    if (kind == 1)
        return menu_build_sprite_packet(output, source, (sint16)source->u, (sint16)source->v, 0, 0, 0, 0, (sint16)source->width, (sint16)source->height);
    return sprite_build_draw_packet(output, source, (sint16)source->u, (sint16)source->v, 0, 0, 0, 0, (sint16)source->width, (sint16)source->height);
}

UI_RECORD *sprite_init_anim_rec(sint16 index, SPRITE_RENDER *data, sint8 type, sint16 value, sint16 first, sint16 second, sint8 flags)
{
    UI_RECORD *address;
    UI_RECORD *record;

    FUNCTION_MARKER(0x80046D18u, "MAIN.EXE");
    if (index < 0 || index >= 180)
        abort();
    sprite_render_at(data, value);
    address = (sprite_records + (sint32)index);
    record = address;
    record->active = 1u;
    record->data = data;
    record->type = (uint8)type;
    record->value = (uint16)value;
    record->x = (uint16)first;
    record->y = (uint16)second;
    record->flags = (uint8)flags;
    record->transition_active = 0u;
    record->command = 0u;
    record->inset_x = 0u;
    record->inset_y = 0u;
    record->delta_x = 0u;
    record->delta_y = 0u;
    record->layer = 0u;
    return address;
}

sint32 render_init_recs(void)
{
    UI_RECORD *records;
    const SPRITE_IMAGE *source_table;
    sint32 count;
    sint32 mode;
    sint32 index;

    FUNCTION_MARKER(0x80046DE0u, "MAIN.EXE");
    sprite_clip_count = 0u;
    render_release_rec_bufs();
    render_alloc_rec_bufs();
    records = sprite_records;
    for (index = 0; index < 180; ++index)
    {
        records[index].type = 0u;
        records[index].value = 0u;
    }
    count = (sint16)menu_asset_counts.labels;
    if (count == 8)
    {
        for (index = 0; index < 8; ++index)
            records[index + 160].type = 0u;
    }
    else
        for (index = 0; index < count; ++index)
        {
            render_dispatch_rec(sprite_render_at(sprite_label_packets, index), (sint16)index, 0);
            sprite_init_anim_rec((sint16)(index + 160), sprite_label_packets, 1, (sint16)index, (sint16)(index << 7), 0, 1);
        }
    count = (sint16)menu_notice.lines;
    for (index = 0; index < count; ++index)
    {
        sint32 first = 0;
        sint32 second = 0;
        mode = (sint16)menu_notice.type;
        if (mode == 1 || mode == 2)
        {
            first = (index << 7) + 128;
            second = 50;
        }
        else if (mode == 3)
        {
            first = 116 * index + 47;
            second = 63;
        }
        else if (mode == 4)
        {
            first = 116 * index + 50;
            second = 55;
        }
        render_dispatch_rec(sprite_render_at(sprite_notice_packets, index), (sint16)index, 2);
        sprite_init_anim_rec((sint16)(index + 170), sprite_notice_packets, 1, (sint16)index, (sint16)first, (sint16)second, 1);
    }
    source_table = sprite_images;
    count = (sint16)menu_asset_counts.sprites;
    for (index = 0; index < count; ++index)
    {
        render_dispatch_rec(sprite_render_at(sprite_image_packets, index), (sint16)index, 1);
        sprite_init_anim_rec((sint16)index, sprite_image_packets, 1, (sint16)index, (sint16)source_table[index].x, (sint16)source_table[index].y, 1);
    }
    return sprite_animate_labels();
}

sint32 scene_release_render_bufs(void)
{
    FUNCTION_MARKER(0x8004715Cu, "MAIN.EXE");
    return render_release_rec_bufs();
}

sint32 sprite_config_prim(SPRITE_RENDER *state, sint32 x, sint32 y)
{
    uint32 width = state->width;
    sint32 kind = (sint32)state->kind;
    uint32 height = state->height;
    sint32 bottom;
    sint32 right;

    FUNCTION_MARKER(0x8004717Cu, "MAIN.EXE");
    if (kind != 0)
    {
        if (kind == 1)
        {
            state->simple.x0 = (uint16)x;
            state->simple.y0 = (uint16)y;
        }
        return 1;
    }
    bottom = (sint32)((uint32)y + height);
    right = (sint32)((uint32)x + width);
    state->quad.x0 = (uint16)x;
    state->quad.y0 = (uint16)y;
    state->quad.x1 = (uint16)x;
    state->quad.y1 = (uint16)bottom;
    state->quad.x2 = (uint16)right;
    state->quad.y2 = (uint16)y;
    state->quad.x3 = (uint16)right;
    state->quad.y3 = (uint16)bottom;
    return right;
}

SPRITE_RENDER *sprite_set_orient(UI_RECORD *address, sint8 orientation)
{
    UI_RECORD *owner = address;
    sint16 record_index;
    SPRITE_RENDER *record;

    FUNCTION_MARKER(0x800471D8u, "MAIN.EXE");
    record_index = (sint16)owner->value;
    record = sprite_render_at(owner->data, record_index);
    record->orientation = (uint8)orientation;
    return record;
}

sint32 sprite_config_uv_orientation(SPRITE_RENDER *state)
{
    uint8 orientation;
    uint32 width;
    uint32 height;
    uint32 x_base;
    uint32 y_base;
    uint8 x0;
    uint8 x1;
    uint8 y0;
    uint8 y1;
    uint8 x[4];
    uint8 y[4];
    uint8 swap;

    FUNCTION_MARKER(0x800471FCu, "MAIN.EXE");
    orientation = (uint8)state->orientation;
    width = state->width;
    x_base = state->u;
    height = state->height;
    x0 = (uint8)x_base;
    x1 = (uint8)(width + x_base);
    state->last_orientation = orientation;
    y_base = state->v;
    y0 = (uint8)y_base;
    y1 = (uint8)(height + y_base);
    x[0] = x0;
    x[1] = x0;
    x[2] = x1;
    x[3] = x1;
    y[0] = y0;
    y[1] = y1;
    y[2] = y0;
    y[3] = y1;
    if (orientation == 1u)
    {
        x[0] = x1;
        x[1] = x1;
        x[2] = x0;
        x[3] = x0;
    }
    if (orientation == 2u)
    {
        swap = x[0];
        x[0] = x[1];
        x[1] = swap;
        swap = x[2];
        x[2] = x[3];
        x[3] = swap;
        swap = y[0];
        y[0] = y[1];
        y[1] = swap;
        swap = y[2];
        y[2] = y[3];
        y[3] = swap;
    }
    if (orientation == 3u)
    {
        swap = x[0];
        x[0] = x[3];
        x[3] = swap;
        swap = x[2];
        x[2] = x[1];
        x[1] = swap;
        swap = y[0];
        y[0] = y[3];
        y[3] = swap;
        swap = y[2];
        y[2] = y[1];
        y[1] = swap;
    }
    state->quad.u0 = x[0];
    state->quad.v0 = y[0];
    state->quad.u1 = x[1];
    state->quad.v1 = y[1];
    state->quad.u2 = x[2];
    state->quad.v2 = y[2];
    state->quad.u3 = x[3];
    state->quad.v3 = y[3];
    return y[3];
}

sint32 sprite_submit_anim(SPRITE_RENDER *state, sint32 x, sint32 y, sint32 ordering_index)
{
    sint32 selector = (sint16)display_state.buffer;
    uint32 *table;

    FUNCTION_MARKER(0x800473A4u, "MAIN.EXE");
    if (selector < 0 || selector >= 2 || ordering_index < 0 || ordering_index >= 40)
        abort();
    table = display_order_slot(selector, ordering_index);
    sprite_config_prim(state, x, y);
    if (state->kind == 1)
    {
        AddPrim(table, &state->simple);
        AddPrim(table, &state->draw_mode);
    }
    else if (state->kind == 0)
        AddPrim(table, &state->quad);
    if (state->orientation != state->last_orientation)
        return sprite_config_uv_orientation(state);
    return state->last_orientation;
}

sint32 sprite_apply_prim_vram_mask(UI_RECORD *owner)
{
    sint16 record_index = (sint16)owner->value;
    SPRITE_RENDER *record = sprite_render_at(owner->data, record_index);
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;
    uint8 mode;
    uint8 flags;
    uint32 tpage_x;
    uint32 tpage_y;

    FUNCTION_MARKER(0x80047454u, "MAIN.EXE");
    mode = (uint8)record->mode;
    if (mode != 0u)
        return mode;
    rectangle.x = (sint16)(uint16)record->clut_x;
    rectangle.y = (sint16)(uint16)record->clut_y;
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    pixels[0] &= UINT16_C(0x7FFF);
    for (index = 1u; index < 16u; ++index)
        pixels[index] |= UINT16_C(0x8000);
    LoadImagePSX(&rectangle, (uint32 *)pixels);
    mode = (uint8)record->mode;
    tpage_x = record->tpage_x;
    tpage_y = record->tpage_y;
    record->quad.tpage = (uint16)getTPage(mode, 3, (sint16)tpage_x, (sint16)tpage_y);
    flags = (uint8)record->quad.code;
    flags = (uint8)(flags | 2u);
    record->quad.code = flags;
    return flags;
}

sint32 sprite_enable_semitransparency(UI_RECORD *owner)
{
    SPRITE_RENDER *record = sprite_render_at(owner->data, (sint16)owner->value);
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;

    FUNCTION_MARKER(0x80047548u, "MAIN.EXE");
    if ((uint8)record->mode != 0u)
        return (uint8)record->mode;
    rectangle.x = (sint16)(uint16)record->clut_x;
    rectangle.y = (sint16)(uint16)record->clut_y;
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    pixels[0] &= UINT16_C(0x7FFF);
    for (index = 1u; index < 16u; ++index)
        pixels[index] |= UINT16_C(0x8000);
    LoadClut((uint32 *)pixels, (sint32)record->clut_x, (sint32)record->clut_y);
    record->quad.tpage = (uint16)getTPage((uint8)record->mode, 0, (sint16)record->tpage_x, (sint16)record->tpage_y);
    record->quad.code |= 2u;
    return (uint8)record->quad.code;
}

sint32 sprite_disable_semitransparency(UI_RECORD *owner)
{
    SPRITE_RENDER *record = sprite_render_at(owner->data, (sint16)owner->value);
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;

    FUNCTION_MARKER(0x8004763Cu, "MAIN.EXE");
    if ((uint8)record->mode != 0u)
        return (uint8)record->mode;
    rectangle.x = (sint16)(uint16)record->clut_x;
    rectangle.y = (sint16)(uint16)record->clut_y;
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    for (index = 1u; index < 16u; ++index)
        pixels[index] &= UINT16_C(0x7FFF);
    LoadClut((uint32 *)pixels, (sint32)record->clut_x, (sint32)record->clut_y);
    DrawSync(0);
    record->quad.tpage = (uint16)getTPage((uint8)record->mode, 0, (sint16)record->tpage_x, (sint16)record->tpage_y);
    record->quad.code &= (uint8)~2u;
    return (uint8)record->quad.code;
}

sint32 sprite_clear_anim_recs(void)
{
    UI_RECORD *base;
    uint32 index;

    FUNCTION_MARKER(0x80047724u, "MAIN.EXE");
    base = sprite_records;
    for (index = 0u; index < 180u; ++index)
    {
        UI_RECORD *record = (base + index);
        record->command = 0u;
        record->inset_y = 0u;
        record->delta_y = 0u;
        record->inset_x = 0u;
        record->delta_x = 0u;
    }
    return 0;
}

sint32 sprite_apply_inset(UI_RECORD *address)
{
    UI_RECORD *owner = address;
    SPRITE_RENDER *record = sprite_render_at(owner->data, (uint16)owner->value);
    sint32 delta_x = (sint16)owner->inset_x;
    sint32 delta_y = (sint16)owner->inset_y;
    sint32 result = delta_y;

    FUNCTION_MARKER(0x800477ACu, "MAIN.EXE");
    if (delta_x != 0)
    {
        owner->x = (uint16)((sint16)owner->x + delta_x);
        record->width -= (uint32)(2 * delta_x);
    }
    if (delta_y != 0)
    {
        owner->y = (uint16)((sint16)owner->y + delta_y);
        record->height -= (uint32)(2 * delta_y);
        result = (sint32)record->height;
    }
    owner->command = 0u;
    owner->delta_x = 0u;
    owner->delta_y = 0u;
    owner->inset_x = 0u;
    owner->inset_y = 0u;
    return result;
}

sint32 menu_move_sprite_rec(UI_RECORD *address, sint8 command, sint16 first, sint16 second, sint32 step_x, sint32 step_y, sint32 unused7, sint32 unused8, sint16 unused9, sint16 unused10)
{
    UI_RECORD *owner = address;
    sint32 index;
    sint32 delta_x;
    sint32 delta_y;
    SPRITE_RENDER *record;
    sint32 result;

    FUNCTION_MARKER(0x800478DCu, "MAIN.EXE");
    index = (sint16)owner->value;
    delta_x = (sint16)owner->inset_x;
    record = sprite_render_at(owner->data, index);
    if (delta_x != 0)
    {
        uint16 position = (uint16)(owner->x + (uint32)delta_x);
        uint32 sprite_position;

        delta_x = (sint16)owner->inset_x;
        owner->x = position;
        sprite_position = record->width - (uint32)delta_x;
        record->width = sprite_position;
        delta_x = (sint16)owner->inset_x;
        sprite_position -= (uint32)delta_x;
        record->width = sprite_position;
    }
    delta_y = (sint16)owner->inset_y;
    result = delta_y;
    if (delta_y != 0)
    {
        uint16 position = (uint16)(owner->y + (uint32)delta_y);
        uint32 sprite_position;

        delta_y = (sint16)owner->inset_y;
        owner->y = position;
        sprite_position = record->height - (uint32)delta_y;
        record->height = sprite_position;
        delta_y = (sint16)owner->inset_y;
        sprite_position -= (uint32)delta_y;
        record->height = sprite_position;
        result = (sint32)sprite_position;
    }
    owner->command = (uint8)command;
    owner->returning = 0u;
    owner->step_x = (uint16)step_x;
    owner->step_y = (uint16)step_y;
    owner->inset_y = 0u;
    owner->delta_y = 0u;
    owner->inset_x = 0u;
    owner->delta_x = 0u;
    owner->target_first = (uint16)first;
    owner->target_second = (uint16)second;
    return result;
}

sint32 sprite_init_trans(UI_RECORD *address, sint32 target_x, sint16 target_y, sint16 target_first, sint16 target_second, sint16 duration)
{
    UI_RECORD *state = address;
    uint16 x = state->x;
    uint16 x_offset = state->inset_x;
    uint16 y;
    uint16 y_offset;
    uint32 current_x;
    uint32 current_y;
    sint16 delta_x;
    sint16 delta_y;
    sint16 divisor = duration;
    sint32 velocity_x = 0;
    sint32 velocity_y = 0;
    uint16 first;
    uint16 second;
    sint32 first_velocity = 0;
    sint32 second_velocity = 0;
    sint32 result;

    FUNCTION_MARKER(0x800479BCu, "MAIN.EXE");
    state->transition_active = 1u;
    y = state->y;
    y_offset = state->inset_y;
    state->transition_target_x = (uint16)target_x;
    state->transition_target_y = (uint16)target_y;
    state->transition_x = 0u;
    state->transition_y = 0u;
    current_x = (uint32)x + (uint32)x_offset;
    current_y = (uint32)y + (uint32)y_offset;
    delta_x = (sint16)(uint16)((uint32)target_x - current_x);
    delta_y = (sint16)(uint16)((uint32)(sint32)target_y - current_y);
    state->transition_base_y = (uint16)current_y;
    state->transition_ticks = (uint8)duration;
    state->transition_base_x = (uint16)current_x;
    if (delta_x != 0)
        velocity_x = math_div_s32((sint32)((uint32)(uint16)delta_x << 16), divisor);
    if (delta_y != 0)
        velocity_y = math_div_s32((sint32)((uint32)(uint16)delta_y << 16), divisor);
    first = state->delta_x;
    second = state->delta_y;
    state->transition_step_y = (uint32)velocity_y;
    state->transition_step_x = (uint32)velocity_x;
    state->transition_base_first = first;
    first = state->delta_x;
    state->transition_target_first = (uint16)target_first;
    state->transition_target_second = (uint16)target_second;
    state->transition_first = 0u;
    state->transition_second = 0u;
    state->transition_base_second = second;
    second = state->delta_y;
    delta_x = (sint16)(uint16)((uint32)(sint32)target_first - (uint32)first);
    delta_y = (sint16)(uint16)((uint32)(sint32)target_second - (uint32)second);
    if (delta_x != 0)
        first_velocity = math_div_s32((sint32)((uint32)(uint16)delta_x << 16), divisor);
    result = delta_y;
    state->transition_step_first = (uint32)first_velocity;
    if (delta_y != 0)
    {
        result = (sint32)((uint32)(uint16)delta_y << 16);
        second_velocity = math_div_s32(result, divisor);
    }
    state->transition_step_second = (uint32)second_velocity;
    return result;
}

sint32 sprite_copy_anim_rec_bytes(const UI_RECORD *source, UI_RECORD *destination)
{
    FUNCTION_MARKER(0x80047B7Cu, "MAIN.EXE");
    *destination = *source;
    return source->unknown_52[1];
}

sint32 sprite_update_anims(void)
{
    UI_RECORD *records = sprite_records;
    uint32 index;

    FUNCTION_MARKER(0x80047BA4u, "MAIN.EXE");
    for (index = 0u; index < 180u; ++index)
    {
        UI_RECORD *record = (records + index);
        SPRITE_RENDER *sprite;
        sint32 delta_x;
        sint32 delta_y;

        if (record->active == 0u)
            continue;
        {
            sint16 sprite_index = (sint16)record->value;
            SPRITE_RENDER *sprite_base = record->data;

            sprite = sprite_render_at(sprite_base, sprite_index);
        }
        delta_x = (sint16)record->inset_x;
        delta_y = (sint16)record->inset_y;
        if (delta_x != 0)
        {
            uint16 position = record->x;
            uint32 sprite_position;

            position = (uint16)(position + delta_x);
            delta_x = (sint16)record->inset_x;
            record->x = position;
            sprite_position = sprite->width - (uint32)delta_x;
            sprite->width = sprite_position;
            delta_x = (sint16)record->inset_x;
            sprite_position -= (uint32)delta_x;
            sprite->width = sprite_position;
        }
        if (delta_y != 0)
        {
            uint16 position = record->y;
            uint32 sprite_position;

            position = (uint16)(position + delta_y);
            delta_y = (sint16)record->inset_y;
            record->y = position;
            sprite_position = sprite->height - (uint32)delta_y;
            sprite->height = sprite_position;
            delta_y = (sint16)record->inset_y;
            sprite_position -= (uint32)delta_y;
            sprite->height = sprite_position;
        }
        if (record->command == 1u)
        {
            if (record->returning != 0u)
            {
                sint16 value = (sint16)record->delta_x;

                if (value != 0)
                    record->delta_x = (uint16)((uint32)value - record->step_x);
                value = (sint16)record->delta_y;
                if (value != 0)
                    record->delta_y = (uint16)((uint32)value - record->step_y);
                value = (sint16)record->delta_x;
                if (value == 0 && (sint16)record->delta_y == 0)
                {
                    record->returning = 0u;
                    record->command = 0u;
                }
            }
            else
            {
                sint16 step = (sint16)record->step_x;

                if (step >= 0)
                {
                    sint16 value = (sint16)record->delta_x;
                    sint16 target = (sint16)record->target_first;

                    if (value != target)
                        record->delta_x = (uint16)(value + step);
                }
                step = (sint16)record->step_y;
                if (step >= 0)
                {
                    sint16 value = (sint16)record->delta_y;
                    sint16 target = (sint16)record->target_second;

                    if (value != target)
                        record->delta_y = (uint16)(value + step);
                }
                if ((sint16)record->delta_x == (sint16)record->target_first && (sint16)record->delta_y == (sint16)record->target_second)
                    record->returning = 1u;
            }
        }
        if (record->transition_active == 1u)
        {
            uint8 remaining = record->transition_ticks;
            uint32 first_position = record->transition_x;
            uint32 first_step = record->transition_step_x;
            uint32 second_position = record->transition_y;
            uint32 second_step = record->transition_step_y;
            uint32 third_position;
            uint32 third_step;
            uint32 fourth_position;
            uint32 fourth_step;
            uint16 base_y;
            sint16 base_x;
            uint16 base_fourth;
            uint16 base_first;
            sint16 base_third;
            uint16 base_third_low;

            first_position += first_step;
            second_position += second_step;
            record->transition_y = second_position;
            record->transition_x = first_position;
            base_y = record->transition_base_y;
            third_step = record->transition_step_first;
            fourth_step = record->transition_step_second;
            third_position = record->transition_first;
            --remaining;
            record->transition_ticks = remaining;
            record->y = (uint16)((sint16)(second_position >> 16) + base_y);
            fourth_position = record->transition_second;
            third_position += third_step;
            base_x = (sint16)(uint16)(record->transition_x >> 16);
            fourth_position += fourth_step;
            record->transition_second = fourth_position;
            record->transition_first = third_position;
            base_fourth = record->transition_base_second;
            base_first = record->transition_base_x;
            record->delta_y = (uint16)((sint16)(fourth_position >> 16) + base_fourth);
            base_third = (sint16)(uint16)(record->transition_first >> 16);
            base_third_low = record->transition_base_first;
            record->x = (uint16)(base_x + base_first);
            remaining = record->transition_ticks;
            record->delta_x = (uint16)(base_third + base_third_low);
            if (remaining == 0u)
            {
                uint16 final_x = record->transition_target_x;
                uint16 final_y = record->transition_target_y;
                uint16 final_first = record->transition_target_first;
                uint16 final_second = record->transition_target_second;

                record->transition_active = 0u;
                record->transition_first = 0u;
                record->transition_second = 0u;
                record->transition_step_first = 0u;
                record->transition_step_second = 0u;
                record->x = final_x;
                record->y = final_y;
                record->delta_x = final_first;
                record->delta_y = final_second;
            }
        }
        if (record->command != 0u)
        {
            delta_x = (sint16)record->delta_x;
            delta_y = (sint16)record->delta_y;
            if (delta_x != 0)
            {
                uint16 position = record->x;
                uint32 sprite_position;

                position = (uint16)(position - delta_x);
                delta_x = (sint16)record->delta_x;
                record->x = position;
                sprite_position = sprite->width + (uint32)delta_x;
                sprite->width = sprite_position;
                delta_x = (sint16)record->delta_x;
                sprite_position += (uint32)delta_x;
                sprite->width = sprite_position;
            }
            if (delta_y != 0)
            {
                uint16 position = record->y;
                uint32 sprite_position;

                position = (uint16)(position - delta_y);
                delta_y = (sint16)record->delta_y;
                record->y = position;
                sprite_position = sprite->height + (uint32)delta_y;
                sprite->height = sprite_position;
                delta_y = (sint16)record->delta_y;
                sprite_position += (uint32)delta_y;
                sprite->height = sprite_position;
            }
        }
        {
            uint16 next_delta_x = record->delta_x;
            uint16 next_delta_y = record->delta_y;

            record->inset_x = next_delta_x;
            record->inset_y = next_delta_y;
        }
    }
    return 0;
}

sint32 sprite_submit_layered_recs(sint32 ordering_index)
{
    sint32 layers = (sint16)menu_state.screen == 10 ? 13 : 1;
    sint32 layer;
    sint32 index;
    sint32 drew = 0;

    FUNCTION_MARKER(0x80047FDCu, "MAIN.EXE");
    for (layer = 0; layer < layers; ++layer)
    {
        for (index = 0; index < 160; ++index)
        {
            UI_RECORD *record = (sprite_records + (sint16)index);
            if (record->layer == (uint8)layer && record->type != 0u && record->flags != 0u)
            {
                sint32 source_index = (sint16)record->value;
                sint32 x = (sint16)record->x;
                sint32 y = (sint16)record->y;
                SPRITE_RENDER *sprite = record->data;

                sprite_submit_anim(sprite_render_at(sprite, source_index), x, y, (sint16)ordering_index);
                drew = 1;
            }
        }
    }
    return (sint16)((uint32)ordering_index + (uint32)drew);
}

sint32 sprite_submit_groups(sint32 ordering_index)
{
    sint32 count = (sint16)menu_asset_counts.labels;
    sint32 source_bound = (sint16)menu_asset_counts.labels;
    uint8 index = 0u;
    uint8 drew = 0u;

    FUNCTION_MARKER(0x80048204u, "MAIN.EXE");
    if (count > 0)
    {
        do
        {
            UI_RECORD *record = (sprite_records + index + 160);
            uint8 active = record->active;

            index = (uint8)(index + 1u);
            if (active != 0u)
            {
                sint32 source_index = (sint16)record->value;
                sint32 x;
                SPRITE_RENDER *sprite;
                sint32 y;

                if (source_index >= source_bound)
                    return 0;
                x = (sint16)record->x;
                sprite = record->data;
                y = (sint16)record->y;
                sprite_submit_anim(sprite_render_at(sprite, source_index), x, y, (sint16)ordering_index);
                drew = 1u;
            }
            count = (sint16)menu_asset_counts.labels;
            source_bound = (sint16)menu_asset_counts.labels;
        } while ((sint32)index < count);
    }
    if (drew != 0u)
    {
        drew = 0u;
        ordering_index = (sint32)((uint32)ordering_index + 1u);
    }
    if ((sint16)menu_notice.value == 1)
    {
        sint32 next_ordering = (sint32)((uint32)ordering_index + 1u);
        sint32 selector = (sint16)display_state.buffer;
        uint32 *ordering = display_order_slot(selector, (sint16)next_ordering);
        TILE *tile = &sprite_notice_tiles[selector];

        ordering_index = (sint32)((uint32)next_ordering + 1u);
        tile->x0 = 132;
        tile->y0 = 54;
        tile->w = 248;
        tile->h = 120;
        setlen(tile, 3);
        tile->r0 = tile->g0 = tile->b0 = 0;
        tile->code = 0x60;
        AddPrim(ordering, tile);
    }
    count = (sint16)menu_notice.lines;
    if (count > 0)
    {
        index = 0u;
        do
        {
            uint8 current = index;
            sint16 record_index = (sint16)((uint32)current + 170u);
            sint16 first = (sint16)(((uint32)current << 7) + 128u);
            sint16 second = 50;
            sint32 mode = (sint16)((uint16)(menu_notice.type - 1u));
            SPRITE_RENDER *data;
            UI_RECORD *record;
            sint32 source_index;
            SPRITE_RENDER *sprite;

            if (mode == 2)
            {
                first = (sint16)(116 * (sint32)current + 47);
                second = 63;
            }
            else if (mode == 3)
            {
                first = (sint16)(116 * (sint32)current + 50);
                second = 55;
            }
            else if (mode == 4)
            {
                first = 144;
                second = 65;
            }
            drew = 1u;
            index = (uint8)(index + 1u);
            data = sprite_notice_packets;
            render_dispatch_rec(sprite_render_at(data, current), (sint16)current, 2);
            data = sprite_notice_packets;
            sprite_init_anim_rec(record_index, data, 1, (sint16)current, first, second, 1);
            record = (sprite_records + record_index);
            source_index = (sint16)record->value;
            sprite = record->data;
            sprite_submit_anim(sprite_render_at(sprite, source_index), first, second, (sint16)ordering_index);
            count = (sint16)menu_notice.lines;
        } while ((sint32)index < count);
    }
    if (drew != 0u)
        ordering_index = (sint32)((uint32)ordering_index + 1u);
    return (sint16)ordering_index;
}

sint32 sprite_submit_pools(sint32 ordering_index)
{
    sint32 group;

    FUNCTION_MARKER(0x80051C18u, "MAIN.EXE");
    for (group = TEXT_FONT_COUNT - 1; group >= 0; --group)
    {
        sint32 buffer = sprite_text_buffer();
        size_t count = text_fonts[group].pending_count[buffer];
        if (count > TEXT_GLYPH_CAPACITY)
            abort();
        if (count != 0u)
        {
            size_t glyph;
            uint32 *ordering = display_order_slot(buffer, (sint16)ordering_index);
            for (glyph = 0; glyph < count; ++glyph)
            {
                const TEXT_GLYPH *source = &text_fonts[group].pending[buffer][glyph];
                SPRT *packet = &sprite_text_packets[group][buffer][glyph];
                packet->x0 = source->x;
                packet->y0 = source->y;
                packet->u0 = source->u;
                packet->v0 = source->v;
                packet->w = source->width;
                packet->h = source->height;
            }
            AddPrim(ordering, &sprite_text_modes[group][buffer]);
            ordering_index = (sint32)((uint32)ordering_index + 1u);
            ordering = display_order_slot(buffer, (sint16)ordering_index);
            for (glyph = count; glyph != 0u; --glyph)
                AddPrim(ordering, &sprite_text_packets[group][buffer][glyph - 1u]);
            ordering_index = (sint32)((uint32)ordering_index + 1u);
            text_fonts[group].pending_count[buffer] = 0;
        }
    }
    return (sint16)ordering_index;
}

sint32 sprite_text_init_packets(void)
{
    static const sint16 widths[TEXT_FONT_COUNT] = {960, 910, 922};
    static const sint16 heights[TEXT_FONT_COUNT] = {256, 256, 352};
    static const sint16 clut_y[TEXT_FONT_COUNT] = {450, 445, 444};
    sint32 group, buffer, index;

    FUNCTION_MARKER(0x80051040u, "MAIN.EXE");
    sprite_register_packets();
    for (group = 0; group < TEXT_FONT_COUNT; ++group)
    {
        uint16 tpage = (uint16)getTPage(0, 0, widths[group], heights[group]);
        uint16 clut = (uint16)getClut(960, clut_y[group]);
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DR_TPAGE *mode = &sprite_text_modes[group][buffer];
            setlen(mode, 1);
            mode->code[0] = 0xE1000400u | tpage;
            for (index = 0; index < TEXT_GLYPH_CAPACITY; ++index)
            {
                SPRT *packet = &sprite_text_packets[group][buffer][index];
                packet->r0 = packet->g0 = packet->b0 = 128;
                packet->clut = clut;
                setlen(packet, 4);
                packet->code = 0x64;
            }
        }
    }
    return 0;
}

sint32 sprite_text_buffer(void)
{
    sint32 buffer = (sint16)display_state.buffer;
    if (buffer < 0 || buffer >= 2)
        abort();
    return buffer;
}
