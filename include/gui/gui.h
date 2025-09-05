#ifndef GUI_H
#define GUI_H

#include "MLX42/MLX42.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


// ---------- Context ----------
typedef struct GuiContext {
    mlx_t* mlx;         // Required
    double now;         // Set each frame via gui_begin_frame()
    
    // Asset search paths (owned; NULL-terminated strings)
    char        **paths;
    unsigned    paths_len;
} GuiContext;

void gui_begin_frame(GuiContext* ctx); // sets ctx->now from mlx_get_time(ctx->mlx)

// ---------- Colors ----------
static inline uint32_t gui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

// ---------- Drawing (onto an existing mlx_image_t) ----------
void gui_draw_rect(mlx_image_t* dst, int x, int y, int w, int h, uint32_t color);
void gui_fill_rect(mlx_image_t* dst, int x, int y, int w, int h, uint32_t color);
void gui_draw_square(mlx_image_t* dst, int x, int y, int side, uint32_t color); // wrapper
void gui_draw_circle(mlx_image_t* dst, int cx, int cy, int r, uint32_t color);
void gui_fill_circle(mlx_image_t* dst, int cx, int cy, int r, uint32_t color);
void gui_draw_triangle(mlx_image_t* dst, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void gui_fill_triangle(mlx_image_t* dst, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);

// ---------- Nine-slice-like skin (corners preserved, sides repeat center-line pixel) ----------
typedef struct GuiNineSlice {
    int left, right, top, bottom;   // margins in source texture
    bool center_fill;               // if true, fill center with solid color
    uint32_t center_color;          // only used when center_fill == true
} GuiNineSlice;

/**
 * Render a skinned rectangle from 'src' texture into a new mlx_image_t of size (out_w x out_h).
 * Sides: use the middle pixel of each source side and repeat; corners copied verbatim.
 * Center: either repeat the single center pixel (default) or fill with center_color when center_fill is true.
 * Returns an image you must add to the window and later delete.
 * Fails (returns NULL) if out_w/out_h are smaller than the margins.
 */
mlx_image_t* gui_skin_render(GuiContext* ctx, const mlx_texture_t* src, GuiNineSlice ns, int out_w, int out_h);

// ---------- Animation (frame deque + FPS) ----------
typedef struct GuiDeque {
    void** data;
    size_t cap, size, head; // circular buffer
} GuiDeque;

void  gui_deque_init(GuiDeque* dq);
void  gui_deque_free(mlx_t *mlx, GuiDeque* dq, void (*free_item)(void*, void *));
bool  gui_deque_push_back(GuiDeque* dq, void* item);
void* gui_deque_pop_front(GuiDeque* dq);
size_t gui_deque_count(const GuiDeque* dq);

typedef struct GuiAnimation {
    GuiDeque frames;           // each is mlx_image_t*
    int width, height;         // enforce all frames same size
    double fps;                // frames per second
    size_t index;              // current frame index
    double last_switch_time;   // internal
    int x, y;                  // where to place the frames
    bool mounted;              // frames added to window?
} GuiAnimation;

/**
 * Initialize an animation. Call gui_animation_mount() once images are created.
 * You can push frames via gui_animation_push_png()/xpm(), or push your own images (same size).
 */
void gui_animation_init(GuiAnimation* a, double fps, int x, int y);
bool gui_animation_push_png(GuiContext* ctx, GuiAnimation* a, const char* path);
bool gui_animation_push_xpm42(GuiContext* ctx, GuiAnimation* a, const char* path);
bool gui_animation_push_image(GuiAnimation* a, mlx_image_t* img); // takes ownership
bool gui_animation_mount(GuiContext* ctx, GuiAnimation* a);       // adds all frames to window (but hides all except current)
void gui_animation_update(GuiContext* ctx, GuiAnimation* a);      // call each frame
void gui_animation_free(mlx_t *mlx, GuiAnimation* a);                         // frees frames

// ---------- Buttons ----------
typedef enum {
    GUI_BTN_NORMAL = 0,
    GUI_BTN_HOVER  = 1,
    GUI_BTN_ACTIVE = 2,
    GUI_BTN_DISABLED = 3
} GuiButtonState;

typedef struct GuiButton {
    int x, y, w, h;
    GuiButtonState state;
    bool was_down;
    // Skin
    const mlx_texture_t* skin_tex; // not owned
    GuiNineSlice skin_cfg;
    mlx_image_t* skin_img;         // rendered to (w,h) from skin_tex; owned
    // Optional label (naive: one static image; you can replace with your text system)
    mlx_image_t* label_img;        // owned
    int label_dx, label_dy;
    // Callbacks
    void (*on_click)(void* userdata);
    void* userdata;
} GuiButton;

bool gui_button_init(GuiContext* ctx, GuiButton* b, int x, int y, int w, int h,
                     const mlx_texture_t* skin_tex, GuiNineSlice skin_cfg,
                     const char* label_text /*nullable*/);
void gui_button_mount(GuiContext* ctx, GuiButton* b); // add images to window
void gui_button_update(GuiContext* ctx, GuiButton* b); // call each frame
void gui_button_free(mlx_t *mlx, GuiButton* b);

// ----------- Path ------------
bool gui_paths_add(GuiContext* ctx, const char* path);
char* gui_paths_find(const GuiContext* ctx, const char* filename);
mlx_texture_t*  gui_load_png_from_paths(const GuiContext* ctx, char *filename);

#endif // GUI_H
