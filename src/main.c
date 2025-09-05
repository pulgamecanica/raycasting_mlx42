#include <MLX42/MLX42.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "canvas.h"
#include "map.h"
#include "raycast.h"
#include "gui.h"

#define WIDTH  800
#define HEIGHT 600

typedef struct {
    mlx_t*   mlx;
    Canvas   screen;   /* the ONLY image attached to the window */
    Canvas   scene;    /* off-screen render target */
    Canvas   minimap;  /* off-screen minimap buffer */
    GridMap  map;
    Camera   cam;
    double   last_time;

    // TESTING GUI
    GuiContext gui;
    GuiButton test_button;
    GuiAnimation test_animation;
} App;

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

    handle_input(a, dt);

    render_scene(&a->scene, &a->map, &a->cam);
    draw_minimap(&a->minimap, &a->map, &a->cam, 6);

    canvas_copy(&a->screen, &a->scene, 0, 0);       /* compose scene */
    canvas_copy(&a->screen, &a->minimap, 8, 8);     /* overlay minimap */

    on_loop_gui(a);
}


// TESTING GUI
static void on_click(void* ud) {
    (void)ud;
    fprintf(stderr, "Button clicked!\n");
}

static void init_gui(App *app) {
    app->gui = (GuiContext){ .mlx = app->mlx, .now = 0.0, .paths = NULL, .paths_len = 0};
    GuiContext *gui_ptr = &app->gui;  // usefull to setup gui elements
    gui_paths_add(gui_ptr, "assets");

    {   // test buton init
        mlx_texture_t* skin = gui_load_png_from_paths(gui_ptr, "button_skin.png");
        if (!skin) { fprintf(stderr, "Missing button_skin.png\n"); exit(EXIT_FAILURE); }
        GuiNineSlice nine = { .left=8, .right=8, .top=8, .bottom=8, .center_fill=true, .center_color=0x00124222 };
        GuiButton *btn_ptr = &app->test_button;
        if (!gui_button_init(gui_ptr, btn_ptr, 40, 140, 200, 60, skin, nine, "Hello")) {
            fprintf(stderr, "Error while initializing button\n");
            exit(EXIT_FAILURE);
        }
        btn_ptr->on_click = on_click;
        gui_button_mount(gui_ptr, btn_ptr);
    }

    {   // test animation init
        GuiAnimation *anim_ptr = &app->test_animation;
        gui_animation_init(anim_ptr, 5.0, gui_ptr->mlx->width - (128 + 42), gui_ptr->mlx->height - (128 + 42));
        // Replace with your actual frames: frame0.png, frame1.png, ...
        gui_animation_push_png(gui_ptr, anim_ptr, "fly_frame0.png");
        gui_animation_push_png(gui_ptr, anim_ptr, "fly_frame1.png");
        gui_animation_push_png(gui_ptr, anim_ptr, "fly_frame2.png");
        gui_animation_push_png(gui_ptr, anim_ptr, "fly_frame3.png");
        if (!gui_animation_mount(gui_ptr, anim_ptr)) {
            fprintf(stderr, "Error mounting animation\n");
            exit(EXIT_FAILURE);
        }
    }
}

static void on_loop_gui(App *app) {
    mlx_image_t *canvas = app->screen.img;
    
    gui_fill_rect(canvas, 20, 20, 120, 60, gui_rgba(30, 144, 255, 255));
    gui_draw_rect(canvas, 20, 20, 120, 60, gui_rgba(255, 255, 255, 255));
    gui_draw_square(canvas, 160, 20, 60, gui_rgba(255, 215, 0, 255));
    gui_fill_circle(canvas, 260, 50, 30, gui_rgba(220, 20, 60, 255));
    gui_draw_circle(canvas, 340, 50, 30, gui_rgba(255,255,255,255));
    gui_fill_triangle(canvas, 420, 80, 470, 20, 520, 80, gui_rgba(46, 204, 113, 255));
    gui_draw_triangle(canvas, 540, 80, 590, 20, 640, 80, gui_rgba(255,255,255,255));

    gui_begin_frame(&app->gui);
    gui_button_update(&app->gui, &app->test_button);
    gui_animation_update(&app->gui, &app->test_animation);
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

    /* Canvases */
    if (canvas_init(&app.screen,  mlx, WIDTH, HEIGHT) ||
        canvas_init(&app.scene,   mlx, WIDTH, HEIGHT) ||
        canvas_init(&app.minimap, mlx, app.map.w * 6, app.map.h * 6))
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

    init_gui(&app); // Testing gui

    mlx_loop_hook(mlx, on_loop, &app);

#ifdef WEB
    g_mlx = mlx;
    emscripten_set_main_loop(emscripten_main_loop, 0, true);
#else
    mlx_loop(mlx);
#endif

    canvas_destroy(&app.minimap);
    canvas_destroy(&app.scene);
    canvas_destroy(&app.screen);
    mlx_terminate(mlx);
    return EXIT_SUCCESS;
}
