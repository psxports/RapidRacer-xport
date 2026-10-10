#ifndef RR_PICKUP_H
#define RR_PICKUP_H

#include "psx.h"

struct RACE_VIEW;

typedef struct
{
    uint8 count, pending;
    uint16 delta, ticks, x, y;
} PICKUP_SLOT;

typedef struct
{
    uint16 limit;
    uint16 x[6], y[6];
    uint16 center_x;
} PICKUP_HUD;

extern PICKUP_HUD pickup_hud[4];

struct BOAT_MENU;

void pickup_reset(void);
void pickup_restore(void);

struct BOAT;
uint32 pickup_scan(const struct BOAT *boat);

sint32 pickup_apply_command(const struct RACE_VIEW *state, struct BOAT_MENU *menu, sint16 command);
sint32 pickup_update_slot_countdown(struct BOAT_MENU *menu, sint16 slot);
sint32 pickup_update_slots(const struct RACE_VIEW *state, uint32 *ot, const struct BOAT *boat);
sint32 pickup_write_racer_indices(uint8 *output);
sint32 pickup_update_racer_flags(void);

#endif /* RR_PICKUP_H */
