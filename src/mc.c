#include "callbacks.h"
#include "tournament.h"
#include "game.h"
#include "profile.h"
#include "boat.h"
#include "text.h"
#include "cd.h"
#include "sound.h"
#include "sprite.h"
#include "upgrades.h"
#include "mc.h"
#include "arena.h"
#include "utils.h"
#include "menu.h"
#include "psx.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
    uint32 size;
    uint32 checksum;
    uint8 data[MC_SAVE_DATA_BYTES];
} MC_SAVE_PAYLOAD;

MC mc_state;

static const char mc_save_name[] = "BESCES-00394 RRACER";
static const char *const mc_bonus_names[3] = {"BESCES-00409 PORSCHE", "BISIPS-60016 PORSCHE", "BASCUS-94187 PORSCHE"};

enum
{
    MC_FILES_MAX = 15,
    MC_FILE_HDR_BYTES = 512
};

typedef struct
{
    uint8 prefix[3];
    uint8 blocks;
    uint8 payload[MC_FILE_HDR_BYTES - 4];
} MC_FILE_HEADER;

typedef char MC_FILE_HEADER_SIZE_CHECK[(sizeof(MC_FILE_HEADER) == MC_FILE_HDR_BYTES && offsetof(MC_FILE_HEADER, blocks) == 3) ? 1 : -1];

static DIRENTRY mc_dir[MC_FILES_MAX + 1];
static MC_FILE_HEADER mc_file_headers[MC_FILES_MAX];

typedef struct
{
    uint8 header[MC_FILE_HDR_BYTES];
    MC_SAVE_PAYLOAD payload;
    uint8 tail[MC_SAVE_FILE_BYTES - MC_FILE_HDR_BYTES - MC_SAVE_BYTES];
} MC_SAVE_FILE;

typedef char MC_SAVE_PAYLOAD_LAYOUT_CHECK[(sizeof(MC_SAVE_PAYLOAD) == MC_SAVE_BYTES && offsetof(MC_SAVE_PAYLOAD, checksum) == 4 && offsetof(MC_SAVE_PAYLOAD, data) == MC_SAVE_HEADER_BYTES) ? 1 : -1];

typedef char MC_SAVE_FILE_SIZE_CHECK[(sizeof(MC_SAVE_FILE) == MC_SAVE_FILE_BYTES && offsetof(MC_SAVE_FILE, payload) == MC_FILE_HDR_BYTES) ? 1 : -1];

typedef struct
{
    uint16 code;
    uint16 first;
} MC_TITLE_RANGE;

static const MC_TITLE_RANGE mc_title_ranges[3] = {{33359, 48}, {33376, 65}, {33409, 97}};
static const uint16 mc_title_punct[33] = {0x8140, 0x8149, 0x8168, 0x8194, 0x8190, 0x8193, 0x8195, 0x8166, 0x8169, 0x816A, 0x8196, 0x817B, 0x8143, 0x817C, 0x8144, 0x815E, 0x8146, 0x8147, 0x8171, 0x8181, 0x8172, 0x8148, 0x8197, 0x816D, 0x818F, 0x816E, 0x814F, 0x8151, 0x8165, 0x816F, 0x8162, 0x8170, 0x8150};

typedef struct
{
    uint8 pal[32];
    uint8 img[128];
    sint32 ready;
} MC_ICON;

static MC_ICON mc_icon;

typedef struct
{
    uint8 magic[2];
    uint8 icon_mode;
    uint8 blocks;
    uint8 title[MC_TITLE_BYTES];
    uint8 reserved0[96 - 4 - MC_TITLE_BYTES];
    uint8 pal[32];
    uint8 img[128];
    uint8 reserved1[MC_FILE_HEADER_BYTES - 256];
} MC_SAVE_HEADER;

typedef char MC_SAVE_HEADER_LAYOUT_CHECK[(sizeof(MC_SAVE_HEADER) == MC_FILE_HEADER_BYTES && offsetof(MC_SAVE_HEADER, title) == 4 && offsetof(MC_SAVE_HEADER, pal) == 96 && offsetof(MC_SAVE_HEADER, img) == 128) ? 1 : -1];

static sint32 mc_decode_icon(const uint8 *src, uint32 size)
{
    uint32 clut_bytes;
    uint32 pixel_block;
    uint32 pixel_bytes;

    memset(&mc_icon, 0, sizeof(mc_icon));
    if (!src || size < 20 || xport_load_le32(src) != 0x10 || (xport_load_le32(src + 4) & 8) == 0)
        return 0;
    clut_bytes = xport_load_le32(src + 8) & ~3u;
    if (clut_bytes < 44 || clut_bytes > size - 8)
        return 0;
    pixel_block = 8 + clut_bytes;
    if (size - pixel_block < 12)
        return 0;
    pixel_bytes = xport_load_le32(src + pixel_block);
    if (pixel_bytes < 140 || pixel_bytes > size - pixel_block)
        return 0;
    memcpy(mc_icon.pal, src + 20, sizeof(mc_icon.pal));
    memcpy(mc_icon.img, src + pixel_block + 12, sizeof(mc_icon.img));
    mc_icon.ready = 1;
    return 1;
}

sint32 mc_load_icon(void)
{
    const uint8 *src;
    uint32 size = 0;

    FUNCTION_MARKER(0x8004AEB4, "MAIN.EXE");
    strcpy(cd_dir, "\\");
    src = cd_load_file_bytes("MENU\\DATA\\RRBADGE.TIM", &size);
    return mc_decode_icon(src, size);
}

sint32 mc_open_events(void)
{
    static const uint32 specifications[4] = {4, 0x8000, 0x100, 0x2000};
    uint32 idx;

    FUNCTION_MARKER(0x800499E8, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
        mc_state.events[idx] = OpenEventGuest(0xF4000001, specifications[idx], 0x2000, 0);
    for (idx = 0; idx < 4; ++idx)
        mc_state.events[idx + 4] = OpenEventGuest(0xF0000011, specifications[idx], 0x2000, 0);
    return 0;
}

sint32 mc_close_events(void)
{
    uint32 idx;
    sint32 result = 0;

    FUNCTION_MARKER(0x80049AF0, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
        result = CloseEvent(mc_state.events[idx]);
    return result;
}

static sint32 mc_poll_primary_card_event(void)
{
    uint32 idx;
    uint32 attempt;

    FUNCTION_MARKER(0x80049EB0, "MAIN.EXE");
    for (attempt = 1; attempt <= 0x13880; ++attempt)
    {
        for (idx = 0; idx < 4; ++idx)
            if (TestEvent(mc_state.events[idx]) == 1)
                return idx;
    }
    return 4;
}

static sint32 mc_test_primary_card_events(void)
{
    uint32 idx;
    sint32 result = 0;

    FUNCTION_MARKER(0x80049F50, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
        result = TestEvent(mc_state.events[idx]);
    return result;
}

static sint32 mc_poll_secondary_card_event(void)
{
    uint32 idx;
    uint32 attempt;

    FUNCTION_MARKER(0x80049F98, "MAIN.EXE");
    for (attempt = 1; attempt <= 0x13880; ++attempt)
    {
        for (idx = 0; idx < 4; ++idx)
            if (TestEvent(mc_state.events[idx + 4]) == 1)
                return idx;
    }
    return 4;
}

static sint32 mc_test_secondary_card_events(void)
{
    uint32 idx;
    sint32 result = 0;

    FUNCTION_MARKER(0x8004A038, "MAIN.EXE");
    for (idx = 0; idx < 4; ++idx)
        result = TestEvent(mc_state.events[idx + 4]);
    return result;
}

static sint32 mc_header_check(sint32 card)
{
    uint8 header[128] = {0};

    FUNCTION_MARKER(0x8004A080, "MAIN.EXE");
    mc_test_secondary_card_events();
    // Original 0x80074CE8 and 0x80074CD8 dispatch BIOS B0:50 and B0:4F
    _new_card();
    _card_read(card, 0, header);
    if (mc_poll_secondary_card_event() != 0)
        return 0;
    return header[0] == 'M' && header[1] == 'C';
}

static sint32 mc_poll_load(sint32 card)
{
    sint32 event;

    FUNCTION_MARKER(0x80049C90, "MAIN.EXE");
    mc_test_secondary_card_events();
    mc_poll_secondary_card_event();
    mc_test_primary_card_events();
    event = mc_poll_primary_card_event();
    if (event == 0)
        return mc_header_check(card) == 0;
    if (event >= 1 && event <= 4)
        return event + 1;
    return -1;
}

sint32 mc_enable_events(void)
{
    uint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x80049B68, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
        EnableEvent(mc_state.events[idx]);
    mc_state.card = 0;
    result = mc_poll_load(0);
    mc_state.card_status = result;
    return result;
}

sint32 mc_disable_events(void)
{
    uint32 idx;

    FUNCTION_MARKER(0x80049C08, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
        DisableEvent(mc_state.events[idx]);
    return 0;
}

sint16 mc_probe_status(sint32 card)
{
    sint32 event;
    sint32 result;

    FUNCTION_MARKER(0x80049DEC, "MAIN.EXE");
    mc_test_primary_card_events();
    // Original 0x80074CA8 dispatches BIOS A0:AB
    _card_info(card);
    event = mc_poll_primary_card_event();
    if (event == 0)
        result = mc_header_check(card) == 0;
    else if (event == 1)
        result = 2;
    else if (event == 2)
        result = 3;
    else if (event == 4)
        result = 5;
    else
        result = 4;
    if (result == 4)
        return mc_poll_load(card);
    return result;
}

sint32 mc_format(sint32 card)
{
    FUNCTION_MARKER(0x8004A108, "MAIN.EXE");
    return 0;
}

static sint16 mc_scan_files(uint32 card)
{
    char pattern[128];
    DIRENTRY *entry = mc_dir;

    FUNCTION_MARKER(0x8004A224, "MAIN.EXE");
    snprintf(pattern, sizeof(pattern), "bu%.2x:*", card);
    mc_state.file_count = 0;
    // Original 0x800677EC dispatches BIOS B0:42
    if (firstfile(pattern, entry) == entry)
    {
        do
        {
            ++mc_state.file_count;
            ++entry;
            if (nextfile(entry) != entry)
                break;
        } while (mc_state.file_count < MC_FILES_MAX);
    }
    return mc_state.file_count;
}

static sint32 mc_read_header(sint32 card, const char *name, MC_FILE_HEADER *dst)
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004A320, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 1);
    if (fd < 0)
    {
#ifndef NDEBUG
        printf("ERROR: The file could not be opened\n");
#endif
        return 0;
    }
    bytes = readPSX(fd, dst, MC_FILE_HDR_BYTES);
#ifndef NDEBUG
    if (bytes != MC_FILE_HDR_BYTES)
        printf("ERROR: During reading   Read: [%d]\n", bytes);
#endif
    closePSX(fd);
    return bytes == MC_FILE_HDR_BYTES;
}

static void mc_read_headers(sint32 count, sint32 card)
{
    sint32 idx;

    FUNCTION_MARKER(0x8004A2A0, "MAIN.EXE");
    if (count > MC_FILES_MAX)
        count = MC_FILES_MAX;
    for (idx = 0; idx < count; ++idx)
        mc_read_header(card, mc_dir[idx].name, &mc_file_headers[idx]);
}

static sint32 mc_write_header(sint32 card, const char *name, const uint8 src[MC_FILE_HEADER_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004A3E0, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 2);
    if (fd < 0)
        return 0;
    bytes = writePSX(fd, src, MC_FILE_HEADER_BYTES);
    closePSX(fd);
    return (uint32)bytes >= MC_FILE_HEADER_BYTES;
}

static uint8 mc_probe(sint32 card, const char *name)
{
    char path[64];
    sint32 fd;

    FUNCTION_MARKER(0x8004A4A8, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 2);
#ifndef NDEBUG
    if (fd >= 0)
        printf(" file exists.\n");
#endif
    closePSX(fd);
    return fd >= 0;
}

static sint32 mc_file_erase(uint32 card, const char *name)
{
    char path[64];

    FUNCTION_MARKER(0x8004A524, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", card, name);
    // Original 0x8006780C dispatches BIOS B0:45
    return erase(path);
}

static sint16 mc_calc_free_blocks(void)
{
    sint32 count = mc_state.file_count;
    uint32 used = 0;
    sint16 idx = 0;
    sint16 used_low;

    FUNCTION_MARKER(0x8004A760, "MAIN.EXE");
    if (count > MC_FILES_MAX)
        count = MC_FILES_MAX;
    if (count > 0)
    {
        do
        {
            uint32 blocks = mc_file_headers[idx].blocks;
            used += blocks != 0 ? blocks : 1u;
            idx++;
        } while (idx < count);
    }
    used_low = used;
    if (used_low >= 16)
        used_low = 15;
    return 15u - (uint16)used_low;
}

static sint32 mc_encode_title(const char *src, uint8 dst[MC_TITLE_BYTES])
{
    uint32 len;
    uint32 idx;

    FUNCTION_MARKER(0x8004AD04, "MAIN.EXE");
    for (len = 0; src[len] != '\0'; ++len)
    {
        uint8 ch = src[len];

        if (len >= MC_TITLE_CHARS)
            return -1;
        if (ch < 32u || ch > 126u)
            exit(EXIT_FAILURE);
    }
    for (idx = 0; idx < MC_TITLE_CHARS; ++idx)
    {
        dst[2 * idx] = 0x81;
        dst[2 * idx + 1] = 0x40;
    }
    for (idx = 0; idx < len; ++idx)
    {
        uint8 ch = src[idx];
        uint32 grp = 0;
        uint32 offset = 0;
        uint16 code;

        if (ch < 48u)
            offset = 1;
        else if (ch < 58u)
            grp = 0;
        else if (ch < 65u)
            offset = 11;
        else if (ch < 91u)
            grp = 1;
        else if (ch < 97u)
            offset = 37;
        else if (ch < 123u)
            grp = 2;
        else
            offset = 63;
        if (offset != 0)
            code = mc_title_punct[ch - (offset + 31)];
        else
            code = mc_title_ranges[grp].code + ch - mc_title_ranges[grp].first;
        dst[2 * idx] = code >> 8;
        dst[2 * idx + 1] = code;
    }
    dst[2 * len] = 0;
    dst[2 * len + 1] = 0;
    dst[2 * len + 2] = 0;
    return 0;
}

static void mc_error_route(sint16 command)
{
    sint32 flag = -1;

    FUNCTION_MARKER(0x8004AF20, "MAIN.EXE");
    if (mc_state.load_active != 0)
    {
        mc_state.load_error = command;
        return;
    }
    if (command == -10)
    {
        menu_clear_rec_bytes();
        flag = 25;
    }
    else if (command == -4)
    {
        menu_clear_rec_bytes();
        flag = 4;
    }
    else if (command == -3)
        flag = 5;
    else if (command == -1 || command == -6)
    {
        menu_clear_rec_bytes();
        menu_update_flag_idx(18, 1);
        flag = 19;
    }
    if (flag >= 0)
        menu_update_flag_idx(flag, 1);
}

static void mc_clear_countdown(void)
{
    FUNCTION_MARKER(0x8004B080, "MAIN.EXE");
    mc_state.countdown = 0;
}

static sint32 mc_start_countdown(void)
{
    FUNCTION_MARKER(0x8004B08C, "MAIN.EXE");
    mc_state.countdown = 150;
    return 150;
}

static void mc_save_notice(sint16 command)
{
    sint32 flag = -1;

    FUNCTION_MARKER(0x8004B0B4, "MAIN.EXE");
    if (command == -9)
    {
        menu_clear_rec_bytes();
        menu_update_flag_idx(6, 1);
        flag = 7;
    }
    else if (command == -8)
    {
        menu_clear_rec_bytes();
        menu_update_flag_idx(6, 1);
        menu_update_flag_idx(8, 1);
        flag = 9;
    }
    else if (command == -7)
    {
        menu_clear_rec_bytes();
        menu_update_flag_idx(16, 1);
        flag = 17;
    }
    else if (command == -3)
        mc_start_countdown();
    else if (command == -2)
        mc_clear_countdown();
    else if (command == -1 || command == -6)
    {
        menu_clear_rec_bytes();
        menu_update_flag_idx(18, 1);
        flag = 19;
    }
    if (flag >= 0)
        menu_update_flag_idx(flag, 1);
}

static void mc_init_menu_status(void)
{
    sint16 result;

    FUNCTION_MARKER(0x8004B224, "MAIN.EXE");
    mc_state.card = 0;
    result = mc_probe_status(0);
    if (result != 5)
        mc_state.card_status = result;
}

static sint32 mc_read_save(const char *name, uint8 dst[MC_SAVE_FILE_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes = 0;

    FUNCTION_MARKER(0x8004B3EC, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)mc_state.card, name);
    fd = openPSX(path, 1);
    if (fd >= 0)
        bytes = readPSX(fd, dst, MC_SAVE_FILE_BYTES);
    if (fd < 0 || bytes != MC_SAVE_FILE_BYTES)
    {
        bytes = 0;
    }
    closePSX(fd);
    return bytes;
}

static sint32 mc_write_save(const char *name, const uint8 src[MC_SAVE_FILE_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004B490, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)mc_state.card, name);
    fd = openPSX(path, 2);
    if (fd >= 0)
    {
        closePSX(fd);
        return 0;
    }
    fd = openPSX(path, 0x10200);
    if (fd == -1)
    {
        return 0;
    }
    closePSX(fd);
    fd = openPSX(path, 2);
    if (fd < 0)
    {
        return 0;
    }
    bytes = writePSX(fd, src, MC_SAVE_FILE_BYTES);
    if (bytes != MC_SAVE_FILE_BYTES)
    {
        bytes = 0;
    }
    closePSX(fd);
    return bytes;
}

sint32 mc_save_encode(MC_SAVE_PAYLOAD *dst)
{
    FUNCTION_MARKER(0x8004B644, "MAIN.EXE");
    // Typed little-endian payload preserves the eight-byte PS1 save header
    dst->size = mc_state.save_size;
    dst->checksum = mc_state.save_checksum;
    for (uint32 idx = 0; idx < MC_SAVE_DATA_BYTES; ++idx)
        dst->data[idx] = profile_save_read_byte(idx);
    return 0;
}

sint32 mc_save_decode(const MC_SAVE_PAYLOAD *src)
{
    FUNCTION_MARKER(0x8004B6BC, "MAIN.EXE");
    tournament_clear_extra_rounds();
    mc_state.save_size = src->size;
    mc_state.save_checksum = src->checksum;
    for (uint32 idx = 0; idx < MC_SAVE_DATA_BYTES; ++idx)
        profile_save_write_byte(idx, src->data[idx]);
    return 0;
}

static sint32 mc_save_desc_checksum_calc(void)
{
    uint32 idx;
    uint32 sum = 0;

    FUNCTION_MARKER(0x8004B734, "MAIN.EXE");
    mc_state.save_size = MC_SAVE_DATA_BYTES;
    for (idx = 0; idx < MC_SAVE_DATA_BYTES; ++idx)
        sum += profile_save_read_byte(idx);
    mc_state.save_checksum = sum;
    return sum;
}

static sint32 mc_save_validate(const MC_SAVE_PAYLOAD *src)
{
    uint32 size;
    uint32 expected;
    uint32 sum = 0;
    uint32 idx;

    FUNCTION_MARKER(0x8004B7E0, "MAIN.EXE");
    mc_save_desc_checksum_calc();
    size = src->size;
    expected = src->checksum;
    if (size != MC_SAVE_DATA_BYTES)
        return 2;
    for (idx = 0; idx < size; ++idx)
        sum += src->data[idx];
    if (mc_state.save_size != size)
        return 2;
    return expected != sum;
}

sint32 mc_load_state_update(void)
{
    sint32 phase;
    sint32 card;
    sint32 load_result = 0;
    sint32 validation = 0;
    sint32 handled = 0;
    sint32 save_found = 1;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004B90C, "MAIN.EXE");
    if (mc_state.countdown != 0)
        mc_state.countdown--;
    phase = mc_state.phase;
    if (phase == MC_LOAD && mc_state.countdown == 0)
    {
        uint8 old_language = menu_state.language;

        ResetGraph(0);
        mc_init_menu_status();
        card = mc_state.card;
        if (mc_probe(card, mc_save_name) != 0)
        {
            MC_SAVE_FILE file = {0};
            load_result = mc_read_save(mc_save_name, (uint8 *)&file);
            if ((sint16)load_result != 0)
            {
                validation = mc_save_validate(&file.payload);
                if ((sint16)validation != 0)
                    load_result = 0;
                else
                {
                    mc_save_decode(&file.payload);
                    upgrades_refresh_grid_limits();
                }
            }
        }
        else
            save_found = 0;
        mc_state.countdown = 150;
        mc_state.phase = MC_LOAD_WAIT;
        menu_clear_rec_bytes();
        if ((sint16)load_result != 0)
        {
            menu_update_flag_idx(3, 1);
            game_selection.flags = game_selection.flags & 0xFF00u | 0;
            if (mc_probe(card, mc_bonus_names[0]) != 0 || mc_probe(card, mc_bonus_names[1]) != 0 || mc_probe(card, mc_bonus_names[2]) != 0)
            {
                if ((game_selection.flags & 0x20u) != 0)
                    game_selection.flags = game_selection.flags & 0xFF00u | (uint8)((uint8)game_selection.flags - 32u);
                menu_state.sound = 3;
                game_selection.flags = game_selection.flags & 0xFF00u | (uint8)((uint8)game_selection.flags + 32u);
            }
            else if ((game_selection.flags & 0x20u) != 0)
                game_selection.flags = game_selection.flags & 0xFF00u | (uint8)((uint8)game_selection.flags - 32u);
            menu_tex_refresh_if_needed();
            if (old_language != menu_state.language)
                menu_set_selection(24, 0);
            sound_mix.music = sound_options.music;
            sound_mix.effects = sound_options.effects;
            voice_set_volume(7, (sint32)((uint32)(sint16)sound_options.music << 8));
            boat_racer_count = 8;
            boat_leader_count = 8;
            boat_trailer_count = 0;
            handled = 1;
        }
        else
        {
            sint16 card_state = mc_state.card_status;
            if (save_found == 0 && card_state != 3)
            {
                mc_error_route(-3);
                handled = 1;
            }
            if ((sint16)validation != 0)
            {
                mc_error_route((sint16)validation == 1 ? -4 : -10);
                handled = 1;
            }
            else if (card_state == 1)
            {
                mc_error_route(-2);
                handled = 1;
            }
            else if (card_state == 2)
            {
                mc_error_route(-5);
                handled = 1;
            }
            else if (card_state == 3)
            {
                mc_state.countdown = 250;
                mc_error_route(-1);
                handled = 1;
            }
        }
        mc_state.load_result = load_result;
        if (handled == 0)
        {
            sint16 card_state = mc_probe_status(card);
            mc_state.card_status = card_state;
            if (card_state == 3)
            {
                mc_state.countdown = 250;
                mc_error_route(-6);
            }
        }
        phase = mc_state.phase;
    }
    if (phase == MC_LOAD_WAIT && mc_state.countdown == 0)
    {
        sint32 idx;
        mc_state.prev_phase = MC_LOAD_WAIT;
        mc_state.phase = MC_IDLE;
        for (idx = 0; idx < 26; ++idx)
        {
            text_menu[idx + 8].visible = 0;
            result = 1;
        }
    }
    else if (phase == MC_LOAD_WAIT)
        result = (sint16)mc_state.countdown;
    return result;
}

static sint32 mc_save_header(void)
{
    MC_SAVE_HEADER header = {0};

    FUNCTION_MARKER(0x8004BD0C, "MAIN.EXE");
    if (!mc_icon.ready)
        return 0;
    mc_encode_title("RAPID RACER", header.title);
    header.magic[0] = 'S';
    header.magic[1] = 'C';
    header.icon_mode = 0x11;
    header.blocks = 1;
    memcpy(header.pal, mc_icon.pal, sizeof(header.pal));
    memcpy(header.img, mc_icon.img, sizeof(header.img));
    return mc_write_header(mc_state.card, mc_save_name, (const uint8 *)&header);
}

sint32 mc_save_state_update(void)
{
    sint32 phase;
    sint32 card;
    sint32 card_state;
    sint32 save_exists;
    sint32 free_blocks;
    sint32 file_exists = 0;
    sint32 save_result = 0;
    sint32 handled = 0;
    sint32 count;
    sint32 idx;

    FUNCTION_MARKER(0x8004BEB0, "MAIN.EXE");
    if (mc_state.countdown != 0)
        mc_state.countdown--;
    phase = mc_state.phase;
    if (phase == MC_SAVE && mc_state.countdown == 0)
    {
        card = mc_state.card;
        count = mc_scan_files(card);
        mc_read_headers(count, card);
        save_exists = mc_probe(card, mc_save_name);
        if (mc_state.card_status != 0)
            free_blocks = 1;
        else
            free_blocks = mc_calc_free_blocks();
        if (save_exists != 0)
        {
            if (mc_state.overwrite != 0)
                file_exists = 1;
        }
        else if (free_blocks != 0)
            file_exists = 1;
        if (file_exists != 0)
        {
            if (mc_probe(card, mc_save_name) != 0 && mc_state.overwrite == 0)
                save_exists = 1;
            else
            {
                MC_SAVE_FILE file = {0};
                mc_save_desc_checksum_calc();
                mc_save_encode(&file.payload);
                mc_file_erase(card, mc_save_name);
                save_result = mc_write_save(mc_save_name, (const uint8 *)&file);
                mc_save_header();
                mc_state.countdown = 150;
                mc_state.phase = MC_SAVE_WAIT;
            }
        }
        menu_clear_rec_bytes();
        mc_state.phase = MC_SAVE_WAIT;
        mc_state.overwrite_prompt = 0;
        mc_state.format_prompt = 0;
        mc_state.countdown = 250;
        if ((sint16)save_result != 0)
        {
            menu_update_flag_idx(1, 1);
            handled = 1;
        }
        else if ((sint16)save_exists == 0)
        {
            card_state = mc_state.card_status;
            if (card_state == 1)
            {
                mc_save_notice(-2);
                mc_state.format_prompt = 1;
                mc_state.countdown = 0;
                handled = 1;
            }
            else if (card_state == 2)
            {
                mc_save_notice(-5);
                handled = 1;
            }
            else if (card_state == 3)
            {
                mc_state.countdown = 250;
                mc_save_notice(-1);
                handled = 1;
            }
        }
        else
        {
            mc_save_notice(-7);
            free_blocks = 1;
            mc_state.countdown = 0;
            mc_state.phase = MC_OVERWRITE;
            mc_state.overwrite_prompt = 1;
            handled = 1;
        }
        if (handled == 0)
        {
            card_state = mc_probe_status(card);
            mc_state.card_status = card_state;
            if (card_state == 3)
                mc_save_notice(-6);
            else if ((sint16)free_blocks == 0 && mc_state.overwrite == 0)
                mc_save_notice(-9);
        }
        phase = mc_state.phase;
    }
    if (phase == MC_SAVE_WAIT && mc_state.countdown == 0)
    {
        mc_state.prev_phase = MC_SAVE_WAIT;
        mc_state.phase = MC_IDLE;
        for (idx = 0; idx < 26; ++idx)
            text_menu[idx + 8].visible = 0;
        return 1;
    }
    return phase;
}

sint32 mc_load_begin(void)
{
    sint32 error;
    sint32 result;

    FUNCTION_MARKER(0x8004CB04, "MAIN.EXE");
    mc_state.load_active = 1;
    mc_state.load_error = 0;
    mc_state.countdown = 0;
    mc_state.phase = MC_LOAD;
    mc_state.card = 0;
    mc_probe_status(0);
    mc_load_state_update();
    error = (sint16)mc_state.load_error;
    mc_state.load_active = 0;
    if (error != 0)
    {
        sint32 message = error == -6 || error == -1 ? 47 : 1;
        if (message == 1)
            mc_calc_free_blocks();
        mc_state.notice_scr = message;
    }
    result = mc_probe(mc_state.card, mc_save_name);
    if (result == 0)
    {
        sint16 count = mc_scan_files(mc_state.card);
        mc_read_headers(count, mc_state.card);
        if (mc_calc_free_blocks() == 0)
            mc_state.notice_scr = 52;
        return 52;
    }
    return (uint32)(uint16)result << 16;
}

sint32 mc_poll_state(void)
{
    uint16 counter;
    sint32 card;
    sint32 state;
    sint32 count;

    FUNCTION_MARKER(0x8004CC50, "MAIN.EXE");
    counter = mc_state.poll_ticks + 1u;
    mc_state.poll_ticks = counter;
    if ((sint16)counter < 5)
        return 1;
    card = mc_state.card;
    mc_state.poll_ticks = 0;
    state = mc_probe_status(card);
    if (state >= 1 && state <= 4)
        return 1;
    card = mc_state.card;
    if (mc_probe(card, mc_save_name) != 0)
        return 0;
    card = mc_state.card;
    count = mc_scan_files(card);
    card = mc_state.card;
    mc_read_headers(count, card);
    card = mc_state.card;
    if (mc_calc_free_blocks() != 0)
        return 0;
    mc_state.notice_scr = 52;
    return 1;
}
