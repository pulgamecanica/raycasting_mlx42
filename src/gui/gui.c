#include "gui.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#ifndef GUI_TEXTURE_MIN_ALPHA
#define GUI_TEXTURE_MIN_ALPHA 1
#endif

#ifndef GUI_MIN_Z_LAYER
#define GUI_MIN_Z_LAYER 1
#endif

static inline int gui_clampi(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }

// ---------------- Context ----------------
void gui_begin_frame(GuiContext* ctx) {
    if (!ctx || !ctx->mlx) return;
    ctx->now = mlx_get_time();
}

// ---------------- Low-level helpers ----------------
static inline void put_px(mlx_image_t* img, int x, int y, uint32_t rgba) {
    if (!img) return;
    if (x < 0 || y < 0 || x >= (int)img->width || y >= (int)img->height) return;
    mlx_put_pixel(img, (uint32_t)x, (uint32_t)y, rgba);
}

static inline uint32_t tex_rgba_sample_visible(const mlx_texture_t* t, int x, int y) {
    // Same as tex_get_rgba but ensure visible alpha when debugging.
    x = gui_clampi(x, 0, (int)t->width  - 1);
    y = gui_clampi(y, 0, (int)t->height - 1);
    const int bpp = (int)t->bytes_per_pixel; // usually 4
    const size_t idx = ((size_t)y * (size_t)t->width + (size_t)x) * (size_t)bpp;
    const uint8_t* p = &t->pixels[idx];      // MLX42 textures are RGBA
    uint8_t r = p[0], g = p[1], b = p[2], a = p[3];
#if GUI_TEXTURE_MIN_ALPHA
    if (a == 0) a = 255;
#endif
    return gui_rgba(r,g,b,a);
}

static inline uint32_t tex_get_rgba(const mlx_texture_t* t, int x, int y) {
    if (!t) return 0;
    return tex_rgba_sample_visible(t, x, y);
}

static void blit_rect(mlx_image_t* dst, int dx, int dy, int w, int h, const mlx_texture_t* src, int sx, int sy) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            put_px(dst, dx + i, dy + j, tex_get_rgba(src, sx + i, sy + j));
}

// ---------------- Drawing primitives ----------------
void gui_draw_rect(mlx_image_t* dst, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; ++i) {
        put_px(dst, x + i, y, color);
        put_px(dst, x + i, y + h - 1, color);
    }
    for (int j = 0; j < h; ++j) {
        put_px(dst, x, y + j, color);
        put_px(dst, x + w - 1, y + j, color);
    }
}

void gui_fill_rect(mlx_image_t* dst, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            put_px(dst, x + i, y + j, color);
}

void gui_draw_square(mlx_image_t* dst, int x, int y, int side, uint32_t color) {
    gui_draw_rect(dst, x, y, side, side, color);
}

void gui_draw_circle(mlx_image_t* dst, int cx, int cy, int r, uint32_t color) {
    if (r <= 0) return;
    int x = r, y = 0, err = 0;
    while (x >= y) {
        put_px(dst, cx + x, cy + y, color);
        put_px(dst, cx + y, cy + x, color);
        put_px(dst, cx - y, cy + x, color);
        put_px(dst, cx - x, cy + y, color);
        put_px(dst, cx - x, cy - y, color);
        put_px(dst, cx - y, cy - x, color);
        put_px(dst, cx + y, cy - x, color);
        put_px(dst, cx + x, cy - y, color);
        y++;
        if (err <= 0) { err += 2*y + 1; } 
        if (err > 0)  { x--; err -= 2*x + 1; }
    }
}

void gui_fill_circle(mlx_image_t* dst, int cx, int cy, int r, uint32_t color) {
    if (r <= 0) return;
    for (int y = -r; y <= r; ++y) {
        int span = (int)sqrt((double)r*r - (double)y*y);
        for (int x = -span; x <= span; ++x)
            put_px(dst, cx + x, cy + y, color);
    }
}

static void draw_line(mlx_image_t* dst, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        put_px(dst, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gui_draw_triangle(mlx_image_t* dst, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    draw_line(dst, x1, y1, x2, y2, color);
    draw_line(dst, x2, y2, x3, y3, color);
    draw_line(dst, x3, y3, x1, y1, color);
}

static void swap_i(int* a, int* b){ int t=*a; *a=*b; *b=t; }

// Simple scanline fill
void gui_fill_triangle(mlx_image_t* dst, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    // Sort by y ascending
    if (y2 < y1) { swap_i(&y1,&y2); swap_i(&x1,&x2); }
    if (y3 < y1) { swap_i(&y1,&y3); swap_i(&x1,&x3); }
    if (y3 < y2) { swap_i(&y2,&y3); swap_i(&x2,&x3); }

    int total_h = y3 - y1;
    if (total_h == 0) return;

    for (int i = 0; i <= total_h; ++i) {
        bool second_half = i > (y2 - y1) || (y2 - y1) == 0;
        int seg_h = second_half ? (y3 - y2) : (y2 - y1);
        if (seg_h == 0) continue;
        float alpha = (float)i / (float)total_h;
        float beta  = (float)(i - (second_half ? (y2 - y1) : 0)) / (float)seg_h;

        int Ax = x1 + (int)((x3 - x1) * alpha);
        int Ay = y1 + i;
        int Bx = second_half ? (x2 + (int)((x3 - x2) * beta))
                             : (x1 + (int)((x2 - x1) * beta));
        if (Ax > Bx) swap_i(&Ax, &Bx);
        for (int x = Ax; x <= Bx; ++x) put_px(dst, x, Ay, color);
    }
}

// ---------------- Skin rendering (corners = exact, sides = repeat mid-pixel) ----------------
mlx_image_t* gui_skin_render(GuiContext* ctx, const mlx_texture_t* src, GuiNineSlice ns, int out_w, int out_h) {
    if (!ctx || !ctx->mlx || !src) return NULL;
    if (out_w <= 0 || out_h <= 0) return NULL;
    if (ns.left + ns.right > (int)src->width || ns.top + ns.bottom > (int)src->height) return NULL;
    if (out_w < ns.left + ns.right || out_h < ns.top + ns.bottom) return NULL;

    mlx_image_t* out = mlx_new_image(ctx->mlx, out_w, out_h);
    if (!out) return NULL;

    const int sw = (int)src->width, sh = (int)src->height;
    const int L = ns.left, R = ns.right, T = ns.top, B = ns.bottom;
    const int mid_top_x    = L + (sw - L - R) / 2;
    const int mid_bottom_x = mid_top_x;
    const int mid_left_y   = T + (sh - T - B) / 2;
    const int mid_right_y  = mid_left_y;

    // Corners
    if (L > 0 && T > 0) blit_rect(out, 0, 0, L, T, src, 0, 0);                                 // TL
    if (R > 0 && T > 0) blit_rect(out, out_w - R, 0, R, T, src, sw - R, 0);                    // TR
    if (L > 0 && B > 0) blit_rect(out, 0, out_h - B, L, B, src, 0, sh - B);                    // BL
    if (R > 0 && B > 0) blit_rect(out, out_w - R, out_h - B, R, B, src, sw - R, sh - B);       // BR

    // Top edge (repeat middle column across width), for rows 0..T-1 excluding corners
    for (int y = 0; y < T; ++y) {
        uint32_t sample = tex_get_rgba(src, mid_top_x, y);
        for (int x = L; x < out_w - R; ++x) put_px(out, x, y, sample);
    }
    // Bottom edge
    for (int y = out_h - B; y < out_h; ++y) {
        uint32_t sample = tex_get_rgba(src, mid_bottom_x, sh - (out_h - y));
        for (int x = L; x < out_w - R; ++x) put_px(out, x, y, sample);
    }
    // Left edge
    for (int x = 0; x < L; ++x) {
        uint32_t sample = tex_get_rgba(src, x, mid_left_y);
        for (int y = T; y < out_h - B; ++y) put_px(out, x, y, sample);
    }
    // Right edge
    for (int x = out_w - R; x < out_w; ++x) {
        uint32_t sample = tex_get_rgba(src, sw - (out_w - x), mid_right_y);
        for (int y = T; y < out_h - B; ++y) put_px(out, x, y, sample);
    }
    // Center
    if (out_w - L - R > 0 && out_h - T - B > 0) {
        if (ns.center_fill) {
            gui_fill_rect(out, L, T, out_w - L - R, out_h - T - B, ns.center_color);
        } else {
            uint32_t c = 0;
            for (int y = T; y < out_h - B; ++y)
                for (int x = L; x < out_w - R; ++x)
                    put_px(out, x, y, c);
        }
    }
    return out;
}

// ---------------- Deque ----------------
void gui_deque_init(GuiDeque* dq) { dq->data=NULL; dq->cap=0; dq->size=0; dq->head=0; }
static bool deque_reserve(GuiDeque* dq, size_t newcap) {
    void** nd = (void**)malloc(newcap * sizeof(void*));
    if (!nd) return false;
    for (size_t i=0;i<dq->size;i++) nd[i]=dq->data[(dq->head + i) % dq->cap];
    free(dq->data);
    dq->data=nd; dq->cap=newcap; dq->head=0;
    return true;
}
bool gui_deque_push_back(GuiDeque* dq, void* item) {
    if (dq->size+1 > dq->cap) {
        size_t nc = dq->cap ? dq->cap*2 : 8;
        if (!deque_reserve(dq, nc)) return false;
    }
    dq->data[(dq->head + dq->size) % dq->cap] = item;
    dq->size++;
    return true;
}
void* gui_deque_pop_front(GuiDeque* dq) {
    if (dq->size==0) return NULL;
    void* it = dq->data[dq->head];
    dq->head = (dq->head + 1) % dq->cap;
    dq->size--;
    return it;
}
size_t gui_deque_count(const GuiDeque* dq){ return dq->size; }
void gui_deque_free(mlx_t *mlx, GuiDeque* dq, void (*free_item)(void*, void*)) {
    if (free_item) {
        while (dq->size) {
            void* it = gui_deque_pop_front(dq);
            free_item(mlx, it);
        }
    }
    free(dq->data);
    dq->data=NULL; dq->cap=0; dq->head=0; dq->size=0;
}

// ---------------- Animation ----------------
void gui_animation_init(GuiAnimation* a, double fps, int x, int y) {
    gui_deque_init(&a->frames);
    a->width = a->height = -1;
    a->fps = fps > 0 ? fps : 12.0;
    a->index = 0;
    a->last_switch_time = 0.0;
    a->x = x; a->y = y;
    a->mounted = false;
}

static void free_image_owned(void *p1, void* p2) {
    mlx_t* mlx = (mlx_t*)p1;
    mlx_image_t* img = (mlx_image_t*)p2;
    if (img && mlx) mlx_delete_image(mlx, img);
}

bool gui_animation_push_image(GuiAnimation* a, mlx_image_t* img) {
    if (!img) return false;
    if (a->width < 0) { a->width = (int)img->width; a->height = (int)img->height; }
    if ((int)img->width != a->width || (int)img->height != a->height) return false;
    return gui_deque_push_back(&a->frames, img);
}

static mlx_image_t* load_image_from_texture(GuiContext* ctx, mlx_texture_t* tex) {
    if (!tex) return NULL;
    mlx_image_t* img = mlx_texture_to_image(ctx->mlx, tex);
    mlx_delete_texture(tex); // we only keep the image
    return img;
}

bool gui_animation_push_png(GuiContext* ctx, GuiAnimation* a, const char* path) {
    const char *file = gui_paths_find(ctx, path);
    if (!file) return false;
    mlx_texture_t* tex = mlx_load_png(file);
    if (!tex) return false;
    mlx_image_t* img = load_image_from_texture(ctx, tex);
    if (!img) return false;
    return gui_animation_push_image(a, img);
}

bool gui_animation_push_xpm42(GuiContext* ctx, GuiAnimation* a, const char* path) {
    const char *file = gui_paths_find(ctx, path);
    if (!file) return false;
    xpm_t* xp = mlx_load_xpm42(file);
    if (!xp) return false;
    mlx_texture_t* tex = &xp->texture;
    if (!tex) goto free_tex;
    mlx_image_t* img = load_image_from_texture(ctx, tex);
    if (!img) goto free_tex;

    free_tex:
        mlx_delete_xpm42(xp);
        return false;

    return gui_animation_push_image(a, img);
}

bool gui_animation_mount(GuiContext* ctx, GuiAnimation* a) {
    if (!ctx || !ctx->mlx || gui_deque_count(&a->frames)==0) return false;
    // Add all frames to the window, place them, and hide all except current
    for (size_t i=0;i<a->frames.size;i++) {
        mlx_image_t* img = (mlx_image_t*)a->frames.data[(a->frames.head + i) % a->frames.cap];
        if (mlx_image_to_window(ctx->mlx, img, a->x, a->y) < 0) return false;
        img->instances[img->count - 1].enabled = (i == 0); // show first frame initially
    }
    a->mounted = true;
    a->last_switch_time = ctx->now;
    return true;
}

void gui_animation_update(GuiContext* ctx, GuiAnimation* a) {
    if (!a->mounted || a->frames.size <= 1 || a->fps <= 0.0) return;
    double spf = 1.0 / a->fps;
    if (ctx->now - a->last_switch_time >= spf) {
        // hide current, show next
        size_t next = (a->index + 1) % a->frames.size;
        mlx_image_t* cur = (mlx_image_t*)a->frames.data[(a->frames.head + a->index) % a->frames.cap];
        mlx_image_t* nxt = (mlx_image_t*)a->frames.data[(a->frames.head + next) % a->frames.cap];
        if (cur->count > 0) cur->instances[cur->count - 1].enabled = false;
        if (nxt->count > 0) nxt->instances[nxt->count - 1].enabled = true;
        a->index = next;
        a->last_switch_time = ctx->now;
    }
}

void gui_animation_free(mlx_t *mlx, GuiAnimation* a) {
    gui_deque_free(mlx, &a->frames, free_image_owned);
    a->width = a->height = -1;
    a->index = 0;
    a->mounted = false;
}

// ---------------- Buttons ----------------
static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

bool gui_button_init(GuiContext* ctx, GuiButton* b, int x, int y, int w, int h,
                     const mlx_texture_t* skin_tex, GuiNineSlice skin_cfg,
                     const char* label_text) {
    if (!ctx || !ctx->mlx || !b || !skin_tex) return false;
    memset(b, 0, sizeof(*b));
    b->x=x; b->y=y; b->w=w; b->h=h;
    b->state = GUI_BTN_NORMAL;
    b->skin_tex = skin_tex;
    b->skin_cfg = skin_cfg;
    b->skin_img = gui_skin_render(ctx, skin_tex, skin_cfg, w, h);
    if (!b->skin_img) return false;
    // Optional label
    if (label_text && *label_text) {
        b->label_img = mlx_put_string(ctx->mlx, label_text, -42, -42);
        if (b->label_img) {
            b->label_dx = (w - (int)b->label_img->width)/2;
            b->label_dy = (h - (int)b->label_img->height)/2;
            if (b->label_img->count > 0) {
                size_t li = b->label_img->count - 1; // usually 0
                b->label_img->instances[li].x = b->x + b->label_dx;
                b->label_img->instances[li].y = b->y + b->label_dy;
            }
        }
    }
    return true;
}

static void render_button(GuiContext* ctx, GuiButton* b) {
    if (b->skin_img) { mlx_delete_image(ctx->mlx, b->skin_img); b->skin_img = NULL; }
    GuiNineSlice cfg = b->skin_cfg;
    if (b->state == GUI_BTN_HOVER) {
        cfg.center_fill = true;
        cfg.center_color = gui_rgba(255,255,255,32); // very light overlay look
    } else if (b->state == GUI_BTN_ACTIVE) {
        cfg.center_fill = true;
        cfg.center_color = gui_rgba(0,0,0,48);       // pressed look
    }
    b->skin_img = gui_skin_render(ctx, b->skin_tex, cfg, b->w, b->h);
    if (b->skin_img) {
        int32_t si = mlx_image_to_window(ctx->mlx, b->skin_img, b->x, b->y);
        if (si >= GUI_MIN_Z_LAYER) b->skin_img->instances[si].z = GUI_MIN_Z_LAYER;
        // keep label on top
        if (b->label_img && b->label_img->count > 0) {
            b->label_img->instances[b->label_img->count-1].z += 1;
        }
    }
}

void gui_button_mount(GuiContext* ctx, GuiButton* b) {
    if (!ctx || !ctx->mlx || !b) return;
    int32_t si = -1;
    if (b->skin_img)
        si = mlx_image_to_window(ctx->mlx, b->skin_img, b->x, b->y);
    // if (b->label_img) mlx_image_to_window(ctx->mlx, b->label_img, b->x + b->label_dx, b->y + b->label_dy);
    if (si >= GUI_MIN_Z_LAYER) b->skin_img->instances[si].z = GUI_MIN_Z_LAYER;
    if (b->label_img && b->label_img->count > 0) {
        size_t li = b->label_img->count - 1;
        b->label_img->instances[li].z = (si >= GUI_MIN_Z_LAYER ? b->skin_img->instances[si].z + 1 : 1);
    }
    render_button(ctx, b);
}

void gui_button_update(GuiContext* ctx, GuiButton* b) {
    if (!ctx || !b) return;
    if (b->state == GUI_BTN_DISABLED) return;

    int mx, my;
    mlx_get_mouse_pos(ctx->mlx, &mx, &my);
    bool over = point_in_rect(mx, my, b->x, b->y, b->w, b->h);
    bool down = mlx_is_mouse_down(ctx->mlx, MLX_MOUSE_BUTTON_LEFT);

    GuiButtonState new_state = over ? (down ? GUI_BTN_ACTIVE : GUI_BTN_HOVER) : GUI_BTN_NORMAL;

    // "Click" on release while hovering
    if (!down && b->was_down && over && b->on_click) b->on_click(b->userdata);

    if (new_state != b->state) {
        b->state = new_state;
        // Visual feedback: simple tint via center fill recolor (optional)
        // Re-render skin to avoid stretching artifacts:
        render_button(ctx, b);
    }
    b->was_down = down;
}

void gui_button_free(mlx_t *mlx, GuiButton* b) {
    if (!b) return;
    if (b->skin_img) { mlx_delete_image(mlx, b->skin_img); b->skin_img=NULL; }
    if (b->label_img){ mlx_delete_image(mlx, b->label_img); b->label_img=NULL; }
    b->skin_tex = NULL;
}

// ---------------- Paths Management ----------------

static bool gui_file_exists_regular(const char* p) {
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISREG(st.st_mode));
}

static char* gui_join_path(const char* base, const char* name) {
    if (!base || !*base) {
        // just duplicate name
        size_t ln = strlen(name);
        char* out = (char*)malloc(ln + 1);
        if (!out) return NULL;
        memcpy(out, name, ln + 1);
        return out;
    }
    size_t lb = strlen(base);
    size_t ln = strlen(name);

    // compute size with exactly one '/'
    bool need_slash = base[lb - 1] != '/';
    size_t total = lb + (need_slash ? 1 : 0) + ln + 1;

    char* out = (char*)malloc(total);
    if (!out) return NULL;

    memcpy(out, base, lb);
    size_t pos = lb;
    if (need_slash) out[pos++] = '/';
    memcpy(out + pos, name, ln);
    out[pos + ln] = '\0';
    return out;
}

bool gui_paths_add(GuiContext* ctx, const char* path) {
    if (!ctx || !path || !*path) return false;

    // Duplicate and (lightly) normalize: strip trailing slashes except root "/"
    size_t lp = strlen(path);
    while (lp > 1 && path[lp - 1] == '/') lp--;
    char* dup = (char*)malloc(lp + 1);
    if (!dup) return false;
    memcpy(dup, path, lp);
    dup[lp] = '\0';

    // Grow array by 1
    char** np = (char**)realloc(ctx->paths, sizeof(char*) * (ctx->paths_len + 1));
    if (!np) { free(dup); return false; }
    ctx->paths = np;
    ctx->paths[ctx->paths_len++] = dup;
    return true;
}

char* gui_paths_find(const GuiContext* ctx, const char* filename) {
    if (!filename || !*filename) return NULL;

    // Absolute or contains '/'? Check directly.
    if (filename[0] == '/' || strchr(filename, '/')) {
        return gui_file_exists_regular(filename) ? strdup(filename) : NULL;
    }

    // Search registered paths
    if (ctx) {
        for (unsigned i = 0; i < ctx->paths_len; ++i) {
            const char* base = ctx->paths[i];
            if (!base) continue;
            char* full = gui_join_path(base, filename);
            if (!full) continue;
            if (gui_file_exists_regular(full)) return full;
            free(full);
        }
    }

    // Fallback: current working directory
    return gui_file_exists_regular(filename) ? strdup(filename) : NULL;
}

mlx_texture_t*  gui_load_png_from_paths(const GuiContext* ctx, char *filename) {
    const char* file = gui_paths_find(ctx, filename);
    if (!file) return NULL;
    return mlx_load_png(file);
}