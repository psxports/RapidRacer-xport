#include "arena.h"
#include "xport_trace.h"
#include <stdlib.h>

typedef struct ARENA_BLOCK
{
    struct ARENA_BLOCK *prev;
    void *data;
} ARENA_BLOCK;

typedef struct ARENA_MARK
{
    struct ARENA_MARK *prev;
    ARENA_BLOCK *block;
} ARENA_MARK;

static ARENA_BLOCK *arena_blocks;
static ARENA_MARK *arena_marks;
static uint32 arena_depth;

void arena_reset(void)
{
    FUNCTION_MARKER(0x8001EFE4, "MAIN.EXE");
    while (arena_marks)
    {
        ARENA_MARK *mark = arena_marks;
        arena_marks = mark->prev;
        free(mark);
    }
    arena_depth = 0;
}

uint32 arena_push(void)
{
    ARENA_MARK *mark;

    FUNCTION_MARKER(0x8001F02C, "MAIN.EXE");
    if (arena_depth == UINT16_MAX)
        abort();
    mark = (ARENA_MARK *)malloc(sizeof(*mark));
    if (!mark)
        abort();
    mark->prev = arena_marks;
    mark->block = arena_blocks;
    arena_marks = mark;
    return ++arena_depth * 4u;
}

sint32 arena_pop(void)
{
    ARENA_MARK *mark = arena_marks;

    FUNCTION_MARKER(0x8001F060, "MAIN.EXE");
    if (!mark)
        abort();
    while (arena_blocks != mark->block)
    {
        ARENA_BLOCK *block = arena_blocks;
        arena_blocks = block->prev;
        free(block->data);
        free(block);
    }
    arena_marks = mark->prev;
    free(mark);
    return --arena_depth;
}

void *arena_alloc(uint32 size)
{
    ARENA_BLOCK *block;
    void *data;

    FUNCTION_MARKER(0x8001F090, "MAIN.EXE");
    if (size > UINT32_MAX - 3u)
        return NULL;
    size = size + 3 & ~3u;
    data = malloc(size ? size : 4u);
    if (!data)
        return NULL;
    block = (ARENA_BLOCK *)malloc(sizeof(*block));
    if (!block)
    {
        free(data);
        return NULL;
    }
    block->data = data;
    block->prev = arena_blocks;
    arena_blocks = block;
    return data;
}
