#include "menu_scene.h"
#include "game_scene.h"
#include "scene_manager.h"
#include "app.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LEVELS_DIR
#define LEVELS_DIR "assets/maps"
#endif

typedef struct MapSelectUD {
    struct App* app;
    GameScene*  game;     // to queue map load (on select, change to map)
    size_t      index;
    const char* path;     // points into MenuScene.map_files[i]
} MapSelectUD;

static void on_select_map(void* ud) {
    MapSelectUD* sel = (MapSelectUD*)ud;
    if (!sel || !sel->app || !sel->game || !sel->path) return;
    // DON’T parse here; just queue & switch. parsing happens when GAME scene is active.
    game_scene_queue_load(sel->game, sel->path);
    sm_request_change(&sel->app->sm, SCN_GAME);  // deferred switch
}

static void ms_build_items(MenuScene* ms, GameScene* gs) {
    // discover files
    if (map_list_levels(LEVELS_DIR, &ms->map_files, &ms->map_file_count) != 0) {
        ms->map_files = NULL;
        ms->map_file_count = 0;
    }

    size_t n = ms->map_file_count ? ms->map_file_count : 3;
    GuiPagedGridItem* items = (GuiPagedGridItem*)calloc(n, sizeof(GuiPagedGridItem));
    if (!items) return;

    MapSelectUD* uds = (MapSelectUD*)calloc(n, sizeof(MapSelectUD)); // owned by scene
    ms->ud = uds;

    if (ms->map_file_count == 0) {
        items[0] = (GuiPagedGridItem){ "Demo 1", on_select_map, NULL };
        items[1] = (GuiPagedGridItem){ "Demo 2", on_select_map, NULL };
        items[2] = (GuiPagedGridItem){ "Demo 3", on_select_map, NULL };
    } else {
        for (size_t i = 0; i < ms->map_file_count; ++i) {
            const char* path = ms->map_files[i];
            const char* base = strrchr(path, '/'); base = base ? base+1 : path;
            const char* dot  = strrchr(base, '.');
            char label[256];
            if (dot && (size_t)(dot - base) < sizeof(label)) {
                size_t L = (size_t)(dot - base);
                memcpy(label, base, L); label[L] = '\0';
            } else {
                strncpy(label, base, sizeof(label)-1);
                label[sizeof(label)-1] = '\0';
            }
            // grid will strdup label internally
            items[i].label    = label;
            items[i].on_click = on_select_map;
            uds[i].app  = ms->base.app;
            uds[i].game = gs;
            uds[i].index= i;
            uds[i].path = path;
            items[i].userdata = &uds[i];
        }
    }

    gui_paged_grid_set_items(&ms->grid, items, n);
    gui_paged_grid_mount(&ms->gui, &ms->grid);
    free(items);
}

static void ms_on_init(Scene* s, struct App* app) {
    MenuScene* ms = (MenuScene*)s;
    s->app = app;

    // offscreen buffer sized to window
    canvas_init(&ms->scene, app->mlx, app->mlx->width, app->mlx->height);

    // gui
    ms->gui = (GuiContext){ .mlx = app->mlx, .now = 0.0, .paths = NULL, .paths_len = 0 };
    gui_paths_add(&ms->gui, "assets");

    // bg
    if (!menu_bg_init(&ms->bg, (int)app->mlx->width, (int)app->mlx->height, 0xC0FFEEu)) {
        fprintf(stderr, "menu_bg_init failed\n");
        exit(EXIT_FAILURE);
    }

    // skins
    ms->ui_item_skin  = gui_load_png_from_paths(&ms->gui, "button_skin.png");
    ms->ui_pager_skin = ms->ui_item_skin;

    GuiNineSlice item_n = { .left=8,.right=8,.top=8,.bottom=8,.center_fill=true,.center_color=gui_rgba(200,200,200,255) };
    GuiNineSlice pager_n= { .left=8,.right=8,.top=8,.bottom=8,.center_fill=true,.center_color=gui_rgba(0,0,0,255) };

    GuiPagedGridConfig cfg = {
        .x=0,.y=0,.w=640,.h=420, .center_h=true,.center_v=true,
        .cols=2,.rows=2,.gap=16,.pager_h=20,
        .item_skin_tex=ms->ui_item_skin,.item_skin_cfg=item_n,
        .pager_skin_tex=ms->ui_pager_skin,.pager_skin_cfg=pager_n,
    };
    if (!gui_paged_grid_init(&ms->gui, &ms->grid, cfg)) {
        fprintf(stderr, "gui_paged_grid_init failed\n"); exit(EXIT_FAILURE);
    }

    // need the Game scene instance to queue loads
    GameScene* gs = (GameScene*)app->sm.scenes[SCN_GAME];
    ms_build_items(ms, gs);

    // hide all GUI by default; scene_show will enable
    gui_paged_grid_set_enabled(&ms->grid, false);
}

static void ms_on_show(Scene* s) {
    MenuScene* ms = (MenuScene*)s;
    gui_paged_grid_set_enabled(&ms->grid, true);
}

static void ms_on_hide(Scene* s) {
    MenuScene* ms = (MenuScene*)s;
    gui_paged_grid_set_enabled(&ms->grid, false);
}

static void ms_on_update(Scene* s, double now, float dt) {
    MenuScene* ms = (MenuScene*)s;
    gui_begin_frame(&ms->gui);
    menu_bg_update(&ms->bg, now, dt);
    gui_paged_grid_update(&ms->gui, &ms->grid);
}

static void ms_on_render(Scene* s) {
    MenuScene* ms = (MenuScene*)s;
    menu_bg_render(&ms->bg, ms->scene.img);
    canvas_copy(&s->app->screen, &ms->scene, 0, 0);
}

static void ms_on_resize(Scene* s, int w, int h) {
    MenuScene* ms = (MenuScene*)s;
    canvas_destroy(&ms->scene);
    canvas_init(&ms->scene, s->app->mlx, w, h);
    menu_bg_resize(&ms->bg, w, h);
}

static void ms_on_destroy(Scene* s) {
    MenuScene* ms = (MenuScene*)s;
    gui_paged_grid_free(&ms->gui, &ms->grid);
    if (ms->ui_item_skin)  mlx_delete_texture(ms->ui_item_skin);
    if (ms->ui_pager_skin && ms->ui_pager_skin != ms->ui_item_skin)
        mlx_delete_texture(ms->ui_pager_skin);
    map_free_paths(ms->map_files, ms->map_file_count);
    free(ms->ud);
    canvas_destroy(&ms->scene);
    menu_bg_free(&ms->bg);
}

void menu_scene_init_instance(MenuScene* ms) {
    memset(ms, 0, sizeof(*ms));
    ms->base.on_init   = ms_on_init;
    ms->base.on_show   = ms_on_show;
    ms->base.on_hide   = ms_on_hide;
    ms->base.on_update = ms_on_update;
    ms->base.on_render = ms_on_render;
    ms->base.on_resize = ms_on_resize;
    ms->base.on_destroy= ms_on_destroy;
}
