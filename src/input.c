#include "game.h"
#include "input.h"
#include "xport_trace.h"
#include "psx.h"
#include "boat.h"
#include "sound.h"
#include <stdlib.h>
#include <string.h>

PAD input_pads[2];
CONTROLLER_CALIBRATION input_calibrations[2];
INPUT_CALIB input_calib;

typedef struct
{
    sint32 prev;
    float value;
} INPUT_VIBRATION;

static INPUT_VIBRATION input_vibration[2][4];

void input_stop_vibration(void)
{
    memset(input_vibration, 0, sizeof(input_vibration));
    for (uint32 player = 0; player < 2; ++player)
    {
        PAD *pad = &input_pads[player];
        pad->act[0] = pad->act[1] = 0;
        PadSetAct(16 * player, pad->act, pad->act_count);
    }
}

void input_init(void)
{
    memset(input_vibration, 0, sizeof(input_vibration));
    memset(input_pads, 0, sizeof(input_pads));
    input_pads[0].type = -1;
    input_pads[1].type = -1;
    PadInitDirect(input_pads[0].packet, input_pads[1].packet);
    PadStartCom();
}

PAD *input_for_context(const RACE_VIEW *context)
{
    if (context == NULL || context->idx >= 2)
        abort();
    return &input_pads[context->idx];
}

static uint16 input_invert_swap16(uint16 val)
{
    uint16 inverted = ~val;

    return (uint16)(inverted << 8) | inverted >> 8;
}

static void input_decode(PAD *pad, uint32 player)
{
    const uint8 *packet = pad->packet;
    sint16 type = packet[0] != 0xFF ? packet[1] >> 4 : -1;
    uint16 cur;
    if (type != pad->type)
    {
        memset(input_vibration[player], 0, sizeof(input_vibration[player]));
        pad->act_ready = pad->act_count = 0;
        pad->act[0] = pad->act[1] = 0;
    }
    if (!pad->act_ready && type != -1 && PadGetState(16 * player) == 6)
    {
        pad->act_count = (uint8)clamp(PadInfoAct(16 * player, -1, 0), 0, 2);
        memset(pad->act_align, 0xFF, sizeof(pad->act_align));
        for (uint32 idx = 0; idx < pad->act_count; ++idx)
            pad->act_align[idx] = pad->act_types[idx] = (uint8)PadInfoAct(16 * player, idx, 3);
        pad->act_ready = 1;
    }
    if (type != 2 && type != 4 && type != 7)
    {
        pad->cur = pad->prev = pad->pressed = 0;
        pad->type = -1;
        return;
    }
    cur = input_invert_swap16(packet[2] | packet[3] << 8);
    if (type == 2)
    {
        if (packet[5] >= 0x41)
            cur |= PADRdown;
        if (packet[6] >= 0x41)
            cur |= PADRleft;
        if (packet[7] >= 0x41)
            cur |= PADL1;
    }
    pad->type = type;
    pad->prev = pad->cur;
    pad->cur = cur;
    pad->pressed = (pad->prev ^ cur) & cur;
}

uint32 input_update_states(void)
{
    FUNCTION_MARKER(0x80010000, "MAIN.EXE");
    pad_publish(0, 1, input_invert_swap16(PadRead(0)));
    input_decode(&input_pads[0], 0);
    input_decode(&input_pads[1], 1);
    return 0;
}

static sint32 input_vibration_step(sint32 value, sint32 kind)
{
    if (kind == 2)
        return 128;
    if (kind == 3)
        return 1 + (value >> 6);
    return value >= 9 ? value / 8 : 1;
}

static sint32 input_vibration_decay(INPUT_VIBRATION *state, sint32 src, sint32 kind)
{
    sint32 expected = state->prev - input_vibration_step(state->prev, kind);

    // Preserve new stimuli and shorten only the existing PAL sound tails
    if (src <= 0 || src > expected)
        state->value = (float)src;
    else
    {
        state->value -= input_vibration_step((sint32)state->value, kind) * (60.0f / FIELD_RATE);
        if (state->value < 0.0f)
            state->value = 0.0f;
    }
    state->prev = src;
    return (sint32)state->value;
}

void input_update_vibration(const RACE_VIEW *context)
{
    // NTSC MAIN.EXE 80010358 reuses the sound contact and impact envelopes
    PAD *pad = input_for_context(context);
    if (!pad->act_ready || pad->act_count == 1)
        return;
    const SOUND_PLAYER *sound = sound_for_context(context);
    INPUT_VIBRATION *state = input_vibration[context->idx];
    sint32 contact0 = input_vibration_decay(&state[0], (sint16)sound->contact[0], 0);
    sint32 contact1 = input_vibration_decay(&state[1], (sint16)sound->contact[1], 1);
    sint32 pitch = input_vibration_decay(&state[2], (sint16)sound->pitch, 2);
    sint32 ground = 4 * input_vibration_decay(&state[3], (sint16)sound->ground_decay, 3);
    if (ground < 96)
        ground = 0;
    else if (ground >= 197)
        ground = 196;
    sint32 strength = 2 * (contact0 + contact1);
    if (strength < pitch / 16)
        strength = pitch / 16;
    strength = clamp(strength, ground, 255);
    if (boat_player_menu(context)->mode != 4)
    {
        memset(state, 0, sizeof(input_vibration[context->idx]));
        strength = 0;
    }
    uint8 changed = 0;
    for (uint32 idx = 0; idx < pad->act_count; ++idx)
    {
        uint8 value = pad->act[idx];
        if (!strength)
            value = 0;
        else if (pad->act_types[idx] == 0 && !value && strength != ground)
            value = 1;
        else if (pad->act_types[idx] == 1)
            value = (uint8)strength;
        changed |= value != pad->act[idx];
        pad->act[idx] = value;
    }
    if (changed)
    {
        PadSetActAlign(16 * context->idx, pad->act_align);
        PadSetAct(16 * context->idx, pad->act, pad->act_count);
    }
}

sint32 input_stop_exit(void)
{
    FUNCTION_MARKER(0x800152A8, "MAIN.EXE");
    PadStopCom();
    exit(EXIT_SUCCESS);
}
