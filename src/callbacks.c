#include "menu.h"
#include "timer.h"
#include "psx.h"
#include "runtime.h"
#include "callbacks.h"
#include "arena.h"
#include "effects.h"
#include "title.h"

#include "global.h"
#include "sound.h"
#include "xport_trace.h"
#include <stdlib.h>

static sint32 cb_dispatch_node(uint32 callback, uint32 node)
{
    if (callback == 0x80040C6Cu)
        return effects_advance_tex_cb(node);
    if (callback == 0x80034C28u)
        return title_dispatch_state_cb(node);
    if (callback == 0x80040DD8u)
        return effects_update_transform_cb(node);
    if (callback == 0x8004199Cu)
        return effects_update_particles(node);
    return 0;
}

uint32 cb_init_pool(void)
{
    uint32 record;
    sint32 index;

    FUNCTION_MARKER(0x80038544u, "MAIN.EXE");
    w_u32(0x800B6860u, 0x800B6864u);
    w_u32(0x800B6864u, 0u);
    record = game_alloc_arena_bytes(1000);
    w_u32(0x800B6868u, record);
    w_u32(0x800B686Cu, 0u);
    for (index = 0; index < 50; ++index)
    {
        w_u32(record, record + 20u);
        record += 20u;
    }
    w_u32(record, 0u);
    return record;
}

uint32 cb_set_active_head(uint32 head)
{
    FUNCTION_MARKER(0x8003859Cu, "MAIN.EXE");
    if (head != 0u)
    {
        w_u32(0x800B6860u, head);
        return head;
    }
    w_u32(0x800B6860u, 0x800B6864u);
    return 0x800B6864u;
}

uint32 cb_clear_list(uint32 head)
{
    uint32 node;
    uint32 result;
    uint32 frame = guest_stack_push(0x20u);
    uint32 slot = frame + 0x10u;

    FUNCTION_MARKER(0x800385C4u, "MAIN.EXE");
    w_u32(0x800B686Cu, slot);
    result = slot;
    node = r_u32(head);
    while (node != 0u)
    {
        uint32 next = r_u32(node);

        w_u32(slot, next);
        result = cb_unlink_active_node(node);
        node = r_u32(slot);
    }
    guest_stack_pop(0x20u);
    return result;
}

uint32 cb_traverse_list(uint32 head)
{
    uint32 node;
    uint32 result;
    uint32 frame = guest_stack_push(0x20u);
    uint32 slot = frame + 0x10u;

    FUNCTION_MARKER(0x80038610u, "MAIN.EXE");
    w_u32(0x800B686Cu, slot);
    result = slot;
    node = r_u32(head);
    while (node != 0u)
    {
        uint32 next = r_u32(node);

        w_u32(slot, next);
        result = (uint32)cb_dispatch_node(r_u32(node + 16u), node);
        next = r_u32(slot);
        node = next;
    }
    guest_stack_pop(0x20u);
    return result;
}

uint32 cb_dispatch_list(void)
{
    uint32 head = r_u32(0x800B6860u);
    uint32 node;
    uint32 result;
    uint32 frame = guest_stack_push(0x20u);
    uint32 slot = frame + 0x10u;

    FUNCTION_MARKER(0x8003866Cu, "MAIN.EXE");
    w_u32(0x800B686Cu, slot);
    result = slot;
    node = r_u32(head);
    while (node != 0u)
    {
        uint32 next = r_u32(node);

        w_u32(slot, next);
        result = (uint32)cb_dispatch_node(r_u32(node + 16u), node);
        next = r_u32(slot);
        node = next;
    }
    guest_stack_pop(0x20u);
    return result;
}

uint32 cb_clear_active_list(void)
{
    uint32 head;
    uint32 node;
    uint32 result;
    uint32 frame = guest_stack_push(0x20u);
    uint32 slot = frame + 0x10u;

    FUNCTION_MARKER(0x800386CCu, "MAIN.EXE");
    head = r_u32(0x800B6860u);
    w_u32(0x800B686Cu, slot);
    result = slot;
    node = r_u32(head);
    while (node != 0u)
    {
        uint32 next = r_u32(node);

        w_u32(slot, next);
        result = cb_unlink_active_node(node);
        node = r_u32(slot);
    }
    guest_stack_pop(0x20u);
    return result;
}

uint32 cb_alloc_node(uint32 callback)
{
    uint32 node = r_u32(0x800B6868u);
    uint32 head = r_u32(0x800B6860u);
    uint32 next;
    uint32 first;
    uint32 linked;

    FUNCTION_MARKER(0x8003871Cu, "MAIN.EXE");
    next = r_u32(node);
    w_u32(0x800B6868u, next);
    first = r_u32(head);
    w_u32(node, first);
    w_u32(head, node);
    linked = r_u32(node);
    w_u32(node + 4u, head);
    if (linked != 0u)
        w_u32(linked + 4u, node);
    w_u32(node + 16u, callback);
    w_u32(node + 8u, 0u);
    w_u32(node + 12u, 0u);
    return node;
}

uint32 cb_unlink_active_node(uint32 node)
{
    uint32 slot;
    uint32 next;
    uint32 previous_link;
    uint32 free_head;

    FUNCTION_MARKER(0x8003876Cu, "MAIN.EXE");
    slot = r_u32(0x800B686Cu);
    if (node == r_u32(slot))
    {
        next = r_u32(node);
        w_u32(slot, next);
    }
    previous_link = r_u32(node + 4u);
    next = r_u32(node);
    w_u32(previous_link, next);
    next = r_u32(node);
    if (next != 0u)
    {
        previous_link = r_u32(node + 4u);
        w_u32(next + 4u, previous_link);
    }
    free_head = r_u32(0x800B6868u);
    w_u32(0x800B6868u, node);
    w_u32(node, free_head);
    return free_head;
}

sint32 cb_vblank_timer(void)
{
    uint32 result;

    FUNCTION_MARKER(0x800387CCu, "MAIN.EXE");
    ResetRCnt(0xF2000002u);
    result = 1u - r_u32(0x800CAE84u);
    w_u32(0x800CAE84u, result);
    w_u32(0x800CAE80u, 1u);
    return (sint32)result;
}

sint32 cb_init_vblank_timer(void)
{
    FUNCTION_MARKER(0x80038810u, "MAIN.EXE");
    w_u32(0x800CAE84u, 0u);
    menu_fn_8006776c();
    SetRCnt(0xF2000002u, 0xFFFFu, 0x2000u);
    runtime_set_vsync_cb(0x800387CCu);
    global_fn_8006777c();
    return 0;
}

sint32 cb_wait_vblank(void)
{
    sint32 result;

    FUNCTION_MARKER(0x80038874u, "MAIN.EXE");
    w_u32(0x800CAE80u, 0u);
    do
    {
        if (VSync(0) < 0)
            return 0;
        result = (sint32)r_u32(0x800CAE80u);
    } while (result == 0);
    w_u32(0x800CAE80u, 0u);
    return result;
}

sint32 cb_sync_video_field(void)
{
    sint32 result;

    FUNCTION_MARKER(0x800388A0u, "MAIN.EXE");
    w_u32(0x800CAE80u, 0u);
    VSync(0);
    result = 1 - (sint32)(r_u32(0x800CAE84u) & 1u);
    w_u32(0x800CAE84u, (uint32)result);
    return result;
}

void cd_handle_ready_cb(uint8 status, uint32 response)
{
    uint32 frame;

    FUNCTION_MARKER(0x80038950u, "MAIN.EXE");
    if (status != 1u)
    {
        w_u32(0x800B6B28u, 0u);
        return;
    }
    frame = guest_stack_push(0x20u);
    if ((r_u8(response + 4u) & 0x80u) == 0u)
    {
        uint8 minute = r_u8(response + 3u);
        uint8 second;

        w_u8(frame + 0x10u, minute);
        second = r_u8(response + 4u);
        w_u8(frame + 0x12u, 0u);
        w_u8(frame + 0x11u, second);
        w_u32(0x800B6C08u, (uint32)race_decode_time_bcd(frame + 0x10u));
    }
    {
        sint32 current = (sint32)r_u32(0x800B6C08u);
        sint32 limit = (sint32)r_u32(0x800B6AE8u);

        if (limit < current)
        {
            w_u16(0x800D6B6Au, 0u);
            w_u16(0x800D6B68u, 0u);
            w_u32(0x800D6B58u, 0xC0u);
            SpuSetVoiceAttr((SpuVoiceAttr *)psx_addr(0x800D6B58u, sizeof(SpuVoiceAttr)));
            w_u32(0x800B6B28u, 0u);
        }
    }
    guest_stack_pop(0x20u);
}
