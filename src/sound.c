#include "camera.h"
#include "input.h"
#include "cd.h"
#include "arena.h"
#include "runtime.h"
#include "sound.h"
#include "global.h"
#include "vehicle.h"
#include "psx_spu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

static sint32 sound_sra(uint32 value, uint32 shift)
{
    return (sint32)((value >> shift) | ((value & 0x80000000u) != 0u ? ~((~0u) >> shift) : 0u));
}

static void sound_make_path(uint32 destination, uint32 source, const char *extension)
{
    uint32 offset = 0u;

    while (r_u8(source + offset) != 0u)
    {
        w_u8(destination + offset, r_u8(source + offset));
        ++offset;
    }
    while (*extension != '\0')
        w_u8(destination + offset++, (uint8)*extension++);
    w_u8(destination + offset, 0u);
}

static uint64 spu_transfer_clock(void *context)
{
    return *(uint64 *)context;
}

static sint32 spu_wait_transfer(void *context, uint64 deadline)
{
    *(uint64 *)context = deadline;
    return 1;
}

static sint32 spu_transfer_complete(void *context)
{
    return context != 0;
}

static uint32 spu_transfer_guest_address(void *context, const void *source)
{
    XportMemoryRegion region;
    uint32 offset;

    if (!context || !xport_memory_pointer_identity(source, 1u, &region, &offset) || region != XPORT_MEMORY_DRAM)
        return 0u;
    return 0x80000000u + offset;
}

uint32 sound_queue_command(uint32 state, sint32 sound, sint32 command, uint32 argument)
{
    uint32 queue = state + 3676u;
    sint32 count;
    uint32 record;
    uint32 config = argument;
    uint32 result;

    FUNCTION_MARKER(0x800353FCu, "MAIN.EXE");
    if (command == 4)
    {
        uint16 timer = r_u16(0x800B3D94u);

        w_u16(state + 3706u, 0u);
        w_u32(0x800B3F5Cu, 0u);
        w_u16(state + 3710u, 0u);
        w_u16(state + 3712u, 1u);
        w_u16(state + 3708u, timer);
        return 1u;
    }
    if (command == 5)
    {
        w_u16(state + 3712u, (uint16)sound);
        return 4u;
    }
    if ((sint16)r_u16(state + 3712u) == 0)
    {
        result = (uint32)sound - 3u;
        if (command != 1)
            return result;
        result = result < 3u;
        if (result == 0u)
            return result;
    }
    count = (sint16)r_u16(queue);
    if (count == (sint16)r_u16(state + 3678u))
        return (uint32)count << 2;
    record = r_u32(state + 3728u) + ((uint32)count << 2) * 5u;
    if (command == 0)
    {
        sint32 age;
        sint32 duration;
        sint8 linked;
        uint16 timer;
        uint16 previous;

        result = (uint32)sound << 1;
        if ((sint16)r_u16(0x800E05D2u) == 2 && sound != 11)
            return result;
        config = 0x800912CCu + (uint32)sound * 12u;
        timer = r_u16(0x800B3D94u);
        previous = r_u16(config + 8u);
        age = (sint32)((uint32)timer - (uint32)previous);
        if (age < 0)
        {
            w_u16(config + 8u, timer);
            age = 0;
        }
        else
        {
            duration = (sint16)r_u16(config + 6u);
            age -= duration;
            if (age < 0)
                return (uint32)duration;
        }
        linked = (sint8)r_u8(config + 10u);
        timer = r_u16(0x800B3D94u);
        w_u16(config + 8u, timer);
        w_u16(config + (uint32)(sint32)linked * 12u + 8u, timer);
        w_u16(queue + 32u, timer);
    }
    else if (command == 6)
    {
        uint32 pending;

        if (sound != 0 && sound != 5)
            return 6u;
        pending = r_u32(state + 3732u);
        if (pending == 0u)
            return 6u;
        result = (uint32)voice_stop_remove_rec(queue, pending);
        w_u32(state + 3732u, 0u);
        return result;
    }
    else if (command == 1)
    {
        sint32 pitch;
        sint32 master;

        config = 0x80091248u + (uint32)sound * 12u;
        if (sound == 0)
            w_u32(state + 3732u, record);
        if (sound == 5)
            w_u32(state + 3732u, record);
        if (sound == 9)
        {
            result = r_u32(0x800B6858u);
            if (result != 0u)
                return result;
            w_u32(0x800B6858u, (uint32)command);
        }
        w_u32(record + 8u, config);
        w_u8(record, 8u);
        w_u16(record + 4u, r_u16(config + 6u));
        w_u16(record + 6u, r_u16(config + 8u));
        w_u16(record + 2u, r_u16(config + 4u));
        w_u16(record + 16u, r_u16(0x800834B4u));
        pitch = (sint16)r_u16(config + 2u);
        master = (sint16)r_u16(0x80083490u);
        result = (uint32)(pitch * master);
        config = r_u32(record + 8u);
        w_u32(record + 12u, result);
        sound_start_sample_voice(record, r_u8(config), 0, 0);
        result = (uint32)r_u16(queue) + 1u;
        w_u16(queue, (uint16)result);
        return result;
    }
    else if (command == 7)
    {
        result = (uint32)sound << 1;
        if (r_u16(0x800E05D2u) == 2u)
            return result;
        config = 0x80091374u + (uint32)sound * 12u;
    }
    else if (command == 2)
    {
        sint32 current;

        result = (uint32)sound + 1u;
        if (r_u32(0x80083478u) == 2u)
            return result;
        result = result < 3u;
        if (result != 0u)
        {
            result = (uint32)r_u16(state + 3706u) + (uint32)sound;
            w_u16(state + 3706u, (uint16)result);
            return result;
        }
        current = (sint16)r_u16(state + 3706u);
        if ((sound < 0 && current > 0) || (sound >= 0 && current < 0))
        {
            sint32 sum = (sint32)((uint32)current + (uint32)sound);

            result = (uint32)(sum / 2);
            w_u16(state + 3706u, (uint16)result);
            return result;
        }
        if (sound < 0)
            result = sound < current;
        else
            result = current < sound;
        if (result != 0u)
            w_u16(state + 3706u, (uint16)sound);
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
        uint32 table;
        uint16 timer;
        uint16 previous_timer;

        result = (uint32)(sint32)(sint16)r_u16(0x800E05D2u);
        if (result != 0u)
            return result;
        result = r_u32(0x80083478u);
        if (result == 2u)
            return result;
        result = 8u;
        if (r_u32(0x80083484u) == 8u)
            return result;
        timer = r_u16(0x800B3D94u);
        previous_timer = r_u16(state + 3708u);
        elapsed_raw = (uint32)timer - (uint32)previous_timer;
        elapsed = (sint16)elapsed_raw;
        result = elapsed_raw & 63u;
        if (elapsed < 64 || result == 0u)
            return result;
        current = (sint16)r_u16(state + 3706u);
        delta = (sint32)((uint32)current - r_u32(0x800B3F5Cu));
        w_u32(0x800B3F5Cu, (uint32)current);
        if (delta < 0)
        {
            sint32 previous = (sint16)r_u16(queue + 30u);

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
            sint32 previous = (sint16)r_u16(queue + 30u);

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
            table = 0x800E5FA8u + (uint32)(sint32)(sint16)candidate * 4u;
            if (r_s32(table) < (sint16)best)
            {
                best = r_u16(table);
                selected = candidate;
            }
        }
        result = (uint32)(sint32)(sint16)best;
        if (result == 32000u)
            return result;
        sound = (sint16)selected;
        table = 0x800E5FA8u + (uint32)sound * 4u;
        timer = r_u16(0x800B3D94u);
        w_u16(queue + 30u, 0u);
        w_u32(0x800B3F5Cu, 0u);
        w_u16(queue + 32u, timer);
        result = r_u32(table);
        config = 0x80091374u + (uint32)sound * 12u;
        w_u32(table, result + 1u);
    }
    {
        sint32 pitch;
        sint32 master;

        w_u8(record, 12u);
        w_u32(record + 8u, config);
        w_u16(record + 6u, 127u);
        w_u16(record + 4u, 127u);
        w_u16(record + 16u, r_u16(0x800834B6u));
        w_u16(record + 2u, r_u16(config + 4u));
        pitch = (sint16)r_u16(config + 2u);
        master = (sint16)r_u16(0x80083490u);
        config = r_u32(record + 8u);
        w_u32(record + 12u, (uint32)(pitch * master));
        sound_start_sample_voice(record, r_u8(config), 0, 0);
        result = (uint32)r_u16(queue) + 1u;
        w_u16(queue, (uint16)result);
        return result;
    }
}

void voice_set_note(uint32 voice_state, sint16 octave, sint16 note)
{
    SpuVoiceAttr attr = {0};

    FUNCTION_MARKER(0x80035A5Cu, "MAIN.EXE");
    attr.voice = 1u << (r_u8(voice_state + 1u) & 31u);
    attr.mask = SPU_VOICE_NOTE;
    attr.note = (uint16)(((uint16)octave << 8) | (uint16)note);
    SpuSetVoiceAttr(&attr);
}

sint32 voice_update_volume(uint32 voice_state)
{
    SpuVoiceAttr attr = {0};
    uint32 scale;
    uint32 selected_voice;
    sint32 target_left;
    sint32 target_right;
    sint32 left;
    sint32 right;

    FUNCTION_MARKER(0x80035AA0u, "MAIN.EXE");
    attr.voice = 1u << (r_u8(voice_state + 1u) & 31u);
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    SpuGetVoiceAttr(&attr);
    scale = r_u32(voice_state + 12u);
    target_left = (sint32)((uint32)(sint32)(sint16)r_u16(voice_state + 4u) * scale) >> 11;
    target_right = (sint32)((uint32)(sint32)(sint16)r_u16(voice_state + 6u) * scale) >> 11;
    left = attr.volume.left;
    right = attr.volume.right;
    if (target_left > 0x3FFF)
        target_left = 0x3FFF;
    if (target_right > 0x3FFF)
        target_right = 0x3FFF;
    if (r_u32(0x80083478u) == 1u)
        selected_voice = r_u32(0x800DEF7Cu) + 40u;
    else
    {
        selected_voice = r_u32(0x800DEF7Cu) + 40u;
        if (voice_state != selected_voice)
            selected_voice = r_u32(0x800DFF24u) + 40u;
    }
    if (voice_state != selected_voice)
    {
        if ((sint16)r_u16(0x800E05D4u) == 0)
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
    if ((sint16)r_u16(0x800E05D4u) == 0)
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
    timer = (sint16)r_u16(0x800E05D4u);
    attr.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    if (timer == 0)
    {
        scale = (sint16)r_u16(0x80083490u);
        attr.depth.left = (sint16)((sint32)(uint32)left * scale);
        attr.depth.right = (sint16)((sint32)(uint32)right * scale);
    }
    else
    {
        value = (sint32)(((uint32)left + (uint32)right) >> 1);
        scale = (sint16)r_u16(0x80083490u);
        value *= scale;
        attr.depth.right = (sint16)value;
        attr.depth.left = (sint16)value;
    }
    return SpuSetReverbModeParam(&attr);
}

sint32 spu_config_voice(sint8 voice, sint32 bank, sint32 sample, sint16 octave, sint32 volume, sint32 unused6, sint32 unused7, sint32 unused8, sint16 unused9)
{
    SpuVoiceAttr attr = {0};
    uint32 voice_bit = 1u << ((uint32)(uint8)voice & 31u);
    sint16 index = (sint16)(uint16)((uint32)bank + (uint32)sample);
    uint32 index_offset = (uint32)(sint32)index * 2u;
    uint16 address_index = r_u16(0x800DDF68u + index_offset);
    uint16 adsr1 = r_u16(0x800E90B0u + index_offset);
    uint16 adsr2 = r_u16(0x800E91B0u + index_offset);

    FUNCTION_MARKER(0x80035DCCu, "MAIN.EXE");
    attr.voice = voice_bit;
    attr.mask = r_u32(0x800B3F60u);
    attr.volume.left = (sint16)(uint16)volume;
    attr.volume.right = (sint16)(uint16)volume;
    attr.note = (uint16)((uint32)(uint16)octave << 8);
    attr.sample_note = (uint16)((uint32)r_u8(0x800E0500u + (uint32)(sint32)index) << 8);
    attr.addr = r_u32(0x800FF0F0u + (uint32)(sint32)(sint16)address_index * 4u);
    attr.a_mode = (adsr1 & 0x8000u) != 0u ? 5 : 1;
    if ((adsr2 & 0xC000u) != 0u)
        attr.s_mode = 7;
    else if ((adsr2 & 0x8000u) != 0u)
        attr.s_mode = 5;
    else if ((adsr2 & 0x4000u) != 0u)
        attr.s_mode = 3;
    else
        attr.s_mode = 1;
    attr.r_mode = (adsr2 & 0x20u) != 0u ? 7 : 3;
    attr.ar = (uint16)((adsr1 >> 8) & 0x7Fu);
    attr.dr = (uint16)((adsr1 >> 4) & 0xFu);
    attr.sr = (uint16)((adsr2 >> 6) & 0xFFu);
    attr.rr = (uint16)(adsr2 & 0x1Fu);
    attr.sl = (uint16)(adsr1 & 0xFu);
    SpuSetVoiceAttr(&attr);
    sound_fn_8007741c(1, voice_bit);
    return (sint32)SpuSetReverbVoice(0, voice_bit);
}

sint32 sound_start_sample_voice(uint32 voice_state, sint32 octave, sint32 note, sint32 loop)
{
    SpuVoiceAttr attr = {0};
    uint32 mask;
    uint32 config;
    uint32 scale;
    uint32 product;
    uint32 voice_bit;
    sint32 sample;
    sint32 volume_left;
    sint32 volume_right;
    sint16 address_index;
    sint16 adsr;
    uint32 adsr1_address;
    uint32 adsr2_address;

    FUNCTION_MARKER(0x80035FB0u, "MAIN.EXE");
    mask = r_u32(0x800B3F60u);
    config = r_u32(voice_state + 8u);
    voice_bit = 1u << (r_u8(voice_state + 1u) & 31u);
    sample = (sint16)(uint16)((uint32)r_u8(config + 1u) + (uint32)r_u16(voice_state + 16u));
    address_index = (sint16)r_u16(0x800DDF68u + (uint32)sample * 2u);
    if ((sint16)r_u16(0x800E05D4u) == 0)
    {
        sint16 left = (sint16)r_u16(voice_state + 4u);

        scale = r_u32(voice_state + 12u);
        product = (uint32)(sint32)left * scale;
        volume_left = sound_sra(product, 11u);
        product = (uint32)(sint32)(sint16)r_u16(voice_state + 6u) * scale;
        volume_right = sound_sra(product, 11u);
    }
    else
    {
        uint32 sum = (uint32)(sint32)(sint16)r_u16(voice_state + 4u);

        sum += (uint32)(sint32)(sint16)r_u16(voice_state + 6u);
        scale = r_u32(voice_state + 12u);
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
    attr.sample_note = (uint16)((uint32)r_u8(0x800E0500u + (uint32)sample) << 8);
    attr.addr = r_u32(0x800FF0F0u + (uint32)(sint32)address_index * 4u);
    adsr1_address = 0x800E90B0u + (uint32)sample * 2u;
    adsr2_address = 0x800E91B0u + (uint32)sample * 2u;
    adsr = (sint16)r_u16(adsr1_address);
    attr.a_mode = ((uint16)adsr & 0x8000u) != 0u ? 5 : 1;
    adsr = (sint16)r_u16(adsr2_address);
    if (((uint16)adsr & 0xC000u) != 0u)
        attr.s_mode = 7;
    else if (((uint16)adsr & 0x8000u) != 0u)
        attr.s_mode = 5;
    else if (((uint16)adsr & 0x4000u) != 0u)
        attr.s_mode = 3;
    else
        attr.s_mode = 1;
    attr.r_mode = (r_u16(adsr2_address) & 0x20u) != 0u ? 7 : 3;
    attr.ar = (uint16)((r_u16(adsr1_address) >> 8) & 0x7Fu);
    attr.dr = (uint16)((r_u16(adsr1_address) >> 4) & 0xFu);
    attr.sr = (uint16)((r_u16(adsr2_address) >> 6) & 0xFFu);
    attr.rr = (uint16)(r_u16(adsr2_address) & 0x1Fu);
    attr.sl = (uint16)(r_u16(adsr1_address) & 0xFu);
    SpuSetVoiceAttr(&attr);
    voice_bit = 1u << (r_u8(voice_state + 1u) & 31u);
    sound_fn_8007741c(1, voice_bit);
    voice_bit = 1u << (r_u8(voice_state + 1u) & 31u);
    return (sint32)SpuSetReverbVoice(loop & 1, voice_bit);
}

sint32 vehicle_update_audio(uint32 context, uint32 unused2, uint32 unused3, uint32 unused4)
{
    uint32 timeline = r_u32(context + 3736u);
    BOAT *boat = vehicle_player(context);
    CONTROLLER_STATE *input;
    uint32 state = context + 3676u;
    uint32 voice;
    sint32 mode;
    sint32 value;
    sint32 changed;
    sint32 index;

    FUNCTION_MARKER(0x800363E8u, "MAIN.EXE");
    if (context == 0x800DE0F0u)
        cd_update_audio(1);
    if (boat == NULL)
        return 0;
    mode = (sint32)vehicle_menu(r_u32(context + 100u))->mode;
    if (mode == 2)
        return (sint32)vehicle_update_nearest_voices(context, 0x80083478u);
    if ((uint32)(mode - 5) < 2u)
        sound_fade_all_voice_volumes(context, 64);
    input = input_for_context(context);
    if ((input->current & 0x40u) != 0u)
    {
        sint32 target;

        value = (sint32)((uint32)(uint32)boat->motion.velocity.speed << 8) / 16000;
        w_u16(state + 40u, (uint16)value);
        target = (uint16)boat->control.throttle;
        if ((uint32)boat->control.mode == 1u)
            target += 350;
        target = ((sint32)(sint16)(uint16)target * 256) / 1400;
        value = (sint16)r_u16(state + 38u);
        w_u16(state + 38u, (uint16)(value + (target - value) / 4));
    }
    else
    {
        value = (sint16)r_u16(state + 40u);
        w_u16(state + 40u, (uint16)(value - value / 32));
        value = (sint16)r_u16(state + 38u);
        w_u16(state + 38u, (uint16)(value - value / 32));
    }

    if ((sint16)camera_for_view(context)->direction == 2 && r_u32(0x80083484u) != 5u && camera_for_view(context)->mode != 5u)
    {
        sound_queue_command(context, -1, 2, unused4);
        sound_queue_command(context, 5, 0, unused4);
    }
    if (r_u32(0x80083484u) != 9u)
    {
        sint32 clock = (sint32)r_u32(0x800D6B54u);

        if ((uint32)(clock - 801) < 99u || (uint32)(clock - 401) < 99u)
            sound_queue_command(context, (global_fn_8006e9d8() & 1) + 7, 0, unused4);
        if (r_u32(0x80083484u) != 4u && vehicle_menu(r_u32(context + 100u))->mode == 4u)
        {
            clock = (sint32)r_u32(0x800D6B54u);
            sint32 hundreds = clock / 100;

            if ((sint16)r_u16(state + 46u) != (sint16)hundreds && clock - hundreds * 100 < 5)
            {
                w_u16(state + 46u, (uint16)hundreds);
                clock = (sint32)r_u32(0x800D6B54u);
                if ((uint32)(clock - 91) < 1009u)
                    sound_queue_command(context, 9, 1, unused4);
            }
        }
    }

    value = (sint16)r_u16(state + 40u);
    if (value >= 201)
        sound_queue_command(context, 1, 2, unused4);
    else if (value < 150)
        sound_queue_command(context, 0, 2, unused4);
    sound_queue_command(context, 1, 3, unused4);

    voice = r_u32(state + 48u);
    sound_update_engine_voices(voice, boat, state);
    voice += 40u;
    sound_update_steering_voice(voice, boat, state, context);
    voice += 20u;
    sound_update_impact(vehicle_player(context), voice, context, context);
    voice += 20u;
    if (r_u32(0x80083478u) == 1u)
    {
        for (index = 0; index < 2; ++index)
        {
            sound_update_proximity(vehicle_tracks(context)[index], voice, state + 12u + (uint32)index * 2u, state + 16u + (uint32)index * 2u);
            voice += 40u;
        }
    }

    changed = 0;
    if ((sint16)r_u16(state + 28u) == 10)
    {
        value = (sint16)r_u16(state + 24u);
        if (value >= 17)
        {
            w_u16(state + 24u, (uint16)(value - 1));
            changed = 1;
        }
        value = (sint16)r_u16(state + 26u);
        if (value >= 17)
        {
            w_u16(state + 26u, (uint16)(value - 1));
            changed = 1;
        }
    }
    else
    {
        value = (sint16)r_u16(state + 24u);
        if (value < 228)
        {
            w_u16(state + 24u, (uint16)(value + 1));
            changed = 1;
        }
        value = (sint16)r_u16(state + 26u);
        if (value < 228)
        {
            w_u16(state + 26u, (uint16)(value + 1));
            changed = 1;
        }
    }
    if (changed != 0)
    {
        uint16 first = r_u16(state + 24u);
        uint16 second = r_u16(state + 26u);

        sound_config_reverb_depth(first, second);
    }

    if (r_u32(0x80083478u) == 1u)
    {
        uint32 setup = r_u32(0x8008349Cu);
        uint32 base = r_u32(setup + 140u);

        if (base != 0u)
        {
            sint32 divisor = (sint32)r_u32(0x800B6A98u);
            sint32 dividend = (sint32)(uint32)boat->race.progress;
            sint32 remainder;

            if (divisor == 0 || (dividend == (sint32)0x80000000u && divisor == -1))
                abort();
            remainder = dividend % divisor;
            if ((sint16)remainder < (sint16)r_u16(timeline))
                timeline -= 8u;
            else if ((sint16)r_u16(timeline + 2u) < (sint16)remainder)
            {
                w_u16(state + 28u, 10u);
                timeline += 8u;
            }
            else
                w_u16(state + 28u, 9u);
        }
        else
            w_u16(state + 28u, 10u);
        {
            sint32 timeline_index;
            sint32 count;

            setup = r_u32(0x8008349Cu);
            base = r_u32(setup + 140u);
            count = (sint16)r_u16(setup + 32u);
            timeline_index = (sint16)((timeline - base) >> 3);
            if (timeline_index == count)
                w_u32(state + 60u, base);
            else if (timeline_index >= 0)
                w_u32(state + 60u, timeline);
            else
                w_u32(state + 60u, base + (uint32)(count - 1) * 8u);
        }
    }
    sound_remove_inactive_voice_recs(state);
    return 1;
}

void sound_remove_inactive_voice_recs(uint32 state)
{
    sint32 remaining = (sint16)r_u16(state);
    uint32 record = r_u32(state + 52u);

    FUNCTION_MARKER(0x80036990u, "MAIN.EXE");
    while (remaining-- > 0)
    {
        uint8 type = r_u8(record);

        if (type == 8u || type == 12u)
        {
            uint32 voice = r_u8(record + 1u) & 31u;
            uint32 voice_mask = 1u << voice;
            sint32 status = sound_fn_80077620(voice_mask);

            if (status != 1)
            {
                if (r_u32(record + 8u) == 0x800912B4u)
                    w_u32(0x800B6858u, 0u);
                voice_stop_remove_rec(state, record);
            }
        }
        record += 20u;
    }
}

sint32 sound_update_impact(BOAT *boat, uint32 voice, uint32 state, uint32 argument)
{
    uint32 raw_accumulator;
    uint32 raw_pitch;
    uint32 raw_volume;
    uint32 note_table;
    sint32 accumulator;
    sint32 steering;
    sint32 pitch;
    sint32 result;

    FUNCTION_MARKER(0x80036A68u, "MAIN.EXE");
    accumulator = math_sra_s32((uint32)boat->impact.strength, 14u);
    raw_accumulator = (uint32)accumulator + (uint32)((sint16)r_u16(state + 3696u) / 2);
    w_u16(state + 3696u, (uint16)raw_accumulator);
    accumulator = (sint16)(uint16)raw_accumulator;
    if (accumulator < 0)
        w_u16(state + 3696u, 0u);
    else if (accumulator >= 513)
        w_u16(state + 3696u, 512u);

    accumulator = (sint16)r_u16(state + 3696u);
    if (accumulator < 9)
    {
        raw_pitch = (uint32)r_u16(state + 3698u) - 128u;
        w_u16(state + 3698u, (uint16)raw_pitch);
        if ((raw_pitch & 0x8000u) != 0u)
            w_u16(state + 3698u, 0u);
    }
    else
    {
        if ((r_u32(0x800E0588u) & 4u) != 0u && accumulator >= 65)
            sound_queue_command(state, 12, 0, argument);
        raw_pitch = r_u16(state + 3698u);
        raw_accumulator = r_u16(state + 3696u);
        raw_pitch += raw_accumulator;
        w_u16(state + 3698u, (uint16)raw_pitch);
        if ((sint16)(uint16)raw_pitch >= 2049)
            w_u16(state + 3698u, 2048u);
    }

    accumulator = (sint16)r_u16(state + 3696u);
    steering = (sint16)r_u16(state + 3716u);
    if (accumulator >= 9 && steering >= 201)
    {
        sint32 value = (sint32)(200u - (uint32)accumulator - (uint32)steering);

        if (value > 0)
            value = 0;
        else if (value < -127)
            value = -127;
        sound_queue_command(state, value, 2, argument);
    }

    raw_pitch = r_u16(state + 3698u);
    pitch = (sint16)(uint16)raw_pitch;
    if (pitch <= 0)
    {
        w_u16(voice + 6u, 0u);
        w_u16(voice + 4u, 0u);
    }
    else
    {
        sint32 octave;
        sint32 note;

        raw_volume = (uint32)r_u16(state + 3696u) + 160u;
        note_table = r_u32(voice + 8u);
        octave = r_u8(note_table);
        note = (pitch / 2) & 0x7F;
        w_u16(voice + 4u, (uint16)raw_volume);
        if ((sint16)(uint16)raw_volume >= 257)
            w_u16(voice + 4u, 256u);
        raw_volume = r_u16(voice + 4u);
        w_u16(voice + 6u, (uint16)raw_volume);
        voice_set_note(voice, (sint16)octave, (sint16)note);
    }
    result = voice_update_volume(voice);
    boat->impact.strength = 0;
    return result;
}

sint32 sound_update_engine_voices(uint32 first_voice, const BOAT *boat, uint32 source)
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
    uint32 note_table;
    sint32 octave;
    uint32 second_voice = first_voice + 20u;

    FUNCTION_MARKER(0x80036C68u, "MAIN.EXE");
    range = (sint16)r_u16(source + 38u);
    half_range = range / 2;
    first_volume = 256 - half_range;
    if (first_volume < 0)
        first_volume = 0;
    raw_pitch = r_u16(source + 40u);
    source_value = (sint16)r_u16(source + 6u);
    second_volume = (sint32)(((uint32)(sint32)(sint16)raw_pitch + (uint32)half_range) << 1);
    first_volume = (sint32)((uint32)first_volume + ((uint32)(sint32)(sint16)r_u16(source + 10u) << 2));
    first_volume = (sint32)((uint32)first_volume + ((uint32)((sint32)(sint16)r_u16(source + 4u) + (uint32)source_value) << 1));
    source_value = (sint16)r_u16(source + 8u);
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
    note_table = r_u32(first_voice + 8u);
    octave = (sint32)r_u8(note_table) + math_sra_s32((uint32)pitch, 7u);
    voice_set_note(first_voice, (sint16)octave, (sint16)(pitch & 0x7F));
    w_u16(first_voice + 6u, (uint16)first_volume);
    w_u16(first_voice + 4u, (uint16)first_volume);
    voice_update_volume(first_voice);
    range = (sint16)r_u16(source + 38u);
    source_value = (sint16)r_u16(source + 40u);
    product = (sint32)((uint32)source_value * (uint32)(range / 32));
    second_pitch = (sint32)((uint32)product + (uint32)(product / 2));
    if (second_pitch > 2048)
        second_pitch = 2048;
    if (speed > 0)
        second_pitch = (sint32)((uint32)second_pitch + 128u);
    note_table = r_u32(second_voice + 8u);
    octave = (sint32)r_u8(note_table) + math_sra_s32((uint32)second_pitch, 7u);
    voice_set_note(second_voice, (sint16)octave, (sint16)(second_pitch & 0x7F));
    w_u16(second_voice + 6u, (uint16)second_volume);
    w_u16(second_voice + 4u, (uint16)second_volume);
    return voice_update_volume(second_voice);
}

sint32 sound_update_steering_voice(uint32 voice, const BOAT *boat, uint32 controls, uint32 sound_state)
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
        value = (sint32)((uint32)r_u16(controls + 4u) + (uint32)((sint16)(uint16)first / 2));
        w_u16(controls + 4u, (uint16)value);
        if ((sint16)r_u16(controls + 4u) >= 129)
            w_u16(controls + 4u, 128u);
    }
    else
    {
        current = (sint16)r_u16(controls + 4u);
        if (current >= 9)
            w_u16(controls + 4u, (uint16)(current - current / 8));
        else if (current > 0)
            w_u16(controls + 4u, (uint16)(current - 1));
    }

    second = ((uint16)boat->contacts.points[1].penetration);
    combined = ((uint16)boat->contacts.points[4].penetration);
    second += combined;
    if ((second & 0x8000u) != 0u)
        second = 0u - second;
    if ((sint16)(uint16)second > 0)
    {
        value = (sint32)((uint32)r_u16(controls + 6u) + (uint32)((sint16)(uint16)second / 2));
        w_u16(controls + 6u, (uint16)value);
        if ((sint16)r_u16(controls + 6u) >= 129)
            w_u16(controls + 6u, 128u);
    }
    else
    {
        current = (sint16)r_u16(controls + 6u);
        if (current >= 9)
            w_u16(controls + 6u, (uint16)(current - current / 8));
        else if (current > 0)
            w_u16(controls + 6u, (uint16)(current - 1));
    }

    combined = first + second;
    current = (sint16)r_u16(controls + 40u);
    if (current >= 121 && (sint16)(uint16)combined >= 17)
    {
        uint32 raw_value = 0u - (((uint32)(sint32)(sint16)(uint16)combined - 16u) << 1u);

        raw_value = raw_value + 120u - (uint32)current;
        value = (sint16)(uint16)raw_value;
        if (value > 0)
            value = 0;
        else if (value < -127)
            value = -127;
        sound_queue_command(sound_state, value, 2, sound_state);
    }
    if ((sint16)(uint16)combined >= 33)
        combined = 32;
    value = (sint32)r_u8(r_u32(voice + 8u)) - (sint16)(uint16)combined / 2;
    voice_set_note(voice, (sint16)(uint16)value, 0);

    current = (sint32)(uint32)boat->contacts.points[1].height;
    if (current <= 0)
    {
        pan = 0;
        w_u16(controls + 8u, (uint16)((uint32)r_u16(controls + 8u) - (uint32)(current / 4)));
    }
    else
    {
        sint32 threshold;

        value = (sint32)(uint32)boat->motion.velocity.speed;
        pan = (uint32)(uint16)boat->contacts.points[1].diagonal[3] - (uint32)(value / 250) + 64u;
        current = (sint16)r_u16(controls + 40u);
        if (current >= 181)
        {
            value = (sint16)r_u16(controls + 8u);
            if (value >= 9)
            {
                value = (sint16)(uint16)((uint32)(value / 8) - 180u + (uint32)current);
                if (value >= 128)
                    value = 127;
                else if (value < 0)
                    value = 0;
                sound_queue_command(sound_state, value, 2, 0u);
            }
        }
        if ((sint16)r_u16(controls + 8u) >= 513)
            w_u16(controls + 8u, 512u);
        value = (sint16)r_u16(controls + 8u);
        if (value > 0)
        {
            threshold = math_sra_s32((uint32)value, 3u) + 30;
            current = (sint16)r_u16(controls + 10u);
            if (current < threshold)
                w_u16(controls + 10u, (uint16)threshold);
        }
        else
        {
            uint32 raw_current = r_u16(controls + 10u);

            if ((sint16)(uint16)raw_current > 0)
            {
                current = (sint16)(uint16)raw_current;
                value = (sint32)(raw_current - 1u - (uint32)math_sra_s32((uint32)current, 6u));
                w_u16(controls + 10u, (uint16)value);
            }
        }
        w_u16(controls + 8u, 0u);
    }

    current = (sint16)r_u16(controls + 10u);
    value = (sint16)r_u16(controls + 4u);
    combined = pan + ((uint32)current << 2u) + ((uint32)value << 5u);
    w_u16(voice + 4u, (uint16)combined);
    current = (sint16)r_u16(controls + 10u);
    value = (sint16)r_u16(controls + 6u);
    left_volume = (sint16)r_u16(voice + 4u);
    combined = pan + ((uint32)current << 2u) + ((uint32)value << 5u);
    w_u16(voice + 6u, (uint16)combined);
    if (left_volume < 0)
        w_u16(voice + 4u, 0u);
    if ((sint16)r_u16(voice + 6u) < 0)
        w_u16(voice + 6u, 0u);
    return voice_update_volume(voice);
}

sint32 sound_init_audio(void)
{
    static const CdlLOC original_toc[10] = {{0x51u, 0x15u, 0x00u, 0x00u}, {0x00u, 0x02u, 0x00u, 0x00u}, {0x16u, 0x52u, 0x00u, 0x00u}, {0x21u, 0x52u, 0x00u, 0x00u}, {0x26u, 0x40u, 0x00u, 0x00u}, {0x31u, 0x41u, 0x00u, 0x00u}, {0x37u, 0x14u, 0x00u, 0x00u}, {0x42u, 0x34u, 0x00u, 0x00u}, {0x47u, 0x35u, 0x00u, 0x00u}, {0x50u, 0x44u, 0x00u, 0x00u}};
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    uint32 frame;
    uint8 status;
    sint32 tracks;

    FUNCTION_MARKER(0x8003730Cu, "MAIN.EXE");
    frame = guest_stack_push(0x68u);
    sound_fn_8007503c();
    sound_bind_spu_transfer();

    w_u32(0x800FF2F8u, 0x40001010u);
    w_u32(0x800FF2FCu, 0x0007EFF0u);
    w_u32(0x800A17A0u, 0x800FF2F8u);
    w_u32(0x800A179Cu, 0u);
    w_u32(0x800A1798u, 128u);
    sound_bind_spu_heap();

    w_u32(frame + 0x10u, 0x2C3u);
    w_u16(frame + 0x14u, 0x3FFFu);
    w_u16(frame + 0x16u, 0x3FFFu);
    w_u16(frame + 0x20u, 0x7FFEu);
    w_u16(frame + 0x22u, 0x7FFEu);
    w_u32(frame + 0x28u, 1u);
    common.mask = 0x2C3u;
    common.mvol.left = 0x3FFF;
    common.mvol.right = 0x3FFF;
    common.cd.volume.left = 0x7FFE;
    common.cd.volume.right = 0x7FFE;
    common.cd.mix = 1;
    SpuSetCommonAttr(&common);

    w_u32(0x800A12E0u, 1u);
    w_u32(frame + 0x38u, 0x1Fu);
    w_u32(frame + 0x3Cu, 4u);
    reverb.mask = 0x1Fu;
    reverb.mode = 4;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(0);
    w_u32(frame + 0x38u, 6u);
    w_u16(frame + 0x40u, 0u);
    w_u16(frame + 0x42u, 0u);
    memset(&reverb, 0, sizeof(reverb));
    reverb.mask = 6u;
    SpuSetReverbModeParam(&reverb);
    SpuSetKey(SPU_OFF, SPU_ALLCH);

    status = 0u;
    CdControl(1u, 0, &status);
    w_u8(frame + 0x58u, status);
    if ((status & 0x10u) != 0u && CdDiskReady(1) != CdlComplete)
        while (CdDiskReady(0) != CdlComplete)
        {
        }

    CdSetToc(original_toc, 9);
    tracks = CdGetToc((CdlLOC *)psx_addr(0x800E1BD8u, sizeof(original_toc)));
    w_u32(0x800B6B88u, (uint32)tracks);
    guest_stack_pop(0x68u);
    return 1;
}

sint32 sound_load_bank(uint32 name)
{
    uint32 frame = guest_stack_push(0x28u);
    uint32 path = frame + 0x10u;
    sint32 header;
    sint32 body;
    sint32 result;

    FUNCTION_MARKER(0x80037428u, "MAIN.EXE");
    game_push_checkpoint();
    sound_make_path(path, name, ".VH");
    header = cd_load_file_alloc(path);
    sound_make_path(path, name, ".VB");
    body = cd_load_file_alloc(path);
    result = sound_upload_bank((uint32)header, (uint32)body);
    game_pop_checkpoint();
    guest_stack_pop(0x28u);
    return (sint16)result;
}

sint32 sound_load_bank_config(uint32 state)
{
    static const char prefix[] = "\\SOUND\\";
    SpuCommonAttr attr = {0};
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x800374A8u, "MAIN.EXE");
    attr.mask = 0x2C3u;
    attr.mvol.left = (sint16)(uint16)(r_u16(0x80083492u) << 8);
    attr.mvol.right = attr.mvol.left;
    attr.cd.volume.left = (sint16)(uint16)(r_u16(0x80083494u) << 9);
    attr.cd.volume.right = attr.cd.volume.left;
    attr.cd.mix = 1;
    w_u16(0x800B3F58u, 0u);
    SpuSetCommonAttr(&attr);
    SpuSetReverb(0);
    for (index = 0; index < (sint32)sizeof(prefix); ++index)
        w_u8(0x800D6958u + (uint32)index, (uint8)prefix[index]);
    result = sound_load_bank(0x800B3F84u);
    w_u16(state + 60u, (uint16)result);
    return result;
}

sint32 sound_start_special_voice(sint32 voice)
{
    sint32 scale;

    FUNCTION_MARKER(0x8003755Cu, "MAIN.EXE");
    if (voice != 7 && voice != 12)
        return 12;
    scale = (sint32)(sint8)r_u8(0x80083494u) * 256;
    return spu_config_voice((sint8)voice, (sint16)r_u16(0x800834B4u), voice, 60, scale, 0, 0, 0, 0);
}

uint32 spu_init_cd_audio(void)
{
    SpuCommonAttr common = {0};
    SpuReverbAttr reverb = {0};
    CdlATV mix;
    uint16 master_volume;
    uint16 cd_volume;
    uint32 record;
    sint32 index;

    FUNCTION_MARKER(0x800375BCu, "MAIN.EXE");
    common.mask = 0x2C3u;
    common.cd.mix = 1;
    master_volume = r_u16(0x80083492u);
    cd_volume = r_u16(0x80083494u);
    w_u16(0x800B3F58u, 0u);
    w_u32(0x800B6858u, 0u);
    common.mvol.left = (sint16)(uint16)(master_volume << 8);
    common.mvol.right = common.mvol.left;
    common.cd.volume.left = (sint16)(uint16)(cd_volume << 9);
    common.cd.volume.right = common.cd.volume.left;
    SpuSetCommonAttr(&common);
    reverb.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    reverb.depth.left = (sint16)(uint16)((uint32)(sint32)(sint16)r_u16(0x80083490u) << 4);
    reverb.depth.right = reverb.depth.left;
    SpuSetReverbModeParam(&reverb);
    SpuSetReverb(1);
    w_u32(0x800D6958u, r_u32(0x800B3F7Cu));
    w_u32(0x800D695Cu, r_u32(0x800B3F80u));
    w_u16(0x800834B4u, (uint16)sound_load_bank(0x800B3F8Cu));
    w_u16(0x800834B6u, (uint16)sound_load_bank(0x800B3F94u));
    for (index = 0; index < 24; ++index)
        w_u8(0x800CAAC1u + 20u * (uint32)index, (uint8)index);
    if (r_u32(0x80083478u) == 1u)
    {
        w_u16(0x800DEF78u, 8u);
        w_u32(0x800DEF7Cu, 0x800CAAC0u);
        w_u16(0x800DEF4Eu, 16u);
        w_u32(0x800DEF80u, 0x800CAB60u);
    }
    else
    {
        w_u32(0x800DEF80u, 0x800CAB10u);
        w_u32(0x800DEF7Cu, 0x800CAAC0u);
        w_u16(0x800DEF78u, 4u);
        w_u16(0x800DEF4Eu, 8u);
        w_u16(0x800DFF20u, 4u);
        w_u32(0x800DFF24u, 0x800CABB0u);
        w_u32(0x800DFF28u, 0x800CAC00u);
        w_u16(0x800DFEF6u, 8u);
    }
    for (index = 0; index < 15; ++index)
        w_u32(0x800E5FE0u - 4u * (uint32)index, 0u);
    sound_init_player_state(0x800DEF4Cu);
    sound_queue_command(0x800DE0F0u, 0, 4, 0u);
    if (r_u32(0x80083478u) == 2u)
    {
        sound_init_player_state(0x800DFEF4u);
        sound_queue_command(0x800DF098u, 0, 4, 0u);
    }
    if ((sint16)r_u16(0x800E05D4u) == 0)
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
    record = 0x800912CCu;
    for (index = 0; index < 14; ++index)
    {
        w_u16(record + 8u, 0u);
        record += 12u;
    }
    return record;
}

sint32 sound_init_player_state(uint32 state)
{
    uint32 records;
    uint32 setup;
    sint32 count;
    sint32 index = 0;
    sint32 result = 7;

    FUNCTION_MARKER(0x8003784Cu, "MAIN.EXE");
    w_u16(state + 26u, 16u);
    setup = r_u16(state + 26u);
    w_u32(state + 56u, 0u);
    w_u16(state + 22u, 0u);
    w_u16(state + 20u, 0u);
    w_u16(state + 24u, 16u);
    sound_config_reverb_depth(16u, (uint16)setup);
    w_u16(state + 28u, 10u);
    setup = r_u32(0x8008349Cu);
    records = r_u32(state + 48u);
    count = (sint16)r_u16(state + 44u);
    setup = r_u32(setup + 140u);
    w_u16(state + 46u, 30u);
    w_u8(state + 42u, 6u);
    w_u16(state, 0u);
    w_u16(state + 6u, 0u);
    w_u16(state + 4u, 0u);
    w_u16(state + 10u, 0u);
    w_u16(state + 8u, 0u);
    w_u16(state + 38u, 0u);
    w_u16(state + 40u, 0u);
    w_u16(state + 14u, 0u);
    w_u16(state + 12u, 0u);
    w_u16(state + 18u, 0u);
    w_u16(state + 16u, 0u);
    w_u16(state + 30u, 0u);
    w_u16(state + 32u, 0u);
    w_u8(state + 43u, 7u);
    w_u32(state + 60u, setup);
    if (count <= 0)
        return result;
    do
    {
        uint32 record = records + (uint32)index * 20u;
        uint32 config = 0x800911A8u + (uint32)index * 20u;
        sint16 sample;
        sint16 scale;

        w_u32(record + 8u, config);
        w_u8(record, (uint8)index);
        w_u16(record + 2u, 0u);
        w_u16(record + 6u, 0u);
        w_u16(record + 4u, 0u);
        w_u16(record + 16u, r_u16(0x800834B4u));
        config = r_u32(record + 8u);
        sample = (sint16)r_u16(config + 2u);
        scale = (sint16)r_u16(0x80083490u);
        w_u32(record + 12u, (uint32)((sint32)sample * (sint32)scale));
        sound_start_sample_voice(record, 60, 0, (sint32)r_u32(0x800911B8u + (uint32)index * 20u));
        count = (sint16)r_u16(state + 44u);
        index = (sint32)((uint32)index + 1u);
        result = index < count;
    } while (result != 0);
    return result;
}

sint32 voice_start_scaled(sint32 bank, sint16 voice)
{
    sint32 scale = (sint16)(r_s16(0x80083490u) * (voice == 8 ? 192 : 256));

    FUNCTION_MARKER(0x800379C0u, "MAIN.EXE");
    return spu_config_voice((sint8)voice, (sint16)bank, voice, 60, scale, 0, 0, 0, 0);
}

sint32 sound_scale_player_volumes(uint32 first, uint32 second)
{
    uint32 records;
    uint32 record;
    sint32 count;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80037A44u, "MAIN.EXE");

    count = (sint16)r_u16(first + 3720u);
    records = r_u32(first + 3724u);
    if (count > 0)
    {
        index = 0;
        record = records + 12u;
        do
        {
            uint32 config = r_u32(record - 4u);
            sint32 sample = (sint16)r_u16(config + 2u);
            sint32 scale = (sint16)r_u16(0x80083490u);

            ++index;
            w_u32(record, (uint32)(sample * scale));
            count = (sint16)r_u16(first + 3720u);
            record += 20u;
        } while (index < count);
    }

    count = (sint16)r_u16(first + 3676u);
    records = r_u32(first + 3728u);
    if (count > 0)
    {
        index = 0;
        record = records + 12u;
        do
        {
            uint32 config = r_u32(record - 4u);
            sint32 sample = (sint16)r_u16(config + 2u);
            sint32 scale = (sint16)r_u16(0x80083490u);

            ++index;
            w_u32(record, (uint32)(sample * scale));
            count = (sint16)r_u16(first + 3676u);
            record += 20u;
        } while (index < count);
    }

    if (r_u32(0x80083478u) != 2u)
        return 2;

    count = (sint16)r_u16(second + 3720u);
    records = r_u32(second + 3724u);
    if (count > 0)
    {
        index = 0;
        record = records + 12u;
        do
        {
            uint32 config = r_u32(record - 4u);
            sint32 sample = (sint16)r_u16(config + 2u);
            sint32 scale = (sint16)r_u16(0x80083490u);

            ++index;
            w_u32(record, (uint32)(sample * scale));
            count = (sint16)r_u16(second + 3720u);
            record += 20u;
        } while (index < count);
    }

    result = (sint16)r_u16(second + 3676u);
    records = r_u32(second + 3728u);
    if (result <= 0)
        return result;
    index = 0;
    record = records + 12u;
    do
    {
        uint32 config = r_u32(record - 4u);
        sint32 sample = (sint16)r_u16(config + 2u);
        sint32 scale = (sint16)r_u16(0x80083490u);

        ++index;
        w_u32(record, (uint32)(sample * scale));
        result = index < (sint16)r_u16(second + 3676u);
        record += 20u;
    } while (result != 0);
    return result;
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

sint32 sound_fade_all_voice_volumes(uint32 unused, sint32 decrement)
{
    uint32 frame;
    uint32 attr_address;
    SpuVoiceAttr *attr;
    sint32 stopped = 0;
    sint32 voice;

    FUNCTION_MARKER(0x80037C3Cu, "MAIN.EXE");
    if (r_u32(0x80083478u) == 2u)
        decrement = (sint32)((uint32)decrement << 1);
    frame = guest_stack_push(0x70u);
    attr_address = frame + 16u;
    attr = (SpuVoiceAttr *)psx_addr(attr_address, sizeof(SpuVoiceAttr));
    for (voice = 0; voice < 24; ++voice)
    {
        sint32 left;
        sint32 right;

        w_u32(attr_address, 1u << (uint32)voice);
        SpuGetVoiceAttr(attr);
        left = (sint16)r_u16(attr_address + 8u);
        if (decrement < left)
            w_u16(attr_address + 8u, (uint32)(left - decrement));
        else
        {
            w_u16(attr_address + 8u, 0u);
            ++stopped;
        }
        right = (sint16)r_u16(attr_address + 10u);
        if (decrement < right)
            w_u16(attr_address + 10u, (uint32)(right - decrement));
        else
        {
            w_u16(attr_address + 10u, 0u);
            ++stopped;
        }
        w_u32(attr_address + 4u, SPU_VOICE_VOLL | SPU_VOICE_VOLR);
        SpuSetVoiceAttr(attr);
    }
    if (stopped < 48)
        sound_config_reverb_depth(0u, 0u);
    else
        SpuSetReverb(0);
    voice = stopped == 48;
    guest_stack_pop(0x70u);
    return voice;
}

sint32 voice_restore_reverb(uint32 state)
{
    sint32 count;
    uint32 records;
    sint32 index;

    FUNCTION_MARKER(0x80037D38u, "MAIN.EXE");
    count = (sint16)r_u16(state + 3720u);
    records = r_u32(state + 3724u);
    if (count > 0)
    {
        index = 0;
        do
        {
            voice_update_volume(records);
            ++index;
            count = (sint16)r_u16(state + 3720u);
            records += 20u;
        } while (index < count);
    }
    w_u16(state + 3702u, 16u);
    count = r_u16(state + 3702u);
    w_u16(state + 3700u, 16u);
    sound_config_reverb_depth(16u, (uint16)count);
    return SpuSetReverb(1);
}

sint32 voice_stop_remove_rec(uint32 state, uint32 record)
{
    sint32 count;
    uint32 first;
    uint32 second;
    uint32 third;
    uint32 fourth;
    uint32 last;
    uint32 result;
    uint8 voice = r_u8(record + 1u);

    FUNCTION_MARKER(0x80037DBCu, "MAIN.EXE");
    sound_fn_8007741c(0, 1u << (voice & 31u));
    count = (sint16)r_u16(state) - 1;
    w_u16(state, (uint16)count);
    result = r_u32(state + 52u);
    last = result + (uint32)count * 20u;
    if (last != record)
    {
        result = voice;
        first = r_u32(last);
        second = r_u32(last + 4u);
        third = r_u32(last + 8u);
        fourth = r_u32(last + 12u);
        w_u32(record, first);
        w_u32(record + 4u, second);
        w_u32(record + 8u, third);
        w_u32(record + 12u, fourth);
        first = r_u32(last + 16u);
        w_u32(record + 16u, first);
        w_u8(last + 1u, voice);
    }
    return (sint32)result;
}

sint32 sound_reset_recs(uint32 state)
{
    sint32 count;
    sint32 reserved_count;
    uint32 records;
    uint32 reserved;
    sint32 index;
    sint32 result;

    FUNCTION_MARKER(0x80037E68u, "MAIN.EXE");
    count = (sint16)r_u16(state + 3676u);
    records = r_u32(state + 3728u);
    for (index = 0; index < count; ++index)
    {
        uint32 record = records + (uint32)index * 20u;
        w_u16(record + 2u, 0u);
        w_u16(record + 6u, 0u);
        w_u16(record + 4u, 0u);
    }
    reserved = r_u32(state + 3724u);
    reserved_count = (sint16)r_u16(state + 3720u);
    result = reserved_count;
    w_u16(state + 3676u, 0u);
    for (index = 0; index < reserved_count;)
    {
        uint32 record = reserved + (uint32)index * 20u;

        ++index;
        w_u16(record + 2u, 0u);
        w_u16(record + 6u, 0u);
        w_u16(record + 4u, 0u);
        reserved_count = (sint16)r_u16(state + 3720u);
        result = index < reserved_count;
    }
    return result;
}

sint32 sound_release_bank_allocs(void)
{
    sint32 count = (sint16)r_u16(0x800B3F58u);
    sint32 index;
    sint32 result = count;

    FUNCTION_MARKER(0x80037EDCu, "MAIN.EXE");
    if (count > 0)
        sound_bind_spu_heap();
    for (index = 0; index < count; ++index)
    {
        uint32 allocation = r_u32(0x800FF0F0u + (uint32)index * 4u);
        SpuFree(allocation);
        count = (sint16)r_u16(0x800B3F58u);
        result = index + 1 < count;
    }
    w_u16(0x800B3F58u, 0u);
    return result;
}

sint32 sound_upload_bank(uint32 context, uint32 sound_address)
{
    sint32 first_count = r_u8(context + 18u);
    sint32 second_count = r_u8(context + 22u);
    sint32 base = (sint16)r_u16(0x800B3F58u);
    sint32 index;

    FUNCTION_MARKER(0x80037F3Cu, "MAIN.EXE");
    for (index = 0; index < first_count; ++index)
    {
        uint32 record = context + 2098u + 512u * (uint32)index;
        w_u8(0x800E0500u + (uint32)(base + index), r_u8(record - 14u));
        w_u16(0x800DDF68u + 2u * (uint32)(base + index), (uint16)(r_u16(record + 4u) + base - 1));
        w_u16(0x800E90B0u + 2u * (uint32)(base + index), r_u16(record - 2u));
        w_u16(0x800E91B0u + 2u * (uint32)(base + index), r_u16(record));
    }
    sound_bind_spu_heap();
    SpuSetTransferMode(0);
    for (index = 0; index < second_count; ++index)
    {
        uint32 table = context + 2082u + 512u * (uint32)first_count + 2u * (uint32)index;
        uint32 byte_count = 8u * r_u16(table);
        uint32 allocation = (uint32)SpuMalloc((sint32)byte_count);
        w_u32(0x800FF0F0u + 4u * (uint32)(base + index), allocation);
        SpuSetTransferStartAddr(allocation);
        SpuWrite(psx_addr(sound_address, byte_count), byte_count);
        SpuIsTransferCompleted(1);
        sound_address += byte_count;
    }
    w_u16(0x800B3F58u, (uint16)(base + second_count));
    return base;
}

uint32 vehicle_update_nearest_voices(uint32 state, uint32 setup)
{
    uint32 records = r_u32(state + 3724u) + 80u;
    sint32 count = (sint32)r_u32(setup + 44u);
    sint32 index;
    sint32 order;
    uint32 result = 0u;

    FUNCTION_MARKER(0x800380C0u, "MAIN.EXE");
    index = 0;
    while (index < count)
    {
        BOAT *vehicle = *(vehicle_racers + (index));
        uint32 state_x = (uint32)camera_for_view(state)->position[0];
        uint32 vehicle_x = (uint32)vehicle->contacts.points[0].position[0];
        uint32 vehicle_z = (uint32)vehicle->contacts.points[0].position[2];
        uint32 state_z = (uint32)camera_for_view(state)->position[2];
        sint32 x = (sint32)(state_x + vehicle_x);
        sint32 z = (sint32)(vehicle_z + state_z);
        sint32 distance = (sint32)((uint32)x * (uint32)x + (uint32)z * (uint32)z);

        vehicle->route.distance = (sint32)((uint32)((distance + (distance < 0 ? 7 : 0)) >> 3));
        index = (sint16)(index + 1);
        count = (sint32)r_u32(setup + 44u);
    }
    order = 1;
    while (order < (sint32)vehicle_racer_count)
    {
        BOAT *previous = *(vehicle_racers + (order - 1));
        BOAT *current = *(vehicle_racers + (order));
        sint32 previous_distance = (sint32)(uint32)previous->route.distance;
        sint32 current_distance = (sint32)(uint32)current->route.distance;

        if (previous_distance >= current_distance)
            ++order;
        else
        {
            *(vehicle_racers + (order - 1)) = current;
            *(vehicle_racers + (order)) = previous;
            if (order >= 2)
                --order;
        }
    }
    for (index = 0; index < 2; ++index)
    {
        uint32 voice = records + (uint32)index * 40u;
        BOAT *vehicle = *(vehicle_racers + (index));
        uint32 first = r_u32(voice + 12u);
        uint32 second = r_u32(voice + 32u);

        w_u32(voice + 12u, first << 1);
        w_u32(voice + 32u, second << 1);
        sound_update_proximity(vehicle, voice, state + 3688u + (uint32)index * 2u, state + 3692u + (uint32)index * 2u);
        first = r_u32(voice + 12u);
        second = r_u32(voice + 32u);
        result = second >> 31;
        w_u32(voice + 12u, (uint32)((sint32)first / 2));
        w_u32(voice + 32u, (uint32)((sint32)second / 2));
    }
    return result;
}

void sound_update_proximity(const BOAT *boat, uint32 first_voice, uint32 accumulator, uint32 decay)
{
    sint32 attenuation;
    sint32 steering;
    sint32 vehicle_steering;
    sint32 speed;
    sint32 pitch;
    sint32 volume;
    sint32 pan;
    uint32 second_voice;

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
    pitch = (sint32)((uint32)speed + (uint32)(speed / 2) + (uint32)(attenuation >> 9) + ((uint32)steering << 4) + ((global_fn_8006e9d8() & 3u) << 4));
    voice_set_note(first_voice, (sint16)(r_u8(r_u32(first_voice + 8u)) + (uint16)(pitch / 128)), (sint16)(pitch & 0x7F));
    volume = (sint32)((uint32)(attenuation >> 3) + (uint32)(pitch / 64));
    w_u16(first_voice + 6u, (uint16)volume);
    w_u16(first_voice + 4u, (uint16)volume);
    voice_update_volume(first_voice);
    second_voice = first_voice + 20u;
    vehicle_steering = (sint32)(uint32)boat->contacts.points[0].height;
    if (vehicle_steering <= 0)
    {
        uint16 accumulated;

        pan = 0;
        accumulated = r_u16(accumulator);
        w_u16(accumulator, (uint16)((uint32)accumulated - (uint32)(vehicle_steering / 4)));
    }
    else
    {
        sint32 accumulated;
        sint32 threshold;

        accumulated = (sint16)r_u16(accumulator);
        if (accumulated >= 513)
            w_u16(accumulator, 512u);
        accumulated = (sint16)r_u16(accumulator);
        if (accumulated <= 0)
        {
            sint32 decay_value = (sint16)r_u16(decay);

            if (decay_value > 0)
                w_u16(decay, (uint16)(decay_value - 1 - (decay_value >> 6)));
        }
        else
        {
            sint32 decay_value;

            threshold = (accumulated >> 3) + 30;
            decay_value = (sint16)r_u16(decay);
            if (decay_value < threshold)
                w_u16(decay, (uint16)threshold);
        }
        w_u16(accumulator, 0u);
    }
    volume = (sint32)((uint32)((sint16)r_u16(decay) * 8 + (sint16)pan) * (uint32)attenuation) >> 12;
    if ((volume & 0x8000) != 0)
        volume = 0;
    w_u16(second_voice + 4u, (uint16)volume);
    w_u16(second_voice + 6u, (uint16)volume);
    voice_update_volume(second_voice);
}

sint32 sound_fn_8007503c(void)
{
    sint32 index;

    FUNCTION_MARKER(0x8007503Cu, "MAIN.EXE");
    SpuInit();
    for (index = 0; index < 24; ++index)
        w_u16(0x800A1304u + 2u * (uint32)index, UINT16_C(0xC000));
    w_u32(0x800A12DCu, 0u);
    w_u32(0x800A12E0u, 0u);
    w_u32(0x800A12ECu, 0u);
    w_u16(0x800A12F0u, 0u);
    w_u16(0x800A12F2u, 0u);
    w_u32(0x800A12F4u, 0u);
    w_u32(0x800A12F8u, 0u);
    w_u32(0x800A12E4u, 0xF204u);
    w_u32(0x800A1764u, 3u);
    w_u32(0x800A176Cu, 7u);
    w_u32(0x800A1798u, 0u);
    w_u32(0x800A179Cu, 0u);
    w_u32(0x800A17A0u, 0u);
    w_u32(0x800A12D8u, 0u);
    w_u32(0x800A1758u, 0u);
    w_u32(0x800A12D4u, 0u);
    w_u32(0x800A1300u, 0u);
    w_u32(0x800A12FCu, 0u);
    w_u32(0x800A1734u, 0u);
    return 0;
}

sint32 sound_fn_80077620(uint32 mask)
{
    sint32 voice;
    uint16 envelope;

    FUNCTION_MARKER(0x80077620u, "MAIN.EXE");
    for (voice = 0; voice < 24; ++voice)
        if ((mask & (1u << (uint32)voice)) != 0u)
            break;
    if (voice == 24)
        return -1;
    envelope = r_u16(r_u32(0x800A173Cu) + (uint32)voice * 16u + 12u);
    if ((r_u32(0x800A12D4u) & (1u << (uint32)voice)) != 0u)
        return envelope == 0u ? 3 : 1;
    return envelope != 0u ? 2 : 0;
}

sint32 sound_fn_8007741c(sint32 enable, uint32 mask)
{
    uint32 value = mask & 0xFFFFFFu;
    uint16 high = (uint16)(value >> 16);

    if (enable != 0)
    {
        if (enable != 1)
            return 1;
        if ((r_u32(0x800A1734u) & 1u) != 0u)
        {
            w_u32(0x800DDCC0u, value);
            xport_update_u32(0x800A1300u, XPORT_MEMORY_UPDATE_OR, 1u);
            xport_update_u32(0x800A12FCu, XPORT_MEMORY_UPDATE_OR, value);
            xport_update_u16(0x800DDCC4u, XPORT_MEMORY_UPDATE_AND, (uint16)~value);
            xport_update_u16(0x800DDCC6u, XPORT_MEMORY_UPDATE_AND, (uint16)~high);
            return r_u16(0x800DDCC6u);
        }
        SpuSetKey(SPU_ON, value);
        xport_update_u32(0x800A12D4u, XPORT_MEMORY_UPDATE_OR, value);
        return (sint32)r_u32(0x800A12D4u);
    }
    if ((r_u32(0x800A1734u) & 1u) == 0u)
    {
        SpuSetKey(SPU_OFF, value);
        xport_update_u32(0x800A12D4u, XPORT_MEMORY_UPDATE_AND, ~value);
        return (sint32)r_u32(0x800A12D4u);
    }
    w_u32(0x800DDCC4u, value);
    xport_update_u32(0x800A1300u, XPORT_MEMORY_UPDATE_OR, 1u);
    xport_update_u32(0x800A12FCu, XPORT_MEMORY_UPDATE_AND, ~value);
    xport_update_u16(0x800DDCC0u, XPORT_MEMORY_UPDATE_AND, (uint16)~value);
    xport_update_u16(0x800DDCC2u, XPORT_MEMORY_UPDATE_AND, (uint16)~high);
    return r_u16(0x800DDCC2u);
}

uint32 spu_write_data_clamped(uint32 address, uint32 size, uint32 mode)
{
    FUNCTION_MARKER(0x800776B4u, "MAIN.EXE");
    if (size > 520176u)
        size = 520176u;
    SpuWrite(psx_addr(address, size), size);
    if (r_u32(0x800A1774u) == 0u)
        w_u32(0x800A1770u, 0u);
    return size;
}

sint32 sound_bind_spu_heap(void)
{
    SpuHeapBinding heap_binding = {0};
    heap_binding.limit = r_u32(0x800A1798u);
    if (heap_binding.limit < 0x3ffffu && (r_u32(0x800A17A0u) & 0xe0000003u) == 0x80000000u && (r_u32(0x800A17A0u) & 0x1fffffffu) <= 0x200000u - 8u * (heap_binding.limit + 1u))
    {
        heap_binding.capacity = heap_binding.limit + 1u;
        heap_binding.entries = (uint32 *)psx_addr(r_u32(0x800A17A0u), 8u * heap_binding.capacity);
        heap_binding.last_index = (uint32 *)psx_addr(0x800A179Cu, 4u);
        heap_binding.address_shift = r_u32(0x800A1764u) & 31u;
        heap_binding.alignment_mask = r_u32(0x800A176Cu);
        heap_binding.reserved_bytes = r_u32(0x800A12E0u) ? (0x10000u - r_u32(0x800A12E4u)) << heap_binding.address_shift : 0u;
    }
    return spu_bind_heap(&heap_binding);
}

sint32 sound_bind_spu_transfer(void)
{
    static uint64 ticks;
    SpuTransferBinding binding = {
        (uint32 *)psx_addr(0x800A12D8u, 4u), (uint32 *)psx_addr(0x800A1758u, 4u), (uint16 *)psx_addr(0x800A1754u, 2u), (uint32 *)psx_addr(0x800A1770u, 4u), (uint32 *)psx_addr(0x800A1774u, 4u), (uint32 *)psx_addr(0x800A1790u, 4u), (uint32 *)psx_addr(0x800A1794u, 4u), r_u32(0x800A1764u) & 31u, r_u32(0x800A1760u) != 0u, r_u32(0x800A1768u), r_u32(0x800A176Cu), 64u, 1024u, spu_transfer_clock, spu_wait_transfer, spu_transfer_complete, spu_transfer_guest_address, &ticks,
    };

    ticks = 0u;
    return spu_bind_transfer(&binding);
}
