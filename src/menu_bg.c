// menu_bg.c
#include "menu_bg.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef MENU_BG_MAX_RIPPLES
#define MENU_BG_MAX_RIPPLES 48
#endif

// --------- small helpers ----------
static inline int imin(int a,int b){ return a<b?a:b; }
static inline int imax(int a,int b){ return a>b?a:b; }
static inline float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }

// Deterministic RNG (LCG)
static inline uint32_t rng_u32(unsigned* s){ *s = (*s)*1664525u + 1013904223u; return *s; }
static inline float rng01(unsigned* s){ return (rng_u32(s) >> 8) * (1.0f/16777216.0f); }

// Simple HSV->RGBA(opaque) approx (h in degrees)
static uint32_t hsv_to_rgba(float h, float s, float v) {
    h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
    s = clampf(s, 0.0f, 1.0f); v = clampf(v, 0.0f, 1.0f);
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h/60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r=0,g=0,b=0;
    if (h < 60)      { r=c; g=x; b=0; }
    else if (h <120) { r=x; g=c; b=0; }
    else if (h <180) { r=0; g=c; b=x; }
    else if (h <240) { r=0; g=x; b=c; }
    else if (h <300) { r=x; g=0; b=c; }
    else             { r=c; g=0; b=x; }
    uint8_t R=(uint8_t)((r+m)*255.0f);
    uint8_t G=(uint8_t)((g+m)*255.0f);
    uint8_t B=(uint8_t)((b+m)*255.0f);
    return gui_rgba(R,G,B,255);
}

// --------- internal layout -----------
static void grid_allocate(MenuBg* bg) {
    // Choose a baseline grid that scales with resolution
    // Aim for ~20x12 @ 800x600; keep cells roughly square-like
    int target_cols = imax(10, bg->w / 40);       // ~20 at 800px
    int target_rows = imax(6,  bg->h / 50);       // ~12 at 600px

    bg->grid_cols = target_cols;
    bg->grid_rows = target_rows;
    bg->cell_w = imax(1, bg->w / bg->grid_cols);
    bg->cell_h = imax(1, bg->h / bg->grid_rows);

    size_t n = (size_t)bg->grid_cols * (size_t)bg->grid_rows;
    free(bg->cell_phase);
    bg->cell_phase = (float*)malloc(n * sizeof(float));
    if (!bg->cell_phase) return;

    unsigned tmp = bg->rng ^ 0x9E3779B9u;
    for (size_t i = 0; i < n; ++i) {
        bg->cell_phase[i] = rng01(&tmp) * 6.2831853f; // 0..2π
    }
}

static void ripples_allocate(MenuBg* bg) {
    if (bg->ripples && bg->ripples_cap == MENU_BG_MAX_RIPPLES) {
        // keep
        for (int i=0;i<bg->ripples_cap;i++) bg->ripples[i].active = false;
        return;
    }
    free(bg->ripples);
    bg->ripples_cap = MENU_BG_MAX_RIPPLES;
    bg->ripples = (struct Ripple*)calloc((size_t)bg->ripples_cap, sizeof(*bg->ripples));
}

// --------- API ----------
bool menu_bg_init(MenuBg* bg, int w, int h, unsigned seed) {
    if (!bg) return false;
    memset(bg, 0, sizeof(*bg));
    bg->w = w; bg->h = h;
    bg->t = 0.0;
    bg->rng = seed ? seed : 0xC0FFEEu;

    grid_allocate(bg);
    ripples_allocate(bg);
    return bg->cell_phase && bg->ripples;
}

void menu_bg_resize(MenuBg* bg, int w, int h) {
    if (!bg) return;
    bg->w = w; bg->h = h;
    grid_allocate(bg);
    ripples_allocate(bg);
    // keep ripples array; existing active ripples will continue
}

void menu_bg_update(MenuBg* bg, double now, float dt) {
    if (!bg) return;
    bg->t = now;

    // Spawn new ripples randomly, keep density moderate
    if (bg->ripples) {
        for (int spawn_attempts = 0; spawn_attempts < 2; ++spawn_attempts) {
            if (rng01(&bg->rng) < 0.045f) {
                // find a free slot
                for (int i = 0; i < bg->ripples_cap; ++i) {
                    if (!bg->ripples[i].active) {
                        bg->ripples[i].active = true;
                        bg->ripples[i].x = rng01(&bg->rng) * (float)bg->w;
                        bg->ripples[i].y = rng01(&bg->rng) * (float)bg->h;
                        bg->ripples[i].r = 8.0f;
                        bg->ripples[i].dr = 60.0f + 160.0f * rng01(&bg->rng);
                        float hue = 360.0f * rng01(&bg->rng);
                        bg->ripples[i].color = hsv_to_rgba(hue, 0.65f, 0.95f);
                        break;
                    }
                }
            }
        }
        // advance & recycle
        float maxR = hypotf((float)bg->w, (float)bg->h) * 1.1f;
        for (int i = 0; i < bg->ripples_cap; ++i) {
            if (!bg->ripples[i].active) continue;
            bg->ripples[i].r += bg->ripples[i].dr * dt;
            if (bg->ripples[i].r > maxR) {
                bg->ripples[i].active = false;
            }
        }
    }
}

// Draw a cheap vertical band gradient as a base
static void draw_band_gradient(MenuBg* bg, mlx_image_t* dst) {
    const int bands = 8;
    int bw = (bg->w + bands - 1) / bands;
    float base_h = fmodf((float)bg->t * 12.0f, 360.0f);
    for (int i = 0; i < bands; ++i) {
        float h = base_h + i * (360.0f / (float)bands);
        uint32_t col = hsv_to_rgba(h, 0.25f, 0.10f + 0.05f * (float)((i%2)==0));
        int x = i * bw;
        int w = imin(bw, bg->w - x);
        if (w > 0) gui_fill_rect(dst, x, 0, w, bg->h, col);
    }
}

// Flowing squares: pulsing outlines + subtle cell fill
static void draw_flowing_squares(MenuBg* bg, mlx_image_t* dst) {
    if (!bg->cell_phase) return;

    const float pulse_rate = 1.3f;
    const float slide_speed = 12.0f; // px/s
    const int cols = bg->grid_cols;
    const int rows = bg->grid_rows;
    const int cw = bg->cell_w;
    const int ch = bg->cell_h;

    float slide = fmodf((float)bg->t * slide_speed, (float)imax(cw, ch));
    float base_h = fmodf((float)bg->t * 10.0f, 360.0f);

    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            int idx = gy * cols + gx;
            int x = gx * cw;
            int y = gy * ch;

            // soft cell background
            float bgp = 0.5f + 0.5f * sinf((float)bg->t * 0.6f + (float)gx*0.3f + (float)gy*0.25f);
            uint32_t cell_col = hsv_to_rgba(base_h + 80.0f * bgp, 0.15f, 0.08f + 0.06f * bgp);
            gui_fill_rect(dst, x, y, cw, ch, cell_col);

            // inner pulsing square (outline)
            float phase = bg->cell_phase[idx];
            float p = 0.5f + 0.5f * sinf((float)bg->t * pulse_rate + phase);
            int side = (int)((0.35f + 0.25f * p) * (float)imin(cw, ch));
            int off = (imin(cw, ch) - side) / 2;
            uint32_t outline = hsv_to_rgba(base_h + 180.0f * p, 0.65f, 0.9f);
            gui_draw_square(dst, x + off + (int)slide % 2, y + off, side, outline);

            // tiny accent (1px vertical bar) for texture
            int ax = x + cw/2 + (int)fmodf(slide + gx*3.0f, (float)imax(2,cw/3)) - cw/6;
            int ah = imax(1, ch/6);
            uint32_t accent = hsv_to_rgba(base_h + 60.0f, 0.25f, 0.25f);
            gui_fill_rect(dst, ax, y + (ch - ah)/2, 1, ah, accent);
        }
    }
}

// Ripple rings overlay
static void draw_ripples(MenuBg* bg, mlx_image_t* dst) {
    if (!bg->ripples) return;
    for (int i = 0; i < bg->ripples_cap; ++i) {
        if (!bg->ripples[i].active) continue;
        int cx = (int)bg->ripples[i].x;
        int cy = (int)bg->ripples[i].y;
        int R  = (int)bg->ripples[i].r;
        uint32_t c = bg->ripples[i].color;

        // 2–3 concentric rings
        gui_draw_circle(dst, cx, cy, R, c);
        if (R > 6)  gui_draw_circle(dst, cx, cy, R - 6, c);
        if (R > 12) gui_draw_circle(dst, cx, cy, R - 12, c);
    }
}

void menu_bg_render(MenuBg* bg, mlx_image_t* dst) {
    if (!bg || !dst) return;

    // Base gradient
    draw_band_gradient(bg, dst);

    // Scene layers
    draw_flowing_squares(bg, dst);
    draw_ripples(bg, dst);
}

void menu_bg_free(MenuBg* bg) {
    if (!bg) return;
    free(bg->cell_phase);  bg->cell_phase = NULL;
    free(bg->ripples);     bg->ripples = NULL;
    bg->ripples_cap = 0;
}
