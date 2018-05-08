inline game_render_commands* InitRenderCommands(game_state* GameState, game_render_settings Settings,
                                                f32 Fov, f32 Near, f32 Far)
{
    u32 AllocSize = MegaBytes(4);
    u8* RenderData = (u8*)PushSize(&GameState->Arena, AllocSize);
    game_render_commands* Result = (game_render_commands*)RenderData;
    RenderData += sizeof(game_render_commands);

    Result->Settings = Settings;
    Result->AspectRatio = ((f32)Settings.ResWidth / (f32)Settings.ResHeight);
    Result->Arena = InitDoubleArena(RenderData, AllocSize - sizeof(game_render_commands));

    f32 AspectRatio = (f32)Result->Settings.ResWidth / (f32)Result->Settings.ResHeight;
    Result->ProjMat = {};
    Result->ProjMat.v[0].x = 1.0f / (AspectRatio*Tan(Fov / 2.0f));
    Result->ProjMat.v[1].y = 1.0f / Tan(Fov / 2.0f);
    Result->ProjMat.v[2].z = -(Far + Near) / (Near - Far);
    Result->ProjMat.v[2].w = 1.0f;
    Result->ProjMat.v[3].z = 2.0f*Far*Near/(Near - Far);
    
    return Result;
}

inline void SetCamera(game_render_commands* Commands, v3 Target, v3 Up, v3 Pos)
{
    v3 NewTarget = Normalize(Target);
    v3 NewHoriz = Normalize(Cross(Up, Target));
    v3 NewUp = Cross(NewTarget, NewHoriz);

    Commands->CameraMat = IdentityM4();
    Commands->CameraMat.v[0].xyz = NewHoriz;
    Commands->CameraMat.v[1].xyz = NewUp;
    Commands->CameraMat.v[2].xyz = NewTarget;
    Commands->CameraMat.v[3].xyz = -Pos;
    Commands->CameraMat.v[3].w = 1.0f;
    Commands->CameraMat = Transpose(Commands->CameraMat);
}

inline v3 GetCameraTarget(game_render_commands* Commands)
{
    v3 Result = Commands->CameraMat.v[2].xyz;

    return Result;
}

inline v3 GetCameraUp(game_render_commands* Commands)
{
    v3 Result = Commands->CameraMat.v[1].xyz;

    return Result;
}

inline void PushModel(game_render_commands* Commands, m4 WTransform, f32 SpecularIntensity,
                      f32 SpecularPower, asset_model* Model)
{
    for (u32 MeshId = 0; MeshId < Model->NumMeshes; ++MeshId)
    //u32 MeshId = 5;
    {
        asset_mesh* Mesh = Model->Meshes + MeshId;

        if (Mesh->NumVertices == 0)
        {
            continue;
        }
        
        model_data* ModelData = 0;
        u32* RenderCmdType = PushStruct(&Commands->Arena, u32);
        *RenderCmdType = RenderCmdType_Mesh;
    
        ModelData = PushStruct(&Commands->Arena, model_data);
        if (Mesh->Texture)
        {
            ModelData->TextureId = Mesh->Texture->GLId;
        }
        else
        {
            // NOTE: Notify OpenGL to use a white texture for this model
            ModelData->TextureId = -1;
        }
        
        ModelData->VboId = Mesh->GLId;
        ModelData->NumVertices = Mesh->NumVertices;
        ModelData->WTransform = WTransform;
        ModelData->SpecularIntensity = SpecularIntensity;
        ModelData->SpecularPower = SpecularPower;
    }
}

inline void PushPointLight(game_render_commands* Commands, assets* Assets, point_light PointLight)
{
    u32* RenderCmdType = BotPushStruct(&Commands->Arena, u32);
    *RenderCmdType = RenderCmdType_PointLight;

    point_light* PointLightData = BotPushStruct(&Commands->Arena, point_light);
    *PointLightData = PointLight;
    asset_model* Sphere = GetModel(Assets, Model_Sphere);
    PointLightData->GLSphereId = Sphere->Meshes[0].GLId;
    PointLightData->NumVertices = Sphere->Meshes[0].NumVertices;
}

inline void PushDirLight(game_render_commands* Commands, assets* Assets, dir_light DirLight)
{
    u32* RenderCmdType = BotPushStruct(&Commands->Arena, u32);
    *RenderCmdType = RenderCmdType_DirLight;

    dir_light* DirLightData = BotPushStruct(&Commands->Arena, dir_light);
    *DirLightData = DirLight;
}
