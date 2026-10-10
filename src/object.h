#include "scene.h"
#ifndef RR_OBJECT_H
#define RR_OBJECT_H

#include "psx.h"

struct ROUTE_SEGMENT;
#include "camera.h"

enum {OBJECT_GROUP_CAPACITY = 16};
typedef struct
{
    uint16 section, status;
} OBJECT_GROUP_ENTRY;
typedef struct
{
    uint16 count, capacity;
    OBJECT_GROUP_ENTRY items[OBJECT_GROUP_CAPACITY];
    OBJECT_GROUP_ENTRY slots[OBJECT_GROUP_CAPACITY];
} OBJECT_GROUP_STATE;

OBJECT_GROUP_STATE *object_for_camera(const CAMERA_STATE *camera);
sint32 object_node_flags_update_linked(uint32 unused);
sint32 object_init_groups(OBJECT_GROUP_STATE *first, OBJECT_GROUP_STATE *second, sint32 mode);
sint32 render_reconcile_slots(OBJECT_GROUP_STATE *state, uint8 slot);
sint32 object_init_racer_object_groups(uint32 first, uint32 second, sint32 mode);
sint32 object_groups_build_linked(const struct ROUTE_SEGMENT *object, OBJECT_GROUP_STATE *output, const CAMERA_STATE *configuration);
void object_linked_lists_expand2(const SCENE_SECTION *section, uint8 slot);
void object_list_expand_linked(SCENE_GROUP_NODE *node, uint8 slot);

void object_build_prim_group_packets(SCENE_PRIM_GROUP *src, uint8 slot);

#endif /* RR_OBJECT_H */
