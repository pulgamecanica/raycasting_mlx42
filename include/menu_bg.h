// menu_bg.h
#ifndef MENU_BG_H
#define MENU_BG_H

#include "MLX42/MLX42.h"
#include "gui.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct MenuBg {
    int     w, h;         // render size (match your screen canvas)
    double  t;            // absolute time (seconds)
    unsigned rng;         // deterministic rng seed

    // Flowing squares grid
    int     grid_cols;
    int     grid_rows;
    int     cell_w;
    int     cell_h;
    float*  cell_phase;   // size = grid_cols * grid_rows

    // Ripples
    struct Ripple {
        bool active;
        float x, y;
        float r;      // radius
        float dr;     // growth speed
        uint32_t color;
    } *ripples;
    int ripples_cap;
} MenuBg;

bool menu_bg_init(MenuBg* bg, int w, int h, unsigned seed);
/** Call if the window/canvas size changes */
void menu_bg_resize(MenuBg* bg, int w, int h);
/** Advance internal timers and motion */
void menu_bg_update(MenuBg* bg, double now, float dt);
/** Draw the animated background into dst (full-screen) */
void menu_bg_render(MenuBg* bg, mlx_image_t* dst);
/** Free all allocations */
void menu_bg_free(MenuBg* bg);


#endif // MENU_BG_H
