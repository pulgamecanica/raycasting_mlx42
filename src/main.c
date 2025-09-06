#include <MLX42/MLX42.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "canvas.h"
#include "map.h"
#include "menu_bg.h"
#include "raycast.h"
#include "gui.h"
#include "gui_paged_grid.h"

#define WIDTH  800
#define HEIGHT 600

#ifndef LEVELS_DIR
#define LEVELS_DIR "assets/maps"
#endif

typedef enum State {
    MENU,
    GAME,
} State;

typedef struct {
    struct App* app;       // forward use; actual App is below
    size_t index;          // which item in list
    const char* path;      // full file path (owned by App.map_files)
} MapSelectUD;

typedef struct Menu {
    Canvas          screen; /* the ONLY image attached to the window */
    Canvas          scene;  /* off-screen render target */
    MenuBg          menu_bg;
    GuiContext      gui;
    GuiPagedGrid    grid;
    mlx_texture_t*  ui_item_skin;
    mlx_texture_t*  ui_pager_skin;
} Menu;


typedef struct Game {
    Canvas   screen;   /* the ONLY image attached to the window */
    Canvas   scene;    /* off-screen render target */
    Canvas   minimap;  /* off-screen minimap buffer */
    GridMap  map;
    Camera   cam;
} Game;


typedef struct {
    mlx_t*  mlx;
    Game*    game;
    Menu*    menu;
    State   state;
    double  last_time;
    
    /// Settings
    char** map_files; // levels
    size_t map_file_count;

    MapSelectUD* map_ud; // same length as map_files, for button callbacks
    int selected_map_index; // -1 if none yet

    int* owned_map_data; // heap map currently in use (NULL if using WORLD_DATA)
} App;

static void init_menu_bg(App* app);
static void on_select_map(void* ud); // Forward declare
static void on_loop_gui(App *app);   // Forward declare
static void on_resize(int32_t w, int32_t h, void* param);
static void switch_to_game(App* a);
static void switch_to_menu(App* a);

#ifdef WEB
# include <emscripten.h>
static mlx_t* g_mlx = NULL; /* WEB needs this for emscripten main loop */
static void emscripten_main_loop(void) { mlx_loop(g_mlx); }
#endif

static void handle_general_input(App* a, float dt) {
    (void)dt;
    if (mlx_is_key_down(a->mlx, MLX_KEY_ESCAPE)) mlx_close_window(a->mlx);
}

static void handle_game_input(App* a, float dt) {
    float move = 3.0f * dt;      /* tiles per second */
    float rot  = 2.0f * dt;      /* radians per second */

    /* strafe vector is dir rotated 90° */
    Vec2f strafe = (Vec2f){ -a->game->cam.dir.y, a->game->cam.dir.x };
    if (mlx_is_key_down(a->mlx, MLX_KEY_M)) {
        switch_to_menu(a);
        return ;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_W)) {
        Vec2f n = { a->game->cam.pos.x + a->game->cam.dir.x * move, a->game->cam.pos.y + a->game->cam.dir.y * move };
        if (!map_is_wall(&a->game->map, (int)n.x, (int)a->game->cam.pos.y)) a->game->cam.pos.x = n.x;
        if (!map_is_wall(&a->game->map, (int)a->game->cam.pos.x, (int)n.y)) a->game->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_S)) {
        Vec2f n = { a->game->cam.pos.x - a->game->cam.dir.x * move, a->game->cam.pos.y - a->game->cam.dir.y * move };
        if (!map_is_wall(&a->game->map, (int)n.x, (int)a->game->cam.pos.y)) a->game->cam.pos.x = n.x;
        if (!map_is_wall(&a->game->map, (int)a->game->cam.pos.x, (int)n.y)) a->game->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_A)) {
        Vec2f n = { a->game->cam.pos.x - strafe.x * move, a->game->cam.pos.y - strafe.y * move };
        if (!map_is_wall(&a->game->map, (int)n.x, (int)a->game->cam.pos.y)) a->game->cam.pos.x = n.x;
        if (!map_is_wall(&a->game->map, (int)a->game->cam.pos.x, (int)n.y)) a->game->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_D)) {
        Vec2f n = { a->game->cam.pos.x + strafe.x * move, a->game->cam.pos.y + strafe.y * move };
        if (!map_is_wall(&a->game->map, (int)n.x, (int)a->game->cam.pos.y)) a->game->cam.pos.x = n.x;
        if (!map_is_wall(&a->game->map, (int)a->game->cam.pos.x, (int)n.y)) a->game->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_LEFT)) {
        float cs = cosf(rot), sn = sinf(rot);
        Vec2f d = a->game->cam.dir, p = a->game->cam.plane;
        a->game->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        a->game->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_RIGHT)) {
        float cs = cosf(-rot), sn = sinf(-rot);
        Vec2f d = a->game->cam.dir, p = a->game->cam.plane;
        a->game->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        a->game->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }
}

static void handle_menu_input(App* a, float dt) {
    (void)a;
    (void)dt;
}

static void handle_input(App* a, float dt) {
    handle_general_input(a, dt);

    switch (a->state) {
    case GAME:
        handle_game_input(a, dt);
        break;

    case MENU:
        handle_menu_input(a, dt);
        break;
    
    default:
        break;
    }
}

static void on_game_loop(App* a, double now, float dt) {
    (void)now;(void)dt;
    render_scene(&a->game->scene, &a->game->map, &a->game->cam);
    draw_minimap(&a->game->minimap, &a->game->map, &a->game->cam, 6);

    canvas_copy(&a->game->screen, &a->game->scene, 0, 0);       /* compose scene */
    canvas_copy(&a->game->screen, &a->game->minimap, 8, 8);     /* overlay minimap */
}

static void on_menu_loop(App* a, double now, float dt) {
    menu_bg_update(&a->menu->menu_bg, now, dt);
    menu_bg_render(&a->menu->menu_bg, a->menu->scene.img);
    canvas_copy(&a->menu->screen, &a->menu->scene, 0, 0);
}

static void on_loop(void* param) {
    App* a = (App*)param;

    double now = mlx_get_time();            /* MLX42 timer */
    float dt = (float)(now - a->last_time);
    if (dt > 0.05f) dt = 0.05f;             /* clamp to avoid tunneling on stalls */
    a->last_time = now;

    handle_input(a, dt);
    on_loop_gui(a);

    switch (a->state) {
    case GAME:
        on_game_loop(a, now, dt);
        break;

    case MENU:
        on_menu_loop(a, now, dt);
        break;
    
    default:
        break;
    }
}

static void on_select_map(void* ud) {
    MapSelectUD* sel = (MapSelectUD*)ud;
    if (!sel || !sel->app) return;
    App* app = (App*)sel->app;

    if (!sel->path) {
        fprintf(stderr, "[menu] no path bound to item (demo placeholder)\n");
        return;
    }

    int* data = NULL; int w = 0, h = 0; char* err = NULL;
    if (map_parse_cub3d_file(sel->path, &data, &w, &h, &err) != 0) {
        fprintf(stderr, "Failed to load map '%s': %s\n", sel->path, err ? err : "parse error");
        free(err);
        return;
    }

    // Successfully parsed: install map into the app (free previous if owned)
    if (app->owned_map_data) free(app->owned_map_data);
    app->owned_map_data = data;
    app->game->map.w = w;
    app->game->map.h = h;
    app->game->map.data = (const int*)app->owned_map_data; // assign

    app->selected_map_index = (int)sel->index;
    switch_to_game(app);
    fprintf(stderr, "Loaded map (%dx%d) from %s\n", w, h, sel->path);
}

static void set_map_files_from_directory(App* app) {
    if (map_list_levels(LEVELS_DIR, &app->map_files, &app->map_file_count) != 0) {
        app->map_files = NULL;
        app->map_file_count = 0;
    }

    // Prepare userdata array (one per item)
    if (app->map_file_count > 0) {
        app->map_ud = (MapSelectUD*)calloc(app->map_file_count, sizeof(MapSelectUD));
        if (!app->map_ud) { app->map_file_count = 0; }
    }
}

static void build_menu_items(App* app) {
    set_map_files_from_directory(app);
    
    // If no maps, set demo samples
    size_t n = app->map_file_count ? app->map_file_count : 3;
    GuiPagedGridItem* items = (GuiPagedGridItem*)calloc(n, sizeof(GuiPagedGridItem));
    if (!items) return;

    if (app->map_file_count == 0) {
        // Fallback: hardcoded entries until you add real files
        items[0] = (GuiPagedGridItem){ "Demo 1", on_select_map, NULL };
        items[1] = (GuiPagedGridItem){ "Demo 2", on_select_map, NULL };
        items[2] = (GuiPagedGridItem){ "Demo 3", on_select_map, NULL };
    } else {
        for (size_t i = 0; i < app->map_file_count; ++i) {
            const char* path = app->map_files[i];
            const char* base = strrchr(path, '/'); base = base ? base+1 : path;
            const char* dot  = strrchr(base, '.');
            // Create a temporary cropped label (stack) will strdup inside grid
            char label[256];
            if (dot && (size_t)(dot - base) < sizeof(label)) {
                size_t L = (size_t)(dot - base);
                memcpy(label, base, L);
                label[L] = '\0';
            } else {
                strncpy(label, base, sizeof(label)-1);
                label[sizeof(label)-1] = '\0';
            }

            // Fill userdata
            app->map_ud[i].app  = (struct App*)app;
            app->map_ud[i].index = i;
            app->map_ud[i].path = path;

            items[i].label    = label;           // grid will strdup
            items[i].on_click = on_select_map;
            items[i].userdata = &app->map_ud[i]; // persistent pointer
        }
    }

    gui_paged_grid_set_items(&app->menu->grid, items, n);
    gui_paged_grid_mount(&app->menu->gui, &app->menu->grid);
    free(items);
}

static void init_menu_gui(App *app) {
    // Initialize gui and app elements
    app->menu->gui = (GuiContext){ .mlx = app->mlx, .now = 0.0, .paths = NULL, .paths_len = 0 };
    

    GuiContext *gui_ptr = &app->menu->gui;

    gui_paths_add(gui_ptr, "assets");

    // Load skins (reuse item skin for pager unless you have a different one)
    app->menu->ui_item_skin  = gui_load_png_from_paths(gui_ptr, "button_skin.png");
    if (!app->menu->ui_item_skin) { fprintf(stderr, "Missing assets/button_skin.png\n"); exit(EXIT_FAILURE); }
    app->menu->ui_pager_skin = app->menu->ui_item_skin;

    GuiNineSlice item_nine  = { .left=8, .right=8, .top=8, .bottom=8, .center_fill=true, .center_color=gui_rgba(200,200,200,255) };
    GuiNineSlice pager_nine = { .left=8, .right=8, .top=8, .bottom=8, .center_fill=true,  .center_color=gui_rgba(0,0,0,255) };

    GuiPagedGridConfig cfg = {
        .x = 0, .y = 0, .w = 640, .h = 420,
        .center_h = true, .center_v = true,
        .cols = 2, .rows = 2, .gap = 16, .pager_h = 20,
        .item_skin_tex = app->menu->ui_item_skin, .item_skin_cfg = item_nine,
        .pager_skin_tex = app->menu->ui_pager_skin, .pager_skin_cfg = pager_nine,
    };
    if (!gui_paged_grid_init(gui_ptr, &app->menu->grid, cfg)) {
        fprintf(stderr, "gui_paged_grid_init failed\n");
        exit(EXIT_FAILURE);
    }

    build_menu_items(app);
}

static void init_gui(App *app) {
    // MENU
    init_menu_gui(app);    
    // GAME GUI...
}

static void on_loop_gui(App *app) {
    if (app->state == MENU) {
        gui_begin_frame(&app->menu->gui);
        gui_paged_grid_update(&app->menu->gui, &app->menu->grid);
    }
}

static void clean_gui(App *app) {
    gui_paged_grid_free(&app->menu->gui, &app->menu->grid);

    // free per-item userdata array
    free(app->map_ud);
    app->map_ud = NULL;

    // free discovered file paths
    map_free_paths(app->map_files, app->map_file_count);
    app->map_files = NULL;
    app->map_file_count = 0;

    // free map if we replaced the builtin WORLD_DATA
    if (app->owned_map_data) {
        free(app->owned_map_data);
        app->owned_map_data = NULL;
    }

    if (app->menu->ui_item_skin)  mlx_delete_texture(app->menu->ui_item_skin);
    if (app->menu->ui_pager_skin && app->menu->ui_pager_skin != app->menu->ui_item_skin)
        mlx_delete_texture(app->menu->ui_pager_skin);
}

static void on_menu_resize(int32_t w, int32_t h, App* app) {
    canvas_destroy(&app->menu->scene);
    canvas_destroy(&app->menu->screen);
    
    canvas_init(&app->menu->screen, app->mlx, w, h);
    canvas_init(&app->menu->scene, app->mlx, w, h);
    menu_bg_resize(&app->menu->menu_bg, (int)w, (int)h);
    
    if (app->state != MENU) return ;

    if (mlx_image_to_window(app->mlx, app->menu->screen.img, 0, 0) < 0) {
        puts(mlx_strerror(mlx_errno));
        exit(EXIT_FAILURE);
    }
}

static void on_game_resize(int32_t w, int32_t h, App* app) {
    canvas_destroy(&app->game->scene);
    canvas_destroy(&app->game->screen);
    
    canvas_init(&app->game->screen, app->mlx, w, h);
    canvas_init(&app->game->scene, app->mlx, w, h);
    
    if (app->state != GAME) return ;

    if (mlx_image_to_window(app->mlx, app->game->screen.img, 0, 0) < 0) {
        puts(mlx_strerror(mlx_errno));
        exit(EXIT_FAILURE);
    }
}

static void on_resize(int32_t w, int32_t h, void* param) {
    App* app = (App*)param;
    on_menu_resize(w, h, app);
    on_game_resize(w, h, app);
}

static void init_menu_bg(App* app) {
    int w = (int)app->mlx->width;
    int h = (int)app->mlx->height;
    if (!menu_bg_init(&app->menu->menu_bg, w, h, 0xC0FFEEu)) {
        fprintf(stderr, "menu_bg_init failed\n");
        exit(EXIT_FAILURE);
    }
}

static void init_canvases(App* app) {
    if (canvas_init(&app->game->screen,  app->mlx, WIDTH, HEIGHT) ||
        canvas_init(&app->game->scene,   app->mlx, WIDTH, HEIGHT) ||
        canvas_init(&app->game->minimap, app->mlx, app->game->map.w * 6, app->game->map.h * 6) ||
        canvas_init(&app->menu->screen,   app->mlx, WIDTH, HEIGHT) ||
        canvas_init(&app->menu->scene,   app->mlx, WIDTH, HEIGHT))
    {
        puts(mlx_strerror(mlx_errno));
        exit(EXIT_FAILURE);
    }
}

// State switching
static void img_set_enabled(mlx_image_t* img, bool en) {
    if (!img) return;
    for (size_t i = 0; i < img->count; ++i) img->instances[i].enabled = en;
}

static void ensure_canvas_attached(mlx_t* mlx, Canvas* c, int x, int y) {
    if (!c || !c->img) return;
    if (c->img->count == 0) {
        if (mlx_image_to_window(mlx, c->img, x, y) < 0) {
            puts(mlx_strerror(mlx_errno));
            exit(EXIT_FAILURE);
        }
    }
}

static void switch_to_game(App* a) {
    // 1) Remove menu GUI from the window so nothing overlays the game
    gui_paged_grid_free(&a->menu->gui, &a->menu->grid);

    // 2) Hide menu screen, show game screen
    if (a->menu->screen.img && a->menu->screen.img->count > 0)
        img_set_enabled(a->menu->screen.img, false);

    ensure_canvas_attached(a->mlx, &a->game->screen, 0, 0);
    img_set_enabled(a->game->screen.img, true);

    a->state = GAME;
}

static void switch_to_menu(App* a) {
    // 1) Hide game screen, show (or attach) menu screen
    if (a->game->screen.img && a->game->screen.img->count > 0)
        img_set_enabled(a->game->screen.img, false);

    ensure_canvas_attached(a->mlx, &a->menu->screen, 0, 0);
    img_set_enabled(a->menu->screen.img, true);

    // 2) (Re)build menu GUI so buttons/pager are visible in this state
    init_menu_gui(a);              // safe: we freed it when entering GAME
    init_menu_bg(a);               // keep your animated bg up-to-date
    a->state = MENU;
}

int main(void) {
    /* Initialize MLX */
    mlx_t* mlx = mlx_init(WIDTH, HEIGHT, "MLX42 Raycaster", true);
    if (!mlx) { puts(mlx_strerror(mlx_errno)); return EXIT_FAILURE; }

    /* App context */
    App app = {0};

    app.game = (Game*)malloc(sizeof(Game) * 1);
    if (!app.game)
        return EXIT_FAILURE;
    app.menu = (Menu*)malloc(sizeof(Menu) * 1);
    if (!app.menu)
        return EXIT_FAILURE;

    app.mlx = mlx;
    app.selected_map_index = -1;
    app.owned_map_data = NULL;
    app.map_files = NULL;
    app.map_file_count = 0;
    app.map_ud = NULL;

    /* World map */
    app.game->map.w = WORLD_W;
    app.game->map.h = WORLD_H;
    /* If your compiler rejects WORLD_DATA aliasing, replace with: extern const int world_template[]; app.map.data = world_template; */
    extern const int WORLD_DATA[];
    app.game->map.data = WORLD_DATA;

    /* Cameras: starting pos in open space, FOV ~ 66° => |plane| ≈ tan(33°) ≈ 0.65 */
    app.game->cam.pos   = (Vec2f){ 12.0f, 12.0f };
    app.game->cam.dir   = (Vec2f){ -1.0f, 0.0f };
    app.game->cam.plane = (Vec2f){ 0.0f, 0.66f };

    /* Start the game on the menu */
    app.state     = MENU;

    init_canvases(&app);

    /* Only the screen is shown */
    if (mlx_image_to_window(mlx, app.menu->screen.img, 0, 0) < 0) {
        puts(mlx_strerror(mlx_errno));
        return EXIT_FAILURE;
    }

    app.last_time = mlx_get_time();

    /* Setup the GUI */
    init_gui(&app);
    
    init_menu_bg(&app);

    mlx_loop_hook(mlx, on_loop, &app);

    mlx_resize_hook(mlx, on_resize, &app);

#ifdef WEB
    g_mlx = mlx;
    emscripten_set_main_loop(emscripten_main_loop, 0, true);
#else
    mlx_loop(mlx);
#endif

    menu_bg_free(&app.menu->menu_bg);
    clean_gui(&app);
    canvas_destroy(&app.menu->screen);
    canvas_destroy(&app.menu->scene);
    canvas_destroy(&app.game->minimap);
    canvas_destroy(&app.game->scene);
    canvas_destroy(&app.game->screen);
    free(app.menu);
    free(app.game);
    mlx_terminate(mlx);
    return EXIT_SUCCESS;
}
