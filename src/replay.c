#include "replay.h"
#include "vehicle.h"
#include "motion.h"
#include "global.h"
#include "xport_trace.h"
#include <string.h>

REPLAY_STATE replay_state;

void replay_init(void)
{
    FUNCTION_MARKER(0x8002396Cu, "MAIN.EXE");
    memset(&replay_state, 0, sizeof(replay_state));
    replay_state.capture = replay_state.buffers[0];
    replay_state.playback = replay_state.buffers[1];
    replay_state.capacity = REPLAY_CAPACITY - 1;
}

void replay_begin(const ROUTE_CONTACT *contact)
{
    FUNCTION_MARKER(0x800239D8u, "MAIN.EXE");
    replay_state.capture_start = *contact;
    replay_state.capturing = 1;
}

void replay_finish(const ROUTE_CONTACT *contact, sint32 replace)
{
    if (replace)
    {
        FUNCTION_MARKER(0x80023A44u, "MAIN.EXE");
    }
    else
    {
        FUNCTION_MARKER(0x80023BB4u, "MAIN.EXE");
    }
    if (replace)
    {
        REPLAY_RECORD *previous = replay_state.playback;
        replay_state.playback = replay_state.capture;
        replay_state.capture = previous;
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

sint32 replay_append(const REPLAY_RECORD *record)
{
    FUNCTION_MARKER(0x80023C98u, "MAIN.EXE");
    if (!replay_state.capturing || replay_state.capture_count >= replay_state.capacity)
        return 0;
    replay_state.capture[replay_state.capture_count++] = *record;
    return 1;
}

void replay_record(const BOAT *boat)
{
    REPLAY_RECORD record;
    size_t axis;
    for (axis = 0; axis < 3; ++axis)
        record.position[axis] = (sint16)(uint16)boat->motion.position[axis];
    for (axis = 0; axis < 4; ++axis)
        record.quaternion[axis] = (sint16)(boat->motion.orientation.quaternion[axis] >> 13);
    replay_append(&record);
}

sint32 replay_advance(void)
{
    FUNCTION_MARKER(0x80023D6Cu, "MAIN.EXE");
    const REPLAY_RECORD *record;
    uint32 squared = 0;
    size_t axis;
    if (!replay_state.playing || replay_state.cursor >= replay_state.playback_count)
    {
        replay_state.playing = 0;
        return 0;
    }
    record = &replay_state.playback[replay_state.cursor++];
    for (axis = 0; axis < 3; ++axis)
    {
        sint32 delta = math_sub_wrap_s32(record->position[axis], replay_state.position[axis]);
        replay_state.velocity[axis] = math_mul_lo_s32(delta, 256);
        squared += (uint32)math_mul_lo_s32(replay_state.velocity[axis], replay_state.velocity[axis]);
        replay_state.position[axis] = record->position[axis];
        replay_state.matrix.t[axis] = record->position[axis];
    }
    replay_state.speed = SquareRoot0((sint32)squared);
    for (axis = 0; axis < 4; ++axis)
        replay_state.quaternion[axis] = (sint32)record->quaternion[axis] * 8192;
    quat_matrix(replay_state.quaternion, &replay_state.matrix);
    SetRotMatrix(&replay_state.matrix);
    SetTransMatrix(&replay_state.matrix);
    route_contact_update(&replay_state.contact);
    if (replay_state.cursor == replay_state.playback_count)
        replay_state.playing = 0;
    return 1;
}
