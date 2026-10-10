#ifndef RR_SOUND_H
#define RR_SOUND_H

#include "psx.h"

struct RACE_VIEW;

struct BOAT;

typedef struct
{
    uint8 octave, sample;
    uint16 gain, note;
} SOUND_CFG;

typedef struct
{
    uint8 kind, id;
    uint16 note, vol_l, vol_r;
    const SOUND_CFG *cfg;
    uint32 gain;
    uint16 bank;
} SOUND_VOICE;

typedef struct
{
    uint16 count, cap;
    uint16 contact[2], ground_acc, ground_decay;
    uint16 rival_acc[2], rival_decay[2], impact, pitch;
    uint16 reverb_l, reverb_r, reverb_mode, score, last_tick, unused;
    uint16 enabled, throttle, speed;
    uint8 sample6, sample7;
    uint16 reserved_count, countdown;
    SOUND_VOICE *reserved, *events, *pending;
    sint16 reverb_idx;
} SOUND_PLAYER;

extern SOUND_PLAYER sound_players[2];
SOUND_PLAYER *sound_for_context(const struct RACE_VIEW *context);



typedef struct
{
    uint16 music;
    uint16 effects;
    uint16 mode;
    uint16 mono;
    uint16 slider_value;
    uint16 tracks[8];
    uint32 track_slot;
} SOUND_OPTIONS;

typedef struct
{
    uint16 effects, master, music;
} SOUND_MIX;

typedef struct
{
    uint16 main, speech;
} SOUND_BANKS;

extern SOUND_MIX sound_mix;
extern SOUND_BANKS sound_banks;

extern SOUND_OPTIONS sound_options;
extern const uint16 sound_default_tracks[8];

void sound_step_track(sint32 direction);

uint32 sound_queue_command(SOUND_PLAYER *state, sint32 sound, sint32 command);
void voice_set_note(const SOUND_VOICE *voice_state, sint16 octave, sint16 note);
sint32 voice_update_volume(const SOUND_VOICE *voice_state);
void voice_set_volume(sint8 voice, sint16 volume);
sint32 sound_config_reverb_depth(uint16 left, uint16 right);
sint32 voice_start(sint8 voice, sint32 bank, sint32 sample, sint16 octave, sint32 volume);
sint32 sound_start_sample_voice(const SOUND_VOICE *voice_state, sint32 octave, sint32 note, sint32 loop);
sint32 vehicle_update_audio(const struct RACE_VIEW *context);
void sound_remove_inactive_voice_recs(SOUND_PLAYER *state);
sint32 sound_update_impact(struct BOAT *boat, SOUND_VOICE *voice, SOUND_PLAYER *state);
sint32 sound_update_engine_voices(SOUND_VOICE *first_voice, const struct BOAT *boat, const SOUND_PLAYER *src);
sint32 sound_update_steering_voice(SOUND_VOICE *voice, const struct BOAT *boat, SOUND_PLAYER *controls);
sint32 sound_init_audio(void);
sint32 sound_load_bank(const char *name);
sint32 sound_load_bank_config(void);
sint32 sound_start_special_voice(sint32 voice);
void sound_init_race_audio(void);
sint32 sound_init_player_state(SOUND_PLAYER *state);
sint32 voice_start_scaled(sint32 bank, sint16 voice);
sint32 sound_scale_player_volumes(SOUND_PLAYER *p0, SOUND_PLAYER *p1);
void spu_decrease_master_volume(sint32 decrement);
sint32 sound_fade_all(sint32 dec);
sint32 sound_fade_pause(sint32 dec);
sint32 voice_restore_reverb(SOUND_PLAYER *state);

void voice_stop_remove_rec(SOUND_PLAYER *state, SOUND_VOICE *record);
sint32 sound_reset_recs(SOUND_PLAYER *state);

sint32 sound_release_bank_allocs(void);
sint32 sound_upload_bank(const uint8 *header, size_t header_size, const uint8 *body, size_t body_size);
uint32 sound_update_rival_voices(SOUND_PLAYER *audio, const sint32 pos[3]);
void sound_update_proximity(const struct BOAT *boat, SOUND_VOICE *first_voice, uint16 *acc, uint16 *decay);

#endif /* RR_SOUND_H */
