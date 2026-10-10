#ifndef RR_CALLBACKS_H
#define RR_CALLBACKS_H

#include "psx.h"

typedef struct CB_NODE CB_NODE;
typedef sint32 (*CB_FN)(CB_NODE *node);

struct CB_NODE
{
    CB_NODE *next;
    CB_NODE **prev;
    CB_FN fn;
    uint32 flags;
    union
    {
        uint32 idx;
        uint32 legacy_state;
    } arg;
};

typedef struct
{
    CB_NODE *head;
} CB_LIST;

extern CB_LIST cb_players[2];

void cb_init_pool(void);
void cb_set_active_head(CB_LIST *list);
void cb_clear_list(CB_LIST *list);
void cb_traverse_list(CB_LIST *list);
void cb_dispatch_list(void);
void cb_clear_active_list(void);
CB_NODE *cb_alloc_node(CB_FN fn);
void cb_unlink_active_node(CB_NODE *node);
sint32 cb_vblank_timer(void);
sint32 cb_init_vblank_timer(void);
sint32 cb_wait_vblank(void);
sint32 cb_sync_video_field(void);

void cd_handle_ready_cb(uint8 status, uint32 response);

#endif /* RR_CALLBACKS_H */
