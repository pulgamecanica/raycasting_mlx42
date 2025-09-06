#ifndef APP_H
#define APP_H

#include <MLX42/MLX42.h>
#include "canvas.h"
#include "scene_manager.h"

typedef struct App {
    mlx_t*        mlx;
    Canvas        screen;      // only image attached to the window
    double        last_time;

    SceneManager  sm;
} App;

App* app_create(int width, int height, const char* title);
void  app_run(App* app);
void  app_destroy(App* app);
#endif