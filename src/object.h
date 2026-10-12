#ifndef OBJECT_H
#define OBJECT_H
#include "track.h"

#include "psx.h"

struct RACE_VIEW;

struct ROUTE_SEGMENT;
#include "camera.h"

enum
{
    OBJECT_GROUP_CAPACITY = 16
};

typedef struct
{
    uint16 section, status;
} OBJECT_GROUP_ENTRY;

typedef struct
{
    uint16 count, capacity;
    OBJECT_GROUP_ENTRY items[OBJECT_GROUP_CAPACITY];
    OBJECT_GROUP_ENTRY slots[OBJECT_GROUP_CAPACITY];
} OBJECT_GROUP;

OBJECT_GROUP *object_for_camera(const CAMERA *camera);
sint32 object_node_flags_update_linked(uint32 unused);

sint32 render_reconcile_slots(OBJECT_GROUP *state, uint8 slot);
sint32 object_init_racer_object_grps(const struct RACE_VIEW *view0, const struct RACE_VIEW *view1, sint32 mode);
sint32 object_grps_build_linked(const struct ROUTE_SEGMENT *object, OBJECT_GROUP *output, const CAMERA *cfg);

#endif /* OBJECT_H */
