#ifndef HD_H
#define HD_H

#include "game.h"
#include "hud.h"
#include "boat.h"

enum
{
    HD_DRAW_SCALE = 2,
    HD_FAR_Z = 16000 * HD_DRAW_SCALE
};

void hd_init(void);
void hd_sync_proj(void);
void hd_seed(void);
sint32 hd_ready(void);
void hd_reset(void);
void hd_reset_boat(const BOAT *boat);
sint32 hd_begin_ui(void);
void hd_draw_ui_quad(const float pos[4][2], const float uv[4][2], uint16 tpage, uint16 clut, uint32 color, uint32 size, sint32 textured, sint32 semi);
sint32 hd_begin_frame(void);
void hd_render_player(const RACE_VIEW *view);
void hd_update_player(const RACE_VIEW *view);
void hd_capture_frame(void);
void hd_set_frame_fract(float fract);
sint32 hd_end_frame(void);
sint32 hd_begin_scene(void);
void hd_draw_circle(uint32 step);
void hd_draw_sprite(const RACE_VIEW *view, float x, float y, float width, float height, uint16 u, uint16 v, uint16 tpage, uint16 clut, uint32 color);
void hd_draw_hud_sprite(const RACE_VIEW *view, float x, float y, float width, float height, uint16 u, uint16 v, uint16 tpage, uint16 clut, uint32 color, float ax, float ay);
void hd_hud_render(const RACE_VIEW *view);
void hd_hud_update(const RACE_VIEW *view);
void hd_hud_init(const RACE_VIEW *view, const HUD *src);
void hd_hud_background(const RACE_VIEW *view);
void hd_render_pause(const RACE_VIEW *view, const HUD *hud);
void hd_race_results(const RACE_VIEW *view);
void hd_race_update(const RACE_VIEW *view);
void hd_free(void);

#endif
