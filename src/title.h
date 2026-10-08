#ifndef RR_TITLE_H
#define RR_TITLE_H

#include "psx.h"

sint32 title_init_cb(uint32 state);
sint32 title_relocate_palette_lut(void);
sint32 title_dispatch_state_cb(uint32 node);

#endif /* RR_TITLE_H */
