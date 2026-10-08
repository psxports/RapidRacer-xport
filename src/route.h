#ifndef RR_ROUTE_H
#define RR_ROUTE_H

#include "psx.h"
#include <stddef.h>

typedef struct
{
    sint32 speed;
    uint16 direction;
    uint16 speed_adjust;
    uint16 behavior;
    uint16 lane_timer;
    uint16 hazard;
    uint16 lookahead;
    uint32 settings;
    uint16 target_segment;
    uint16 segment;
    uint16 target_lane;
    uint16 steering;
    uint32 ticks;
    // Shared by AI gaps and proximity audio
    sint32 distance;
    sint32 link_distance;
} BOAT_ROUTE_STATE;

typedef struct
{
    sint32 vector[3];
    sint32 length;
} BOAT_ROUTE_TARGET;

typedef struct
{
    SVECTOR sample;
    SVECTOR normal;
    uint32 object;
    uint32 entry;
    uint32 left;
    uint32 right;
    sint32 position[3];
    sint32 height;
    sint32 penetration;
    uint32 surface;
    sint32 plane;
    sint32 delta[3];
    sint32 diagonal[4];
} ROUTE_CONTACT;

sint32 route_contact_update(ROUTE_CONTACT *state);

sint32 route_sample_geom(uint32 base, sint32 index, sint32 apply_height, uint32 output);
void route_contact_init(ROUTE_CONTACT *state, sint32 object_index, sint32 entry);

sint32 route_calc_boundary_direction(uint32 object, SVECTOR *output);
sint32 route_sample_geom_host(uint32 base, sint32 index, sint32 apply_height, sint32 output[4]);
sint32 route_build_boundary_vec(void);
struct BOAT;
sint32 route_sample_dist_slot(const struct BOAT *boat, uint32 descriptor, uint32 output);
sint32 route_aim(struct BOAT *boat);
sint32 route_init_lookahead(struct BOAT *boat);
sint32 route_select_direction_vec(struct BOAT *boat);
sint32 route_update_speed_ctrl(const struct BOAT *source, struct BOAT *boat);
sint32 route_advance_target(BOAT_ROUTE_STATE *route);
sint32 route_init_steps(struct BOAT *boat);
sint32 route_assign_topology_states(void);

void route_sample_boundary(const sint32 position[3], uint32 descriptor);

#endif /* RR_ROUTE_H */
