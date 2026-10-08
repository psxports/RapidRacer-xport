#ifndef RR_GAME_MAIN_H
#define RR_GAME_MAIN_H

#include "psx.h"
#include "game.h"
#include <stddef.h>
#include <stdio.h>

void xport_main(void);

sint32 cd_host_file_size(uint32 guest_path, size_t *size);
sint32 cd_read_host_file(uint32 guest_path, uint32 destination, size_t capacity, size_t *size, uint32 command_fields, uint32 response_field, uint32 initial_sectors_per_field, uint32 sectors_per_field, uint32 completion_fields, uint32 setup_field, uint32 fixed_buffer);

#endif /* RR_GAME_MAIN_H */
