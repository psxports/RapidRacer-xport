#include "mdec.h"
#include "cd.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "psx_gpu.h"
#include "psx_press.h"
#include "psx_stream.h"
#include "psx_spu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MDEC_RING_SECTORS 64u
#define MDEC_VLC_BYTES 132096u

static sint32 mdec_stream_end;

static void mdec_end_stream(void)
{
    mdec_stream_end = 1;
}

static sint32 mdec_get_frame(uint32 **src, StHEADER **header, sint32 (*skip)(void), sint32 *skipping)
{
    sint32 waits = 0;
    uint32 *record;

    while (StGetNext(src, &record) != 0u)
    {
        sint32 progress = stream_pump();
        if (mdec_stream_end)
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
    *header = (StHEADER *)record;
    return 0;
}

static sint32 mdec_draw_frame(const MDEC_MOVIE *movie, uint32 *src, uint32 *vlc, uint32 *slice, sint32 page)
{
    PSX_RECT rect;
    sint32 padded_h = (movie->height + 15) & ~15;
    sint32 slice_w = movie->rgb24 ? 24 : 16;
    sint32 words = slice_w * padded_h / 2;
    sint32 x;

    // Bound the SDK output command before decoding into the native VLC buffer
    if (((src[0] & 0xFFFFu) + 1u) * sizeof(uint32) > MDEC_VLC_BYTES)
        return 0;
    if (DecDCTvlc(src, vlc) != 0)
        return 0;
    DecDCTin(vlc, movie->rgb24 ? 3 : 2);
    setRECT(&rect, movie->rgb24 ? movie->x * 3 / 2 : movie->x,
            movie->y + 8 + page * 256, slice_w, padded_h);
    for (x = 0; x < movie->width; x += 16)
    {
        DecDCTout(slice, words);
        if (DecDCToutSync(1) != 0)
            return 0;
        LoadImagePSX(&rect, slice);
        rect.x += (sint16)slice_w;
    }
    return 1;
}

sint32 mdec_stream_play(const MDEC_MOVIE *movie, sint32 (*skip)(void), sint32 mode)
{
    CdlLOC pos;
    char path[256];
    size_t len;
    DISPENV disp, saved = *display_disp_env(display_state.buffer);
    SpuCommonAttr attr = {0};
    DecDCTCallback prev_in, prev_out;
    uint32 *ring, *vlc, *slice, *src = NULL;
    StHEADER *header;
    sint32 result = 3, page = 0, skipping = 0;
    uint32 frames = 0;

    FUNCTION_MARKER(0x80062810u, "MAIN.EXE");
    if (!movie || !movie->path || movie->width == 0 || movie->width > 512 ||
        movie->height == 0 || movie->height > 240 || (movie->width & 15) != 0)
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
    ring = malloc(MDEC_RING_SECTORS * 2048u);
    vlc = malloc(MDEC_VLC_BYTES);
    slice = malloc((movie->rgb24 ? 48u : 32u) * (uint32)((movie->height + 15) & ~15));
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
    StSetRing(ring, MDEC_RING_SECTORS);
    mdec_stream_end = 0;
    StSetStream(movie->rgb24, 5u, movie->frames + 1u, NULL, mdec_end_stream);
    stream_set_read_mode(0xC0u);
    ResetGraph(0);
    display_clear_quadrants();
    attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX;
    attr.cd.volume.left = attr.cd.volume.right = (sint16)(movie->volume << 8);
    attr.cd.mix = SPU_ON;
    SpuSetCommonAttr(&attr);
    if (!CdControlB(CdlSeekL, (uint8 *)&pos, NULL) || !CdRead2(0xC0))
        goto stop;
    for (;;)
    {
        uint32 frame;
        result = mdec_get_frame(&src, &header, skip, &skipping);
        if (result != 0)
            break;
        ++frames;
        frame = header->frameCount;
        if (header->width != movie->width || header->height != movie->height ||
            frame < frames || frame > movie->frames)
        {
            result = 1;
            StFreeRing(src);
            src = NULL;
            break;
        }
        if (!mdec_draw_frame(movie, src, vlc, slice, page))
        {
            result = 3;
            StFreeRing(src);
            src = NULL;
            break;
        }
        StFreeRing(src);
        src = NULL;
        if (frames == 1u && mode >= 5)
            sound_start_special_voice(12);
        input_update_states();
        if (!skipping && skip)
            skipping = skip() != 0;
        if ((movie->frames - 20u < frame && (uint32)(mode - 1) < 4u) || skipping)
            result = cd_adjust_audio_volume(1u);
        SetDefDispEnv(&disp, 0, page * 256, movie->screen_width, 256);
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
    cd_adjust_audio_volume(0u);
    if (movie->width == 512 && movie->rgb24)
        display_clear_quadrants();
    PutDispEnv(&saved);
    free(slice);
    free(vlc);
    free(ring);
    return result;
}
