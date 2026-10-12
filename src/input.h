#ifndef INPUT_H
#define INPUT_H

#include "psx.h"

struct RACE_VIEW;
#include <stddef.h>

typedef struct
{
    uint16 cur;
    uint16 prev;
    uint16 pressed;
    sint16 type;
    uint8 packet[34];
} PAD;

typedef struct
{
    sint16 steer_center;
    sint16 steer_range;
    sint16 accel_range;
    sint16 brake_range;
    sint16 stick_center;
    sint16 stick_range;
} CONTROLLER_CALIBRATION;

typedef struct
{
    uint16 released;
    sint16 peak;
} INPUT_CALIB;

extern INPUT_CALIB input_calib;

extern CONTROLLER_CALIBRATION input_calibrations[2];
extern PAD input_pads[2];
void input_init(void);

PAD *input_for_context(const struct RACE_VIEW *context);

uint32 input_update_states(void);
sint32 input_stop_exit(void);

#endif /* INPUT_H */
