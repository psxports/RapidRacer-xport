#ifndef RR_SOUND_H
#define RR_SOUND_H

#include "psx.h"

struct BOAT;

uint32 sound_queue_command(uint32 state, sint32 sound, sint32 command, uint32 argument);
void voice_set_note(uint32 voice_state, sint16 octave, sint16 note);
sint32 voice_update_volume(uint32 voice_state);
void voice_set_volume(sint8 voice, sint16 volume);
sint32 sound_config_reverb_depth(uint16 left, uint16 right);
sint32 spu_config_voice(sint8 voice, sint32 bank, sint32 sample, sint16 octave, sint32 volume, sint32 unused6, sint32 unused7, sint32 unused8, sint16 unused9);
sint32 sound_start_sample_voice(uint32 voice_state, sint32 octave, sint32 note, sint32 loop);
sint32 vehicle_update_audio(uint32 context, uint32 unused2, uint32 unused3, uint32 unused4);
void sound_remove_inactive_voice_recs(uint32 state);
sint32 sound_update_impact(struct BOAT *boat, uint32 voice, uint32 state, uint32 argument);
sint32 sound_update_engine_voices(uint32 first_voice, const struct BOAT *boat, uint32 source);
sint32 sound_update_steering_voice(uint32 voice, const struct BOAT *boat, uint32 controls, uint32 sound_state);
sint32 sound_init_audio(void);
sint32 sound_load_bank(uint32 name);
sint32 sound_load_bank_config(uint32 state);
sint32 sound_start_special_voice(sint32 voice);
uint32 spu_init_cd_audio(void);
sint32 sound_init_player_state(uint32 state);
sint32 voice_start_scaled(sint32 bank, sint16 voice);
sint32 sound_scale_player_volumes(uint32 first, uint32 second);
void spu_decrease_master_volume(sint32 decrement);
sint32 sound_fade_all_voice_volumes(uint32 unused, sint32 decrement);
sint32 voice_restore_reverb(uint32 state);

sint32 voice_stop_remove_rec(uint32 state, uint32 record);
sint32 sound_reset_recs(uint32 state);

sint32 sound_release_bank_allocs(void);
sint32 sound_upload_bank(uint32 context, uint32 sound_address);
uint32 vehicle_update_nearest_voices(uint32 state, uint32 setup);
void sound_update_proximity(const struct BOAT *boat, uint32 first_voice, uint32 accumulator, uint32 decay);
sint32 sound_fn_8007503c(void);
sint32 sound_fn_80077620(uint32 mask);
sint32 sound_fn_8007741c(sint32 enable, uint32 mask);
uint32 spu_write_data_clamped(uint32 address, uint32 size, uint32 mode);
sint32 sound_bind_spu_heap(void);
sint32 sound_bind_spu_transfer(void);

#endif /* RR_SOUND_H */
