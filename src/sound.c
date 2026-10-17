#include "timer.h"
#include "route.h"
#include "game.h"
#include "camera.h"
#include "input.h"
#include "cd.h"
#include "arena.h"
#include "sound.h"
#include "track.h"
#include "utils.h"
#include "boat.h"
#include "psx_spu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Options from MAIN.EXE 800E05CE/05D0/05D2/05D4
SOUND_OPTIONS sound_options;
SOUND_MIX sound_mix = {40, 63, 63};
SOUND_BANKS sound_banks;

SOUND_PLAYER sound_players[2];
static SOUND_VOICE sound_voices[24];
static const uint32 sound_voice_mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_NOTE | SPU_VOICE_SAMPLE_NOTE | SPU_VOICE_WDSA | SPU_VOICE_ADSR_AMODE | SPU_VOICE_ADSR_SMODE | SPU_VOICE_ADSR_RMODE | SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR | SPU_VOICE_ADSR_SR | SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL;

// Temporary context boundary until the remaining view users migrate
SOUND_PLAYER *sound_for_context(const RACE_VIEW *context)
{
    if (context == NULL || context->idx >= 2u)
        abort();
    return &sound_players[context->idx];
}

typedef struct
{
    SOUND_CFG cfg;
    uint16 vol_l, vol_r;
} SOUND_PRESET;

typedef struct
{
    SOUND_CFG cfg;
    uint32 loop;
} SOUND_RESERVED;

static const SOUND_RESERVED sound_reserved_cfg[8] = {
    {{62, 0, 1524, 0}, 1}, {{50, 4, 3072, 40}, 1}, {{60, 3, 1524, 10}, 0}, {{40, 2, 4096, 0}, 1}, {{54, 4, 768, 40}, 1}, {{60, 3, 1024, 10}, 0}, {{56, 4, 768, 40}, 1}, {{60, 3, 1024, 10}, 0},
};

static const SOUND_PRESET sound_presets[11] = {
    {{53, 1, 4096, 240}, 512, 512}, {{60, 5, 2400, 120}, 512, 512}, {{60, 6, 2200, 120}, 512, 512}, {{60, 8, 2048, 40}, 512, 512}, {{60, 9, 2048, 16}, 512, 512}, {{60, 10, 512, 8}, 512, 512}, {{60, 12, 3000, 60}, 512, 512}, {{60, 13, 1800, 60}, 512, 512}, {{54, 14, 600, 60}, 512, 512}, {{60, 16, 2400, 30}, 512, 512}, {{60, 15, 2400, 60}, 512, 512},
};

static const SOUND_CFG sound_fx_cfg[14] = {
    {60, 19, 7680, 120}, {60, 20, 8192, 120}, {60, 23, 7680, 120}, {60, 24, 7680, 120}, {60, 25, 7680, 120}, {60, 16, 5120, 120}, {60, 22, 5120, 120}, {60, 8, 5120, 120}, {60, 18, 5120, 120}, {60, 15, 5120, 120}, {60, 26, 5120, 120}, {60, 28, 5120, 60}, {60, 29, 7680, 60}, {60, 21, 7680, 60},
};

static const SOUND_CFG sound_speech_cfg[15] = {
    {60, 9, 5120, 120}, {60, 12, 5120, 120}, {60, 17, 5120, 120}, {60, 13, 5120, 120}, {60, 14, 5120, 120}, {60, 10, 5120, 120}, {60, 11, 5120, 120}, {60, 7, 5120, 120}, {60, 1, 5120, 120}, {60, 2, 5120, 120}, {60, 6, 5120, 120}, {60, 3, 5120, 120}, {60, 4, 5120, 120}, {60, 5, 5120, 120}, {60, 0, 5120, 120},
};

// VAB sample metadata and SPU RAM allocation offsets
#define SOUND_SAMPLE_MAX 128u

typedef struct
{
    uint8 note;
    uint16 addr_idx;
    uint16 adsr0, adsr1;
} SOUND_SAMPLE;

static uint32 sound_spu_allocs[2u * (128u + 1u)];

static SOUND_SAMPLE sound_samples[SOUND_SAMPLE_MAX];
static uint32 sound_allocs[SOUND_SAMPLE_MAX];
static uint16 sound_alloc_count;

static const SOUND_SAMPLE *sound_sample(sint32 idx)
{
    if (idx >= SOUND_SAMPLE_MAX)
        abort();
    return &sound_samples[idx];
}

static uint32 sound_sample_addr(const SOUND_SAMPLE *sample)
{
    sint16 idx = sample->addr_idx;

    if (idx >= SOUND_SAMPLE_MAX)
        abort();
    return sound_allocs[idx];
}

static void sound_set_adsr(SpuVoiceAttr *attr, uint16 adsr0, uint16 adsr1)
{
    attr->a_mode = (adsr0 & 0x8000u) != 0 ? 5 : 1;
    if ((adsr1 & 0xC000u) != 0)
        attr->s_mode = 7;
    else if ((adsr1 & 0x8000u) != 0)
        attr->s_mode = 5;
    else if ((adsr1 & 0x4000u) != 0)
        attr->s_mode = 3;
    else
        attr->s_mode = 1;
    attr->r_mode = (adsr1 & 0x20u) != 0 ? 7 : 3;
    attr->ar = adsr0 >> 8 & 0x7Fu;
    attr->dr = adsr0 >> 4 & 0xFu;
    attr->sr = adsr1 >> 6 & 0xFFu;
    attr->rr = adsr1 & 0x1Fu;
    attr->sl = adsr0 & 0xFu;
}

// Shared effect cooldowns and commentary history from MAIN.EXE
typedef struct
{
    uint16 ticks;
    sint8 link;
} SOUND_FX_LIMIT;

static const SOUND_FX_LIMIT sound_fx_limits[14] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {240, 0}, {480, 0}, {480, 1}, {480, -1}, {240, 0}, {0, 0}, {0, 0}, {120, 0}, {120, 0}};
static uint16 sound_fx_ticks[14];
static uint32 sound_speech_count[15];
static uint32 sound_prev_score;
static uint32 sound_countdown_active;

// Default choices from MAIN.EXE 80098D1C; runtime slots at 800E05BC and 80083740
const uint16 sound_default_tracks[8] = {2, 3, 4, 5, 6, 7, 8, 8};

// Track stepping from MAIN.EXE pause menu 80011540
void sound_step_track(sint32 dir)
{
    uint16 *track = &sound_options.tracks[sound_options.track_slot];

    *track = *track + dir;
    if (dir < 0 ? (sint16)*track < 2 : (sint16)*track >= 9)
        *track = dir < 0 ? 8u : 2u;
}

static sint32 sound_sra(uint32 val, uint32 shift)
{
    return val >> shift | ((val & 0x80000000) != 0 ? ~(~0u >> shift) : 0u);
}

static void sound_make_path(char *dst, const char *src, const char *ext)
{
    while (*src)
        *dst++ = *src++;
    while (*ext)
        *dst++ = *ext++;
    *dst = '\0';
}

static sint32 sound_start_sample_voice(const SOUND_VOICE *voice_state, sint32 octave, sint32 note, sint32 loop)
{
    SpuVoiceAttr attr = {0};
    uint32 mask;
    const SOUND_CFG *config;
    uint32 scale;
    uint32 product;
    uint32 voice_bit;
    sint32 sample;
    sint32 volume_left;
    sint32 volume_right;
    const SOUND_SAMPLE *cfg;

    FUNCTION_MARKER(0x80035FB0, "MAIN.EXE");
    mask = sound_voice_mask;
    config = voice_state->cfg;
    voice_bit = 1u << (voice_state->id & 31u);
    sample = (sint16)(config->sample + voice_state->bank);
    cfg = sound_sample(sample);
    if (sound_options.mono == 0)
    {
        sint16 left = voice_state->vol_l;

        scale = voice_state->gain;
        product = left * scale;
        volume_left = sound_sra(product, 11);
        product = (sint16)voice_state->vol_r * scale;
        volume_right = sound_sra(product, 11);
    }
    else
    {
        uint32 sum = (sint16)voice_state->vol_l;

        sum += (sint16)voice_state->vol_r;
        product = sum * voice_state->gain;
        volume_right = sound_sra(product, 12);
        volume_left = volume_right;
    }
    if (volume_left >= 0x4000)
        volume_left = 0x3FFF;
    if (volume_right >= 0x4000)
        volume_right = 0x3FFF;
    attr.voice = voice_bit;
    attr.mask = mask;
    attr.volume.right = volume_right;
    attr.volume.left = volume_left;
    attr.note = (uint32)octave << 8 | (uint32)note;
    attr.sample_note = cfg->note << 8;
    attr.addr = sound_sample_addr(cfg);
    sound_set_adsr(&attr, cfg->adsr0, cfg->adsr1);
    SpuSetVoiceAttr(&attr);
    voice_bit = 1u << (voice_state->id & 31u);
    SpuSetKey(SPU_ON, voice_bit);
    voice_bit = 1u << (voice_state->id & 31u);
    return SpuSetReverbVoice(loop & 1, voice_bit);
}

static void voice_stop_remove_rec(SOUND_PLAYER *state, SOUND_VOICE *rec)
{
    uint8 id = rec->id;
    SOUND_VOICE *last;

    FUNCTION_MARKER(0x80037DBC, "MAIN.EXE");
    SpuSetKey(SPU_OFF, 1u << (id & 31u));
    state->count--;
    last = state->events + state->count;
    if (last != rec)
    {
        *rec = *last;
        last->id = id;
    }
}

uint32 sound_queue_command(SOUND_PLAYER *state, sint32 sound, sint32 command)
{
    SOUND_PLAYER *queue = state;
    sint32 count;
    SOUND_VOICE *rec;
    const SOUND_CFG *config = NULL;
    uint32 result;

    FUNCTION_MARKER(0x800353FC, "MAIN.EXE");
    if (command == 4)
    {
        uint16 timer = game_timing.ticks;

        state->score = 0;
        sound_prev_score = 0;
        state->unused = 0;
        state->enabled = 1;
        state->last_tick = timer;
        return 1;
    }
    if (command == 5)
    {
        state->enabled = sound;
        return 4;
    }
    if (state->enabled == 0)
    {
        result = sound - 3u;
        if (command != 1)
            return result;
        result = result < 3;
        if (result == 0)
            return result;
    }
    count = queue->count;
    if (count == state->cap)
        return (uint32)count << 2;
    rec = state->events + count;
    if (command == 0)
    {
        sint32 age;
        sint32 duration;
        sint8 linked;
        uint16 timer;

        result = (uint32)sound << 1;
        if (sound_options.mode == 2 && sound != 11)
            return result;
        if ((uint32)sound >= 14)
            abort();
        config = &sound_fx_cfg[sound];
        timer = game_timing.ticks;
        age = timer - sound_fx_ticks[sound];
        if (age < 0)
        {
            sound_fx_ticks[sound] = timer;
            age = 0;
        }
        else
        {
            duration = (sint16)sound_fx_limits[sound].ticks;
            age -= duration;
            if (age < 0)
                return duration;
        }
        linked = sound_fx_limits[sound].link;
        timer = game_timing.ticks;
        sound_fx_ticks[sound] = timer;
        sound_fx_ticks[sound + linked] = timer;
        queue->last_tick = timer;
    }
    else if (command == 6)
    {
        SOUND_VOICE *pending;

        if (sound != 0 && sound != 5)
            return 6;
        pending = state->pending;
        if (pending == 0u)
            return 6;
        voice_stop_remove_rec(queue, pending);
        result = 0;
        state->pending = NULL;
        return result;
    }
    else if (command == 1)
    {
        sint32 pitch;
        sint32 master;

        config = &sound_presets[sound].cfg;
        if (sound == 0)
            state->pending = rec;
        if (sound == 5)
            state->pending = rec;
        if (sound == 9)
        {
            result = sound_countdown_active;
            if (result != 0)
                return result;
            sound_countdown_active = command;
        }
        rec->cfg = config;
        rec->kind = 8;
        rec->vol_l = sound_presets[sound].vol_l;
        rec->vol_r = sound_presets[sound].vol_r;
        rec->note = config->note;
        rec->bank = sound_banks.main;
        pitch = (sint16)config->gain;
        master = (sint16)sound_mix.effects;
        result = pitch * master;
        config = rec->cfg;
        rec->gain = result;
        sound_start_sample_voice(rec, config->octave, 0, 0);
        result = queue->count + 1u;
        queue->count = result;
        return result;
    }
    else if (command == 7)
    {
        result = (uint32)sound << 1;
        if (sound_options.mode == 2)
            return result;
        config = &sound_speech_cfg[sound];
    }
    else if (command == 2)
    {
        sint32 cur;

        result = sound + 1u;
        if (race_cfg.players == 2)
            return result;
        result = result < 3;
        if (result != 0)
        {
            result = state->score + sound;
            state->score = result;
            return result;
        }
        cur = (sint16)state->score;
        if (sound < 0 && cur > 0 || sound >= 0 && cur < 0)
        {
            result = (cur + sound) / 2;
            state->score = result;
            return result;
        }
        if (sound < 0)
            result = sound < cur;
        else
            result = cur < sound;
        if (result != 0)
            state->score = sound;
        return result;
    }
    else if (command == 3)
    {
        uint32 elapsed_raw;
        sint32 elapsed;
        sint32 delta;
        sint32 candidate;
        sint32 attempt;
        sint32 sel = 0;
        uint16 best = 32000;
        sint32 cur;
        uint32 *use_count;
        uint16 timer;
        result = (sint16)sound_options.mode;
        if (result != 0)
            return result;
        result = race_cfg.players;
        if (result == 2)
            return result;
        result = 8;
        if (race_cfg.mode == 8)
            return result;
        elapsed_raw = game_timing.ticks - state->last_tick;
        elapsed = (sint16)elapsed_raw;
        result = elapsed_raw & 63;
        if (elapsed < 64 || result == 0)
            return result;
        cur = (sint16)state->score;
        delta = cur - sound_prev_score;
        sound_prev_score = cur;
        if (delta < 0)
        {
            sint16 prev = queue->score;

            delta = delta - elapsed / 64;
            if (delta >= -63)
                return 0;
            if (prev < 0)
                delta = delta + prev;
            else
                delta = delta - prev;
        }
        else if (delta > 0)
        {
            sint16 prev = queue->score;

            delta = delta + elapsed / 64;
            if (delta < 64)
                return 1;
            if (prev > 0)
                delta = delta + prev;
            else
                delta = delta - prev;
        }
        delta /= 2;
        if (delta == 0)
            return 0;
        delta = clamp(delta, -128, 128);
        candidate = delta < 0 ? 6 * delta / 128 + 5 : 7 * delta / 128 + 6;
        for (attempt = 0; attempt < 3; ++attempt, ++candidate)
        {
            if (delta < 0 && (uint16)candidate >= 7u || delta >= 0 && (uint16)(candidate - 7) >= 8u)
                continue;
            use_count = &sound_speech_count[(sint16)candidate];
            if ((sint32)*use_count < (sint16)best)
            {
                best = *use_count;
                sel = candidate;
            }
        }
        result = (sint16)best;
        if (result == 32000)
            return result;
        sound = (sint16)sel;
        use_count = &sound_speech_count[sound];
        timer = game_timing.ticks;
        queue->score = 0;
        sound_prev_score = 0;
        queue->last_tick = timer;
        result = *use_count;
        config = &sound_speech_cfg[sound];
        *use_count = result + 1;
    }
    {
        sint32 pitch;
        sint32 master;

        rec->kind = 12;
        rec->cfg = config;
        rec->vol_r = 127;
        rec->vol_l = 127;
        rec->bank = sound_banks.speech;
        rec->note = config->note;
        pitch = (sint16)config->gain;
        master = (sint16)sound_mix.effects;
        config = rec->cfg;
        rec->gain = pitch * master;
        sound_start_sample_voice(rec, config->octave, 0, 0);
        result = queue->count + 1u;
        queue->count = result;
        return result;
    }
}

static void voice_set_note(const SOUND_VOICE *voice_state, sint16 octave, sint16 note)
{
    SpuVoiceAttr attr = {0};

    FUNCTION_MARKER(0x80035A5C, "MAIN.EXE");
    attr.voice = 1u << (voice_state->id & 31u);
    attr.mask = SPU_VOICE_NOTE;
    attr.note = (uint16)octave << 8 | (uint16)note;
    SpuSetVoiceAttr(&attr);
}

sint32 voice_update_volume(const SOUND_VOICE *voice_state)
{
    SpuVoiceAttr attr = {0};
    uint32 scale;
    const SOUND_VOICE *sel_voice;
    sint32 target_left;
    sint32 target_right;
    sint32 left;
    sint32 right;

    FUNCTION_MARKER(0x80035AA0, "MAIN.EXE");
    attr.voice = 1u << (voice_state->id & 31u);
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    SpuGetVoiceAttr(&attr);
    scale = voice_state->gain;
    target_left = (sint32)((sint16)voice_state->vol_l * scale) >> 11;
    target_right = (sint32)((sint16)voice_state->vol_r * scale) >> 11;
    left = attr.volume.left;
    right = attr.volume.right;
    if (target_left > 0x3FFF)
        target_left = 0x3FFF;
    if (target_right > 0x3FFF)
        target_right = 0x3FFF;
    if (race_cfg.players == 1)
        sel_voice = sound_players[0].reserved + 2;
    else
    {
        sel_voice = sound_players[0].reserved + 2;
        if (voice_state != sel_voice)
            sel_voice = sound_players[1].reserved + 2;
    }
    if (voice_state != sel_voice)
    {
        if (sound_options.mono == 0)
        {
            attr.volume.left = target_left;
            attr.volume.right = target_right;
        }
        else
        {
            sint32 avg = (target_left + target_right) / 2;

            attr.volume.right = avg;
            attr.volume.left = avg;
        }
        SpuSetVoiceAttr(&attr);
        return 0;
    }
    if (sound_options.mono == 0)
    {
        do
        {
            if (left < target_left)
            {
                left += 128;
                if (left > target_left)
                    left = target_left;
            }
            else if (left > target_left)
            {
                left -= 128;
                if (left < target_left)
                    left = target_left;
            }
            if (right < target_right)
            {
                right += 128;
                if (right > target_right)
                    right = target_right;
            }
            else if (right > target_right)
            {
                right -= 128;
                if (right < target_right)
                    right = target_right;
            }
            attr.volume.left = left;
            attr.volume.right = right;
            SpuSetVoiceAttr(&attr);
        } while (left != target_left || right != target_right);
    }
    else
    {
        sint32 target = (target_left + target_right) / 2;
        sint32 cur = (left + right) / 2;

        do
        {
            if (cur < target)
            {
                cur += 128;
                if (cur > target)
                    cur = target;
            }
            else if (cur > target)
            {
                cur -= 128;
                if (cur < target)
                    cur = target;
            }
            attr.volume.right = cur;
            attr.volume.left = cur;
            SpuSetVoiceAttr(&attr);
        } while (cur != target);
    }
    return 0;
}

void voice_set_volume(sint8 voice, sint16 volume)
{
    SpuVoiceAttr attr = {0};

    FUNCTION_MARKER(0x80035D00, "MAIN.EXE");
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    attr.volume.left = volume;
    attr.volume.right = volume;
    attr.voice = 1u << (voice & 31);
    SpuSetVoiceAttr(&attr);
}

sint32 sound_config_reverb_depth(uint16 left, uint16 right)
{
    SpuReverbAttr attr = {0};
    sint16 timer;
    sint32 scale;
    sint32 val;

    FUNCTION_MARKER(0x80035D3C, "MAIN.EXE");
    timer = sound_options.mono;
    attr.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    if (timer == 0)
    {
        scale = (sint16)sound_mix.effects;
        attr.depth.left = left * scale;
        attr.depth.right = right * scale;
    }
    else
    {
        val = left + right >> 1;
        scale = (sint16)sound_mix.effects;
        val *= scale;
        attr.depth.right = val;
        attr.depth.left = val;
    }
    return SpuSetReverbModeParam(&attr);
}

static sint32 voice_start(sint8 voice, sint32 bank, sint32 sample, sint16 octave, sint32 volume)
{
    SpuVoiceAttr attr = {0};
    uint32 voice_bit = 1u << (voice & 31);
    sint16 idx = bank + sample;
    const SOUND_SAMPLE *cfg = sound_sample(idx);

    FUNCTION_MARKER(0x80035DCC, "MAIN.EXE");
    attr.voice = voice_bit;
    attr.mask = sound_voice_mask;
    attr.volume.left = volume;
    attr.volume.right = volume;
    attr.note = (uint16)octave << 8;
    attr.sample_note = cfg->note << 8;
    attr.addr = sound_sample_addr(cfg);
    sound_set_adsr(&attr, cfg->adsr0, cfg->adsr1);
    SpuSetVoiceAttr(&attr);
    SpuSetKey(SPU_ON, voice_bit);
    return SpuSetReverbVoice(0, voice_bit);
}

static sint32 sound_update_impact(BOAT *boat, SOUND_VOICE *voice, SOUND_PLAYER *state)
{
    uint32 raw_accumulator;
    uint32 raw_pitch;
    uint32 raw_volume;
    const SOUND_CFG *note_table;
    sint32 accumulator;
    sint32 steering;
    sint32 pitch;
    sint32 result;

    FUNCTION_MARKER(0x80036A68, "MAIN.EXE");
    accumulator = sra(boat->impact.strength, 14u);
    raw_accumulator = accumulator + (sint16)state->impact / 2;
    state->impact = raw_accumulator;
    accumulator = (sint16)raw_accumulator;
    if (accumulator < 0)
        state->impact = 0;
    else if (accumulator >= 513)
        state->impact = 512;

    accumulator = (sint16)state->impact;
    if (accumulator < 9)
    {
        raw_pitch = state->pitch - 128u;
        state->pitch = raw_pitch;
        if ((raw_pitch & 0x8000) != 0)
            state->pitch = 0;
    }
    else
    {
        if ((game_selection.flags & 4) != 0 && accumulator >= 65)
            sound_queue_command(state, 12, 0);
        raw_pitch = state->pitch;
        raw_accumulator = state->impact;
        raw_pitch += raw_accumulator;
        state->pitch = raw_pitch;
        if ((sint16)raw_pitch >= 2049)
            state->pitch = 2048;
    }

    accumulator = (sint16)state->impact;
    steering = (sint16)state->speed;
    if (accumulator >= 9 && steering >= 201)
    {
        sint32 val = 200u - accumulator - steering;

        val = clamp(val, -127, 0);
        sound_queue_command(state, val, 2);
    }

    raw_pitch = state->pitch;
    pitch = (sint16)raw_pitch;
    if (pitch <= 0)
    {
        voice->vol_r = 0;
        voice->vol_l = 0;
    }
    else
    {
        sint32 octave;
        sint32 note;

        raw_volume = state->impact + 160u;
        note_table = voice->cfg;
        octave = note_table->octave;
        note = pitch / 2 & 0x7F;
        voice->vol_l = raw_volume;
        if ((sint16)raw_volume >= 257)
            voice->vol_l = 256;
        raw_volume = voice->vol_l;
        voice->vol_r = raw_volume;
        voice_set_note(voice, octave, note);
    }
    result = voice_update_volume(voice);
    boat->impact.strength = 0;
    return result;
}

static sint32 sound_update_engine_voices(SOUND_VOICE *voice0, const BOAT *boat, const SOUND_PLAYER *src)
{
    sint32 range;
    sint32 half_range;
    sint32 volume0;
    sint32 speed;
    sint32 volume1;
    sint32 pitch0;
    sint32 pitch1;
    sint32 src_val;
    sint32 product;
    uint32 raw_pitch;
    const SOUND_CFG *note_table;
    sint32 octave;
    SOUND_VOICE *voice1 = voice0 + 1;

    FUNCTION_MARKER(0x80036C68, "MAIN.EXE");
    range = (sint16)src->throttle;
    half_range = range / 2;
    volume0 = 256 - half_range;
    if (volume0 < 0)
        volume0 = 0;
    raw_pitch = src->speed;
    src_val = (sint16)src->contact[1];
    volume1 = (sint16)raw_pitch + half_range << 1;
    volume0 = volume0 + ((uint32)(sint16)src->ground_decay << 2);
    volume0 = volume0 + ((sint16)src->contact[0] + src_val << 1);
    src_val = (sint16)src->ground_acc;
    speed = -boat->contacts.points[0].height;
    if (speed < 64)
        speed = 0;
    volume1 = volume1 + ((uint32)src_val << 5);
    volume1 = volume1 + speed;
    if (volume1 > 1024)
        volume1 = 1024;
    if (volume0 > 1024)
        volume0 = 1024;
    src_val = (sint16)raw_pitch;
    src_val /= 2;
    pitch0 = src_val * range / 16;
    pitch0 = pitch0 + pitch0 / 2;
    if (pitch0 > 1536)
        pitch0 = 1536;
    note_table = voice0->cfg;
    octave = note_table->octave + sra(pitch0, 7u);
    voice_set_note(voice0, octave, pitch0 & 0x7F);
    voice0->vol_r = volume0;
    voice0->vol_l = volume0;
    voice_update_volume(voice0);
    product = (sint16)src->speed * ((sint16)src->throttle / 32);
    pitch1 = product + product / 2;
    if (pitch1 > 2048)
        pitch1 = 2048;
    if (speed > 0)
        pitch1 = pitch1 + 128u;
    note_table = voice1->cfg;
    octave = note_table->octave + sra(pitch1, 7u);
    voice_set_note(voice1, octave, pitch1 & 0x7F);
    voice1->vol_r = volume1;
    voice1->vol_l = volume1;
    return voice_update_volume(voice1);
}

static sint32 sound_update_steering_voice(SOUND_VOICE *voice, const BOAT *boat, SOUND_PLAYER *controls)
{
    sint32 depth0;
    sint32 depth1;
    uint32 combined;
    uint32 pan;
    sint32 cur;
    sint32 left_volume;
    sint32 val;

    FUNCTION_MARKER(0x80036E44, "MAIN.EXE");
    depth0 = (uint16)boat->contacts.points[1].penetration;
    depth1 = (uint16)boat->contacts.points[3].penetration;
    depth0 += depth1;
    if ((depth0 & 0x8000) != 0)
        depth0 = -depth0;
    if ((sint16)depth0 > 0)
    {
        val = controls->contact[0] + (sint16)depth0 / 2;
        controls->contact[0] = val;
        if ((sint16)controls->contact[0] >= 129)
            controls->contact[0] = 128;
    }
    else
    {
        cur = (sint16)controls->contact[0];
        if (cur >= 9)
            controls->contact[0] = cur - cur / 8;
        else if (cur > 0)
            controls->contact[0] = cur - 1;
    }

    depth1 = (uint16)boat->contacts.points[1].penetration;
    combined = (uint16)boat->contacts.points[4].penetration;
    depth1 += combined;
    if ((depth1 & 0x8000) != 0)
        depth1 = -depth1;
    if ((sint16)depth1 > 0)
    {
        val = controls->contact[1] + (sint16)depth1 / 2;
        controls->contact[1] = val;
        if ((sint16)controls->contact[1] >= 129)
            controls->contact[1] = 128;
    }
    else
    {
        cur = (sint16)controls->contact[1];
        if (cur >= 9)
            controls->contact[1] = cur - cur / 8;
        else if (cur > 0)
            controls->contact[1] = cur - 1;
    }

    combined = depth0 + depth1;
    cur = (sint16)controls->speed;
    if (cur >= 121 && (sint16)combined >= 17)
    {
        sint32 raw_val = -((sint16)combined - 16 << 1);

        raw_val = raw_val + 120 - cur;
        val = (sint16)raw_val;
        val = clamp(val, -127, 0);
        sound_queue_command(controls, val, 2);
    }
    if ((sint16)combined >= 33)
        combined = 32;
    val = voice->cfg->octave - (sint16)combined / 2;
    voice_set_note(voice, val, 0);

    cur = boat->contacts.points[1].height;
    if (cur <= 0)
    {
        pan = 0;
        controls->ground_acc = controls->ground_acc - cur / 4;
    }
    else
    {
        sint32 threshold;

        val = boat->velocity.speed;
        pan = (uint16)boat->contacts.points[1].weight - val / 250 + 64u;
        cur = (sint16)controls->speed;
        if (cur >= 181)
        {
            val = (sint16)controls->ground_acc;
            if (val >= 9)
            {
                val = (sint16)(val / 8 - 180u + cur);
                val = clamp(val, 0, 127);
                sound_queue_command(controls, val, 2);
            }
        }
        if ((sint16)controls->ground_acc >= 513)
            controls->ground_acc = 512;
        val = (sint16)controls->ground_acc;
        if (val > 0)
        {
            threshold = sra(val, 3u) + 30;
            cur = (sint16)controls->ground_decay;
            if (cur < threshold)
                controls->ground_decay = threshold;
        }
        else
        {
            uint32 raw_cur = controls->ground_decay;

            if ((sint16)raw_cur > 0)
            {
                cur = (sint16)raw_cur;
                val = raw_cur - 1 - sra(cur, 6u);
                controls->ground_decay = val;
            }
        }
        controls->ground_acc = 0;
    }

    voice->vol_l = pan + ((uint32)(sint16)controls->ground_decay << 2) + ((uint32)(sint16)controls->contact[0] << 5);
    cur = (sint16)controls->ground_decay;
    val = (sint16)controls->contact[1];
    left_volume = (sint16)voice->vol_l;
    voice->vol_r = pan + ((uint32)cur << 2) + ((uint32)val << 5);
    if (left_volume < 0)
        voice->vol_l = 0;
    if ((sint16)voice->vol_r < 0)
        voice->vol_r = 0;
    return voice_update_volume(voice);
}

static sint32 sound_fade_voices(uint32 mask, sint32 dec)
{
    SpuVoiceAttr attr = {0};
    sint32 stopped = 0;
    sint32 channels = 0;
    sint32 voice;

    if (race_cfg.players == 2)
        dec = (uint32)dec << 1;
    for (voice = 0; voice < 24; ++voice)
    {
        attr.voice = 1u << (uint32)voice;
        if ((mask & attr.voice) == 0)
            continue;
        channels += 2;
        SpuGetVoiceAttr(&attr);
        if (dec < attr.volume.left)
            attr.volume.left = attr.volume.left - dec;
        else
        {
            attr.volume.left = 0;
            ++stopped;
        }
        if (dec < attr.volume.right)
            attr.volume.right = attr.volume.right - dec;
        else
        {
            attr.volume.right = 0;
            ++stopped;
        }
        attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
        SpuSetVoiceAttr(&attr);
    }
    if (stopped < channels)
        sound_config_reverb_depth(0, 0);
    else
        SpuSetReverb(0);
    return stopped == channels;
}

static sint32 sound_fade_all(sint32 dec)
{
    FUNCTION_MARKER(0x80037C3C, "MAIN.EXE");
    return sound_fade_voices(SPU_ALLCH, dec);
}

static void sound_update_proximity(const BOAT *boat, SOUND_VOICE *voice0, uint16 *acc, uint16 *decay)
{
    sint32 attenuation;
    sint32 steering;
    sint32 boat_steering;
    sint32 speed;
    sint32 pitch;
    sint32 volume;
    sint32 pan;
    SOUND_VOICE *voice1;

    FUNCTION_MARKER(0x800382DC, "MAIN.EXE");
    if (boat == NULL)
        return;
    attenuation = boat->route.distance;
    attenuation = abs(attenuation);
    attenuation = 4096 - (attenuation >> 10);
    if (attenuation < 7)
        attenuation = 7;
    boat_steering = boat->contacts.points[0].height;
    steering = -boat_steering;
    if (steering < 0)
        steering = boat_steering;
    if (steering > 32)
        steering = 32;
    speed = (sint32)((uint32)boat->velocity.speed << 8) / 16000;
    pan = (uint16)boat->contacts.points[0].weight - speed / 4 + 64u;
    pitch = speed + speed / 2 + (attenuation >> 9) + ((uint32)steering << 4) + ((random_next() & 3u) << 4);
    voice_set_note(voice0, voice0->cfg->octave + (uint16)(pitch / 128), pitch & 0x7F);
    volume = (attenuation >> 3) + pitch / 64;
    voice0->vol_r = volume;
    voice0->vol_l = volume;
    voice_update_volume(voice0);
    voice1 = voice0 + 1;
    boat_steering = boat->contacts.points[0].height;
    if (boat_steering <= 0)
    {
        pan = 0;
        *acc -= boat_steering / 4;
    }
    else
    {
        sint32 accumulated;
        sint32 threshold;

        accumulated = (sint16)*acc;
        if (accumulated >= 513)
            *acc = 512;
        accumulated = (sint16)*acc;
        if (accumulated <= 0)
        {
            sint16 decay_val = *decay;

            if (decay_val > 0)
                *decay = decay_val - 1 - (decay_val >> 6);
        }
        else
        {
            sint32 decay_val;

            threshold = (accumulated >> 3) + 30;
            decay_val = (sint16)*decay;
            if (decay_val < threshold)
                *decay = threshold;
        }
        *acc = 0;
    }
    volume = ((sint16)*decay * 8 + (sint16)pan) * attenuation >> 12;
    if ((volume & 0x8000) != 0)
        volume = 0;
    voice1->vol_l = volume;
    voice1->vol_r = volume;
    voice_update_volume(voice1);
}

static uint32 sound_update_rival_voices(SOUND_PLAYER *audio, const sint32 pos[3])
{
    SOUND_VOICE *recs = audio->reserved + 4;
    sint32 count = boat_racer_count;
    sint32 idx;
    sint32 order;
    uint32 result = 0;

    FUNCTION_MARKER(0x800380C0, "MAIN.EXE");
    idx = 0;
    while (idx < count)
    {
        BOAT *vehicle = boat_racers[idx];
        uint32 state_x = pos[0];
        uint32 boat_x = vehicle->contacts.points[0].pos.x;
        uint32 boat_z = vehicle->contacts.points[0].pos.z;
        uint32 state_z = pos[2];
        sint32 x = state_x + boat_x;
        sint32 z = boat_z + state_z;
        vehicle->route.distance = (x * x + z * z) / 8;
        idx = (sint16)(idx + 1);
        count = boat_racer_count;
    }
    order = 1;
    while (order < boat_racer_count)
    {
        BOAT *prev = boat_racers[order - 1];
        BOAT *cur = boat_racers[order];
        sint32 prev_distance = prev->route.distance;
        sint32 cur_distance = cur->route.distance;

        if (prev_distance - cur_distance >= 0)
            ++order;
        else
        {
            boat_racers[order - 1] = cur;
            boat_racers[order] = prev;
            if (order >= 2)
                --order;
        }
    }
    for (idx = 0; idx < 2; ++idx)
    {
        SOUND_VOICE *voice = recs + idx * 2;
        BOAT *vehicle = boat_racers[idx];
        uint32 gain0 = voice->gain;
        uint32 gain1 = voice[1].gain;

        voice->gain = gain0 << 1;
        voice[1].gain = gain1 << 1;
        sound_update_proximity(vehicle, voice, &audio->rival_acc[idx], &audio->rival_decay[idx]);
        gain0 = voice->gain;
        gain1 = voice[1].gain;
        result = gain1 >> 31;
        voice->gain = (sint32)gain0 / 2;
        voice[1].gain = (sint32)gain1 / 2;
    }
    return result;
}

sint32 boat_update_audio(const RACE_VIEW *context)
{
    SOUND_PLAYER *state = sound_for_context(context);
    sint16 *reverb_idx = &state->reverb_idx;
    BOAT *boat = boat_player(context);
    PAD *input;
    SOUND_VOICE *voice;
    sint32 mode;
    sint32 val;
    sint32 changed;
    sint32 idx;

    FUNCTION_MARKER(0x800363E8, "MAIN.EXE");
    if (context == &race_views[0])
        cd_update_audio(1);
    if (boat == NULL)
        return 0;
    mode = boat_player_menu(context)->mode;
    if (mode == 2)
        return sound_update_rival_voices(state, camera_for_view(context)->pos);
    if ((uint32)(mode - 5) < 2)
        sound_fade_all(64);
    input = input_for_context(context);
    if ((input->cur & 0x40u) != 0)
    {
        sint32 target;

        state->speed = (sint32)((uint32)boat->velocity.speed << 8) / 16000;
        target = (uint16)boat->control.throttle;
        if (boat->control.mode == 1)
            target += 350;
        target = (sint16)target * 256 / 1400;
        state->throttle += (target - (sint16)state->throttle) / 4;
    }
    else
    {
        state->speed -= (sint16)state->speed / 32;
        state->throttle -= (sint16)state->throttle / 32;
    }

    if (camera_for_view(context)->dir == 2 && race_cfg.mode != 5 && camera_for_view(context)->mode != 5)
    {
        sound_queue_command(sound_for_context(context), -1, 2);
        sound_queue_command(sound_for_context(context), 5, 0);
    }
    if (race_cfg.mode != 9)
    {
        sint32 clock = race_time.ticks;

        if ((uint32)(clock - 801) < 99 || (uint32)(clock - 401) < 99)
            sound_queue_command(sound_for_context(context), (random_next() & 1) + 7, 0);
        if (race_cfg.mode != 4 && boat_player_menu(context)->mode == 4)
        {
            clock = race_time.ticks;
            sint32 hundreds = clock / 100;

            if ((sint16)state->countdown != (sint16)hundreds && clock - hundreds * 100 < 5)
            {
                state->countdown = hundreds;
                clock = race_time.ticks;
                if ((uint32)(clock - 91) < 1009)
                    sound_queue_command(sound_for_context(context), 9, 1);
            }
        }
    }

    val = (sint16)state->speed;
    if (val >= 201)
        sound_queue_command(sound_for_context(context), 1, 2);
    else if (val < 150)
        sound_queue_command(sound_for_context(context), 0, 2);
    sound_queue_command(sound_for_context(context), 1, 3);

    voice = state->reserved;
    sound_update_engine_voices(voice, boat, state);
    voice += 2;
    sound_update_steering_voice(voice, boat, state);
    ++voice;
    sound_update_impact(boat_player(context), voice, state);
    ++voice;
    if (race_cfg.players == 1)
    {
        for (idx = 0; idx < 2; ++idx)
        {
            sound_update_proximity(boat_tracks(context)[idx], voice, &state->rival_acc[idx], &state->rival_decay[idx]);
            voice += 2;
        }
    }

    changed = 0;
    if (state->reverb_mode == 10)
    {
        val = (sint16)state->reverb_l;
        if (val >= 17)
        {
            state->reverb_l = val - 1;
            changed = 1;
        }
        val = (sint16)state->reverb_r;
        if (val >= 17)
        {
            state->reverb_r = val - 1;
            changed = 1;
        }
    }
    else
    {
        val = (sint16)state->reverb_l;
        if (val < 228)
        {
            state->reverb_l = val + 1;
            changed = 1;
        }
        val = (sint16)state->reverb_r;
        if (val < 228)
        {
            state->reverb_r = val + 1;
            changed = 1;
        }
    }
    if (changed != 0)
    {
        sound_config_reverb_depth(state->reverb_l, state->reverb_r);
    }

    if (race_cfg.players == 1)
    {
        sint32 idx = *reverb_idx;

        if (track_reverb->entries != NULL)
        {
            const TRACK_REVERB *entry;
            sint32 divisor = route_resources.count;
            sint32 dividend = boat->race.progress;
            sint32 remainder;

            remainder = dividend % divisor;
            if (idx < 0 || idx >= track_reverb->count)
                abort();
            entry = &track_reverb->entries[idx];
            if ((sint16)remainder < entry->first)
                --idx;
            else if (entry->last < (sint16)remainder)
            {
                state->reverb_mode = 10;
                ++idx;
            }
            else
                state->reverb_mode = 9;
        }
        else
            state->reverb_mode = 10;
        if (idx == track_reverb->count)
            idx = 0;
        else if (idx < 0)
            idx = track_reverb->count - 1;
        *reverb_idx = idx;
    }
    sound_remove_inactive_voice_recs(state);
    return 1;
}

void sound_remove_inactive_voice_recs(SOUND_PLAYER *state)
{
    sint32 remaining = state->count;
    SOUND_VOICE *rec = state->events;

    FUNCTION_MARKER(0x80036990, "MAIN.EXE");
    while (remaining-- > 0)
    {
        uint8 type = rec->kind;

        if (type == 8 || type == 12)
        {
            uint32 voice = rec->id & 31u;
            uint32 voice_mask = 1u << voice;
            sint32 status = SpuGetKeyStatus(voice_mask);

            if (status != 1)
            {
                if (rec->cfg == &sound_presets[9].cfg)
                    sound_countdown_active = 0;
                voice_stop_remove_rec(state, rec);
            }
        }
        ++rec;
    }
}

sint32 sound_init_audio(void)
{
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    uint8 status;

    FUNCTION_MARKER(0x8003730C, "MAIN.EXE");
    SpuInit();

    SpuInitMalloc(128, (char *)sound_spu_allocs);

    common.mask = 0x2C3;
    common.mvol.left = 0x3FFF;
    common.mvol.right = 0x3FFF;
    common.cd.volume.left = 0x7FFE;
    common.cd.volume.right = 0x7FFE;
    common.cd.mix = 1;
    SpuSetCommonAttr(&common);

    SpuReserveReverbWorkArea(1);
    reverb.mask = 0x1F;
    reverb.mode = 4;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(0);
    memset(&reverb, 0, sizeof(reverb));
    reverb.mask = 6;
    SpuSetReverbModeParam(&reverb);
    SpuSetKey(SPU_OFF, SPU_ALLCH);

    status = 0;
    CdControl(1, 0, &status);
    if ((status & 0x10u) != 0 && CdDiskReady(1) != CdlComplete)
        while (CdDiskReady(0) != CdlComplete)
        {
        }

    cd_init_audio_toc();
    return 1;
}

static sint32 sound_upload_bank(const uint8 *header, uint32 header_size, const uint8 *body, uint32 body_size)
{
    uint32 prog_count, vag_count;
    uint32 base = sound_alloc_count;
    uint32 sizes_off, body_off = 0;
    uint32 idx;

    FUNCTION_MARKER(0x80037F3C, "MAIN.EXE");
    if (!header || header_size < 24 || !body && body_size != 0 || base > SOUND_SAMPLE_MAX)
        return -1;
    prog_count = header[18];
    vag_count = header[22];
    if (vag_count != 0 && !body)
        return -1;
    if (prog_count > SOUND_SAMPLE_MAX - base || vag_count > SOUND_SAMPLE_MAX - base)
        return -1;
    sizes_off = 2082 + 512 * prog_count;
    if ((prog_count != 0 || vag_count != 0) && (sizes_off > header_size || 2 * vag_count > header_size - sizes_off))
        return -1;
    for (idx = 0; idx < prog_count; ++idx)
    {
        uint16 vag = xport_load_le16(header + 2102u + 512 * idx);

        if (vag == 0 || vag > vag_count)
            return -1;
    }
    for (idx = 0; idx < vag_count; ++idx)
    {
        uint32 bytes = 8u * xport_load_le16(header + sizes_off + 2 * idx);

        if (bytes > body_size - body_off)
            return -1;
        body_off += bytes;
    }
    for (idx = 0; idx < prog_count; ++idx)
    {
        const uint8 *tone = header + 2080u + 512 * idx;
        SOUND_SAMPLE *sample = &sound_samples[base + idx];

        sample->note = tone[4];
        sample->addr_idx = xport_load_le16(tone + 22u) + base - 1;
        sample->adsr0 = xport_load_le16(tone + 16u);
        sample->adsr1 = xport_load_le16(tone + 18u);
    }
    SpuSetTransferMode(0);
    body_off = 0;
    for (idx = 0; idx < vag_count; ++idx)
    {
        uint32 bytes = 8u * xport_load_le16(header + sizes_off + 2 * idx);
        uint32 alloc = SpuMalloc(bytes);

        sound_allocs[base + idx] = alloc;
        SpuSetTransferStartAddr(alloc);
        SpuWrite(body + body_off, bytes);
        SpuIsTransferCompleted(1);
        body_off += bytes;
    }
    sound_alloc_count = base + vag_count;
    return base;
}

static sint32 sound_load_bank(const char *name)
{
    char path[16];
    const uint8 *header;
    const uint8 *body;
    uint32 header_size, body_size;
    sint32 result;

    FUNCTION_MARKER(0x80037428, "MAIN.EXE");
    arena_push();
    sound_make_path(path, name, ".VH");
    header = cd_load_file_bytes(path, &header_size);
    sound_make_path(path, name, ".VB");
    body = cd_load_file_bytes(path, &body_size);
    result = sound_upload_bank(header, header_size, body, body_size);
    arena_pop();
    return (sint16)result;
}

sint32 sound_load_bank_config(void)
{
    SpuCommonAttr attr = {0};
    sint32 result;

    FUNCTION_MARKER(0x800374A8, "MAIN.EXE");
    attr.mask = 0x2C3;
    attr.mvol.left = sound_mix.master << 8;
    attr.mvol.right = attr.mvol.left;
    attr.cd.volume.left = sound_mix.music << 9;
    attr.cd.volume.right = attr.cd.volume.left;
    attr.cd.mix = 1;
    sound_alloc_count = 0;
    SpuSetCommonAttr(&attr);
    SpuSetReverb(0);
    strcpy(cd_dir, "\\SOUND\\");
    result = sound_load_bank("MENU");
    sound_banks.main = result;
    return result;
}

sint32 sound_start_special_voice(sint32 voice)
{
    sint32 scale;

    FUNCTION_MARKER(0x8003755C, "MAIN.EXE");
    if (voice != 7 && voice != 12)
        return 12;
    scale = (sint8)sound_mix.music * 256;
    return voice_start(voice, (sint16)sound_banks.main, voice, 60, scale);
}

static sint32 sound_init_player_state(SOUND_PLAYER *state)
{
    SOUND_VOICE *recs;
    uint32 setup;
    sint32 count;
    sint32 idx = 0;
    sint32 result = 7;

    FUNCTION_MARKER(0x8003784C, "MAIN.EXE");
    state->reverb_r = 16;
    setup = state->reverb_r;
    state->pending = NULL;
    state->pitch = 0;
    state->impact = 0;
    state->reverb_l = 16;
    sound_config_reverb_depth(16, setup);
    state->reverb_mode = 10;
    recs = state->reserved;
    count = state->reserved_count;
    state->countdown = 30;
    state->sample6 = 6;
    state->count = 0;
    state->contact[1] = 0;
    state->contact[0] = 0;
    state->ground_decay = 0;
    state->ground_acc = 0;
    state->throttle = 0;
    state->speed = 0;
    state->rival_acc[1] = 0;
    state->rival_acc[0] = 0;
    state->rival_decay[1] = 0;
    state->rival_decay[0] = 0;
    state->score = 0;
    state->last_tick = 0;
    state->sample7 = 7;
    state->reverb_idx = 0;
    if (count <= 0)
        return result;
    do
    {
        SOUND_VOICE *rec = recs + idx;
        const SOUND_CFG *config = &sound_reserved_cfg[idx].cfg;
        sint16 sample;
        sint16 scale;

        rec->cfg = config;
        rec->kind = idx;
        rec->note = 0;
        rec->vol_r = 0;
        rec->vol_l = 0;
        rec->bank = sound_banks.main;
        config = rec->cfg;
        sample = config->gain;
        scale = sound_mix.effects;
        rec->gain = sample * scale;
        sound_start_sample_voice(rec, 60, 0, sound_reserved_cfg[idx].loop);
        count = state->reserved_count;
        idx++;
        result = idx < count;
    } while (result != 0);
    return result;
}

void sound_init_race_audio(void)
{
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    CdlATV mix;
    uint16 master_volume;
    uint16 cd_volume;
    sint32 idx;

    FUNCTION_MARKER(0x800375BC, "MAIN.EXE");
    common.mask = 0x2C3;
    common.cd.mix = 1;
    master_volume = sound_mix.master;
    cd_volume = sound_mix.music;
    sound_alloc_count = 0;
    sound_countdown_active = 0;
    common.mvol.left = master_volume << 8;
    common.mvol.right = common.mvol.left;
    common.cd.volume.left = cd_volume << 9;
    common.cd.volume.right = common.cd.volume.left;
    SpuSetCommonAttr(&common);
    reverb.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    reverb.depth.left = (uint32)(sint16)sound_mix.effects << 4;
    reverb.depth.right = reverb.depth.left;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(1);
    strcpy(cd_dir, "\\SOUND\\");
    sound_banks.main = sound_load_bank("JET5");
    sound_banks.speech = sound_load_bank("SPEAK");
    for (idx = 0; idx < 24; ++idx)
        sound_voices[idx].id = idx;
    sound_players[0].reserved = sound_voices;
    if (race_cfg.players == 1)
    {
        sound_players[0].reserved_count = 8;
        sound_players[0].events = sound_voices + 8;
        sound_players[0].cap = 16;
    }
    else
    {
        sound_players[0].reserved_count = 4;
        sound_players[0].events = sound_voices + 4;
        sound_players[0].cap = 8;
        sound_players[1].reserved_count = 4;
        sound_players[1].reserved = sound_voices + 12;
        sound_players[1].events = sound_voices + 16;
        sound_players[1].cap = 8;
    }
    memset(sound_speech_count, 0, sizeof(sound_speech_count));
    sound_init_player_state(&sound_players[0]);
    sound_queue_command(sound_for_context(&race_views[0]), 0, 4);
    if (race_cfg.players == 2)
    {
        sound_init_player_state(&sound_players[1]);
        sound_queue_command(sound_for_context(&race_views[1]), 0, 4);
    }
    if (sound_options.mono == 0)
    {
        mix.val2 = 0xFF;
        mix.val0 = 0xFF;
        mix.val3 = 0;
        mix.val1 = 0;
    }
    else
    {
        mix.val2 = 0x7F;
        mix.val0 = 0x7F;
        mix.val3 = 0x7F;
        mix.val1 = 0x7F;
    }
    CdMix(&mix);
    memset(sound_fx_ticks, 0, sizeof(sound_fx_ticks));
}

sint32 voice_start_scaled(sint32 bank, sint16 voice)
{
    sint16 scale = (sint16)sound_mix.effects * (voice == 8 ? 192 : 256);

    FUNCTION_MARKER(0x800379C0, "MAIN.EXE");
    return voice_start(voice, (sint16)bank, voice, 60, scale);
}

static void sound_scale_voices(SOUND_VOICE *voices, uint16 count)
{
    sint16 scale = sound_mix.effects;
    sint32 idx;

    for (idx = 0; idx < count; ++idx)
        voices[idx].gain = (sint16)voices[idx].cfg->gain * scale;
}

sint32 sound_scale_player_volumes(SOUND_PLAYER *p0, SOUND_PLAYER *p1)
{
    FUNCTION_MARKER(0x80037A44, "MAIN.EXE");
    sound_scale_voices(p0->reserved, p0->reserved_count);
    sound_scale_voices(p0->events, p0->count);
    if (race_cfg.players != 2)
        return 2;
    sound_scale_voices(p1->reserved, p1->reserved_count);
    sound_scale_voices(p1->events, p1->count);
    return p1->count <= 0 ? p1->count : 0;
}

void spu_decrease_master_volume(sint32 dec)
{
    SpuCommonAttr attr = {0};

    FUNCTION_MARKER(0x80037BB8, "MAIN.EXE");
    attr.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    SpuGetCommonAttr(&attr);
    if (dec < attr.mvol.left)
        attr.mvol.left = attr.mvol.left - dec;
    else
        attr.mvol.left = 0;
    if (dec < attr.mvol.right)
        attr.mvol.right = attr.mvol.right - dec;
    else
        attr.mvol.right = 0;
    SpuSetCommonAttr(&attr);
}

sint32 sound_fade_pause(sint32 dec)
{
    uint32 mask = SPU_ALLCH;
    sint32 players = race_cfg.players == 2 ? 2 : 1;
    sint32 player, idx;

    for (player = 0; player < players; ++player)
    {
        const SOUND_PLAYER *state = &sound_players[player];

        for (idx = 0; idx < state->count; ++idx)
        {
            const SOUND_VOICE *voice = &state->events[idx];

            // Pause UI sounds finish without the race fade changing their volume
            if (voice->kind == 8 && (voice->cfg == &sound_presets[3].cfg || voice->cfg == &sound_presets[4].cfg || voice->cfg == &sound_presets[5].cfg))
                mask &= ~(1u << (voice->id & 31u));
        }
    }
    return sound_fade_voices(mask, dec);
}

sint32 voice_restore_reverb(SOUND_PLAYER *state)
{
    sint32 count;
    SOUND_VOICE *recs;
    sint32 idx;

    FUNCTION_MARKER(0x80037D38, "MAIN.EXE");
    count = state->reserved_count;
    recs = state->reserved;
    if (count > 0)
    {
        idx = 0;
        do
        {
            voice_update_volume(recs);
            ++idx;
            count = state->reserved_count;
            ++recs;
        } while (idx < count);
    }
    state->reverb_r = 16;
    count = state->reverb_r;
    state->reverb_l = 16;
    sound_config_reverb_depth(16, count);
    return SpuSetReverb(1);
}

sint32 sound_reset_recs(SOUND_PLAYER *state)
{
    sint32 count;
    sint32 reserved_count;
    SOUND_VOICE *recs;
    SOUND_VOICE *reserved;
    sint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x80037E68, "MAIN.EXE");
    count = state->count;
    recs = state->events;
    for (idx = 0; idx < count; ++idx)
    {
        SOUND_VOICE *rec = recs + idx;
        rec->note = 0;
        rec->vol_r = 0;
        rec->vol_l = 0;
    }
    reserved = state->reserved;
    reserved_count = state->reserved_count;
    result = reserved_count;
    state->count = 0;
    for (idx = 0; idx < reserved_count;)
    {
        SOUND_VOICE *rec = reserved + idx;

        ++idx;
        rec->note = 0;
        rec->vol_r = 0;
        rec->vol_l = 0;
        reserved_count = state->reserved_count;
        result = idx < reserved_count;
    }
    return result;
}

sint32 sound_release_bank_allocs(void)
{
    sint32 count = (sint16)sound_alloc_count;
    sint32 idx;
    sint32 result = count;

    FUNCTION_MARKER(0x80037EDC, "MAIN.EXE");
    for (idx = 0; idx < count; ++idx)
    {
        uint32 allocation = sound_allocs[idx];
        SpuFree(allocation);
        count = (sint16)sound_alloc_count;
        result = idx + 1 < count;
    }
    sound_alloc_count = 0;
    return result;
}
