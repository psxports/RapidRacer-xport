#include "records.h"
#include "xport_trace.h"
#include <stdlib.h>

sint32 rec_relocate_groups(uint32 structure, sint32 relocation)
{
    uint32 index;
    uint32 result;

    FUNCTION_MARKER(0x8001C2A4u, "MAIN.EXE");
    for (index = 2u; index <= 26u; index += 3u)
    {
        if (index == 8u || index == 13u)
        {
            if (r_u32(structure + index * 4u) + r_u32(structure + (index + 1u) * 4u) != 0u)
            {
                xport_update_u32(structure + (index + 2u) * 4u, XPORT_MEMORY_UPDATE_ADD, (uint32)relocation);
                if (r_u32(structure + index * 4u) != 0u)
                    xport_update_u32(structure + (index + 3u) * 4u, XPORT_MEMORY_UPDATE_ADD, (uint32)relocation);
                if (r_u32(structure + (index + 1u) * 4u) != 0u)
                    xport_update_u32(structure + (index + 4u) * 4u, XPORT_MEMORY_UPDATE_ADD, (uint32)relocation);
            }
            index += 2u;
        }
        else if (r_u32(structure + index * 4u) != 0u)
        {
            xport_update_u32(structure + (index + 1u) * 4u, XPORT_MEMORY_UPDATE_ADD, (uint32)relocation);
            xport_update_u32(structure + (index + 2u) * 4u, XPORT_MEMORY_UPDATE_ADD, (uint32)relocation);
        }
    }
    result = r_u32(structure + 108u);
    if (result != 0u)
    {
        uint32 last;

        result = r_u32(structure + 112u) + (uint32)relocation;
        last = r_u32(structure + 116u);
        last += (uint32)relocation;
        w_u32(structure + 112u, result);
        w_u32(structure + 116u, last);
    }
    return (sint32)result;
}

sint32 records_relocate_pair_ptrs(uint32 structure, sint32 relocation)
{
    sint32 result;

    FUNCTION_MARKER(0x8001C454u, "MAIN.EXE");
    result = (sint32)r_u32(structure + 12u) + relocation;
    w_u32(structure + 12u, (uint32)result);
    w_u32(structure + 16u, r_u32(structure + 16u) + (uint32)relocation);
    return result;
}

sint32 rec_relocate_holder(uint32 holder, sint32 relocation)
{
    uint32 structure;

    FUNCTION_MARKER(0x8001C470u, "MAIN.EXE");
    structure = r_u32(holder) + (uint32)relocation;
    w_u32(holder, structure);
    records_relocate_pair_ptrs(structure, relocation);
    if (r_u32(holder + 12u) != 0u)
        w_u32(holder + 12u, r_u32(holder + 12u) + (uint32)relocation);
    return (sint32)r_u32(holder + 12u);
}

sint32 rec_relocate_linked(uint32 holder, sint32 relocation)
{
    uint32 structure;
    uint32 next;

    FUNCTION_MARKER(0x8001C4CCu, "MAIN.EXE");
    structure = r_u32(holder) + (uint32)relocation;
    w_u32(holder, structure);
    rec_relocate_groups(structure, relocation);
    next = r_u32(holder + 4u);
    if (next != 0u)
        w_u32(holder + 4u, next + (uint32)relocation);
    return (sint32)r_u32(holder + 4u);
}

sint32 rec_relocate_list_heads(uint32 structure, sint32 relocation)
{
    uint32 holder;
    uint32 value;

    FUNCTION_MARKER(0x8001C528u, "MAIN.EXE");
    value = r_u32(structure + 16u);
    if (value != 0u)
    {
        holder = (uint32)relocation + value;
        w_u32(structure + 16u, holder);
        do
            holder = (uint32)rec_relocate_linked(holder, relocation);
        while (holder != 0u);
    }
    value = r_u32(structure + 20u);
    if (value != 0u)
    {
        holder = (uint32)relocation + value;
        w_u32(structure + 20u, holder);
        do
            holder = (uint32)rec_relocate_linked(holder, relocation);
        while (holder != 0u);
    }
    value = r_u32(structure + 24u);
    if (value != 0u)
    {
        holder = (uint32)relocation + value;
        w_u32(structure + 24u, holder);
        do
            holder = (uint32)rec_relocate_holder(holder, relocation);
        while (holder != 0u);
    }
    value = r_u32(structure + 28u);
    holder = (uint32)relocation + value;
    if (value == 0u)
        return (sint32)holder;
    w_u32(structure + 28u, holder);
    do
        holder = (uint32)rec_relocate_holder(holder, relocation);
    while (holder != 0u);
    return (sint32)holder;
}

sint32 rec_relocate_array(uint32 structure)
{
    uint32 records;
    uint32 offset = 4u;
    uint32 index = 1u;
    uint8 count;

    FUNCTION_MARKER(0x8001C5F8u, "MAIN.EXE");
    records = r_u32(structure + 4u);
    count = r_u8(records + 1u);
    if (count == 0u)
        return 0;
    do
    {
        uint32 relocation = r_u32(structure + 8u);

        rec_relocate_list_heads(records + offset, (sint32)relocation);
        records = r_u32(structure + 4u);
        count = r_u8(records + 1u);
        ++index;
        offset += 32u;
    } while (index <= (uint32)count);
    return 1;
}

uint32 records_ptrs_relocate8(uint32 structure)
{
    uint32 first;
    uint32 second;

    FUNCTION_MARKER(0x8001C670u, "MAIN.EXE");
    first = r_u32(structure + 32u);
    second = r_u32(structure + 36u);
    w_u32(structure + 32u, structure + first);
    first = r_u32(structure + 40u);
    w_u32(structure + 36u, structure + second);
    second = r_u32(structure + 44u);
    w_u32(structure + 40u, structure + first);
    first = r_u32(structure + 48u);
    w_u32(structure + 44u, structure + second);
    second = r_u32(structure + 52u);
    w_u32(structure + 48u, structure + first);
    first = r_u32(structure + 56u);
    w_u32(structure + 52u, structure + second);
    second = r_u32(structure + 60u);
    w_u32(structure + 56u, structure + first);
    w_u32(structure + 60u, structure + second);
    return structure;
}
