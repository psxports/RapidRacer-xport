#ifndef RR_VEHICLE_H
#define RR_VEHICLE_H

#include "psx.h"
#include "motion.h"
#include "route.h"
#include <stddef.h>

typedef struct
{
    sint32 boundary_section;
    ROUTE_CONTACT points[5];
    sint32 speeds[5];
    sint32 force[3];
    sint32 correction[3];
    MATRIX ground;
} BOAT_CONTACTS;

typedef struct
{
    uint32 bound_radius;
    uint32 radius;
    SVECTOR local[2];
    VECTOR world[2];
} BOAT_CAPSULE;

typedef struct
{
    uint32 driver;
    sint32 throttle;
    sint32 thrust;
    uint32 mode;
    sint32 recovery_tick;
    sint32 recovery_from[4];
    sint32 recovery_to[4];
    sint32 boost_tick;
    sint32 roll;
    sint32 pitch_target;
    sint32 pitch;
    sint32 steering;
    sint32 roll_input;
    sint16 force_arm[3];
    sint16 pitch_axis[3];
} BOAT_CONTROL;

typedef struct
{
    uint32 mass;
    uint32 dimensions[3];
    uint32 pitch;
    uint32 drive_scale;
    uint32 contact_force;
    uint32 throttle;
    uint32 engine_class;
    uint32 contact_height;
    uint32 grid_index;
    uint32 hull_model;
    uint32 handling;
    uint32 model_index;
    uint32 ride_height;
    uint32 propellers;
} BOAT_SETUP;

typedef struct
{
    sint32 strength;
    sint16 normal[2];
} BOAT_IMPACT;

typedef struct
{
    sint32 progress;
    sint32 progress_step;
    uint16 phase;
    uint16 mode;
} BOAT_RACE;

typedef struct
{
    SVECTOR position;
    SVECTOR direction;
    sint16 growth;
    sint32 width;
    uint16 texture;
} TRAIL_PARTICLE;

typedef struct
{
    sint32 intensity[5];
    uint16 texture[5];
    uint16 scroll;
    uint32 contact;
    SVECTOR front;
    SVECTOR rear;
    TRAIL_PARTICLE particles[5];
    TRAIL_PARTICLE *order[5];
} TRAIL_GROUP;

typedef struct
{
    SVECTOR position;
    SVECTOR direction;
    sint16 velocity[3];
    uint16 fade;
    sint16 lift;
    uint16 fade_step;
    uint16 texture;
} TRAIL_SPRAY;

typedef struct
{
    sint32 origin[3];
    SVECTOR front[2];
    SVECTOR rear[2];
    SVECTOR propeller;
    TRAIL_GROUP groups[2];
    TRAIL_SPRAY spray[5];
    TRAIL_SPRAY *spray_order[5];
    uint16 scroll;
    uint32 phase;
} BOAT_TRAIL;

typedef struct
{
    uint32 mode;
    uint8 racer_num;
} BOAT_MENU;

typedef struct BOAT
{
    MOTION_STATE motion;
    BOAT_SETUP setup;
    BOAT_CONTROL control;
    BOAT_CONTACTS contacts;
    BOAT_CAPSULE capsule;
    BOAT_IMPACT impact;
    BOAT_ROUTE_TARGET route_target;
    BOAT_ROUTE_STATE route;
    BOAT_RACE race;
    BOAT_MENU menu;
    BOAT_TRAIL trail;
    uint32 model_slot;
} BOAT;

BOAT_MENU *vehicle_menu(uint32 menu);
extern const uint8 vehicle_base_levels[9];
extern BOAT vehicle_boats[16];
extern uint32 vehicle_racer_count;
extern uint16 vehicle_leader_count;
extern uint16 vehicle_trailer_count;
extern BOAT *vehicle_racers[16];
extern BOAT *vehicle_leaders[16];
extern BOAT *vehicle_trailers[16];
extern BOAT *vehicle_players[2];
extern BOAT *vehicle_tracked[2][16];
BOAT **vehicle_tracks(uint32 context);
BOAT *vehicle_player(uint32 context);
uint32 vehicle_legacy(const BOAT *boat);

enum
{
    BOAT_LEGACY_STRIDE = 2384u
};

sint32 vehicle_steer_edge(BOAT *boat);
sint32 vehicle_config_feature_desc(BOAT_SETUP *output, sint32 index, sint32 mode);
sint32 vehicle_init(BOAT *boat, sint16 segment, sint16 entry, sint32 driver, sint32 cfg_idx);
sint32 vehicle_init_slots(void);
sint32 vehicle_damp_vel(BOAT *boat);
sint32 vehicle_input(BOAT *boat, uint32 context);
sint32 vehicle_resolve_collision(BOAT *first, BOAT *second, const SVECTOR *normal, const VECTOR *point, sint32 penetration);
void vehicle_collide_boats(BOAT *boat);
sint32 vehicle_control_force(BOAT *boat);
// Returns the horizontal length used by control forces
sint32 vehicle_steer(BOAT *boat);
sint32 vehicle_reset_motion(BOAT *boat);
void vehicle_sum_contacts(BOAT *boat, sint32 reset);

sint32 vehicle_solve_contacts(BOAT *boat);
sint32 vehicle_contact_force(BOAT *boat, const ROUTE_CONTACT *control, sint32 *cross_x);
sint32 vehicle_assign_list_identifiers(void);

sint32 vehicle_update_terrain(BOAT *boat, sint32 reset);
sint32 vehicle_update_route(BOAT *boat);

sint32 race_update_racers(sint32 argument);
sint32 vehicle_init_motion(BOAT *boat);
void vehicle_update_matrix(BOAT *boat);

sint32 vehicle_update_steering_force(BOAT *boat);

void vehicle_ground_matrix(BOAT *boat);
sint32 vehicle_mark_contacts(BOAT *boat);

#endif /* RR_VEHICLE_H */
