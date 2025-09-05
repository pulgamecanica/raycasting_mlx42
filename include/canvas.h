#ifndef CANVAS_H
#define CANVAS_H

#include <MLX42/MLX42.h>
#include "types.h"

typedef struct {
    mlx_t*       mlx;
    mlx_image_t* img;
    int          w, h;
} Canvas;

int  canvas_init(Canvas* c, mlx_t* mlx, int w, int h);
void canvas_destroy(Canvas* c);

void canvas_clear(Canvas* c, Color col);
void canvas_put(Canvas* c, int x, int y, Color col);
void canvas_fill_rect(Canvas* c, int x, int y, int w, int h, Color col);

/* Opaque blit (no alpha). Copies src into dst at (dx,dy). Bounds-safe. */
void canvas_copy(Canvas* dst, const Canvas* src, int dx, int dy);

#endif
