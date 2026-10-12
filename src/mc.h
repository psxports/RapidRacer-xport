#ifndef MC_H
#define MC_H

#include "psx.h"

typedef enum
{
    MC_IDLE,
    MC_SAVE,
    MC_SAVE_WAIT,
    MC_LOAD,
    MC_LOAD_WAIT,
    MC_OVERWRITE
} MC_PHASE;

typedef struct
{
    uint32 events[8];
    uint16 countdown;
    uint16 overwrite_prompt;
    uint16 overwrite_sel;
    uint16 format_sel;
    uint16 format_prompt;
    sint16 card_status;
    sint16 card;
    uint16 load_result;
    uint16 load_active;
    uint16 load_error;
    uint16 notice_scr;
    MC_PHASE phase;
    MC_PHASE prev_phase;
    uint16 format_pending;
    uint16 overwrite;
    uint16 poll_ticks;
    sint16 observed_status;
    uint32 save_size;
    uint32 save_checksum;
    uint32 file_count;
} MC;

extern MC mc_state;

enum
{
    MC_TITLE_CHARS = 38,
    MC_TITLE_BYTES = 2 * MC_TITLE_CHARS + 3,
    MC_SAVE_DATA_BYTES = 5684,
    MC_SAVE_HEADER_BYTES = 8,
    MC_SAVE_BYTES = MC_SAVE_HEADER_BYTES + MC_SAVE_DATA_BYTES,
    MC_FILE_HEADER_BYTES = 512,
    MC_SAVE_FILE_BYTES = 8192
};

sint32 mc_open_events(void);
sint32 mc_close_events(void);
sint32 mc_enable_events(void);
sint32 mc_disable_events(void);

sint16 mc_probe_status(sint32 card);

sint32 mc_format(sint32 card);

sint32 mc_load_state_update(void);
sint32 mc_load_icon(void);

sint32 mc_save_state_update(void);
sint32 mc_load_begin(void);
sint32 mc_poll_state(void);

#endif /* MC_H */
