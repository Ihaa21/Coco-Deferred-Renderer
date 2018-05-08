global char* GlobalShaderHeaderCode = R"FOO(
#version 330 core

#define b32 bool
#define i32 int
#define u32 int unsigned
#define f32 float
#define v2 vec2
#define v3 vec3
#define v4 vec4
#define V2 vec2
#define V3 vec3
#define V4 vec4
#define m3 mat3
#define m4 mat4

)FOO";

global opengl OpenGL;

inline void GLCheckError()
{
#if COCO_DEBUG
    GLuint InvalidEnum = GL_INVALID_ENUM;
    GLuint InvalidValue = GL_INVALID_VALUE;
    GLuint InvalidOperation = GL_INVALID_OPERATION;
    GLuint InvalidFrameBufferOperation = GL_INVALID_FRAMEBUFFER_OPERATION;
    GLuint OutOfMemory = GL_OUT_OF_MEMORY;

    GLuint ErrorCode = glGetError();
    Assert(ErrorCode == GL_NO_ERROR);
#endif
}

inline b32 IsGLHandleValid(GLuint Handle)
{
    b32 Result = (Handle != -1);
    return Result;
}

internal GLuint OpenGLCreateProgram(char* HeaderCode, char* VertexCode, char* FragmentCode)
{
    GLuint Result = 0;
    
    GLint Temp = GL_FALSE;
    int InfoLogLength;
    
    GLuint VertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* VertexShaderCode[] =
    {
        HeaderCode,
        VertexCode,
    };
    glShaderSource(VertexShaderId, ArrayCount(VertexShaderCode), VertexShaderCode, NULL);
    glCompileShader(VertexShaderId);
    glGetShaderiv(VertexShaderId, GL_COMPILE_STATUS, &Temp);
    glGetShaderiv(VertexShaderId, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0)
    {
        char InfoLog[1024];
        glGetShaderInfoLog(VertexShaderId, InfoLogLength, NULL, &InfoLog[0]);
        InvalidCodePath;
    }

    GLuint FragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* FragShaderCode[] =
    {
        HeaderCode,
        FragmentCode,
    };
    glShaderSource(FragmentShaderId, ArrayCount(FragShaderCode), FragShaderCode, NULL);
    glCompileShader(FragmentShaderId);
    glGetShaderiv(FragmentShaderId, GL_COMPILE_STATUS, &Temp);
    glGetShaderiv(FragmentShaderId, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0)
    {
        char InfoLog[1024];
        glGetShaderInfoLog(FragmentShaderId, InfoLogLength, NULL, &InfoLog[0]);
        InvalidCodePath;
    }

    Result = glCreateProgram();
    glAttachShader(Result, VertexShaderId);
    glAttachShader(Result, FragmentShaderId);
    glLinkProgram(Result);

    glGetProgramiv(Result, GL_LINK_STATUS, &Temp);
    glGetProgramiv(Result, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0)
    {
        char InfoLog[1024];
        glGetProgramInfoLog(Result, InfoLogLength, NULL, &InfoLog[0]);
        InvalidCodePath;
    }

    glDetachShader(Result, VertexShaderId);
    glDetachShader(Result, FragmentShaderId);
    
    glDeleteShader(VertexShaderId);
    glDeleteShader(FragmentShaderId);

    return Result;
}

internal void CompileDeferredGeometryProg(deferred_geometry_prog* Prog)
{
    char* VertexCode = R"FOO(
layout(location = 0) in v3 VertPos;
layout(location = 1) in v2 VertUV;
layout(location = 2) in v3 VertNormal;

uniform m4 WVPTransform;
uniform m4 WTransform;

out v2 UV;
out v3 Normal;
out v3 WorldPos;

void main()
{
    gl_Position = WVPTransform*V4(VertPos, 1);
    UV = VertUV;
    Normal = (WTransform*V4(VertNormal, 0)).xyz;
    WorldPos = (WTransform*V4(VertPos, 1)).xyz;
}
)FOO";

    char* FragmentCode = R"FOO(
in v2 UV;
in v3 Normal;
in v3 WorldPos;

layout(location = 0) out v4 WorldPosOut;
layout(location = 1) out v4 DiffuseOut;
layout(location = 2) out v3 NormalOut;

uniform sampler2D Sampler;
uniform f32 SpecularIntensity;
uniform f32 SpecularPower;

void main()
{
    WorldPosOut.xyz = WorldPos;
    if (UV.x == 0.0 && UV.y == 0)
    {
        DiffuseOut.rgb = V3(1, 1, 1);
    }
    else
    {
        DiffuseOut.rgb = texture(Sampler, UV).rgb;
    }

    WorldPosOut.a = SpecularIntensity;
    DiffuseOut.a = SpecularPower;

    NormalOut = normalize(Normal);
}
)FOO";

    Prog->ProgId = OpenGLCreateProgram(GlobalShaderHeaderCode, VertexCode, FragmentCode);
    Prog->WVPTransformId = glGetUniformLocation(Prog->ProgId, "WVPTransform");
    Prog->WTransformId = glGetUniformLocation(Prog->ProgId, "WTransform");
    Prog->SamplerId = glGetUniformLocation(Prog->ProgId, "Sampler");
    Prog->SpecularIntensityId = glGetUniformLocation(Prog->ProgId, "SpecularIntensity");
    Prog->SpecularPowerId = glGetUniformLocation(Prog->ProgId, "SpecularPower");
}

internal void SetProgParams(deferred_geometry_prog* Prog, m4 WVPTransform, m4 WTransform,
                            f32 SpecularIntensity, f32 SpecularPower)
{
    glUniform1i(Prog->SamplerId, 0);
    glUniformMatrix4fv(Prog->WVPTransformId, 1, false, WVPTransform.e);
    glUniformMatrix4fv(Prog->WTransformId, 1, false, WTransform.e);

    glUniform1f(Prog->SpecularIntensityId, SpecularIntensity);
    glUniform1f(Prog->SpecularPowerId, SpecularPower);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)(sizeof(v3)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)(sizeof(v3) + sizeof(v2)));
}

internal void ClearProgParams(deferred_geometry_prog* Prog)
{
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

internal void CompileDeferredStencilProg(deferred_stencil_prog* Prog)
{
    char* VertexCode = R"FOO(
layout(location = 0) in v3 VertPos;

uniform m4 WVPTransform;

void main()
{
    gl_Position = WVPTransform*V4(VertPos, 1);
}
)FOO";
    
    char* FragmentCode = R"FOO(
void main()
{
}
)FOO";

    Prog->ProgId = OpenGLCreateProgram(GlobalShaderHeaderCode, VertexCode, FragmentCode);
    Prog->WVPTransformId = glGetUniformLocation(Prog->ProgId, "WVPTransform");
}

internal void BeginProgram(deferred_stencil_prog* Prog, m4 WVPTransform)
{
    glDrawBuffer(GL_NONE);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 0, 0);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    glUseProgram(OpenGL.DeferredStencilProg.ProgId);

    glUniformMatrix4fv(Prog->WVPTransformId, 1, false, WVPTransform.e);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)0);
}

internal void EndProgram(deferred_stencil_prog* Prog)
{
    glUseProgram(0);
    glDisableVertexAttribArray(0);
}

internal void CompileDeferredPointLightProg(deferred_point_light_prog* Prog)
{
    char* VertexCode = R"FOO(
layout(location = 0) in v3 VertPos;

uniform m4 WVPTransform;

void main()
{
    gl_Position = WVPTransform*V4(VertPos, 1);
}
)FOO";

    char* FragmentCode = R"FOO(

struct point_light
{
    v3 Color;
    f32 AmbientIntensity;
    f32 DiffuseIntensity;

    f32 ConstantAttenuation;
    f32 LinearAttenuation;
    f32 ExpAttenuation;

    v3 Pos;
};

uniform v2 ScreenSize;
uniform sampler2D PosSampler;
uniform sampler2D DiffuseSampler;
uniform sampler2D NormalSampler;

uniform v3 EyeWorldPos;
uniform point_light PointLight;

uniform f32 SpecularIntensity;
uniform f32 SpecularPower;

out v4 PixelColor;

v3 CalcLightColor(v3 WorldPos, v3 Normal, v3 LightDir, f32 AmbientIntensity, f32 DiffuseIntensity,
                  f32 SpecularIntensity, f32 SpecularPower, v3 LightColor)
{
    v3 Result;

    f32 DiffuseFactor = dot(Normal, -LightDir);
    v3 DiffuseColor = V3(0, 0, 0);
    v3 SpecularColor = V3(0, 0, 0);
    if (DiffuseFactor > 0)
    {
        DiffuseColor = V3(LightColor*DiffuseIntensity*DiffuseFactor);

        v3 VertexToEye = normalize(EyeWorldPos - WorldPos);
        v3 LightReflectionDir = normalize(reflect(LightDir, Normal));
        f32 SpecularFactor = dot(VertexToEye, LightReflectionDir);
        if (SpecularFactor > 0)
        {
            SpecularFactor = pow(SpecularFactor, SpecularPower);
            SpecularColor = V3(LightColor * SpecularIntensity * SpecularFactor);
        }
    }

    Result = AmbientIntensity*LightColor + DiffuseColor + SpecularColor;
    return Result;
}

void main()
{
    v2 UV = gl_FragCoord.xy / ScreenSize;
    v4 WorldPos = texture(PosSampler, UV).rgba;
    v4 Diffuse = texture(DiffuseSampler, UV).rgba;
    v3 Normal = normalize(texture(NormalSampler, UV).rgb);

    v3 PointLightColor;
    {
        v3 LightDir = WorldPos.xyz - PointLight.Pos;
        f32 Distance = length(LightDir);
        LightDir = normalize(LightDir);
        f32 Attenuation = (PointLight.ConstantAttenuation +
                           PointLight.LinearAttenuation*Distance +
                           PointLight.ExpAttenuation*Distance*Distance);
        PointLightColor = CalcLightColor(WorldPos.xyz, Normal, LightDir, PointLight.AmbientIntensity,
                                         PointLight.DiffuseIntensity, WorldPos.a, Diffuse.a,
                                         PointLight.Color);
        PointLightColor = PointLightColor / Attenuation;
    }

    PixelColor = V4(Diffuse.rgb, 1.0)*V4(PointLightColor, 1);
}
)FOO";
    
    Prog->ProgId = OpenGLCreateProgram(GlobalShaderHeaderCode, VertexCode, FragmentCode);
    Prog->WVPTransformId = glGetUniformLocation(Prog->ProgId, "WVPTransform");
    Prog->ScreenSizeId = glGetUniformLocation(Prog->ProgId, "ScreenSize");
    Prog->PosSamplerId = glGetUniformLocation(Prog->ProgId, "PosSampler");
    Prog->DiffuseSamplerId = glGetUniformLocation(Prog->ProgId, "DiffuseSampler");
    Prog->NormalSamplerId = glGetUniformLocation(Prog->ProgId, "NormalSampler");

    Prog->EyeWorldPosId = glGetUniformLocation(Prog->ProgId, "EyeWorldPos");
    Prog->PointLightColorId = glGetUniformLocation(Prog->ProgId, "PointLight.Color");
    Prog->PointLightAmbientIntensityId = glGetUniformLocation(Prog->ProgId, "PointLight.AmbientIntensity");
    Prog->PointLightDiffuseIntensityId = glGetUniformLocation(Prog->ProgId, "PointLight.DiffuseIntensity");
    Prog->PointLightConstantAttenuationId = glGetUniformLocation(Prog->ProgId, "PointLight.ConstantAttenuation");
    Prog->PointLightLinearAttenuationId = glGetUniformLocation(Prog->ProgId, "PointLight.LinearAttenuation");
    Prog->PointLightExpAttenuationId = glGetUniformLocation(Prog->ProgId, "PointLight.ExpAttenuation");
    Prog->PointLightPosId = glGetUniformLocation(Prog->ProgId, "PointLight.Pos");
}

internal void BeginProgram(deferred_point_light_prog* Prog, game_render_commands* Commands,
                           m4 WVPTransform, point_light* PointLight)
{
    glDrawBuffer(GL_COLOR_ATTACHMENT4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[2]);

    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glUseProgram(Prog->ProgId);
    glUniform3f(Prog->EyeWorldPosId, -Commands->CameraMat.v[3].x, -Commands->CameraMat.v[3].y, -Commands->CameraMat.v[3].z);
    glUniform2f(Prog->ScreenSizeId, (f32)Commands->Settings.ResWidth, (f32)Commands->Settings.ResHeight);

    glUniformMatrix4fv(Prog->WVPTransformId, 1, false, &WVPTransform.e[0]);
    glUniform1i(Prog->PosSamplerId, GBufferAttribute_Position);
    glUniform1i(Prog->DiffuseSamplerId, GBufferAttribute_Diffuse);
    glUniform1i(Prog->NormalSamplerId, GBufferAttribute_Normal);

    glUniform3f(Prog->PointLightColorId, PointLight->Color.x, PointLight->Color.y, PointLight->Color.z);
    glUniform1f(Prog->PointLightAmbientIntensityId, PointLight->AmbientIntensity);
    glUniform1f(Prog->PointLightDiffuseIntensityId, PointLight->DiffuseIntensity);
    glUniform1f(Prog->PointLightConstantAttenuationId, PointLight->ConstantAttenuation);
    glUniform1f(Prog->PointLightLinearAttenuationId, PointLight->LinearAttenuation);
    glUniform1f(Prog->PointLightExpAttenuationId, PointLight->ExpAttenuation);
    glUniform3f(Prog->PointLightPosId, PointLight->Pos.x, PointLight->Pos.y, PointLight->Pos.z);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)0);
}

internal void EndProgram(deferred_point_light_prog* Prog)
{
    glDisableVertexAttribArray(0);
    
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
}

internal void CompileDeferredDirLightProg(deferred_dir_light_prog* Prog)
{
    char* VertexCode = R"FOO(
layout(location = 0) in v3 VertPos;

void main()
{
    gl_Position = V4(2.0f*VertPos, 1);
}
)FOO";

    char* FragmentCode = R"FOO(

struct dir_light
{
    v3 Color;
    f32 AmbientIntensity;
    f32 DiffuseIntensity;

    v3 Dir;
};

uniform v2 ScreenSize;
uniform sampler2D PosSampler;
uniform sampler2D DiffuseSampler;
uniform sampler2D NormalSampler;

uniform v3 EyeWorldPos;
uniform dir_light DirLight;

uniform f32 SpecularIntensity;
uniform f32 SpecularPower;

out v4 PixelColor;

v3 CalcLightColor(v3 WorldPos, v3 Normal, v3 LightDir, f32 AmbientIntensity, f32 DiffuseIntensity,
                  f32 SpecularIntensity, f32 SpecularPower, v3 LightColor)
{
    v3 Result;

    f32 DiffuseFactor = dot(Normal, -LightDir);
    v3 DiffuseColor = V3(0, 0, 0);
    v3 SpecularColor = V3(0, 0, 0);
    if (DiffuseFactor > 0)
    {
        DiffuseColor = V3(LightColor*DiffuseIntensity*DiffuseFactor);

        v3 VertexToEye = normalize(EyeWorldPos - WorldPos);
        v3 LightReflectionDir = normalize(reflect(LightDir, Normal));
        f32 SpecularFactor = dot(VertexToEye, LightReflectionDir);
        if (SpecularFactor > 0)
        {
            SpecularFactor = pow(SpecularFactor, SpecularPower);
            SpecularColor = V3(LightColor * SpecularIntensity * SpecularFactor);
        }
    }

    Result = AmbientIntensity*LightColor + DiffuseColor + SpecularColor;
    return Result;
}

void main()
{
    v2 UV = gl_FragCoord.xy / ScreenSize;
    v4 WorldPos = texture(PosSampler, UV).rgba;
    v4 Diffuse = texture(DiffuseSampler, UV).rgba;
    v3 Normal = normalize(texture(NormalSampler, UV).rgb);
    v3 DirLightColor = CalcLightColor(WorldPos.xyz, Normal, normalize(DirLight.Dir), 
                                      DirLight.AmbientIntensity, DirLight.DiffuseIntensity,
                                      WorldPos.a, Diffuse.a, DirLight.Color);

    PixelColor = V4(Diffuse.rgb, 1)*V4(DirLightColor, 1);
}
)FOO";
    
    Prog->ProgId = OpenGLCreateProgram(GlobalShaderHeaderCode, VertexCode, FragmentCode);
    Prog->ScreenSizeId = glGetUniformLocation(Prog->ProgId, "ScreenSize");
    Prog->PosSamplerId = glGetUniformLocation(Prog->ProgId, "PosSampler");
    Prog->DiffuseSamplerId = glGetUniformLocation(Prog->ProgId, "DiffuseSampler");
    Prog->NormalSamplerId = glGetUniformLocation(Prog->ProgId, "NormalSampler");

    Prog->EyeWorldPosId = glGetUniformLocation(Prog->ProgId, "EyeWorldPos");
    Prog->DirLightColorId = glGetUniformLocation(Prog->ProgId, "DirLight.Color");
    Prog->DirLightAmbientIntensityId = glGetUniformLocation(Prog->ProgId, "DirLight.AmbientIntensity");
    Prog->DirLightDiffuseIntensityId = glGetUniformLocation(Prog->ProgId, "DirLight.DiffuseIntensity");
    Prog->DirLightDirId = glGetUniformLocation(Prog->ProgId, "DirLight.Dir");
}

internal void BeginProgram(deferred_dir_light_prog* Prog, game_render_commands* Commands,
                           dir_light* DirLight)
{
    glDrawBuffer(GL_COLOR_ATTACHMENT4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[2]);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    
    glUseProgram(Prog->ProgId);
    glUniform3f(Prog->EyeWorldPosId, -Commands->CameraMat.v[3].x, -Commands->CameraMat.v[3].y, -Commands->CameraMat.v[3].z);
    glUniform2f(Prog->ScreenSizeId, (f32)Commands->Settings.ResWidth, (f32)Commands->Settings.ResHeight);
    
    glUniform1i(Prog->PosSamplerId, GBufferAttribute_Position);
    glUniform1i(Prog->DiffuseSamplerId, GBufferAttribute_Diffuse);
    glUniform1i(Prog->NormalSamplerId, GBufferAttribute_Normal);

    glUniform3f(Prog->DirLightColorId, DirLight->Color.x, DirLight->Color.y, DirLight->Color.z);
    glUniform1f(Prog->DirLightAmbientIntensityId, DirLight->AmbientIntensity);
    glUniform1f(Prog->DirLightDiffuseIntensityId, DirLight->DiffuseIntensity);
    glUniform3f(Prog->DirLightDirId, DirLight->Dir.x, DirLight->Dir.y, DirLight->Dir.z);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*)0);
}

internal void EndProgram(deferred_dir_light_prog* Prog)
{
    glDisableVertexAttribArray(0);
    
    glDisable(GL_BLEND);
}

GL_LOAD_TEXTURE_TO_GPU(GLLoadTextureToGpu)
{
    u32 Result = 0;
    
    glGenTextures(1, &Result);
    glBindTexture(GL_TEXTURE_2D, Result);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, Width, Height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, Pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);

    GLCheckError();

    return Result;
}

GL_LOAD_MODEL_TO_GPU(GLLoadModelToGpu)
{
    u32 Result = 0;
    
    glGenBuffers(1, &Result);
    glBindBuffer(GL_ARRAY_BUFFER, Result);
    glBufferData(GL_ARRAY_BUFFER, VERTEX_SIZE*NumVertices, VertexArray, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    GLCheckError();

    return Result;
}

internal void InitOpenGL(game_render_settings Settings, game_render_commands* Commands, void* Mem,
                         u32 MemSize)
{
    glEnable(GL_FRAMEBUFFER_SRGB);

    CompileDeferredGeometryProg(&OpenGL.DeferredGeometryProg);
    CompileDeferredStencilProg(&OpenGL.DeferredStencilProg);
    CompileDeferredPointLightProg(&OpenGL.DeferredPointLightProg);
    CompileDeferredDirLightProg(&OpenGL.DeferredDirLightProg);
    
    // NOTE: Create a white texture
    {
        u32 WhitePixel = 0xFFFFFFFF;
        OpenGL.WhiteTextureId = GLLoadTextureToGpu(1, 1, &WhitePixel);
        Commands->WhiteTextureId = OpenGL.WhiteTextureId;
    }
    
    // NOTE: Create a box model
    f32 RectVerts[] =
    {
        -0.5, -0.5, 0, 0, 0, 0, 0, 0,
        -0.5, 0.5, 0, 0, 1, 0, 0, 0,
        0.5, 0.5, 0, 1, 1, 0, 0, 0,
        -0.5, -0.5, 0, 0, 0, 0, 0, 0,
        0.5, 0.5, 0, 1, 1, 0, 0, 0,
        0.5, -0.5, 0, 1, 0, 0, 0, 0,
    };

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glGenBuffers(1, &OpenGL.SquareVBO);
    glBindBuffer(GL_ARRAY_BUFFER, OpenGL.SquareVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(RectVerts), RectVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // NOTE: Init gbuffer    
    {
        glGenFramebuffers(1, &OpenGL.GBufferFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OpenGL.GBufferFbo);

        glGenTextures(GBufferAttribute_NumElements, OpenGL.GBufferTextures);
        glGenTextures(1, &OpenGL.GBufferDepthTexture);
        glGenTextures(1, &OpenGL.GBufferFinalTexture);

        // NOTE: GBuffer position
        glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Position]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, Settings.ResWidth, Settings.ResHeight, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);            
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GBufferAttribute_Position,
                               GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Position], 0);

        // NOTE: GBuffer Diffuse + Specular
        glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Diffuse]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, Settings.ResWidth, Settings.ResHeight, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);            
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GBufferAttribute_Diffuse,
                               GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Diffuse], 0);

        // NOTE: GBuffer normal
        glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Normal]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, Settings.ResWidth, Settings.ResHeight, 0, GL_RGB, GL_FLOAT, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);            
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GBufferAttribute_Normal,
                               GL_TEXTURE_2D, OpenGL.GBufferTextures[GBufferAttribute_Normal], 0);

        // NOTE: Depth buffer
        glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferDepthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, Settings.ResWidth, Settings.ResHeight, 0,
                     GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OpenGL.GBufferDepthTexture, NULL);

        // NOTE: Final Color
        glBindTexture(GL_TEXTURE_2D, OpenGL.GBufferFinalTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Settings.ResWidth, Settings.ResHeight, 0, GL_RGB,
                     GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D,
                               OpenGL.GBufferFinalTexture, 0);
        
        GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (Status != GL_FRAMEBUFFER_COMPLETE)
        {
            InvalidCodePath;
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }
    
    // NOTE: Init our rendered texture and fbo
    {
        glGenFramebuffers(1, &OpenGL.FrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, OpenGL.FrameBuffer);

        // NOTE: Add color texture
        glGenTextures(1, &OpenGL.FrameTexture);
        glBindTexture(GL_TEXTURE_2D, OpenGL.FrameTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8_EXT, Settings.ResWidth, Settings.ResHeight, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               OpenGL.FrameTexture, 0);

        // NOTE: Add depth texture
        glGenTextures(1, &OpenGL.FrameDepth);
        glBindTexture(GL_TEXTURE_2D, OpenGL.FrameDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, Settings.ResWidth, Settings.ResHeight, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, OpenGL.FrameDepth, 0);
    
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            InvalidCodePath;
        }
    
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_FRAMEBUFFER_SRGB);
        GLCheckError();
    }

    GLCheckError();
}

internal void DeferredExecuteCommands(game_render_commands* Commands)
{
    mm CurrentByte = 0;
    mem_double_arena Arena = Commands->Arena;
    game_render_settings Settings = Commands->Settings;
    m4 VPTransform = Commands->ProjMat*Commands->CameraMat;

    glViewport(0, 0, Settings.ResWidth, Settings.ResHeight);

    // NOTE: Clear the final color texture
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OpenGL.GBufferFbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // NOTE: Geometry pass
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OpenGL.GBufferFbo);
    GLenum DrawBuffers[] =
        {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
        };
    glDrawBuffers(GBufferAttribute_NumElements, DrawBuffers);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glUseProgram(OpenGL.DeferredGeometryProg.ProgId);

    while (CurrentByte < Arena.UsedTop)
    {
        u32* CommandType = (u32*)((u8*)Arena.Mem + CurrentByte);
        CurrentByte += sizeof(u32);

        void* RenderCommand = (u8*)Arena.Mem + CurrentByte;
        
        switch (*CommandType)
        {
            case RenderCmdType_Mesh:
            {
                model_data* ModelData = (model_data*)RenderCommand;
                CurrentByte += sizeof(model_data);

                glActiveTexture(GL_TEXTURE0);
                
                i32 TextureId = ModelData->TextureId;
                if (TextureId == -1)
                {
                    TextureId = OpenGL.WhiteTextureId;
                }
                glBindTexture(GL_TEXTURE_2D, (GLuint)TextureId);
                glBindBuffer(GL_ARRAY_BUFFER, ModelData->VboId);

                SetProgParams(&OpenGL.DeferredGeometryProg, VPTransform*ModelData->WTransform,
                              ModelData->WTransform, ModelData->SpecularIntensity,
                              ModelData->SpecularPower);
                glDrawArrays(GL_TRIANGLES, 0, ModelData->NumVertices);
                ClearProgParams(&OpenGL.DeferredGeometryProg);

                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindTexture(GL_TEXTURE_2D, 0);

                GLCheckError();
            } break;
        }
    }

    glDepthMask(GL_FALSE);
    glUseProgram(0);

    // NOTE: Stencil test
    glEnable(GL_STENCIL_TEST);

    // NOTE: The lights are stored in reverse at the bottom of the arena, traverse backwards
    CurrentByte = Arena.Size;
    while (CurrentByte > Arena.Size - Arena.UsedBot)
    {
        CurrentByte -= sizeof(u32);
        u32* CommandType = (u32*)((u8*)Arena.Mem + CurrentByte);
        
        switch (*CommandType)
        {
            case RenderCmdType_PointLight:
            {
                CurrentByte -= sizeof(point_light);
                point_light* PointLight = (point_light*)((u8*)Arena.Mem + CurrentByte);
                f32 SphereScale = GetPointLightSphereScale(PointLight);
                m4 WVPTransform = VPTransform*(PosMat(PointLight->Pos)*ScaleMat(SphereScale, SphereScale, SphereScale));
                
                glBindBuffer(GL_ARRAY_BUFFER, PointLight->GLSphereId);

                // NOTE: Stencil pass on the point light
                BeginProgram(&OpenGL.DeferredStencilProg, WVPTransform);
                glDrawArrays(GL_TRIANGLES, 0, PointLight->NumVertices);
                EndProgram(&OpenGL.DeferredStencilProg);

                // NOTE: Point Lighting pass
                BeginProgram(&OpenGL.DeferredPointLightProg, Commands, WVPTransform, PointLight);
                glDrawArrays(GL_TRIANGLES, 0, PointLight->NumVertices);
                EndProgram(&OpenGL.DeferredPointLightProg);

                glBindBuffer(GL_ARRAY_BUFFER, 0);
            } break;

            case RenderCmdType_DirLight:
            {
                CurrentByte -= sizeof(dir_light);
                dir_light* DirLight = (dir_light*)((u8*)Arena.Mem + CurrentByte);
            } break;
        }
    }
    
    glDisable(GL_STENCIL_TEST);
    glCullFace(GL_FRONT);

    // NOTE: Loop over and render directional lights
    CurrentByte = Arena.Size;
    while (CurrentByte > Arena.Size - Arena.UsedBot)
    {
        CurrentByte -= sizeof(u32);
        u32* CommandType = (u32*)((u8*)Arena.Mem + CurrentByte);
        
        switch (*CommandType)
        {
            case RenderCmdType_PointLight:
            {
                CurrentByte -= sizeof(point_light);
            } break;

            case RenderCmdType_DirLight:
            {
                CurrentByte -= sizeof(dir_light);
                dir_light* DirLight = (dir_light*)((u8*)Arena.Mem + CurrentByte);

                // NOTE: Render directional light
                glBindBuffer(GL_ARRAY_BUFFER, OpenGL.SquareVBO);
                BeginProgram(&OpenGL.DeferredDirLightProg, Commands, DirLight);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                EndProgram(&OpenGL.DeferredDirLightProg);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            } break;
        }
    }
    
    // NOTE: Final pass, draw to final texture
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OpenGL.FrameBuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL.GBufferFbo);
    glReadBuffer(GL_COLOR_ATTACHMENT4);
    glBlitFramebuffer(0, 0, Settings.ResWidth, Settings.ResHeight, 0, 0, Settings.ResWidth,
                      Settings.ResHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

#if 0
    // NOTE: Outputs the GBuffer data
    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL.GBufferFbo);

    GLsizei HalfWidth = (GLsizei)(Settings.ResWidth / 2.0f);
    GLsizei HalfHeight = (GLsizei)(Settings.ResHeight / 2.0f);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + GBufferAttribute_Position);
    glBlitFramebuffer(0, 0, Settings.ResWidth, Settings.ResHeight, 0, 0, HalfWidth, HalfHeight,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + GBufferAttribute_Diffuse);
    glBlitFramebuffer(0, 0, Settings.ResWidth, Settings.ResHeight, 0, HalfHeight,
                      HalfWidth, Settings.ResHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glReadBuffer(GL_COLOR_ATTACHMENT0 + GBufferAttribute_Normal);
    glBlitFramebuffer(0, 0, Settings.ResWidth, Settings.ResHeight, HalfWidth, HalfHeight,
                      Settings.ResWidth, Settings.ResHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
#endif
  
    ClearArena(&Commands->Arena);
    GLCheckError();
}
