#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "scene.h"
#include "canvas.h"
#include "map.h"
#include "types.h"

typedef struct GameScene {
    Scene   base;

    // resources
    Canvas  scene;       // off-screen color buffer
    Canvas  minimap;

    GridMap map;
    Camera  cam;

    // map to load on next show (NULL = keep current)
    const char* pending_map_path;
} GameScene;

void game_scene_init_instance(GameScene* gs);
void game_scene_queue_load(GameScene* gs, const char* map_path); // called from menu callback
#endif