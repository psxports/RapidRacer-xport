#include "tournament.h"
#include "game.h"
#include "profile.h"
#include "vehicle.h"
#include "text.h"
#include "cd.h"
#include "game_main.h"
#include "sound.h"
#include "sprite.h"
#include "vehicle_select.h"
#include "runtime.h"
#include "mc.h"
#include "arena.h"
#include "global.h"
#include "menu.h"
#include "psx.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

MC_STATE mc_state;

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
    uint8 payload[MC_SAVE_BYTES];
    uint8 tail[MC_SAVE_FILE_BYTES - MC_FILE_HDR_BYTES - MC_SAVE_BYTES];
} MC_SAVE_FILE;

typedef char MC_SAVE_FILE_SIZE_CHECK[(sizeof(MC_SAVE_FILE) == MC_SAVE_FILE_BYTES && offsetof(MC_SAVE_FILE, payload) == MC_FILE_HDR_BYTES) ? 1 : -1];

typedef struct
{
    uint16 code;
    uint16 first;
} MC_TITLE_RANGE;

static const MC_TITLE_RANGE mc_title_ranges[3] = {{33359u, 48u}, {33376u, 65u}, {33409u, 97u}};
static const uint16 mc_title_punct[33] = {0x8140u, 0x8149u, 0x8168u, 0x8194u, 0x8190u, 0x8193u, 0x8195u, 0x8166u, 0x8169u, 0x816Au, 0x8196u, 0x817Bu, 0x8143u, 0x817Cu, 0x8144u, 0x815Eu, 0x8146u, 0x8147u, 0x8171u, 0x8181u, 0x8172u, 0x8148u, 0x8197u, 0x816Du, 0x818Fu, 0x816Eu, 0x814Fu, 0x8151u, 0x8165u, 0x816Fu, 0x8162u, 0x8170u, 0x8150u};

typedef struct
{
    uint8 palette[32];
    uint8 pixels[128];
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
    uint8 palette[32];
    uint8 pixels[128];
    uint8 reserved1[MC_FILE_HEADER_BYTES - 256];
} MC_SAVE_HEADER;

typedef char MC_SAVE_HEADER_LAYOUT_CHECK[(sizeof(MC_SAVE_HEADER) == MC_FILE_HEADER_BYTES && offsetof(MC_SAVE_HEADER, title) == 4 && offsetof(MC_SAVE_HEADER, palette) == 96 && offsetof(MC_SAVE_HEADER, pixels) == 128) ? 1 : -1];

static sint32 mc_decode_icon(const uint8 *src, size_t size)
{
    size_t clut_bytes;
    size_t pixel_block;
    size_t pixel_bytes;

    memset(&mc_icon, 0, sizeof(mc_icon));
    if (!src || size < 20 || xport_load_le32(src) != 0x10u || (xport_load_le32(src + 4) & 8u) == 0u)
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
    memcpy(mc_icon.palette, src + 20, sizeof(mc_icon.palette));
    memcpy(mc_icon.pixels, src + pixel_block + 12, sizeof(mc_icon.pixels));
    mc_icon.ready = 1;
    return 1;
}

sint32 mc_load_icon(void)
{
    const uint8 *src;
    size_t size = 0;

    FUNCTION_MARKER(0x8004AEB4u, "MAIN.EXE");
    strcpy(cd_dir, "\\");
    src = cd_load_file_bytes("MENU\\DATA\\RRBADGE.TIM", &size);
    return mc_decode_icon(src, size);
}

void mc_fn_800499e0(void)
{
    FUNCTION_MARKER(0x800499E0u, "MAIN.EXE");
}

sint32 mc_open_events(void)
{
    static const uint32 specifications[4] = {4u, 0x8000u, 0x100u, 0x2000u};
    uint32 index;

    FUNCTION_MARKER(0x800499E8u, "MAIN.EXE");
    menu_fn_8006776c();
    for (index = 0u; index < 4u; ++index)
        mc_state.events[index] = OpenEventGuest(0xF4000001u, specifications[index], 0x2000u, 0u);
    for (index = 0u; index < 4u; ++index)
        mc_state.events[index + 4u] = OpenEventGuest(0xF0000011u, specifications[index], 0x2000u, 0u);
    global_fn_8006777c();
    return 0;
}

sint32 mc_close_events(void)
{
    uint32 index;
    sint32 result = 0;

    FUNCTION_MARKER(0x80049AF0u, "MAIN.EXE");
    for (index = 0u; index < 8u; ++index)
        result = CloseEvent(mc_state.events[index]);
    return result;
}

sint32 mc_enable_events(void)
{
    uint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80049B68u, "MAIN.EXE");
    menu_fn_8006776c();
    for (index = 0u; index < 8u; ++index)
        EnableEvent(mc_state.events[index]);
    global_fn_8006777c();
    mc_state.card = 0u;
    result = mc_poll_load(0);
    mc_state.card_status = (uint16)result;
    return result;
}

sint32 mc_disable_events(void)
{
    uint32 index;

    FUNCTION_MARKER(0x80049C08u, "MAIN.EXE");
    menu_fn_8006776c();
    for (index = 0u; index < 8u; ++index)
        DisableEvent(mc_state.events[index]);
    global_fn_8006777c();
    return 0;
}

sint32 mc_poll_load(sint32 card)
{
    sint32 event;

    FUNCTION_MARKER(0x80049C90u, "MAIN.EXE");
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

sint32 mc_probe_status(sint32 card)
{
    sint32 event;
    sint32 result;

    FUNCTION_MARKER(0x80049DECu, "MAIN.EXE");
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

sint32 mc_poll_primary_card_event(void)
{
    uint32 index;
    uint32 attempt;

    FUNCTION_MARKER(0x80049EB0u, "MAIN.EXE");
    for (attempt = 1u; attempt <= 0x13880u; ++attempt)
    {
        for (index = 0u; index < 4u; ++index)
            if (TestEvent(mc_state.events[index]) == 1)
                return (sint32)index;
    }
    return 4;
}

sint32 mc_test_primary_card_events(void)
{
    uint32 index;
    sint32 result = 0;

    FUNCTION_MARKER(0x80049F50u, "MAIN.EXE");
    for (index = 0u; index < 4u; ++index)
        result = TestEvent(mc_state.events[index]);
    return result;
}

sint32 mc_poll_secondary_card_event(void)
{
    uint32 index;
    uint32 attempt;

    FUNCTION_MARKER(0x80049F98u, "MAIN.EXE");
    for (attempt = 1u; attempt <= 0x13880u; ++attempt)
    {
        for (index = 0u; index < 4u; ++index)
            if (TestEvent(mc_state.events[index + 4u]) == 1)
                return (sint32)index;
    }
    return 4;
}

sint32 mc_test_secondary_card_events(void)
{
    uint32 index;
    sint32 result = 0;

    FUNCTION_MARKER(0x8004A038u, "MAIN.EXE");
    for (index = 0u; index < 4u; ++index)
        result = TestEvent(mc_state.events[index + 4u]);
    return result;
}

sint32 mc_header_check(sint32 card)
{
    uint8 header[128] = {0};

    FUNCTION_MARKER(0x8004A080u, "MAIN.EXE");
    mc_test_secondary_card_events();
    // Original 0x80074CE8 and 0x80074CD8 dispatch BIOS B0:50 and B0:4F
    _new_card();
    _card_read(card, 0, header);
    if (mc_poll_secondary_card_event() != 0)
        return 0;
    return header[0] == 'M' && header[1] == 'C';
}

sint32 mc_fn_8004a108(sint32 card)
{
    FUNCTION_MARKER(0x8004A108u, "MAIN.EXE");
    return 0;
}

sint32 mc_scan_files(uint32 card)
{
    char pattern[128];
    DIRENTRY *entry = mc_dir;

    FUNCTION_MARKER(0x8004A224u, "MAIN.EXE");
    snprintf(pattern, sizeof(pattern), "bu%.2x:*", card);
    mc_state.file_count = 0u;
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
    return (sint32)mc_state.file_count;
}

static sint32 mc_read_header(sint32 card, const char *name, MC_FILE_HEADER *dst)
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004A320u, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 1u);
    if (fd < 0)
    {
        printf("ERROR: The file could not be opened\n");
        return 0;
    }
    bytes = readPSX(fd, dst, MC_FILE_HDR_BYTES);
    if (bytes != MC_FILE_HDR_BYTES)
        printf("ERROR: During reading   Read: [%d]\n", bytes);
    closePSX(fd);
    return bytes == MC_FILE_HDR_BYTES;
}

void mc_read_headers(sint32 count, sint32 card)
{
    sint32 idx;

    FUNCTION_MARKER(0x8004A2A0u, "MAIN.EXE");
    if (count > MC_FILES_MAX)
        count = MC_FILES_MAX;
    for (idx = 0; idx < count; ++idx)
        mc_read_header(card, mc_dir[idx].name, &mc_file_headers[idx]);
}

sint32 mc_write_header(sint32 card, const char *name, const uint8 src[MC_FILE_HEADER_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004A3E0u, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 2u);
    if (fd < 0)
        return 0;
    bytes = writePSX(fd, src, MC_FILE_HEADER_BYTES);
    closePSX(fd);
    return (uint32)bytes >= MC_FILE_HEADER_BYTES;
}

sint32 mc_probe(sint32 card, const char *name)
{
    char path[64];
    sint32 fd;

    FUNCTION_MARKER(0x8004A4A8u, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)card, name);
    fd = openPSX(path, 2u);
    if (fd >= 0)
        printf(" file exists.\n");
    closePSX(fd);
    return fd >= 0;
}

sint32 mc_file_erase(uint32 card, const char *name)
{
    char path[64];

    FUNCTION_MARKER(0x8004A524u, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", card, name);
    // Original 0x8006780C dispatches BIOS B0:45
    return erase(path);
}

sint32 mc_calc_free_blocks(void)
{
    sint32 count = (sint32)mc_state.file_count;
    uint32 used = 0u;
    sint16 idx = 0;
    sint16 used_low;

    FUNCTION_MARKER(0x8004A760u, "MAIN.EXE");
    if (count > MC_FILES_MAX)
        count = MC_FILES_MAX;
    if (count > 0)
    {
        do
        {
            uint32 blocks = mc_file_headers[idx].blocks;
            used += blocks != 0u ? blocks : 1u;
            idx = (sint16)(idx + 1);
        } while ((sint32)idx < count);
    }
    used_low = (sint16)(uint16)used;
    if (used_low >= 16)
        used_low = 15;
    return (sint16)(uint16)(15u - (uint16)used_low);
}

sint32 mc_encode_title(const char *src, uint8 dst[MC_TITLE_BYTES])
{
    size_t len;
    size_t idx;

    FUNCTION_MARKER(0x8004AD04u, "MAIN.EXE");
    for (len = 0u; src[len] != '\0'; ++len)
    {
        uint8 ch = (uint8)src[len];

        if (len >= MC_TITLE_CHARS)
            return -1;
        if (ch < 32u || ch > 126u)
            exit(EXIT_FAILURE);
    }
    for (idx = 0u; idx < MC_TITLE_CHARS; ++idx)
    {
        dst[2u * idx] = 0x81u;
        dst[2u * idx + 1u] = 0x40u;
    }
    for (idx = 0u; idx < len; ++idx)
    {
        uint8 ch = (uint8)src[idx];
        uint32 group = 0u;
        uint32 offset = 0u;
        uint16 code;

        if (ch < 48u)
            offset = 1u;
        else if (ch < 58u)
            group = 0u;
        else if (ch < 65u)
            offset = 11u;
        else if (ch < 91u)
            group = 1u;
        else if (ch < 97u)
            offset = 37u;
        else if (ch < 123u)
            group = 2u;
        else
            offset = 63u;
        if (offset != 0u)
            code = mc_title_punct[ch - (offset + 31u)];
        else
            code = (uint16)(mc_title_ranges[group].code + ch - mc_title_ranges[group].first);
        dst[2u * idx] = (uint8)(code >> 8);
        dst[2u * idx + 1u] = (uint8)code;
    }
    dst[2u * len] = 0u;
    dst[2u * len + 1u] = 0u;
    dst[2u * len + 2u] = 0u;
    return 0;
}

void mc_fn_8004ae78(void)
{
    FUNCTION_MARKER(0x8004AE78u, "MAIN.EXE");
}

void mc_fn_8004af00(void)
{
    FUNCTION_MARKER(0x8004AF00u, "MAIN.EXE");
}

void mc_fn_8004af18(void)
{
    FUNCTION_MARKER(0x8004AF18u, "MAIN.EXE");
}

void mc_error_route(sint16 command)
{
    sint32 flag = -1;

    FUNCTION_MARKER(0x8004AF20u, "MAIN.EXE");
    if (mc_state.load_active != 0u)
    {
        mc_state.load_error = (uint16)command;
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
        menu_update_flag_index(18, 1);
        flag = 19;
    }
    if (flag >= 0)
        menu_update_flag_index(flag, 1);
}

void mc_clear_countdown(void)
{
    FUNCTION_MARKER(0x8004B080u, "MAIN.EXE");
    mc_state.countdown = 0u;
}

sint32 mc_start_countdown(void)
{
    FUNCTION_MARKER(0x8004B08Cu, "MAIN.EXE");
    mc_state.countdown = 150u;
    return 150;
}

void mc_fn_8004b0a4(void)
{
    FUNCTION_MARKER(0x8004B0A4u, "MAIN.EXE");
}

void mc_fn_8004b0b4(sint16 command)
{
    sint32 flag = -1;

    FUNCTION_MARKER(0x8004B0B4u, "MAIN.EXE");
    if (command == -9)
    {
        menu_clear_rec_bytes();
        menu_update_flag_index(6, 1);
        flag = 7;
    }
    else if (command == -8)
    {
        menu_clear_rec_bytes();
        menu_update_flag_index(6, 1);
        menu_update_flag_index(8, 1);
        flag = 9;
    }
    else if (command == -7)
    {
        menu_clear_rec_bytes();
        menu_update_flag_index(16, 1);
        flag = 17;
    }
    else if (command == -3)
        mc_start_countdown();
    else if (command == -2)
        mc_clear_countdown();
    else if (command == -1 || command == -6)
    {
        menu_clear_rec_bytes();
        menu_update_flag_index(18, 1);
        flag = 19;
    }
    if (flag >= 0)
        menu_update_flag_index(flag, 1);
}

void mc_init_menu_status(void)
{
    sint16 result;

    FUNCTION_MARKER(0x8004B224u, "MAIN.EXE");
    mc_state.card = 0u;
    result = (sint16)mc_probe_status(0);
    if (result != 5)
        mc_state.card_status = (uint16)result;
}

sint32 mc_read_save(const char *name, uint8 dst[MC_SAVE_FILE_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes = 0;

    FUNCTION_MARKER(0x8004B3ECu, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)(sint32)(sint16)mc_state.card, name);
    fd = openPSX(path, 1u);
    if (fd >= 0)
        bytes = readPSX(fd, dst, MC_SAVE_FILE_BYTES);
    if (fd < 0 || bytes != MC_SAVE_FILE_BYTES)
    {
        mc_fn_8004ae78();
        bytes = 0;
    }
    closePSX(fd);
    return bytes;
}

sint32 mc_write_save(const char *name, const uint8 src[MC_SAVE_FILE_BYTES])
{
    char path[64];
    sint32 fd;
    sint32 bytes;

    FUNCTION_MARKER(0x8004B490u, "MAIN.EXE");
    snprintf(path, sizeof(path), "bu%.2x:%s", (uint32)(sint32)(sint16)mc_state.card, name);
    fd = openPSX(path, 2u);
    if (fd >= 0)
    {
        mc_fn_8004ae78();
        closePSX(fd);
        return 0;
    }
    fd = openPSX(path, 0x10200u);
    if (fd == -1)
    {
        mc_fn_8004ae78();
        return 0;
    }
    closePSX(fd);
    fd = openPSX(path, 2u);
    if (fd < 0)
    {
        mc_fn_8004ae78();
        return 0;
    }
    bytes = writePSX(fd, src, MC_SAVE_FILE_BYTES);
    if (bytes != MC_SAVE_FILE_BYTES)
    {
        mc_fn_8004ae78();
        bytes = 0;
    }
    closePSX(fd);
    return bytes;
}



sint32 mc_save_encode(uint8 dst[MC_SAVE_BYTES])
{
    FUNCTION_MARKER(0x8004B644u, "MAIN.EXE");
    // Encode the compatible header independently of native layout
    xport_store_le32(dst, mc_state.save_size);
    xport_store_le32(dst + 4u, mc_state.save_checksum);
    for (uint32 idx = 0u; idx < MC_SAVE_DATA_BYTES; ++idx)
        dst[MC_SAVE_HEADER_BYTES + idx] = profile_save_read_byte(idx);
    return 0;
}

sint32 mc_save_decode(const uint8 src[MC_SAVE_BYTES])
{
    FUNCTION_MARKER(0x8004B6BCu, "MAIN.EXE");
    tournament_clear_extra_rounds();
    mc_state.save_size = xport_load_le32(src);
    mc_state.save_checksum = xport_load_le32(src + 4u);
    for (uint32 idx = 0u; idx < MC_SAVE_DATA_BYTES; ++idx)
        profile_save_write_byte(idx, src[MC_SAVE_HEADER_BYTES + idx]);
    return 0;
}

sint32 mc_save_desc_checksum_calc(void)
{
    uint32 index;
    uint32 sum = 0u;

    FUNCTION_MARKER(0x8004B734u, "MAIN.EXE");
    mc_state.save_size = MC_SAVE_DATA_BYTES;
    for (index = 0u; index < MC_SAVE_DATA_BYTES; ++index)
        sum += profile_save_read_byte(index);
    mc_state.save_checksum = sum;
    return (sint32)sum;
}

sint32 mc_save_validate(const uint8 data[MC_SAVE_BYTES])
{
    uint32 size;
    uint32 expected;
    uint32 sum = 0u;
    uint32 idx;

    FUNCTION_MARKER(0x8004B7E0u, "MAIN.EXE");
    mc_save_desc_checksum_calc();
    size = xport_load_le32(data);
    expected = xport_load_le32(data + 4u);
    if (size != MC_SAVE_DATA_BYTES)
        return 2;
    for (idx = 0u; idx < size; ++idx)
        sum += data[MC_SAVE_HEADER_BYTES + idx];
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

    FUNCTION_MARKER(0x8004B90Cu, "MAIN.EXE");
    if ((sint16)mc_state.countdown != 0)
        mc_state.countdown = (uint16)((sint16)mc_state.countdown - 1);
    phase = (sint16)(uint16)mc_state.phase;
    if (phase == MC_LOAD && (sint16)mc_state.countdown == 0)
    {
        uint8 old_language = menu_state.language;

        ResetGraph(0);
        mc_init_menu_status();
        card = (sint16)mc_state.card;
        if ((sint16)mc_probe(card, mc_save_name) != 0)
        {
            MC_SAVE_FILE file = {0};
            load_result = mc_read_save(mc_save_name, (uint8 *)&file);
            if ((sint16)load_result != 0)
            {
                validation = mc_save_validate(file.payload);
                if ((sint16)validation != 0)
                    load_result = 0;
                else
                {
                    mc_save_decode(file.payload);
                    vehicle_select_fn_80062394();
                }
            }
        }
        else
            save_found = 0;
        mc_state.countdown = 150u;
        mc_state.phase = MC_LOAD_WAIT;
        menu_clear_rec_bytes();
        if ((sint16)load_result != 0)
        {
            menu_update_flag_index(3, 1);
            game_selection.flags = (uint16)((game_selection.flags & 0xFF00u) | (uint8)(0u));
            if ((sint16)mc_probe(card, mc_bonus_names[0]) != 0 || (sint16)mc_probe(card, mc_bonus_names[1]) != 0 || (sint16)mc_probe(card, mc_bonus_names[2]) != 0)
            {
                if (((uint8)game_selection.flags & 0x20u) != 0u)
                    game_selection.flags = (uint16)((game_selection.flags & 0xFF00u) | (uint8)((uint8)((uint8)game_selection.flags - 32u)));
                menu_state.sound = 3u;
                game_selection.flags = (uint16)((game_selection.flags & 0xFF00u) | (uint8)((uint8)((uint8)game_selection.flags + 32u)));
            }
            else if (((uint8)game_selection.flags & 0x20u) != 0u)
                game_selection.flags = (uint16)((game_selection.flags & 0xFF00u) | (uint8)((uint8)((uint8)game_selection.flags - 32u)));
            menu_tex_refresh_if_needed();
            if (old_language != menu_state.language)
                menu_fn_80050440(24, 0);
            sound_mix.music = sound_options.music;
            sound_mix.effects = sound_options.effects;
            voice_set_volume(7, (sint32)((uint32)(sint32)(sint16)sound_options.music << 8));
            vehicle_racer_count = (uint32)(8u);
            vehicle_leader_count = (uint16)(8u);
            vehicle_trailer_count = (uint16)(0u);
            handled = 1;
        }
        else
        {
            sint32 card_state = (sint16)mc_state.card_status;
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
                mc_state.countdown = 250u;
                mc_error_route(-1);
                handled = 1;
            }
        }
        mc_state.load_result = (uint16)load_result;
        if (handled == 0)
        {
            sint32 card_state = (sint16)mc_probe_status(card);
            mc_state.card_status = (uint16)card_state;
            if (card_state == 3)
            {
                mc_state.countdown = 250u;
                mc_error_route(-6);
            }
        }
        phase = (sint16)(uint16)mc_state.phase;
    }
    if (phase == MC_LOAD_WAIT && (sint16)mc_state.countdown == 0)
    {
        sint32 index;
        mc_state.prev_phase = MC_LOAD_WAIT;
        mc_state.phase = MC_IDLE;
        for (index = 0; index < 26; ++index)
        {
            text_menu[index + 8].visible = 0;
            result = 1;
        }
    }
    else if (phase == MC_LOAD_WAIT)
        result = (sint16)mc_state.countdown;
    return result;
}

sint32 mc_save_header(void)
{
    MC_SAVE_HEADER header = {0};

    FUNCTION_MARKER(0x8004BD0Cu, "MAIN.EXE");
    if (!mc_icon.ready)
        return 0;
    mc_encode_title("RAPID RACER", header.title);
    header.magic[0] = 'S';
    header.magic[1] = 'C';
    header.icon_mode = 0x11u;
    header.blocks = 1u;
    memcpy(header.palette, mc_icon.palette, sizeof(header.palette));
    memcpy(header.pixels, mc_icon.pixels, sizeof(header.pixels));
    return mc_write_header((sint16)mc_state.card, mc_save_name, (const uint8 *)&header);
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
    sint32 index;

    FUNCTION_MARKER(0x8004BEB0u, "MAIN.EXE");
    if ((sint16)mc_state.countdown != 0)
        mc_state.countdown = (uint16)((sint16)mc_state.countdown - 1);
    phase = (sint16)(uint16)mc_state.phase;
    if (phase == MC_SAVE && (sint16)mc_state.countdown == 0)
    {
        card = (sint16)mc_state.card;
        count = (sint16)mc_scan_files((uint32)card);
        mc_read_headers(count, card);
        save_exists = (sint16)mc_probe(card, mc_save_name);
        if ((sint16)mc_state.card_status != 0)
            free_blocks = 1;
        else
            free_blocks = (sint16)mc_calc_free_blocks();
        if (save_exists != 0)
        {
            if ((sint16)mc_state.overwrite != 0)
                file_exists = 1;
        }
        else if (free_blocks != 0)
            file_exists = 1;
        if (file_exists != 0)
        {
            if ((sint16)mc_probe(card, mc_save_name) != 0 && (sint16)mc_state.overwrite == 0)
                save_exists = 1;
            else
            {
                MC_SAVE_FILE file = {0};
                mc_save_desc_checksum_calc();
                mc_save_encode(file.payload);
                mc_file_erase((uint32)card, mc_save_name);
                save_result = mc_write_save(mc_save_name, (const uint8 *)&file);
                mc_save_header();
                mc_state.countdown = 150u;
                mc_state.phase = MC_SAVE_WAIT;
            }
        }
        menu_clear_rec_bytes();
        mc_state.phase = MC_SAVE_WAIT;
        mc_state.overwrite_prompt = 0u;
        mc_state.format_prompt = 0u;
        mc_state.countdown = 250u;
        if ((sint16)save_result != 0)
        {
            menu_update_flag_index(1, 1);
            handled = 1;
        }
        else if ((sint16)save_exists == 0)
        {
            card_state = (sint16)mc_state.card_status;
            if (card_state == 1)
            {
                mc_fn_8004b0b4(-2);
                mc_state.format_prompt = 1u;
                mc_state.countdown = 0u;
                handled = 1;
            }
            else if (card_state == 2)
            {
                mc_fn_8004b0b4(-5);
                handled = 1;
            }
            else if (card_state == 3)
            {
                mc_state.countdown = 250u;
                mc_fn_8004b0b4(-1);
                handled = 1;
            }
        }
        else
        {
            mc_fn_8004b0b4(-7);
            free_blocks = 1;
            mc_state.countdown = 0u;
            mc_state.phase = MC_OVERWRITE;
            mc_state.overwrite_prompt = 1u;
            handled = 1;
        }
        if (handled == 0)
        {
            card_state = (sint16)mc_probe_status(card);
            mc_state.card_status = (uint16)card_state;
            if (card_state == 3)
                mc_fn_8004b0b4(-6);
            else if ((sint16)free_blocks == 0 && (sint16)mc_state.overwrite == 0)
                mc_fn_8004b0b4(-9);
        }
        phase = (sint16)(uint16)mc_state.phase;
    }
    if (phase == MC_SAVE_WAIT && (sint16)mc_state.countdown == 0)
    {
        mc_state.prev_phase = MC_SAVE_WAIT;
        mc_state.phase = MC_IDLE;
        for (index = 0; index < 26; ++index)
            text_menu[index + 8].visible = 0u;
        return 1;
    }
    return phase;
}

sint32 mc_load_begin(void)
{
    sint32 error;
    sint32 result;

    FUNCTION_MARKER(0x8004CB04u, "MAIN.EXE");
    mc_state.load_active = 1u;
    mc_state.load_error = 0u;
    mc_state.countdown = 0u;
    mc_state.phase = MC_LOAD;
    mc_state.card = 0u;
    mc_probe_status(0);
    mc_load_state_update();
    error = (sint16)mc_state.load_error;
    mc_state.load_active = 0u;
    if (error != 0)
    {
        sint32 message = error == -6 || error == -1 ? 47 : 1;
        if (message == 1)
            mc_calc_free_blocks();
        mc_state.notice_screen = (uint16)message;
    }
    result = (sint16)mc_probe((sint16)mc_state.card, mc_save_name);
    if (result == 0)
    {
        sint32 count = (sint16)mc_scan_files((uint32)(sint32)(sint16)mc_state.card);
        mc_read_headers(count, (sint16)mc_state.card);
        if ((sint16)mc_calc_free_blocks() == 0)
            mc_state.notice_screen = 52u;
        return 52;
    }
    return (sint32)((uint32)(uint16)result << 16);
}

sint32 mc_poll_state(void)
{
    uint16 counter;
    sint32 card;
    sint32 state;
    sint32 count;

    FUNCTION_MARKER(0x8004CC50u, "MAIN.EXE");
    counter = (uint16)(mc_state.poll_ticks + 1u);
    mc_state.poll_ticks = counter;
    if ((sint16)counter < 5)
        return 1;
    card = (sint16)mc_state.card;
    mc_state.poll_ticks = 0u;
    state = (sint16)mc_probe_status(card);
    if (state >= 1 && state <= 4)
        return 1;
    card = (sint16)mc_state.card;
    if ((sint16)mc_probe(card, mc_save_name) != 0)
        return 0;
    card = (sint16)mc_state.card;
    count = (sint16)mc_scan_files((uint32)card);
    card = (sint16)mc_state.card;
    mc_read_headers(count, card);
    card = (sint16)mc_state.card;
    if ((sint16)mc_calc_free_blocks() != 0)
        return 0;
    mc_state.notice_screen = 52u;
    return 1;
}





void mc_fn_80074d30(uint32 card)
{
    FUNCTION_MARKER(0x80074D30u, "MAIN.EXE");
    menu_fn_8006776c();
    mc_fn_80074dec(card);
    global_fn_8006777c();
}

void mc_fn_80074d84(void)
{
    FUNCTION_MARKER(0x80074D84u, "MAIN.EXE");
    menu_fn_8006776c();
    mc_fn_80074dfc();
    global_fn_8006777c();
}

void mc_fn_80074dec(uint32 card)
{
    FUNCTION_MARKER(0x80074DECu, "MAIN.EXE");
}

void mc_fn_80074dfc(void)
{
    FUNCTION_MARKER(0x80074DFCu, "MAIN.EXE");
}
