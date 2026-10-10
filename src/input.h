#ifndef RR_INPUT_H
#define RR_INPUT_H

#include "psx.h"

struct RACE_VIEW;
#include <stddef.h>

typedef struct
{
    uint16 current;
    uint16 previous;
    uint16 pressed;
    sint16 type;
    uint8 packet[34];
} CONTROLLER_STATE;

typedef struct
{
    uint16 steer_center;
    uint16 steer_range;
    uint16 accel_range;
    uint16 brake_range;
    uint16 stick_center;
    uint16 stick_range;
} CONTROLLER_CALIBRATION;

typedef struct
{
    uint16 released;
    uint16 peak;
} INPUT_CALIB_STATE;

extern INPUT_CALIB_STATE input_calib;

extern CONTROLLER_CALIBRATION input_calibrations[2];
extern CONTROLLER_STATE input_controllers[2];
void input_init(void);
void input_decode(CONTROLLER_STATE *controller);
CONTROLLER_STATE *input_for_context(const struct RACE_VIEW *context);

uint32 input_update_states(void);
sint32 input_stop_exit(void);

#endif /* RR_INPUT_H */
