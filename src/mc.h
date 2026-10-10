#ifndef RR_MC_H
#define RR_MC_H

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
    uint16 format_selected;
    uint16 format_prompt;
    uint16 card_status;
    uint16 card;
    uint16 load_result;
    uint16 load_active;
    uint16 load_error;
    uint16 notice_screen;
    MC_PHASE phase;
    MC_PHASE prev_phase;
    uint16 format_pending;
    uint16 overwrite;
    uint16 poll_ticks;
    uint16 observed_status;
    uint32 save_size;
    uint32 save_checksum;
    uint32 file_count;
} MC_STATE;

extern MC_STATE mc_state;

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

void mc_fn_800499e0(void);
sint32 mc_open_events(void);
sint32 mc_close_events(void);
sint32 mc_enable_events(void);
sint32 mc_disable_events(void);
sint32 mc_poll_load(sint32 card);
sint32 mc_probe_status(sint32 card);
sint32 mc_poll_primary_card_event(void);
sint32 mc_test_primary_card_events(void);
sint32 mc_poll_secondary_card_event(void);
sint32 mc_test_secondary_card_events(void);
sint32 mc_header_check(sint32 card);
sint32 mc_fn_8004a108(sint32 card);
sint32 mc_scan_files(uint32 card);
void mc_read_headers(sint32 count, sint32 card);
sint32 mc_write_header(sint32 card, const char *name, const uint8 src[MC_FILE_HEADER_BYTES]);
sint32 mc_probe(sint32 card, const char *name);
sint32 mc_file_erase(uint32 card, const char *name);
sint32 mc_calc_free_blocks(void);
sint32 mc_encode_title(const char *src, uint8 dst[MC_TITLE_BYTES]);
void mc_fn_8004ae78(void);
void mc_fn_8004af00(void);
void mc_fn_8004af18(void);
void mc_error_route(sint16 command);
void mc_clear_countdown(void);
sint32 mc_start_countdown(void);
void mc_fn_8004b0a4(void);
void mc_fn_8004b0b4(sint16 command);
void mc_init_menu_status(void);
sint32 mc_read_save(const char *name, uint8 dst[MC_SAVE_FILE_BYTES]);
sint32 mc_write_save(const char *name, const uint8 src[MC_SAVE_FILE_BYTES]);

sint32 mc_save_encode(uint8 dst[MC_SAVE_BYTES]);
sint32 mc_save_decode(const uint8 src[MC_SAVE_BYTES]);
sint32 mc_save_desc_checksum_calc(void);
sint32 mc_save_validate(const uint8 data[MC_SAVE_BYTES]);
sint32 mc_load_state_update(void);
sint32 mc_load_icon(void);
sint32 mc_save_header(void);
sint32 mc_save_state_update(void);
sint32 mc_load_begin(void);
sint32 mc_poll_state(void);

void mc_fn_80074d30(uint32 card);
void mc_fn_80074d84(void);
void mc_fn_80074dec(uint32 card);
void mc_fn_80074dfc(void);

#endif /* RR_MC_H */
