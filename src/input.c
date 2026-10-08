#include "input.h"
#include "xport_trace.h"
#include "psx.h"
#include <stdlib.h>
#include <string.h>

CONTROLLER_STATE input_controllers[2];
CONTROLLER_CALIBRATION input_calibrations[2];

void input_init(void)
{
    memset(input_controllers, 0, sizeof(input_controllers));
    input_controllers[0].type = -1;
    input_controllers[1].type = -1;
    PadInitDirect(input_controllers[0].packet, input_controllers[1].packet);
}

CONTROLLER_STATE *input_for_context(uint32 context)
{
    // Player contexts remain legacy until the boat migration in stage 3
    uint32 player = r_u32(context + 4u);
    if (player >= 2u)
        abort();
    return &input_controllers[player];
}

static uint16 input_invert_swap16(uint16 value)
{
    uint16 inverted = (uint16)~value;

    return (uint16)((uint16)(inverted << 8) | (uint16)(inverted >> 8));
}

void input_decode(CONTROLLER_STATE *controller)
{
    const uint8 *packet = controller->packet;
    sint16 type = packet[0] != 0xFFu ? packet[1] >> 4 : -1;
    uint16 current;
    if (type != 2 && type != 4 && type != 7)
    {
        controller->current = controller->previous = controller->pressed = 0;
        controller->type = -1;
        return;
    }
    current = input_invert_swap16((uint16)(packet[2] | (uint16)packet[3] << 8));
    if (type == 2)
    {
        if (packet[5] >= 0x41u)
            current |= 0x0040u;
        if (packet[6] >= 0x41u)
            current |= 0x0080u;
        if (packet[7] >= 0x41u)
            current |= 0x0004u;
    }
    controller->type = type;
    controller->previous = controller->current;
    controller->current = current;
    controller->pressed = (controller->previous ^ current) & current;
}

uint32 input_update_states(void)
{
    FUNCTION_MARKER(0x80010000u, "MAIN.EXE");
    pad_publish(0, 1, input_invert_swap16((uint16)PadRead(0)));
    input_decode(&input_controllers[0]);
    input_decode(&input_controllers[1]);
    return 0;
}

sint32 input_stop_exit(void)
{
    FUNCTION_MARKER(0x800152A8u, "MAIN.EXE");
    PadStopCom();
    exit(EXIT_SUCCESS);
}
