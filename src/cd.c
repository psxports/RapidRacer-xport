#include "route.h"
#include "render.h"
#include "sound.h"
#include "profile.h"
#include "game.h"
#include "effects.h"
#include "mesh.h"
#include "vehicle.h"
#include "menu.h"
#include "pickup.h"
#include "scene.h"
#include "timer.h"
#include "vehicle_select.h"
#include "runtime.h"
#include "psx_gpu.h"
#include "cd.h"
#include "game_main.h"
#include "arena.h"
#include "global.h"
#include "records.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

static uint32 cd_read_file_alloc_span(uint32 path, size_t *span_size);

static uint32 rr_cd_search_parent_lba;
static uint32 rr_cd_search_parent_index;
static uint32 rr_cd_search_cached_directory_lba;
static sint32 rr_cd_search_refreshed_directory;

static uint32 cd_bswap_u32(uint32 value)
{
    return (value >> 24) | ((value >> 8) & 0x0000FF00u) | ((value << 8) & 0x00FF0000u) | (value << 24);
}

static sint32 cd_sra_two(uint32 value)
{
    return (sint32)((value >> 2) | ((value & 0x80000000u) != 0u ? 0xC0000000u : 0u));
}

sint32 cd_decompress_backrefs(uint32 input, uint32 output)
{
    uint32 start = output;

    FUNCTION_MARKER(0x8002B158u, "MAIN.EXE");
    for (;;)
    {
        uint8 flags = r_u8(input++);
        sint32 bit;

        for (bit = 7; bit >= 0; --bit)
        {
            if ((flags & (1u << bit)) == 0u)
                w_u8(output++, r_u8(input++));
            else
            {
                uint16 token = (uint16)(((uint16)r_u8(input) << 8) | r_u8(input + 1u));
                sint32 length;
                uint32 source;
                input += 2u;
                if (token == 0xFFFFu)
                    return (sint32)(output - start);
                length = (token >> 11) + 3;
                source = output - ((token & 0x07FFu) + 1u);
                while (length-- > 0)
                    w_u8(output++, r_u8(source++));
            }
        }
    }
}

sint32 cd_load_course_assets(uint32 context)
{
    uint32 frame = guest_stack_push(0x48u);
    uint32 directory = frame + 0x10u;
    uint32 suffix = frame + 0x28u;
    uint32 path;
    uint32 data;
    size_t geometry_size;
    sint32 course;
    sint32 mode;
    sint32 result;

    FUNCTION_MARKER(0x8002B664u, "MAIN.EXE");
    runtime_copy_guest_8(0x800D6958u, 0x800B3E74u);
    if (!cd_load_model_archive())
    {
        result = 0;
        goto finish;
    }
    if (r_u32(context + 12u) == 8u)
    {
        runtime_copy_guest_8(0x800D6958u, 0x800B3E7Cu);
        result = cd_load_archive_assets(r_u32(0x8008348Cu) == 2u ? 0x80080D68u : 0x80080D74u);
        goto finish;
    }
    mode = (sint32)r_u32(0x8008348Cu);
    if (mode == 1)
        runtime_copy_guest_5(suffix, 0x800B3E84u);
    else if (mode == 2)
        runtime_copy_guest_7(suffix, 0x800B3E8Cu);
    else
        runtime_copy_guest_8(suffix, 0x800B3E94u);
    course = (sint32)r_u32(context + 40u);
    w_u32(0x800B3D90u, 1u);
    switch (course)
    {
        case 0:
            runtime_copy_guest_9(directory, 0x80080D80u);
            break;
        case 1:
            runtime_copy_guest_8(directory, 0x800B3E9Cu);
            break;
        case 2:
            runtime_copy_guest_9(directory, 0x80080D8Cu);
            break;
        case 3:
            runtime_copy_guest_7(directory, 0x800B3EA4u);
            break;
        case 4:
            runtime_copy_guest_7(directory, 0x800B3EACu);
            break;
        case 5:
            runtime_copy_guest_7(directory, 0x800B3EB4u);
            break;
        case 6:
            runtime_copy_guest_7(directory, 0x800B3EBCu);
            break;
        default:
            exit(1);
    }
    course = (sint32)r_u32(context + 40u);
    if (course == 6)
        runtime_copy_guest_text(0x800D6958u, directory);
    else
        runtime_join_guest_text(0x800D6958u, directory, suffix);
    course = (sint32)r_u32(context + 40u);
    if (course == 6)
    {
        mode = (sint32)r_u32(0x80083478u);
        runtime_format_vram_filename(directory, (sint16)race_selection.resource, mode != 1);
        cd_load_archive_assets(directory);
        route_gen_init(context);
    }
    else
    {
        cd_load_archive_assets(r_u32(0x80083478u) == 1u ? 0x80080DB4u : 0x80080DC0u);
        data = cd_read_file_alloc_span(runtime_build_cd_path(0x80080DCCu), &geometry_size);
        if (data == 0u)
        {
            result = 0;
            goto finish;
        }
        cd_publish_sections(psx_addr(data, geometry_size), geometry_size);
        mode = profile_selection.slot;
        path = mode == 0 ? 0x80080DD8u : (mode == 1 ? 0x800B3ED0u : 0x80080DE4u);
        {
            size_t route_size;
            uint32 route_data = cd_read_file_alloc_span(runtime_build_cd_path(path), &route_size);
            if (route_data == 0u || !cd_load_versioned_rec_archive((const uint8 *)psx_addr(route_data, route_size), route_size, r_u32(0x8008348Cu) == 2u))
            {
                result = 0;
                goto finish;
            }
        }
    }
    scene_process_published_rec_flags();
    course = (sint32)r_u32(context + 40u);
    if (course == 6)
        result = pickup_update_racer_flags();
    else
        result = 6;
finish:
    guest_stack_pop(0x48u);
    return result;
}

sint32 cd_init_drive(void)
{
    FUNCTION_MARKER(0x8002BA40u, "MAIN.EXE");
    return 1;
}

uint32 cd_load_file_alloc(uint32 path)
{
    FUNCTION_MARKER(0x8002BA60u, "MAIN.EXE");
    return cd_read_file_alloc(runtime_build_cd_path(path));
}

uint32 cd_load_file_buf(uint32 path, uint32 buffer)
{
    FUNCTION_MARKER(0x8002BA9Cu, "MAIN.EXE");
    return cd_read_file_buf(runtime_build_cd_path(path), buffer);
}

sint32 cd_upload_image_clut(uint32 request)
{
    uint32 cursor = request + 8u;
    sint32 result = 0;

    FUNCTION_MARKER(0x8002BAE8u, "MAIN.EXE");
    if ((r_u32(request + 4u) & 8u) != 0u)
    {
        result = LoadImagePSX((PSX_RECT *)psx_addr(request + 12u, sizeof(PSX_RECT)), (uint32 *)psx_addr(request + 20u, 4u));
        cursor += (uint32)((sint32)r_u32(cursor) / 4) * 4u;
    }
    result = LoadImagePSX((PSX_RECT *)psx_addr(cursor + 4u, sizeof(PSX_RECT)), (uint32 *)psx_addr(cursor + 12u, 4u));
    return result;
}

uint32 cd_find_tima_chunk(uint32 input, sint32 requested_index)
{
    uint32 cursor;
    uint32 raw_size;
    uint8 tag0;
    uint8 tag1;
    uint8 tag2;
    uint8 tag3;
    sint32 remaining;
    sint32 index = 0;

    FUNCTION_MARKER(0x8002BB58u, "MAIN.EXE");
    tag0 = r_u8(input);
    tag1 = r_u8(input + 1u);
    tag2 = r_u8(input + 2u);
    tag3 = r_u8(input + 3u);
    raw_size = r_u32(input + 4u);
    cursor = input + 8u;
    remaining = (sint32)(cd_bswap_u32(raw_size) - 4u);
    if (tag0 != 'F' || tag1 != 'O' || tag2 != 'R' || tag3 != 'M')
        abort();
    tag0 = r_u8(cursor);
    tag1 = r_u8(cursor + 1u);
    tag2 = r_u8(cursor + 2u);
    tag3 = r_u8(cursor + 3u);
    cursor += 4u;
    if (tag0 != 'J' || tag1 != 'E' || tag2 != 'T' || tag3 != 'S')
        abort();
    while (remaining > 0)
    {
        sint32 size;
        uint32 advance;
        uint32 payload;

        tag0 = r_u8(cursor);
        tag1 = r_u8(cursor + 1u);
        tag2 = r_u8(cursor + 2u);
        tag3 = r_u8(cursor + 3u);
        raw_size = r_u32(cursor + 4u);
        payload = cursor + 8u;
        size = (sint32)cd_bswap_u32(raw_size);
        if (tag0 == 'T' && tag1 == 'I' && tag2 == 'M' && tag3 == 'A')
        {
            if (index == requested_index)
                return payload;
            ++index;
        }
        remaining = (sint32)((uint32)remaining - (((uint32)size + 11u) & ~3u));
        advance = (uint32)size + 3u;
        if ((sint32)advance < 0)
            advance = (uint32)size + 6u;
        cursor = payload + ((uint32)cd_sra_two(advance) << 2);
    }
    return 0u;
}

sint32 cd_load_tex(uint32 path, sint16 x, sint16 y, sint32 width, sint32 height)
{
    sint32 result;

    FUNCTION_MARKER(0x8002BDA8u, "MAIN.EXE");
    game_push_checkpoint();
    cd_upload_vram_image(runtime_build_cd_path(path), x, y, width, height);
    result = game_pop_checkpoint();
    return result;
}

sint32 cd_load_archive_assets(uint32 path)
{
    uint32 frame = guest_stack_push(0xA0u);
    uint32 data;
    uint32 request;
    sint32 result;

    FUNCTION_MARKER(0x8002BE3Cu, "MAIN.EXE");
    game_push_checkpoint();
    data = cd_read_file_alloc(runtime_build_cd_path_at(frame + 0x10u, path));
    request = cd_find_tima_chunk(data, 0);
    cd_upload_image_clut(request);
    request = cd_find_tima_chunk(data, 1);
    cd_upload_image_clut(request);
    request = cd_find_tima_chunk(data, 2);
    cd_upload_image_clut(request);
    request = cd_find_tima_chunk(data, 2 * menu_state.language + 3);
    cd_upload_image_clut(request);
    request = cd_find_tima_chunk(data, 2 * menu_state.language + 4);
    cd_upload_image_clut(request);
    if (menu_state.language == 2u)
    {
        request = r_u32(0x80083478u) == 2u ? 0x80089F6Cu : 0x80089E54u;
        cd_upload_image_clut(request);
    }
    if (r_u32(0x80083484u) == 5u)
    {
        cd_upload_image_clut(cd_find_tima_chunk(data, 13));
        cd_upload_image_clut(cd_find_tima_chunk(data, 14));
    }
    DrawSync(0);
    result = game_pop_checkpoint();
    guest_stack_pop(0xA0u);
    return result;
}

sint32 cd_load_versioned_rec_archive(const uint8 *src, size_t size, sint32 mirror)
{
    uint16 version;
    size_t light_off = 0u;
    size_t pal_off = 0u;
    uint32 light_words;
    uint32 pal_words;

    FUNCTION_MARKER(0x8002BF90u, "MAIN.EXE");
    if (!src || size < 8u || xport_load_le16(src) != 0xBABEu)
        return 0;
    version = xport_load_le16(src + 2u);
    if (version < 4u || version > 7u)
        return 0;
    if (version >= 5u)
    {
        if (size < (version == 5u ? 16u : 20u))
            return 0;
        pal_words = xport_load_le32(src + (version == 5u ? 8u : 12u));
        light_words = xport_load_le32(src + (version == 5u ? 12u : 16u));
        if (pal_words > size / 4u || light_words > pal_words)
            return 0;
        light_off = (size_t)light_words * 4u;
        pal_off = (size_t)pal_words * 4u;
        if ((pal_off - light_off) % 56u != 0u || pal_off == light_off || (pal_off - light_off) / 56u > 256u)
            return 0;
    }
    if (route_decode_archive(src, size, &route_resources) != ROUTE_DECODE_OK)
        return 0;
    if (version == 4u)
    {
        render_select_lighting(mirror ? RENDER_LIGHT_MIRROR : RENDER_LIGHT_NORMAL);
        render_select_palette(mirror != 0);
    }
    else
    {
        render_load_lighting(src + light_off, pal_off - light_off);
        render_load_palette(src + pal_off, size - pal_off);
    }
    return 1;
}

sint32 cd_publish_sections(const uint8 *src, size_t size)
{
    FUNCTION_MARKER(0x8002C1D8u, "MAIN.EXE");
    scene_load_sections(src, size);
    return scene_sections.count != 0u;
}

sint32 cd_wait_stream(void)
{
    FUNCTION_MARKER(0x8002C208u, "MAIN.EXE");
    while (cd_stream_is_complete() == 0)
        cd_process_sector_cb();
    return 1;
}

sint32 cd_search_file(uint32 path, uint32 location, uint32 size)
{
    const char *disc_path;
    char image_path[1024];
    char component[128];
    uint8 sector[2048];
    uint8 *directory = NULL;
    uint8 *path_table = NULL;
    FILE *file = NULL;
    uint32 extent = 0u;
    uint32 data_size = 0u;
    uint32 parent_directory_size = 0u;
    uint32 path_table_extent = 0u;
    uint32 path_table_size = 0u;
    uint32 stride = 2048u;
    uint32 data_offset = 0u;
    uint32 cursor = 0u;
    uint32 index;
    sint32 found = 0;

    FUNCTION_MARKER(0x8002C450u, "MAIN.EXE");
    rr_cd_search_refreshed_directory = 0;
    disc_path = getenv("XPORT_DISC_IMAGE");
    if (!disc_path)
        return 0;
    while (disc_path[cursor] && cursor + 1u < sizeof(image_path))
    {
        image_path[cursor] = disc_path[cursor];
        ++cursor;
    }
    image_path[cursor] = '\0';
    if (cursor >= 4u && image_path[cursor - 4u] == '.' && (image_path[cursor - 3u] == 'c' || image_path[cursor - 3u] == 'C') && (image_path[cursor - 2u] == 'u' || image_path[cursor - 2u] == 'U') && (image_path[cursor - 1u] == 'e' || image_path[cursor - 1u] == 'E'))
    {
        char line[1024];
        FILE *cue = fopen(image_path, "rb");

        if (!cue || !fgets(line, sizeof(line), cue))
        {
            if (cue)
                fclose(cue);
            return 0;
        }
        fclose(cue);
        for (index = cursor; index > 0u; --index)
            if (image_path[index - 1u] == '\\' || image_path[index - 1u] == '/')
                break;
        cursor = index;
        for (index = 0u; line[index] && line[index] != '"'; ++index)
            ;
        if (line[index] != '"')
            return 0;
        ++index;
        while (line[index] && line[index] != '"' && cursor + 1u < sizeof(image_path))
            image_path[cursor++] = line[index++];
        image_path[cursor] = '\0';
        if (line[index] != '"')
            return 0;
        stride = 2352u;
        data_offset = 24u;
    }
    file = fopen(image_path, "rb");
    if (!file || fseek(file, (long)(16u * stride + data_offset), SEEK_SET) != 0 || fread(sector, 1u, sizeof(sector), file) != sizeof(sector) || sector[0] != 1u || sector[1] != 'C' || sector[2] != 'D' || sector[3] != '0' || sector[4] != '0' || sector[5] != '1')
        goto finish;
    extent = (uint32)sector[158] | ((uint32)sector[159] << 8) | ((uint32)sector[160] << 16) | ((uint32)sector[161] << 24);
    data_size = (uint32)sector[166] | ((uint32)sector[167] << 8) | ((uint32)sector[168] << 16) | ((uint32)sector[169] << 24);
    path_table_size = (uint32)sector[132] | ((uint32)sector[133] << 8) | ((uint32)sector[134] << 16) | ((uint32)sector[135] << 24);
    path_table_extent = (uint32)sector[140] | ((uint32)sector[141] << 8) | ((uint32)sector[142] << 16) | ((uint32)sector[143] << 24);
    path_table = (uint8 *)malloc(path_table_size ? path_table_size : 1u);
    if (!path_table)
        goto finish;
    for (index = 0u; index * 2048u < path_table_size; ++index)
    {
        uint32 transfer = path_table_size - index * 2048u;

        if (transfer > 2048u)
            transfer = 2048u;
        if (fseek(file, (long)((path_table_extent + index) * stride + data_offset), SEEK_SET) != 0 || fread(path_table + index * 2048u, 1u, transfer, file) != transfer)
            goto finish;
    }
    while (r_u8(path) == '\\' || r_u8(path) == '/')
        ++path;
    while (r_u8(path) != 0u)
    {
        uint32 component_length = 0u;
        uint32 directory_cursor = 0u;
        uint32 sector_index;
        uint32 sector_count = (data_size + 2047u) / 2048u;
        sint32 last_component;

        while (r_u8(path) != 0u && r_u8(path) != '\\' && r_u8(path) != '/')
        {
            uint8 character = r_u8(path++);

            if (character == ';')
            {
                while (r_u8(path) != 0u && r_u8(path) != '\\' && r_u8(path) != '/')
                    ++path;
                break;
            }
            if (component_length + 1u >= sizeof(component))
                goto finish;
            component[component_length++] = (char)character;
        }
        component[component_length] = '\0';
        while (r_u8(path) == '\\' || r_u8(path) == '/')
            ++path;
        last_component = r_u8(path) == 0u;
        if (last_component)
        {
            uint32 directory_index = 1u;
            uint32 path_table_cursor = 0u;

            rr_cd_search_parent_lba = extent;
            parent_directory_size = data_size;
            rr_cd_search_parent_index = 0u;
            while (path_table_cursor + 8u <= path_table_size)
            {
                uint32 name_length = path_table[path_table_cursor];
                uint32 path_extent;

                if (name_length == 0u || path_table_cursor + 8u + name_length > path_table_size)
                    break;
                path_extent = (uint32)path_table[path_table_cursor + 2u] | ((uint32)path_table[path_table_cursor + 3u] << 8) | ((uint32)path_table[path_table_cursor + 4u] << 16) | ((uint32)path_table[path_table_cursor + 5u] << 24);
                if (path_extent == extent)
                {
                    rr_cd_search_parent_index = directory_index;
                    break;
                }
                path_table_cursor += 8u + name_length + (name_length & 1u);
                ++directory_index;
            }
            if (rr_cd_search_parent_index == 0u)
                goto finish;
        }
        free(directory);
        directory = (uint8 *)malloc(data_size ? data_size : 1u);
        if (!directory)
            goto finish;
        for (sector_index = 0u; sector_index < sector_count; ++sector_index)
        {
            uint32 transfer = data_size - sector_index * 2048u;

            if (transfer > 2048u)
                transfer = 2048u;
            if (fseek(file, (long)((extent + sector_index) * stride + data_offset), SEEK_SET) != 0 || fread(directory + sector_index * 2048u, 1u, transfer, file) != transfer)
                goto finish;
        }
        found = 0;
        while (directory_cursor < data_size)
        {
            uint32 record_length = directory[directory_cursor];
            uint32 name_length;
            uint32 comparable_length;
            sint32 equal = 1;

            if (record_length == 0u)
            {
                directory_cursor = (directory_cursor + 2048u) & ~2047u;
                continue;
            }
            if (directory_cursor + record_length > data_size || record_length < 34u)
                goto finish;
            name_length = directory[directory_cursor + 32u];
            comparable_length = name_length;
            for (index = 0u; index < name_length; ++index)
                if (directory[directory_cursor + 33u + index] == ';')
                {
                    comparable_length = index;
                    break;
                }
            if (comparable_length != component_length)
                equal = 0;
            for (index = 0u; equal && index < comparable_length; ++index)
            {
                uint8 left = directory[directory_cursor + 33u + index];
                uint8 right = (uint8)component[index];

                if (left >= 'a' && left <= 'z')
                    left = (uint8)(left - 'a' + 'A');
                if (right >= 'a' && right <= 'z')
                    right = (uint8)(right - 'a' + 'A');
                if (left != right)
                    equal = 0;
            }
            if (equal)
            {
                extent = (uint32)directory[directory_cursor + 2u] | ((uint32)directory[directory_cursor + 3u] << 8) | ((uint32)directory[directory_cursor + 4u] << 16) | ((uint32)directory[directory_cursor + 5u] << 24);
                data_size = (uint32)directory[directory_cursor + 10u] | ((uint32)directory[directory_cursor + 11u] << 8) | ((uint32)directory[directory_cursor + 12u] << 16) | ((uint32)directory[directory_cursor + 13u] << 24);
                if ((!last_component && (directory[directory_cursor + 25u] & 2u) == 0u) || (last_component && (directory[directory_cursor + 25u] & 2u) != 0u))
                    goto finish;
                found = 1;
                break;
            }
            directory_cursor += record_length;
        }
        if (!found)
            goto finish;
        if (last_component)
            break;
    }
    if (found)
    {
        uint32 absolute_sector = extent + 150u;
        uint32 minute = absolute_sector / (75u * 60u);
        uint32 second = (absolute_sector / 75u) % 60u;
        uint32 frame = absolute_sector % 75u;

        w_u32(size, data_size);
        w_u8(location, (minute / 10u) * 16u + minute % 10u);
        w_u8(location + 1u, (second / 10u) * 16u + second % 10u);
        w_u8(location + 2u, (frame / 10u) * 16u + frame % 10u);
        w_u8(location + 3u, 0u);
    }
finish:
    free(path_table);
    free(directory);
    if (file)
        fclose(file);
    return found ? r_u8(location + 3u) : 0;
}

sint32 cd_start_async_read(void)
{
    sint32 result;

    FUNCTION_MARKER(0x8002C4DCu, "MAIN.EXE");
    w_u32(0x800B6BC8u, 1u);
    result = (sint32)r_u32(0x800B6AA8u) + 10;
    w_u32(0x800B6AA8u, (uint32)result);
    return result;
}

sint32 cd_prepare_alloc_read(uint32 path)
{
    sint32 size;
    uint32 index;

    FUNCTION_MARKER(0x8002C5E4u, "MAIN.EXE");
    cd_search_file(path, 0x800B6818u, 0x800B6814u);
    size = (sint32)r_u32(0x800B6814u);
    w_u8(0x800B3F02u, 0u);
    w_u16(0x800B6804u, 0u);
    w_u16(0x800B6800u, 0u);
    w_u16(0x800B680Cu, 0u);
    w_u16(0x800B6808u, UINT16_C(0xFFFF));
    w_u32(0x800B67F0u, 0u);
    w_u16(0x800B6814u, 0u);
    w_u32(0x800B67F4u, (uint32)size);
    w_u16(0x800B67FCu, (uint16)(((uint32)size + 2047u) >> 11));
    if ((r_u8(0x800B3EF8u) & 1u) != 0u)
    {
        w_u32(0x800B67F8u, (uint32)-1);
        for (index = 0u; index < 20u; ++index)
            w_u8(0x800B95F8u + index, 0u);
        w_u16(0x800B6810u, UINT16_C(0xFFFF));
        w_u32(0x800B67ECu, 0u);
    }
    else
    {
        w_u32(0x800B67F8u, (uint32)size);
        w_u32(0x800B67E8u, game_alloc_arena_bytes(size));
        w_u32(0x800B67ECu, r_u32(0x800B67E8u));
    }
    return cd_start_async_read();
}

sint32 cd_prepare_buf_read(uint32 path, uint32 buffer)
{
    sint32 size;
    uint32 index;

    FUNCTION_MARKER(0x8002C698u, "MAIN.EXE");
    cd_search_file(path, 0x800B6818u, 0x800B6814u);
    size = (sint32)r_u32(0x800B6814u);
    w_u32(0x800B67F8u, (uint32)-1);
    w_u16(0x800B6804u, 0u);
    w_u16(0x800B6800u, 0u);
    w_u16(0x800B680Cu, 0u);
    w_u16(0x800B6808u, (uint16)-1);
    w_u8(0x800B3F02u, 0u);
    w_u32(0x800B67F0u, 0u);
    w_u16(0x800B6814u, 0u);
    w_u32(0x800B67F4u, (uint32)size);
    w_u16(0x800B67FCu, ((uint32)size + 2047u) >> 11);
    if ((r_u8(0x800B3EF8u) & 1u) != 0u)
    {
        for (index = 0u; index < 20u; ++index)
            w_u8(0x800B95F8u + index, 0u);
        w_u16(0x800B6810u, (uint16)-1);
    }
    w_u32(0x800B67ECu, buffer);
    w_u32(0x800B67E8u, buffer);
    return cd_start_async_read();
}

sint32 cd_restart_stream_buf(void)
{
    FUNCTION_MARKER(0x8002C770u, "MAIN.EXE");
    w_u32(0x800B6804u, 0u);
    w_u32(0x800B6800u, 0u);
    w_u32(0x800B680Cu, 0u);
    w_u32(0x800B6808u, (uint32)-1);
    w_u32(0x800B67F0u, 0u);
    w_u32(0x800B6814u, 0u);
    if ((r_u8(0x800B3EF8u) & 1u) != 0u)
        w_u32(0x800B6810u, (uint32)-1);
    w_u32(0x800B67E8u, r_u32(0x800B67ECu));
    w_u8(0x800B3F01u, 0u);
    return cd_start_async_read();
}

sint32 cd_stream_is_complete(void)
{
    FUNCTION_MARKER(0x8002C810u, "MAIN.EXE");
    return r_u32(0x800B67FCu) == 0u || r_u32(0x800B6800u) >= r_u32(0x800B67FCu);
}

sint32 cd_copy_sector_bytes(uint32 destination, uint32 source, sint32 size)
{
    sint32 index;
    sint32 result = 0;

    FUNCTION_MARKER(0x8002C84Cu, "MAIN.EXE");
    for (index = 0; index < size; ++index)
    {
        w_u8(destination + (uint32)index, r_u8(source + (uint32)index));
        result = index + 1 < size;
    }
    return result;
}

uint32 cd_search_parent_location(void)
{
    return rr_cd_search_parent_lba;
}

sint32 cd_search_refreshed(void)
{
    return rr_cd_search_refreshed_directory;
}

sint32 cd_process_sector_cb(void)
{
    sint16 next;
    uint32 source;
    sint32 result = 0;

    FUNCTION_MARKER(0x8002C8C8u, "MAIN.EXE");
    if (r_u8(0x800B3F01u) != 0u || r_u8(0x800B3F02u) != 0u)
    {
        menu_fn_8006776c();
        cd_set_data_cb(0u);
        CdReadyCallback(0);
        global_fn_8006777c();
        if (r_u8(0x800B3F02u) != 0u)
            w_u8(0x800B3F02u, 0u);
        return cd_restart_stream_buf();
    }

    w_u8(0x800B3F01u, (uint8)(CdStatus() & 0x10));
    if ((r_u8(0x800B3EF8u) & 1u) == 0u)
        return 0;
    next = (sint16)(r_u16(0x800B6810u) + 1u);
    if (next == 20)
        next = 0;
    if (r_u8(0x800B95F8u + (uint32)next) == 0u)
        return 0;

    source = 0x800B960Cu + ((uint32)(uint16)next << 11);
    if (r_u16(0x800B6800u) == 0u)
    {
        uint32 size;

        menu_fn_8006776c();
        size = r_u32(source);
        source += 4u;
        w_u32(0x800B67F8u, size);
        if ((sint32)size < 0)
        {
            w_u16(0x800B6814u, 1u);
            w_u32(0x800B67F8u, size & 0x7FFFFFFFu);
        }
        if (r_u32(0x800B67ECu) == 0u)
        {
            uint32 buffer = game_alloc_arena_bytes((sint32)r_u32(0x800B67F8u));
            w_u32(0x800B67ECu, buffer);
            w_u32(0x800B67E8u, buffer);
        }
        global_fn_8006777c();
    }

    if (r_u16(0x800B6814u) != 0u)
    {
        uint16 size = 2048u;
        uint16 block = r_u16(0x800B6800u);
        uint32 destination = r_u32(0x800B67E8u);

        if (block == (uint16)(r_u16(0x800B67FCu) - 1u))
            size = (uint16)(r_u16(0x800B67F4u) - (block << 11));
        if (block == 0u)
            size = (uint16)(size - 4u);
        menu_fn_8006776c();
        xport_update_u32(0x800B67F0u, XPORT_MEMORY_UPDATE_ADD, size);
        xport_update_u32(0x800B67E8u, XPORT_MEMORY_UPDATE_ADD, size);
        global_fn_8006777c();
        result = cd_copy_sector_bytes(destination, source, size);
    }
    else
    {
        sint32 size = cd_decompress_backrefs(source, r_u32(0x800B67E8u));
        xport_update_u32(0x800B67F0u, XPORT_MEMORY_UPDATE_ADD, (uint16)size);
        xport_update_u32(0x800B67E8u, XPORT_MEMORY_UPDATE_ADD, (uint16)size);
        result = size;
    }

    menu_fn_8006776c();
    w_u8(0x800B95F8u + (uint32)(uint16)next, 0u);
    w_u16(0x800B6800u, (uint16)(r_u16(0x800B6800u) + 1u));
    global_fn_8006777c();
    w_u16(0x800B6810u, (uint16)next);
    return result;
}

sint32 cd_stop_read(void)
{
    FUNCTION_MARKER(0x8002CB38u, "MAIN.EXE");
    CdControl(9u, 0, 0);
    menu_fn_8006776c();
    cd_set_data_cb(0u);
    CdReadyCallback(0);
    global_fn_8006777c();
    return 0;
}

static uint32 cd_read_file_alloc_span(uint32 path, size_t *span_size)
{
    size_t size;
    size_t read_size;
    uint32 destination;
    sint32 refreshed_directory;

    FUNCTION_MARKER(0x8002CB80u, "MAIN.EXE");
    if (span_size)
        *span_size = 0u;
    cd_search_file(path, 0x800B6818u, 0x800B6814u);
    refreshed_directory = cd_search_refreshed();
    w_u32(0x800B6814u, 0u);
    if (!cd_host_file_size(path, &size) || size > 0x7FFFFFFFu)
        return 0u;
    w_u32(0x800B67F8u, (uint32)size);
    destination = game_alloc_arena_bytes((sint32)size);
    if ((destination & 0x1FFFFFFFu) > sizeof(DRAM) || size > sizeof(DRAM) - (destination & 0x1FFFFFFFu) || !cd_read_host_file(path, destination, size, &read_size, refreshed_directory ? 6u : 4u, 3u, refreshed_directory ? 3u : 1u, 3u, 0u, 0u, 0u) || read_size != size)
        return 0u;
    w_u32(0x800B67F0u, (uint32)size);
    // MAIN.EXE:0x8002C5E4 and CD callbacks keep pointers separate from sector counts
    w_u32(0x800B67F4u, (uint32)size);
    w_u32(0x800B67E8u, destination + (uint32)size);
    w_u16(0x800B67FCu, (uint16)((size + 2047u) / 2048u));
    w_u16(0x800B6800u, r_u16(0x800B67FCu));
    w_u16(0x800B6804u, r_u16(0x800B67FCu));
    w_u32(0x800B67ECu, destination);
    cd_stop_read();
    if (span_size)
        *span_size = read_size;
    return destination;
}

uint32 cd_read_file_alloc(uint32 path)
{
    return cd_read_file_alloc_span(path, NULL);
}

uint32 cd_read_file_buf(uint32 path, uint32 buffer)
{
    size_t size;
    size_t read_size;
    sint32 refreshed_directory;

    FUNCTION_MARKER(0x8002CD44u, "MAIN.EXE");
    cd_search_file(path, 0x800B6818u, 0x800B6814u);
    refreshed_directory = cd_search_refreshed();
    w_u32(0x800B6814u, 0u);
    if (!cd_host_file_size(path, &size) || (buffer & 0x1FFFFFFFu) > sizeof(DRAM) || size > sizeof(DRAM) - (buffer & 0x1FFFFFFFu) || !cd_read_host_file(path, buffer, size, &read_size, refreshed_directory ? 3u : 6u, 3u, refreshed_directory ? 1u : 3u, 3u, refreshed_directory ? 0u : 3u, refreshed_directory ? 2u : 0u, 1u) || read_size != size)
        return 0u;
    w_u32(0x800B67F0u, (uint32)size);
    // MAIN.EXE:0x8002C698 stores the caller buffer at GP+2A68, not GP+2A78
    w_u32(0x800B67F4u, (uint32)size);
    w_u32(0x800B67ECu, buffer);
    w_u32(0x800B67E8u, buffer + (uint32)size);
    w_u16(0x800B67FCu, (uint16)((size + 2047u) / 2048u));
    w_u16(0x800B6800u, r_u16(0x800B67FCu));
    w_u16(0x800B6804u, r_u16(0x800B67FCu));
    cd_stop_read();
    return (uint32)size;
}

sint32 cd_upload_vram_image(uint32 path, sint16 x, sint16 y, sint32 width, sint32 height)
{
    uint32 size = (uint32)width * (uint32)height * 2u;
    uint32 buffer;
    PSX_RECT rectangle;

    FUNCTION_MARKER(0x8002CE6Cu, "MAIN.EXE");
    buffer = game_alloc_arena_bytes((sint32)size);
    cd_read_file_buf(path, buffer);
    setRECT(&rectangle, x, y, width, height);
    LoadImagePSX(&rectangle, (uint32 *)psx_addr(buffer, size));
    DrawSync(0);
    return cd_stop_read();
}

sint32 cd_update_audio(sint32 trigger)
{
    SpuCommonAttr attr = {0};
    sint32 state = (sint32)r_u32(0x800B6B28u);
    sint32 mode = (sint32)r_u32(0x800B6B30u);
    sint32 index;
    sint32 result;
    uint16 volume;

    FUNCTION_MARKER(0x800362ACu, "MAIN.EXE");
    if (state == 2)
        return 2;
    if (mode != 2 || trigger != 1)
        return 1;
    if (state != 0)
    {
        if (state != 1)
            return 5;
        result = CdControl(3u, 0, 0);
        if (result != 0)
            w_u32(0x800B6B28u, (uint32)mode);
        return result;
    }
    index = r_u32(0x80083484u) == 5u ? 2 : (sint16)sound_options.tracks[sound_options.track_slot];
    if (index < 2 || index >= (sint32)r_u32(0x800B6B88u))
        return index < 2;
    if (CdControl(3u, (uint8 *)psx_addr(0x800E1BD8u + 4u * (uint32)index, 4u), 0) == 0)
        return 0;
    w_u32(0x800B6B28u, 2u);
    w_u32(0x800B6AE8u, (uint32)(race_decode_time_bcd(0x800E1BDCu + 4u * (uint32)index) - 150));
    w_u32(0x800B6C08u, (uint32)race_decode_time_bcd(0x800E1BD8u + 4u * (uint32)index));
    attr.mask = 0xC0u;
    volume = r_u16(0x80083494u);
    attr.cd.volume.left = (sint16)(uint16)(volume << 9);
    attr.cd.volume.right = attr.cd.volume.left;
    SpuSetCommonAttr(&attr);
    return 0;
}

sint32 cd_load_model_archive(void)
{
    uint8 *src = NULL;
    const PLAYER_PROFILE *table;
    size_t archive_size, read_size;
    sint32 players = (sint32)r_u32(0x80083478u);
    MESH_ARCHIVE decoded = {0};
    MESH_BOAT_MODELS models[2][16] = {0};
    MESH_MODEL event_models[4] = {0};
    sint32 player;
    sint32 record;

    FUNCTION_MARKER(0x80042454u, "MAIN.EXE");
    table = profile_current();
    if (!xport_file_size("DATA/BOATS/MODELS.IFF", &archive_size) || archive_size == 0u)
        goto fail;
    src = malloc(archive_size);
    if (!src || !xport_file_read("DATA/BOATS/MODELS.IFF", src, archive_size, &read_size) || read_size != archive_size || !mesh_decode_archive(src, archive_size, &decoded))
        goto fail;
    free(src);
    src = NULL;
    if (players < 1 || players > 2)
        goto fail;
    for (record = 15; record >= 0; --record)
    {
        const RACE_PARTICIPANT *participant = &race_participants[record];
        uint32 kind = participant->mode;
        sint32 model;
        sint32 player;

        if (kind < 2u)
        {
            if (participant->boat >= 9u)
                goto fail;
            model = vehicle_base_levels[participant->boat];
        }
        else if (kind < 4u)
        {
            uint8 row = participant->boat;

            model = profile_get_table_byte(table, row, 0);
        }
        else
            model = -1;
        if (model <= 0)
            continue;
        for (player = 0; player < players; ++player)
        {
            if (!mesh_load_boat_models(&decoded, participant->boat, model, participant->variant, game_selection.flags, &models[player][record]))
                goto fail;
        }
    }
    if (r_u32(0x80083484u) == 8u)
    {
        if (decoded.count < 4u)
            goto fail;
        for (record = 0; record < 4; ++record)
            if (!mesh_copy_model(&decoded.models[record], &event_models[record]))
                goto fail;
    }
    mesh_clear_archive(&decoded);
    for (record = 0; record < 4; ++record)
    {
        mesh_clear_model(&mesh_event_models[record]);
        mesh_event_models[record] = event_models[record];
    }
    for (player = 0; player < 2; ++player)
        for (record = 0; record < 16; ++record)
        {
            mesh_clear_boat_models(&mesh_boat_models[player][record]);
            mesh_boat_models[player][record] = models[player][record];
        }

    return 1;
fail:
    free(src);
    mesh_clear_archive(&decoded);
    for (record = 0; record < 4; ++record)
        mesh_clear_model(&event_models[record]);
    for (player = 0; player < 2; ++player)
        for (record = 0; record < 16; ++record)
            mesh_clear_boat_models(&models[player][record]);
    return 0;
}

uint32 cd_load_selected_asset(uint32 path)
{
    uint32 result;

    FUNCTION_MARKER(0x8004AEB4u, "MAIN.EXE");
    w_u8(0x800D6958u, r_u8(0x800B40F0u));
    w_u8(0x800D6959u, r_u8(0x800B40F1u));
    result = cd_load_file_alloc(path);
    w_u32(0x800B6A38u, result);
    return result;
}

sint32 cd_retry_read_sequence(uint32 request)
{
    sint16 control_attempt = 0;
    sint16 read_attempt = 0;

    FUNCTION_MARKER(0x80063168u, "MAIN.EXE");
    for (;;)
    {
        if (CdControl(0x15u, (uint8 *)psx_addr(request, 4u), 0) == 0)
        {
            ++control_attempt;
            if (control_attempt == 5)
                return 0;
            continue;
        }
        ++read_attempt;
        control_attempt = 0;
        if (read_attempt == 6)
            return 0;
        if (CdRead2(448) != 0)
            return 1;
    }
}

sint32 cd_adjust_audio_volume(uint32 enabled)
{
    SpuCommonAttr attr = {0};

    FUNCTION_MARKER(0x8006343Cu, "MAIN.EXE");
    if (enabled != 0u)
    {
        SpuGetCommonAttr(&attr);
        attr.cd.volume.left = attr.cd.volume.left < 4097 ? 0 : (sint16)(attr.cd.volume.left - 4096);
        attr.cd.volume.right = attr.cd.volume.right < 4097 ? 0 : (sint16)(attr.cd.volume.right - 4096);
    }
    attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    SpuSetCommonAttr(&attr);
    if (enabled != 0u && attr.cd.volume.left == 0 && attr.cd.volume.right == 0)
        return 2;
    return 0;
}

void cd_clear_data_cbs(void)
{
    FUNCTION_MARKER(0x8007328Cu, "MAIN.EXE");
    menu_fn_8006776c();
    CdDataCallback(0);
    CdReadyCallback(0);
    w_u8(r_u32(0x800A11DCu), 0u);
    w_u8(r_u32(0x800A11E8u), 0u);
    global_fn_8006777c();
}
