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
    MC_PHASE phase;
    uint16 format_pending;
    uint16 overwrite;
    uint16 poll_ticks;
    uint16 observed_status;
    uint32 save_size;
    uint32 save_checksum;
} MC_STATE;

extern MC_STATE mc_state;

enum
{
    MC_SAVE_DATA_BYTES = 5684,
    MC_SAVE_HEADER_BYTES = 8,
    MC_SAVE_BYTES = MC_SAVE_HEADER_BYTES + MC_SAVE_DATA_BYTES
};

sint32 mc_parse_checkpoint_data(void);
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
sint32 mc_enumerate_files(uint32 card, uint32 record);
sint32 mc_read_file_entries(uint32 name, sint32 count, sint32 card);
sint32 mc_fn_8004a320(sint32 card, uint32 name, uint32 output);
sint32 mc_fn_8004a3e0(sint32 card, uint32 name, uint32 input);
sint32 mc_probe(sint32 card, uint32 name);
sint32 mc_file_erase(uint32 card, uint32 name);
sint32 mc_calc_free_blocks(void);
sint32 mc_fn_8004ad04(uint32 source, uint32 destination, sint32 unused, uint8 group);
void mc_fn_8004ae78(void);
sint32 mc_copy_sized_bytes(uint32 source, uint32 destination, sint32 size);
void mc_fn_8004af00(void);
void mc_fn_8004af18(void);
void mc_error_route(sint16 command);
void mc_clear_countdown(void);
sint32 mc_start_countdown(void);
void mc_fn_8004b0a4(void);
void mc_fn_8004b0b4(sint16 command);
void mc_init_menu_status(void);
sint32 mc_fn_8004b3ec(uint32 name, uint32 output);
sint32 mc_fn_8004b490(uint32 name, uint32 input);

sint32 mc_save_segment_sizes_sum(sint32 unused, sint32 total);
sint32 mc_save_segments_gather(uint32 destination);
sint32 mc_save_segments_scatter(uint32 source);
sint32 mc_save_desc_checksum_calc(void);
sint32 mc_save_payload_validate(uint32 data);
sint32 mc_load_state_update(void);
sint32 mc_fn_8004bd0c(sint32 unused1, sint32 unused2, sint32 language, uint8 group);
sint32 mc_fn_8004beb0(void);
sint32 mc_load_begin(void);
sint32 mc_poll_state(void);
uint32 mc_file_find_first(const char *pattern, uint32 record);
sint32 mc_fn_8006780c(const char *path);
sint32 mc_fn_80067a8c(uint32 pad1, sint32 pad1_size, uint32 pad2, sint32 pad2_size);
sint32 mc_fn_80067a9c(void);
sint32 mc_fn_80067bbc(void);

void mc_fn_80074d30(uint32 card);
void mc_fn_80074d84(void);
void mc_fn_80074dec(uint32 card);
void mc_fn_80074dfc(void);

#endif /* RR_MC_H */
