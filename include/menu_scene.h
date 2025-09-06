#ifndef MENU_SCENE_H
#define MENU_SCENE_H
#include "scene.h"
#include "canvas.h"
#include "menu_bg.h"
#include "gui.h"
#include "gui_paged_grid.h"

typedef struct MenuScene {
    Scene        base;

    Canvas       scene;      // offscreen menu canvas
    MenuBg       bg;

    GuiContext   gui;
    GuiPagedGrid grid;
    mlx_texture_t* ui_item_skin;
    mlx_texture_t* ui_pager_skin;

    // file list
    char**  map_files;
    size_t  map_file_count;

    struct MapSelectUD* ud;  // forward-declared in .c
} MenuScene;

void menu_scene_init_instance(MenuScene* ms);
#endif