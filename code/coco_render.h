#if !defined(COCO_RENDER_H)

struct game_render_commands;

enum render_cmd_type
{
    RenderCmdType_Mesh,
    RenderCmdType_PointLight,
    RenderCmdType_DirLight,
};

struct dir_light
{
    v3 Color;
    f32 AmbientIntensity;
    f32 DiffuseIntensity;

    v3 Dir;
};

struct point_light
{
    v3 Color;
    f32 AmbientIntensity;
    f32 DiffuseIntensity;

    f32 ConstantAttenuation;
    f32 LinearAttenuation;
    f32 ExpAttenuation;

    v3 Pos;
    u32 GLSphereId;
    u32 NumVertices;
};

struct model_data
{
    i32 TextureId;
    u32 VboId;
    u32 NumVertices;

    m4 WTransform;
  
    f32 SpecularIntensity;
    f32 SpecularPower;
};

inline f32 GetPointLightSphereScale(point_light* PointLight)
{
    f32 MaxChannel = Max(Max(PointLight->Color.r, PointLight->Color.g), PointLight->Color.b);
    f32 Result = (-PointLight->LinearAttenuation + SquareRoot(Square(PointLight->LinearAttenuation) - 4.0f*PointLight->ExpAttenuation * (PointLight->ExpAttenuation - 256.0f*MaxChannel*PointLight->DiffuseIntensity))) 
        / (2.0f * PointLight->ExpAttenuation);

    return Result;
}

#define COCO_RENDER_H
#endif
