#ifndef RR_ROUTE_H
#define RR_ROUTE_H

#include "psx.h"
#include <stddef.h>

enum {ROUTE_SEGMENT_CAPACITY = 65535, ROUTE_VERTEX_CAPACITY = 14};

typedef struct
{
    uint8 position[3];
    uint8 flags;
    uint8 phase[2];
    uint8 curve;
    uint8 texture_phase;
    sint8 deformation[2];
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
    uint16 palette;
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
    ROUTE_SEGMENT *segments[ROUTE_SEGMENT_CAPACITY];
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

ROUTE_DECODE_RESULT route_decode_archive(const uint8 *data, size_t size, ROUTE_RESOURCES *archive);


extern ROUTE_RESOURCES route_resources;
extern ROUTE_SEGMENT route_showcase_seg;
void route_init_showcase(void);

typedef struct
{
    uint32 turn_step;
    uint32 turn_amount;
    uint32 turn_phase;
    uint32 slope_step;
    uint32 slope_amount;
    uint32 slope_phase;
    uint32 left_offsets;
    uint32 outer_offsets;
    uint32 random_ranges;
    uint32 color_ranges;
    uint32 color_bases;
    uint32 control;
    uint32 delta;
} ROUTE_GEN_DEFAULTS;

extern const ROUTE_GEN_DEFAULTS route_gen_defaults;
extern const uint8 route_gen_commands[14];
extern const uint8 route_gen_sources[15];
extern const uint8 route_gen_materials[12][3];
extern const uint8 route_gen_patterns[12][4];

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
    uint16 direction;
    uint16 speed_adjust;
    uint16 behavior;
    uint16 lane_timer;
    uint16 hazard;
    uint16 lookahead;
    const AI_ROUTE_CFG *settings;
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
    ROUTE_SEGMENT *seg;
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

typedef struct
{
    ROUTE_SEGMENT *seg;
    uint32 vertex_idx;
    SVECTOR dir;
    SVECTOR normal;
    uint32 mode;
    uint32 route_idx;
} ROUTE_BOUNDARY;
enum {ROUTE_BOUNDARY_CAPACITY = 2 * ROUTE_SEGMENT_CAPACITY + 2};
typedef struct
{
    uint32 count;
    ROUTE_BOUNDARY entries[ROUTE_BOUNDARY_CAPACITY];
} ROUTE_BOUNDARIES;

extern ROUTE_BOUNDARIES route_boundaries;

struct BOAT;
ROUTE_DECODE_RESULT route_decode_archive(const uint8 *data, size_t size, ROUTE_RESOURCES *archive);
sint32 route_sample_geom(const ROUTE_SEGMENT *src, sint32 idx, sint32 apply_height, sint32 dst[4]);
void route_contact_init(ROUTE_CONTACT *state, ROUTE_SEGMENT *seg, sint32 entry);
sint32 route_contact_update(ROUTE_CONTACT *state);
sint32 route_advance_target(BOAT_ROUTE_STATE *route, const ROUTE_RESOURCES *archive);
sint32 route_init_steps(struct BOAT *boat, const ROUTE_SEGMENT *seg);
sint32 route_init_lookahead(struct BOAT *boat, const ROUTE_SEGMENT *seg, uint32 segment_count);
sint32 route_calc_boundary_dir(const ROUTE_SEGMENT *src, SVECTOR *dst);
sint32 route_init_lanes(ROUTE_RESOURCES *archive, uint32 mode);
sint32 route_select_dir(struct BOAT *boat, const ROUTE_RESOURCES *archive);
sint32 route_aim(struct BOAT *boat, const ROUTE_SEGMENT *seg);
sint32 route_build_boundaries(ROUTE_RESOURCES *route, ROUTE_BOUNDARIES *dst);
sint32 route_init_cmds(ROUTE_RESOURCES *route);
void route_sample_boundary(const sint32 position[3], const ROUTE_BOUNDARY *src);
void vehicle_sample_boundary(struct BOAT *boat, const ROUTE_BOUNDARIES *boundaries);
void route_gen_geom(ROUTE_SEGMENT *records, sint32 count);
sint32 route_gen_vertices(ROUTE_SEGMENT *record, sint32 count);
sint32 route_update_speed_ctrl(const struct BOAT *src, struct BOAT *boat, uint32 race_mode);

sint32 route_gen_init(uint32 state);

#endif
