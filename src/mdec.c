#include "display.h"
#include "vehicle_select.h"
#include "mdec.h"
#include "arena.h"
#include "cd.h"
#include "global.h"
#include "xport_trace.h"
#include <stdlib.h>

static sint32 mdec_poll_queue(uint32 *result, uint32 *record)
{
    uint32 queue_index = r_u32(0x800F4264u);
    uint32 entry = r_u32(0x800FF748u) + 32u * queue_index;
    if (r_u16(entry) == 1u)
    {
        w_u32(0x800F4264u, 0u);
        if (r_u32(0x800FF2F4u) != 0u)
            w_u16(entry, 0u);
        queue_index = r_u32(0x800F4264u);
        entry = r_u32(0x800FF748u) + 32u * queue_index;
    }
    if (r_u16(entry) != 2u)
        return 1;
    w_u16(entry, 4u);
    *result = r_u32(0x800FF748u) + 32u * r_u32(0x80101814u) + 2016u * queue_index;
    *record = entry;
    return 0;
}

static sint32 mdec_release_queue(uint32 request)
{
    sint32 queue_index = ((sint32)request - (sint32)(r_u32(0x800FF748u) + 32u * r_u32(0x80101814u))) / 2016;
    uint32 entry = r_u32(0x800FF748u) + 32u * (uint32)queue_index;
    sint32 count;
    sint32 index;

    if (r_u16(entry) != 4u)
        return 1;
    count = (sint16)r_u16(entry + 6u);
    for (index = 0; index < count; ++index)
        w_u16(r_u32(0x800FF748u) + 32u * (uint32)(queue_index + index), 0u);
    w_u32(0x800F4264u, (uint32)(queue_index + count));
    return 0;
}

sint32 mdec_stream_play(uint32 desc, sint32 (*callback)(void), sint32 mode)
{
    uint32 request = 0x800FF6E0u;
    sint32 retries;

    FUNCTION_MARKER(0x80062810u, "MAIN.EXE");
    if (r_u32(desc) == 0u)
        return 3;
    mdec_init_stream(desc);
    if (cd_retry_read_sequence(request) == 0)
    {
        mdec_stop_stream(desc);
        return 3;
    }
    for (retries = 0; retries < 5; ++retries)
    {
        if (mdec_poll_stream_request() != 0)
            break;
    }
    if (retries == 5)
    {
        mdec_stop_stream(desc);
        return 3;
    }
    mdec_decode_request(request);
    mdec_stop_stream(desc);
    return r_u16(0x800CD598u) != 0u ? 1 : 0;
}

sint32 mdec_init_stream(uint32 desc)
{
    sint16 width = (sint16)r_u16(desc + 8u);
    sint16 first_y = (sint16)r_u16(desc + 10u);
    sint16 second_y = (sint16)r_u16(desc + 12u);
    sint16 third_y = (sint16)r_u16(desc + 14u);
    sint16 height = (sint16)r_u16(desc + 16u);
    uint32 size;

    FUNCTION_MARKER(0x80062C14u, "MAIN.EXE");
    w_u32(0x800B6918u, r_u32(0x800A12A0u));
    global_fn_80068900(1u);
    w_u16(0x800CD568u, r_u16(desc + 4u));
    w_u16(0x800CD56Au, (uint16)width);
    w_u16(0x800CD56Cu, (uint16)first_y);
    w_u16(0x800CD56Eu, (uint16)(second_y + 8));
    w_u16(0x800CD570u, (uint16)third_y);
    w_u16(0x800CD572u, (uint16)height);
    w_u32(0x800CD574u, r_u32(desc + 20u));
    w_u16(0x800CD578u, r_u16(desc + 28u));
    w_u32(0x800CD57Cu, 0u);
    w_u16(0x800CD598u, 0u);
    w_u16(0x800CD59Au, 0u);
    w_u16(0x800CD59Cu, 0u);
    w_u16(0x800CD59Eu, 0u);
    w_u16(0x800CD5A0u, 0u);
    ResetGraph(0);
    w_u16(0x800CD5BCu, 0u);
    w_u16(0x800CD5BEu, 18u);
    w_u16(0x800CD5C0u, 0u);
    w_u16(0x800CD5C2u, 256u);
    w_u16(0x800CD5D0u, 0u);
    w_u16(0x800CD5D2u, 18u);
    w_u16(0x800CD5D4u, 0u);
    w_u16(0x800CD5D6u, 256u);
    w_u8(0x800CD5D9u, r_u16(desc + 4u) == 1u);
    w_u8(0x800CD5C5u, r_u8(0x800CD5D9u));
    if (height == 240)
    {
        display_clear_page_region(0, 0, 240, 512, 16, 0u, 0u, 0u);
        display_clear_page_region(1, 0, 240, 512, 16, 0u, 0u, 0u);
    }
    w_u16(0x800CD580u, (uint16)mdec_scale_output_width((uint16)first_y));
    w_u16(0x800CD582u, (uint16)(second_y + 8));
    w_u16(0x800CD584u, (uint16)mdec_scale_output_width((uint16)third_y));
    w_u16(0x800CD586u, (uint16)height);
    w_u16(0x800CD588u, (uint16)mdec_scale_output_width((uint16)first_y));
    w_u16(0x800CD58Au, (uint16)(second_y + 264));
    w_u16(0x800CD58Cu, (uint16)mdec_scale_output_width((uint16)third_y));
    w_u16(0x800CD58Eu, (uint16)height);
    w_u16(0x800CD594u, (uint16)mdec_scale_output_width(16u));
    size = r_u32(desc + 24u) != 0u ? r_u32(desc + 24u) : 132096u;
    w_u32(0x800CD5A4u, arena_alloc_aligned(size));
    w_u32(0x800CD5A8u, arena_alloc_aligned(size));
    size = (r_u16(0x800CD568u) != 0u ? 48u : 32u) * (uint32)(sint32)height;
    w_u32(0x800CD5ACu, arena_alloc_aligned(size));
    w_u32(0x800CD5B0u, arena_alloc_aligned(size));
    return 0;
}

sint32 mdec_stop_stream(uint32 desc)
{
    sint32 result = 512;

    FUNCTION_MARKER(0x80063074u, "MAIN.EXE");
    cd_clear_data_cbs();
    cd_adjust_audio_volume(0u);
    CdControlB(CdlStop, 0, 0);
    w_u32(0x800A12A0u, r_u32(0x800B6918u));
    if (r_u16(desc + 14u) == 512u)
    {
        result = r_u16(0x800CD568u);
        if (result != 0)
            result = vehicle_select_fn_800630f8();
    }
    return result;
}

sint32 mdec_poll_stream_request(void)
{
    uint32 result = 0u;
    uint32 record = 0u;
    uint32 attempts = 0x800000u;
    uint32 value;

    FUNCTION_MARKER(0x8006321Cu, "MAIN.EXE");
    do
    {
        --attempts;
        if (mdec_poll_queue(&result, &record) == 0)
            break;
    } while (attempts != 0u);
    if (attempts == 0u)
        return 0;
    xport_update_u32(0x800CD57Cu, XPORT_MEMORY_UPDATE_ADD, 1u);
    value = r_u32(record + 8u);
    if (r_u32(0x800CD574u) - 20u < value)
        w_u32(0x800B4268u, 1u);
    if (r_u32(0x800CD574u) < value || value < r_u32(0x800CD57Cu) || r_u16(record + 16u) != r_u16(0x800CD570u) || r_u16(record + 18u) != r_u16(0x800CD572u))
        w_u16(0x800CD598u, 1u);
    return (sint32)result;
}

sint32 mdec_decode_request(uint32 request)
{
    sint32 result = 0;

    FUNCTION_MARKER(0x80063320u, "MAIN.EXE");
    if (request != 0u)
    {
        xport_update_u16(0x800CD59Eu, XPORT_MEMORY_UPDATE_XOR, 1u);
        result = mdec_release_queue(request);
    }
    return result;
}

sint32 mdec_wait_output(void)
{
    uint32 attempts = 0x7FFFFFu;
    uint32 slot;
    sint32 result;

    FUNCTION_MARKER(0x80063388u, "MAIN.EXE");
    while (r_u16(0x800CD59Au) == 0u && attempts != 0u)
        --attempts;
    w_u16(0x800CD59Au, 0u);
    xport_update_u16(0x800CD59Cu, XPORT_MEMORY_UPDATE_XOR, 1u);
    slot = 0x800CD580u + 8u * r_u16(0x800CD59Cu);
    w_u16(0x800CD590u, r_u16(slot));
    result = r_u16(slot + 2u);
    w_u16(0x800CD592u, (uint16)result);
    return result;
}

sint32 mdec_get_output_buf_size(void)
{
    sint32 groups = ((sint32)r_u16(0x800CD572u) + 15) >> 4;

    FUNCTION_MARKER(0x800634E4u, "MAIN.EXE");
    return groups * (r_u16(0x800CD568u) != 0u ? 192 : 128);
}

sint32 mdec_scale_output_width(uint16 value)
{
    FUNCTION_MARKER(0x80063554u, "MAIN.EXE");
    if (r_u16(0x800CD568u) != 0u)
        return (sint32)(uint16)((3u * value) >> 1);
    return value;
}

sint32 mdec_get_output_depth(void)
{
    FUNCTION_MARKER(0x8006358Cu, "MAIN.EXE");
    return r_u16(0x800CD568u) != 0u ? 3 : 2;
}

void mdec_fn_8007361c(uint32 first, uint32 second, uint32 third)
{
    FUNCTION_MARKER(0x8007361Cu, "MAIN.EXE");
    w_u32(0x800FF700u, first);
    w_u32(0x800DE090u, second);
    w_u32(0x800FF2F4u, third);
}
