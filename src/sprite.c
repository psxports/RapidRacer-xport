#include "text.h"
#include "sprite.h"
#include "psx_gpu.h"
#include "arena.h"
#include "disp.h"
#include "utils.h"
#include "menu.h"
#include "menu_select.h"
#include "render.h"
#include "hd.h"
#include "name.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static SPRITE_CLIP spr_clips[SPRITE_CLIP_CAPACITY];
static uint16 spr_clip_count;

// Sprite packet owners keep stable addresses for the shared GPU registry
static SPRITE_RENDER spr_label_packets[10];
static SPRITE_RENDER spr_image_packets[160];
static SPRITE_RENDER spr_notice_packets[10];
SPRITE_RENDER spr_glyphs[52];
uint16 spr_buffers_active;

// Menu text packets from MAIN.EXE 800EA690/800EC5D0/800EE518 and 800D71F0
static SPRT spr_text_packets[TEXT_FONT_COUNT][2][TEXT_GLYPH_CAPACITY];
static DR_TPAGE spr_text_modes[TEXT_FONT_COUNT][2];
// Notice background owner from MAIN.EXE 800D6BC8
static TILE spr_notice_tiles[2];

void spr_register_packets(void)
{
    if (!gpu_register_packet_range(spr_label_packets, sizeof(spr_label_packets)) || !gpu_register_packet_range(spr_image_packets, sizeof(spr_image_packets)) || !gpu_register_packet_range(spr_notice_packets, sizeof(spr_notice_packets)) || !gpu_register_packet_range(spr_glyphs, sizeof(spr_glyphs)) || !gpu_register_packet_range(spr_text_packets, sizeof(spr_text_packets)) || !gpu_register_packet_range(spr_text_modes, sizeof(spr_text_modes)) || !gpu_register_packet_range(spr_notice_tiles, sizeof(spr_notice_tiles)))
        abort();
}

SPRITE_RENDER *spr_render_at(SPRITE_RENDER *base, sint32 idx)
{
    uint32 count;
    if (base == spr_label_packets)
        count = 10;
    else if (base == spr_image_packets)
        count = 160;
    else if (base == spr_notice_packets)
        count = 10;
    else if (base == spr_glyphs)
        count = 52;
    else
        abort();
    if (idx < 0 || (uint32)idx >= count)
        abort();
    return base + idx;
}

// UI array from MAIN.EXE 800B6A74
UI_RECORD spr_recs[180];

// Label descriptors from MAIN.EXE 800957C4
SPRITE_IMAGE spr_labels[8] = {{.path = "BACK\\F01.TIM"}, {.path = "BACK\\F02.TIM"}, {.path = "BACK\\F03.TIM"}, {.path = "BACK\\F04.TIM"}, {.path = "BACK\\F05.TIM"}, {.path = "BACK\\F06.TIM"}, {.path = "BACK\\F07.TIM"}, {.path = "BACK\\F08.TIM"}};

// Working descriptors from MAIN.EXE 800E35C0/800E5B48
SPRITE_IMAGE spr_images[160];
SPRITE_IMAGE spr_notices[10];

void spr_decode_image(SPRITE_IMAGE *image, const uint8 *src)
{
    memcpy(image->path, src, 60);
    image->path[60] = 0;
    image->mode = src[30];
    image->reserved = src[31];
    image->u = xport_load_le16(src + 32);
    image->v = xport_load_le16(src + 34);
    image->width = xport_load_le16(src + 36);
    image->height = xport_load_le16(src + 38);
    image->tpage_x = xport_load_le16(src + 40);
    image->tpage_y = xport_load_le16(src + 42);
    image->clut_x = xport_load_le16(src + 44);
    image->clut_y = xport_load_le16(src + 46);
    image->x = xport_load_le16(src + 48);
    image->y = xport_load_le16(src + 50);
    image->tpage = xport_load_le16(src + 52);
    image->clut = xport_load_le16(src + 54);
}

static void spr_encode_image(const SPRITE_IMAGE *image, uint8 *target)
{
    memcpy(target, image->path, 30);
    target[30] = image->mode;
    target[31] = image->reserved;
    target[32] = image->u;
    target[33] = image->u >> 8;
    target[34] = image->v;
    target[35] = image->v >> 8;
    target[36] = image->width;
    target[37] = image->width >> 8;
    target[38] = image->height;
    target[39] = image->height >> 8;
    target[40] = image->tpage_x;
    target[41] = image->tpage_x >> 8;
    target[42] = image->tpage_y;
    target[43] = image->tpage_y >> 8;
    target[44] = image->clut_x;
    target[45] = image->clut_x >> 8;
    target[46] = image->clut_y;
    target[47] = image->clut_y >> 8;
    target[48] = image->x;
    target[49] = image->x >> 8;
    target[50] = image->y;
    target[51] = image->y >> 8;
    target[52] = image->tpage;
    target[53] = image->tpage >> 8;
    target[54] = image->clut;
    target[55] = image->clut >> 8;
    // Preserve the unused disk tail without interpreting it as a runtime pointer
    memcpy(target + 56, image->path + 56, 4);
}

void spr_refresh_image_path(SPRITE_IMAGE *image)
{
    uint8 packed[60];
    spr_encode_image(image, packed);
    memcpy(image->path, packed, 60);
    image->path[60] = 0;
}

void spr_set_image_path(SPRITE_IMAGE *image, const char *path)
{
    uint8 packed[60];
    size_t len = strlen(path);
    if (len >= sizeof(packed))
        abort();
    // Long names overlay metadata in the compatible file format
    spr_encode_image(image, packed);
    memcpy(packed, path, len + 1);
    spr_decode_image(image, packed);
}

// Pixel scale from MAIN.EXE 80081008/81810 and page width from 80081816
static const struct
{
    sint16 scale;
    sint16 page_width;
} spr_pixel_modes[4] = {{4, 256}, {2, 128}, {1, 64}, {256, 0}};

sint32 spr_pixel_scale(uint8 mode)
{
    if (mode >= 4u)
        abort();
    return spr_pixel_modes[mode].scale;
}

static const SPRITE_IMAGE *spr_image_src(sint16 kind, sint16 idx)
{
    static const struct
    {
        const SPRITE_IMAGE *images;
        uint32 count;
    } grps[3] = {{spr_labels, 8}, {spr_images, 160}, {spr_notices, 10}};

    if (kind < 0 || kind >= 3 || idx < 0 || (uint32)idx >= grps[kind].count)
        abort();
    return &grps[kind].images[idx];
}

sint32 spr_flush_pending(sint32 ordering_idx)
{
    sint32 selector = disp_state.buffer;
    LINE_F2 *line;
    uint32 *table;

    FUNCTION_MARKER(0x800455C8, "MAIN.EXE");
    if (selector < 0 || selector >= 2 || ordering_idx < 0 || ordering_idx >= 40 || disp_state.line_count > 200u)
        abort();
    table = disp_order_slot(selector, ordering_idx);
    if (disp_state.line_count != 0)
    {
        line = disp_line_buffer(selector);
        do
        {
            AddPrim(table, line++);
            --disp_state.line_count;
        } while (disp_state.line_count != 0);
    }
    return ordering_idx;
}

typedef struct
{
    uint16 phase;
    uint16 ticks;
    uint16 frames[4];
} SPRITE_LABEL_ANIMATION;

// Label animation from MAIN.EXE 800B40B6/40B8/F0478
static SPRITE_LABEL_ANIMATION spr_label_anim;

// Label positions from MAIN.EXE 800967A4/967AC
static const sint16 spr_label_pos[4][2] = {{0, 0}, {256, 0}, {0, 120}, {256, 120}};

sint32 spr_reset_labels(void)
{
    FUNCTION_MARKER(0x80046654, "MAIN.EXE");
    spr_label_anim.phase = 0;
    spr_label_anim.ticks = 0;
    return spr_animate_labels();
}

static sint32 spr_build_draw_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *src, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    sint32 src_height;

    FUNCTION_MARKER(0x80046B80, "MAIN.EXE");
    output->kind = 1;
    setSprt(&output->simple);
    setRGB0(&output->simple, 0x80, 0x80, 0x80);
    output->simple.p[0].attr = src->clut;
    output->clut_x = (sint16)src->clut_x;
    output->clut_y = (sint16)src->clut_y;
    output->tpage_x = (sint16)src->tpage_x;
    src_height = (sint16)src->tpage_y;
    output->offset_y = 0;
    output->offset_x = 0;
    output->u = x;
    output->v = y;
    output->width = width;
    output->height = height;
    setUV0(&output->simple, x, y);
    setWH(&output->simple, width, height);
    output->tpage_y = src_height;
    output->last_orientation = 0;
    output->orientation = 0;
    output->mode = src->mode;
    SetDrawMode1(&output->draw_mode, 1, 0, src->tpage);
    return output->draw_mode.code[0];
}

static sint32 render_dispatch_rec(SPRITE_RENDER *output, sint16 src_idx, sint16 kind)
{
    const SPRITE_IMAGE *src;

    FUNCTION_MARKER(0x80046C84, "MAIN.EXE");
    src = spr_image_src(kind, src_idx);
    if (kind == 1)
        return menu_build_spr_packet(output, src, (sint16)src->u, (sint16)src->v, 0, 0, 0, 0, (sint16)src->width, (sint16)src->height);
    return spr_build_draw_packet(output, src, (sint16)src->u, (sint16)src->v, 0, 0, 0, 0, (sint16)src->width, (sint16)src->height);
}

sint32 spr_animate_labels(void)
{
    sint32 phase = spr_label_anim.phase;
    UI_RECORD *base = spr_recs;
    sint32 idx;

    FUNCTION_MARKER(0x8004667C, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
    {
        ++phase;
        spr_label_anim.frames[idx] = phase;
        if (phase == 8)
            phase = 0;
    }
    spr_label_anim.ticks++;
    if (menu_asset_counts.labels == 8)
    {
        for (idx = 0; idx < 8; ++idx)
            base[idx + 160].type = 0;
        for (idx = 0; idx < 4; ++idx)
        {
            sint16 src_idx = spr_label_anim.frames[idx] - 1;
            render_dispatch_rec(spr_render_at(spr_label_packets, idx), src_idx, 0);
            spr_init_anim_rec(idx + 160, spr_label_packets, 1, idx, spr_label_pos[idx][0], spr_label_pos[idx][1], 1);
        }
    }
    if (spr_label_anim.ticks == 6)
    {
        spr_label_anim.ticks = 0;
        phase = (sint16)spr_label_anim.phase + 1;
        if (phase == 8)
            phase = 0;
        spr_label_anim.phase = phase;
        return phase;
    }
    return 6;
}

static sint32 render_alloc_rec_bufs(void)
{
    FUNCTION_MARKER(0x8004684C, "MAIN.EXE");
    spr_register_packets();
    arena_push();
    spr_buffers_active = 1;
    disp_state.line_count = 0;
    return 1;
}

static sint32 render_release_rec_bufs(void)
{
    sint32 enabled;

    FUNCTION_MARKER(0x800468AC, "MAIN.EXE");
    enabled = (sint16)spr_buffers_active;
    if (enabled != 0)
        return arena_pop();
    return enabled;
}

sint32 menu_build_spr_packet(SPRITE_RENDER *output, const SPRITE_IMAGE *src, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    sint32 idx;
    sint32 divisor;
    sint32 limit;
    sint32 visible_width;
    sint32 overflow_width;
    sint32 src_height;
    sint32 right_edge;
    SPRITE_CLIP *rec;
    uint16 selector;

    FUNCTION_MARKER(0x800468D4, "MAIN.EXE");
    output->kind = 0;
    idx = src->mode;
    divisor = spr_pixel_scale(src->mode);
    limit = spr_pixel_modes[idx].page_width;
    visible_width = width - 1u;
    right_edge = divisor != 0 ? x + (sint16)(width / divisor) : 0;
    if (divisor != 0 && right_edge >= limit)
    {
        sint32 clipped_width = limit - x;

        overflow_width = visible_width - clipped_width;
        visible_width = clipped_width;
        selector = spr_clip_count;
        if (selector >= SPRITE_CLIP_CAPACITY)
            abort();
        rec = &spr_clips[selector];
        setPolyFT4(&rec->quad);
        setSemiTrans(&rec->quad, 1);
        setRGB0(&rec->quad, 0x80, 0x80, 0x80);
        rec->quad.p[1].attr = getTPage(idx, 0, src->tpage_x + visible_width, src->tpage_y);
        rec->quad.p[0].attr = getClut(src->clut_x, src->clut_y);
        rec->offset_y = 0;
        rec->offset_x = 0;
        rec->u = 0;
        rec->v = y;
        rec->width = (sint16)overflow_width;
        rec->height = height;
        selector = spr_clip_count;
        spr_clip_count = selector + 1u;
        output->clip_idx = selector;
        setUV4(&rec->quad, 0, y, 0, y + height, overflow_width, y, overflow_width, y + height);
    }
    else
    {
        output->clip_idx = 0;
    }

    setPolyFT4(&output->quad);
    setSemiTrans(&output->quad, 1);
    setRGB0(&output->quad, 0x80, 0x80, 0x80);
    output->quad.p[1].attr = src->tpage;
    output->quad.p[0].attr = src->clut;
    output->clut_x = (sint16)src->clut_x;
    output->clut_y = (sint16)src->clut_y;
    output->tpage_x = (sint16)src->tpage_x;
    src_height = (sint16)src->tpage_y;
    output->offset_y = 0;
    output->offset_x = 0;
    output->u = x;
    output->v = y;
    output->width = visible_width;
    output->height = height;
    output->tpage_y = src_height;
    setUV4(&output->quad, x, y, x, y + height, x + visible_width, y, x + visible_width, y + height);

    output->last_orientation = 0;
    output->orientation = 0;
    output->mode = idx;
    return x + visible_width;
}

UI_RECORD *spr_init_anim_rec(sint16 idx, SPRITE_RENDER *data, sint8 type, sint16 val, sint16 x, sint16 y, sint8 flags)
{
    UI_RECORD *address;
    UI_RECORD *rec;

    FUNCTION_MARKER(0x80046D18, "MAIN.EXE");
    if (idx < 0 || idx >= 180)
        abort();
    spr_render_at(data, val);
    address = spr_recs + idx;
    rec = address;
    rec->active = 1;
    rec->data = data;
    rec->type = type;
    rec->val = val;
    rec->x = x;
    rec->y = y;
    rec->flags = flags;
    rec->transition_active = 0;
    rec->command = 0;
    rec->inset_x = 0;
    rec->inset_y = 0;
    rec->delta_x = 0;
    rec->delta_y = 0;
    rec->layer = 0;
    return address;
}

sint32 render_init_recs(void)
{
    UI_RECORD *recs;
    const SPRITE_IMAGE *src_table;
    sint32 count;
    sint32 mode;
    sint32 idx;

    FUNCTION_MARKER(0x80046DE0, "MAIN.EXE");
    spr_clip_count = 0;
    render_release_rec_bufs();
    render_alloc_rec_bufs();
    recs = spr_recs;
    for (idx = 0; idx < 180; ++idx)
    {
        recs[idx].type = 0;
        recs[idx].val = 0;
    }
    count = menu_asset_counts.labels;
    if (count == 8)
    {
        for (idx = 0; idx < 8; ++idx)
            recs[idx + 160].type = 0;
    }
    else
        for (idx = 0; idx < count; ++idx)
        {
            render_dispatch_rec(spr_render_at(spr_label_packets, idx), idx, 0);
            spr_init_anim_rec(idx + 160, spr_label_packets, 1, idx, idx << 7, 0, 1);
        }
    count = (sint16)menu_notice.lines;
    for (idx = 0; idx < count; ++idx)
    {
        sint32 x = 0;
        sint32 y = 0;
        mode = (sint16)menu_notice.type;
        if (mode == 1 || mode == 2)
        {
            x = (idx << 7) + 128;
            y = 50;
        }
        else if (mode == 3)
        {
            x = 116 * idx + 47;
            y = 63;
        }
        else if (mode == 4)
        {
            x = 116 * idx + 50;
            y = 55;
        }
        render_dispatch_rec(spr_render_at(spr_notice_packets, idx), idx, 2);
        spr_init_anim_rec(idx + 170, spr_notice_packets, 1, idx, x, y, 1);
    }
    src_table = spr_images;
    count = menu_asset_counts.sprs;
    for (idx = 0; idx < count; ++idx)
    {
        render_dispatch_rec(spr_render_at(spr_image_packets, idx), idx, 1);
        spr_init_anim_rec(idx, spr_image_packets, 1, idx, src_table[idx].x, src_table[idx].y, 1);
    }
    return spr_animate_labels();
}

sint32 track_release_render_bufs(void)
{
    FUNCTION_MARKER(0x8004715C, "MAIN.EXE");
    return render_release_rec_bufs();
}

static sint32 spr_config_prim(SPRITE_RENDER *state, sint32 x, sint32 y)
{
    uint32 width = state->width;
    sint32 kind = state->kind;
    uint32 height = state->height;
    sint32 bottom;
    sint32 right;

    FUNCTION_MARKER(0x8004717C, "MAIN.EXE");
    if (kind != 0)
    {
        if (kind == 1)
        {
            setXY0(&state->simple, x, y);
        }
        return 1;
    }
    bottom = y + height;
    right = x + width;
    setXY0(&state->quad, x, y);
    state->quad.p[1].xy.x = x;
    state->quad.p[1].xy.y = bottom;
    state->quad.p[2].xy.x = right;
    state->quad.p[2].xy.y = y;
    state->quad.p[3].xy.x = right;
    state->quad.p[3].xy.y = bottom;
    return right;
}

SPRITE_RENDER *spr_set_orient(UI_RECORD *address, sint8 orientation)
{
    UI_RECORD *owner = address;
    sint16 rec_idx;
    SPRITE_RENDER *rec;

    FUNCTION_MARKER(0x800471D8, "MAIN.EXE");
    rec_idx = owner->val;
    rec = spr_render_at(owner->data, rec_idx);
    rec->orientation = orientation;
    return rec;
}

static sint32 spr_config_uv_orientation(SPRITE_RENDER *state)
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

    FUNCTION_MARKER(0x800471FC, "MAIN.EXE");
    orientation = state->orientation;
    width = state->width;
    x_base = state->u;
    height = state->height;
    x0 = x_base;
    x1 = width + x_base;
    state->last_orientation = orientation;
    y_base = state->v;
    y0 = y_base;
    y1 = height + y_base;
    x[0] = x0;
    x[1] = x0;
    x[2] = x1;
    x[3] = x1;
    y[0] = y0;
    y[1] = y1;
    y[2] = y0;
    y[3] = y1;
    if (orientation == 1)
    {
        x[0] = x1;
        x[1] = x1;
        x[2] = x0;
        x[3] = x0;
    }
    if (orientation == 2)
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
    if (orientation == 3)
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
    setUV4(&state->quad, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);

    return y[3];
}

static sint32 spr_submit_anim(SPRITE_RENDER *state, sint32 x, sint32 y, sint32 ordering_idx)
{
    sint32 selector = disp_state.buffer;
    uint32 *table;

    FUNCTION_MARKER(0x800473A4, "MAIN.EXE");
    if (selector < 0 || selector >= 2 || ordering_idx < 0 || ordering_idx >= 40)
        abort();
    table = disp_order_slot(selector, ordering_idx);
    spr_config_prim(state, x, y);
    if (state->kind == 1)
    {
        AddPrim(table, &state->simple);
        AddPrim(table, &state->draw_mode);
    }
    else if (state->kind == 0)
        AddPrim(table, &state->quad);
    if (state->orientation != state->last_orientation)
        return spr_config_uv_orientation(state);
    return state->last_orientation;
}

sint32 spr_apply_prim_vram_mask(UI_RECORD *owner)
{
    sint16 rec_idx = owner->val;
    SPRITE_RENDER *rec = spr_render_at(owner->data, rec_idx);
    PSX_RECT rect;
    uint16 pal[16];
    uint32 idx;
    uint8 mode;
    uint8 flags;
    uint32 tpage_x;
    uint32 tpage_y;

    FUNCTION_MARKER(0x80047454, "MAIN.EXE");
    mode = rec->mode;
    if (mode != 0)
        return mode;
    rect.x = rec->clut_x;
    rect.y = rec->clut_y;
    rect.w = 16;
    rect.h = 1;
    StoreImage(&rect, (uint32 *)pal);
    DrawSync(0);
    pal[0] &= 0x7FFF;
    for (idx = 1; idx < 16; ++idx)
        pal[idx] |= 0x8000;
    LoadImagePSX(&rect, (uint32 *)pal);
    mode = rec->mode;
    tpage_x = rec->tpage_x;
    tpage_y = rec->tpage_y;
    rec->quad.p[1].attr = getTPage(mode, 3, tpage_x, tpage_y);
    flags = rec->quad.rgbc.code;
    flags = flags | 2u;
    rec->quad.rgbc.code = flags;
    return flags;
}

sint32 spr_enable_semitransparency(UI_RECORD *owner)
{
    SPRITE_RENDER *rec = spr_render_at(owner->data, owner->val);
    PSX_RECT rect;
    uint16 pal[16];
    uint32 idx;

    FUNCTION_MARKER(0x80047548, "MAIN.EXE");
    if (rec->mode != 0)
        return rec->mode;
    rect.x = rec->clut_x;
    rect.y = rec->clut_y;
    rect.w = 16;
    rect.h = 1;
    StoreImage(&rect, (uint32 *)pal);
    DrawSync(0);
    pal[0] &= 0x7FFF;
    for (idx = 1; idx < 16; ++idx)
        pal[idx] |= 0x8000;
    LoadClut((uint32 *)pal, rec->clut_x, rec->clut_y);
    rec->quad.p[1].attr = getTPage(rec->mode, 0, rec->tpage_x, rec->tpage_y);
    rec->quad.rgbc.code |= 2;
    return rec->quad.rgbc.code;
}

sint32 spr_disable_semitransparency(UI_RECORD *owner)
{
    SPRITE_RENDER *rec = spr_render_at(owner->data, owner->val);
    PSX_RECT rect;
    uint16 pal[16];
    uint32 idx;

    FUNCTION_MARKER(0x8004763C, "MAIN.EXE");
    if (rec->mode != 0)
        return rec->mode;
    rect.x = rec->clut_x;
    rect.y = rec->clut_y;
    rect.w = 16;
    rect.h = 1;
    StoreImage(&rect, (uint32 *)pal);
    DrawSync(0);
    for (idx = 1; idx < 16; ++idx)
        pal[idx] &= 0x7FFF;
    LoadClut((uint32 *)pal, rec->clut_x, rec->clut_y);
    DrawSync(0);
    rec->quad.p[1].attr = getTPage(rec->mode, 0, rec->tpage_x, rec->tpage_y);
    rec->quad.rgbc.code &= ~2u;
    return rec->quad.rgbc.code;
}

sint32 spr_clear_anim_recs(void)
{
    UI_RECORD *base;
    uint32 idx;

    FUNCTION_MARKER(0x80047724, "MAIN.EXE");
    base = spr_recs;
    for (idx = 0; idx < 180; ++idx)
    {
        UI_RECORD *rec = base + idx;
        rec->command = 0;
        rec->inset_y = 0;
        rec->delta_y = 0;
        rec->inset_x = 0;
        rec->delta_x = 0;
    }
    return 0;
}

sint32 spr_apply_inset(UI_RECORD *address)
{
    UI_RECORD *owner = address;
    SPRITE_RENDER *rec = spr_render_at(owner->data, owner->val);
    sint16 delta_x = owner->inset_x;
    sint16 delta_y = owner->inset_y;
    sint32 result = delta_y;

    FUNCTION_MARKER(0x800477AC, "MAIN.EXE");
    if (delta_x != 0)
    {
        owner->x = owner->x + delta_x;
        rec->width -= 2 * delta_x;
    }
    if (delta_y != 0)
    {
        owner->y = owner->y + delta_y;
        rec->height -= 2 * delta_y;
        result = rec->height;
    }
    owner->command = 0;
    owner->delta_x = 0;
    owner->delta_y = 0;
    owner->inset_x = 0;
    owner->inset_y = 0;
    return result;
}

sint32 menu_move_spr_rec(UI_RECORD *address, sint8 command, sint16 inset_x, sint16 inset_y, sint32 step_x, sint32 step_y)
{
    UI_RECORD *owner = address;
    sint32 idx;
    sint32 delta_x;
    sint32 delta_y;
    SPRITE_RENDER *rec;
    sint32 result;

    FUNCTION_MARKER(0x800478DC, "MAIN.EXE");
    idx = owner->val;
    delta_x = owner->inset_x;
    rec = spr_render_at(owner->data, idx);
    if (delta_x != 0)
    {
        uint16 pos = owner->x + delta_x;
        uint32 spr_pos;

        delta_x = owner->inset_x;
        owner->x = pos;
        spr_pos = rec->width - delta_x;
        rec->width = spr_pos;
        delta_x = owner->inset_x;
        spr_pos -= delta_x;
        rec->width = spr_pos;
    }
    delta_y = owner->inset_y;
    result = delta_y;
    if (delta_y != 0)
    {
        uint16 pos = owner->y + delta_y;
        uint32 spr_pos;

        delta_y = owner->inset_y;
        owner->y = pos;
        spr_pos = rec->height - delta_y;
        rec->height = spr_pos;
        delta_y = owner->inset_y;
        spr_pos -= delta_y;
        rec->height = spr_pos;
        result = spr_pos;
    }
    owner->command = command;
    owner->returning = 0;
    owner->step_x = step_x;
    owner->step_y = step_y;
    owner->inset_y = 0;
    owner->delta_y = 0;
    owner->inset_x = 0;
    owner->delta_x = 0;
    owner->target0 = inset_x;
    owner->target1 = inset_y;
    return result;
}

sint32 spr_init_trans(UI_RECORD *address, sint32 target_x, sint16 target_y, sint16 target0, sint16 target1, sint16 duration)
{
    UI_RECORD *state = address;
    uint16 x = state->x;
    uint16 x_offset = state->inset_x;
    uint16 y;
    uint16 y_offset;
    uint32 cur_x;
    uint32 cur_y;
    sint16 delta_x;
    sint16 delta_y;
    sint16 divisor = duration;
    sint32 velocity_x = 0;
    sint32 velocity_y = 0;
    sint32 velocity0 = 0;
    sint32 velocity1 = 0;
    sint32 result;

    FUNCTION_MARKER(0x800479BC, "MAIN.EXE");
    state->transition_active = 1;
    y = state->y;
    y_offset = state->inset_y;
    state->transition_target_x = target_x;
    state->transition_target_y = target_y;
    state->transition_x = 0;
    state->transition_y = 0;
    cur_x = x + x_offset;
    cur_y = y + y_offset;
    delta_x = target_x - cur_x;
    delta_y = target_y - cur_y;
    state->transition_base_y = cur_y;
    state->transition_ticks = duration;
    state->transition_base_x = cur_x;
    if (delta_x != 0)
        velocity_x = (sint32)((uint32)(uint16)delta_x << 16) / divisor;
    if (delta_y != 0)
        velocity_y = (sint32)((uint32)(uint16)delta_y << 16) / divisor;
    state->transition_step_y = velocity_y;
    state->transition_step_x = velocity_x;
    state->transition_base0 = state->delta_x;
    state->transition_target0 = target0;
    state->transition_target1 = target1;
    state->transition0 = 0;
    state->transition1 = 0;
    state->transition_base1 = state->delta_y;
    delta_x = target0 - state->delta_x;
    delta_y = target1 - state->delta_y;
    if (delta_x != 0)
        velocity0 = (sint32)((uint32)(uint16)delta_x << 16) / divisor;
    result = delta_y;
    state->transition_step0 = velocity0;
    if (delta_y != 0)
    {
        result = (uint32)(uint16)delta_y << 16;
        velocity1 = result / divisor;
    }
    state->transition_step1 = velocity1;
    return result;
}

sint32 spr_copy_anim_rec_bytes(const UI_RECORD *src, UI_RECORD *dst)
{
    FUNCTION_MARKER(0x80047B7C, "MAIN.EXE");
    *dst = *src;
    return src->unknown_52[1];
}

static void spr_step_anim(UI_RECORD *rec, SPRITE_RENDER *spr)
{
    sint32 delta_x, delta_y;
    delta_x = rec->inset_x;
    delta_y = rec->inset_y;
    if (delta_x != 0)
    {
        uint16 pos = rec->x;
        uint32 spr_pos;

        pos = pos + delta_x;
        delta_x = rec->inset_x;
        rec->x = pos;
        spr_pos = spr->width - delta_x;
        spr->width = spr_pos;
        delta_x = rec->inset_x;
        spr_pos -= delta_x;
        spr->width = spr_pos;
    }
    if (delta_y != 0)
    {
        uint16 pos = rec->y;
        uint32 spr_pos;

        pos = pos + delta_y;
        delta_y = rec->inset_y;
        rec->y = pos;
        spr_pos = spr->height - delta_y;
        spr->height = spr_pos;
        delta_y = rec->inset_y;
        spr_pos -= delta_y;
        spr->height = spr_pos;
    }
    if (rec->command == 1)
    {
        if (rec->returning != 0)
        {
            sint16 val = rec->delta_x;

            if (val != 0)
                rec->delta_x = val - rec->step_x;
            val = rec->delta_y;
            if (val != 0)
                rec->delta_y = val - rec->step_y;
            val = rec->delta_x;
            if (val == 0 && rec->delta_y == 0)
            {
                rec->returning = 0;
                rec->command = 0;
            }
        }
        else
        {
            sint16 step = rec->step_x;

            if (step >= 0)
            {
                sint16 val = rec->delta_x;
                sint16 target = rec->target0;

                if (val != target)
                    rec->delta_x = val + step;
            }
            step = rec->step_y;
            if (step >= 0)
            {
                sint16 val = rec->delta_y;
                sint16 target = rec->target1;

                if (val != target)
                    rec->delta_y = val + step;
            }
            if (rec->delta_x == rec->target0 && rec->delta_y == rec->target1)
                rec->returning = 1;
        }
    }
    if (rec->transition_active == 1)
    {
        rec->transition_x += rec->transition_step_x;
        rec->transition_y += rec->transition_step_y;
        --rec->transition_ticks;
        rec->y = (rec->transition_y >> 16) + rec->transition_base_y;
        rec->transition0 += rec->transition_step0;
        rec->transition1 += rec->transition_step1;
        rec->delta_y = (rec->transition1 >> 16) + rec->transition_base1;
        rec->x = (rec->transition_x >> 16) + rec->transition_base_x;
        rec->delta_x = (rec->transition0 >> 16) + rec->transition_base0;
        if (rec->transition_ticks == 0)
        {
            rec->transition_active = 0;
            rec->transition0 = 0;
            rec->transition1 = 0;
            rec->transition_step0 = 0;
            rec->transition_step1 = 0;
            rec->x = rec->transition_target_x;
            rec->y = rec->transition_target_y;
            rec->delta_x = rec->transition_target0;
            rec->delta_y = rec->transition_target1;
        }
    }
    if (rec->command != 0)
    {
        delta_x = rec->delta_x;
        delta_y = rec->delta_y;
        if (delta_x != 0)
        {
            uint16 pos = rec->x;
            uint32 spr_pos;

            pos = pos - delta_x;
            delta_x = rec->delta_x;
            rec->x = pos;
            spr_pos = spr->width + delta_x;
            spr->width = spr_pos;
            delta_x = rec->delta_x;
            spr_pos += delta_x;
            spr->width = spr_pos;
        }
        if (delta_y != 0)
        {
            uint16 pos = rec->y;
            uint32 spr_pos;

            pos = pos - delta_y;
            delta_y = rec->delta_y;
            rec->y = pos;
            spr_pos = spr->height + delta_y;
            spr->height = spr_pos;
            delta_y = rec->delta_y;
            spr_pos += delta_y;
            spr->height = spr_pos;
        }
    }
    uint16 next_delta_x = rec->delta_x;
    uint16 next_delta_y = rec->delta_y;

    rec->inset_x = next_delta_x;
    rec->inset_y = next_delta_y;
}

sint32 spr_update_anims(void)
{
    FUNCTION_MARKER(0x80047BA4, "MAIN.EXE");
    for (uint32 idx = 0; idx < 180; ++idx)
    {
        UI_RECORD *rec = &spr_recs[idx];
        if (rec->active)
            spr_step_anim(rec, spr_render_at(rec->data, rec->val));
    }
    return 0;
}

sint32 spr_submit_layered_recs(sint32 ordering_idx)
{
    sint32 layers = menu_state.scr == 10 ? 13 : 1;
    sint32 layer;
    sint32 idx;
    sint32 drew = 0;

    FUNCTION_MARKER(0x80047FDC, "MAIN.EXE");
    for (layer = 0; layer < layers; ++layer)
    {
        for (idx = 0; idx < 160; ++idx)
        {
            UI_RECORD *rec = spr_recs + (sint16)idx;
            if (rec->layer == (uint8)layer && rec->type != 0 && rec->flags != 0)
            {
                sint16 src_idx = rec->val;
                sint16 x = rec->x;
                sint16 y = rec->y;
                SPRITE_RENDER *spr = rec->data;

                spr_submit_anim(spr_render_at(spr, src_idx), x, y, (sint16)ordering_idx);
                drew = 1;
            }
        }
    }
    return (sint16)(ordering_idx + drew);
}

sint32 spr_submit_grps(sint32 ordering_idx)
{
    sint32 count = menu_asset_counts.labels;
    sint32 src_bound = menu_asset_counts.labels;
    uint8 idx = 0;
    uint8 drew = 0;

    FUNCTION_MARKER(0x80048204, "MAIN.EXE");
    if (count > 0)
    {
        do
        {
            UI_RECORD *rec = spr_recs + idx + 160;
            uint8 active = rec->active;

            idx++;
            if (active != 0)
            {
                sint16 src_idx = rec->val;
                sint32 x;
                SPRITE_RENDER *spr;
                sint32 y;

                if (src_idx >= src_bound)
                    return 0;
                x = rec->x;
                spr = rec->data;
                y = rec->y;
                spr_submit_anim(spr_render_at(spr, src_idx), x, y, (sint16)ordering_idx);
                drew = 1;
            }
            count = menu_asset_counts.labels;
            src_bound = menu_asset_counts.labels;
        } while (idx < count);
    }
    if (drew != 0)
    {
        drew = 0;
        ordering_idx++;
    }
    if (menu_notice.val == 1)
    {
        sint32 next_ordering = ordering_idx + 1u;
        sint32 selector = disp_state.buffer;
        uint32 *ordering = disp_order_slot(selector, (sint16)next_ordering);
        TILE *tile = &spr_notice_tiles[selector];

        ordering_idx = next_ordering + 1u;
        tile->p[0].x = 132;
        tile->p[0].y = 54;
        setWH(tile, 248, 120);
        setTile(tile);
        setRGB0(tile, 0, 0, 0);
        AddPrim(ordering, tile);
    }
    count = (sint16)menu_notice.lines;
    if (count > 0)
    {
        idx = 0;
        do
        {
            uint8 cur = idx;
            sint16 rec_idx = cur + 170u;
            sint16 x = (cur << 7) + 128;
            sint16 y = 50;
            sint16 mode = menu_notice.type - 1u;
            SPRITE_RENDER *data;
            UI_RECORD *rec;
            sint32 src_idx;
            SPRITE_RENDER *spr;

            if (mode == 2)
            {
                x = 116 * cur + 47;
                y = 63;
            }
            else if (mode == 3)
            {
                x = 116 * cur + 50;
                y = 55;
            }
            else if (mode == 4)
            {
                x = 144;
                y = 65;
            }
            drew = 1;
            idx++;
            data = spr_notice_packets;
            render_dispatch_rec(spr_render_at(data, cur), cur, 2);
            data = spr_notice_packets;
            spr_init_anim_rec(rec_idx, data, 1, cur, x, y, 1);
            rec = spr_recs + rec_idx;
            src_idx = rec->val;
            spr = rec->data;
            spr_submit_anim(spr_render_at(spr, src_idx), x, y, (sint16)ordering_idx);
            count = (sint16)menu_notice.lines;
        } while (idx < count);
    }
    if (drew != 0)
        ordering_idx++;
    return (sint16)ordering_idx;
}

sint32 spr_submit_pools(sint32 ordering_idx)
{
    sint32 grp;

    FUNCTION_MARKER(0x80051C18, "MAIN.EXE");
    for (grp = TEXT_FONT_COUNT - 1; grp >= 0; --grp)
    {
        sint32 buffer = spr_text_buffer();
        uint32 count = text_fonts[grp].pending_count[buffer];
        if (count > TEXT_GLYPH_CAPACITY)
            abort();
        if (count != 0)
        {
            uint32 glyph;
            uint32 *ordering = disp_order_slot(buffer, (sint16)ordering_idx);
            for (glyph = 0; glyph < count; ++glyph)
            {
                const TEXT_GLYPH *src = &text_fonts[grp].pending[buffer][glyph];
                SPRT *packet = &spr_text_packets[grp][buffer][glyph];
                setXY0(packet, src->x, src->y);
                setUV0(packet, src->u, src->v);
                setWH(packet, src->width, src->height);
            }
            AddPrim(ordering, &spr_text_modes[grp][buffer]);
            ordering_idx++;
            ordering = disp_order_slot(buffer, (sint16)ordering_idx);
            for (glyph = count; glyph != 0; --glyph)
                AddPrim(ordering, &spr_text_packets[grp][buffer][glyph - 1]);
            ordering_idx++;
            text_fonts[grp].pending_count[buffer] = 0;
        }
    }
    return (sint16)ordering_idx;
}

static void hd_spr_rect(float x, float y, float width, float height, float u, float v, uint16 tpage, uint16 clut, uint32 color, sint32 textured, sint32 semi)
{
    float pos[4][2], uv[4][2];
    for (uint32 idx = 0; idx < 4; ++idx)
    {
        pos[idx][0] = x + (idx & 2 ? width : 0);
        pos[idx][1] = y + (idx & 1 ? height : 0);
        uv[idx][0] = u + (idx & 2 ? width : 0);
        uv[idx][1] = v + (idx & 1 ? height : 0);
    }
    hd_draw_ui_quad(pos, uv, tpage, clut, color, 256, textured, semi);
}

static void hd_spr_sample(const UI_RECORD *rec, float fract, float rect[4])
{
    const SPRITE_RENDER *spr = spr_render_at(rec->data, rec->val);
    UI_RECORD next = *rec;
    SPRITE_RENDER next_spr = *spr;
    if (rec->active && fract > 0)
        spr_step_anim(&next, &next_spr);
    rect[0] = rec->x + (next.x - rec->x) * fract;
    rect[1] = rec->y + (next.y - rec->y) * fract;
    rect[2] = (float)spr->width + ((float)next_spr.width - spr->width) * fract;
    rect[3] = (float)spr->height + ((float)next_spr.height - spr->height) * fract;
}

static void hd_spr_anim(const UI_RECORD *rec, float fract)
{
    const SPRITE_RENDER *state = spr_render_at(rec->data, rec->val);
    float rect[4];
    hd_spr_sample(rec, fract, rect);
    const UI_RECORD *parent = hd_menu_spr_parent(rec);
    if (parent)
    {
        float parent_rect[4];
        hd_spr_sample(parent, fract, parent_rect);
        rect[0] += parent_rect[0] - parent->x;
        rect[1] += parent_rect[1] - parent->y;
    }
    else if (menu_state.scr == 10 && rec >= spr_recs + 18 && rec <= spr_recs + 20)
        rect[0] += hd_menu_course_step() * fract;
    if (state->kind == 1)
    {
        const SPRT *spr = &state->simple;
        hd_spr_rect(rect[0], rect[1], spr->w, spr->h, spr->p[0].uv.u, spr->p[0].uv.v, state->draw_mode.code[0] & 0x1ff, spr->p[0].attr, spr->rgbc.rgbc, 1, (spr->rgbc.code & 2) != 0);
    }
    else if (state->kind == 0)
    {
        float pos[4][2], uv[4][2];
        for (uint32 idx = 0; idx < 4; ++idx)
        {
            pos[idx][0] = rect[0] + (idx & 2 ? rect[2] : 0);
            pos[idx][1] = rect[1] + (idx & 1 ? rect[3] : 0);
            uv[idx][0] = state->quad.p[idx].uv.u;
            uv[idx][1] = state->quad.p[idx].uv.v;
        }
        hd_draw_ui_quad(pos, uv, state->quad.p[1].attr, state->quad.p[0].attr, state->quad.rgbc.rgbc, 256, 1, (state->quad.rgbc.code & 2) != 0);
    }
}

void hd_spr_prepare_menu(void)
{
    for (sint32 idx = 0; idx < (sint16)menu_notice.lines; ++idx)
    {
        sint16 x = (idx << 7) + 128, y = 50;
        switch (menu_notice.type - 1)
        {
            case 2:
                x = 116 * idx + 47;
                y = 63;
                break;
            case 3:
                x = 116 * idx + 50;
                y = 55;
                break;
            case 4:
                x = 144;
                y = 65;
                break;
        }
        render_dispatch_rec(spr_render_at(spr_notice_packets, idx), idx, 2);
        spr_init_anim_rec(idx + 170, spr_notice_packets, 1, idx, x, y, 1);
    }
    for (sint32 idx = 0; idx < 180; ++idx)
    {
        const UI_RECORD *rec = &spr_recs[idx];
        if (idx < 160)
        {
            if (!(rec->type && rec->flags))
                continue;
        }
        else if (idx < 170)
        {
            if (idx >= 160 + menu_asset_counts.labels || !rec->active || rec->val >= menu_asset_counts.labels)
                continue;
        }
        else if (idx >= 170 + menu_notice.lines)
            continue;
        SPRITE_RENDER *spr = spr_render_at(rec->data, rec->val);
        if (spr->orientation != spr->last_orientation)
            spr_config_uv_orientation(spr);
    }
}

static void hd_spr_grps(float fract)
{
    for (sint32 idx = menu_asset_counts.labels - 1; idx >= 0; --idx)
    {
        UI_RECORD *rec = &spr_recs[idx + 160];
        if (rec->active)
        {
            if (rec->val >= menu_asset_counts.labels)
                return;
            hd_spr_anim(rec, fract);
        }
    }
    if (menu_notice.val == 1)
        hd_spr_rect(132, 54, 248, 120, 0, 0, 0, 0, 0, 0, 0);
    for (sint32 idx = (sint16)menu_notice.lines - 1; idx >= 0; --idx)
    {
        UI_RECORD *rec = &spr_recs[idx + 170];
        hd_spr_anim(rec, fract);
    }
}

static void hd_spr_layers(float fract)
{
    sint32 layers = menu_state.scr == 10 ? 13 : 1;
    for (sint32 layer = layers - 1; layer >= 0; --layer)
        for (sint32 idx = 159; idx >= 0; --idx)
        {
            if (menu_state.scr == 25 && idx == 57)
            {
                hd_name_wheel(fract);
                idx = 43;
                continue;
            }
            const UI_RECORD *rec = &spr_recs[idx];
            if (rec->layer == layer && rec->type && rec->flags)
                hd_spr_anim(rec, fract);
        }
}

static void hd_spr_lines(sint32 buffer)
{
    const LINE_F2 *lines = disp_line_buffer(buffer);
    if (disp_state.line_count > 200)
        abort();
    for (sint32 idx = (sint32)disp_state.line_count - 1; idx >= 0; --idx)
    {
        const LINE_F2 *line = &lines[idx];
        float dx = line->p[1].x - line->p[0].x, dy = line->p[1].y - line->p[0].y;
        float length = sqrtf(dx * dx + dy * dy);
        float nx = length ? -dy * 0.5f / length : 0.5f;
        float ny = length ? dx * 0.5f / length : 0.5f;
        float pos[4][2];
        for (uint32 corner = 0; corner < 4; ++corner)
        {
            pos[corner][0] = line->p[corner >> 1].x + (corner & 1 ? nx : -nx);
            pos[corner][1] = line->p[corner >> 1].y + (corner & 1 ? ny : -ny);
        }
        hd_draw_ui_quad(pos, NULL, 0, 0, line->rgbc.rgbc, 256, 0, 0);
    }
}

static void hd_spr_pools(sint32 buffer)
{
    if (buffer < 0 || buffer >= 2)
        abort();
    for (sint32 grp = TEXT_FONT_COUNT - 1; grp >= 0; --grp)
    {
        TEXT_FONT *font = &text_fonts[grp];
        uint32 count = font->pending_count[buffer];
        if (count > TEXT_GLYPH_CAPACITY)
            abort();
        for (uint32 idx = 0; idx < count; ++idx)
        {
            const TEXT_GLYPH *glyph = &font->pending[buffer][idx];
            const SPRT *spr = &spr_text_packets[grp][buffer][idx];
            hd_spr_rect(glyph->x, glyph->y, glyph->width, glyph->height, glyph->u, glyph->v, spr_text_modes[grp][buffer].code[0] & 0x1ff, spr->p[0].attr, spr->rgbc.rgbc, 1, (spr->rgbc.code & 2) != 0);
        }
    }
}

void hd_spr_menu(float fract, sint32 buffer)
{
    hd_spr_grps(fract);
    hd_spr_layers(fract);
    hd_spr_lines(buffer);
    hd_spr_pools(buffer);
}

sint32 spr_text_init_packets(void)
{
    static const sint16 widths[TEXT_FONT_COUNT] = {960, 910, 922};
    static const sint16 heights[TEXT_FONT_COUNT] = {256, 256, 352};
    static const sint16 clut_y[TEXT_FONT_COUNT] = {450, 445, 444};
    sint32 grp, buffer, idx;

    FUNCTION_MARKER(0x80051040, "MAIN.EXE");
    spr_register_packets();
    for (grp = 0; grp < TEXT_FONT_COUNT; ++grp)
    {
        uint16 tpage = getTPage(0, 0, widths[grp], heights[grp]);
        uint16 clut = getClut(960, clut_y[grp]);
        for (buffer = 0; buffer < 2; ++buffer)
        {
            DR_TPAGE *mode = &spr_text_modes[grp][buffer];
            setlen(mode, 1);
            mode->code[0] = psx_draw_mode(1, 0, tpage);
            for (idx = 0; idx < TEXT_GLYPH_CAPACITY; ++idx)
            {
                SPRT *packet = &spr_text_packets[grp][buffer][idx];
                setRGB0(packet, 128, 128, 128);
                packet->p[0].attr = clut;
                setSprt(packet);
            }
        }
    }
    return 0;
}

sint32 spr_text_buffer(void)
{
    sint32 buffer = disp_state.buffer;
    if (buffer < 0 || buffer >= 2)
        abort();
    return buffer;
}
