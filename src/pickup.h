#ifndef RR_PICKUP_H
#define RR_PICKUP_H

#include "psx.h"

struct BOAT;
uint32 pickup_scan(const struct BOAT *boat);

sint32 pickup_apply_command(uint32 state, uint32 menu, sint16 command, uint32 argument);
sint32 pickup_update_slot_countdown(uint32 state, sint16 slot);
sint32 pickup_update_slots(uint32 state, uint32 selection, const struct BOAT *boat);
sint32 pickup_write_racer_indices(uint32 output);
sint32 pickup_update_racer_flags(void);
sint32 pickup_fn_8003d8dc(uint32 state);
uint32 pickup_fn_8003da8c(uint32 records, sint32 count);

#endif /* RR_PICKUP_H */
