#ifndef RR_RACE_EVENTS_H
#define RR_RACE_EVENTS_H

#include "psx.h"
#include "route.h"
#include "mesh.h"
#include "camera.h"

struct BOAT;

typedef struct
{
    sint16 pos[3];
    uint16 angle;
    sint16 item_pos[3];
    sint16 kind;
    sint16 border_start, border_count;
} EVENT_REC;

typedef struct
{
    EVENT_REC *recs;
    sint32 count, idx, dist_sq, prev_proj;
} EVENT_LIST;

extern EVENT_LIST race_event_course, race_event_markers;
extern sint32 race_event_selection;

sint32 race_events_racer_modes_trans(uint32 state);
void race_events_select_frame(sint32 buffer, sint32 view);
void race_events_render_horizon(uint32 *scene_ot, const CAMERA_STATE *view, uint32 mode, uint32 layout, uint32 players);
void race_events_render(uint32 state, uint32 *overlay_ot, sint32 buffer_index);
sint32 race_events_nearby_event_check(const struct BOAT *boat);
sint32 race_events_place_model(const CAMERA_STATE *camera, const struct BOAT *boat, const MESH_MODEL *model, sint16 x, sint16 z, sint16 angle, const uint32 colors[2], uint32 *const ot[2]);
void race_events_draw_model(uint32 *ot, uint32 bucket_count, const MESH_MODEL *model, uint32 color);
sint32 race_events_fn_8003a2fc(void);
sint32 terrain_collide_capsule(struct BOAT *boat, const EVENT_REC *segment);

sint32 race_events_cross_marker(const struct BOAT *boat, EVENT_LIST *list);
sint32 vehicle_update_route_segments(uint32 state);
sint32 race_events_find_nearest(uint32 state, EVENT_LIST *list);
void terrain_init_contact(ROUTE_CONTACT *point);
sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground);
sint32 race_events_init_course(void);
sint32 race_events_fn_8003b508(void);
void race_events_init_fractal_light(uint32 state, sint32 count);

#endif /* RR_RACE_EVENTS_H */
