#ifndef CD_H
#define CD_H

#include "psx.h"

struct RACE_CFG;

extern char cd_dir[128];

sint32 cd_load_course_assets(const struct RACE_CFG *cfg);
sint32 cd_init_drive(void);
uint8 *cd_load_file_alloc(const char *path);
const uint8 *cd_load_file_bytes(const char *path, uint32 *size);
uint32 cd_load_file_buf(const char *path, void *dst, uint32 capacity);

sint32 cd_load_tex(const char *path, sint16 x, sint16 y, sint32 width, sint32 height);

void cd_init_audio_toc(void);
void cd_reset_audio(void);
void cd_pause_audio(void);
void cd_stop_audio(void);
void cd_set_audio_vol(uint16 vol);
void cd_handle_ready_cb(uint8 status, uint8 *response);
sint32 cd_update_audio(sint32 trigger);

sint32 cd_adjust_audio_volume(uint32 enabled);

#endif /* CD_H */
