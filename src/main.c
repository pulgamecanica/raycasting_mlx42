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

// REPLACE the GUI-related fields in App with this
// (remove test_button and test_animation)
typedef enum State {
    MENU,
    GAME,
} State;

// ADD near the top (after typedefs), to carry info into on_select_map:
typedef struct {
    struct App* app;       // forward use; actual App is below
    size_t index;          // which item in list
    const char* path;      // full file path (owned by App.map_files)
} MapSelectUD;

typedef struct {
    mlx_t*   mlx;
    Canvas   screen;   /* the ONLY image attached to the window */
    Canvas   scene;    /* off-screen render target */
    Canvas   minimap;  /* off-screen minimap buffer */
    Canvas   menu;  /* off-screen minimap buffer */
    GridMap  map;
    Camera   cam;
    State    state;
    double   last_time;

    // GUI
    MenuBg menu_bg;
    GuiContext   gui;
    GuiPagedGrid grid;
    mlx_texture_t* ui_item_skin;
    mlx_texture_t* ui_pager_skin;

    // Menu Map data
    char** map_files; // levels
    size_t map_file_count;

    MapSelectUD* map_ud; // same length as map_files, for button callbacks
    int selected_map_index; // -1 if none yet

    int* owned_map_data; // heap map currently in use (NULL if using WORLD_DATA)
} App;

static void init_menu_bg(App* app);
static void on_select_map(void* ud); // Forward declare
static void on_loop_gui(App *app); // Forward declaration

#ifdef WEB
# include <emscripten.h>
static mlx_t* g_mlx = NULL; /* WEB needs this for emscripten main loop */
static void emscripten_main_loop(void) { mlx_loop(g_mlx); }
#endif

static void handle_input(App* a, float dt) {
    if (mlx_is_key_down(a->mlx, MLX_KEY_ESCAPE)) mlx_close_window(a->mlx);

    float move = 3.0f * dt;      /* tiles per second */
    float rot  = 2.0f * dt;      /* radians per second */

    /* strafe vector is dir rotated 90° */
    Vec2f strafe = (Vec2f){ -a->cam.dir.y, a->cam.dir.x };
    if (mlx_is_key_down(a->mlx, MLX_KEY_M)) {
        init_menu_bg(a);
        a->state = MENU;
        return ;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_W)) {
        Vec2f n = { a->cam.pos.x + a->cam.dir.x * move, a->cam.pos.y + a->cam.dir.y * move };
        if (!map_is_wall(&a->map, (int)n.x, (int)a->cam.pos.y)) a->cam.pos.x = n.x;
        if (!map_is_wall(&a->map, (int)a->cam.pos.x, (int)n.y)) a->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_S)) {
        Vec2f n = { a->cam.pos.x - a->cam.dir.x * move, a->cam.pos.y - a->cam.dir.y * move };
        if (!map_is_wall(&a->map, (int)n.x, (int)a->cam.pos.y)) a->cam.pos.x = n.x;
        if (!map_is_wall(&a->map, (int)a->cam.pos.x, (int)n.y)) a->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_A)) {
        Vec2f n = { a->cam.pos.x - strafe.x * move, a->cam.pos.y - strafe.y * move };
        if (!map_is_wall(&a->map, (int)n.x, (int)a->cam.pos.y)) a->cam.pos.x = n.x;
        if (!map_is_wall(&a->map, (int)a->cam.pos.x, (int)n.y)) a->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_D)) {
        Vec2f n = { a->cam.pos.x + strafe.x * move, a->cam.pos.y + strafe.y * move };
        if (!map_is_wall(&a->map, (int)n.x, (int)a->cam.pos.y)) a->cam.pos.x = n.x;
        if (!map_is_wall(&a->map, (int)a->cam.pos.x, (int)n.y)) a->cam.pos.y = n.y;
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_LEFT)) {
        float cs = cosf(rot), sn = sinf(rot);
        Vec2f d = a->cam.dir, p = a->cam.plane;
        a->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        a->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }
    if (mlx_is_key_down(a->mlx, MLX_KEY_RIGHT)) {
        float cs = cosf(-rot), sn = sinf(-rot);
        Vec2f d = a->cam.dir, p = a->cam.plane;
        a->cam.dir   = (Vec2f){ d.x*cs - d.y*sn, d.x*sn + d.y*cs };
        a->cam.plane = (Vec2f){ p.x*cs - p.y*sn, p.x*sn + p.y*cs };
    }
}

static void on_loop(void* param) {
    App* a = (App*)param;

    double now = mlx_get_time();            /* MLX42 timer */
    float dt = (float)(now - a->last_time);
    if (dt > 0.05f) dt = 0.05f;             /* clamp to avoid tunneling on stalls */
    a->last_time = now;

    if (a->state == GAME) {
        handle_input(a, dt);

        render_scene(&a->scene, &a->map, &a->cam);
        draw_minimap(&a->minimap, &a->map, &a->cam, 6);

        canvas_copy(&a->screen, &a->scene, 0, 0);       /* compose scene */
        canvas_copy(&a->screen, &a->minimap, 8, 8);     /* overlay minimap */
    } else {
        menu_bg_update(&a->menu_bg, now, dt);
        menu_bg_render(&a->menu_bg, a->menu.img);
        canvas_copy(&a->screen, &a->menu, 0, 0);
    }

    on_loop_gui(a);
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
    app->map.w = w;
    app->map.h = h;
    app->map.data = (const int*)app->owned_map_data; // assign

    app->selected_map_index = (int)sel->index;
    app->state = GAME;
    // gui_paged_grid_free(&app->gui, &app->grid);
    fprintf(stderr, "Loaded map (%dx%d) from %s\n", w, h, sel->path);
}

// static const char* basename_no_ext(const char* path) {
//     const char* base = strrchr(path, '/');
//     base = base ? base + 1 : path;
//     const char* dot = strrchr(base, '.');
//     return (dot && dot > base) ? ( (char[]){0} ) : base; // placeholder to silence warning
// }

// static const char* label_from_path(const char* path) {
//     const char* base = strrchr(path, '/');
//     base = base ? base + 1 : path;
//     // const char* dot = strrchr(base, '.');
//     return base;
// }

static void build_menu_items(App* app) {
    const char* levels_dir = "assets/maps"; // change to your folder
    if (map_list_levels(levels_dir, &app->map_files, &app->map_file_count) != 0) {
        app->map_files = NULL;
        app->map_file_count = 0;
    }

    // Prepare userdata array (one per item)
    if (app->map_file_count > 0) {
        app->map_ud = (MapSelectUD*)calloc(app->map_file_count, sizeof(MapSelectUD));
        if (!app->map_ud) { app->map_file_count = 0; }
    }

    size_t n = app->map_file_count ? app->map_file_count : 3; // fallback samples
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
            // Create a temporary cropped label (stack) then strdup inside grid
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

    gui_paged_grid_set_items(&app->grid, items, n);
    gui_paged_grid_mount(&app->gui, &app->grid);
    free(items);
}

static void init_gui(App *app) {
    app->gui = (GuiContext){ .mlx = app->mlx, .now = 0.0, .paths = NULL, .paths_len = 0 };
    app->selected_map_index = -1;
    app->owned_map_data = NULL;
    app->map_files = NULL;
    app->map_file_count = 0;
    app->map_ud = NULL;

    GuiContext *gui_ptr = &app->gui;

    gui_paths_add(gui_ptr, "assets");

    // Load skins (reuse item skin for pager unless you have a different one)
    app->ui_item_skin  = gui_load_png_from_paths(gui_ptr, "button_skin.png");
    if (!app->ui_item_skin) { fprintf(stderr, "Missing assets/button_skin.png\n"); exit(EXIT_FAILURE); }
    app->ui_pager_skin = app->ui_item_skin;

    GuiNineSlice item_nine  = { .left=8, .right=8, .top=8, .bottom=8, .center_fill=true, .center_color=gui_rgba(200,200,200,255) };
    GuiNineSlice pager_nine = { .left=8, .right=8, .top=8, .bottom=8, .center_fill=true,  .center_color=gui_rgba(0,0,0,255) };

    GuiPagedGridConfig cfg = {
        .x = 0, .y = 0, .w = 640, .h = 420,
        .center_h = true, .center_v = true,
        .cols = 2, .rows = 2, .gap = 16, .pager_h = 20,
        .item_skin_tex = app->ui_item_skin, .item_skin_cfg = item_nine,
        .pager_skin_tex = app->ui_pager_skin, .pager_skin_cfg = pager_nine,
    };
    if (!gui_paged_grid_init(gui_ptr, &app->grid, cfg)) {
        fprintf(stderr, "gui_paged_grid_init failed\n");
        exit(EXIT_FAILURE);
    }

    build_menu_items(app);
}

static void on_loop_gui(App *app) {
    gui_begin_frame(&app->gui);
    if (app->state == MENU) {
        gui_paged_grid_update(&app->gui, &app->grid);
    }
}

static void clean_gui(App *app) {
    gui_paged_grid_free(&app->gui, &app->grid);

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

    if (app->ui_item_skin)  mlx_delete_texture(app->ui_item_skin);
    if (app->ui_pager_skin && app->ui_pager_skin != app->ui_item_skin)
        mlx_delete_texture(app->ui_pager_skin);
}


static void init_menu_bg(App* app) {
    // Use the actual canvas size in case it differs from WIDTH/HEIGHT
    int w = (int)app->mlx->width;
    int h = (int)app->mlx->height;
    if (!menu_bg_init(&app->menu_bg, w, h, 0xC0FFEEu)) {
        fprintf(stderr, "menu_bg_init failed\n");
        exit(EXIT_FAILURE);
    }
}

static void on_resize(int32_t w, int32_t h, void* param) {
    App* app = (App*)param;
    canvas_destroy(&app->scene);
    canvas_destroy(&app->menu);
    canvas_destroy(&app->screen);
    
    canvas_init(&app->screen, app->mlx, w, h);
    canvas_init(&app->scene, app->mlx, w, h);
    canvas_init(&app->menu, app->mlx, w, h);
    if (mlx_image_to_window(app->mlx, app->screen.img, 0, 0) < 0) {
        puts(mlx_strerror(mlx_errno));
        exit(EXIT_FAILURE);
    }
    menu_bg_resize(&app->menu_bg, (int)w, (int)h);
}

int main(void) {
    /* Initialize MLX */
    mlx_t* mlx = mlx_init(WIDTH, HEIGHT, "MLX42 Raycaster", true);
    if (!mlx) { puts(mlx_strerror(mlx_errno)); return EXIT_FAILURE; }

    /* App context */
    App app = {0};
    app.mlx = mlx;

    /* World map */
    app.map.w = WORLD_W;
    app.map.h = WORLD_H;
    /* If your compiler rejects WORLD_DATA aliasing, replace with: extern const int world_template[]; app.map.data = world_template; */
    extern const int WORLD_DATA[];
    app.map.data = WORLD_DATA;

    /* Cameras: starting pos in open space, FOV ~ 66° => |plane| ≈ tan(33°) ≈ 0.65 */
    app.cam.pos   = (Vec2f){ 12.0f, 12.0f };
    app.cam.dir   = (Vec2f){ -1.0f, 0.0f };
    app.cam.plane = (Vec2f){ 0.0f, 0.66f };
    app.state     = MENU;

    /* Canvases */
    if (canvas_init(&app.screen,  mlx, WIDTH, HEIGHT) ||
        canvas_init(&app.scene,   mlx, WIDTH, HEIGHT) ||
        canvas_init(&app.minimap, mlx, app.map.w * 6, app.map.h * 6) ||
        canvas_init(&app.menu, mlx, WIDTH, HEIGHT))
    {
        puts(mlx_strerror(mlx_errno));
        return EXIT_FAILURE;
    }

    /* Only the screen is shown */
    if (mlx_image_to_window(mlx, app.screen.img, 0, 0) < 0) {
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

    menu_bg_free(&app.menu_bg);
    clean_gui(&app);
    canvas_destroy(&app.menu);
    canvas_destroy(&app.minimap);
    canvas_destroy(&app.scene);
    canvas_destroy(&app.screen);
    mlx_terminate(mlx);
    return EXIT_SUCCESS;
}
