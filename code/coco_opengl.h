#if !defined(COCO_OPENGL_H)

struct deferred_geometry_prog
{
    GLuint ProgId;
    
    GLuint WVPTransformId;
    GLuint WTransformId;
    GLuint SamplerId;

    GLuint SpecularIntensityId;
    GLuint SpecularPowerId;
};

struct deferred_stencil_prog
{
    GLuint ProgId;

    GLuint WVPTransformId;
};

struct deferred_point_light_prog
{
    GLuint ProgId;

    GLuint WVPTransformId;
    GLuint ScreenSizeId;
    GLuint PosSamplerId;
    GLuint DiffuseSamplerId;
    GLuint NormalSamplerId;

    GLuint EyeWorldPosId;
    
    GLuint PointLightColorId;
    GLuint PointLightAmbientIntensityId;
    GLuint PointLightDiffuseIntensityId;
    GLuint PointLightConstantAttenuationId;
    GLuint PointLightLinearAttenuationId;
    GLuint PointLightExpAttenuationId;
    GLuint PointLightPosId;
};

struct deferred_dir_light_prog
{
    GLuint ProgId;

    GLuint ScreenSizeId;
    GLuint PosSamplerId;
    GLuint DiffuseSamplerId;
    GLuint NormalSamplerId;

    GLuint EyeWorldPosId;
    
    GLuint DirLightColorId;
    GLuint DirLightAmbientIntensityId;
    GLuint DirLightDiffuseIntensityId;
    GLuint DirLightDirId;
};

enum GBufferAttribute
{
    GBufferAttribute_Position,
    GBufferAttribute_Diffuse,
    GBufferAttribute_Normal,

    GBufferAttribute_NumElements,
};

#define VERTEX_SIZE (2*sizeof(v3) + sizeof(v2))
#define OPENGL_MAX_NUM_QUADS 20000
#define OPENGL_NUM_INSTANCE_BUFFERS 8
struct opengl
{
    GLuint FrameBuffer;
    GLuint FrameTexture;
    GLuint FrameDepth;

    // NOTE: Deferred Rendering State
    GLuint GBufferFbo;
    GLuint GBufferTextures[GBufferAttribute_NumElements];
    GLuint GBufferFinalTexture;
    GLuint GBufferDepthTexture;
    
    GLuint WhiteTextureId;
    GLuint SquareVBO;
    
    deferred_geometry_prog DeferredGeometryProg;
    deferred_stencil_prog DeferredStencilProg;
    deferred_point_light_prog DeferredPointLightProg;
    deferred_dir_light_prog DeferredDirLightProg;
};

#define COCO_OPENGL_H
#endif
