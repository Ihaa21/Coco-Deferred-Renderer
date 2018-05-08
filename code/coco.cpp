#include "coco_platform.h"

#include <stdio.h>
#include <stdlib.h>

#include "coco.h"
#include "coco_assets.cpp"
#include "coco_render.cpp"

platform_api PlatformApi;

inline v3 RotateAroundAxis(v3 Vec, v3 Axis, f32 Angle)
{
    // NOTE: We rotate clockwise but the unit circle is flipped vertically
    v3 Result = {};

    quat VecQuat = Quat(Vec);

    quat RotateQuat = {};
    RotateQuat.v = Axis*Sin(Angle * 0.5f);
    RotateQuat.w = Cos(Angle * 0.5f);
    
    quat RotateConj = Conjugate(RotateQuat);
    quat OutputQuat = (RotateQuat*VecQuat)*RotateConj;

    Assert(LengthSq(RotateQuat) < 1.001f);
    Assert(LengthSq(RotateConj) < 1.001f);
    Assert(LengthSq(OutputQuat) < 1.001f);

    Result = OutputQuat.v;
    
    return Result;
}

extern "C" GAME_INIT(GameInit)
{
    game_state* GameState = (game_state*)GameMem->PermanentMem;
    PlatformApi = GameMem->PlatformApi;

    GameState->Arena = InitArena((u8*)GameState + sizeof(game_state),
                                 GameMem->PermanentMemSize - sizeof(game_state));
    GameState->Commands = InitRenderCommands(GameState, RenderSettings, DegreeToRad(90.0f),
                                             0.05f, 2000.0f);
    GameState->Assets = LoadAssets(GameMem, &GameState->Arena);

    // NOTE: Sets up the angles given a target vector
    camera* Camera = &GameState->Camera;
    v3 Target = V3(0, 0, 1); // NOTE: Input target vector
    v3 HTarget = Normalize(V3(Target.x, 0, Target.z));

    if (HTarget.z >= 0.0f)
    {
        if (HTarget.x >= 0.0f)
        {
            Camera->CurrentRotation.x = (2.0f*Pi32) - ArcSin(HTarget.z);
        }
        else
        {
            Camera->CurrentRotation.x = (2.0f*Pi32) - ArcSin(HTarget.z);
        }
    }
    else
    {
        if (HTarget.x >= 0.0f)
        {
            Camera->CurrentRotation.x = ArcSin(-HTarget.z);
        }
        else
        {
            Camera->CurrentRotation.x = Pi32 - ArcSin(-HTarget.z);
        }
    }

    Camera->CurrentRotation.y = -ArcSin(Target.y);
    GameState->PrevMouseP = V2(0.5f, 0.5f);

    return GameState->Commands;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state* GameState = (game_state*)GameMem->PermanentMem;
    PlatformApi = GameMem->PlatformApi;

    game_render_commands* Commands = GameState->Commands;
    
    // NOTE: Build camera matrix and process camera movement
    m4 WorldToCamera = {};
    {
        camera* Camera = &GameState->Camera;

        v2 Delta = Input->MouseP - GameState->PrevMouseP;
        GameState->PrevMouseP = Input->MouseP;

        v2 AngleDelta = V2(Delta.x, -Delta.y) / 2.5f;

        Camera->CurrentRotation += AngleDelta;
        f32 RotationSpeed = 0.005f;
        if (Delta.x == 0.0f)
        {
            if (Input->MouseP.x < 0.3f)
            {
                Camera->CurrentRotation.x += -RotationSpeed;
            }
            else if (Input->MouseP.x >= 0.7f)
            {
                Camera->CurrentRotation.x += RotationSpeed;
            }
        }
        if (Delta.y == 0.0f)
        {
            if (Input->MouseP.y < 0.3f)
            {
                Camera->CurrentRotation.y += RotationSpeed;
            }
            else if (Input->MouseP.y >= 0.7f)
            {
                Camera->CurrentRotation.y += -RotationSpeed;
            }
        }
    
        // NOTE: Keep high precision
        if (Camera->CurrentRotation.x > 2.0f*Pi32)
        {
            Camera->CurrentRotation.x -= 2.0f*Pi32;
        }
        else if (Camera->CurrentRotation.x < -2.0f*Pi32)
        {
            Camera->CurrentRotation.x += 2.0f*Pi32;
        }
        if (Camera->CurrentRotation.y > 2.0f*Pi32)
        {
            Camera->CurrentRotation.y -= 2.0f*Pi32;
        }
        else if (Camera->CurrentRotation.y < -2.0f*Pi32)
        {
            Camera->CurrentRotation.y += 2.0f*Pi32;
        }

        // NOTE: Our camera matrix is in a left hand coordinate system
        v3 Target = V3(1, 0, 0); // NOTE: This vector has both angles = 0
        v3 Up = V3(0, 1, 0);
        Target = Normalize(RotateAroundAxis(Target, Up, Camera->CurrentRotation.x));

        v3 Horizontal = Cross(Up, Target);
        Horizontal = Normalize(Horizontal);
        Target = Normalize(RotateAroundAxis(Target, Horizontal, Camera->CurrentRotation.y));

        Up = Normalize(Cross(Target, Horizontal));

        Assert(Abs(Inner(Target, Up)) <= 0.001f);
        Assert(Abs(Inner(Up, Horizontal)) <= 0.001f);
        Assert(Abs(Inner(Horizontal, Target)) <= 0.001f);
        
        // NOTE: Move the camera by the given input
        f32 MoveSpeed = 0.2f;
        f32 MoveForward = 0.0f;
        f32 MoveSideways = 0.0f;
        if (Input->MoveLeftDown)
        {
            MoveSideways += -MoveSpeed;
        }
        if (Input->MoveRightDown)
        {
            MoveSideways += MoveSpeed;
        }
        if (Input->MoveUpDown)
        {
            MoveForward += MoveSpeed;
        }
        if (Input->MoveDownDown)
        {
            MoveForward += -MoveSpeed;
        }

        v3 MovementVector = 0.1f*(MoveForward*Target + MoveSideways*Horizontal);
        Camera->Pos += MovementVector;
    
        {
            // NOTE: We inverse the transpose to apply the opposite
            // transformation of the cameras orientation
            m4 Rotation = {};
            Rotation.v[0].xyz = Horizontal;
            Rotation.v[1].xyz = Up;
            Rotation.v[2].xyz = Target;
            Rotation.v[3].w = 1.0f;
            Rotation = Transpose(Rotation);

            m4 Translation = PosMat(-Camera->Pos);
            WorldToCamera = Rotation*Translation;
        }
    }
    Commands->CameraMat = WorldToCamera;
    
    f32 SpecularIntensity = 1.0f;
    f32 SpecularPower = 16.0f;

    // NOTE: Bunny + 175 lights
#if 1
    {
        point_light PointLight = {};
        {
            PointLight.Color = V3(1, 1, 1);
            PointLight.DiffuseIntensity = 0.89f;
            PointLight.ConstantAttenuation = 0.0f;
            PointLight.LinearAttenuation = 0.0f;
            PointLight.ExpAttenuation = 6.3f;
            PointLight.Pos = V3(0, 0, 6);
        }

        for (u32 LightZ = 0; LightZ < 7; ++LightZ)
        {
            for (u32 LightY = 0; LightY < 5; ++LightY)
            {
                for (u32 LightX = 0; LightX < 5; ++LightX)
                {
                    PointLight.Pos.x = 2.5f - LightX;
                    PointLight.Pos.y = 2.5f - LightY;
                    PointLight.Pos.z = 7.0f - LightZ;

                    PointLight.Color = V3(LightX / 5.0f, LightY / 5.0f, LightZ / 4.0f);
                    
                    PushPointLight(Commands, &GameState->Assets, PointLight);
                }
            }
        }
        
        local_global f32 SinT = 0.0f;
        SinT += 0.005f;
        if (SinT > 2.0f*Pi32)
        {
            SinT = 0.0f;
        }

        for (u32 ModelZ = 0; ModelZ < 3; ++ModelZ)
        {
            for (u32 ModelY = 0; ModelY < 2; ++ModelY)
            {
                for (u32 ModelX = 0; ModelX < 4; ++ModelX)
                {
                    v3 ModelPos = 3.0f*V3(1.6f - ModelX, 0.5f - ModelY, ModelZ + 1.6f);
                    m4 ModelMat = PosMat(ModelPos)*RotMat(SinT, SinT, SinT)*ScaleMat(0.45, 0.45, 0.45);
                    asset_model* Model = GetModel(&GameState->Assets, Model_Bunny);
                    PushModel(Commands, ModelMat, SpecularIntensity, SpecularPower, Model);
                }
            }
        }
    }
#elif 0
    {
        // NOTE: Statues
        dir_light DirLight = {};
        {
            DirLight.Color = V3(0.6, 0.5, 0.8);
            DirLight.AmbientIntensity = 0.0f;
            DirLight.DiffuseIntensity = 0.0f;
            DirLight.Dir = Normalize(V3(0, 0, 1));
        }

        PushDirLight(Commands, &GameState->Assets, DirLight);

        point_light PointLight = {};
        {
            PointLight.Color = V3(1, 1, 1);
            PointLight.DiffuseIntensity = 7.89f;
            PointLight.ConstantAttenuation = 0.0f;
            PointLight.LinearAttenuation = 0.0f;
            PointLight.ExpAttenuation = 100.3f;
            PointLight.Pos = V3(0, 0, 0);
        }

        v3 LightColors[] =
        {
            V3(1, 0, 0),
            V3(0, 1, 0),
            V3(0, 0, 1),
            V3(0.5, 0.5, 0),
            V3(0.0, 0.5, 0.5),
            V3(0.5, 0.0, 0.5),
            V3(1, 0.0, 0.5),
            V3(0.5, 0.0, 1),
            V3(1, 0.5, 0),
            V3(0.5, 1, 0),
        };

        u32 LightIndex = 0;
        for (u32 ModelZ = 0; ModelZ < 3; ++ModelZ)
        {
            for (u32 ModelY = 0; ModelY < 1; ++ModelY)
            {
                for (u32 ModelX = 0; ModelX < 4; ++ModelX)
                {
                    v3 ModelPos = 3.0f*V3(1.6f - ModelX, 0.5f - ModelY, ModelZ + 1.6f);

                    PointLight.Pos = ModelPos - V3(1, 1, 0);
                    PointLight.Color = LightColors[LightIndex];
                    PushPointLight(Commands, &GameState->Assets, PointLight);

                    LightIndex = (LightIndex + 1) % ArrayCount(LightColors);
                    
                    PointLight.Pos = ModelPos - V3(-1, 0, 0);
                    PointLight.Color = LightColors[LightIndex];
                    PushPointLight(Commands, &GameState->Assets, PointLight);

                    LightIndex = (LightIndex + 1) % ArrayCount(LightColors);

                    PointLight.Pos = ModelPos - V3(1, 1, -1);
                    PointLight.Color = LightColors[LightIndex];
                    PushPointLight(Commands, &GameState->Assets, PointLight);

                    LightIndex = (LightIndex + 1) % ArrayCount(LightColors);

                    PointLight.Pos = ModelPos - V3(-1, 0, 1);
                    PointLight.Color = LightColors[LightIndex];
                    PushPointLight(Commands, &GameState->Assets, PointLight);

                    LightIndex = (LightIndex + 1) % ArrayCount(LightColors);
                    
                    m4 ModelMat = PosMat(ModelPos)*RotMat(0, 0, 0)*ScaleMat(0.0003, 0.0003, 0.0003);
                    asset_model* Model = GetModel(&GameState->Assets, Model_Elephant);
                    PushModel(Commands, ModelMat, SpecularIntensity, SpecularPower, Model);
                }
            }
        }
    }
#else
    // NOTE: Sponza
    {
        dir_light DirLight = {};
        {
            DirLight.Color = V3(1, 1, 1);
            DirLight.AmbientIntensity = 0.1f;
            DirLight.DiffuseIntensity = 0.4f;
            DirLight.Dir = Normalize(V3(0, 0, 1));
        }

        PushDirLight(Commands, &GameState->Assets, DirLight);

        point_light PointLight = {};
        {
            PointLight.Color = V3(1, 0, 0);
            PointLight.AmbientIntensity = 0.1f;
            PointLight.DiffuseIntensity = 15.89f;
            PointLight.ConstantAttenuation = 0.0f;
            PointLight.LinearAttenuation = 0.5f;
            PointLight.ExpAttenuation = 100.3f;
            PointLight.Pos = V3(0, 0, 0);
        }

        for (u32 LightZ = 0; LightZ < 5; ++LightZ)
        {
            for (u32 LightY = 0; LightY < 5; ++LightY)
            {
                for (u32 LightX = 0; LightX < 5; ++LightX)
                {
                    PointLight.Pos.x = 2.0f*(f32)LightX;
                    PointLight.Pos.y = (f32)LightY;
                    PointLight.Pos.z = (f32)LightZ;

                    PointLight.Color = V3(LightX / 5.0f, LightY / 5.0f, LightZ / 4.0f);
                    
                    PushPointLight(Commands, &GameState->Assets, PointLight);
                }
            }
        }

        f32 Rotation = 1.5f*Pi32;

        v3 ModelPos = V3(0, 0, 2.2f);
        m4 ModelMat = PosMat(ModelPos)*RotMat(Rotation, 0, 0)*ScaleMat(0.015, 0.015, 0.015);
        asset_model* Sponza = GetModel(&GameState->Assets, Model_Sponza);
        PushModel(Commands, ModelMat, SpecularIntensity, SpecularPower, Sponza);
    }
#endif
}
