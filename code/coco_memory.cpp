//
// NOTE: Thread safe memory allocator
//

struct mem_block
{
    mem_block* Next;
    mem_block* Prev;
};

#define MEM_POOL_BLOCK_SIZE (MegaBytes(10))
struct mem_pool_arena
{
    u64 Size;
    u64 NumBlocks;
    void* Base;
    
    mem_block FreeSentinel;
};

inline mem_pool_arena InitPoolArena(u64 Size, void* Base)
{
    mem_pool_arena Arena = {};
    Arena.Size = Size;
    Arena.Base = Base;

    // NOTE: Build the free list
    Assert(IsDivisible(Size, MEM_POOL_BLOCK_SIZE + sizeof(mem_block)));
    Arena.NumBlocks = Size / (MEM_POOL_BLOCK_SIZE + sizeof(mem_block));
    u8* CurrByte = (u8*)Arena.Base;

    Arena.FreeSentinel.Prev = &Arena.FreeSentinel;
    Arena.FreeSentinel.Next = &Arena.FreeSentinel;

    mem_block* CurrBlock = (mem_block*)Arena.Base;
    for (u64 BlockId = 0; BlockId < Arena.NumBlocks; ++BlockId)
    {
        CurrBlock->Prev = &Arena.FreeSentinel;
        CurrBlock->Next = Arena.FreeSentinel.Next;

        CurrBlock->Prev->Next = CurrBlock;
        CurrBlock->Next->Prev = CurrBlock;

        CurrBlock += 1;
    }

    return Arena;
}

internal void* PushSize(mem_pool_arena* Arena, u64 Size)
{
    void* Result = 0;
    
    if (Size > (MEM_POOL_BLOCK_SIZE - sizeof(mem_block)))
    {
        InvalidCodePath;
    }
    else
    {
        // NOTE: All blocks are taken, out of memory
        Assert(Arena->FreeSentinel.Next != &Arena->FreeSentinel);

        mem_block* Block = Arena->FreeSentinel.Next;
        u64 BlockIndex = ((u64)Block - (u64)Arena->Base) / sizeof(mem_block);
        u64 BlockMemOffset = Arena->NumBlocks*sizeof(mem_block) + BlockIndex*MEM_POOL_BLOCK_SIZE;
        Result = (void*)((u8*)Arena->Base + BlockMemOffset);

        Block->Prev->Next = Block->Next;
        Block->Next->Prev = Block->Prev;

        Block->Prev = 0;
        Block->Next = 0;
    }

    return Result;
}

internal void Free(mem_pool_arena* Arena, void* Data)
{
    u64 BlockMemOffset = (u64)Data - (u64)Arena->Base;
    u64 BlockId = (BlockMemOffset - Arena->NumBlocks*sizeof(mem_block)) / MEM_POOL_BLOCK_SIZE;

    mem_block* Block = (mem_block*)((u8*)Arena->Base + sizeof(mem_block)*BlockId);
    Block->Prev = &Arena->FreeSentinel;
    Block->Next = Arena->FreeSentinel.Next;

    Block->Prev->Next = Block;
    Block->Next->Prev = Block;
}

//
// NOTE: Linear allocator
// 

inline mem_arena InitArena(void* Mem, mm Size)
{
    mem_arena Result = {};
    Result.Size = Size;
    Result.Used = 0;
    Result.Mem = (u8*)Mem;

    return Result;
}

inline mem_double_arena InitDoubleArena(void* Mem, mm Size)
{
    mem_double_arena Result = {};
    Result.Size = Size;
    Result.UsedTop = 0;
    Result.UsedBot = 0;
    Result.Mem = (u8*)Mem;

    return Result;
}

inline void ClearArena(mem_arena* Arena)
{
    Arena->Used = 0;
}

inline void ClearArena(mem_double_arena* Arena)
{
    Arena->UsedTop = 0;
    Arena->UsedBot = 0;
}

inline temp_mem BeginTempMem(mem_arena* Arena)
{
    // NOTE: This function lets us take all memory allocated past this point and later
    // free it
    temp_mem TempMem = {};
    TempMem.Arena = Arena;
    TempMem.Used = Arena->Used;

    return TempMem;
}

inline void EndTempMem(temp_mem TempMem)
{
    TempMem.Arena->Used = TempMem.Used;
}

inline temp_double_mem BeginTempMem(mem_double_arena* Arena)
{
    // NOTE: This function lets us take all memory allocated past this point and later
    // free it
    temp_double_mem TempMem = {};
    TempMem.Arena = Arena;
    TempMem.UsedTop = Arena->UsedTop;
    TempMem.UsedBot = Arena->UsedBot;

    return TempMem;
}

inline void EndTempMem(temp_double_mem TempMem)
{
    TempMem.Arena->UsedTop = TempMem.UsedTop;
    TempMem.Arena->UsedBot = TempMem.UsedBot;
}

#define PushStruct(Arena, type) (type*)PushSize(Arena, sizeof(type))
#define PushArray(Arena, type, count) (type*)PushSize(Arena, sizeof(type)*count)
inline void* PushSize(mem_arena* Arena, mm Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Mem + Arena->Used;
    Arena->Used += Size;
    
    return Result;
}

inline void* PushSize(mem_double_arena* Arena, mm Size)
{
    Assert(Arena->UsedTop + Arena->UsedBot + Size <= Arena->Size);
    void* Result = Arena->Mem + Arena->UsedTop;
    Arena->UsedTop += Size;
    
    return Result;
}

#define BotPushStruct(Arena, type) (type*)BotPushSize(Arena, sizeof(type))
#define BotPushArray(Arena, type, count) (type*)BotPushSize(Arena, sizeof(type)*count)
inline void* BotPushSize(mem_double_arena* Arena, mm Size)
{
    Assert(Arena->UsedTop + Arena->UsedBot + Size <= Arena->Size);
    Arena->UsedBot += Size;
    void* Result = Arena->Mem + Arena->Size - Arena->UsedBot;
    
    return Result;
}

inline mem_arena SubArena(mem_arena* Arena, mm Size)
{
    mem_arena Result = {};
    Result.Size = Size;
    Result.Used = 0;
    Result.Mem = (u8*)PushSize(Arena, Size);

    return Result;
}

inline void ClearMem(void* Mem, mm Size)
{
    u8* CurrByte = (u8*)Mem;
    for (mm Byte = 0; Byte < Size; ++Byte)
    {
        *CurrByte++ = 0;
    }
}

inline void Copy(void* Mem, void* Dest, mm Size)
{
    u8* CurrentByte = (u8*)Mem;
    u8* DestByte = (u8*)Dest;
    for (mm Byte = 0; Byte < Size; ++Byte)
    {
        *DestByte++ = *CurrentByte++;
    }
}

inline void ZeroMem(void* Mem, mm NumBytes)
{
    u8* ByteMem = (u8*)Mem;
    for (mm ByteId = 0; ByteId < NumBytes; ++ByteId)
    {
        *ByteMem = 0;
        ByteMem++;
    }
}

#define ShiftPtrByBytes(Ptr, Step, Type) (Type*)ShiftPtrByBytes_((u8*)Ptr, Step)
inline u8* ShiftPtrByBytes_(u8* Ptr, mm Step)
{
    u8* Result = Ptr + Step;
    return Result;
}
