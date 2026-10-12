#ifndef PERF_H
#define PERF_H

#include "psx.h"

void perf_init(sint32 scale);
void perf_begin_frame(void);
sint16 perf_mark_frame(void);
sint16 perf_frame_end(void);
void perf_render_graph(uint32 *ot_entry, sint16 buffer);
void perf_render_cur(uint32 *ot_entry, sint16 buffer);

#endif /* PERF_H */
