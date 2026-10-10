#ifndef RR_MDEC_H
#define RR_MDEC_H

#include "psx.h"

sint32 mdec_stream_play(uint32 desc, sint32 (*callback)(void), sint32 mode);
sint32 mdec_init_stream(uint32 desc);
sint32 mdec_stop_stream(uint32 desc);
sint32 mdec_poll_stream_request(void);
sint32 mdec_decode_request(uint32 request);
sint32 mdec_wait_output(void);

sint32 mdec_get_output_buf_size(void);

sint32 mdec_scale_output_width(uint16 value);
sint32 mdec_get_output_depth(void);
void mdec_fn_8007361c(uint32 first, uint32 second, uint32 third);

#endif /* RR_MDEC_H */
