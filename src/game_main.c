#include "game.h"
#include "game_main.h"
#include "polygon.h"
#include "callbacks.h"
#include "psx.h"
#include "psx_gpu.h"
#include "psx_spu.h"
#include "sound_data.h"
#include "sound.h"
#include "xport_trace.h"
#include "initial_dram.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const uint32 xport_gpu_graph_type_address = 0x800A0C38u;
const uint32 xport_cd_sync_callback_address = 0x800A0E88u;
const uint32 xport_cd_ready_callback_address = 0x800A0E8Cu;
const uint32 xport_cd_status_address = 0x800A0E98u;
const uint32 xport_cd_setloc_table_address = 0x800A0E00u;
const uint32 xport_spu_register_pointer_address = 0x800A173Cu;

void xport_bind_native_spu_transfer(void)
{
    if (!sound_bind_spu_transfer())
        abort();
}

sint32 cd_host_file_size(uint32 guest_path, size_t *size)
{
    return xport_cd_file_size(guest_path, "DATA", size);
}

sint32 cd_read_host_file(uint32 guest_path, uint32 destination, size_t capacity, size_t *size, uint32 command_fields, uint32 response_field, uint32 initial_sectors_per_field, uint32 sectors_per_field, uint32 completion_fields, uint32 setup_field, uint32 fixed_buffer)
{
    return xport_cd_file_read(guest_path, "DATA", destination, capacity, size);
}

static sint32 game_load_initial_dram(void)
{
    uint32 word_index;
    uint32 value_index = 0u;

    if (!xport_guest_fill(0u, 0u, PSX_DRAM_SIZE))
        return 0;
    for (word_index = 0u; word_index < RR_INITIAL_DRAM_WORD_COUNT; ++word_index)
    {
        if ((rr_initial_dram_bitmap[word_index >> 3] & (1u << (word_index & 7u))) == 0u)
            continue;
        if (value_index >= RR_INITIAL_DRAM_VALUE_COUNT || !w_u32(RR_INITIAL_DRAM_LOAD_ADDRESS + word_index * 4u, rr_initial_dram_values[value_index++]))
            return 0;
    }
    return value_index == RR_INITIAL_DRAM_VALUE_COUNT && w_u32(0x000A12A0u, 0x80101818u) && w_u32(0x000A12A4u, 0x000FE3E0u);
}

void xport_main(void)
{
    if (!game_load_initial_dram())
    {
        xport_set_exit_code(2);
        return;
    }
    spu_bind_reverb_presets(sound_reverb_work, sound_reverb_presets, 3u);
    game_start();
}
