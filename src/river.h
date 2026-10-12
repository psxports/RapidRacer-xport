#ifndef RIVER_H
#define RIVER_H

#include "psx.h"

struct RACE_CFG;

struct RACE_VIEW;
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
} RIVER_REC;

typedef struct
{
    RIVER_REC *recs;
    sint32 count, idx, dist_sq, prev_proj;
} RIVER_LIST;

extern RIVER_LIST river_course, river_markers;
extern sint32 river_selection;

void river_select_frame(sint32 buffer, sint32 view);
void river_render_showcase(const struct RACE_VIEW *state, uint32 *overlay_ot, sint32 buffer_idx);
sint32 river_nearby_event_check(const struct BOAT *boat);

sint32 boat_update_route_segs(const struct RACE_VIEW *state);

void terrain_init_contact(ROUTE_CONTACT *point);
sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground);

sint32 river_init_showcase(void);
void river_init_light_fractal(const struct RACE_CFG *cfg, sint32 count);

void river_render(const ROUTE_SEGMENT *sequence, const struct RACE_VIEW *view, sint32 frame);

#endif /* RIVER_H */
