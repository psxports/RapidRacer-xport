#ifndef RR_SCENE_H
#define RR_SCENE_H

#include "psx.h"

struct ROUTE_SEGMENT;
#include "timer.h"
#include <stddef.h>

typedef struct
{
    uint8 hi, lo, env;
} SCENE_VISIBILITY;
typedef struct
{
    SVECTOR v[4];
    uint32 attrs[5];
} SCENE_PRIM_FACE;

typedef struct
{
    uint32 count, split;
    SCENE_PRIM_FACE *faces;
} SCENE_PRIM_SET;

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
} SCENE_PRIMS;

typedef struct
{
    SCENE_PRIM_SET sets[8];
    SCENE_PRIMS prims[2][8];
} SCENE_PRIM_GROUP;

typedef struct SCENE_GROUP_NODE SCENE_GROUP_NODE;
struct SCENE_GROUP_NODE
{
    SCENE_GROUP_NODE *next;
    SCENE_PRIM_GROUP *group;
};

typedef struct
{
    SCENE_GROUP_NODE *groups[2];
    SCENE_VISIBILITY forward, backward;
} SCENE_SECTION;
typedef struct
{
    uint32 count;
    uint16 payload_size;
    SCENE_SECTION sections[255];
} SCENE_SECTIONS;

typedef union
{
    uint32 word;
    CVECTOR rgb;
} SCENE_COLOR;

typedef struct
{
    SCENE_COLOR colors[3];
    uint8 gradient;
} SCENE_COLORS;

SCENE_COLORS *scene_colors(uint32 variant, uint32 course);

typedef struct
{
    uint8 alternating, gradient, edge, strips;
    sint16 ground, edge_step, strip_step, top_step, gradient_step;
    uint16 tex_width;
    SCENE_COLOR colors[3];
} SCENE_HORIZON;

extern SCENE_HORIZON *scene_horizon;
SCENE_HORIZON *scene_select_horizon(uint32 course, uint32 mode);
void scene_showcase_horizon(uint32 mode);

typedef struct
{
    sint16 first, last;
    uint32 command;
} SCENE_REVERB;

typedef struct
{
    const SCENE_REVERB *entries;
    sint16 count;
} SCENE_REVERB_CFG;

extern const SCENE_REVERB_CFG *scene_reverb;
void scene_select_reverb(uint32 course, uint32 mode);
void scene_showcase_reverb(void);

typedef struct
{
    TIME_REC time;
    sint16 seg;
} SCENE_CHECKPOINT;

typedef struct
{
    sint16 laps, checkpoint_count;
    SCENE_CHECKPOINT checkpoints[5];
} SCENE_RACE_CFG;

extern SCENE_RACE_CFG *scene_race;
SCENE_CHECKPOINT *scene_checkpoint(uint32 idx);
void scene_select(uint32 course, uint32 mode);
void scene_showcase_race(void);

typedef struct
{
    sint16 gap, camera_seg, grid_offset, attract_seg;
    uint32 event_tick;
    sint16 pos[3], angles[3];
} SCENE_START;

extern const SCENE_START *scene_start;
void scene_select_start(uint32 course, uint32 mode);
void scene_clear_start(void);

extern SCENE_SECTIONS scene_sections;

struct AI_CONTROL_CFG;
extern const struct AI_CONTROL_CFG *scene_control;
sint32 scene_decode_sections(const uint8 *src, size_t size, SCENE_SECTIONS *dst);
void scene_load_sections(const uint8 *src, size_t size);
SCENE_SECTION *scene_section(uint32 idx);
void scene_clear_sections(void);

sint32 scene_render_player(uint32 context, sint32 buffer);
sint32 scene_clear_group_item_flags(sint16 group);

void scene_render_player_sequence(const struct ROUTE_SEGMENT *sequence, uint32 state_base, sint32 state_index);
sint32 scene_process_published_rec_flags(void);

#endif /* RR_SCENE_H */
