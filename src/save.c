#include "save.h"
#include "profile.h"
#include "sound.h"
#include "tournament.h"
#include "upgrades.h"
#include "name.h"
#include "xport.h"

enum
{
    SAVE_VERSION = 1,
    SAVE_DATA_BYTES = 5684,
    SAVE_HEADER_BYTES = 4
};

static uint8 save_data[SAVE_HEADER_BYTES + SAVE_DATA_BYTES];
sint32 save_loaded;

// Keep persistent fields at their existing codec offsets
static sint32 save_persistent(uint32 offset)
{
    static const struct
    {
        uint16 first, end;
    } spans[] = {
        {4, 6}, {8, 10}, {16, 23}, {24, 29}, {45, 51}, {55, 56}, {60, 1458}, {1600, 4948}, {4984, 5420}, {5636, 5643}, {5664, 5668},
    };

    for (uint32 idx = 0; idx < sizeof(spans) / sizeof(spans[0]); ++idx)
        if (offset >= spans[idx].first && offset < spans[idx].end)
            return 1;
    return 0;
}

void save_load(void)
{
    sint32 size = xport_load(save_data, sizeof(save_data));
    if (size != (sint32)sizeof(save_data) || xport_load_le32(save_data) != SAVE_VERSION)
        return;
    tournament_clear_extra_rounds();
    for (uint32 idx = 0; idx < SAVE_DATA_BYTES; ++idx)
        if (save_persistent(idx))
            profile_save_write_byte(idx, save_data[SAVE_HEADER_BYTES + idx]);
    for (uint32 idx = 0; idx < 5; ++idx)
        profile_at(idx)->reset_pending = 0;
    upgrades_refresh_grid_limits();
    sound_mix.music = sound_options.music;
    sound_mix.effects = sound_options.effects;
    name_update_cheats();
    save_loaded = 1;
}

void save_flush(void)
{
    xport_store_le32(save_data, SAVE_VERSION);
    for (uint32 idx = 0; idx < SAVE_DATA_BYTES; ++idx)
        save_data[SAVE_HEADER_BYTES + idx] = save_persistent(idx) ? profile_save_read_byte(idx) : 0;
    // Preserve the Porsche unlock and activation without startup-only cheats
    save_data[SAVE_HEADER_BYTES + 8] &= 0x28;
    save_data[SAVE_HEADER_BYTES + 9] = 0;
    xport_save(save_data, sizeof(save_data));
}
