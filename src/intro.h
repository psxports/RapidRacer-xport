#ifndef RR_INTRO_H
#define RR_INTRO_H

#include "psx.h"
#include "sprite.h"

sint32 intro_config_cutscene(sint16 selection);
sint32 intro_skip_is_requested(void);

sint32 intro_run_skippable(void);
sint32 intro_show_image(SPRITE_IMAGE *state);

#endif /* RR_INTRO_H */
