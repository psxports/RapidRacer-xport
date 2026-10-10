#ifndef RR_MDEC_H
#define RR_MDEC_H

#include "psx.h"

typedef struct
{
    const char *path;
    uint16 rgb24, screen_width, x, y, width, height;
    uint32 frames;
    uint16 volume;
} MDEC_MOVIE;

sint32 mdec_stream_play(const MDEC_MOVIE *movie, sint32 (*skip)(void), sint32 mode);

#endif /* RR_MDEC_H */
