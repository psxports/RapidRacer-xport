#ifndef RR_REPLAY_H
#define RR_REPLAY_H
#include "psx.h"
#include "route.h"
#include <stddef.h>

enum
{
    REPLAY_CAPACITY = 10000
};

typedef struct
{
    sint32 position[3];
    sint16 quaternion[4];
} REPLAY_RECORD;

typedef struct
{
    REPLAY_RECORD buffers[2][REPLAY_CAPACITY];
    REPLAY_RECORD *capture;
    REPLAY_RECORD *playback;
    size_t capacity;
    size_t capture_count;
    size_t playback_count;
    size_t cursor;
    sint32 capturing;
    sint32 playing;
    ROUTE_CONTACT capture_start;
    ROUTE_CONTACT playback_start;
    ROUTE_CONTACT contact;
    sint32 position[3];
    sint32 velocity[3];
    sint32 speed;
    sint32 quaternion[4];
    MATRIX matrix;
} REPLAY_STATE;

extern REPLAY_STATE replay_state;
void replay_init(void);
void replay_begin(const ROUTE_CONTACT *contact);
void replay_finish(const ROUTE_CONTACT *contact, sint32 replace);
sint32 replay_append(const REPLAY_RECORD *record);
struct BOAT;
void replay_record(const struct BOAT *boat);
sint32 replay_advance(void);

#endif
