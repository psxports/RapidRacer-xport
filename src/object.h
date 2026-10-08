#ifndef RR_OBJECT_H
#define RR_OBJECT_H

#include "psx.h"
#include "camera.h"

sint32 object_node_flags_update_linked(uint32 unused);
sint32 object_alloc_index_groups(uint32 first, uint32 second, sint32 mode);
sint32 render_reconcile_slots(uint32 state, uint8 slot);
sint32 object_init_racer_object_groups(uint32 first, uint32 second, sint32 mode);
sint32 object_groups_build_linked(uint32 object, uint32 output, const CAMERA_STATE *configuration);
uint32 object_linked_lists_expand2(uint32 structure, uint32 output, uint8 slot);
uint32 object_list_expand_linked(uint32 list, uint32 output, uint8 slot);

uint32 object_build_prim_group_packets(uint32 source, uint32 output, uint8 slot);

#endif /* RR_OBJECT_H */
