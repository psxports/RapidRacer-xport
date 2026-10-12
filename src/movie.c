#include "movie.h"
#include "game.h"
#include "menu.h"
#include "input.h"
#include "sprite.h"
#include "sound.h"
#include "cd.h"
#include "disp.h"
#include "psx_gpu.h"
#include "psx_press.h"
#include "psx_stream.h"
#include "psx_spu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct
{
    const char *path;
    uint16 rgb24, scr_width, x, y, width, height;
    uint32 frames;
    uint16 volume;
} MOVIE;

#define MOVIE_RING_SECTORS 64u
#define MOVIE_VLC_BYTES 132096u

static sint32 movie_stream_end;

// Intro descriptor from MAIN.EXE 80099148
static SPRITE_IMAGE movie_intro_image = {.path = "INTRO.TIM"};

// Movie descriptors from MAIN.EXE 80098F94..800990F4
// clang-format off
static const MOVIE intro_movies[] = {
    {"\\STREAMS\\INTRO.STR;1", 1, 320, 0, 0, 320, 240, 418, 127},
    {"\\STREAMS\\CHMP_E.STR;1", 1, 320, 0, 0, 320, 240, 1615, 127},
    {"\\STREAMS\\CHMP_M.STR;1", 1, 320, 0, 0, 320, 240, 1622, 127},
    {"\\STREAMS\\CHMP_H.STR;1", 1, 320, 0, 0, 320, 240, 1622, 127},
    {"\\STREAMS\\YOUWIN.STR;1", 1, 320, 0, 0, 320, 240, 1450, 127},
    {"\\STREAMS\\YOULOSE1.STR;1", 1, 512, 0, 0, 512, 240, 99, 127},
    {"\\STREAMS\\YOULOSE2.STR;1", 1, 512, 0, 0, 512, 240, 128, 127},
    {"\\STREAMS\\YOULOSE3.STR;1", 1, 512, 0, 0, 512, 240, 74, 127},
    {"\\STREAMS\\YOULOSE4.STR;1", 1, 512, 0, 0, 512, 240, 121, 127},
    {"\\STREAMS\\YOULOSE5.STR;1", 1, 512, 0, 0, 512, 240, 126, 127},
    {"\\STREAMS\\YOULOSE6.STR;1", 1, 512, 0, 0, 512, 240, 91, 127},
    {"\\STREAMS\\YOULOSE7.STR;1", 1, 512, 0, 0, 512, 240, 100, 127},
};
// clang-format on

static void movie_end_stream(void)
{
    movie_stream_end = 1;
}

static sint32 movie_get_frame(uint32 **src, StHEADER **header, sint32 (*skip)(void), sint32 *skipping)
{
    sint32 waits = 0;
    uint32 *rec;

    while (StGetNext(src, &rec) != 0)
    {
        sint32 progress = stream_pump();
        if (movie_stream_end)
            return 1;
        if (progress < 0)
            return 3;
        if (progress == 0)
        {
            VSync(0);
            input_update_states();
            if (!*skipping && skip && skip())
                *skipping = 1;
            if (++waits == 250)
                return 3;
        }
    }
    *header = (StHEADER *)rec;
    return 0;
}

static sint32 movie_draw_frame(const MOVIE *movie, uint32 *src, uint32 *vlc, uint32 *slice, sint32 page)
{
    PSX_RECT rect;
    sint32 padded_h = movie->height + 15 & ~15;
    sint32 slice_w = movie->rgb24 ? 24 : 16;
    sint32 words = slice_w * padded_h / 2;
    sint32 x;

    // Bound the SDK output command before decoding into the native VLC buffer
    if (((src[0] & 0xFFFF) + 1) * sizeof(uint32) > MOVIE_VLC_BYTES)
        return 0;
    if (DecDCTvlc(src, vlc) != 0)
        return 0;
    DecDCTin(vlc, movie->rgb24 ? 3 : 2);
    setRECT(&rect, movie->rgb24 ? movie->x * 3 / 2 : movie->x, movie->y + 8 + page * 256, slice_w, padded_h);
    for (x = 0; x < movie->width; x += 16)
    {
        DecDCTout(slice, words);
        if (DecDCToutSync(1) != 0)
            return 0;
        LoadImagePSX(&rect, slice);
        rect.x += slice_w;
    }
    return 1;
}

static sint32 movie_play_stream(const MOVIE *movie, sint32 (*skip)(void), sint32 mode)
{
    CdlLOC pos;
    char path[256];
    size_t len;
    DISPENV disp, saved = *disp_disp_env(disp_state.buffer);
    SpuCommonAttr attr = {0};
    DecDCTCallback prev_in, prev_out;
    uint32 *ring, *vlc, *slice, *src = NULL;
    StHEADER *header;
    sint32 result = 3, page = 0, skipping = 0;
    uint32 frames = 0;

    FUNCTION_MARKER(0x80062810, "MAIN.EXE");
    if (!movie || !movie->path || movie->width == 0 || movie->width > 512 || movie->height == 0 || movie->height > 240 || (movie->width & 15) != 0)
        return 3;
    if (snprintf(path, sizeof(path), "DATA%s", movie->path) < 0 || strlen(movie->path) + 4u >= sizeof(path))
        return 3;
    for (len = 0; path[len]; ++len)
        if (path[len] == '\\')
            path[len] = '/';
    if (strchr(path, ';'))
        *strchr(path, ';') = 0;
    if (!cd_mount_stream_file(path))
        return 3;
    CdIntToPos(0, &pos);
    ring = malloc(MOVIE_RING_SECTORS * 2048u);
    vlc = malloc(MOVIE_VLC_BYTES);
    slice = malloc((movie->rgb24 ? 48u : 32u) * (movie->height + 15 & ~15));
    if (!ring || !vlc || !slice)
    {
        free(slice);
        free(vlc);
        free(ring);
        return 3;
    }
    prev_in = DecDCTinCallback(NULL);
    prev_out = DecDCToutCallback(NULL);
    DecDCTReset(0);
    DecDCTvlcSize(0);
    StSetRing(ring, MOVIE_RING_SECTORS);
    movie_stream_end = 0;
    StSetStream(movie->rgb24, 5, movie->frames + 1, NULL, movie_end_stream);
    stream_set_read_mode(0xC0);
    ResetGraph(0);
    disp_clear_quadrants();
    attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX;
    attr.cd.volume.left = attr.cd.volume.right = movie->volume << 8;
    attr.cd.mix = SPU_ON;
    SpuSetCommonAttr(&attr);
    if (!CdControlB(CdlSeekL, (uint8 *)&pos, NULL) || !CdRead2(0xC0))
        goto stop;
    for (;;)
    {
        uint32 frame;
        result = movie_get_frame(&src, &header, skip, &skipping);
        if (result != 0)
            break;
        ++frames;
        frame = header->frameCount;
        if (header->width != movie->width || header->height != movie->height || frame < frames || frame > movie->frames)
        {
            result = 1;
            StFreeRing(src);
            src = NULL;
            break;
        }
        if (!movie_draw_frame(movie, src, vlc, slice, page))
        {
            result = 3;
            StFreeRing(src);
            src = NULL;
            break;
        }
        StFreeRing(src);
        src = NULL;
        if (frames == 1 && mode >= 5)
            sound_start_special_voice(12);
        input_update_states();
        if (!skipping && skip)
            skipping = skip() != 0;
        if (movie->frames - 20 < frame && (uint32)(mode - 1) < 4 || skipping)
            result = cd_adjust_audio_volume(1);
        SetDefDispEnv(&disp, 0, page * 256, movie->scr_width, 256);
        disp.screen.y = 18;
        disp.screen.h = 256;
        disp.isrgb24 = movie->rgb24 != 0;
        DrawSync(0);
        VSync(0);
        PutDispEnv(&disp);
        SetDispMask(1);
        gpu_present();
        page ^= 1;
        if (frame == movie->frames)
            result = 1;
        if (result != 0)
            break;
    }
stop:
    if (src)
        StFreeRing(src);
    StUnSetRing();
    StSetRing(NULL, 0u);
    CdControlB(CdlStop, NULL, NULL);
    DecDCTReset(1);
    DecDCTinCallback(prev_in);
    DecDCToutCallback(prev_out);
    cd_adjust_audio_volume(0);
    if (movie->width == 512 && movie->rgb24)
        disp_clear_quadrants();
    PutDispEnv(&saved);
    free(slice);
    free(vlc);
    free(ring);
    return result;
}

static sint32 movie_skip_is_requested(void)
{
    FUNCTION_MARKER(0x800627F8, "MAIN.EXE");
    return (input_pads[0].pressed & 0x840u) != 0;
}

sint32 movie_play(sint16 selection)
{
    SpuCommonAttr attr = {0};
    sint32 idx = selection - 1;

    FUNCTION_MARKER(0x80062718, "MAIN.EXE");
    input_pads[0].pressed = 0;
    input_update_states();
    // Preserve held buttons so skipping the logo does not create a new movie-skip edge
    input_pads[0].pressed = 0;
    if (idx >= sizeof(intro_movies) / sizeof(intro_movies[0]))
        return 0;
    movie_play_stream(&intro_movies[idx], movie_skip_is_requested, idx);
    if (idx >= 5)
        SpuSetKey(SPU_OFF, 0x1000u);
    attr.mask = 0xC0;
    attr.cd.volume.left = sound_mix.music << 9;
    attr.cd.volume.right = attr.cd.volume.left;
    SpuSetCommonAttr(&attr);
    if (idx < 5)
        return 0;
    VSync(0);
    SetDispMask(0);
    return 3;
}

static sint32 movie_show_image(SPRITE_IMAGE *state)
{
    PSX_RECT rect;

    FUNCTION_MARKER(0x80063750, "MAIN.EXE");
    menu_set_resource_prefix();
    menu_load_tex(state, "\\DATA\\");
    setRECT(&rect, 512, 0, 512, 240);
    MoveImage(&rect, 0, 0);
    MoveImage(&rect, 0, 240);
    VSync(0);
    DrawSync(0);
    SetDispMask(1);
    // Host display adapter after original 0x800637D0 display enable
    gpu_present();
    return 0;
}

sint32 movie_show_intro(void)
{
    sint16 counter;

    FUNCTION_MARKER(0x80063694, "MAIN.EXE");
    menu_state.cutscene = 0;
    if (game_selection.ready != 0)
        return game_selection.ready;
    movie_show_image(&movie_intro_image);
    counter = 150;
    while (counter != 0)
    {
        sint16 next = counter - 1;
        VSync(0);
        DrawSync(0);
        input_update_states();
        counter = next;
        if ((input_pads[0].cur & 0x40u) != 0 && 150 - next >= 3)
            counter = 0;
    }
    menu_state.cutscene = 1;
    return 1;
}
