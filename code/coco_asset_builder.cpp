#include <windows.h>
#include <intrin.h>

#include "coco_asset_builder.h"
#include "coco_string.h"

global build_state State;

//
// NOTE: Helper functions
//

inline u32 GetThreadId()
{
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 Result = *(u32 *)(ThreadLocalStorage + 0x48);
    
    return Result;
}

inline char* CombineStrings(char* Str1, char* Str2, u32 Str2Size)
{
    u32 Str1Len = (u32)strlen(Str1);
    u32 Str2Len = Str2Size;
    
    char* Result = (char*)malloc(Str1Len + Str2Len + 1);
    strcpy(Result, Str1);
    strcpy(Result + Str1Len, Str2);
    Result[Str1Len + Str2Len] = 0;

    return Result;
}

inline char* CombineStrings(char* Str1, char* Str2)
{
    char* Result = CombineStrings(Str1, Str2, (u32)strlen(Str2));
    
    return Result;
}

inline void AllocAndCopyString(char* Str, char** Dest)
{
    mm StrSize = (strlen(Str) + 1)*sizeof(char);
    *Dest = (char*)malloc(StrSize);
    Copy(Str, *Dest, StrSize);
    (*Dest)[StrSize-1] = 0;
}

inline loaded_file LoadFile(char* FilePath)
{
    loaded_file Result = {};
    
    FILE* File = fopen(FilePath, "rb");
    if (!File)
    {
        InvalidCodePath;
    }
    
    fseek(File, 0, SEEK_END);
    Result.Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    Assert(Result.Size > 0);
    
    Result.Data = (u8*)malloc(Result.Size);
    fread(Result.Data, Result.Size, 1, File);
    fclose(File);

    return Result;
}

//
// NOTE: Work queue
//

inline void Win32AddWorkQueueEntry(work_queue* Queue, work_queue_callback* Callback, void* Data,
                                   u32 volatile* DependancyCounter = 0)
{
    u32 OrigNextEntryToWrite;
    u32 NewNextEntryToWrite;
    for (;;)
    {
        OrigNextEntryToWrite = Queue->NextEntryToWrite;
        NewNextEntryToWrite = ((OrigNextEntryToWrite + 1) % ArrayCount(Queue->Entries));
        u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToWrite,
                                               NewNextEntryToWrite, OrigNextEntryToWrite);
        if (Index == OrigNextEntryToWrite)
        {
            break;
        }
    }

    _WriteBarrier();
    
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);

    work_queue_entry* Entry = Queue->Entries + OrigNextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;

    if (DependancyCounter)
    {
        Entry->DependancyCounter = DependancyCounter;
        InterlockedIncrement((LONG volatile*)DependancyCounter);
    }
    
    _WriteBarrier();
    InterlockedIncrement((LONG volatile*)&Queue->CompletionGoal);

    for (;;)
    {
        u32 OrigNextEntryToMakeVisible = Queue->NextEntryToMakeVisible;
        if (OrigNextEntryToMakeVisible == OrigNextEntryToWrite)
        {
            u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToMakeVisible,
                                                   NewNextEntryToWrite, OrigNextEntryToMakeVisible);
            if (Index == OrigNextEntryToMakeVisible)
            {
                break;
            }
        }
    }

    _WriteBarrier();
    
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal b32 Win32DoNextWorkQueueEntry(work_queue* Queue)
{
    b32 ShouldSleep = false;

    u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToMakeVisible)
    {
        u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToRead,
                                               NewNextEntryToRead, OriginalNextEntryToRead);
        if (Index == OriginalNextEntryToRead)
        {
            u32 ThreadId = GetThreadId();
            {
                char Buffer[256];
                wsprintf(Buffer, "Thread %u has index %u\n", ThreadId, Index);
                OutputDebugString(Buffer);
            }
            work_queue_entry Entry = Queue->Entries[Index];
            //Entry.Callback(Queue, Entry.Data);
            Entry.Callback(Entry.Data);

            _WriteBarrier();

            // NOTE: If a job is waiting on this one, we let it know we finished
            if (Entry.DependancyCounter)
            {
                InterlockedDecrement((LONG volatile*)Entry.DependancyCounter);
            }
            
            InterlockedIncrement((LONG volatile*)&Queue->CompletionCount);
        }
    }
    else
    {
        // NOTE: We sleep if next entry to do is equal to entry count
        ShouldSleep = true;
    }

    return ShouldSleep;
}

internal void Win32CompleteAllWork(work_queue* Queue)
{
    while (Queue->CompletionGoal != Queue->CompletionCount)
    {
        Win32DoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

internal void Win32WaitForAllWork(work_queue* Queue)
{
    while (Queue->CompletionGoal != Queue->CompletionCount)
    {
    }
}

//
// NOTE: Dependancy work queue
//

inline void Win32AddWorkQueueDependancyEntry(dependancy_work_queue* Queue,
                                             work_queue_callback* Callback,
                                             void* Data, u32 volatile* DependancyCounter)
{
    u32 OrigNextEntryToWrite;
    u32 NewNextEntryToWrite;
    for (;;)
    {
        OrigNextEntryToWrite = Queue->NextEntryToWrite;
        NewNextEntryToWrite = ((OrigNextEntryToWrite + 1) % ArrayCount(Queue->Entries));
        u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToWrite,
                                               NewNextEntryToWrite, OrigNextEntryToWrite);
        if (Index == OrigNextEntryToWrite)
        {
            break;
        }
    }

    _WriteBarrier();
    
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);

    work_queue_dependancy_entry* Entry = Queue->Entries + OrigNextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    Entry->DependancyCounter = DependancyCounter;
    
    _WriteBarrier();
    InterlockedIncrement((LONG volatile*)&Queue->CompletionGoal);

    for (;;)
    {
        u32 OrigNextEntryToMakeVisible = Queue->NextEntryToMakeVisible;
        if (OrigNextEntryToMakeVisible == OrigNextEntryToWrite)
        {
            u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToMakeVisible,
                                                   NewNextEntryToWrite, OrigNextEntryToMakeVisible);
            if (Index == OrigNextEntryToMakeVisible)
            {
                break;
            }
        }
    }

    _WriteBarrier();
    
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal b32 Win32DoNextDependancyWorkQueueEntry(dependancy_work_queue* Queue)
{
    b32 ShouldSleep = false;

    u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextEntryToRead != Queue->NextEntryToMakeVisible &&
        *Queue->Entries[OriginalNextEntryToRead].DependancyCounter == 0)
    {
        u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToRead,
                                               NewNextEntryToRead, OriginalNextEntryToRead);
        if (Index == OriginalNextEntryToRead)
        {
            u32 ThreadId = GetThreadId();
            {
                char Buffer[256];
                wsprintf(Buffer, "Thread %u has index %u\n", ThreadId, Index);
                OutputDebugString(Buffer);
            }
            work_queue_dependancy_entry Entry = Queue->Entries[Index];
            //Entry.Callback(Queue, Entry.Data);
            Entry.Callback(Entry.Data);

            _WriteBarrier();
            
            InterlockedIncrement((LONG volatile*)&Queue->CompletionCount);
        }
    }
    else
    {
        // NOTE: We sleep if next entry to do is equal to entry count
        ShouldSleep = true;
    }

    return ShouldSleep;
}

internal void Win32CompleteAllWork(dependancy_work_queue* Queue)
{
    while (Queue->CompletionGoal != Queue->CompletionCount)
    {
        Win32DoNextDependancyWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

//
// NOTE: Work queue thread procs
//

DWORD WINAPI DependancyThreadProc(LPVOID lpParameter)
{
    win32_thread_info* ThreadInfo = (win32_thread_info*)lpParameter;

    for (;;)
    {
        if (Win32DoNextDependancyWorkQueueEntry(ThreadInfo->DependancyQueue) &&
            Win32DoNextWorkQueueEntry(ThreadInfo->Queue))
        {
            // NOTE: Thread goes to sleep
            WaitForSingleObjectEx(ThreadInfo->Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    win32_thread_info* ThreadInfo = (win32_thread_info*)lpParameter;

    for (;;)
    {
        if (Win32DoNextWorkQueueEntry(ThreadInfo->Queue))
        {
            // NOTE: Thread goes to sleep
            WaitForSingleObjectEx(ThreadInfo->Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

//
// NOTE: File Io Queue
//

internal WORK_QUEUE_CALLBACK(DoFileIoWork)
{
    file_io_work Work = *(file_io_work*)Data;
    free(Data);
    
    fseek(State.AssetFile, (u32)Work.DataOffset, SEEK_SET);
    fwrite(Work.Data, Work.Size, 1, State.AssetFile);
}

inline void AddFileIoEntry(file_io_work Entry)
{
    file_io_work* Work = (file_io_work*)malloc(sizeof(file_io_work));
    *Work = Entry;

    Win32AddWorkQueueEntry(&State.FileIoQueue, DoFileIoWork, Work);
}

inline u64 GetAssetFileDataOffset(u64 DataSize)
{
    u64 Result = 0;
    
    while (true)
    {
        u64 OriginalDataOffset = State.CurrDataOffset;
        u64 NewDataOffset = OriginalDataOffset + DataSize;
        u64 FoundDataOffset = InterlockedCompareExchange64((LONGLONG volatile*)&State.CurrDataOffset,
                                                           NewDataOffset, OriginalDataOffset);
        if (FoundDataOffset == OriginalDataOffset)
        {
            // NOTE: We modified the value correctly
            Result = OriginalDataOffset;
            break;
        }
    }

    return Result;
}

//
// NOTE: Load asset jobs
//

internal WORK_QUEUE_CALLBACK(DoLoadFontWork)
{
    load_font_work Work = *(load_font_work*)Data;
    free(Data);
    
    file_font* Font = Work.Font;
    file_io_work FileIoWork = {};

    char* FullPath = CombineStrings("c:/windows/fonts/", Work.FileName);
    loaded_file File = LoadFile(FullPath);
    free(FullPath);
    free(Work.FileName);

    stbtt_fontinfo FontInfo;
    stbtt_InitFont(&FontInfo, (u8*)File.Data, stbtt_GetFontOffsetForIndex((u8*)File.Data, 0));
    f32 ScaleFactor = stbtt_ScaleForPixelHeight(&FontInfo, 128.0f);

    stbtt_GetFontVMetrics(&FontInfo, &Font->MaxAscent, &Font->MaxDescent, &Font->LineGap);
    Font->MaxAscent = (i32)((f32)Font->MaxAscent * ScaleFactor);
    Font->MaxDescent = (i32)((f32)Font->MaxDescent * ScaleFactor);
    Font->LineGap = (i32)((f32)Font->LineGap * ScaleFactor);
    Font->MinGlyph = Work.MinGlyph;
    Font->MaxGlyph = Work.MaxGlyph;

    // NOTE: Figure out how much memory we are going to need
    u32 NumGlyphsToLoad = Font->MaxGlyph - Font->MinGlyph;
    u32 StepArraySize = NumGlyphsToLoad*sizeof(f32);
    u32 KernArraySize = Square(NumGlyphsToLoad)*sizeof(f32);
    FileIoWork.Size = NumGlyphsToLoad*sizeof(file_glyph) + KernArraySize + StepArraySize;
    for (u32 BitmapIndex = 0, CodePoint = Font->MinGlyph;
         CodePoint < Font->MaxGlyph; ++BitmapIndex, ++CodePoint)
    {
        i32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&FontInfo, CodePoint, ScaleFactor, ScaleFactor, &x0, &y0,
                                    &x1, &y1);
        
        FileIoWork.Size += (x1-x0)*(y1-y0)*sizeof(u32);
    }

    FileIoWork.DataOffset = GetAssetFileDataOffset(FileIoWork.Size);
    Font->StepArrayOffset = FileIoWork.DataOffset;
    Font->KernArrayOffset = FileIoWork.DataOffset + StepArraySize;
    Font->GlyphArrayOffset = FileIoWork.DataOffset + KernArraySize + StepArraySize;
    FileIoWork.Data = malloc(FileIoWork.Size);

    // NOTE: Load step array
    f32* StepData = (f32*)FileIoWork.Data;
    for (u32 Glyph = Work.MinGlyph; Glyph < Work.MaxGlyph; ++Glyph)
    {
        i32 Step;
        stbtt_GetCodepointHMetrics(&FontInfo, Glyph, &Step, 0);
        StepData[Glyph - Work.MinGlyph] = ScaleFactor*(f32)Step;
    }
    
    // NOTE: Load kerning array
    u32 KernDim = Work.MaxGlyph - Work.MinGlyph;
    f32* KernData = ShiftPtrByBytes(FileIoWork.Data, StepArraySize, f32);
    for (u32 Glyph1 = Work.MinGlyph; Glyph1 < Work.MaxGlyph; ++Glyph1)
    {
        for (u32 Glyph2 = Work.MinGlyph; Glyph2 < Work.MaxGlyph; ++Glyph2)
        {            
            u32 ArrayIndex = (Glyph2 - Work.MinGlyph)*KernDim + (Glyph1 - Work.MinGlyph);
            i32 t = stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2);
            KernData[ArrayIndex] =  (ScaleFactor*
                                     (f32)stbtt_GetCodepointKernAdvance(&FontInfo, Glyph1, Glyph2));
        }
    }

    // NOTE: Load the glyphs and their texture vars
    file_glyph* CurrentGlyph = ShiftPtrByBytes(FileIoWork.Data, StepArraySize + KernArraySize, file_glyph);
    u8* PixelMem = ShiftPtrByBytes(FileIoWork.Data, KernArraySize + sizeof(file_glyph)*NumGlyphsToLoad + StepArraySize, u8);
    mm CurrentByte = (FileIoWork.DataOffset + StepArraySize + KernArraySize + sizeof(file_glyph)*NumGlyphsToLoad);
    for (u32 CodePoint = Font->MinGlyph; CodePoint < Font->MaxGlyph;
         ++CodePoint, ++CurrentGlyph)
    {
        i32 Width, Height, XOffset, YOffset;
        u8 *MonoBitmap = stbtt_GetCodepointBitmap(&FontInfo, 0, ScaleFactor, CodePoint, &Width, &Height,
                                                  &XOffset, &YOffset);
        
        i32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&FontInfo, CodePoint, ScaleFactor, ScaleFactor, &x0, &y0, &x1, &y1);
        *CurrentGlyph = {};
        CurrentGlyph->Width = Width;
        CurrentGlyph->Height = Height;
        CurrentGlyph->AlignPos = V2i(x0, y0) + 0.5f*V2i(x1 - x0, y1 - y0);
        CurrentGlyph->PixelOffset = CurrentByte;

        Assert(x1 - x0 == Width);
        Assert(y1 - y0 == Height);
        
        u8* SourcePixels = MonoBitmap;
        u8* DestRow = PixelMem + sizeof(u32)*Width*(Height - 1);
        for (i32 Y = 0; Y < Height; ++Y)
        {
            u32* DestPixel = (u32*)DestRow;
        
            for (i32 X = 0; X < Width; ++X)
            {
                u8 Gray = *SourcePixels++;
                                
                v4 Texel = SRGBToLinear(V4(Gray, Gray, Gray, Gray));
                Texel.rgb *= Texel.a;
                Texel = LinearToSRGB(Texel);
                
                *DestPixel++ = (((u32)(Texel.a + 0.5f) << 24) |
                                ((u32)(Texel.r + 0.5f) << 16) |
                                ((u32)(Texel.g + 0.5f) << 8) |
                                ((u32)(Texel.b + 0.5f)));
            }

            DestRow -= Width*sizeof(u32);
        }
        
        stbtt_FreeBitmap(MonoBitmap, 0);
        PixelMem += sizeof(u32)*CurrentGlyph->Width*CurrentGlyph->Height;
        CurrentByte += sizeof(u32)*CurrentGlyph->Width*CurrentGlyph->Height;
    }

    AddFileIoEntry(FileIoWork);
}

inline void GetPastWhiteSpaceAndSlashes(char** StrPtr)
{
    char* Str = *StrPtr;
    while (IsWhiteSpace(*Str) || *Str == '/')
    {
        Str += 1;
    }

    *StrPtr = Str;
}

inline void GetPastWhiteSpace(char** StrPtr)
{
    char* Str = *StrPtr;
    while (IsWhiteSpace(*Str))
    {
        Str += 1;
    }

    *StrPtr = Str;
}

inline void GetPastWhiteSpaceNewLine(char** StrPtr)
{
    char* Str = *StrPtr;
    while (IsWhiteSpace(*Str) || *Str == '\n')
    {
        Str += 1;
    }

    *StrPtr = Str;
}

inline void SkipPastNewLine(char** StrPtr, loaded_file File)
{
    char* Str = *StrPtr;
    while (*Str != '\n')
    {
        Str += 1;
        if ((u64)(Str - (char*)File.Data) >= File.Size)
        {
            break;
        }
    }

    Str += 1;
    *StrPtr = Str;
}

inline void GetToNewLine(char** StrPtr)
{
    char* Str = *StrPtr;
    while (*Str != '\n' && *Str != '\r')
    {
        Str += 1;
    }

    *StrPtr = Str;
}

internal WORK_QUEUE_CALLBACK(DoLoadModelWork)
{
    load_model_work Work = *(load_model_work*)Data;
    free(Data);
    
    file_model* Model = Work.Model;
    file_io_work FileIoWork = {};
    
    char* FullPathMtl = CombineStrings("models/", Work.MtlFileName);
    loaded_file FileMtl = LoadFile(FullPathMtl);
    free(FullPathMtl);
    free(Work.MtlFileName);

    u32 volatile* DependancyCounter = (u32 volatile*)malloc(sizeof(u32 volatile));
    *DependancyCounter = 0;

    _WriteBarrier();
    
    // IMPORTANT: We over allocate here because we don't know how many vertices and meshes we will have.
    // We only push onto the asset file as many as are actually used.
#define MAX_NUM_TEXTURES 40
    u32 CurrMtlId = 0;
    char** MtlNameArray = (char**)malloc(sizeof(char*)*MAX_NUM_TEXTURES);
    u32* MtlNameLengthArray = (u32*)malloc(sizeof(u32)*MAX_NUM_TEXTURES);
    file_texture* TextureArray = (file_texture*)malloc(sizeof(file_texture)*MAX_NUM_TEXTURES);

    // NOTE: Load all the textures and process their data + set offsets
    {
        char* CurrChar = (char*)FileMtl.Data;
        while ((u64)(CurrChar - (char*)FileMtl.Data) < FileMtl.Size)
        {
            GetPastWhiteSpace(&CurrChar);
        
            if (CurrChar[0] == 'n' && CurrChar[1] == 'e' && CurrChar[2] == 'w' && CurrChar[3] == 'm' &&
                CurrChar[4] == 't' && CurrChar[5] == 'l')
            {
                // NOTE: We have a new mtl, add its name to our array of names
                CurrChar += 6;
                GetPastWhiteSpace(&CurrChar);

                MtlNameArray[CurrMtlId] = CurrChar;

                char* CurrPos = CurrChar;
                GetToNewLine(&CurrChar);
                MtlNameLengthArray[CurrMtlId] = (u32)(CurrChar - CurrPos);
                
                SkipPastNewLine(&CurrChar, FileMtl);
            }
            else if (CurrChar[0] == 'm' && CurrChar[1] == 'a' && CurrChar[2] == 'p' &&
                     CurrChar[3] == '_' && CurrChar[4] == 'K' && CurrChar[5] == 'a')
            {
                CurrChar += 6;
                GetPastWhiteSpace(&CurrChar);

                // NOTE: We get rid of the textures/ in the path
                CurrChar += 9;
                
                u32 PathCharId = 0;
                char FilePath[256] = {};
                while (!IsWhiteSpace(*CurrChar))
                {
                    FilePath[PathCharId++] = *CurrChar;
                    CurrChar += 1;
                }
                
                AddTextureAsset(FilePath, TextureArray + CurrMtlId, DependancyCounter);
                GetPastWhiteSpaceNewLine(&CurrChar);
            
                CurrMtlId += 1;
            }
            else
            {
                SkipPastNewLine(&CurrChar, FileMtl);
            }
        }
    }

    // NOTE: We pass the texture array and the file name arrays into the next job so that we can
    // reference those textures when building our meshes.

    AddLoadObjWork(MtlNameArray, MtlNameLengthArray, TextureArray, CurrMtlId, Work.FileName, Model,
                   DependancyCounter);
}

internal WORK_QUEUE_CALLBACK(DoLoadObjWork)
{
    load_obj_work Work = *(load_obj_work*)Data;
    free(Data);
    
    char* FullPathObj = CombineStrings("models/", Work.ObjFileName);
    loaded_file FileObj = LoadFile(FullPathObj);
    free(FullPathObj);
    
    file_model* Model = Work.Model;
    Model->NumTextures = Work.NumMaterials;
    u32 CurrTextureId = 0;
    
    // IMPORTANT: We over allocate here because we don't know how many vertices and meshes we will have.
    // We only push onto the asset file as many as are actually used.
#define MAX_NUM_MESHES 1000
#define MAX_NUM_VERTICES 500000
#define MAX_NUM_FACES 10000000

    file_mesh* MeshArray = (file_mesh*)malloc(sizeof(file_mesh)*MAX_NUM_MESHES);
    ClearMem(MeshArray, sizeof(file_mesh)*1000);

    file_mesh* CurrMesh = MeshArray;
    CurrMesh->VertexArrayOffset = 0;
    
    u32 CurrPos = 0;
    v3* PosArray = (v3*)malloc(sizeof(v3)*MAX_NUM_VERTICES);

    u32 CurrUV = 0;
    v2* UVArray = (v2*)malloc(sizeof(v3)*MAX_NUM_VERTICES);

    u32 CurrNormal = 0;
    v3* NormalArray = (v3*)malloc(sizeof(v3)*MAX_NUM_VERTICES);

    u32 CurrVertexNum = 0;
    u32 CurrMeshVertexNum = 0;
    file_vertex* VertexArray = (file_vertex*)malloc(sizeof(file_vertex)*MAX_NUM_FACES);
    file_vertex* CurrVertex = VertexArray;

    char* CurrChar = (char*)FileObj.Data;
    while ((u64)(CurrChar - (char*)FileObj.Data) < FileObj.Size)
    {
        if (CurrChar[0] == 'v' && CurrChar[1] == 't')
        {
            // NOTE: We have a uv coord
            CurrChar += 2;
            GetPastWhiteSpace(&CurrChar);
            
            f32 Val1, Val2;
            
            {
                GetFloatFromStr(&CurrChar, &Val1);
                GetPastWhiteSpace(&CurrChar);
            }

            {
                GetFloatFromStr(&CurrChar, &Val2);
                GetPastWhiteSpace(&CurrChar);
            }

            GetToNewLine(&CurrChar);
            CurrChar += 1;
            
            v2 UV = V2(Val1, Val2);
            UVArray[CurrUV++] = UV;
        }
        else if (CurrChar[0] == 'v' && CurrChar[1] == 'n')
        {
            // NOTE: We have a normal
            CurrChar += 2;
            GetPastWhiteSpace(&CurrChar);

            f32 Val1, Val2, Val3;

            {
                GetFloatFromStr(&CurrChar, &Val1);
                GetPastWhiteSpace(&CurrChar);
            }

            {
                GetFloatFromStr(&CurrChar, &Val2);
                GetPastWhiteSpace(&CurrChar);
            }

            {
                GetFloatFromStr(&CurrChar, &Val3);
                GetPastWhiteSpace(&CurrChar);
            }

            GetPastWhiteSpaceNewLine(&CurrChar);

            v3 Normal = V3(Val1, Val2, Val3);
            NormalArray[CurrNormal++] = Normal;
        }
        else if (CurrChar[0] == 'v')
        {
            // NOTE: We have a position
            CurrChar += 1;
            GetPastWhiteSpace(&CurrChar);
            
            f32 Val1, Val2, Val3;

            {
                GetFloatFromStr(&CurrChar, &Val1);
                GetPastWhiteSpace(&CurrChar);
            }

            {
                GetFloatFromStr(&CurrChar, &Val2);
                GetPastWhiteSpace(&CurrChar);
            }

            {
                GetFloatFromStr(&CurrChar, &Val3);
                GetPastWhiteSpace(&CurrChar);
            }

            GetPastWhiteSpaceNewLine(&CurrChar);

            v3 Pos = V3(Val1, Val2, Val3);
            PosArray[CurrPos++] = Pos;
        }
        else if (CurrChar[0] == 'f')
        {
            // NOTE: We get indicies and bulid the output array
            CurrChar += 1;
            GetPastWhiteSpace(&CurrChar);

            if (CurrUV == 0)
            {
                // NOTE: We don't have indicies to tex coords
                {
                    u32 VertPosId, VertNormId;

                    GetUIntFromStr(&CurrChar, &VertPosId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);
                    GetUIntFromStr(&CurrChar, &VertNormId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);

                    v3 Pos = PosArray[VertPosId - 1];
                    v2 UV = V2(0, 0);
                    v3 Normal = NormalArray[VertNormId - 1];

                    VertexArray[CurrVertexNum++] = {Pos, UV, Normal};
                    CurrMeshVertexNum += 1;
                }

                {
                    u32 VertPosId, VertNormId;

                    GetUIntFromStr(&CurrChar, &VertPosId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);
                    GetUIntFromStr(&CurrChar, &VertNormId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);

                    v3 Pos = PosArray[VertPosId - 1];
                    v2 UV = V2(0, 0);
                    v3 Normal = NormalArray[VertNormId - 1];

                    VertexArray[CurrVertexNum++] = {Pos, UV, Normal};
                    CurrMeshVertexNum += 1;
                }
                
                {
                    u32 VertPosId, VertNormId;

                    GetUIntFromStr(&CurrChar, &VertPosId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);
                    GetUIntFromStr(&CurrChar, &VertNormId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);

                    v3 Pos = PosArray[VertPosId - 1];
                    v2 UV = V2(0, 0);
                    v3 Normal = NormalArray[VertNormId - 1];

                    VertexArray[CurrVertexNum++] = {Pos, UV, Normal};
                    CurrMeshVertexNum += 1;
                }
            }
            else
            {
                file_vertex FaceVerts[4] = {};
                u32 FaceVertId = 0;
                
                while (IsDigit(*CurrChar))
                {
                    u32 VertPosId, VertUVId, VertNormId;

                    GetUIntFromStr(&CurrChar, &VertPosId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);
                    GetUIntFromStr(&CurrChar, &VertUVId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);
                    GetUIntFromStr(&CurrChar, &VertNormId);
                    GetPastWhiteSpaceAndSlashes(&CurrChar);

                    v3 Pos = PosArray[VertPosId - 1];
                    v2 UV = UVArray[VertUVId - 1];
                    v3 Normal = NormalArray[VertNormId - 1];

                    FaceVerts[FaceVertId++] = {Pos, UV, Normal};
                }

                if (FaceVertId == 3)
                {
                    VertexArray[CurrVertexNum++] = FaceVerts[0];
                    VertexArray[CurrVertexNum++] = FaceVerts[1];
                    VertexArray[CurrVertexNum++] = FaceVerts[2];
                    
                    CurrMeshVertexNum += 3;
                }
                else if (FaceVertId == 4)
                {
                    VertexArray[CurrVertexNum++] = FaceVerts[0];
                    VertexArray[CurrVertexNum++] = FaceVerts[1];
                    VertexArray[CurrVertexNum++] = FaceVerts[2];

                    VertexArray[CurrVertexNum++] = FaceVerts[0];
                    VertexArray[CurrVertexNum++] = FaceVerts[2];
                    VertexArray[CurrVertexNum++] = FaceVerts[3];

                    CurrMeshVertexNum += 6;
                }
                else
                {
                    InvalidCodePath;
                }
            }
            
            GetPastWhiteSpaceNewLine(&CurrChar);
        }
        else if (CurrChar[0] == 'u' && CurrChar[1] == 's' && CurrChar[2] == 'e' &&
                 CurrChar[3] == 'm' && CurrChar[4] == 't' && CurrChar[5] == 'l')
        {
            // NOTE: We have a material attached to this mesh
            CurrChar += 6;
            GetPastWhiteSpace(&CurrChar);

            i32 FoundTextureId = -1;
            for (u32 TextureId = 0; TextureId < Model->NumTextures; ++TextureId)
            {
                char* TextureName = Work.MtlNameArray[TextureId];
                u32 TextureNameLen = Work.MtlNameLengthArray[TextureId];

                if (StringsAreEqual(TextureNameLen, TextureName, TextureNameLen, CurrChar))
                {
                    FoundTextureId = TextureId;
                    CurrChar += TextureNameLen;
                    break;
                }
            }

            // NOTE: Texture id of -1 is a invalid texture
            CurrTextureId = FoundTextureId;
            GetPastWhiteSpaceNewLine(&CurrChar);
        }
        else if (CurrChar[0] == 'g')
        {
            // NOTE: We have a new group
            CurrChar += 1;
            GetPastWhiteSpace(&CurrChar);

            CurrMesh->NumVertices = CurrMeshVertexNum;
            CurrMesh->TextureId = CurrTextureId;
            CurrMeshVertexNum = 0;

            // NOTE: Setup vertex offset for the current mesh
            CurrMesh += 1;
            CurrMesh->VertexArrayOffset = sizeof(file_vertex)*CurrVertexNum;

            GetToNewLine(&CurrChar);
            GetPastWhiteSpaceNewLine(&CurrChar);
        }
        else
        {
            SkipPastNewLine(&CurrChar, FileObj);
        }
    }

    // NOTE: Add the final mesh we where working on
    {
        CurrMesh->NumVertices = CurrMeshVertexNum;
        CurrMesh->TextureId = CurrTextureId;
    }
    Model->NumMeshes = (u32)(CurrMesh - MeshArray) + 1;

    {
        // NOTE: Load the vertex table
        file_io_work FileIoWork = {};
        FileIoWork.Size = sizeof(file_vertex)*CurrVertexNum;
        u64 VertexDataOffset = GetAssetFileDataOffset(FileIoWork.Size);
        FileIoWork.DataOffset = VertexDataOffset;
        FileIoWork.Data = VertexArray;

        for (u32 MeshId = 0; MeshId < Model->NumMeshes; ++MeshId)
        {
            file_mesh* CurrMesh = MeshArray + MeshId;
            CurrMesh->VertexArrayOffset += VertexDataOffset;
        }

        _WriteBarrier();
        
        AddFileIoEntry(FileIoWork);
    }

    if (Work.NumMaterials > 0)
    {
        // NOTE: Load the textures
        file_io_work FileIoWork = {};
        FileIoWork.Size = sizeof(file_texture)*Work.NumMaterials + sizeof(file_mesh)*Model->NumMeshes;
        FileIoWork.DataOffset = GetAssetFileDataOffset(FileIoWork.Size);
        Model->TextureArrayOffset = FileIoWork.DataOffset;
        FileIoWork.Data = Work.TextureArray;

        _WriteBarrier();
                
        AddFileIoEntry(FileIoWork);
    }
    
    {
        // NOTE: Load the mesh table
        file_io_work FileIoWork = {};
        FileIoWork.Size = sizeof(file_mesh)*Model->NumMeshes + sizeof(file_mesh)*Model->NumMeshes;
        FileIoWork.DataOffset = GetAssetFileDataOffset(FileIoWork.Size);
        Model->MeshArrayOffset = FileIoWork.DataOffset;
        FileIoWork.Data = MeshArray;

        _WriteBarrier();

        AddFileIoEntry(FileIoWork);
    }
    
    free(PosArray);
    free(UVArray);
    free(NormalArray);
}

internal WORK_QUEUE_CALLBACK(DoLoadBitmapWork)
{
    load_bitmap_work Work = *(load_bitmap_work*)Data;
    free(Data);

    file_texture* Texture = Work.Texture;
    file_io_work FileIoWork = {};
    
    char* FullPath = CombineStrings("textures/", Work.FileName);
    loaded_file File = LoadFile(FullPath);
    free(FullPath);
    free(Work.FileName);

    bitmap_header *Header = (bitmap_header*)File.Data;
    Texture->Width = Header->Width;
    Texture->Height = Header->Height;
    
    if (Header->BitsPerPixel == 24)
    {
        // NOTE: We use these to load the bitmap to the asset file
        FileIoWork.Size = sizeof(u32)*Texture->Width*Texture->Height;
        FileIoWork.Data = malloc(FileIoWork.Size);

        u8* SourceDest = (u8*)(File.Data + Header->BitmapOffset);
        u32* DestPixels = (u32*)FileIoWork.Data;
        u32 PaddingSize = Texture->Width % 4;
            
        for (i32 Y = 0; Y < Header->Height; ++Y)
        {
            SourceDest += PaddingSize;

            for (i32 X = 0; X < Header->Width; ++X)
            {
                u8 Blue = *(SourceDest);
                u8 Green = *(SourceDest + 1);
                u8 Red = *(SourceDest + 2);
                u8 Alpha = 0xFF;

                v4 Texel = SRGBToLinear(V4(Red, Green, Blue, Alpha));
                Texel.rgb *= Texel.a;
                Texel = LinearToSRGB(Texel);
                
                u32 DestColor = (((u32)(Texel.a + 0.5f) << 24) |
                                 ((u32)(Texel.b + 0.5f) << 16) |
                                 ((u32)(Texel.g + 0.5f) << 8) |
                                 ((u32)(Texel.r + 0.5f) << 0));
                
                *DestPixels++ = DestColor;
                SourceDest += 3;
            }
        }

        free(File.Data);
    }
    else if (Header->BitsPerPixel == 32)
    {
        FileIoWork.Size = sizeof(u32)*Texture->Width*Texture->Height;
        FileIoWork.Data = malloc(FileIoWork.Size);

        u32 RedMask = Header->RedMask;
        u32 GreenMask = Header->GreenMask;
        u32 BlueMask = Header->BlueMask;
        u32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        i32 RedShiftDown = (i32)RedScan.Index;
        i32 GreenShiftDown = (i32)GreenScan.Index;
        i32 BlueShiftDown = (i32)BlueScan.Index;
        i32 AlphaShiftDown = (i32)AlphaScan.Index;

        u8* SourceDest = (u8*)(File.Data + Header->BitmapOffset);
        u32* DestPixels = (u32*)FileIoWork.Data;
        
        for (i32 Y = 0; Y < Header->Height; ++Y)
        {
            for (i32 X = 0; X < Header->Width; ++X)
            {
                u32 Color = *(u32*)SourceDest;
                v4 Texel = V4i((Color & RedMask) >> RedShiftDown,
                               (Color & GreenMask) >> GreenShiftDown,
                               (Color & BlueMask) >> BlueShiftDown,
                               (Color & AlphaMask) >> AlphaShiftDown);
                
                Texel = SRGBToLinear(Texel);
                Texel.rgb *= Texel.a;
                Texel = LinearToSRGB(Texel);
                
                u32 DestColor = (((u32)(Texel.a + 0.5f) << 24) |
                                 ((u32)(Texel.b + 0.5f) << 16) |
                                 ((u32)(Texel.g + 0.5f) << 8) |
                                 ((u32)(Texel.r + 0.5f) << 0));
                
                *DestPixels++ = DestColor;
                SourceDest += 4;
            }
        }

        free(File.Data);
    }
    else
    {
        InvalidCodePath;
    }

    FileIoWork.DataOffset = GetAssetFileDataOffset(FileIoWork.Size);
    Texture->PixelOffset = FileIoWork.DataOffset;
    AddFileIoEntry(FileIoWork);
}

internal void AddFontAsset(char* FileName, u32 Id, u32 MinGlyph, u32 MaxGlyph)
{
    load_font_work* Work = (load_font_work*)malloc(sizeof(load_font_work));
    Work->Font = State.FontArray + Id;
    Work->MinGlyph = MinGlyph;
    Work->MaxGlyph = MaxGlyph;
    AllocAndCopyString(FileName, &Work->FileName);

    Win32AddWorkQueueEntry(&State.Queue, DoLoadFontWork, Work);
}

internal void AddModelAsset(char* FileName, u32 Id)
{
    load_obj_work* Work = (load_obj_work*)malloc(sizeof(load_obj_work));
    *Work = {};
    Work->Model = State.ModelArray + Id;
    AllocAndCopyString(FileName, &Work->ObjFileName);

    Win32AddWorkQueueEntry(&State.Queue, DoLoadObjWork, Work);
}

internal void AddModelAsset(char* FileName, char* MtlFileName, u32 Id)
{
    load_model_work* Work = (load_model_work*)malloc(sizeof(load_model_work));
    Work->Model = State.ModelArray + Id;
    AllocAndCopyString(FileName, &Work->FileName);
    AllocAndCopyString(MtlFileName, &Work->MtlFileName);

    Win32AddWorkQueueEntry(&State.Queue, DoLoadModelWork, Work);
}

internal void AddLoadObjWork(char** MtlNameArray, u32* MtlNameLengthArray, file_texture* TextureArray,
                             u32 CurrMtlId, char* FileName, file_model* Model,
                             u32 volatile* DependancyCounter)
{
    load_obj_work* Work = (load_obj_work*)malloc(sizeof(load_obj_work));
    Work->NumMaterials = CurrMtlId;
    Work->MtlNameArray = MtlNameArray;
    Work->MtlNameLengthArray = MtlNameLengthArray;
    Work->TextureArray = TextureArray;

    Work->ObjFileName = FileName;
    Work->Model = Model;

    Win32AddWorkQueueDependancyEntry(&State.DependancyQueue, DoLoadObjWork, Work, DependancyCounter);
}

internal void AddTextureAsset(char* FileName, file_texture* Texture, u32 volatile* DependancyCounter)
{
    load_bitmap_work* Work = (load_bitmap_work*)malloc(sizeof(load_bitmap_work));
    Work->Texture = Texture;
    AllocAndCopyString(FileName, &Work->FileName);

    Win32AddWorkQueueEntry(&State.Queue, DoLoadBitmapWork, Work, DependancyCounter);
}

inline void AddTextureAsset(char* FileName, u32 Id)
{
    AddTextureAsset(FileName, State.TextureArray + Id);
}

int main(int argc, char** argv)
{    
    State.Queue = {};
    {
        win32_thread_info ThreadInfo[2] = {};
        u32 ThreadCount = ArrayCount(ThreadInfo);
        u32 InitialCount = 0;
        HANDLE SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);

        State.Queue.SemaphoreHandle = SemaphoreHandle;
    
        for (u32 ThreadId = 0; ThreadId < ThreadCount; ++ThreadId)
        {
            win32_thread_info* Info = ThreadInfo + ThreadId;
            Info->Queue = &State.Queue;
            Info->DependancyQueue = &State.DependancyQueue;
            
            DWORD Win32ThreadId;
            HANDLE ThreadHandle = CreateThread(0, 0, DependancyThreadProc, Info, 0, &Win32ThreadId);
            CloseHandle(ThreadHandle);
        }
    }

    State.FileIoQueue = {};
    {
        win32_thread_info ThreadInfo[1] = {};
        u32 ThreadCount = ArrayCount(ThreadInfo);
        u32 InitialCount = 0;
        HANDLE SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);

        State.FileIoQueue.SemaphoreHandle = SemaphoreHandle;
    
        for (u32 ThreadId = 0; ThreadId < ThreadCount; ++ThreadId)
        {
            win32_thread_info* Info = ThreadInfo + ThreadId;
            Info->Queue = &State.FileIoQueue;
            
            DWORD Win32ThreadId;
            HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Info, 0, &Win32ThreadId);
            CloseHandle(ThreadHandle);
        }
    }

    // NOTE: Build the asset file    
    State.AssetFile = fopen("coco.assets", "wb");
    if (!State.AssetFile)
    {
        InvalidCodePath;
    }

    file_header Header = {};
    {
        mm CurrByte = sizeof(file_header);
        Header.FontArrayPos = CurrByte;
        Header.FontCount = Font_NumElements;
        CurrByte += sizeof(file_font)*Header.FontCount;

        Header.ModelArrayPos = CurrByte;
        Header.ModelCount = Model_NumElements;
        CurrByte += sizeof(file_model)*Header.ModelCount;
        
        Header.TextureArrayPos = CurrByte;
        Header.TextureCount = Texture_NumElements;
        CurrByte += sizeof(file_texture)*Header.TextureCount;
        
        State.CurrDataOffset = CurrByte;
    }

    State.FontArray = (file_font*)malloc(sizeof(file_font)*Header.FontCount);
    State.ModelArray = (file_model*)malloc(sizeof(file_model)*Header.ModelCount);
    State.TextureArray = (file_texture*)malloc(sizeof(file_texture)*Header.TextureCount);
    
    AddFontAsset("LiberationMono-Regular.ttf", Font_General, 32, 127);
    
    AddModelAsset("bunny.obj", Model_Bunny);
    AddModelAsset("ateneam.obj", Model_Elephant);
    //AddModelAsset("cube.obj", Model_Cube);
    AddModelAsset("sphere.obj", Model_Sphere);
    AddModelAsset("sponza.obj", "sponza.mtl", Model_Sponza);

    Win32CompleteAllWork(&State.Queue);
    Win32CompleteAllWork(&State.DependancyQueue);
    Win32WaitForAllWork(&State.FileIoQueue);

    fseek(State.AssetFile, 0, SEEK_SET);
    fwrite(&Header, sizeof(file_header), 1, State.AssetFile);
    fwrite(State.FontArray, Font_NumElements*sizeof(file_font), 1, State.AssetFile);
    fwrite(State.ModelArray, Model_NumElements*sizeof(file_model), 1, State.AssetFile);
    fwrite(State.TextureArray, Texture_NumElements*sizeof(file_texture), 1, State.AssetFile);
    
    u32 Temp = ferror(State.AssetFile);
    if (Temp)
    {
        InvalidCodePath;
    }
    
    fclose(State.AssetFile);
    
    return 1;
}
