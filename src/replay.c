#include "replay.h"
#include "boat.h"
#include "utils.h"
#include "xport_trace.h"
#include <string.h>

REPLAY replay_state;

void replay_init(void)
{
    FUNCTION_MARKER(0x8002396C, "MAIN.EXE");
    memset(&replay_state, 0, sizeof(replay_state));
    replay_state.capture = replay_state.buffers[0];
    replay_state.playback = replay_state.buffers[1];
    replay_state.capacity = REPLAY_CAPACITY - 1;
}

void replay_begin(const ROUTE_CONTACT *contact)
{
    FUNCTION_MARKER(0x800239D8, "MAIN.EXE");
    replay_state.capture_start = *contact;
    replay_state.capturing = 1;
}

void replay_finish(const ROUTE_CONTACT *contact, sint32 replace)
{
    if (replace)
    {
        FUNCTION_MARKER(0x80023A44, "MAIN.EXE");
    }
    else
    {
        FUNCTION_MARKER(0x80023BB4, "MAIN.EXE");
    }
    if (replace)
    {
        REPLAY_RECORD *prev = replay_state.playback;
        replay_state.playback = replay_state.capture;
        replay_state.capture = prev;
        replay_state.playback_count = replay_state.capture_count;
        replay_state.playback_start = replay_state.capture_start;
        replay_state.capacity = REPLAY_CAPACITY;
    }
    replay_state.contact = replay_state.playback_start;
    replay_state.cursor = 0;
    replay_state.capture_count = 0;
    replay_state.capture_start = *contact;
    replay_state.playing = replay_state.playback_count != 0;
}

static sint32 replay_append(const REPLAY_RECORD *rec)
{
    FUNCTION_MARKER(0x80023C98, "MAIN.EXE");
    if (!replay_state.capturing || replay_state.capture_count >= replay_state.capacity)
        return 0;
    replay_state.capture[replay_state.capture_count++] = *rec;
    return 1;
}

void replay_rec(const BOAT *boat)
{
    REPLAY_RECORD rec;
    uint32 axis;
    for (axis = 0; axis < 3; ++axis)
        rec.pos.v[axis] = (sint16)boat->pos.v[axis];
    for (axis = 0; axis < 4; ++axis)
        rec.quat[axis] = boat->quat.v[axis] >> 13;
    replay_append(&rec);
}

sint32 replay_advance(void)
{
    FUNCTION_MARKER(0x80023D6C, "MAIN.EXE");
    const REPLAY_RECORD *rec;
    uint32 sq = 0;
    uint32 axis;
    if (!replay_state.playing || replay_state.cursor >= replay_state.playback_count)
    {
        replay_state.playing = 0;
        return 0;
    }
    rec = &replay_state.playback[replay_state.cursor++];
    for (axis = 0; axis < 3; ++axis)
    {
        replay_state.velocity.v[axis] = mul32(rec->pos.v[axis] - replay_state.pos.v[axis], 256);
        sq += mul32(replay_state.velocity.v[axis], replay_state.velocity.v[axis]);
        replay_state.pos.v[axis] = rec->pos.v[axis];
        replay_state.matrix.t[axis] = rec->pos.v[axis];
    }
    replay_state.speed = SquareRoot0(sq);
    for (axis = 0; axis < 4; ++axis)
        replay_state.quat.v[axis] = rec->quat[axis] * 8192;
    quat_matrix(&replay_state.quat, &replay_state.matrix);
    SetRotMatrix(&replay_state.matrix);
    SetTransMatrix(&replay_state.matrix);
    route_contact_update(&replay_state.contact);
    if (replay_state.cursor == replay_state.playback_count)
        replay_state.playing = 0;
    return 1;
}
