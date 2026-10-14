#ifndef EDITOR_VIS_H
#define EDITOR_VIS_H

#ifdef _DEBUG
    #include "game.h"
    #include "route.h"
    #include "input.h"

void editor_vis_begin(void);
void editor_vis_end(void);
void editor_vis_update(void);
void editor_vis_pad(PAD *pad);
void editor_vis_draw(void);
void editor_vis_free(void);
const ROUTE_SEGMENT *editor_vis_route(const RACE_VIEW *view);
sint32 editor_vis_reversed(void);
void editor_vis_camera(const RACE_VIEW *view, MATRIXF *rotation, VEC3F *pos);
#endif

#endif
