#ifndef EFFECTS_H
#define EFFECTS_H

#include "psx.h"
#include "mesh.h"
#include "render.h"

struct RACE_VIEW;
#include "callbacks.h"

uint32 effects_init_transform_object(const struct RACE_VIEW *state);
const MESH_MODEL *hd_effects_countdown(const struct RACE_VIEW *view, MATRIXF *matrix);
const MESH_MODEL *effects_countdown(const struct RACE_VIEW *view, MATRIX *matrix);

typedef struct
{
    sint16 pos[3];
    sint16 vel[3];
    sint16 kind;
    sint16 size;
} EFFECT_PARTICLE;

enum
{
    EFFECT_PARTICLE_COUNT = 40
};

void effects_particles(const EFFECT_PARTICLE **previous, const EFFECT_PARTICLE **current, RENDER_MATERIAL mats[3]);
void effects_depth_quad(uint32 idx, SVECTOR *origin, SVECTOR *extent, RENDER_MATERIAL *material, sint32 *brightness);
void effects_sun_material(uint32 idx, RENDER_MATERIAL *material, float *width, float *scale);

sint32 effects_register_mode(void);
sint32 effects_capture_scr_region(const struct RACE_VIEW *state, sint32 buffer_idx);

sint32 effects_render_quad_proj_fade(const struct RACE_VIEW *state, sint32 buffer_idx);
void effects_render(const struct RACE_VIEW *player, sint32 player_idx);

sint32 effects_run_expanding_circle_trans(void);
sint32 hd_effects_trans(void);

#endif /* EFFECTS_H */
