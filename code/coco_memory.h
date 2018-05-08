#if !defined(COCO_MEMORY_H)

struct mem_arena
{
    mm Size;
    mm Used;
    u8* Mem;
};

struct temp_mem
{
    mem_arena* Arena;
    mm Used;
};

struct mem_double_arena
{
    mm Size;
    mm UsedTop;
    mm UsedBot;
    u8* Mem;
};

struct temp_double_mem
{
    mem_double_arena* Arena;
    mm UsedTop;
    mm UsedBot;
};

#define COCO_MEMORY_H
#endif
