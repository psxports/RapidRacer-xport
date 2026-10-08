#include "text.h"
#include "sprite.h"
#include "arena.h"
#include "display.h"
#include "global.h"
#include "menu.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 sprite_flush_pending(sint32 ordering_index)
{
    sint32 selector = (sint16)r_u16(0x800B6B40u);
    uint32 packet = 0x800E1C78u + 3200u * (uint32)selector;
    uint32 ordering = 0x800DD210u + 160u * (uint32)selector + 4u * (uint32)ordering_index;
    sint32 count = (sint16)r_u16(0x800B6B68u);

    FUNCTION_MARKER(0x800455C8u, "MAIN.EXE");
    if (count != 0)
    {
        do
        {
            uint32 packet_value = r_u32(packet);
            uint32 ordering_value = r_u32(ordering);

            w_u32(packet, (packet_value & 0xFF000000u) | (ordering_value & 0x00FFFFFFu));
            ordering_value = r_u32(ordering);
            w_u32(ordering, (ordering_value & 0xFF000000u) | (packet & 0x00FFFFFFu));
            count = (uint16)(r_u16(0x800B6B68u) - 1u);
            w_u16(0x800B6B68u, (uint16)count);
            packet += 16u;
        } while ((uint16)count != 0u);
    }
    return ordering_index;
}

sint32 sprite_reset_rotating_state(void)
{
    FUNCTION_MARKER(0x80046654u, "MAIN.EXE");
    w_u16(0x800B40B6u, 0u);
    w_u16(0x800B40B8u, 0u);
    return sprite_update_rotating_recs();
}

sint32 sprite_update_rotating_recs(void)
{
    sint32 phase = (uint16)r_u16(0x800B40B6u);
    uint32 base = r_u32(0x800B6A74u);
    sint32 index;

    FUNCTION_MARKER(0x8004667Cu, "MAIN.EXE");
    for (index = 0; index < 4; ++index)
    {
        ++phase;
        w_u16(0x800F0478u + 2u * (uint32)index, (uint16)phase);
        if (phase == 8)
            phase = 0;
    }
    w_u16(0x800B40B8u, (uint16)(r_u16(0x800B40B8u) + 1u));
    if ((sint16)r_u16(0x800B6BA8u) == 8)
    {
        for (index = 0; index < 8; ++index)
            w_u8(base + 84u * (uint32)index + 13444u, 0u);
        for (index = 0; index < 4; ++index)
        {
            sint16 source_index = (sint16)r_u16(0x800F0478u + 2u * (uint32)index) - 1;
            render_dispatch_rec(r_u32(0x800B6A04u) + 120u * (uint32)index, source_index, 0);
            sprite_init_anim_rec((sint16)(index + 160), r_u32(0x800B6A04u), 1, (sint16)index, (sint16)r_u16(0x800967A4u + 2u * (uint32)index), (sint16)r_u16(0x800967ACu + 2u * (uint32)index), 1);
        }
    }
    if ((sint16)r_u16(0x800B40B8u) == 6)
    {
        w_u16(0x800B40B8u, 0u);
        phase = (sint16)r_u16(0x800B40B6u) + 1;
        if (phase == 8)
            phase = 0;
        w_u16(0x800B40B6u, (uint16)phase);
        return phase;
    }
    return 6;
}

sint32 render_alloc_rec_bufs(void)
{
    FUNCTION_MARKER(0x8004684Cu, "MAIN.EXE");
    game_push_checkpoint();
    w_u32(0x800B6A48u, game_alloc_arena_bytes(600));
    w_u32(0x800B6A04u, game_alloc_arena_bytes(1200));
    w_u32(0x800B6A84u, game_alloc_arena_bytes(19200));
    w_u32(0x800B6BCCu, game_alloc_arena_bytes(1200));
    w_u16(0x800B40B2u, 1u);
    w_u16(0x800B6B68u, 0u);
    return 1;
}

sint32 render_release_rec_bufs(void)
{
    sint32 enabled;

    FUNCTION_MARKER(0x800468ACu, "MAIN.EXE");
    enabled = r_s16(0x800B40B2u);
    if (enabled != 0)
        return game_pop_checkpoint();
    return enabled;
}

sint32 menu_build_sprite_packet(uint32 output, uint32 source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    uint32 index;
    sint32 divisor;
    sint32 limit;
    sint32 visible_width;
    sint32 overflow_width;
    sint32 source_height;
    sint32 right_edge;
    uint32 record;
    uint16 selector;

    FUNCTION_MARKER(0x800468D4u, "MAIN.EXE");
    w_u32(output + 40u, 0u);
    index = r_u8(source + 30u);
    divisor = (sint16)r_u16(0x80081810u + index * 2u);
    limit = (sint16)r_u16(0x80081816u + index * 2u);
    visible_width = (sint32)((uint32)width - 1u);
    right_edge = divisor != 0 ? (sint32)((uint32)x + (uint32)(sint32)(sint16)(width / divisor)) : 0;
    if (divisor != 0 && right_edge >= limit)
    {
        sint32 clipped_width = (sint32)((uint32)limit - (uint32)x);

        overflow_width = (sint32)((uint32)visible_width - (uint32)clipped_width);
        visible_width = clipped_width;
        selector = r_u16(0x800B40B0u);
        record = r_u32(0x800B6A48u) + 120u * (uint32)(sint32)(sint16)selector;
        w_u8(record + 55u, 9u);
        w_u8(record + 59u, 46u);
        w_u8(record + 56u, 0x80u);
        w_u8(record + 57u, 0x80u);
        w_u8(record + 58u, 0x80u);
        w_u16(record + 74u, (uint16)getTPage((sint32)index, 0, (sint16)r_u16(source + 40u) + visible_width, (sint16)r_u16(source + 42u)));
        w_u16(record + 66u, (uint16)getClut((sint16)r_u16(source + 44u), (sint16)r_u16(source + 46u)));
        w_u32(record + 12u, 0u);
        w_u32(record + 8u, 0u);
        w_u32(record, 0u);
        w_u32(record + 4u, (uint32)y);
        w_u32(record + 16u, (uint32)(sint16)overflow_width);
        w_u32(record + 20u, (uint32)height);
        selector = r_u16(0x800B40B0u);
        w_u16(0x800B40B0u, (uint16)(selector + 1u));
        w_u16(output + 48u, selector);
        w_u8(record + 64u, 0u);
        w_u8(record + 65u, (uint8)y);
        w_u8(record + 72u, 0u);
        w_u8(record + 73u, (uint8)((uint32)y + (uint32)height));
        w_u8(record + 80u, (uint8)overflow_width);
        w_u8(record + 81u, (uint8)y);
        w_u8(record + 88u, (uint8)overflow_width);
        w_u8(record + 89u, (uint8)((uint32)y + (uint32)height));
    }
    else
    {
        w_u16(output + 48u, 0u);
    }

    w_u8(output + 55u, 9u);
    w_u8(output + 59u, 46u);
    w_u8(output + 56u, 0x80u);
    w_u8(output + 57u, 0x80u);
    w_u8(output + 58u, 0x80u);
    w_u16(output + 74u, r_u16(source + 52u));
    w_u16(output + 66u, r_u16(source + 54u));
    w_u32(output + 24u, (uint32)(sint16)r_u16(source + 44u));
    w_u32(output + 28u, (uint32)(sint16)r_u16(source + 46u));
    w_u32(output + 32u, (uint32)(sint16)r_u16(source + 40u));
    source_height = (sint16)r_u16(source + 42u);
    w_u32(output + 12u, 0u);
    w_u32(output + 8u, 0u);
    w_u32(output, (uint32)x);
    w_u32(output + 4u, (uint32)y);
    w_u32(output + 16u, (uint32)visible_width);
    w_u32(output + 20u, (uint32)height);
    w_u32(output + 36u, (uint32)source_height);
    w_u8(output + 64u, (uint8)x);
    w_u8(output + 65u, (uint8)y);
    w_u8(output + 72u, (uint8)x);
    w_u8(output + 73u, (uint8)((uint32)y + (uint32)height));
    w_u8(output + 80u, (uint8)((uint32)x + (uint32)visible_width));
    w_u8(output + 81u, (uint8)y);
    w_u8(output + 88u, (uint8)((uint32)x + (uint32)visible_width));
    w_u8(output + 89u, (uint8)((uint32)y + (uint32)height));
    w_u8(output + 47u, 0u);
    w_u8(output + 46u, 0u);
    w_u8(output + 44u, (uint8)index);
    return (sint32)((uint32)x + (uint32)visible_width);
}

sint32 sprite_build_draw_packet(uint32 output, uint32 source, sint32 x, sint32 y, sint32 unused5, sint32 unused6, sint32 unused7, sint32 unused8, sint32 width, sint32 height)
{
    sint32 source_height;

    FUNCTION_MARKER(0x80046B80u, "MAIN.EXE");
    w_u32(output + 40u, 1u);
    w_u8(output + 95u, 4u);
    w_u8(output + 99u, 100u);
    w_u8(output + 96u, 0x80u);
    w_u8(output + 97u, 0x80u);
    w_u8(output + 98u, 0x80u);
    w_u16(output + 106u, r_u16(source + 54u));
    w_u32(output + 24u, (uint32)(sint16)r_u16(source + 44u));
    w_u32(output + 28u, (uint32)(sint16)r_u16(source + 46u));
    w_u32(output + 32u, (uint32)(sint16)r_u16(source + 40u));
    source_height = (sint16)r_u16(source + 42u);
    w_u32(output + 12u, 0u);
    w_u32(output + 8u, 0u);
    w_u32(output, (uint32)x);
    w_u32(output + 4u, (uint32)y);
    w_u32(output + 16u, (uint32)width);
    w_u32(output + 20u, (uint32)height);
    w_u8(output + 104u, (uint8)x);
    w_u8(output + 105u, (uint8)y);
    w_u16(output + 108u, (uint16)width);
    w_u16(output + 110u, (uint16)height);
    w_u32(output + 36u, (uint32)source_height);
    w_u8(output + 47u, 0u);
    w_u8(output + 46u, 0u);
    w_u8(output + 44u, r_u8(source + 30u));
    SetDrawMode1(psx_addr(output + 112u, sizeof(DR_MODE)), 1, 0, r_u16(source + 52u));
    return (sint32)r_u32(output + 116u);
}

sint32 render_dispatch_rec(uint32 output, sint16 source_index, sint16 kind)
{
    uint32 source;

    FUNCTION_MARKER(0x80046C84u, "MAIN.EXE");
    source = r_u32(0x800EA680u + 4u * (uint32)(sint32)kind) + 60u * (uint32)(sint32)source_index;
    if (kind == 1)
        return menu_build_sprite_packet(output, source, (sint16)r_u16(source + 32u), (sint16)r_u16(source + 34u), 0, 0, 0, 0, (sint16)r_u16(source + 36u), (sint16)r_u16(source + 38u));
    return sprite_build_draw_packet(output, source, (sint16)r_u16(source + 32u), (sint16)r_u16(source + 34u), 0, 0, 0, 0, (sint16)r_u16(source + 36u), (sint16)r_u16(source + 38u));
}

uint32 sprite_init_anim_rec(sint16 index, uint32 data, sint8 type, sint16 value, sint16 first, sint16 second, sint8 flags)
{
    uint32 record;

    FUNCTION_MARKER(0x80046D18u, "MAIN.EXE");
    record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)index;
    w_u8(record + offsetof(UI_RECORD, field_50), 1u);
    w_u32(record + offsetof(UI_RECORD, data), data);
    w_u8(record + offsetof(UI_RECORD, type), (uint8)type);
    w_u16(record + offsetof(UI_RECORD, value), (uint16)value);
    w_u16(record + offsetof(UI_RECORD, x), (uint16)first);
    w_u16(record + offsetof(UI_RECORD, y), (uint16)second);
    w_u8(record + offsetof(UI_RECORD, flags), (uint8)flags);
    w_u8(record + offsetof(UI_RECORD, field_1e), 0u);
    w_u8(record + offsetof(UI_RECORD, command), 0u);
    w_u16(record + offsetof(UI_RECORD, inset_x), 0u);
    w_u16(record + offsetof(UI_RECORD, inset_y), 0u);
    w_u16(record + offsetof(UI_RECORD, field_16), 0u);
    w_u16(record + offsetof(UI_RECORD, field_18), 0u);
    w_u8(record + offsetof(UI_RECORD, field_51), 0u);
    return record;
}

sint32 render_init_recs(void)
{
    uint32 records;
    uint32 source_table;
    sint32 count;
    sint32 mode;
    sint32 index;

    FUNCTION_MARKER(0x80046DE0u, "MAIN.EXE");
    w_u16(0x800B40B0u, 0u);
    render_release_rec_bufs();
    render_alloc_rec_bufs();
    records = r_u32(0x800B6A74u);
    for (index = 0; index < 180; ++index)
    {
        w_u8(records + 84u * (uint32)index + 4u, 0u);
        w_u16(records + 84u * (uint32)index + 6u, 0u);
    }
    count = (sint16)r_u16(0x800B6BA8u);
    if (count == 8)
    {
        for (index = 0; index < 8; ++index)
            w_u8(records + 84u * (uint32)index + 13444u, 0u);
    }
    else
        for (index = 0; index < count; ++index)
        {
            render_dispatch_rec(r_u32(0x800B6A04u) + 120u * (uint32)index, (sint16)index, 0);
            sprite_init_anim_rec((sint16)(index + 160), r_u32(0x800B6A04u), 1, (sint16)index, (sint16)(index << 7), 0, 1);
        }
    count = (sint16)r_u16(0x800B6BACu);
    for (index = 0; index < count; ++index)
    {
        sint32 first = 0;
        sint32 second = 0;
        mode = (sint16)r_u16(0x800B6ABEu);
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
        render_dispatch_rec(r_u32(0x800B6BCCu) + 120u * (uint32)index, (sint16)index, 2);
        sprite_init_anim_rec((sint16)(index + 170), r_u32(0x800B6BCCu), 1, (sint16)index, (sint16)first, (sint16)second, 1);
    }
    source_table = r_u32(0x800EA684u);
    count = (sint16)r_u16(0x800B6BAAu);
    for (index = 0; index < count; ++index)
    {
        render_dispatch_rec(r_u32(0x800B6A84u) + 120u * (uint32)index, (sint16)index, 1);
        sprite_init_anim_rec((sint16)index, r_u32(0x800B6A84u), 1, (sint16)index, (sint16)r_u16(source_table + 60u * (uint32)index + 48u), (sint16)r_u16(source_table + 60u * (uint32)index + 50u), 1);
    }
    return sprite_update_rotating_recs();
}

sint32 scene_release_render_bufs(void)
{
    FUNCTION_MARKER(0x8004715Cu, "MAIN.EXE");
    return render_release_rec_bufs();
}

sint32 sprite_config_prim(uint32 state, sint32 x, sint32 y)
{
    uint32 width = r_u32(state + 16u);
    sint32 kind = (sint32)r_u32(state + 40u);
    uint32 height = r_u32(state + 20u);
    sint32 bottom;
    sint32 right;

    FUNCTION_MARKER(0x8004717Cu, "MAIN.EXE");
    if (kind != 0)
    {
        if (kind == 1)
        {
            w_u16(state + 100u, (uint16)x);
            w_u16(state + 102u, (uint16)y);
        }
        return 1;
    }
    bottom = (sint32)((uint32)y + height);
    right = (sint32)((uint32)x + width);
    w_u16(state + 60u, (uint16)x);
    w_u16(state + 62u, (uint16)y);
    w_u16(state + 68u, (uint16)x);
    w_u16(state + 70u, (uint16)bottom);
    w_u16(state + 76u, (uint16)right);
    w_u16(state + 78u, (uint16)y);
    w_u16(state + 84u, (uint16)right);
    w_u16(state + 86u, (uint16)bottom);
    return right;
}

uint32 sprite_set_orient(uint32 owner, sint8 orientation)
{
    sint16 record_index;
    uint32 record;

    FUNCTION_MARKER(0x800471D8u, "MAIN.EXE");
    record_index = (sint16)r_u16(owner + 6u);
    record = r_u32(owner) + 120u * (uint32)(sint32)record_index;
    w_u8(record + 46u, (uint8)orientation);
    return record;
}

sint32 sprite_config_uv_orientation(uint32 state)
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
    orientation = r_u8(state + 46u);
    width = r_u32(state + 16u);
    x_base = r_u32(state);
    height = r_u32(state + 20u);
    x0 = (uint8)x_base;
    x1 = (uint8)(width + x_base);
    w_u8(state + 47u, orientation);
    y_base = r_u32(state + 4u);
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
    w_u8(state + 64u, x[0]);
    w_u8(state + 65u, y[0]);
    w_u8(state + 72u, x[1]);
    w_u8(state + 73u, y[1]);
    w_u8(state + 80u, x[2]);
    w_u8(state + 81u, y[2]);
    w_u8(state + 88u, x[3]);
    w_u8(state + 89u, y[3]);
    return y[3];
}

sint32 sprite_submit_anim(uint32 state, sint32 x, sint32 y, sint32 ordering_index)
{
    sint32 selector = (sint16)r_u16(0x800B6B40u);
    uint32 ordering = 0x800DD210u + 160u * (uint32)selector + 4u * (uint32)ordering_index;
    uint32 primitive;
    sint32 kind;
    uint32 primitive_value;
    uint32 ordering_value;
    uint8 current_frame;
    uint8 last_frame;

    FUNCTION_MARKER(0x800473A4u, "MAIN.EXE");
    sprite_config_prim(state, x, y);
    kind = (sint32)r_u32(state + 40u);
    if (kind == 1)
    {
        primitive = state + 92u;
        primitive_value = r_u32(primitive);
        ordering_value = r_u32(ordering);
        w_u32(primitive, (primitive_value & 0xFF000000u) | (ordering_value & 0x00FFFFFFu));
        ordering_value = r_u32(ordering);
        w_u32(ordering, (ordering_value & 0xFF000000u) | (primitive & 0x00FFFFFFu));
        primitive = state + 112u;
        primitive_value = r_u32(primitive);
        ordering_value = r_u32(ordering);
        w_u32(primitive, (primitive_value & 0xFF000000u) | (ordering_value & 0x00FFFFFFu));
        ordering_value = r_u32(ordering);
        w_u32(ordering, (ordering_value & 0xFF000000u) | (primitive & 0x00FFFFFFu));
    }
    else if (kind == 0)
    {
        primitive = state + 52u;
        primitive_value = r_u32(primitive);
        ordering_value = r_u32(ordering);
        w_u32(primitive, (primitive_value & 0xFF000000u) | (ordering_value & 0x00FFFFFFu));
        ordering_value = r_u32(ordering);
        w_u32(ordering, (ordering_value & 0xFF000000u) | (primitive & 0x00FFFFFFu));
    }
    current_frame = r_u8(state + 46u);
    last_frame = r_u8(state + 47u);
    if (current_frame != last_frame)
        return sprite_config_uv_orientation(state);
    return last_frame;
}

sint32 sprite_apply_prim_vram_mask(uint32 owner)
{
    sint16 record_index = (sint16)r_u16(owner + 6u);
    uint32 record = r_u32(owner) + 120u * (uint32)(sint32)record_index;
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;
    uint8 mode;
    uint8 flags;
    uint32 tpage_x;
    uint32 tpage_y;

    FUNCTION_MARKER(0x80047454u, "MAIN.EXE");
    mode = r_u8(record + 44u);
    if (mode != 0u)
        return mode;
    rectangle.x = (sint16)r_u16(record + 24u);
    rectangle.y = (sint16)r_u16(record + 28u);
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    pixels[0] &= UINT16_C(0x7FFF);
    for (index = 1u; index < 16u; ++index)
        pixels[index] |= UINT16_C(0x8000);
    LoadImagePSX(&rectangle, (uint32 *)pixels);
    mode = r_u8(record + 44u);
    tpage_x = r_u32(record + 32u);
    tpage_y = r_u32(record + 36u);
    w_u16(record + 74u, (uint16)getTPage(mode, 3, (sint16)tpage_x, (sint16)tpage_y));
    flags = r_u8(record + 59u);
    flags = (uint8)(flags | 2u);
    w_u8(record + 59u, flags);
    return flags;
}

sint32 sprite_enable_semitransparency(uint32 owner)
{
    uint32 record = r_u32(owner) + 120u * (uint32)(sint32)(sint16)r_u16(owner + 6u);
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;

    FUNCTION_MARKER(0x80047548u, "MAIN.EXE");
    if (r_u8(record + 44u) != 0u)
        return r_u8(record + 44u);
    rectangle.x = (sint16)r_u16(record + 24u);
    rectangle.y = (sint16)r_u16(record + 28u);
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    pixels[0] &= UINT16_C(0x7FFF);
    for (index = 1u; index < 16u; ++index)
        pixels[index] |= UINT16_C(0x8000);
    LoadClut((uint32 *)pixels, (sint32)r_u32(record + 24u), (sint32)r_u32(record + 28u));
    w_u16(record + 74u, (uint16)getTPage(r_u8(record + 44u), 0, (sint16)r_u32(record + 32u), (sint16)r_u32(record + 36u)));
    xport_update_u8(record + 59u, XPORT_MEMORY_UPDATE_OR, 2u);
    return r_u8(record + 59u);
}

sint32 sprite_disable_semitransparency(uint32 owner)
{
    uint32 record = r_u32(owner) + 120u * (uint32)(sint32)(sint16)r_u16(owner + 6u);
    PSX_RECT rectangle;
    uint16 pixels[16];
    uint32 index;

    FUNCTION_MARKER(0x8004763Cu, "MAIN.EXE");
    if (r_u8(record + 44u) != 0u)
        return r_u8(record + 44u);
    rectangle.x = (sint16)r_u16(record + 24u);
    rectangle.y = (sint16)r_u16(record + 28u);
    rectangle.w = 16;
    rectangle.h = 1;
    StoreImage(&rectangle, (uint32 *)pixels);
    DrawSync(0);
    for (index = 1u; index < 16u; ++index)
        pixels[index] &= UINT16_C(0x7FFF);
    LoadClut((uint32 *)pixels, (sint32)r_u32(record + 24u), (sint32)r_u32(record + 28u));
    DrawSync(0);
    w_u16(record + 74u, (uint16)getTPage(r_u8(record + 44u), 0, (sint16)r_u32(record + 32u), (sint16)r_u32(record + 36u)));
    xport_update_u8(record + 59u, XPORT_MEMORY_UPDATE_AND, (uint8)~2u);
    return r_u8(record + 59u);
}

sint32 sprite_clear_anim_recs(void)
{
    uint32 base;
    uint32 index;

    FUNCTION_MARKER(0x80047724u, "MAIN.EXE");
    base = r_u32(0x800B6A74u);
    for (index = 0u; index < 180u; ++index)
    {
        uint32 record = base + 84u * index;
        w_u8(record + 12u, 0u);
        w_u16(record + 28u, 0u);
        w_u16(record + 24u, 0u);
        w_u16(record + 26u, 0u);
        w_u16(record + 22u, 0u);
    }
    return 0;
}

sint32 sprite_apply_inset(uint32 owner)
{
    uint32 record = r_u32(owner + offsetof(UI_RECORD, data)) + 120u * (uint32)(uint16)r_u16(owner + offsetof(UI_RECORD, value));
    sint32 delta_x = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_x));
    sint32 delta_y = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_y));
    sint32 result = delta_y;

    FUNCTION_MARKER(0x800477ACu, "MAIN.EXE");
    if (delta_x != 0)
    {
        w_u16(owner + offsetof(UI_RECORD, x), (uint16)((sint16)r_u16(owner + offsetof(UI_RECORD, x)) + delta_x));
        xport_update_u32(record + 16u, XPORT_MEMORY_UPDATE_SUBTRACT, (uint32)(2 * delta_x));
    }
    if (delta_y != 0)
    {
        w_u16(owner + offsetof(UI_RECORD, y), (uint16)((sint16)r_u16(owner + offsetof(UI_RECORD, y)) + delta_y));
        xport_update_u32(record + 20u, XPORT_MEMORY_UPDATE_SUBTRACT, (uint32)(2 * delta_y));
        result = (sint32)r_u32(record + 20u);
    }
    w_u8(owner + offsetof(UI_RECORD, command), 0u);
    w_u16(owner + offsetof(UI_RECORD, field_16), 0u);
    w_u16(owner + offsetof(UI_RECORD, field_18), 0u);
    w_u16(owner + offsetof(UI_RECORD, inset_x), 0u);
    w_u16(owner + offsetof(UI_RECORD, inset_y), 0u);
    return result;
}

sint32 menu_move_sprite_rec(uint32 owner, sint8 command, sint16 first, sint16 second, sint32 step_x, sint32 step_y, sint32 unused7, sint32 unused8, sint16 unused9, sint16 unused10)
{
    sint32 index;
    sint32 delta_x;
    sint32 delta_y;
    uint32 record;
    sint32 result;

    FUNCTION_MARKER(0x800478DCu, "MAIN.EXE");
    index = (sint16)r_u16(owner + offsetof(UI_RECORD, value));
    delta_x = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_x));
    record = r_u32(owner + offsetof(UI_RECORD, data)) + 120u * (uint32)index;
    if (delta_x != 0)
    {
        uint16 position = (uint16)(r_u16(owner + offsetof(UI_RECORD, x)) + (uint32)delta_x);
        uint32 sprite_position;

        delta_x = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_x));
        w_u16(owner + offsetof(UI_RECORD, x), position);
        sprite_position = r_u32(record + 16u) - (uint32)delta_x;
        w_u32(record + 16u, sprite_position);
        delta_x = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_x));
        sprite_position -= (uint32)delta_x;
        w_u32(record + 16u, sprite_position);
    }
    delta_y = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_y));
    result = delta_y;
    if (delta_y != 0)
    {
        uint16 position = (uint16)(r_u16(owner + offsetof(UI_RECORD, y)) + (uint32)delta_y);
        uint32 sprite_position;

        delta_y = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_y));
        w_u16(owner + offsetof(UI_RECORD, y), position);
        sprite_position = r_u32(record + 20u) - (uint32)delta_y;
        w_u32(record + 20u, sprite_position);
        delta_y = (sint16)r_u16(owner + offsetof(UI_RECORD, inset_y));
        sprite_position -= (uint32)delta_y;
        w_u32(record + 20u, sprite_position);
        result = (sint32)sprite_position;
    }
    w_u8(owner + offsetof(UI_RECORD, command), (uint8)command);
    w_u8(owner + offsetof(UI_RECORD, field_0d), 0u);
    w_u16(owner + offsetof(UI_RECORD, step_x), (uint16)step_x);
    w_u16(owner + offsetof(UI_RECORD, step_y), (uint16)step_y);
    w_u16(owner + offsetof(UI_RECORD, inset_y), 0u);
    w_u16(owner + offsetof(UI_RECORD, field_18), 0u);
    w_u16(owner + offsetof(UI_RECORD, inset_x), 0u);
    w_u16(owner + offsetof(UI_RECORD, field_16), 0u);
    w_u16(owner + offsetof(UI_RECORD, target_first), (uint16)first);
    w_u16(owner + offsetof(UI_RECORD, target_second), (uint16)second);
    return result;
}

sint32 sprite_init_trans(uint32 state, sint32 target_x, sint16 target_y, sint16 target_first, sint16 target_second, sint16 duration)
{
    uint16 x = r_u16(state + 8u);
    uint16 x_offset = r_u16(state + 26u);
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
    w_u8(state + 30u, 1u);
    y = r_u16(state + 10u);
    y_offset = r_u16(state + 28u);
    w_u16(state + 36u, (uint16)target_x);
    w_u16(state + 38u, (uint16)target_y);
    w_u32(state + 40u, 0u);
    w_u32(state + 44u, 0u);
    current_x = (uint32)x + (uint32)x_offset;
    current_y = (uint32)y + (uint32)y_offset;
    delta_x = (sint16)(uint16)((uint32)target_x - current_x);
    delta_y = (sint16)(uint16)((uint32)(sint32)target_y - current_y);
    w_u16(state + 34u, (uint16)current_y);
    w_u8(state + 31u, (uint8)duration);
    w_u16(state + 32u, (uint16)current_x);
    if (delta_x != 0)
        velocity_x = math_div_s32((sint32)((uint32)(uint16)delta_x << 16), divisor);
    if (delta_y != 0)
        velocity_y = math_div_s32((sint32)((uint32)(uint16)delta_y << 16), divisor);
    first = r_u16(state + 22u);
    second = r_u16(state + 24u);
    w_u32(state + 52u, (uint32)velocity_y);
    w_u32(state + 48u, (uint32)velocity_x);
    w_u16(state + 56u, first);
    first = r_u16(state + 22u);
    w_u16(state + 60u, (uint16)target_first);
    w_u16(state + 62u, (uint16)target_second);
    w_u32(state + 64u, 0u);
    w_u32(state + 68u, 0u);
    w_u16(state + 58u, second);
    second = r_u16(state + 24u);
    delta_x = (sint16)(uint16)((uint32)(sint32)target_first - (uint32)first);
    delta_y = (sint16)(uint16)((uint32)(sint32)target_second - (uint32)second);
    if (delta_x != 0)
        first_velocity = math_div_s32((sint32)((uint32)(uint16)delta_x << 16), divisor);
    result = delta_y;
    w_u32(state + 72u, (uint32)first_velocity);
    if (delta_y != 0)
    {
        result = (sint32)((uint32)(uint16)delta_y << 16);
        second_velocity = math_div_s32(result, divisor);
    }
    w_u32(state + 76u, (uint32)second_velocity);
    return result;
}

sint32 sprite_copy_anim_rec_bytes(uint32 source, uint32 destination)
{
    uint32 index;
    sint32 result = 0;

    FUNCTION_MARKER(0x80047B7Cu, "MAIN.EXE");
    for (index = 0u; index < 84u; ++index)
    {
        result = r_u8(source + index);
        w_u8(destination + index, (uint8)result);
    }
    return result;
}

sint32 sprite_update_anims(void)
{
    uint32 records = r_u32(0x800B6A74u);
    uint32 index;

    FUNCTION_MARKER(0x80047BA4u, "MAIN.EXE");
    for (index = 0u; index < 180u; ++index)
    {
        uint32 record = records + 84u * index;
        uint32 sprite;
        sint32 delta_x;
        sint32 delta_y;

        if (r_u8(record + 80u) == 0u)
            continue;
        {
            sint16 sprite_index = (sint16)r_u16(record + 6u);
            uint32 sprite_base = r_u32(record);

            sprite = sprite_base + 120u * (uint32)(sint32)sprite_index;
        }
        delta_x = (sint16)r_u16(record + 26u);
        delta_y = (sint16)r_u16(record + 28u);
        if (delta_x != 0)
        {
            uint16 position = r_u16(record + 8u);
            uint32 sprite_position;

            position = (uint16)(position + delta_x);
            delta_x = (sint16)r_u16(record + 26u);
            w_u16(record + 8u, position);
            sprite_position = r_u32(sprite + 16u) - (uint32)delta_x;
            w_u32(sprite + 16u, sprite_position);
            delta_x = (sint16)r_u16(record + 26u);
            sprite_position -= (uint32)delta_x;
            w_u32(sprite + 16u, sprite_position);
        }
        if (delta_y != 0)
        {
            uint16 position = r_u16(record + 10u);
            uint32 sprite_position;

            position = (uint16)(position + delta_y);
            delta_y = (sint16)r_u16(record + 28u);
            w_u16(record + 10u, position);
            sprite_position = r_u32(sprite + 20u) - (uint32)delta_y;
            w_u32(sprite + 20u, sprite_position);
            delta_y = (sint16)r_u16(record + 28u);
            sprite_position -= (uint32)delta_y;
            w_u32(sprite + 20u, sprite_position);
        }
        if (r_u8(record + 12u) == 1u)
        {
            if (r_u8(record + 13u) != 0u)
            {
                sint16 value = (sint16)r_u16(record + 22u);

                if (value != 0)
                    w_u16(record + 22u, (uint16)((uint32)value - r_u16(record + 14u)));
                value = (sint16)r_u16(record + 24u);
                if (value != 0)
                    w_u16(record + 24u, (uint16)((uint32)value - r_u16(record + 16u)));
                value = (sint16)r_u16(record + 22u);
                if (value == 0 && (sint16)r_u16(record + 24u) == 0)
                {
                    w_u8(record + 13u, 0u);
                    w_u8(record + 12u, 0u);
                }
            }
            else
            {
                sint16 step = (sint16)r_u16(record + 14u);

                if (step >= 0)
                {
                    sint16 value = (sint16)r_u16(record + 22u);
                    sint16 target = (sint16)r_u16(record + 18u);

                    if (value != target)
                        w_u16(record + 22u, (uint16)(value + step));
                }
                step = (sint16)r_u16(record + 16u);
                if (step >= 0)
                {
                    sint16 value = (sint16)r_u16(record + 24u);
                    sint16 target = (sint16)r_u16(record + 20u);

                    if (value != target)
                        w_u16(record + 24u, (uint16)(value + step));
                }
                if ((sint16)r_u16(record + 22u) == (sint16)r_u16(record + 18u) && (sint16)r_u16(record + 24u) == (sint16)r_u16(record + 20u))
                    w_u8(record + 13u, 1u);
            }
        }
        if (r_u8(record + 30u) == 1u)
        {
            uint8 remaining = r_u8(record + 31u);
            uint32 first_position = r_u32(record + 40u);
            uint32 first_step = r_u32(record + 48u);
            uint32 second_position = r_u32(record + 44u);
            uint32 second_step = r_u32(record + 52u);
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
            w_u32(record + 44u, second_position);
            w_u32(record + 40u, first_position);
            base_y = r_u16(record + 34u);
            third_step = r_u32(record + 72u);
            fourth_step = r_u32(record + 76u);
            third_position = r_u32(record + 64u);
            --remaining;
            w_u8(record + 31u, remaining);
            w_u16(record + 10u, (uint16)((sint16)(second_position >> 16) + base_y));
            fourth_position = r_u32(record + 68u);
            third_position += third_step;
            base_x = (sint16)r_u16(record + 42u);
            fourth_position += fourth_step;
            w_u32(record + 68u, fourth_position);
            w_u32(record + 64u, third_position);
            base_fourth = r_u16(record + 58u);
            base_first = r_u16(record + 32u);
            w_u16(record + 24u, (uint16)((sint16)(fourth_position >> 16) + base_fourth));
            base_third = (sint16)r_u16(record + 66u);
            base_third_low = r_u16(record + 56u);
            w_u16(record + 8u, (uint16)(base_x + base_first));
            remaining = r_u8(record + 31u);
            w_u16(record + 22u, (uint16)(base_third + base_third_low));
            if (remaining == 0u)
            {
                uint16 final_x = r_u16(record + 36u);
                uint16 final_y = r_u16(record + 38u);
                uint16 final_first = r_u16(record + 60u);
                uint16 final_second = r_u16(record + 62u);

                w_u8(record + 30u, 0u);
                w_u32(record + 64u, 0u);
                w_u32(record + 68u, 0u);
                w_u32(record + 72u, 0u);
                w_u32(record + 76u, 0u);
                w_u16(record + 8u, final_x);
                w_u16(record + 10u, final_y);
                w_u16(record + 22u, final_first);
                w_u16(record + 24u, final_second);
            }
        }
        if (r_u8(record + 12u) != 0u)
        {
            delta_x = (sint16)r_u16(record + 22u);
            delta_y = (sint16)r_u16(record + 24u);
            if (delta_x != 0)
            {
                uint16 position = r_u16(record + 8u);
                uint32 sprite_position;

                position = (uint16)(position - delta_x);
                delta_x = (sint16)r_u16(record + 22u);
                w_u16(record + 8u, position);
                sprite_position = r_u32(sprite + 16u) + (uint32)delta_x;
                w_u32(sprite + 16u, sprite_position);
                delta_x = (sint16)r_u16(record + 22u);
                sprite_position += (uint32)delta_x;
                w_u32(sprite + 16u, sprite_position);
            }
            if (delta_y != 0)
            {
                uint16 position = r_u16(record + 10u);
                uint32 sprite_position;

                position = (uint16)(position - delta_y);
                delta_y = (sint16)r_u16(record + 24u);
                w_u16(record + 10u, position);
                sprite_position = r_u32(sprite + 20u) + (uint32)delta_y;
                w_u32(sprite + 20u, sprite_position);
                delta_y = (sint16)r_u16(record + 24u);
                sprite_position += (uint32)delta_y;
                w_u32(sprite + 20u, sprite_position);
            }
        }
        {
            uint16 next_delta_x = r_u16(record + 22u);
            uint16 next_delta_y = r_u16(record + 24u);

            w_u16(record + 26u, next_delta_x);
            w_u16(record + 28u, next_delta_y);
        }
    }
    return 0;
}

sint32 sprite_submit_layered_recs(sint32 ordering_index)
{
    sint32 layers = (sint16)r_u16(0x800B413Cu) == 10 ? 13 : 1;
    sint32 layer;
    sint32 index;
    sint32 drew = 0;

    FUNCTION_MARKER(0x80047FDCu, "MAIN.EXE");
    for (layer = 0; layer < layers; ++layer)
    {
        for (index = 0; index < 160; ++index)
        {
            uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint16)index;
            if (r_u8(record + 81u) == (uint8)layer && r_u8(record + 4u) != 0u && r_u8(record + 5u) != 0u)
            {
                sint32 source_index = (sint16)r_u16(record + 6u);
                sint32 x = (sint16)r_u16(record + 8u);
                sint32 y = (sint16)r_u16(record + 10u);
                uint32 sprite = r_u32(record);

                sprite_submit_anim(sprite + 120u * (uint32)source_index, x, y, (sint16)ordering_index);
                drew = 1;
            }
        }
    }
    return (sint16)((uint32)ordering_index + (uint32)drew);
}

sint32 sprite_submit_groups(sint32 ordering_index)
{
    sint32 count = (sint16)r_u16(0x800B6BA8u);
    sint32 source_bound = (sint16)r_u16(0x800B6BA8u);
    uint8 index = 0u;
    uint8 drew = 0u;

    FUNCTION_MARKER(0x80048204u, "MAIN.EXE");
    if (count > 0)
    {
        do
        {
            uint32 record = r_u32(0x800B6A74u) + 84u * ((uint32)index + 160u);
            uint8 active = r_u8(record + 80u);

            index = (uint8)(index + 1u);
            if (active != 0u)
            {
                sint32 source_index = (sint16)r_u16(record + 6u);
                sint32 x;
                uint32 sprite;
                sint32 y;

                if (source_index >= source_bound)
                    return 0;
                x = (sint16)r_u16(record + 8u);
                sprite = r_u32(record);
                y = (sint16)r_u16(record + 10u);
                sprite_submit_anim(sprite + 120u * (uint32)source_index, x, y, (sint16)ordering_index);
                drew = 1u;
            }
            count = (sint16)r_u16(0x800B6BA8u);
            source_bound = (sint16)r_u16(0x800B6BA8u);
        } while ((sint32)index < count);
    }
    if (drew != 0u)
    {
        drew = 0u;
        ordering_index = (sint32)((uint32)ordering_index + 1u);
    }
    if ((sint16)r_u16(0x800B4264u) == 1)
    {
        sint32 next_ordering = (sint32)((uint32)ordering_index + 1u);
        sint32 selector = (sint16)r_u16(0x800B6B40u);
        uint32 ordering = 0x800DD210u + 160u * (uint32)selector + 4u * (uint32)(sint16)next_ordering;
        uint32 tile = 0x800D6BC8u + 16u * (uint32)selector;
        uint32 tile_value;
        uint32 ordering_value;

        ordering_index = (sint32)((uint32)next_ordering + 1u);
        w_u16(tile + 8u, 132u);
        w_u16(tile + 10u, 54u);
        w_u16(tile + 12u, 248u);
        w_u16(tile + 14u, 120u);
        w_u8(tile + 3u, 3u);
        w_u8(tile + 5u, 0u);
        w_u8(tile + 6u, 0u);
        w_u8(tile + 4u, 0u);
        w_u8(tile + 7u, 96u);
        tile_value = r_u32(tile);
        ordering_value = r_u32(ordering);
        w_u32(tile, (tile_value & 0xFF000000u) | (ordering_value & 0x00FFFFFFu));
        ordering_value = r_u32(ordering);
        w_u32(ordering, (ordering_value & 0xFF000000u) | (tile & 0x00FFFFFFu));
    }
    count = (sint16)r_u16(0x800B6BACu);
    if (count > 0)
    {
        index = 0u;
        do
        {
            uint8 current = index;
            sint16 record_index = (sint16)((uint32)current + 170u);
            sint16 first = (sint16)(((uint32)current << 7) + 128u);
            sint16 second = 50;
            sint32 mode = (sint16)((uint16)(r_u16(0x800B6ABEu) - 1u));
            uint32 data;
            uint32 record;
            sint32 source_index;
            uint32 sprite;

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
            data = r_u32(0x800B6BCCu);
            render_dispatch_rec(data + 120u * (uint32)current, (sint16)current, 2);
            data = r_u32(0x800B6BCCu);
            sprite_init_anim_rec(record_index, data, 1, (sint16)current, first, second, 1);
            record = r_u32(0x800B6A74u) + 84u * (uint32)record_index;
            source_index = (sint16)r_u16(record + 6u);
            sprite = r_u32(record);
            sprite_submit_anim(sprite + 120u * (uint32)source_index, first, second, (sint16)ordering_index);
            count = (sint16)r_u16(0x800B6BACu);
        } while ((sint32)index < count);
    }
    if (drew != 0u)
        ordering_index = (sint32)((uint32)ordering_index + 1u);
    return (sint16)ordering_index;
}

sint32 sprite_build_tex_desc_grid(void)
{
    static const uint32 descriptor_bases[3] = {0x80091B04u, 0x80091C6Cu, 0x80091DD4u};
    uint32 group;

    FUNCTION_MARKER(0x80048B30u, "MAIN.EXE");
    for (group = 0u; group < 3u; ++group)
    {
        sint32 outer;
        uint32 descriptor_index = 0u;
        for (outer = (sint32)group; outer < (sint32)group + 3; ++outer)
        {
            sint32 row;
            for (row = 0; row < 6; ++row, ++descriptor_index)
            {
                uint32 descriptor = descriptor_bases[group] + 20u * descriptor_index;
                uint32 output = r_u32(descriptor + 4u);
                sint32 column;
                w_u16(descriptor + 2u, (uint16)(r_u16(0x800FF708u + 2u * (uint32)(outer + 5 * row)) + 1u));
                for (column = 0; column < 5; ++column)
                {
                    uint32 source = 0x800DCC40u + 6u * (uint32)(25 * row + 5 * outer + column);
                    w_u16(output, r_u16(source));
                    w_u16(output + 2u, r_u16(source + 2u));
                    w_u16(output + 4u, r_u16(source + 4u));
                    w_u16(output + 6u, r_u16(0x800F5B68u + 2u * (uint32)(5 * row + column)));
                    output += 8u;
                }
            }
        }
    }
    return 0;
}

sint32 sprite_submit_pools(sint32 ordering_index)
{
    static const uint32 packet_addresses[3] = {0x800D71F0u, 0x800D7200u, 0x800D7210u};
    static const uint32 record_bases[3] = {0x800EA690u, 0x800EC5D0u, 0x800EE518u};
    sint32 group;

    FUNCTION_MARKER(0x80051C18u, "MAIN.EXE");
    for (group = 2; group >= 0; --group)
    {
        sint32 buffer = sprite_text_buffer();
        uint16 count = (uint16)text_fonts[group].pending_count[buffer];

        if (count != 0u)
        {
            size_t glyph;
            for (glyph = 0; glyph < count; ++glyph)
            {
                const TEXT_GLYPH *source = &text_fonts[group].pending[buffer][glyph];
                uint32 destination = record_bases[group] + 4000u * (uint32)buffer + 20u * (uint32)glyph;
                w_u16(destination + 8u, (uint16)source->x);
                w_u16(destination + 10u, (uint16)source->y);
                w_u8(destination + 12u, source->u);
                w_u8(destination + 13u, source->v);
                w_u16(destination + 16u, source->width);
                w_u16(destination + 18u, source->height);
            }
        }
        if (count != 0u)
        {
            uint32 record = record_bases[group] + 4000u * (uint32)buffer + 20u * (uint32)((sint32)(sint16)count - 1);
            uint32 ordering = 0x800DD210u + 160u * (uint32)buffer + 4u * (uint32)(sint16)ordering_index;
            uint32 packet = packet_addresses[group] + 8u * (uint32)(sint32)buffer;
            uint32 packet_value = r_u32(packet);
            uint32 ordering_value = r_u32(ordering);

            w_u32(packet, (packet_value & UINT32_C(0xFF000000)) | (ordering_value & UINT32_C(0x00FFFFFF)));
            ordering_value = r_u32(ordering);
            w_u32(ordering, (ordering_value & UINT32_C(0xFF000000)) | (packet & UINT32_C(0x00FFFFFF)));
            ordering_index = (sint32)((uint32)ordering_index + 1u);
            buffer = (sint16)r_u16(0x800B6B40u);
            ordering = 0x800DD210u + 160u * (uint32)buffer + 4u * (uint32)(sint16)ordering_index;
            do
            {
                packet_value = r_u32(record);
                ordering_value = r_u32(ordering);
                w_u32(record, (packet_value & UINT32_C(0xFF000000)) | (ordering_value & UINT32_C(0x00FFFFFF)));
                ordering_value = r_u32(ordering);
                w_u32(ordering, (ordering_value & UINT32_C(0xFF000000)) | (record & UINT32_C(0x00FFFFFF)));
                count = (uint16)(count - 1u);
                record -= 20u;
            } while (count != 0u);
            ordering_index = (sint32)((uint32)ordering_index + 1u);
            buffer = (sint16)r_u16(0x800B6B40u);
            text_fonts[group].pending_count[buffer] = 0;
        }
    }
    return (sint16)ordering_index;
}

sint32 sprite_text_init_packets(void)
{
    static const uint32 first_buffers[3] = {0x800EA690u, 0x800EC5D0u, 0x800EE518u};
    static const uint32 second_buffers[3] = {0x800EB630u, 0x800ED570u, 0x800EF4B8u};
    static const uint32 draw_packets[3] = {0x800D71F0u, 0x800D7200u, 0x800D7210u};
    static const sint16 widths[3] = {960, 910, 922};
    static const sint16 heights[3] = {256, 256, 352};
    static const sint16 clut_y[3] = {450, 445, 444};
    sint32 group;
    sint32 index;

    FUNCTION_MARKER(0x80051040u, "MAIN.EXE");
    for (group = 0; group < 3; ++group)
    {
        uint16 tpage;
        uint16 clut;
        uint32 packet;
        tpage = (uint16)getTPage(0, 0, widths[group], heights[group]);
        clut = (uint16)getClut(960, clut_y[group]);
        packet = draw_packets[group];
        w_u8(packet + 3u, 1u);
        w_u32(packet + 4u, UINT32_C(0xE1000400) | tpage);
        w_u8(packet + 11u, 1u);
        w_u32(packet + 12u, UINT32_C(0xE1000400) | tpage);
        for (index = 0; index < 200; ++index)
        {
            uint32 first = first_buffers[group] + 20u * (uint32)index;
            uint32 second = second_buffers[group] + 20u * (uint32)index;
            w_u8(first + 4u, 128u);
            w_u8(first + 5u, 128u);
            w_u8(first + 6u, 128u);
            w_u16(first + 14u, clut);
            w_u8(first + 3u, 4u);
            w_u8(first + 7u, 100u);
            w_u8(second + 4u, 128u);
            w_u8(second + 5u, 128u);
            w_u8(second + 6u, 128u);
            w_u16(second + 14u, clut);
            w_u8(second + 3u, 4u);
            w_u8(second + 7u, 100u);
        }
    }
    return 0;
}

sint32 sprite_text_buffer(void)
{
    sint32 buffer = r_s16(0x800B6B40u);
    if (buffer < 0 || buffer >= 2)
        abort();
    return buffer;
}
