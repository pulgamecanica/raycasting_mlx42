#include "game_scene.h"
#include "app.h"
#include "raycast.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static void gs_on_init(Scene* s, struct App* app) {
    GameScene* gs = (GameScene*)s;
    s->app = app;

    // init buffers
    canvas_init(&gs->scene,  app->mlx, app->mlx->width,  app->mlx->height);
    canvas_init(&gs->minimap,app->mlx, 1, 1); // resized after map load

    // default world, later will be changed
    extern const int WORLD_DATA[];
    gs->map.w = WORLD_W;
    gs->map.h = WORLD_H;
    gs->map.data = WORLD_DATA;

    gs->cam.pos   = (Vec2f){ 12.0f, 12.0f };
    gs->cam.dir   = (Vec2f){ -1.0f, 0.0f };
    gs->cam.plane = (Vec2f){  0.0f, 0.66f };
}

static void gs_on_show(Scene* s) {
    (void)s; // nothing to enable; we render into App->screen
}

static void gs_on_hide(Scene* s) {
    (void)s; // nothing to disable
}

static void load_map(GameScene* gs, const char* path) {
    if (!path) return;
    int* data = NULL; int w=0,h=0; char* err=NULL;
    if (map_parse_cub3d_file(path, &data, &w, &h, &err) != 0) {
        fprintf(stderr, "Failed to load map '%s': %s\n", path, err?err:"parse error");
        free(err);
        return;
    }
    // replace map
    if (gs->map.data != NULL && gs->map.data != WORLD_DATA) free((void*)gs->map.data);
    gs->map.data = data; gs->map.w = w; gs->map.h = h;
    // resize minimap
    canvas_destroy(&gs->minimap);
    canvas_init(&gs->minimap, gs->base.app->mlx, gs->map.w * 6, gs->map.h * 6);
}

static void gs_on_update(Scene* s, double now, float dt) {
    GameScene* gs = (GameScene*)s;
    (void)now;

    // late apply of pending map load when scene is active
    if (gs->pending_map_path) {
        load_map(gs, gs->pending_map_path);
        gs->pending_map_path = NULL;
    }

    // input (WASD/LR) kept from your code:
    float move = 3.0f * dt, rot = 2.0f * dt;
    mlx_t* mlx = gs->base.app->mlx;
    Vec2f strafe = (Vec2f){ -gs->cam.dir.y, gs->cam.dir.x };
    if (mlx_is_key_down(mlx, MLX_KEY_W)) {
        Vec2f n = { gs->cam.pos.x + gs->cam.dir.x * move, gs->cam.pos.y + gs->cam.dir.y * move };
        if (!map_is_wall(&gs->map, (int)n.x, (int)gs->cam.pos.y)) gs->cam.pos.x = n.x;
        if (!map_is_wall(&gs->map, (int)gs->cam.pos.x, (int)n.y)) gs->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(mlx, MLX_KEY_S)) {
        Vec2f n = { gs->cam.pos.x - gs->cam.dir.x * move, gs->cam.pos.y - gs->cam.dir.y * move };
        if (!map_is_wall(&gs->map, (int)n.x, (int)gs->cam.pos.y)) gs->cam.pos.x = n.x;
        if (!map_is_wall(&gs->map, (int)gs->cam.pos.x, (int)n.y)) gs->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(mlx, MLX_KEY_A)) {
        Vec2f n = { gs->cam.pos.x - strafe.x * move, gs->cam.pos.y - strafe.y * move };
        if (!map_is_wall(&gs->map, (int)n.x, (int)gs->cam.pos.y)) gs->cam.pos.x = n.x;
        if (!map_is_wall(&gs->map, (int)gs->cam.pos.x, (int)n.y)) gs->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(mlx, MLX_KEY_D)) {
        Vec2f n = { gs->cam.pos.x + strafe.x * move, gs->cam.pos.y + strafe.y * move };
        if (!map_is_wall(&gs->map, (int)n.x, (int)gs->cam.pos.y)) gs->cam.pos.x = n.x;
        if (!map_is_wall(&gs->map, (int)gs->cam.pos.x, (int)n.y)) gs->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(mlx, MLX_KEY_LEFT)) {
        float cs = cosf(rot), sn = sinf(rot);
        Vec2f d = gs->cam.dir, p = gs->cam.plane;
        gs->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        gs->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }
    if (mlx_is_key_down(mlx, MLX_KEY_RIGHT)) {
        float cs = cosf(-rot), sn = sinf(-rot);
        Vec2f d = gs->cam.dir, p = gs->cam.plane;
        gs->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        gs->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }

    if (mlx_is_key_down(mlx, MLX_KEY_M)) {
        sm_request_change(&gs->base.app->sm, SCN_MENU);
    }
}

static void gs_on_render(Scene* s) {
    GameScene* gs = (GameScene*)s;
    // your existing renderers:
    render_scene(&gs->scene, &gs->map, &gs->cam);
    draw_minimap(&gs->minimap, &gs->map, &gs->cam, 6);

    // composite into App screen
    // for (int x=0;x<100;++x)
    //     for (int y=0;y<100;++y)
    //         canvas_put(&gs->minimap, x, y, rgba(255,100,100,255));

    canvas_copy(&s->app->screen, &gs->scene, 0, 0);
    canvas_copy(&s->app->screen, &gs->minimap, 8, 8);
}

static void gs_on_resize(Scene* s, int w, int h) {
    GameScene* gs = (GameScene*)s;
    canvas_destroy(&gs->scene);
    canvas_init(&gs->scene, s->app->mlx, w, h);
    // minimap will be resized on map load; optional: keep scale here
}

static void gs_on_destroy(Scene* s) {
    GameScene* gs = (GameScene*)s;
    if (gs->map.data && gs->map.data != WORLD_DATA) free((void*)gs->map.data);
    canvas_destroy(&gs->minimap);
    canvas_destroy(&gs->scene);
}

void game_scene_init_instance(GameScene* gs) {
    memset(gs, 0, sizeof(*gs));
    gs->base.on_init   = gs_on_init;
    gs->base.on_show   = gs_on_show;
    gs->base.on_hide   = gs_on_hide;
    gs->base.on_update = gs_on_update;
    gs->base.on_render = gs_on_render;
    gs->base.on_resize = gs_on_resize;
    gs->base.on_destroy= gs_on_destroy;
}

void game_scene_queue_load(GameScene* gs, const char* map_path) {
    gs->pending_map_path = map_path;
}
