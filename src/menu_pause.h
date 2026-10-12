#ifndef MENU_PAUSE_H
#define MENU_PAUSE_H

#include "psx.h"
#include "hud.h"

struct RACE_VIEW;

void menu_pause_reset(void);
sint32 menu_pause_run(const struct RACE_VIEW *view);
sint32 menu_pause_update_recs(HUD_PRIM *prims);
sint32 menu_pause_submit(uint32 *ot);

#endif /* MENU_PAUSE_H */
