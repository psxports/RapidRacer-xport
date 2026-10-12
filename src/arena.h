#ifndef ARENA_H
#define ARENA_H

#include "psx.h"

void arena_reset(void);
uint32 arena_push(void);
sint32 arena_pop(void);
void *arena_alloc(uint32 size);

#endif /* ARENA_H */
