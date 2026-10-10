#include "timer.h"
#include "route.h"
#include "game.h"
#include "camera.h"
#include "input.h"
#include "cd.h"
#include "arena.h"
#include "runtime.h"
#include "sound.h"
#include "scene.h"
#include "global.h"
#include "vehicle.h"
#include "psx_spu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Options from MAIN.EXE 800E05CE/05D0/05D2/05D4
SOUND_OPTIONS sound_options;
SOUND_MIX sound_mix = {40u, 63u, 63u};
SOUND_BANKS sound_banks;

SOUND_PLAYER sound_players[2];
static SOUND_VOICE sound_voices[24];
static const uint32 sound_voice_mask =
    SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_NOTE | SPU_VOICE_SAMPLE_NOTE | SPU_VOICE_WDSA |
    SPU_VOICE_ADSR_AMODE | SPU_VOICE_ADSR_SMODE | SPU_VOICE_ADSR_RMODE |
    SPU_VOICE_ADSR_AR | SPU_VOICE_ADSR_DR | SPU_VOICE_ADSR_SR | SPU_VOICE_ADSR_RR | SPU_VOICE_ADSR_SL;

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
    {{62u, 0u, 1524u, 0u}, 1u},
    {{50u, 4u, 3072u, 40u}, 1u},
    {{60u, 3u, 1524u, 10u}, 0u},
    {{40u, 2u, 4096u, 0u}, 1u},
    {{54u, 4u, 768u, 40u}, 1u},
    {{60u, 3u, 1024u, 10u}, 0u},
    {{56u, 4u, 768u, 40u}, 1u},
    {{60u, 3u, 1024u, 10u}, 0u},
};

static const SOUND_PRESET sound_presets[11] = {
    {{53u, 1u, 4096u, 240u}, 512u, 512u},
    {{60u, 5u, 2400u, 120u}, 512u, 512u},
    {{60u, 6u, 2200u, 120u}, 512u, 512u},
    {{60u, 8u, 2048u, 40u}, 512u, 512u},
    {{60u, 9u, 2048u, 16u}, 512u, 512u},
    {{60u, 10u, 512u, 8u}, 512u, 512u},
    {{60u, 12u, 3000u, 60u}, 512u, 512u},
    {{60u, 13u, 1800u, 60u}, 512u, 512u},
    {{54u, 14u, 600u, 60u}, 512u, 512u},
    {{60u, 16u, 2400u, 30u}, 512u, 512u},
    {{60u, 15u, 2400u, 60u}, 512u, 512u},
};

static const SOUND_CFG sound_fx_cfg[14] = {
    {60u, 19u, 7680u, 120u},
    {60u, 20u, 8192u, 120u},
    {60u, 23u, 7680u, 120u},
    {60u, 24u, 7680u, 120u},
    {60u, 25u, 7680u, 120u},
    {60u, 16u, 5120u, 120u},
    {60u, 22u, 5120u, 120u},
    {60u, 8u, 5120u, 120u},
    {60u, 18u, 5120u, 120u},
    {60u, 15u, 5120u, 120u},
    {60u, 26u, 5120u, 120u},
    {60u, 28u, 5120u, 60u},
    {60u, 29u, 7680u, 60u},
    {60u, 21u, 7680u, 60u},
};

static const SOUND_CFG sound_speech_cfg[15] = {
    {60u, 9u, 5120u, 120u},
    {60u, 12u, 5120u, 120u},
    {60u, 17u, 5120u, 120u},
    {60u, 13u, 5120u, 120u},
    {60u, 14u, 5120u, 120u},
    {60u, 10u, 5120u, 120u},
    {60u, 11u, 5120u, 120u},
    {60u, 7u, 5120u, 120u},
    {60u, 1u, 5120u, 120u},
    {60u, 2u, 5120u, 120u},
    {60u, 6u, 5120u, 120u},
    {60u, 3u, 5120u, 120u},
    {60u, 4u, 5120u, 120u},
    {60u, 5u, 5120u, 120u},
    {60u, 0u, 5120u, 120u},
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
    if ((uint32)idx >= SOUND_SAMPLE_MAX)
        abort();
    return &sound_samples[idx];
}

static uint32 sound_sample_addr(const SOUND_SAMPLE *sample)
{
    sint32 idx = (sint16)sample->addr_idx;

    if ((uint32)idx >= SOUND_SAMPLE_MAX)
        abort();
    return sound_allocs[idx];
}

static void sound_set_adsr(SpuVoiceAttr *attr, uint16 adsr0, uint16 adsr1)
{
    attr->a_mode = (adsr0 & 0x8000u) != 0u ? 5 : 1;
    if ((adsr1 & 0xC000u) != 0u)
        attr->s_mode = 7;
    else if ((adsr1 & 0x8000u) != 0u)
        attr->s_mode = 5;
    else if ((adsr1 & 0x4000u) != 0u)
        attr->s_mode = 3;
    else
        attr->s_mode = 1;
    attr->r_mode = (adsr1 & 0x20u) != 0u ? 7 : 3;
    attr->ar = (uint16)((adsr0 >> 8) & 0x7Fu);
    attr->dr = (uint16)((adsr0 >> 4) & 0xFu);
    attr->sr = (uint16)((adsr1 >> 6) & 0xFFu);
    attr->rr = (uint16)(adsr1 & 0x1Fu);
    attr->sl = (uint16)(adsr0 & 0xFu);
}


// Shared effect cooldowns and commentary history from MAIN.EXE
typedef struct
{
    uint16 ticks;
    sint8 link;
} SOUND_FX_LIMIT;

static const SOUND_FX_LIMIT sound_fx_limits[14] = {
    {0u, 0}, {0u, 0}, {0u, 0}, {0u, 0}, {0u, 0},
    {240u, 0}, {480u, 0}, {480u, 1}, {480u, -1},
    {240u, 0}, {0u, 0}, {0u, 0}, {120u, 0}, {120u, 0}
};
static uint16 sound_fx_ticks[14];
static uint32 sound_speech_count[15];
static uint32 sound_prev_score;
static uint32 sound_countdown_active;

// Default choices from MAIN.EXE 80098D1C; runtime slots at 800E05BC and 80083740
const uint16 sound_default_tracks[8] = {2, 3, 4, 5, 6, 7, 8, 8};

// Track stepping from MAIN.EXE pause menu 80011540
void sound_step_track(sint32 direction)
{
    uint16 *track = &sound_options.tracks[sound_options.track_slot];

    *track = (uint16)((uint32)*track + (uint32)direction);
    if (direction < 0 ? (sint16)*track < 2 : (sint16)*track >= 9)
        *track = direction < 0 ? 8u : 2u;
}

static sint32 sound_sra(uint32 value, uint32 shift)
{
    return (sint32)((value >> shift) | ((value & 0x80000000u) != 0u ? ~((~0u) >> shift) : 0u));
}

static void sound_make_path(char *dst, const char *src, const char *ext)
{
    while (*src)
        *dst++ = *src++;
    while (*ext)
        *dst++ = *ext++;
    *dst = '\0';
}





uint32 sound_queue_command(SOUND_PLAYER *state, sint32 sound, sint32 command)
{
    SOUND_PLAYER *queue = state;
    sint32 count;
    SOUND_VOICE *record;
    const SOUND_CFG *config = NULL;
    uint32 result;

    FUNCTION_MARKER(0x800353FCu, "MAIN.EXE");
    if (command == 4)
    {
        uint16 timer = game_timing.ticks;

        state->score = 0u;
        sound_prev_score = 0u;
        state->unused = 0u;
        state->enabled = 1u;
        state->last_tick = timer;
        return 1u;
    }
    if (command == 5)
    {
        state->enabled = (uint16)sound;
        return 4u;
    }
    if ((sint16)state->enabled == 0)
    {
        result = (uint32)sound - 3u;
        if (command != 1)
            return result;
        result = result < 3u;
        if (result == 0u)
            return result;
    }
    count = (sint16)queue->count;
    if (count == (sint16)state->cap)
        return (uint32)count << 2;
    record = state->events + (uint32)count;
    if (command == 0)
    {
        sint32 age;
        sint32 duration;
        sint8 linked;
        uint16 timer;
        uint16 previous;

        result = (uint32)sound << 1;
        if ((sint16)sound_options.mode == 2 && sound != 11)
            return result;
        if ((uint32)sound >= 14u)
            abort();
        config = &sound_fx_cfg[sound];
        timer = game_timing.ticks;
        previous = sound_fx_ticks[sound];
        age = (sint32)((uint32)timer - (uint32)previous);
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
                return (uint32)duration;
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
            return 6u;
        pending = state->pending;
        if (pending == 0u)
            return 6u;
        voice_stop_remove_rec(queue, pending);
        result = 0u;
        state->pending = NULL;
        return result;
    }
    else if (command == 1)
    {
        sint32 pitch;
        sint32 master;

        config = &sound_presets[sound].cfg;
        if (sound == 0)
            state->pending = record;
        if (sound == 5)
            state->pending = record;
        if (sound == 9)
        {
            result = sound_countdown_active;
            if (result != 0u)
                return result;
            sound_countdown_active = (uint32)command;
        }
        record->cfg = config;
        record->kind = 8u;
        record->vol_l = sound_presets[sound].vol_l;
        record->vol_r = sound_presets[sound].vol_r;
        record->note = config->note;
        record->bank = sound_banks.main;
        pitch = (sint16)config->gain;
        master = (sint16)sound_mix.effects;
        result = (uint32)(pitch * master);
        config = record->cfg;
        record->gain = result;
        sound_start_sample_voice(record, config->octave, 0, 0);
        result = (uint32)queue->count + 1u;
        queue->count = (uint16)result;
        return result;
    }
    else if (command == 7)
    {
        result = (uint32)sound << 1;
        if (sound_options.mode == 2u)
            return result;
        config = &sound_speech_cfg[sound];
    }
    else if (command == 2)
    {
        sint32 current;

        result = (uint32)sound + 1u;
        if (race_cfg.players == 2u)
            return result;
        result = result < 3u;
        if (result != 0u)
        {
            result = (uint32)state->score + (uint32)sound;
            state->score = (uint16)result;
            return result;
        }
        current = (sint16)state->score;
        if ((sound < 0 && current > 0) || (sound >= 0 && current < 0))
        {
            sint32 sum = (sint32)((uint32)current + (uint32)sound);

            result = (uint32)(sum / 2);
            state->score = (uint16)result;
            return result;
        }
        if (sound < 0)
            result = sound < current;
        else
            result = current < sound;
        if (result != 0u)
            state->score = (uint16)sound;
        return result;
    }
    else if (command == 3)
    {
        uint32 elapsed_raw;
        sint32 elapsed;
        sint32 delta;
        sint32 candidate;
        sint32 attempt;
        sint32 selected = 0;
        uint16 best = 32000u;
        sint32 current;
        uint32 *use_count;
        uint16 timer;
        uint16 previous_timer;

        result = (uint32)(sint32)(sint16)sound_options.mode;
        if (result != 0u)
            return result;
        result = race_cfg.players;
        if (result == 2u)
            return result;
        result = 8u;
        if (race_cfg.mode == 8u)
            return result;
        timer = game_timing.ticks;
        previous_timer = state->last_tick;
        elapsed_raw = (uint32)timer - (uint32)previous_timer;
        elapsed = (sint16)elapsed_raw;
        result = elapsed_raw & 63u;
        if (elapsed < 64 || result == 0u)
            return result;
        current = (sint16)state->score;
        delta = (sint32)((uint32)current - sound_prev_score);
        sound_prev_score = (uint32)current;
        if (delta < 0)
        {
            sint32 previous = (sint16)queue->score;

            delta = (sint32)((uint32)delta - (uint32)(elapsed / 64));
            if (delta >= -63)
                return 0u;
            if (previous < 0)
                delta = (sint32)((uint32)delta + (uint32)previous);
            else
                delta = (sint32)((uint32)delta - (uint32)previous);
        }
        else if (delta > 0)
        {
            sint32 previous = (sint16)queue->score;

            delta = (sint32)((uint32)delta + (uint32)(elapsed / 64));
            if (delta < 64)
                return 1u;
            if (previous > 0)
                delta = (sint32)((uint32)delta + (uint32)previous);
            else
                delta = (sint32)((uint32)delta - (uint32)previous);
        }
        delta /= 2;
        if (delta == 0)
            return 0u;
        if (delta < -128)
            delta = -128;
        if (delta > 128)
            delta = 128;
        candidate = delta < 0 ? 6 * delta / 128 + 5 : 7 * delta / 128 + 6;
        for (attempt = 0; attempt < 3; ++attempt, ++candidate)
        {
            if ((delta < 0 && (uint16)candidate >= 7u) || (delta >= 0 && (uint16)(candidate - 7) >= 8u))
                continue;
            use_count = &sound_speech_count[(sint16)candidate];
            if ((sint32)*use_count < (sint16)best)
            {
                best = (uint16)*use_count;
                selected = candidate;
            }
        }
        result = (uint32)(sint32)(sint16)best;
        if (result == 32000u)
            return result;
        sound = (sint16)selected;
        use_count = &sound_speech_count[sound];
        timer = game_timing.ticks;
        queue->score = 0u;
        sound_prev_score = 0u;
        queue->last_tick = timer;
        result = *use_count;
        config = &sound_speech_cfg[sound];
        *use_count = result + 1u;
    }
    {
        sint32 pitch;
        sint32 master;

        record->kind = 12u;
        record->cfg = config;
        record->vol_r = 127u;
        record->vol_l = 127u;
        record->bank = sound_banks.speech;
        record->note = config->note;
        pitch = (sint16)config->gain;
        master = (sint16)sound_mix.effects;
        config = record->cfg;
        record->gain = (uint32)(pitch * master);
        sound_start_sample_voice(record, config->octave, 0, 0);
        result = (uint32)queue->count + 1u;
        queue->count = (uint16)result;
        return result;
    }
}

void voice_set_note(const SOUND_VOICE *voice_state, sint16 octave, sint16 note)
{
    SpuVoiceAttr attr = {0};

    FUNCTION_MARKER(0x80035A5Cu, "MAIN.EXE");
    attr.voice = 1u << (voice_state->id & 31u);
    attr.mask = SPU_VOICE_NOTE;
    attr.note = (uint16)(((uint16)octave << 8) | (uint16)note);
    SpuSetVoiceAttr(&attr);
}

sint32 voice_update_volume(const SOUND_VOICE *voice_state)
{
    SpuVoiceAttr attr = {0};
    uint32 scale;
    const SOUND_VOICE *selected_voice;
    sint32 target_left;
    sint32 target_right;
    sint32 left;
    sint32 right;

    FUNCTION_MARKER(0x80035AA0u, "MAIN.EXE");
    attr.voice = 1u << (voice_state->id & 31u);
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    SpuGetVoiceAttr(&attr);
    scale = voice_state->gain;
    target_left = (sint32)((uint32)(sint32)(sint16)voice_state->vol_l * scale) >> 11;
    target_right = (sint32)((uint32)(sint32)(sint16)voice_state->vol_r * scale) >> 11;
    left = attr.volume.left;
    right = attr.volume.right;
    if (target_left > 0x3FFF)
        target_left = 0x3FFF;
    if (target_right > 0x3FFF)
        target_right = 0x3FFF;
    if (race_cfg.players == 1u)
        selected_voice = sound_players[0].reserved + 2;
    else
    {
        selected_voice = sound_players[0].reserved + 2;
        if (voice_state != selected_voice)
            selected_voice = sound_players[1].reserved + 2;
    }
    if (voice_state != selected_voice)
    {
        if ((sint16)sound_options.mono == 0)
        {
            attr.volume.left = (sint16)target_left;
            attr.volume.right = (sint16)target_right;
        }
        else
        {
            sint32 average = (sint32)((uint32)target_left + (uint32)target_right) / 2;

            attr.volume.right = (sint16)average;
            attr.volume.left = (sint16)average;
        }
        SpuSetVoiceAttr(&attr);
        return 0;
    }
    if ((sint16)sound_options.mono == 0)
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
            attr.volume.left = (sint16)left;
            attr.volume.right = (sint16)right;
            SpuSetVoiceAttr(&attr);
        } while (left != target_left || right != target_right);
    }
    else
    {
        sint32 target = (sint32)((uint32)target_left + (uint32)target_right) / 2;
        sint32 current = (sint32)((uint32)left + (uint32)right) / 2;

        do
        {
            if (current < target)
            {
                current += 128;
                if (current > target)
                    current = target;
            }
            else if (current > target)
            {
                current -= 128;
                if (current < target)
                    current = target;
            }
            attr.volume.right = (sint16)current;
            attr.volume.left = (sint16)current;
            SpuSetVoiceAttr(&attr);
        } while (current != target);
    }
    return 0;
}

void voice_set_volume(sint8 voice, sint16 volume)
{
    SpuVoiceAttr attr = {0};

    FUNCTION_MARKER(0x80035D00u, "MAIN.EXE");
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    attr.volume.left = volume;
    attr.volume.right = volume;
    attr.voice = 1u << ((uint32)(uint8)voice & 31u);
    SpuSetVoiceAttr(&attr);
}

sint32 sound_config_reverb_depth(uint16 left, uint16 right)
{
    SpuReverbAttr attr = {0};
    sint16 timer;
    sint32 scale;
    sint32 value;

    FUNCTION_MARKER(0x80035D3Cu, "MAIN.EXE");
    timer = (sint16)sound_options.mono;
    attr.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    if (timer == 0)
    {
        scale = (sint16)sound_mix.effects;
        attr.depth.left = (sint16)((sint32)(uint32)left * scale);
        attr.depth.right = (sint16)((sint32)(uint32)right * scale);
    }
    else
    {
        value = (sint32)(((uint32)left + (uint32)right) >> 1);
        scale = (sint16)sound_mix.effects;
        value *= scale;
        attr.depth.right = (sint16)value;
        attr.depth.left = (sint16)value;
    }
    return SpuSetReverbModeParam(&attr);
}

sint32 voice_start(sint8 voice, sint32 bank, sint32 sample, sint16 octave, sint32 volume)
{
    SpuVoiceAttr attr = {0};
    uint32 voice_bit = 1u << ((uint32)(uint8)voice & 31u);
    sint16 index = (sint16)(uint16)((uint32)bank + (uint32)sample);
    const SOUND_SAMPLE *cfg = sound_sample(index);

    FUNCTION_MARKER(0x80035DCCu, "MAIN.EXE");
    attr.voice = voice_bit;
    attr.mask = sound_voice_mask;
    attr.volume.left = (sint16)(uint16)volume;
    attr.volume.right = (sint16)(uint16)volume;
    attr.note = (uint16)((uint32)(uint16)octave << 8);
    attr.sample_note = (uint16)((uint32)cfg->note << 8);
    attr.addr = sound_sample_addr(cfg);
    sound_set_adsr(&attr, cfg->adsr0, cfg->adsr1);
    SpuSetVoiceAttr(&attr);
    SpuSetKey(SPU_ON, voice_bit);
    return (sint32)SpuSetReverbVoice(0, voice_bit);
}

sint32 sound_start_sample_voice(const SOUND_VOICE *voice_state, sint32 octave, sint32 note, sint32 loop)
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

    FUNCTION_MARKER(0x80035FB0u, "MAIN.EXE");
    mask = sound_voice_mask;
    config = voice_state->cfg;
    voice_bit = 1u << (voice_state->id & 31u);
    sample = (sint16)(uint16)((uint32)config->sample + (uint32)voice_state->bank);
    cfg = sound_sample(sample);
    if ((sint16)sound_options.mono == 0)
    {
        sint16 left = (sint16)voice_state->vol_l;

        scale = voice_state->gain;
        product = (uint32)(sint32)left * scale;
        volume_left = sound_sra(product, 11u);
        product = (uint32)(sint32)(sint16)voice_state->vol_r * scale;
        volume_right = sound_sra(product, 11u);
    }
    else
    {
        uint32 sum = (uint32)(sint32)(sint16)voice_state->vol_l;

        sum += (uint32)(sint32)(sint16)voice_state->vol_r;
        scale = voice_state->gain;
        product = sum * scale;
        volume_right = sound_sra(product, 12u);
        volume_left = volume_right;
    }
    if (volume_left >= 0x4000)
        volume_left = 0x3FFF;
    if (volume_right >= 0x4000)
        volume_right = 0x3FFF;
    attr.voice = voice_bit;
    attr.mask = mask;
    attr.volume.right = (sint16)volume_right;
    attr.volume.left = (sint16)volume_left;
    attr.note = (uint16)(((uint32)octave << 8) | (uint32)note);
    attr.sample_note = (uint16)((uint32)cfg->note << 8);
    attr.addr = sound_sample_addr(cfg);
    sound_set_adsr(&attr, cfg->adsr0, cfg->adsr1);
    SpuSetVoiceAttr(&attr);
    voice_bit = 1u << (voice_state->id & 31u);
    SpuSetKey(SPU_ON, voice_bit);
    voice_bit = 1u << (voice_state->id & 31u);
    return (sint32)SpuSetReverbVoice(loop & 1, voice_bit);
}

sint32 vehicle_update_audio(const RACE_VIEW *context)
{
    SOUND_PLAYER *state = sound_for_context(context);
    sint16 *reverb_idx = &state->reverb_idx;
    BOAT *boat = vehicle_player(context);
    CONTROLLER_STATE *input;
    SOUND_VOICE *voice;
    sint32 mode;
    sint32 value;
    sint32 changed;
    sint32 index;

    FUNCTION_MARKER(0x800363E8u, "MAIN.EXE");
    if (context == &race_views[0])
        cd_update_audio(1);
    if (boat == NULL)
        return 0;
    mode = (sint32)vehicle_player_menu(context)->mode;
    if (mode == 2)
        return (sint32)sound_update_rival_voices(state, camera_for_view(context)->position);
    if ((uint32)(mode - 5) < 2u)
        sound_fade_all(64);
    input = input_for_context(context);
    if ((input->current & 0x40u) != 0u)
    {
        sint32 target;

        value = (sint32)((uint32)(uint32)boat->motion.velocity.speed << 8) / 16000;
        state->speed = (uint16)value;
        target = (uint16)boat->control.throttle;
        if ((uint32)boat->control.mode == 1u)
            target += 350;
        target = ((sint32)(sint16)(uint16)target * 256) / 1400;
        value = (sint16)state->throttle;
        state->throttle = (uint16)(value + (target - value) / 4);
    }
    else
    {
        value = (sint16)state->speed;
        state->speed = (uint16)(value - value / 32);
        value = (sint16)state->throttle;
        state->throttle = (uint16)(value - value / 32);
    }

    if ((sint16)camera_for_view(context)->direction == 2 && race_cfg.mode != 5u && camera_for_view(context)->mode != 5u)
    {
        sound_queue_command(sound_for_context(context), -1, 2);
        sound_queue_command(sound_for_context(context), 5, 0);
    }
    if (race_cfg.mode != 9u)
    {
        sint32 clock = (sint32)race_time.ticks;

        if ((uint32)(clock - 801) < 99u || (uint32)(clock - 401) < 99u)
            sound_queue_command(sound_for_context(context), (game_random_next() & 1) + 7, 0);
        if (race_cfg.mode != 4u && vehicle_player_menu(context)->mode == 4u)
        {
            clock = (sint32)race_time.ticks;
            sint32 hundreds = clock / 100;

            if ((sint16)state->countdown != (sint16)hundreds && clock - hundreds * 100 < 5)
            {
                state->countdown = (uint16)hundreds;
                clock = (sint32)race_time.ticks;
                if ((uint32)(clock - 91) < 1009u)
                    sound_queue_command(sound_for_context(context), 9, 1);
            }
        }
    }

    value = (sint16)state->speed;
    if (value >= 201)
        sound_queue_command(sound_for_context(context), 1, 2);
    else if (value < 150)
        sound_queue_command(sound_for_context(context), 0, 2);
    sound_queue_command(sound_for_context(context), 1, 3);

    voice = state->reserved;
    sound_update_engine_voices(voice, boat, state);
    voice += 2;
    sound_update_steering_voice(voice, boat, state);
    ++voice;
    sound_update_impact(vehicle_player(context), voice, state);
    ++voice;
    if (race_cfg.players == 1u)
    {
        for (index = 0; index < 2; ++index)
        {
            sound_update_proximity(vehicle_tracks(context)[index], voice, &state->rival_acc[index], &state->rival_decay[index]);
            voice += 2;
        }
    }

    changed = 0;
    if ((sint16)state->reverb_mode == 10)
    {
        value = (sint16)state->reverb_l;
        if (value >= 17)
        {
            state->reverb_l = (uint16)(value - 1);
            changed = 1;
        }
        value = (sint16)state->reverb_r;
        if (value >= 17)
        {
            state->reverb_r = (uint16)(value - 1);
            changed = 1;
        }
    }
    else
    {
        value = (sint16)state->reverb_l;
        if (value < 228)
        {
            state->reverb_l = (uint16)(value + 1);
            changed = 1;
        }
        value = (sint16)state->reverb_r;
        if (value < 228)
        {
            state->reverb_r = (uint16)(value + 1);
            changed = 1;
        }
    }
    if (changed != 0)
    {
        uint16 first = state->reverb_l;
        uint16 second = state->reverb_r;

        sound_config_reverb_depth(first, second);
    }

    if (race_cfg.players == 1u)
    {
        sint32 idx = *reverb_idx;

        if (scene_reverb->entries != NULL)
        {
            const SCENE_REVERB *entry;
            sint32 divisor = (sint32)route_resources.count;
            sint32 dividend = (sint32)(uint32)boat->race.progress;
            sint32 remainder;

            if (divisor == 0 || (dividend == (sint32)0x80000000u && divisor == -1))
                abort();
            remainder = dividend % divisor;
            if (idx < 0 || idx >= scene_reverb->count)
                abort();
            entry = &scene_reverb->entries[idx];
            if ((sint16)remainder < entry->first)
                --idx;
            else if (entry->last < (sint16)remainder)
            {
                state->reverb_mode = 10u;
                ++idx;
            }
            else
                state->reverb_mode = 9u;
        }
        else
            state->reverb_mode = 10u;
        if (idx == scene_reverb->count)
            idx = 0;
        else if (idx < 0)
            idx = scene_reverb->count - 1;
        *reverb_idx = (sint16)idx;
    }
    sound_remove_inactive_voice_recs(state);
    return 1;
}

void sound_remove_inactive_voice_recs(SOUND_PLAYER *state)
{
    sint32 remaining = (sint16)state->count;
    SOUND_VOICE *record = state->events;

    FUNCTION_MARKER(0x80036990u, "MAIN.EXE");
    while (remaining-- > 0)
    {
        uint8 type = record->kind;

        if (type == 8u || type == 12u)
        {
            uint32 voice = record->id & 31u;
            uint32 voice_mask = 1u << voice;
            sint32 status = SpuGetKeyStatus(voice_mask);

            if (status != 1)
            {
                if (record->cfg == &sound_presets[9].cfg)
                    sound_countdown_active = 0u;
                voice_stop_remove_rec(state, record);
            }
        }
        ++record;
    }
}

sint32 sound_update_impact(BOAT *boat, SOUND_VOICE *voice, SOUND_PLAYER *state)
{
    uint32 raw_accumulator;
    uint32 raw_pitch;
    uint32 raw_volume;
    const SOUND_CFG *note_table;
    sint32 accumulator;
    sint32 steering;
    sint32 pitch;
    sint32 result;

    FUNCTION_MARKER(0x80036A68u, "MAIN.EXE");
    accumulator = math_sra_s32((uint32)boat->impact.strength, 14u);
    raw_accumulator = (uint32)accumulator + (uint32)((sint16)state->impact / 2);
    state->impact = (uint16)raw_accumulator;
    accumulator = (sint16)(uint16)raw_accumulator;
    if (accumulator < 0)
        state->impact = 0u;
    else if (accumulator >= 513)
        state->impact = 512u;

    accumulator = (sint16)state->impact;
    if (accumulator < 9)
    {
        raw_pitch = (uint32)state->pitch - 128u;
        state->pitch = (uint16)raw_pitch;
        if ((raw_pitch & 0x8000u) != 0u)
            state->pitch = 0u;
    }
    else
    {
        if ((((uint32)game_selection.flags | ((uint32)game_selection.ready << 16) | ((uint32)game_selection.event << 24)) & 4u) != 0u && accumulator >= 65)
            sound_queue_command(state, 12, 0);
        raw_pitch = state->pitch;
        raw_accumulator = state->impact;
        raw_pitch += raw_accumulator;
        state->pitch = (uint16)raw_pitch;
        if ((sint16)(uint16)raw_pitch >= 2049)
            state->pitch = 2048u;
    }

    accumulator = (sint16)state->impact;
    steering = (sint16)state->speed;
    if (accumulator >= 9 && steering >= 201)
    {
        sint32 value = (sint32)(200u - (uint32)accumulator - (uint32)steering);

        if (value > 0)
            value = 0;
        else if (value < -127)
            value = -127;
        sound_queue_command(state, value, 2);
    }

    raw_pitch = state->pitch;
    pitch = (sint16)(uint16)raw_pitch;
    if (pitch <= 0)
    {
        voice->vol_r = 0u;
        voice->vol_l = 0u;
    }
    else
    {
        sint32 octave;
        sint32 note;

        raw_volume = (uint32)state->impact + 160u;
        note_table = voice->cfg;
        octave = note_table->octave;
        note = (pitch / 2) & 0x7F;
        voice->vol_l = (uint16)raw_volume;
        if ((sint16)(uint16)raw_volume >= 257)
            voice->vol_l = 256u;
        raw_volume = voice->vol_l;
        voice->vol_r = (uint16)raw_volume;
        voice_set_note(voice, (sint16)octave, (sint16)note);
    }
    result = voice_update_volume(voice);
    boat->impact.strength = 0;
    return result;
}

sint32 sound_update_engine_voices(SOUND_VOICE *first_voice, const BOAT *boat, const SOUND_PLAYER *src)
{
    sint32 range;
    sint32 half_range;
    sint32 first_volume;
    sint32 speed;
    sint32 second_volume;
    sint32 pitch;
    sint32 second_pitch;
    sint32 source_value;
    sint32 product;
    uint32 raw_pitch;
    const SOUND_CFG *note_table;
    sint32 octave;
    SOUND_VOICE *second_voice = first_voice + 1;

    FUNCTION_MARKER(0x80036C68u, "MAIN.EXE");
    range = (sint16)src->throttle;
    half_range = range / 2;
    first_volume = 256 - half_range;
    if (first_volume < 0)
        first_volume = 0;
    raw_pitch = src->speed;
    source_value = (sint16)src->contact[1];
    second_volume = (sint32)(((uint32)(sint32)(sint16)raw_pitch + (uint32)half_range) << 1);
    first_volume = (sint32)((uint32)first_volume + ((uint32)(sint32)(sint16)src->ground_decay << 2));
    first_volume = (sint32)((uint32)first_volume + ((uint32)((sint32)(sint16)src->contact[0] + (uint32)source_value) << 1));
    source_value = (sint16)src->ground_acc;
    speed = (sint32)(0u - (uint32)boat->contacts.points[0].height);
    if (speed < 64)
        speed = 0;
    second_volume = (sint32)((uint32)second_volume + ((uint32)source_value << 5));
    second_volume = (sint32)((uint32)second_volume + (uint32)speed);
    if (second_volume > 1024)
        second_volume = 1024;
    if (first_volume > 1024)
        first_volume = 1024;
    source_value = (sint16)raw_pitch;
    source_value /= 2;
    product = (sint32)((uint32)source_value * (uint32)range);
    pitch = product / 16;
    pitch = (sint32)((uint32)pitch + (uint32)(pitch / 2));
    if (pitch > 1536)
        pitch = 1536;
    note_table = first_voice->cfg;
    octave = (sint32)note_table->octave + math_sra_s32((uint32)pitch, 7u);
    voice_set_note(first_voice, (sint16)octave, (sint16)(pitch & 0x7F));
    first_voice->vol_r = (uint16)first_volume;
    first_voice->vol_l = (uint16)first_volume;
    voice_update_volume(first_voice);
    range = (sint16)src->throttle;
    source_value = (sint16)src->speed;
    product = (sint32)((uint32)source_value * (uint32)(range / 32));
    second_pitch = (sint32)((uint32)product + (uint32)(product / 2));
    if (second_pitch > 2048)
        second_pitch = 2048;
    if (speed > 0)
        second_pitch = (sint32)((uint32)second_pitch + 128u);
    note_table = second_voice->cfg;
    octave = (sint32)note_table->octave + math_sra_s32((uint32)second_pitch, 7u);
    voice_set_note(second_voice, (sint16)octave, (sint16)(second_pitch & 0x7F));
    second_voice->vol_r = (uint16)second_volume;
    second_voice->vol_l = (uint16)second_volume;
    return voice_update_volume(second_voice);
}

sint32 sound_update_steering_voice(SOUND_VOICE *voice, const BOAT *boat, SOUND_PLAYER *controls)
{
    uint32 first;
    uint32 second;
    uint32 combined;
    uint32 pan;
    sint32 current;
    sint32 left_volume;
    sint32 value;

    FUNCTION_MARKER(0x80036E44u, "MAIN.EXE");
    first = ((uint16)boat->contacts.points[1].penetration);
    second = ((uint16)boat->contacts.points[3].penetration);
    first += second;
    if ((first & 0x8000u) != 0u)
        first = 0u - first;
    if ((sint16)(uint16)first > 0)
    {
        value = (sint32)((uint32)controls->contact[0] + (uint32)((sint16)(uint16)first / 2));
        controls->contact[0] = (uint16)value;
        if ((sint16)controls->contact[0] >= 129)
            controls->contact[0] = 128u;
    }
    else
    {
        current = (sint16)controls->contact[0];
        if (current >= 9)
            controls->contact[0] = (uint16)(current - current / 8);
        else if (current > 0)
            controls->contact[0] = (uint16)(current - 1);
    }

    second = ((uint16)boat->contacts.points[1].penetration);
    combined = ((uint16)boat->contacts.points[4].penetration);
    second += combined;
    if ((second & 0x8000u) != 0u)
        second = 0u - second;
    if ((sint16)(uint16)second > 0)
    {
        value = (sint32)((uint32)controls->contact[1] + (uint32)((sint16)(uint16)second / 2));
        controls->contact[1] = (uint16)value;
        if ((sint16)controls->contact[1] >= 129)
            controls->contact[1] = 128u;
    }
    else
    {
        current = (sint16)controls->contact[1];
        if (current >= 9)
            controls->contact[1] = (uint16)(current - current / 8);
        else if (current > 0)
            controls->contact[1] = (uint16)(current - 1);
    }

    combined = first + second;
    current = (sint16)controls->speed;
    if (current >= 121 && (sint16)(uint16)combined >= 17)
    {
        uint32 raw_value = 0u - (((uint32)(sint32)(sint16)(uint16)combined - 16u) << 1u);

        raw_value = raw_value + 120u - (uint32)current;
        value = (sint16)(uint16)raw_value;
        if (value > 0)
            value = 0;
        else if (value < -127)
            value = -127;
        sound_queue_command(controls, value, 2);
    }
    if ((sint16)(uint16)combined >= 33)
        combined = 32;
    value = (sint32)voice->cfg->octave - (sint16)(uint16)combined / 2;
    voice_set_note(voice, (sint16)(uint16)value, 0);

    current = (sint32)(uint32)boat->contacts.points[1].height;
    if (current <= 0)
    {
        pan = 0;
        controls->ground_acc = (uint16)((uint32)controls->ground_acc - (uint32)(current / 4));
    }
    else
    {
        sint32 threshold;

        value = (sint32)(uint32)boat->motion.velocity.speed;
        pan = (uint32)(uint16)boat->contacts.points[1].diagonal[3] - (uint32)(value / 250) + 64u;
        current = (sint16)controls->speed;
        if (current >= 181)
        {
            value = (sint16)controls->ground_acc;
            if (value >= 9)
            {
                value = (sint16)(uint16)((uint32)(value / 8) - 180u + (uint32)current);
                if (value >= 128)
                    value = 127;
                else if (value < 0)
                    value = 0;
                sound_queue_command(controls, value, 2);
            }
        }
        if ((sint16)controls->ground_acc >= 513)
            controls->ground_acc = 512u;
        value = (sint16)controls->ground_acc;
        if (value > 0)
        {
            threshold = math_sra_s32((uint32)value, 3u) + 30;
            current = (sint16)controls->ground_decay;
            if (current < threshold)
                controls->ground_decay = (uint16)threshold;
        }
        else
        {
            uint32 raw_current = controls->ground_decay;

            if ((sint16)(uint16)raw_current > 0)
            {
                current = (sint16)(uint16)raw_current;
                value = (sint32)(raw_current - 1u - (uint32)math_sra_s32((uint32)current, 6u));
                controls->ground_decay = (uint16)value;
            }
        }
        controls->ground_acc = 0u;
    }

    current = (sint16)controls->ground_decay;
    value = (sint16)controls->contact[0];
    combined = pan + ((uint32)current << 2u) + ((uint32)value << 5u);
    voice->vol_l = (uint16)combined;
    current = (sint16)controls->ground_decay;
    value = (sint16)controls->contact[1];
    left_volume = (sint16)voice->vol_l;
    combined = pan + ((uint32)current << 2u) + ((uint32)value << 5u);
    voice->vol_r = (uint16)combined;
    if (left_volume < 0)
        voice->vol_l = 0u;
    if ((sint16)voice->vol_r < 0)
        voice->vol_r = 0u;
    return voice_update_volume(voice);
}

sint32 sound_init_audio(void)
{
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    uint8 status;

    FUNCTION_MARKER(0x8003730Cu, "MAIN.EXE");
    SpuInit();

    SpuInitMalloc(128, (char *)sound_spu_allocs);

    common.mask = 0x2C3u;
    common.mvol.left = 0x3FFF;
    common.mvol.right = 0x3FFF;
    common.cd.volume.left = 0x7FFE;
    common.cd.volume.right = 0x7FFE;
    common.cd.mix = 1;
    SpuSetCommonAttr(&common);

    SpuReserveReverbWorkArea(1);
    reverb.mask = 0x1Fu;
    reverb.mode = 4;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(0);
    memset(&reverb, 0, sizeof(reverb));
    reverb.mask = 6u;
    SpuSetReverbModeParam(&reverb);
    SpuSetKey(SPU_OFF, SPU_ALLCH);

    status = 0u;
    CdControl(1u, 0, &status);
    if ((status & 0x10u) != 0u && CdDiskReady(1) != CdlComplete)
        while (CdDiskReady(0) != CdlComplete)
        {
        }

    cd_init_audio_toc();
    return 1;
}

sint32 sound_load_bank(const char *name)
{
    char path[16];
    const uint8 *header;
    const uint8 *body;
    size_t header_size, body_size;
    sint32 result;

    FUNCTION_MARKER(0x80037428u, "MAIN.EXE");
    game_push_checkpoint();
    sound_make_path(path, name, ".VH");
    header = cd_load_file_bytes(path, &header_size);
    sound_make_path(path, name, ".VB");
    body = cd_load_file_bytes(path, &body_size);
    result = sound_upload_bank(header, header_size, body, body_size);
    game_pop_checkpoint();
    return (sint16)result;
}

sint32 sound_load_bank_config(void)
{
    SpuCommonAttr attr = {0};
    sint32 result;

    FUNCTION_MARKER(0x800374A8u, "MAIN.EXE");
    attr.mask = 0x2C3u;
    attr.mvol.left = (sint16)(uint16)(sound_mix.master << 8);
    attr.mvol.right = attr.mvol.left;
    attr.cd.volume.left = (sint16)(uint16)(sound_mix.music << 9);
    attr.cd.volume.right = attr.cd.volume.left;
    attr.cd.mix = 1;
    sound_alloc_count = 0u;
    SpuSetCommonAttr(&attr);
    SpuSetReverb(0);
    strcpy(cd_dir, "\\SOUND\\");
    result = sound_load_bank("MENU");
    sound_banks.main = (uint16)result;
    return result;
}

sint32 sound_start_special_voice(sint32 voice)
{
    sint32 scale;

    FUNCTION_MARKER(0x8003755Cu, "MAIN.EXE");
    if (voice != 7 && voice != 12)
        return 12;
    scale = (sint32)(sint8)(uint8)sound_mix.music * 256;
    return voice_start((sint8)voice, (sint16)sound_banks.main, voice, 60, scale);
}

void sound_init_race_audio(void)
{
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    CdlATV mix;
    uint16 master_volume;
    uint16 cd_volume;
    sint32 index;

    FUNCTION_MARKER(0x800375BCu, "MAIN.EXE");
    common.mask = 0x2C3u;
    common.cd.mix = 1;
    master_volume = sound_mix.master;
    cd_volume = sound_mix.music;
    sound_alloc_count = 0u;
    sound_countdown_active = 0u;
    common.mvol.left = (sint16)(uint16)(master_volume << 8);
    common.mvol.right = common.mvol.left;
    common.cd.volume.left = (sint16)(uint16)(cd_volume << 9);
    common.cd.volume.right = common.cd.volume.left;
    SpuSetCommonAttr(&common);
    reverb.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    reverb.depth.left = (sint16)(uint16)((uint32)(sint32)(sint16)sound_mix.effects << 4);
    reverb.depth.right = reverb.depth.left;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(1);
    strcpy(cd_dir, "\\SOUND\\");
    sound_banks.main = (uint16)sound_load_bank("JET5");
    sound_banks.speech = (uint16)sound_load_bank("SPEAK");
    for (index = 0; index < 24; ++index)
        sound_voices[index].id = (uint8)index;
    sound_players[0].reserved = sound_voices;
    if (race_cfg.players == 1u)
    {
        sound_players[0].reserved_count = 8u;
        sound_players[0].events = sound_voices + 8;
        sound_players[0].cap = 16u;
    }
    else
    {
        sound_players[0].reserved_count = 4u;
        sound_players[0].events = sound_voices + 4;
        sound_players[0].cap = 8u;
        sound_players[1].reserved_count = 4u;
        sound_players[1].reserved = sound_voices + 12;
        sound_players[1].events = sound_voices + 16;
        sound_players[1].cap = 8u;
    }
    memset(sound_speech_count, 0, sizeof(sound_speech_count));
    sound_init_player_state(&sound_players[0]);
    sound_queue_command(sound_for_context(&race_views[0]), 0, 4);
    if (race_cfg.players == 2u)
    {
        sound_init_player_state(&sound_players[1]);
        sound_queue_command(sound_for_context(&race_views[1]), 0, 4);
    }
    if ((sint16)sound_options.mono == 0)
    {
        mix.val2 = 0xFFu;
        mix.val0 = 0xFFu;
        mix.val3 = 0u;
        mix.val1 = 0u;
    }
    else
    {
        mix.val2 = 0x7Fu;
        mix.val0 = 0x7Fu;
        mix.val3 = 0x7Fu;
        mix.val1 = 0x7Fu;
    }
    CdMix(&mix);
    memset(sound_fx_ticks, 0, sizeof(sound_fx_ticks));
}

sint32 sound_init_player_state(SOUND_PLAYER *state)
{
    SOUND_VOICE *records;
    uint32 setup;
    sint32 count;
    sint32 index = 0;
    sint32 result = 7;

    FUNCTION_MARKER(0x8003784Cu, "MAIN.EXE");
    state->reverb_r = 16u;
    setup = state->reverb_r;
    state->pending = NULL;
    state->pitch = 0u;
    state->impact = 0u;
    state->reverb_l = 16u;
    sound_config_reverb_depth(16u, (uint16)setup);
    state->reverb_mode = 10u;
    records = state->reserved;
    count = (sint16)state->reserved_count;
    state->countdown = 30u;
    state->sample6 = 6u;
    state->count = 0u;
    state->contact[1] = 0u;
    state->contact[0] = 0u;
    state->ground_decay = 0u;
    state->ground_acc = 0u;
    state->throttle = 0u;
    state->speed = 0u;
    state->rival_acc[1] = 0u;
    state->rival_acc[0] = 0u;
    state->rival_decay[1] = 0u;
    state->rival_decay[0] = 0u;
    state->score = 0u;
    state->last_tick = 0u;
    state->sample7 = 7u;
    state->reverb_idx = 0;
    if (count <= 0)
        return result;
    do
    {
        SOUND_VOICE *record = records + index;
        const SOUND_CFG *config = &sound_reserved_cfg[index].cfg;
        sint16 sample;
        sint16 scale;

        record->cfg = config;
        record->kind = (uint8)index;
        record->note = 0u;
        record->vol_r = 0u;
        record->vol_l = 0u;
        record->bank = sound_banks.main;
        config = record->cfg;
        sample = (sint16)config->gain;
        scale = (sint16)sound_mix.effects;
        record->gain = (uint32)((sint32)sample * (sint32)scale);
        sound_start_sample_voice(record, 60, 0, (sint32)sound_reserved_cfg[index].loop);
        count = (sint16)state->reserved_count;
        index = (sint32)((uint32)index + 1u);
        result = index < count;
    } while (result != 0);
    return result;
}

sint32 voice_start_scaled(sint32 bank, sint16 voice)
{
    sint32 scale = (sint16)((sint16)sound_mix.effects * (voice == 8 ? 192 : 256));

    FUNCTION_MARKER(0x800379C0u, "MAIN.EXE");
    return voice_start((sint8)voice, (sint16)bank, voice, 60, scale);
}

static void sound_scale_voices(SOUND_VOICE *voices, uint16 count)
{
    sint32 scale = (sint16)sound_mix.effects;
    sint32 idx;

    for (idx = 0; idx < (sint16)count; ++idx)
        voices[idx].gain = (uint32)((sint16)voices[idx].cfg->gain * scale);
}

sint32 sound_scale_player_volumes(SOUND_PLAYER *p0, SOUND_PLAYER *p1)
{
    FUNCTION_MARKER(0x80037A44u, "MAIN.EXE");
    sound_scale_voices(p0->reserved, p0->reserved_count);
    sound_scale_voices(p0->events, p0->count);
    if (race_cfg.players != 2u)
        return 2;
    sound_scale_voices(p1->reserved, p1->reserved_count);
    sound_scale_voices(p1->events, p1->count);
    return (sint16)p1->count <= 0 ? (sint16)p1->count : 0;
}

void spu_decrease_master_volume(sint32 decrement)
{
    SpuCommonAttr attr = {0};

    FUNCTION_MARKER(0x80037BB8u, "MAIN.EXE");
    attr.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    SpuGetCommonAttr(&attr);
    if (decrement < attr.mvol.left)
        attr.mvol.left = (sint16)((uint16)attr.mvol.left - (uint32)decrement);
    else
        attr.mvol.left = 0;
    if (decrement < attr.mvol.right)
        attr.mvol.right = (sint16)((uint16)attr.mvol.right - (uint32)decrement);
    else
        attr.mvol.right = 0;
    SpuSetCommonAttr(&attr);
}

static sint32 sound_fade_voices(uint32 mask, sint32 dec)
{
    SpuVoiceAttr attr = {0};
    sint32 stopped = 0;
    sint32 channels = 0;
    sint32 voice;

    if (race_cfg.players == 2u)
        dec = (sint32)((uint32)dec << 1);
    for (voice = 0; voice < 24; ++voice)
    {
        attr.voice = 1u << (uint32)voice;
        if ((mask & attr.voice) == 0u)
            continue;
        channels += 2;
        SpuGetVoiceAttr(&attr);
        if (dec < attr.volume.left)
            attr.volume.left = (sint16)((uint16)attr.volume.left - (uint32)dec);
        else
        {
            attr.volume.left = 0;
            ++stopped;
        }
        if (dec < attr.volume.right)
            attr.volume.right = (sint16)((uint16)attr.volume.right - (uint32)dec);
        else
        {
            attr.volume.right = 0;
            ++stopped;
        }
        attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
        SpuSetVoiceAttr(&attr);
    }
    if (stopped < channels)
        sound_config_reverb_depth(0u, 0u);
    else
        SpuSetReverb(0);
    return stopped == channels;
}

sint32 sound_fade_all(sint32 dec)
{
    FUNCTION_MARKER(0x80037C3Cu, "MAIN.EXE");
    return sound_fade_voices(SPU_ALLCH, dec);
}

sint32 sound_fade_pause(sint32 dec)
{
    uint32 mask = SPU_ALLCH;
    sint32 players = race_cfg.players == 2u ? 2 : 1;
    sint32 player, idx;

    for (player = 0; player < players; ++player)
    {
        const SOUND_PLAYER *state = &sound_players[player];

        for (idx = 0; idx < (sint16)state->count; ++idx)
        {
            const SOUND_VOICE *voice = &state->events[idx];

            // Pause UI sounds finish without the race fade changing their volume
            if (voice->kind == 8u && (voice->cfg == &sound_presets[3].cfg ||
                voice->cfg == &sound_presets[4].cfg || voice->cfg == &sound_presets[5].cfg))
                mask &= ~(1u << (voice->id & 31u));
        }
    }
    return sound_fade_voices(mask, dec);
}

sint32 voice_restore_reverb(SOUND_PLAYER *state)
{
    sint32 count;
    SOUND_VOICE *records;
    sint32 index;

    FUNCTION_MARKER(0x80037D38u, "MAIN.EXE");
    count = (sint16)state->reserved_count;
    records = state->reserved;
    if (count > 0)
    {
        index = 0;
        do
        {
            voice_update_volume(records);
            ++index;
            count = (sint16)state->reserved_count;
            ++records;
        } while (index < count);
    }
    state->reverb_r = 16u;
    count = state->reverb_r;
    state->reverb_l = 16u;
    sound_config_reverb_depth(16u, (uint16)count);
    return SpuSetReverb(1);
}

void voice_stop_remove_rec(SOUND_PLAYER *state, SOUND_VOICE *record)
{
    uint8 id = record->id;
    SOUND_VOICE *last;

    FUNCTION_MARKER(0x80037DBCu, "MAIN.EXE");
    SpuSetKey(SPU_OFF, 1u << (id & 31u));
    state->count = (uint16)((sint16)state->count - 1);
    last = state->events + (sint16)state->count;
    if (last != record)
    {
        *record = *last;
        last->id = id;
    }
}

sint32 sound_reset_recs(SOUND_PLAYER *state)
{
    sint32 count;
    sint32 reserved_count;
    SOUND_VOICE *records;
    SOUND_VOICE *reserved;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80037E68u, "MAIN.EXE");
    count = (sint16)state->count;
    records = state->events;
    for (index = 0; index < count; ++index)
    {
        SOUND_VOICE *record = records + index;
        record->note = 0u;
        record->vol_r = 0u;
        record->vol_l = 0u;
    }
    reserved = state->reserved;
    reserved_count = (sint16)state->reserved_count;
    result = reserved_count;
    state->count = 0u;
    for (index = 0; index < reserved_count;)
    {
        SOUND_VOICE *record = reserved + index;

        ++index;
        record->note = 0u;
        record->vol_r = 0u;
        record->vol_l = 0u;
        reserved_count = (sint16)state->reserved_count;
        result = index < reserved_count;
    }
    return result;
}

sint32 sound_release_bank_allocs(void)
{
    sint32 count = (sint16)sound_alloc_count;
    sint32 index;
    sint32 result = count;

    FUNCTION_MARKER(0x80037EDCu, "MAIN.EXE");
    for (index = 0; index < count; ++index)
    {
        uint32 allocation = sound_allocs[index];
        SpuFree(allocation);
        count = (sint16)sound_alloc_count;
        result = index + 1 < count;
    }
    sound_alloc_count = 0u;
    return result;
}

sint32 sound_upload_bank(const uint8 *header, size_t header_size, const uint8 *body, size_t body_size)
{
    uint32 prog_count, vag_count;
    uint32 base = sound_alloc_count;
    size_t sizes_off, body_off = 0u;
    uint32 idx;

    FUNCTION_MARKER(0x80037F3Cu, "MAIN.EXE");
    if (!header || header_size < 24u || (!body && body_size != 0u) || base > SOUND_SAMPLE_MAX)
        return -1;
    prog_count = header[18];
    vag_count = header[22];
    if (vag_count != 0u && !body)
        return -1;
    if (prog_count > SOUND_SAMPLE_MAX - base || vag_count > SOUND_SAMPLE_MAX - base)
        return -1;
    sizes_off = 2082u + 512u * prog_count;
    if ((prog_count != 0u || vag_count != 0u) && (sizes_off > header_size || 2u * vag_count > header_size - sizes_off))
        return -1;
    for (idx = 0u; idx < prog_count; ++idx)
    {
        uint16 vag = xport_load_le16(header + 2102u + 512u * idx);

        if (vag == 0u || vag > vag_count)
            return -1;
    }
    for (idx = 0u; idx < vag_count; ++idx)
    {
        size_t bytes = 8u * xport_load_le16(header + sizes_off + 2u * idx);

        if (bytes > body_size - body_off)
            return -1;
        body_off += bytes;
    }
    for (idx = 0u; idx < prog_count; ++idx)
    {
        const uint8 *tone = header + 2080u + 512u * idx;
        SOUND_SAMPLE *sample = &sound_samples[base + idx];

        sample->note = tone[4];
        sample->addr_idx = (uint16)(xport_load_le16(tone + 22u) + base - 1u);
        sample->adsr0 = xport_load_le16(tone + 16u);
        sample->adsr1 = xport_load_le16(tone + 18u);
    }
    SpuSetTransferMode(0);
    body_off = 0u;
    for (idx = 0u; idx < vag_count; ++idx)
    {
        uint32 bytes = 8u * xport_load_le16(header + sizes_off + 2u * idx);
        uint32 alloc = (uint32)SpuMalloc((sint32)bytes);

        sound_allocs[base + idx] = alloc;
        SpuSetTransferStartAddr(alloc);
        SpuWrite(body + body_off, bytes);
        SpuIsTransferCompleted(1);
        body_off += bytes;
    }
    sound_alloc_count = (uint16)(base + vag_count);
    return (sint32)base;
}

uint32 sound_update_rival_voices(SOUND_PLAYER *audio, const sint32 pos[3])
{
    SOUND_VOICE *records = audio->reserved + 4;
    sint32 count = (sint32)vehicle_racer_count;
    sint32 idx;
    sint32 order;
    uint32 result = 0u;

    FUNCTION_MARKER(0x800380C0u, "MAIN.EXE");
    idx = 0;
    while (idx < count)
    {
        BOAT *vehicle = vehicle_racers[idx];
        uint32 state_x = (uint32)pos[0];
        uint32 vehicle_x = (uint32)vehicle->contacts.points[0].position[0];
        uint32 vehicle_z = (uint32)vehicle->contacts.points[0].position[2];
        uint32 state_z = (uint32)pos[2];
        sint32 x = (sint32)(state_x + vehicle_x);
        sint32 z = (sint32)(vehicle_z + state_z);
        sint32 distance = (sint32)((uint32)x * (uint32)x + (uint32)z * (uint32)z);

        vehicle->route.distance = distance / 8;
        idx = (sint16)(idx + 1);
        count = (sint32)vehicle_racer_count;
    }
    order = 1;
    while (order < (sint32)vehicle_racer_count)
    {
        BOAT *previous = vehicle_racers[order - 1];
        BOAT *current = vehicle_racers[order];
        sint32 previous_distance = (sint32)(uint32)previous->route.distance;
        sint32 current_distance = (sint32)(uint32)current->route.distance;

        if ((sint32)((uint32)previous_distance - (uint32)current_distance) >= 0)
            ++order;
        else
        {
            vehicle_racers[order - 1] = current;
            vehicle_racers[order] = previous;
            if (order >= 2)
                --order;
        }
    }
    for (idx = 0; idx < 2; ++idx)
    {
        SOUND_VOICE *voice = records + idx * 2;
        BOAT *vehicle = vehicle_racers[idx];
        uint32 gain0 = voice->gain;
        uint32 gain1 = voice[1].gain;

        voice->gain = gain0 << 1;
        voice[1].gain = gain1 << 1;
        sound_update_proximity(vehicle, voice, &audio->rival_acc[idx], &audio->rival_decay[idx]);
        gain0 = voice->gain;
        gain1 = voice[1].gain;
        result = gain1 >> 31;
        voice->gain = (uint32)((sint32)gain0 / 2);
        voice[1].gain = (uint32)((sint32)gain1 / 2);
    }
    return result;
}

void sound_update_proximity(const BOAT *boat, SOUND_VOICE *first_voice, uint16 *acc, uint16 *decay)
{
    sint32 attenuation;
    sint32 steering;
    sint32 vehicle_steering;
    sint32 speed;
    sint32 pitch;
    sint32 volume;
    sint32 pan;
    SOUND_VOICE *second_voice;

    FUNCTION_MARKER(0x800382DCu, "MAIN.EXE");
    if (boat == NULL)
        return;
    attenuation = (sint32)(uint32)boat->route.distance;
    if (attenuation < 0)
        attenuation = (sint32)(0u - (uint32)attenuation);
    attenuation = 4096 - (attenuation >> 10);
    if (attenuation < 7)
        attenuation = 7;
    vehicle_steering = (sint32)(uint32)boat->contacts.points[0].height;
    steering = (sint32)(0u - (uint32)vehicle_steering);
    if (steering < 0)
        steering = vehicle_steering;
    if (steering > 32)
        steering = 32;
    speed = (sint32)((uint32)(uint32)boat->motion.velocity.speed << 8) / 16000;
    pan = (sint32)((uint32)(uint16)boat->contacts.points[0].diagonal[3] - (uint32)(speed / 4) + 64u);
    pitch = (sint32)((uint32)speed + (uint32)(speed / 2) + (uint32)(attenuation >> 9) + ((uint32)steering << 4) + ((game_random_next() & 3u) << 4));
    voice_set_note(first_voice, (sint16)(first_voice->cfg->octave + (uint16)(pitch / 128)), (sint16)(pitch & 0x7F));
    volume = (sint32)((uint32)(attenuation >> 3) + (uint32)(pitch / 64));
    first_voice->vol_r = (uint16)volume;
    first_voice->vol_l = (uint16)volume;
    voice_update_volume(first_voice);
    second_voice = first_voice + 1;
    vehicle_steering = (sint32)(uint32)boat->contacts.points[0].height;
    if (vehicle_steering <= 0)
    {
        uint16 accumulated;

        pan = 0;
        accumulated = *acc;
        *acc = (uint16)((uint32)accumulated - (uint32)(vehicle_steering / 4));
    }
    else
    {
        sint32 accumulated;
        sint32 threshold;

        accumulated = (sint16)*acc;
        if (accumulated >= 513)
            *acc = 512u;
        accumulated = (sint16)*acc;
        if (accumulated <= 0)
        {
            sint32 decay_value = (sint16)*decay;

            if (decay_value > 0)
                *decay = (uint16)(decay_value - 1 - (decay_value >> 6));
        }
        else
        {
            sint32 decay_value;

            threshold = (accumulated >> 3) + 30;
            decay_value = (sint16)*decay;
            if (decay_value < threshold)
                *decay = (uint16)threshold;
        }
        *acc = 0u;
    }
    volume = (sint32)((uint32)((sint16)*decay * 8 + (sint16)pan) * (uint32)attenuation) >> 12;
    if ((volume & 0x8000) != 0)
        volume = 0;
    second_voice->vol_l = (uint16)volume;
    second_voice->vol_r = (uint16)volume;
    voice_update_volume(second_voice);
}




