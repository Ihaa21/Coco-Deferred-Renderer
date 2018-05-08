#if !defined(COCO_ASSET_BUILDER_H)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "coco_platform.h"

#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#pragma pack(push, 1)
struct bitmap_header
{
    u16 FileType;
    u32 FileSize;
    u16 Reserved1;
    u16 Reserved2;
    u32 BitmapOffset;
    u32 Size;
    i32 Width;
    i32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 SizeOfBitmap;
    i32 HorzResolution;
    i32 VertResolution;
    u32 ColorsUsed;
    u32 ColorsImportant;

    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;
};
#pragma pack(pop)

struct work_queue;
#define WORK_QUEUE_CALLBACK(name) void name(void* Data)
typedef WORK_QUEUE_CALLBACK(work_queue_callback);
struct work_queue_entry
{
    work_queue_callback* Callback;
    void* Data;
    u32 volatile* DependancyCounter;
};

struct work_queue
{
    u32 volatile CompletionGoal;
    u32 volatile CompletionCount;

    u32 volatile NextEntryToWrite;
    u32 volatile NextEntryToMakeVisible;
    u32 volatile NextEntryToRead;

    HANDLE SemaphoreHandle;
    work_queue_entry Entries[1024];
};

struct work_queue_dependancy_entry
{
    work_queue_callback* Callback;
    void* Data;
    u32 volatile* DependancyCounter;
};

struct dependancy_work_queue
{
    u32 volatile CompletionGoal;
    u32 volatile CompletionCount;

    u32 volatile NextEntryToWrite;
    u32 volatile NextEntryToMakeVisible;
    u32 volatile NextEntryToRead;

    HANDLE SemaphoreHandle;
    work_queue_dependancy_entry Entries[1024];
};

struct win32_thread_info
{
    work_queue* Queue;
    dependancy_work_queue* DependancyQueue;
};

struct load_font_work
{
    char* FileName;
    file_font* Font;
    u32 MinGlyph;
    u32 MaxGlyph;
};

struct load_model_work
{
    char* FileName;
    char* MtlFileName;
    file_model* Model;
};

struct load_obj_work
{
    u32 NumMaterials;
    char** MtlNameArray;
    u32* MtlNameLengthArray;
    file_texture* TextureArray;
    
    char* ObjFileName;
    file_model* Model;
};

struct load_bitmap_work
{
    char* FileName;
    file_texture* Texture;
};

struct file_io_work
{
    u64 Size;
    u64 DataOffset;
    void* Data;
};

struct loaded_file
{
    u64 Size;
    u8* Data;
};

struct build_state
{
    file_font* FontArray;
    file_model* ModelArray;
    file_texture* TextureArray;
    
    // NOTE: Multiple producer, multiple consumer
    work_queue Queue;
    dependancy_work_queue DependancyQueue;
    // NOTE: Multiple producer, single consumer
    FILE* AssetFile;
    u64 volatile CurrDataOffset;
    work_queue FileIoQueue;
};

internal void AddFontAsset(char* FileName, u32 Id, u32 MinGlyph, u32 MaxGlyph);
internal void AddModelAsset(char* FileName, char* MtlFileName, u32 Id);
internal void AddLoadObjWork(char** MtlNameArray, u32* MtlNameLengthArray, file_texture* TextureArray,
                             u32 CurrMtlId, char* FileName, file_model* Model,
                             u32 volatile* DependancyCounter);
internal void AddTextureAsset(char* FileName, file_texture* Texture, u32 volatile* DependancyCounter = 0);
internal void AddTextureAsset(char* FileName, u32 Id);

#define COCO_ASSET_BUILDER_H
#endif
