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

// Native ownership for MAIN.EXE 80038544..800387CC
static CB_NODE cb_nodes[50];
static CB_NODE *cb_free, *cb_next;
static CB_LIST cb_global;
CB_LIST cb_players[2];
static CB_LIST *cb_active = &cb_global;

void cb_init_pool(void)
{
    uint32 idx;

    FUNCTION_MARKER(0x80038544u, "MAIN.EXE");
    cb_global.head = cb_players[0].head = cb_players[1].head = NULL;
    cb_active = &cb_global;
    cb_next = NULL;
    for (idx = 0; idx < 50u; ++idx)
    {
        cb_nodes[idx].next = idx + 1u < 50u ? &cb_nodes[idx + 1u] : NULL;
        cb_nodes[idx].prev = NULL;
    }
    cb_free = cb_nodes;
}

void cb_set_active_head(CB_LIST *list)
{
    FUNCTION_MARKER(0x8003859Cu, "MAIN.EXE");
    cb_active = list ? list : &cb_global;
}

void cb_clear_list(CB_LIST *list)
{
    CB_NODE *node = list->head;

    FUNCTION_MARKER(0x800385C4u, "MAIN.EXE");
    while (node)
    {
        cb_next = node->next;
        cb_unlink_active_node(node);
        node = cb_next;
    }
}

void cb_traverse_list(CB_LIST *list)
{
    CB_NODE *node = list->head;

    FUNCTION_MARKER(0x80038610u, "MAIN.EXE");
    while (node)
    {
        cb_next = node->next;
        node->fn(node);
        node = cb_next;
    }
}

void cb_dispatch_list(void)
{
    FUNCTION_MARKER(0x8003866Cu, "MAIN.EXE");
    cb_traverse_list(cb_active);
}

void cb_clear_active_list(void)
{
    FUNCTION_MARKER(0x800386CCu, "MAIN.EXE");
    cb_clear_list(cb_active);
}

CB_NODE *cb_alloc_node(CB_FN fn)
{
    CB_NODE *node = cb_free;

    FUNCTION_MARKER(0x8003871Cu, "MAIN.EXE");
    if (!node || !fn)
        abort();
    cb_free = node->next;
    node->next = cb_active->head;
    node->prev = &cb_active->head;
    if (node->next)
        node->next->prev = &node->next;
    cb_active->head = node;
    node->fn = fn;
    node->flags = node->arg.idx = 0;
    return node;
}

void cb_unlink_active_node(CB_NODE *node)
{
    FUNCTION_MARKER(0x8003876Cu, "MAIN.EXE");
    if (!node || !node->prev)
        abort();
    if (node == cb_next)
        cb_next = node->next;
    *node->prev = node->next;
    if (node->next)
        node->next->prev = node->prev;
    node->prev = NULL;
    node->next = cb_free;
    cb_free = node;
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
