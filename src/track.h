#ifndef TRACK_H
#define TRACK_H

#include "psx.h"

struct RACE_VIEW;
struct ROUTE_SEGMENT;

typedef struct
{
    uint32 count;
    uint16 *values;
} TRACK_VIS;

TRACK_VIS *track_visibility(uint32 course);
uint32 track_vis_idx(const struct ROUTE_SEGMENT *seg);
void track_vis_range(const struct ROUTE_SEGMENT *seg, uint32 *next, uint32 *prev);
sint32 track_vis_contains(const struct ROUTE_SEGMENT *seg, sint32 idx);

typedef struct
{
    sint32 pitch, yaw;
} TRACK_LIGHT_ANGLES;

extern const TRACK_LIGHT_ANGLES track_light_angles[7];

struct ROUTE_SEGMENT;
#include "timer.h"
#include <stddef.h>

typedef struct
{
    uint8 hi, lo, env;
} TRACK_VISIBILITY;

typedef struct
{
    SVECTOR v[4];
    uint32 attrs[5];
} TRACK_PRIM_FACE;

typedef struct
{
    uint32 count, split;
    TRACK_PRIM_FACE *faces;
} TRACK_PRIM_SET;

typedef union
{
    void *data;
    POLY_F3 *f3;
    POLY_F4 *f4;
    POLY_FT3 *ft3;
    POLY_FT4 *ft4;
    POLY_G3 *g3;
    POLY_G4 *g4;
    POLY_GT3 *gt3;
    POLY_GT4 *gt4;
} TRACK_PRIMS;

typedef struct
{
    TRACK_PRIM_SET sets[8];
    TRACK_PRIMS prims[2][8];
} TRACK_PRIM_GROUP;

typedef struct TRACK_GROUP_NODE TRACK_GROUP_NODE;

struct TRACK_GROUP_NODE
{
    TRACK_GROUP_NODE *next;
    TRACK_PRIM_GROUP *grp;
};

typedef struct
{
    TRACK_GROUP_NODE *grps[2];
    TRACK_VISIBILITY forward, backward;
} SECTION;

typedef struct
{
    uint32 count;
    uint16 payload_size;
    SECTION sections[255];
} TRACK_SECTIONS;

typedef union
{
    uint32 word;
    CVECTOR rgb;
} TRACK_COLOR;

typedef struct
{
    TRACK_COLOR colors[3];
    uint8 gradient;
} TRACK_COLORS;

TRACK_COLORS *track_colors(uint32 variant, uint32 course);

typedef struct
{
    uint8 alternating, gradient, edge, strips;
    sint16 ground, edge_step, strip_step, top_step, gradient_step;
    uint16 tex_width;
    TRACK_COLOR colors[3];
} TRACK_HORIZON;

extern TRACK_HORIZON *track_horizon;

void track_showcase_horizon(uint32 mode);

typedef struct
{
    sint16 first, last;
    uint32 command;
} TRACK_REVERB;

typedef struct
{
    const TRACK_REVERB *entries;
    sint16 count;
} TRACK_REVERB_CFG;

extern const TRACK_REVERB_CFG *track_reverb;

void track_showcase_reverb(void);

typedef struct
{
    TIME_REC time;
    sint16 seg;
} TRACK_CHECKPOINT;

typedef struct
{
    sint16 laps, checkpoint_count;
    TRACK_CHECKPOINT checkpoints[5];
} TRACK_RACE_CFG;

extern TRACK_RACE_CFG *track_race;
TRACK_CHECKPOINT *track_checkpoint(uint32 idx);
void track_select(uint32 course, uint32 mode);
void track_showcase_race(void);

typedef struct
{
    sint16 gap, camera_seg, grid_offset, attract_seg;
    uint32 event_tick;
    sint16 pos[3], angles[3];
} TRACK_START;

extern const TRACK_START *track_start;

void track_clear_start(void);

extern TRACK_SECTIONS track_sections;

struct AI_CONTROL_CFG;
extern const struct AI_CONTROL_CFG *track_control;

void track_load_sections(const uint8 *src, uint32 size);
SECTION *track_section(uint32 idx);
void track_clear_sections(void);

sint32 track_render_player(const struct RACE_VIEW *context, sint32 buffer);
sint32 track_clear_grp_item_flags(sint16 grp);

sint32 track_process_published_rec_flags(void);

#endif /* TRACK_H */
