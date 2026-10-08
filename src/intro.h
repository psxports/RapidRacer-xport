#ifndef RR_INTRO_H
#define RR_INTRO_H

#include "psx.h"

sint32 intro_config_cutscene(sint16 selection);
sint32 intro_skip_is_requested(void);

sint32 intro_run_skippable(void);
sint32 intro_show_image(uint32 state);

#endif /* RR_INTRO_H */
