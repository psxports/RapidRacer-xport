#include "arena.h"
#include "xport_trace.h"
#include <stdlib.h>

uint32 game_reset_checkpoint_stack(void)
{
    uint32 result = r_u32(0x800A12A0u);

    FUNCTION_MARKER(0x8001EFE4u, "MAIN.EXE");
    w_u16(0x800B67C0u, 0u);
    w_u16(0x800B6BD0u, 0u);
    w_u32(0x800B7E20u, result);
    return result;
}

uint32 game_push_checkpoint(void)
{
    uint32 index;

    FUNCTION_MARKER(0x8001F02Cu, "MAIN.EXE");
    w_u16(0x800B67C0u, (uint16)(r_u16(0x800B67C0u) + 1u));
    index = (uint32)r_u16(0x800B67C0u) * 4u;
    w_u32(0x800B7E20u + index, r_u32(0x800A12A0u));
    return index;
}

sint32 game_pop_checkpoint(void)
{
    uint32 index = (uint32)r_u16(0x800B67C0u);
    uint32 cursor = r_u32(0x800B7E20u + index * 4u);

    FUNCTION_MARKER(0x8001F060u, "MAIN.EXE");
    w_u16(0x800B67C0u, (uint16)(index - 1u));
    w_u32(0x800A12A0u, cursor);
    return (sint32)index - 1;
}

uint32 game_alloc_arena_bytes(sint32 size)
{
    uint32 result = r_u32(0x800A12A0u);

    FUNCTION_MARKER(0x8001F090u, "MAIN.EXE");
    w_u32(0x800A12A0u, result + (((uint32)size + 3u) >> 2) * 4u);
    return result;
}

uint32 arena_activate_scope(void)
{
    FUNCTION_MARKER(0x8001F0B8u, "MAIN.EXE");
    w_u16(0x800B6BD0u, 1u);
    return r_u32(0x800A12A0u);
}

uint32 arena_release_aligned(sint32 size)
{
    uint32 result = r_u32(0x800A12A0u);

    FUNCTION_MARKER(0x8001F0D0u, "MAIN.EXE");
    w_u16(0x800B6BD0u, 0u);
    w_u32(0x800A12A0u, result + (((uint32)size + 3u) >> 2) * 4u);
    return result;
}

uint32 arena_alloc_aligned(uint32 size)
{
    uint32 result = r_u32(0x800A12A0u);

    FUNCTION_MARKER(0x800635B0u, "MAIN.EXE");
    if ((size & 3u) != 0u)
        size = (size + 4u) & 0xFFFFFFFCu;
    xport_update_u32(0x800A12A0u, XPORT_MEMORY_UPDATE_ADD, size);
    return result;
}
