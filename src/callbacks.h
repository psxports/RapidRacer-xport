#ifndef RR_CALLBACKS_H
#define RR_CALLBACKS_H

#include "psx.h"

uint32 cb_init_pool(void);

uint32 cb_set_active_head(uint32 head);
uint32 cb_clear_list(uint32 head);
uint32 cb_traverse_list(uint32 head);
uint32 cb_dispatch_list(void);
uint32 cb_clear_active_list(void);
uint32 cb_alloc_node(uint32 callback);
uint32 cb_unlink_active_node(uint32 node);
sint32 cb_vblank_timer(void);
sint32 cb_init_vblank_timer(void);
sint32 cb_wait_vblank(void);
sint32 cb_sync_video_field(void);

void cd_handle_ready_cb(uint8 status, uint32 response);

#endif /* RR_CALLBACKS_H */
