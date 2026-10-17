#ifndef REPLAY_H
#define REPLAY_H
#include "psx.h"
#include "route.h"
#include "utils.h"
#include <stddef.h>

enum
{
    REPLAY_CAPACITY = 10000
};

typedef struct
{
    VEC3 pos;
    sint16 quat[4];
} REPLAY_RECORD;

typedef struct
{
    REPLAY_RECORD buffers[2][REPLAY_CAPACITY];
    REPLAY_RECORD *capture;
    REPLAY_RECORD *playback;
    uint32 capacity;
    uint32 capture_count;
    uint32 playback_count;
    uint32 cursor;
    sint32 capturing;
    sint32 playing;
    ROUTE_CONTACT capture_start;
    ROUTE_CONTACT playback_start;
    ROUTE_CONTACT contact;
    VEC3 pos;
    VEC3 velocity;
    sint32 speed;
    QUAT quat;
    MATRIX matrix;
} REPLAY;

extern REPLAY replay_state;
void replay_init(void);
void replay_begin(const ROUTE_CONTACT *contact);
void replay_finish(const ROUTE_CONTACT *contact, sint32 replace);

struct BOAT;
void replay_rec(const struct BOAT *boat);
sint32 replay_advance(void);

#endif
