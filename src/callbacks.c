#include "menu.h"
#include "timer.h"
#include "psx.h"
#include "callbacks.h"
#include "arena.h"
#include "effects.h"
#include "banner.h"

#include "utils.h"
#include "sound.h"
#include "xport_trace.h"
#include <stdlib.h>

// Native ownership for MAIN.EXE 80038544..800387CC
static CB_NODE cb_nodes[50];
static CB_NODE *cb_free, *cb_next;
static CB_LIST cb_global;
CB_LIST cb_players[2];
static CB_LIST *cb_active = &cb_global;
static volatile uint32 cb_field, cb_vblank_ready;

void cb_init_pool(void)
{
    uint32 idx;

    FUNCTION_MARKER(0x80038544, "MAIN.EXE");
    cb_global.head = cb_players[0].head = cb_players[1].head = NULL;
    cb_active = &cb_global;
    cb_next = NULL;
    for (idx = 0; idx < 50; ++idx)
    {
        cb_nodes[idx].next = idx + 1 < 50 ? &cb_nodes[idx + 1] : NULL;
        cb_nodes[idx].prev = NULL;
    }
    cb_free = cb_nodes;
}

void cb_set_active_head(CB_LIST *list)
{
    FUNCTION_MARKER(0x8003859C, "MAIN.EXE");
    cb_active = list ? list : &cb_global;
}

void cb_clear_list(CB_LIST *list)
{
    CB_NODE *node = list->head;

    FUNCTION_MARKER(0x800385C4, "MAIN.EXE");
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

    FUNCTION_MARKER(0x80038610, "MAIN.EXE");
    while (node)
    {
        cb_next = node->next;
        node->fn(node);
        node = cb_next;
    }
}

void cb_dispatch_list(void)
{
    FUNCTION_MARKER(0x8003866C, "MAIN.EXE");
    cb_traverse_list(cb_active);
}

void cb_clear_active_list(void)
{
    FUNCTION_MARKER(0x800386CC, "MAIN.EXE");
    cb_clear_list(cb_active);
}

CB_NODE *cb_alloc_node(CB_FN fn)
{
    CB_NODE *node = cb_free;

    FUNCTION_MARKER(0x8003871C, "MAIN.EXE");
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
    FUNCTION_MARKER(0x8003876C, "MAIN.EXE");
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

static sint32 cb_vblank_timer(void)
{
    uint32 result;

    FUNCTION_MARKER(0x800387CC, "MAIN.EXE");
    ResetRCnt(0xF2000002);
    result = 1 - cb_field;
    cb_field = result;
    cb_vblank_ready = 1;
    return result;
}

static void cb_on_vblank(void)
{
    cb_vblank_timer();
}

sint32 cb_init_vblank_timer(void)
{
    FUNCTION_MARKER(0x80038810, "MAIN.EXE");
    cb_field = 0;
    SetRCnt(0xF2000002, 0xFFFF, 0x2000);
    VSyncCallback(cb_on_vblank);
    return 0;
}

sint32 cb_wait_vblank(void)
{
    sint32 result;

    FUNCTION_MARKER(0x80038874, "MAIN.EXE");
    cb_vblank_ready = 0;
    do
    {
        if (VSync(0) < 0)
            return 0;
        result = cb_vblank_ready;
    } while (result == 0);
    cb_vblank_ready = 0;
    return result;
}

sint32 cb_sync_video_field(void)
{
    sint32 result;

    FUNCTION_MARKER(0x800388A0, "MAIN.EXE");
    cb_vblank_ready = 0;
    VSync(0);
    result = 1 - (cb_field & 1);
    cb_field = result;
    return result;
}
