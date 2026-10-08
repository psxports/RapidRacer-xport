#ifndef RR_RECORDS_H
#define RR_RECORDS_H

#include "psx.h"

sint32 rec_relocate_groups(uint32 structure, sint32 relocation);
sint32 records_relocate_pair_ptrs(uint32 structure, sint32 relocation);
sint32 rec_relocate_holder(uint32 holder, sint32 relocation);
sint32 rec_relocate_linked(uint32 holder, sint32 relocation);
sint32 rec_relocate_list_heads(uint32 structure, sint32 relocation);
sint32 rec_relocate_array(uint32 structure);
uint32 records_ptrs_relocate8(uint32 structure);

#endif /* RR_RECORDS_H */
