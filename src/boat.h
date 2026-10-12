#ifndef BOAT_H
#define BOAT_H

#include "psx.h"

struct RACE_VIEW;
#include "utils.h"
#include "route.h"
#include "pickup.h"
#include "timer.h"
#include <stddef.h>

typedef struct
{
    sint32 boundary_section;
    ROUTE_CONTACT points[5];
    sint32 speeds[5];
    VEC3 force;
    VEC3 correction;
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
    QUAT recovery_from;
    QUAT recovery_to;
    sint32 boost_tick;
    sint32 roll;
    sint32 pitch_target;
    sint32 pitch;
    sint32 steering;
    sint32 roll_input;
    SVECTOR force_arm;
    SVECTOR pitch_axis;
} BOAT_CONTROL;

typedef struct
{
    uint32 mass;
    VEC3 dimensions;
    sint32 pitch;
    uint32 drive_scale;
    uint32 contact_force;
    sint32 throttle;
    sint32 engine_class;
    uint32 contact_height;
    uint32 grid_idx;
    uint32 hull_model;
    uint32 handling;
    uint32 model_idx;
    uint32 ride_height;
    uint32 propellers;
} BOAT_SETUP;

typedef struct
{
    sint32 strength;
    sint16 n[2];
} BOAT_IMPACT;

typedef struct
{
    sint32 progress;
    sint32 progress_step;
    sint16 phase;
    sint16 mode;
} BOAT_RACE;

typedef struct
{
    SVECTOR pos;
    SVECTOR dir;
    sint16 growth;
    sint32 width;
    uint16 tex;
} TRAIL_PARTICLE;

typedef struct
{
    sint32 intensity[5];
    uint16 tex[5];
    uint16 scroll;
    uint32 contact;
    SVECTOR front;
    SVECTOR rear;
    TRAIL_PARTICLE particles[5];
    TRAIL_PARTICLE *order[5];
} TRAIL_GROUP;

typedef struct
{
    SVECTOR pos;
    SVECTOR dir;
    SVECTOR velocity;
    uint16 fade;
    sint16 lift;
    uint16 fade_step;
    uint16 tex;
} TRAIL_SPRAY;

typedef struct
{
    VEC3 origin;
    SVECTOR front[2];
    SVECTOR rear[2];
    SVECTOR propeller;
    TRAIL_GROUP grps[2];
    TRAIL_SPRAY spray[5];
    TRAIL_SPRAY *spray_order[5];
    uint16 scroll;
    uint32 phase;
} BOAT_TRAIL;

typedef struct BOAT_MENU
{
    sint32 mode;
    uint32 delta_timer;
    uint32 checkpoint_total;
    uint32 start_ready;
    uint32 finish_state;
    uint8 racer_num;
    uint8 lap_idx;
    uint8 best_lap_idx;
    uint8 checkpoint_idx;
    uint8 rank;
    uint8 prev_rank;
    TIME_REC start_time;
    TIME_REC total_time;
    TIME_REC best_time;
    TIME_REC lap_times[5];
    TIME_REC checkpoint_times[5][5];
    PICKUP_SLOT pickups[6];
} BOAT_MENU;

typedef struct
{
    VEC3 vector;
    sint32 speed;
} VELOCITY;

typedef struct BOAT
{
    MATRIX matrix;
    MATRIX pose;
    VEC3 pos;
    QUAT quat;
    QUAT attitude;
    QUAT rotation;
    VELOCITY velocity;
    sint32 mass;
    VEC3 inertia;
    VEC3 force;
    VEC3 torque;
    BOAT_SETUP setup;
    BOAT_CONTROL control;
    BOAT_CONTACTS contacts;
    BOAT_CAPSULE capsule;
    BOAT_IMPACT impact;
    BOAT_ROUTE_TARGET route_target;
    BOAT_ROUTE route;
    BOAT_RACE race;
    BOAT_MENU menu;
    BOAT_TRAIL trail;
    uint32 model_slot;
} BOAT;

BOAT_MENU *boat_player_menu(const struct RACE_VIEW *context);
TIME_REC *boat_lap_time(BOAT_MENU *menu, uint32 idx);
TIME_REC *boat_checkpoint_time(BOAT_MENU *menu, uint32 lap, uint32 idx);
extern const uint8 boat_base_levels[9];
extern BOAT boat_boats[16];
extern sint32 boat_racer_count;
extern sint16 boat_leader_count;
extern sint16 boat_trailer_count;
extern BOAT *boat_racers[16];
extern BOAT *boat_leaders[16];
extern BOAT *boat_trailers[16];
extern BOAT *boat_players[2];

BOAT **boat_tracks(const struct RACE_VIEW *context);
BOAT *boat_player(const struct RACE_VIEW *context);

sint32 boat_init_slots(void);
sint32 boat_damp_vel(BOAT *boat);
sint32 boat_input(BOAT *boat, const struct RACE_VIEW *context);
sint32 boat_resolve_col(BOAT *boat0, BOAT *boat1, const SVECTOR *n, const VECTOR *point, sint32 penetration);

// Returns the horizontal length used by control forces

sint32 race_update_racers(sint32 argument);
sint32 boat_init_motion(BOAT *boat);

extern sint32 boat_half_period;
extern sint32 boat_horizontal_locked;

#endif /* BOAT_H */
