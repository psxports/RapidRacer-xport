#ifndef RR_CD_H
#define RR_CD_H

#include "psx.h"

sint32 cd_decompress_backrefs(uint32 input, uint32 output);
sint32 cd_load_course_assets(uint32 context);
sint32 cd_init_drive(void);
uint32 cd_load_file_alloc(uint32 path);
uint32 cd_load_file_buf(uint32 path, uint32 buffer);
sint32 cd_upload_image_clut(uint32 request);
uint32 cd_find_tima_chunk(uint32 input, sint32 requested_index);
sint32 cd_load_tex(uint32 path, sint16 x, sint16 y, sint32 width, sint32 height);
sint32 cd_load_archive_assets(uint32 path);
sint32 cd_load_versioned_rec_archive(const uint8 *src, size_t size, sint32 mirror);
sint32 cd_publish_sections(const uint8 *src, size_t size);
sint32 cd_wait_stream(void);
sint32 cd_search_file(uint32 path, uint32 location, uint32 size);
sint32 cd_start_async_read(void);
sint32 cd_prepare_alloc_read(uint32 path);

sint32 cd_prepare_buf_read(uint32 path, uint32 buffer);
sint32 cd_restart_stream_buf(void);

sint32 cd_stream_is_complete(void);
sint32 cd_copy_sector_bytes(uint32 destination, uint32 source, sint32 size);
uint32 cd_search_parent_location(void);
sint32 cd_search_refreshed(void);
sint32 cd_process_sector_cb(void);
sint32 cd_stop_read(void);
uint32 cd_read_file_alloc(uint32 path);
uint32 cd_read_file_buf(uint32 path, uint32 buffer);
sint32 cd_upload_vram_image(uint32 path, sint16 x, sint16 y, sint32 width, sint32 height);
sint32 cd_update_audio(sint32 trigger);
sint32 cd_load_model_archive(void);
uint32 cd_load_selected_asset(uint32 path);
sint32 cd_retry_read_sequence(uint32 request);
sint32 cd_adjust_audio_volume(uint32 enabled);

void cd_clear_data_cbs(void);

#endif /* RR_CD_H */
