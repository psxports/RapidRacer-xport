#ifndef RR_PICKUP_H
#define RR_PICKUP_H

#include "psx.h"

void pickup_reset(void);
void pickup_restore(void);

struct BOAT;
uint32 pickup_scan(const struct BOAT *boat);

sint32 pickup_apply_command(uint32 state, uint32 menu, sint16 command, uint32 argument);
sint32 pickup_update_slot_countdown(uint32 state, sint16 slot);
sint32 pickup_update_slots(uint32 state, uint32 *ot, const struct BOAT *boat);
sint32 pickup_write_racer_indices(uint8 *output);
sint32 pickup_update_racer_flags(void);

#endif /* RR_PICKUP_H */
