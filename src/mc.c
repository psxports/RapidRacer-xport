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

MC_STATE mc_state;
#include <string.h>

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

sint32 mc_enumerate_files(uint32 card, uint32 record)
{
    char pattern[128];
    uint32 current = record;
    uint32 count = 0u;

    FUNCTION_MARKER(0x8004A224u, "MAIN.EXE");
    snprintf(pattern, sizeof(pattern), "bu%.2x:*", card);
    w_u32(0x800B6BE4u, 0u);
    if (mc_file_find_first(pattern, record) == record)
    {
        current += 40u;
        do
        {
            ++count;
            w_u32(0x800B6BE4u, (uint32)count);
            if (nextfile(psx_addr(current, 40u)) != psx_addr(current, 40u))
                break;
            current += 40u;
        } while (1);
    }
    return (sint32)r_u32(0x800B6BE4u);
}

sint32 mc_read_file_entries(uint32 name, sint32 count, sint32 card)
{
    uint32 index;

    FUNCTION_MARKER(0x8004A2A0u, "MAIN.EXE");
    for (index = 0u; (sint32)index < count; ++index)
    {
        mc_fn_8004a320(card, name, 0x800FF948u + 512u * (uint32)index);
        name += 40u;
    }
    return 0;
}

sint32 mc_fn_8004a320(sint32 card, uint32 name, uint32 output)
{
    FUNCTION_MARKER(0x8004A320u, "MAIN.EXE");
    return 0;
}

sint32 mc_fn_8004a3e0(sint32 card, uint32 name, uint32 input)
{
    FUNCTION_MARKER(0x8004A3E0u, "MAIN.EXE");
    return 0;
}

sint32 mc_probe(sint32 card, uint32 name)
{
    FUNCTION_MARKER(0x8004A4A8u, "MAIN.EXE");
    return 0;
}

sint32 mc_file_erase(uint32 card, uint32 name)
{
    static const char digits[] = "0123456789abcdef";
    char path[64];
    uint32 index = 0u;

    FUNCTION_MARKER(0x8004A524u, "MAIN.EXE");
    path[index++] = 'b';
    path[index++] = 'u';
    path[index++] = digits[(card >> 4) & 15u];
    path[index++] = digits[card & 15u];
    path[index++] = ':';
    while (index + 1u < sizeof(path) && r_u8(name) != 0u)
        path[index++] = (char)r_u8(name++);
    path[index] = '\0';
    return mc_fn_8006780c(path);
}

sint32 mc_calc_free_blocks(void)
{
    sint32 count = (sint32)r_u32(0x800B6BE4u);
    uint32 used = 0u;
    sint16 index = 0;
    sint16 used_low;

    FUNCTION_MARKER(0x8004A760u, "MAIN.EXE");
    if (count > 0)
    {
        do
        {
            uint32 blocks = r_u8(0x800FF94Bu + ((uint32)(sint32)index << 9));
            used += blocks != 0u ? blocks : 1u;
            index = (sint16)(index + 1);
        } while ((sint32)index < count);
    }
    used_low = (sint16)(uint16)used;
    if (used_low >= 16)
        used_low = 15;
    return (sint16)(uint16)(15u - (uint16)used_low);
}

sint32 mc_fn_8004ad04(uint32 source, uint32 destination, sint32 unused, uint8 group)
{
    uint32 cursor = destination;
    uint32 index;
    uint8 character;

    FUNCTION_MARKER(0x8004AD04u, "MAIN.EXE");
    for (index = 0u; index < 38u; ++index)
    {
        w_u8(cursor++, 0x81u);
        w_u8(cursor++, 0x40u);
    }
    while ((character = r_u8(source++)) != 0u)
    {
        uint16 encoded;
        uint8 offset = 0u;
        if (character >= 32u && character < 48u)
            offset = 1u;
        else if (character >= 48u && character < 58u)
            group = 0u;
        else if (character >= 58u && character < 65u)
            offset = 11u;
        else if (character >= 65u && character < 91u)
            group = 1u;
        else if (character >= 91u && character < 97u)
            offset = 37u;
        else if (character >= 97u && character < 123u)
            group = 2u;
        else
            offset = 63u;
        if (offset != 0u)
            encoded = r_u16(0x800967F0u + 2u * (uint32)(character - (offset + 31u)));
        else
            encoded = (uint16)(r_u16(0x800967E4u + 4u * group) + character - r_u16(0x800967E6u + 4u * group));
        w_u8(cursor++, (uint8)(encoded >> 8));
        w_u8(cursor++, (uint8)encoded);
    }
    w_u8(cursor++, 0u);
    w_u8(cursor++, 0u);
    w_u8(cursor, 0u);
    return 0;
}

void mc_fn_8004ae78(void)
{
    FUNCTION_MARKER(0x8004AE78u, "MAIN.EXE");
}

sint32 mc_copy_sized_bytes(uint32 source, uint32 destination, sint32 size)
{
    sint32 result = 0;
    sint32 index;

    FUNCTION_MARKER(0x8004AE80u, "MAIN.EXE");
    for (index = 0; index < size; ++index)
    {
        result = r_u8(source + (uint32)index);
        w_u8(destination + (uint32)index, (uint8)result);
    }
    return result;
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
    if (r_u16(0x800B6AE0u) != 0u)
    {
        w_u16(0x800B6AEEu, (uint16)command);
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

sint32 mc_fn_8004b3ec(uint32 name, uint32 output)
{
    FUNCTION_MARKER(0x8004B3ECu, "MAIN.EXE");
    return 0;
}

sint32 mc_fn_8004b490(uint32 name, uint32 input)
{
    FUNCTION_MARKER(0x8004B490u, "MAIN.EXE");
    return 0;
}

sint32 mc_save_segment_sizes_sum(sint32 unused, sint32 total)
{
    FUNCTION_MARKER(0x8004B608u, "MAIN.EXE");
    return (sint32)((uint32)total + MC_SAVE_BYTES);
}

sint32 mc_save_segments_gather(uint32 destination)
{
    FUNCTION_MARKER(0x8004B644u, "MAIN.EXE");
    // Encode the compatible header independently of native layout
    w_u32(destination, mc_state.save_size);
    w_u32(destination + 4u, mc_state.save_checksum);
    for (uint32 index = 0u; index < MC_SAVE_DATA_BYTES; ++index)
        w_u8(destination + MC_SAVE_HEADER_BYTES + index, profile_save_read_byte(index));
    return 0;
}

sint32 mc_save_segments_scatter(uint32 source)
{
    FUNCTION_MARKER(0x8004B6BCu, "MAIN.EXE");
    tournament_clear_extra_rounds();
    mc_state.save_size = r_u32(source);
    mc_state.save_checksum = r_u32(source + 4u);
    for (uint32 index = 0u; index < MC_SAVE_DATA_BYTES; ++index)
        profile_save_write_byte(index, r_u8(source + MC_SAVE_HEADER_BYTES + index));
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

sint32 mc_save_payload_validate(uint32 data)
{
    uint32 size;
    uint32 expected;
    uint32 sum = 0u;
    uint32 index;

    FUNCTION_MARKER(0x8004B7E0u, "MAIN.EXE");
    mc_save_desc_checksum_calc();
    size = r_u32(data);
    expected = r_u32(data + 4u);
    for (index = 0u; index < size; ++index)
        sum += r_u8(data + 8u + index);
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
        if ((sint16)mc_probe(card, 0x80081DCCu) != 0)
        {
            sint32 size;
            uint32 buffer;

            game_push_checkpoint();
            size = mc_save_segment_sizes_sum(0, 0);
            buffer = game_alloc_arena_bytes(size + 512);
            load_result = mc_fn_8004b3ec(0x80081DCCu, buffer);
            if ((sint16)load_result != 0)
            {
                validation = mc_save_payload_validate(buffer + 512u);
                if ((sint16)validation != 0)
                    load_result = 0;
                else
                {
                    mc_save_segments_scatter(buffer + 512u);
                    vehicle_select_fn_80062394();
                }
            }
            game_pop_checkpoint();
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
            if ((sint16)mc_probe(card, 0x80081DE4u) != 0 || (sint16)mc_probe(card, 0x80081DFCu) != 0 || (sint16)mc_probe(card, 0x80081E14u) != 0)
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
            w_u16(0x80083494u, sound_options.music);
            w_u16(0x80083490u, sound_options.effects);
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
        w_u16(0x800B6B1Au, 4u);
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

sint32 mc_fn_8004bd0c(sint32 unused1, sint32 unused2, sint32 language, uint8 group)
{
    FUNCTION_MARKER(0x8004BD0Cu, "MAIN.EXE");
    return 0;
}

sint32 mc_fn_8004beb0(void)
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
        count = (sint16)mc_enumerate_files((uint32)card, 0x800D7220u);
        mc_read_file_entries(0x800D7220u, count, card);
        save_exists = (sint16)mc_probe(card, 0x80081DCCu);
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
            if ((sint16)mc_probe(card, 0x80081DCCu) != 0 && (sint16)mc_state.overwrite == 0)
                save_exists = 1;
            else
            {
                sint32 size;
                uint32 buffer;

                game_push_checkpoint();
                size = mc_save_segment_sizes_sum(0, 0);
                buffer = game_alloc_arena_bytes(size + 512);
                mc_save_desc_checksum_calc();
                mc_save_segments_gather(buffer + 512u);
                mc_file_erase((uint32)card, 0x80081DCCu);
                save_result = mc_fn_8004b490(0x80081DCCu, buffer);
                game_pop_checkpoint();
                mc_fn_8004bd0c(0, 0, 0, 0u);
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
        w_u16(0x800B6B1Au, 2u);
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
    w_u16(0x800B6AE0u, 1u);
    w_u16(0x800B6AEEu, 0u);
    mc_state.countdown = 0u;
    mc_state.phase = MC_LOAD;
    mc_state.card = 0u;
    mc_probe_status(0);
    mc_load_state_update();
    error = (sint16)r_u16(0x800B6AEEu);
    w_u16(0x800B6AE0u, 0u);
    if (error != 0)
    {
        sint32 message = error == -6 || error == -1 ? 47 : 1;
        if (message == 1)
            mc_calc_free_blocks();
        w_u16(0x800B6BD2u, (uint16)message);
    }
    result = (sint16)mc_probe((sint16)mc_state.card, 0x80081DCCu);
    if (result == 0)
    {
        sint32 count = (sint16)mc_enumerate_files((uint32)(sint32)(sint16)mc_state.card, 0x800D7220u);
        mc_read_file_entries(0x800D7220u, count, (sint16)mc_state.card);
        if ((sint16)mc_calc_free_blocks() == 0)
            w_u16(0x800B6BD2u, 52u);
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
    if ((sint16)mc_probe(card, 0x80081DCCu) != 0)
        return 0;
    card = (sint16)mc_state.card;
    count = (sint16)mc_enumerate_files((uint32)card, 0x800D7220u);
    card = (sint16)mc_state.card;
    mc_read_file_entries(0x800D7220u, count, card);
    card = (sint16)mc_state.card;
    if ((sint16)mc_calc_free_blocks() != 0)
        return 0;
    w_u16(0x800B6BD2u, 52u);
    return 1;
}

uint32 mc_file_find_first(const char *pattern, uint32 record)
{
    FUNCTION_MARKER(0x800677ECu, "MAIN.EXE");
    return 0u;
}

sint32 mc_fn_8006780c(const char *path)
{
    FUNCTION_MARKER(0x8006780Cu, "MAIN.EXE");
    return 0;
}

sint32 mc_fn_80067a8c(uint32 pad1, sint32 pad1_size, uint32 pad2, sint32 pad2_size)
{
    FUNCTION_MARKER(0x80067A8Cu, "MAIN.EXE");
    PadInitDirect((uint8 *)xport_guest_ptr(pad1, sizeof(uint8)), (uint8 *)xport_guest_ptr(pad2, sizeof(uint8)));
    return 1;
}

sint32 mc_fn_80067a9c(void)
{
    FUNCTION_MARKER(0x80067A9Cu, "MAIN.EXE");
    PadStartCom();
    return 1;
}

sint32 mc_fn_80067bbc(void)
{
    FUNCTION_MARKER(0x80067BBCu, "MAIN.EXE");
    PadStopCom();
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
