#include "game.h"
#include "input.h"
#include "xport_trace.h"
#include "psx.h"
#include <stdlib.h>
#include <string.h>

PAD input_pads[2];
CONTROLLER_CALIBRATION input_calibrations[2];
INPUT_CALIB input_calib;

void input_init(void)
{
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

static void input_decode(PAD *pad)
{
    const uint8 *packet = pad->packet;
    sint16 type = packet[0] != 0xFF ? packet[1] >> 4 : -1;
    uint16 cur;
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
    input_decode(&input_pads[0]);
    input_decode(&input_pads[1]);
    return 0;
}

sint32 input_stop_exit(void)
{
    FUNCTION_MARKER(0x800152A8, "MAIN.EXE");
    PadStopCom();
    exit(EXIT_SUCCESS);
}
