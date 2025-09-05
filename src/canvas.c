#include "canvas.h"
#include <string.h>

int canvas_init(Canvas* c, mlx_t* mlx, int w, int h) {
    c->mlx = mlx; c->w = w; c->h = h;
    c->img = mlx_new_image(mlx, w, h);
    return c->img ? 0 : -1;
}

void canvas_destroy(Canvas* c) {
    if (c->img) {
        mlx_delete_image(c->mlx, c->img);
        c->img = NULL;
    }
}

void canvas_put(Canvas* c, int x, int y, Color col) {
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h) return;
    uint32_t* px = (uint32_t*)c->img->pixels;
    px[y * c->w + x] = color_to_u32(col);
}

void canvas_clear(Canvas* c, Color col) {
    uint32_t* px = (uint32_t*)c->img->pixels;
    uint32_t v = color_to_u32(col);
    for (int i = 0; i < c->w * c->h; ++i) px[i] = v;
}

void canvas_fill_rect(Canvas* c, int x, int y, int w, int h, Color col) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > c->w) x1 = c->w;
    int y1 = y + h; if (y1 > c->h) y1 = c->h;
    uint32_t v = color_to_u32(col);
    uint32_t* px = (uint32_t*)c->img->pixels;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = px + yy * c->w;
        for (int xx = x0; xx < x1; ++xx) row[xx] = v;
    }
}

void canvas_copy(Canvas* dst, const Canvas* src, int dx, int dy) {
    int w = src->w, h = src->h;
    for (int y = 0; y < h; ++y) {
        int ty = y + dy;
        if ((unsigned)ty >= (unsigned)dst->h) continue;
        for (int x = 0; x < w; ++x) {
            int tx = x + dx;
            if ((unsigned)tx >= (unsigned)dst->w) continue;
            ((uint32_t*)dst->img->pixels)[ty * dst->w + tx] =
                ((uint32_t*)src->img->pixels)[y * src->w + x];
        }
    }
}
