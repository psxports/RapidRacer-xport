#ifndef RR_RUNTIME_H
#define RR_RUNTIME_H

#include "psx.h"

void cd_set_data_cb(uint32 callback);
void runtime_set_vsync_cb(uint32 callback);
void runtime_load_font(sint32 x, sint32 y);
sint32 runtime_open_font(sint32 x, sint32 y, sint32 width, sint32 height, sint32 background, sint32 max_characters);
void runtime_set_font_dump(sint32 font);
uint32 cd_set_ready_cb(uint32 callback);
uint32 runtime_build_cd_path_at(uint32 output, uint32 input);
uint32 runtime_build_cd_path(uint32 input);
uint32 runtime_join_paths(uint32 prefix, const char *suffix);
sint32 runtime_parse_decimal(uint32 text, sint32 length);
void runtime_copy_guest_text(uint32 destination, uint32 source);
void runtime_copy_host_text(uint32 destination, const char *source);
void runtime_join_guest_text(uint32 destination, uint32 first, uint32 second);
void runtime_copy_guest_5(uint32 destination, uint32 source);
void runtime_copy_guest_7(uint32 destination, uint32 source);
void runtime_copy_guest_8(uint32 destination, uint32 source);
void runtime_copy_guest_9(uint32 destination, uint32 source);
void runtime_format_vram_filename(uint32 destination, sint32 selection, sint32 two_player);

#endif /* RR_RUNTIME_H */
