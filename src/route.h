#ifndef ROUTE_H
#define ROUTE_H

#include "utils.h"

struct RACE_CFG;
#include <stddef.h>

enum
{
    ROUTE_SEGMENT_CAPACITY = 65535,
    ROUTE_VERTEX_CAPACITY = 14
};

typedef struct
{
    uint8 pos[3];
    uint8 flags;
    uint8 phase[2];
    uint8 curve;
    uint8 tex_phase;
    sint8 deform[2];
    uint16 impact_tick;
    uint8 lane;
    uint8 shade;
} ROUTE_VERTEX;

typedef struct ROUTE_SEGMENT ROUTE_SEGMENT;

// Link indices are semantic directory positions even when entries alias
typedef struct
{
    ROUTE_SEGMENT *seg;
    uint16 idx;
} ROUTE_LINK;

struct ROUTE_SEGMENT
{
    ROUTE_LINK next;
    ROUTE_LINK prev;
    uint16 pal;
    uint8 lighting;
    uint8 vertex_count;
    sint16 origin[3];
    uint8 join[2];
    uint8 section;
    uint8 commands[14];
    ROUTE_VERTEX vertices[ROUTE_VERTEX_CAPACITY];
};

typedef struct
{
    uint32 count;
    uint32 owned_count;
    ROUTE_SEGMENT *segs[ROUTE_SEGMENT_CAPACITY];
    ROUTE_SEGMENT storage[ROUTE_SEGMENT_CAPACITY];
} ROUTE_RESOURCES;

typedef enum
{
    ROUTE_DECODE_OK,
    ROUTE_DECODE_HEADER,
    ROUTE_DECODE_BOUNDS,
    ROUTE_DECODE_VERTICES,
    ROUTE_DECODE_LINK
} ROUTE_DECODE_RESULT;

ROUTE_DECODE_RESULT route_decode_archive(const uint8 *data, uint32 size, ROUTE_RESOURCES *archive);

extern ROUTE_RESOURCES route_resources;
extern ROUTE_SEGMENT route_showcase_seg;
void route_init_showcase(void);

typedef struct
{
    sint16 gap;
    sint16 slow_range;
    sint16 boost_range;
    sint16 min_adjust;
    sint16 max_adjust;
    sint16 yaw_limit;
    uint8 lane_change;
    uint8 lane2;
    uint8 lane3;
    uint8 heading;
    sint16 speed_offset;
} AI_ROUTE_CFG;

typedef struct
{
    sint32 speed;
    uint16 dir;
    sint16 speed_adjust;
    uint16 behavior;
    uint16 lane_timer;
    uint16 hazard;
    sint16 lookahead;
    const AI_ROUTE_CFG *settings;
    uint16 target_seg;
    uint16 seg;
    sint16 target_lane;
    sint16 steering;
    uint32 ticks;
    // Shared by AI gaps and proximity audio
    sint32 distance;
    sint32 link_distance;
} BOAT_ROUTE;

typedef struct
{
    VEC3 vector;
    sint32 len;
} BOAT_ROUTE_TARGET;

typedef struct
{
    SVECTOR sample;
    SVECTOR n;
    ROUTE_SEGMENT *seg;
    uint32 entry;
    uint32 left;
    uint32 right;
    VEC3 pos;
    sint32 height;
    sint32 penetration;
    uint32 surface;
    sint32 plane;
    VEC3 delta;
    VEC3 diagonal;
    sint32 weight;
} ROUTE_CONTACT;

typedef struct
{
    ROUTE_SEGMENT *seg;
    uint32 vertex_idx;
    SVECTOR dir;
    SVECTOR n;
    uint32 mode;
    uint32 route_idx;
} ROUTE_BOUNDARY;

enum
{
    ROUTE_BOUNDARY_CAPACITY = 2 * ROUTE_SEGMENT_CAPACITY + 2
};

typedef struct
{
    uint32 count;
    ROUTE_BOUNDARY entries[ROUTE_BOUNDARY_CAPACITY];
} ROUTE_BOUNDARIES;

extern ROUTE_BOUNDARIES route_boundaries;

struct BOAT;
sint32 route_sample_geom(const ROUTE_SEGMENT *src, sint32 idx, sint32 apply_height, sint32 dst[4]);
void route_contact_init(ROUTE_CONTACT *state, ROUTE_SEGMENT *seg, sint32 entry);
sint32 route_contact_update(ROUTE_CONTACT *state);
sint32 route_advance_target(BOAT_ROUTE *route, const ROUTE_RESOURCES *archive);
sint32 route_init_steps(struct BOAT *boat, const ROUTE_SEGMENT *seg);
sint32 route_init_lookahead(struct BOAT *boat, const ROUTE_SEGMENT *seg, uint32 seg_count);
sint32 route_calc_boundary_dir(const ROUTE_SEGMENT *src, SVECTOR *dst);
sint32 route_init_lanes(ROUTE_RESOURCES *archive, uint32 mode);
sint32 route_select_dir(struct BOAT *boat, const ROUTE_RESOURCES *archive);
sint32 route_aim(struct BOAT *boat, const ROUTE_SEGMENT *seg);
sint32 route_build_boundaries(ROUTE_RESOURCES *route, ROUTE_BOUNDARIES *dst);

void boat_sample_boundary(struct BOAT *boat, const ROUTE_BOUNDARIES *boundaries);

sint32 route_update_speed_ctrl(const struct BOAT *src, struct BOAT *boat, uint32 race_mode);

sint32 route_gen_init_fractal(const struct RACE_CFG *cfg);

#endif
