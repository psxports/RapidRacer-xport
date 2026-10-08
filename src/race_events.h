#ifndef RR_RACE_EVENTS_H
#define RR_RACE_EVENTS_H

#include "psx.h"
#include "route.h"

struct BOAT;

sint32 race_events_racer_modes_trans(uint32 state);
sint32 race_events_fn_800389f4(sint32 index, sint32 variant);
uint32 race_events_fn_80038b04(uint32 state);
uint32 race_events_fn_80039728(uint32 state, uint32 player, sint32 buffer_index);
sint32 race_events_nearby_event_check(const struct BOAT *boat);
sint32 race_events_fn_80039e10(uint32 state, sint32 unused, uint32 model, uint32 parameters, sint32 a5, sint32 a6, sint32 a7, sint32 a8, uint32 colors);
void race_events_fn_8003a100(uint32 ordering_table, uint32 bucket_count, uint32 model, uint32 colors);
sint32 race_events_fn_8003a2fc(void);
sint32 terrain_collide_capsule(struct BOAT *boat, uint32 segment);

sint32 race_events_fn_8003a7e0(const struct BOAT *boat, uint32 entry);
sint32 vehicle_update_route_segments(uint32 state);
sint32 race_events_fn_8003aae4(uint32 state, uint32 route);
void terrain_init_contact(ROUTE_CONTACT *point);
sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground);
sint32 race_events_fn_8003af00(uint32 setup, uint32 argument);
sint32 race_events_fn_8003b508(void);
sint32 race_events_fn_8003f164(uint32 unused, sint32 count);
uint32 race_events_init_fractal_light(uint32 state, sint32 count);

#endif /* RR_RACE_EVENTS_H */
