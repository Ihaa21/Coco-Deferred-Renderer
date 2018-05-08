internal assets LoadAssets(game_memory* GameMem, mem_arena* Arena)
{
    assets Assets = {};
    file_header* Header = (file_header*)GameMem->AssetMem;
    Assets.FontCount = Header->FontCount;
    Assets.ModelCount = Header->ModelCount;
    Assets.TextureCount = Header->TextureCount;
    
    Assets.Fonts = PushArray(Arena, asset_font, Assets.FontCount);
    Assets.Models = PushArray(Arena, asset_model, Assets.ModelCount);
    Assets.Textures = PushArray(Arena, asset_texture, Assets.TextureCount);
    
    file_font* FileFonts = (file_font*)(GameMem->AssetMem + Header->FontArrayPos);
    file_model* FileModels = (file_model*)(GameMem->AssetMem + Header->ModelArrayPos);
    file_texture* FileTextures = (file_texture*)(GameMem->AssetMem + Header->TextureArrayPos);

    for (u64 FontIndex = 0; FontIndex < Assets.FontCount; ++FontIndex)
    {
        asset_font* AssetFont = Assets.Fonts + FontIndex;
        file_font FileFont = FileFonts[FontIndex];
        u32 NumGlyphsToLoad = FileFont.MaxGlyph - FileFont.MinGlyph;

        AssetFont->MinGlyph = FileFont.MinGlyph;
        AssetFont->MaxGlyph = FileFont.MaxGlyph;
        AssetFont->MaxAscent = FileFont.MaxAscent;
        AssetFont->MaxDescent = FileFont.MaxDescent;
        AssetFont->LineGap = FileFont.LineGap;
        
        // NOTE: Copy step array
        {
            u64 MemSize = FileFont.KernArrayOffset - FileFont.StepArrayOffset;
            AssetFont->StepArray = (f32*)PushSize(Arena, MemSize);
            Copy(GameMem->AssetMem + FileFont.StepArrayOffset, AssetFont->StepArray, MemSize);
        }

        // NOTE: Copy kern array
        {
            u64 MemSize = FileFont.GlyphArrayOffset - FileFont.KernArrayOffset;
            AssetFont->KernArray = (f32*)PushSize(Arena, MemSize);
            Copy(GameMem->AssetMem + FileFont.KernArrayOffset, AssetFont->KernArray, MemSize);
        }

        // NOTE: Copy glyph array
        {
            AssetFont->GlyphArray = PushArray(Arena, asset_glyph, NumGlyphsToLoad);

            file_glyph* FileGlyphArray = (file_glyph*)(GameMem->AssetMem + FileFont.GlyphArrayOffset);
            for (u32 GlyphIndex = 0; GlyphIndex < NumGlyphsToLoad; ++GlyphIndex)
            {
                file_glyph FileGlyph = FileGlyphArray[GlyphIndex];
                asset_glyph AssetGlyph = {};
                AssetGlyph.Width = FileGlyph.Width;
                AssetGlyph.Height = FileGlyph.Height;
                AssetGlyph.Min = V2(0, 0);
                AssetGlyph.Dim = V2(1, 1);
                AssetGlyph.AlignPos = FileGlyph.AlignPos;
                AssetGlyph.GLId = PlatformApi.GLLoadTextureToGpu(FileGlyph.Width, FileGlyph.Height,
                                                                 GameMem->AssetMem + FileGlyph.PixelOffset);

                AssetFont->GlyphArray[GlyphIndex] = AssetGlyph;
            }
        }
    }

    for (u64 ModelIndex = 0; ModelIndex < Assets.ModelCount; ++ModelIndex)
    {
        asset_model* Model = Assets.Models + ModelIndex;
        file_model FileModel = FileModels[ModelIndex];

        Model->NumTextures = FileModel.NumTextures;
        if (Model->NumTextures == 0)
        {
            Model->TextureArray = 0;
        }
        else
        {
            Model->TextureArray = PushArray(Arena, asset_texture, FileModel.NumTextures);
        }

        // NOTE: Load the textures
        for (u32 TextureId = 0; TextureId < FileModel.NumTextures; ++TextureId)
        {
            asset_texture* Texture = Model->TextureArray + TextureId;
            file_texture* FileTexture = (file_texture*)(GameMem->AssetMem + FileModel.TextureArrayOffset);
            FileTexture += TextureId;

            Texture->MinUV = V2(0, 0);
            Texture->DimUV = V2(1, 1);
            Texture->AspectRatio = (f32)FileTexture->Width / (f32)FileTexture->Height;
            Texture->GLId = PlatformApi.GLLoadTextureToGpu(FileTexture->Width, FileTexture->Height,
                                                           GameMem->AssetMem + FileTexture->PixelOffset);
        }
        
        Model->NumMeshes = FileModel.NumMeshes;
        Model->Meshes = PushArray(Arena, asset_mesh, 1000);

        // NOTE: Load the meshes
        file_mesh* FileMesh = (file_mesh*)(GameMem->AssetMem + FileModel.MeshArrayOffset);
        for (u32 MeshId = 0; MeshId < Model->NumMeshes; ++MeshId)
        {
            asset_mesh* Mesh = Model->Meshes + MeshId;

            Mesh->NumVertices = FileMesh->NumVertices;
            if (FileMesh->NumVertices != 0)
            {
                Mesh->GLId = PlatformApi.GLLoadModelToGpu(FileMesh->NumVertices,
                                                          GameMem->AssetMem + FileMesh->VertexArrayOffset);
            }

            Mesh->Texture = Model->TextureArray + FileMesh->TextureId;

            FileMesh += 1;
        }

        int i = 0;
    }
    
    for (u64 TextureIndex = 0; TextureIndex < Assets.TextureCount; ++TextureIndex)
    {
        asset_texture* Texture = Assets.Textures + TextureIndex;

        // NOTE: We check that this isnt a sub img
        if (!Texture->GLId)
        {
            file_texture FileTexture = FileTextures[TextureIndex];
            Texture->MinUV = V2(0, 0);
            Texture->DimUV = V2(1, 1);
            Texture->AspectRatio = (f32)FileTexture.Width / (f32)FileTexture.Height;
            Texture->GLId = PlatformApi.GLLoadTextureToGpu(FileTexture.Width, FileTexture.Height,
                                                           GameMem->AssetMem + FileTexture.PixelOffset);
        }
    }
    
    return Assets;
}
