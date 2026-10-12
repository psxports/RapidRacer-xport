#include "callbacks.h"
#include "route.h"
#include "render.h"
#include "sound.h"
#include "profile.h"
#include "game.h"
#include "effects.h"
#include "mesh.h"
#include "boat.h"
#include "menu.h"
#include "pickup.h"
#include "track.h"
#include "timer.h"
#include "upgrades.h"
#include "psx_gpu.h"
#include "cd.h"
#include "arena.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

typedef struct CD_AUDIO
{
    CdlLOC toc[100];
    sint32 tracks;
    sint32 state;
    sint32 mode;
    sint32 pos;
    sint32 end;
} CD_AUDIO;

static CD_AUDIO cd_audio;
char cd_dir[128];

// Course directories from MAIN.EXE 80080D80/800B3E9C/80080D8C/800B3EA4..3EBC
static const char *const cd_course_dirs[7] = {"\\CANYON\\", "\\MIAMI\\", "\\ALASKA\\", "\\WILD\\", "\\LOST\\", "\\LAVA\\", "\\COMP\\"};

// Resident TIM from reviewed MAIN.EXE
static const uint32 cd_notice_tim_1p[] = {
    0x00000010, 0x00000008, 0x0000002C, 0x017002C0, 0x00010010, 0x379D0000, 0x2E542ED9, 0x196B29F1, 0x35866F6D, 0x14A2630B, 0x4E694607, 0x5AA91440, 0x24E21907, 0x000000DE, 0x017803F1, 0x00230003, 0x00CCCC00, 0x77CC0000, 0x00009C77, 0x977AAAA7, 0xBBBA0009, 0x000977AA, 0x77ABDDDB, 0x88DD00F9, 0x009F7ABD, 0x7BDAAB88, 0xCCA80097, 0x0F97ADFC, 0xAA4111EB, 0x11370FF7, 0x0FF7B721, 0xBA31112F, 0x112F0FF7, 0x0FF7B871, 0xB6B3112F, 0x111E0F97, 0x0097B6A3, 0xB6A21115, 0x11150097, 0x0097B6A3, 0xB6B31113, 0x11140097, 0x009FB684, 0xA8651112, 0x1111009F, 0x000FA867, 0x7D6A2111, 0x41110009, 0x00097D6D, 0x7B6D4111, 0x51110009, 0x00097A88, 0xFA867211, 0x72110009, 0x000097B6, 0x97B8D511, 0x6ACC0000, 0x00009FAD, 0x9F7B88DB, 0xB8860000, 0x00000C7A, 0x0CF7ABD8, 0x7ABB0000, 0x000000C7, 0x000977AA, 0xCF770000, 0x0000000C, 0x00000CCC, 0x00000000,
};

// Resident TIM from reviewed MAIN.EXE
static const uint32 cd_notice_tim_2p[] = {
    0x00000010, 0x00000008, 0x0000002C, 0x010102F0, 0x00010010, 0x379D0000, 0x2E542ED9, 0x196B29F1, 0x35866F6D, 0x14A2630B, 0x4E694607, 0x5AA91440, 0x24E21907, 0x0000009C, 0x011302EE, 0x00120004, 0x999999CC, 0x000000CC, 0x7AABBAA7, 0x000099F7, 0xBDDDDDDD, 0x0009F77A, 0xA7777B88, 0x009977AD, 0x4222257D, 0x0FC77AD7, 0x411114FB, 0x0FC77A8A, 0xD11111FA, 0x009F7A86, 0xB511112E, 0x00997A86, 0x8E111114, 0x00097AD6, 0x6A511111, 0x0009FAB6, 0x68E21111, 0x000997B8, 0x66A51111, 0x0000C7AD, 0xD68E2111, 0x0000997A, 0xAD6B5222, 0x00000CF7, 0x7AD8DA77, 0x000000C7, 0xF7ABD888, 0x0000000C, 0xC9F77AAB, 0x00000000, 0x00CCC999, 0x00000000,
};

void cd_init_audio_toc(void)
{
    static const CdlLOC original_toc[10] = {
        {0x51, 0x15, 0x00, 0x00}, {0x00, 0x02, 0x00, 0x00}, {0x16, 0x52, 0x00, 0x00}, {0x21, 0x52, 0x00, 0x00}, {0x26, 0x40, 0x00, 0x00}, {0x31, 0x41, 0x00, 0x00}, {0x37, 0x14, 0x00, 0x00}, {0x42, 0x34, 0x00, 0x00}, {0x47, 0x35, 0x00, 0x00}, {0x50, 0x44, 0x00, 0x00},
    };

    CdSetToc(original_toc, 9);
    cd_audio.tracks = CdGetToc(cd_audio.toc);
}

static sint32 cd_upload_image_clut(const uint8 *src, uint32 size)
{
    GpuTimImage image;

    FUNCTION_MARKER(0x8002BAE8, "MAIN.EXE");
    if (!src || size < 4 || xport_load_le32(src) != 0x10 || !gpu_decode_tim_body(src + 4u, size - 4, &image))
        abort();
    if (image.clut_data)
        LoadImagePSX(&image.clut, (uint32 *)image.clut_data);
    return LoadImagePSX(&image.pixels, (uint32 *)image.pixel_data);
}

static const uint8 *cd_find_tima_chunk(const uint8 *src, uint32 size, sint32 idx, uint32 *tim_size)
{
    uint32 off = 12;
    uint32 end;
    sint32 chunk_idx = 0;

    FUNCTION_MARKER(0x8002BB58, "MAIN.EXE");
    if (tim_size)
        *tim_size = 0;
    if (!src || !tim_size || idx < 0 || size < sizeof(IFF_HEADER))
        return NULL;
    const IFF_HEADER *header = (const IFF_HEADER *)src;
    if (header->chunk.id != CHUNK_FORM || header->type != CHUNK_JETS)
        return NULL;
    end = swap32(header->chunk.size_be);
    if (end < 4 || end > size - 8)
        return NULL;
    end += 8;
    while (off < end)
    {
        uint32 chunk_size;
        uint32 padded_size;

        if (end - off < 8)
            return NULL;
        const IFF_CHUNK *chunk = (const IFF_CHUNK *)(src + off);
        chunk_size = swap32(chunk->size_be);
        off += 8;
        if (chunk_size > end - off)
            return NULL;
        padded_size = chunk_size + (4 - (chunk_size & 3) & 3);
        if (padded_size < chunk_size || padded_size > end - off)
            return NULL;
        if (chunk->id == CHUNK_TIMA)
        {
            if (chunk_idx == idx)
            {
                *tim_size = chunk_size;
                return src + off;
            }
            ++chunk_idx;
        }
        off += padded_size;
    }
    return NULL;
}

static void cd_upload_archive_tim(const uint8 *src, uint32 size, sint32 idx)
{
    uint32 tim_size;
    const uint8 *tim = cd_find_tima_chunk(src, size, idx, &tim_size);
    if (!tim || tim_size < 20)
        abort();
    cd_upload_image_clut(tim, tim_size);
}

static sint32 cd_stop_read(void)
{
    FUNCTION_MARKER(0x8002CB38, "MAIN.EXE");
    CdControl(9, 0, 0);
    CdDataCallback(NULL);
    CdReadyCallback(0);
    return 0;
}

static sint32 cd_host_file_size(const char *path, uint32 *size)
{
    size_t host_size;
    if (!size)
        return 0;
    *size = 0;
    if (!xport_cd_file_size_native(path, "DATA", &host_size) || host_size > UINT32_MAX)
        return 0;
    *size = host_size;
    return 1;
}

static sint32 cd_read_host_file(const char *path, void *dst, uint32 capacity, uint32 *size)
{
    size_t host_size;
    if (!size)
        return 0;
    *size = 0;
    if (!xport_cd_file_read_native(path, "DATA", dst, capacity, &host_size) || host_size > UINT32_MAX)
        return 0;
    *size = host_size;
    return 1;
}

static uint8 *cd_read_file_alloc_span(const char *path, uint32 *span_size)
{
    uint32 size;
    uint32 read_size;
    uint8 *dst;

    FUNCTION_MARKER(0x8002CB80, "MAIN.EXE");
    if (span_size)
        *span_size = 0;
    if (!cd_host_file_size(path, &size))
        return NULL;
    dst = (uint8 *)arena_alloc(size);
    if (!dst || !cd_read_host_file(path, dst, size, &read_size) || read_size != size)
        return NULL;
    cd_stop_read();
    if (span_size)
        *span_size = read_size;
    return dst;
}

static void cd_build_path(char *dst, const char *src)
{
    uint32 idx = 0;
    const char *dir = cd_dir;

    while (*dir && idx < 125)
        dst[idx++] = *dir++;
    while (*src && idx < 125)
        dst[idx++] = *src++;
    dst[idx++] = ';';
    dst[idx++] = '1';
    dst[idx] = '\0';
}

static sint32 cd_load_archive_assets(const char *path)
{
    char full_path[128];
    const uint8 *data;
    const uint8 *request;
    uint32 size = 0;

    FUNCTION_MARKER(0x8002BE3C, "MAIN.EXE");
    arena_push();
    cd_build_path(full_path, path);
    data = cd_read_file_alloc_span(full_path, &size);
    cd_upload_archive_tim(data, size, 0);
    cd_upload_archive_tim(data, size, 1);
    cd_upload_archive_tim(data, size, 2);
    cd_upload_archive_tim(data, size, 2 * menu_state.language + 3);
    cd_upload_archive_tim(data, size, 2 * menu_state.language + 4);
    if (menu_state.language == 2)
    {
        request = (const uint8 *)(race_cfg.players == 2 ? cd_notice_tim_2p : cd_notice_tim_1p);
        cd_upload_image_clut(request, race_cfg.players == 2 ? sizeof(cd_notice_tim_2p) : sizeof(cd_notice_tim_1p));
    }
    if (race_cfg.mode == 5)
    {
        cd_upload_archive_tim(data, size, 13);
        cd_upload_archive_tim(data, size, 14);
    }
    DrawSync(0);
    return arena_pop();
}

static sint32 cd_load_versioned_rec_archive(const uint8 *src, uint32 size, sint32 mirror)
{
    uint16 version;
    uint32 light_off = 0;
    uint32 pal_off = 0;
    uint32 light_words;
    uint32 pal_words;

    FUNCTION_MARKER(0x8002BF90, "MAIN.EXE");
    if (!src || size < 8 || xport_load_le16(src) != 0xBABE)
        return 0;
    version = xport_load_le16(src + 2u);
    if (version < 4u || version > 7u)
        return 0;
    if (version >= 5u)
    {
        if (size < (version == 5 ? 16u : 20u))
            return 0;
        pal_words = xport_load_le32(src + (version == 5 ? 8u : 12u));
        light_words = xport_load_le32(src + (version == 5 ? 12u : 16u));
        if (pal_words > size / 4 || light_words > pal_words)
            return 0;
        light_off = light_words * 4;
        pal_off = pal_words * 4;
        if ((pal_off - light_off) % 56 != 0 || pal_off == light_off || (pal_off - light_off) / 56 > 256)
            return 0;
    }
    if (route_decode_archive(src, size, &route_resources) != ROUTE_DECODE_OK)
        return 0;
    if (version == 4)
    {
        render_select_lighting(mirror ? RENDER_LIGHT_MIRROR : RENDER_LIGHT_NORMAL);
        render_select_pal(mirror != 0);
    }
    else
    {
        render_load_lighting(src + light_off, pal_off - light_off);
        render_load_pal(src + pal_off, size - pal_off);
    }
    return 1;
}

static sint32 cd_publish_sections(const uint8 *src, uint32 size)
{
    FUNCTION_MARKER(0x8002C1D8, "MAIN.EXE");
    track_load_sections(src, size);
    return track_sections.count != 0;
}

static sint32 cd_load_model_archive(void)
{
    uint8 *src = NULL;
    const PLAYER_PROFILE *table;
    uint32 archive_size, read_size;
    sint32 players = race_cfg.players;
    MESH_ARCHIVE decoded = {0};
    MESH_BOAT_MODELS models[2][16] = {0};
    MESH_MODEL event_models[4] = {0};
    sint32 player;
    sint32 rec;

    FUNCTION_MARKER(0x80042454, "MAIN.EXE");
    table = profile_cur();
    if (!cd_host_file_size("BOATS/MODELS.IFF", &archive_size) || archive_size == 0)
        goto fail;
    src = malloc(archive_size);
    if (!src || !cd_read_host_file("BOATS/MODELS.IFF", src, archive_size, &read_size) || read_size != archive_size || !mesh_decode_archive(src, archive_size, &decoded))
        goto fail;
    free(src);
    src = NULL;
    if (players < 1 || players > 2)
        goto fail;
    for (rec = 15; rec >= 0; --rec)
    {
        const RACE_PARTICIPANT *participant = &race_participants[rec];
        uint32 kind = participant->mode;
        sint32 model;
        sint32 player;

        if (kind < 2)
        {
            if (participant->boat >= 9u)
                goto fail;
            model = boat_base_levels[participant->boat];
        }
        else if (kind < 4)
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
            if (!mesh_load_boat_models(&decoded, participant->boat, model, participant->variant, game_selection.flags, &models[player][rec]))
                goto fail;
        }
    }
    if (race_cfg.mode == 8)
    {
        if (decoded.count < 4)
            goto fail;
        for (rec = 0; rec < 4; ++rec)
            if (!mesh_copy_model(&decoded.models[rec], &event_models[rec]))
                goto fail;
    }
    mesh_clear_archive(&decoded);
    for (rec = 0; rec < 4; ++rec)
    {
        mesh_clear_model(&mesh_event_models[rec]);
        mesh_event_models[rec] = event_models[rec];
    }
    for (player = 0; player < 2; ++player)
        for (rec = 0; rec < 16; ++rec)
        {
            mesh_clear_boat_models(&mesh_boat_models[player][rec]);
            mesh_boat_models[player][rec] = models[player][rec];
        }

    return 1;
fail:
    free(src);
    mesh_clear_archive(&decoded);
    for (rec = 0; rec < 4; ++rec)
        mesh_clear_model(&event_models[rec]);
    for (player = 0; player < 2; ++player)
        for (rec = 0; rec < 16; ++rec)
            mesh_clear_boat_models(&models[player][rec]);
    return 0;
}

sint32 cd_load_course_assets(const RACE_CFG *cfg)
{
    char filename[24];
    const char *suffix;
    const char *path;
    const uint8 *data;
    uint32 size;
    sint32 course;

    FUNCTION_MARKER(0x8002B664, "MAIN.EXE");
    strcpy(cd_dir, "\\BOATS\\");
    if (!cd_load_model_archive())
        return 0;
    if (cfg->mode == 8)
    {
        strcpy(cd_dir, "\\BONUS\\");
        return cd_load_archive_assets(race_cfg.variant == 2 ? "VRAM1.IFF" : "VRAM0.IFF");
    }
    suffix = race_cfg.variant == 1 ? "DAY\\" : race_cfg.variant == 2 ? "NIGHT\\" : "MIRROR\\";
    course = cfg->course;
    if ((uint32)course >= 7)
        exit(1);
    strcpy(cd_dir, cd_course_dirs[course]);
    if (course != 6)
        strcat(cd_dir, suffix);
    if (course == 6)
    {
        snprintf(filename, sizeof(filename), race_cfg.players == 1 ? "VRAM%d.IFF" : "VRAM%d_2P.IFF", (sint16)race_selection.resource);
        cd_load_archive_assets(filename);
        route_gen_init_fractal(cfg);
    }
    else
    {
        cd_load_archive_assets(race_cfg.players == 1 ? "VRAM.IFF" : "VRAM_2P.IFF");
        data = cd_load_file_bytes("COURSE.GPH", &size);
        if (!data)
            return 0;
        cd_publish_sections(data, size);
        path = profile_selection.slot == 0 ? "EASY.JET" : profile_selection.slot == 1 ? "MED.JET" : "HARD.JET";
        data = cd_load_file_bytes(path, &size);
        if (!data || !cd_load_versioned_rec_archive(data, size, race_cfg.variant == 2))
            return 0;
    }
    track_process_published_rec_flags();
    return cfg->course == 6 ? pickup_update_racer_flags() : 6;
}

sint32 cd_init_drive(void)
{
    FUNCTION_MARKER(0x8002BA40, "MAIN.EXE");
    return 1;
}

uint8 *cd_load_file_alloc(const char *path)
{
    char full_path[128];

    FUNCTION_MARKER(0x8002BA60, "MAIN.EXE");
    cd_build_path(full_path, path);
    return cd_read_file_alloc_span(full_path, NULL);
}

const uint8 *cd_load_file_bytes(const char *path, uint32 *size)
{
    char full_path[128];

    if (!size)
        return NULL;
    cd_build_path(full_path, path);
    return cd_read_file_alloc_span(full_path, size);
}

static uint32 cd_read_file_buf(const char *path, void *dst, uint32 capacity)
{
    uint32 size;
    uint32 read_size;

    FUNCTION_MARKER(0x8002CD44, "MAIN.EXE");
    if (!dst || !cd_host_file_size(path, &size) || size > capacity || !cd_read_host_file(path, dst, capacity, &read_size) || read_size != size)
        return 0;
    cd_stop_read();
    return size;
}

uint32 cd_load_file_buf(const char *path, void *dst, uint32 capacity)
{
    char full_path[128];

    FUNCTION_MARKER(0x8002BA9C, "MAIN.EXE");
    cd_build_path(full_path, path);
    return cd_read_file_buf(full_path, dst, capacity);
}

static sint32 cd_upload_vram_image(const char *path, sint16 x, sint16 y, sint32 width, sint32 height)
{
    uint32 size = width * height * 2u;
    uint8 *buffer;
    PSX_RECT rect;

    FUNCTION_MARKER(0x8002CE6C, "MAIN.EXE");
    buffer = (uint8 *)arena_alloc(size);
    if (!buffer || cd_read_file_buf(path, buffer, size) != size)
        return 0;
    setRECT(&rect, x, y, width, height);
    LoadImagePSX(&rect, (uint32 *)buffer);
    DrawSync(0);
    return cd_stop_read();
}

sint32 cd_load_tex(const char *path, sint16 x, sint16 y, sint32 width, sint32 height)
{
    char full_path[128];

    FUNCTION_MARKER(0x8002BDA8, "MAIN.EXE");
    arena_push();
    cd_build_path(full_path, path);
    cd_upload_vram_image(full_path, x, y, width, height);
    return arena_pop();
}

void cd_reset_audio(void)
{
    cd_audio.mode = 2;
    cd_audio.state = 0;
}

void cd_pause_audio(void)
{
    cd_audio.state = 1;
}

void cd_stop_audio(void)
{
    CdControl(8, NULL, NULL);
    cd_audio.state = 0;
}

void cd_set_audio_vol(uint16 vol)
{
    SpuCommonAttr attr = {0};

    attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    attr.cd.volume.left = vol << 9;
    attr.cd.volume.right = attr.cd.volume.left;
    SpuSetCommonAttr(&attr);
}

void cd_handle_ready_cb(uint8 status, uint8 *response)
{
    FUNCTION_MARKER(0x80038950, "MAIN.EXE");
    if (status != CdlDataReady)
    {
        cd_audio.state = 0;
        return;
    }
    if ((response[4] & 0x80u) == 0)
    {
        CdlLOC pos = {response[3], response[4], 0, 0};

        cd_audio.pos = CdPosToInt(&pos);
    }
    if (cd_audio.end < cd_audio.pos)
    {
        cd_set_audio_vol(0);
        cd_audio.state = 0;
    }
}

sint32 cd_update_audio(sint32 trigger)
{
    sint32 state;
    sint32 mode;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x800362AC, "MAIN.EXE");
    state = cd_audio.state;
    mode = cd_audio.mode;
    result = 2;
    if (state == 2)
        goto done;
    result = 1;
    if (mode != 2 || trigger != 1)
        goto done;
    if (state != 0)
    {
        result = 5;
        if (state != 1)
            goto done;
        result = CdControl(3, NULL, NULL);
        if (result != 0)
            cd_audio.state = mode;
        goto done;
    }
    idx = race_cfg.mode == 5 ? 2 : (sint16)sound_options.tracks[sound_options.track_slot];
    result = idx < 2;
    if (idx < 2 || idx >= cd_audio.tracks)
        goto done;
    result = 0;
    if (CdControl(3, (uint8 *)&cd_audio.toc[idx], NULL) == 0)
        goto done;
    cd_audio.state = 2;
    cd_audio.end = CdPosToInt(&cd_audio.toc[idx + 1]) - 150;
    cd_audio.pos = CdPosToInt(&cd_audio.toc[idx]);
    cd_set_audio_vol(sound_mix.music);
done:
    return result;
}

sint32 cd_adjust_audio_volume(uint32 enabled)
{
    SpuCommonAttr attr = {0};

    FUNCTION_MARKER(0x8006343C, "MAIN.EXE");
    if (enabled != 0)
    {
        SpuGetCommonAttr(&attr);
        attr.cd.volume.left = attr.cd.volume.left < 4097 ? 0 : (attr.cd.volume.left - 4096);
        attr.cd.volume.right = attr.cd.volume.right < 4097 ? 0 : (attr.cd.volume.right - 4096);
    }
    attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    SpuSetCommonAttr(&attr);
    if (enabled != 0 && attr.cd.volume.left == 0 && attr.cd.volume.right == 0)
        return 2;
    return 0;
}
