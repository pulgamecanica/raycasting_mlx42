#include "app.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef WEB
# include <emscripten.h>
static mlx_t* g_mlx = NULL; /* WEB needs this for emscripten main loop */
static void emscripten_main_loop(void) { mlx_loop(g_mlx); }
#endif

static void on_resize_hook(int32_t w, int32_t h, void* param) {
    App* app = (App*)param;
    // keep screen canvas exactly window-sized
    canvas_destroy(&app->screen);
    canvas_init(&app->screen, app->mlx, w, h);
    if (app->screen.img->count == 0) {
        if (mlx_image_to_window(app->mlx, app->screen.img, 0, 0) < 0) {
            puts(mlx_strerror(mlx_errno)); exit(EXIT_FAILURE);
        }
    }
    // let all scenes react (so switching back later is instant)
    for (int i = 0; i < SCN_COUNT; ++i) {
        Scene* sc = app->sm.scenes[i];
        if (sc) scene_resize(sc, w, h);
    }
}

static void loop(void* param) {
    App* app = (App*)param;
    double now = mlx_get_time();
    float  dt  = (float)(now - app->last_time);
    if (dt > 0.05f) dt = 0.05f;
    app->last_time = now;

    Scene* sc = sm_active(&app->sm);
    if (sc) {
        scene_update(sc, now, dt);
        scene_render(sc);
    }

    // apply any requested scene change at safe point
    sm_process_switch(&app->sm);
}

App* app_create(int width, int height, const char* title) {
    App* app = (App*)calloc(1, sizeof(App));
    if (!app) return NULL;

    app->mlx = mlx_init(width, height, title, true);
    if (!app->mlx) { puts(mlx_strerror(mlx_errno)); free(app); return NULL; }

    canvas_init(&app->screen, app->mlx, width, height);
    if (mlx_image_to_window(app->mlx, app->screen.img, 0, 0) < 0) {
        puts(mlx_strerror(mlx_errno)); mlx_terminate(app->mlx); free(app); return NULL;
    }

    sm_init(&app->sm, app);
    app->last_time = mlx_get_time();

    mlx_resize_hook(app->mlx, on_resize_hook, app);
    mlx_loop_hook(app->mlx, loop, app);
    return app;
}

void app_run(App* app) {
#ifdef WEB
    g_mlx = app->mlx;
    emscripten_set_main_loop(emscripten_main_loop, 0, true);
#else
    mlx_loop(app->mlx);
#endif
}

void app_destroy(App* app) {
    if (!app) return;
    // destroy scenes
    for (int i = 0; i < SCN_COUNT; ++i) {
        Scene* sc = app->sm.scenes[i];
        if (sc) scene_destroy(sc);
    }
    canvas_destroy(&app->screen);
    mlx_terminate(app->mlx);
    free(app);
}
