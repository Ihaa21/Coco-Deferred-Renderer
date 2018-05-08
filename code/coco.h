#if !defined(COCO_H)

struct camera
{
    v2 CurrentRotation;
    v3 Pos;
};

struct game_state
{    
    mem_arena Arena;
    assets Assets;
    game_render_commands* Commands;
    
    v2 PrevMouseP;
    camera Camera;
};

#define COCO_H
#endif
