#include "raycast.h"
#include <math.h>
#include <stdlib.h>

static void draw_vertical(Canvas* c, int x, int y0, int y1, Color col) {
    if (y0 < 0) y0 = 0;
    if (y1 >= c->h) y1 = c->h - 1;
    if ((unsigned)x >= (unsigned)c->w || y0 > y1) return;
    uint32_t v = color_to_u32(col);
    uint32_t* px = (uint32_t*)c->img->pixels;
    for (int y = y0; y <= y1; ++y) px[y * c->w + x] = v;
}

void render_scene(Canvas* scene, const GridMap* map, const Camera* cam) {
    /* Simple sky/floor clear */
    for (int y = 0; y < scene->h; ++y) {
        Color col = (y < scene->h/2) ? rgba(135,206,235,255) : rgba(40,40,40,255);
        uint32_t v = color_to_u32(col);
        uint32_t* row = (uint32_t*)scene->img->pixels + y * scene->w;
        for (int x = 0; x < scene->w; ++x) row[x] = v;
    }

    for (int x = 0; x < scene->w; ++x) {
        float cameraX = 2.0f * x / (float)scene->w - 1.0f;
        float rayDirX = cam->dir.x + cam->plane.x * cameraX;
        float rayDirY = cam->dir.y + cam->plane.y * cameraX;

        int mapX = (int)cam->pos.x;
        int mapY = (int)cam->pos.y;

        float deltaX = (rayDirX == 0.0f) ? 1e30f : fabsf(1.0f / rayDirX);
        float deltaY = (rayDirY == 0.0f) ? 1e30f : fabsf(1.0f / rayDirY);

        int stepX, stepY;
        float sideX, sideY;

        if (rayDirX < 0) { stepX = -1; sideX = (cam->pos.x - mapX) * deltaX; }
        else             { stepX =  1; sideX = (mapX + 1.0f - cam->pos.x) * deltaX; }
        if (rayDirY < 0) { stepY = -1; sideY = (cam->pos.y - mapY) * deltaY; }
        else             { stepY =  1; sideY = (mapY + 1.0f - cam->pos.y) * deltaY; }

        int hit = 0, side = 0, tile = 0;
        while (!hit) {
            if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else               { sideY += deltaY; mapY += stepY; side = 1; }
            tile = map_at(map, mapX, mapY);
            if (tile > 0) hit = 1;
        }

        float perpDist = (side == 0) ? (sideX - deltaX) : (sideY - deltaY);
        if (perpDist < 1e-6f) perpDist = 1e-6f;

        int lineH = (int)(scene->h / perpDist);
        int drawStart = -lineH / 2 + scene->h / 2;
        int drawEnd   =  lineH / 2 + scene->h / 2;

        Color base = (tile == 1) ? rgba(200, 0, 0, 255) :
                     (tile == 2) ? rgba(0, 200, 0, 255) :
                     (tile == 3) ? rgba(0, 0, 200, 255) :
                     (tile == 4) ? rgba(200, 200, 0, 255) :
                                   rgba(200, 200, 200, 255);

        if (side == 1) { /* simple shading for y-sides */
            base.r >>= 1; base.g >>= 1; base.b >>= 1;
        }

        draw_vertical(scene, x, drawStart, drawEnd, base);
    }
}

void draw_minimap(Canvas* mini, const GridMap* map, const Camera* cam, int scale) {
    canvas_clear(mini, rgba(0,0,0,255));
    /* walls */
    for (int y = 0; y < map->h; ++y) {
        for (int x = 0; x < map->w; ++x) {
            int v = map_at(map, x, y);
            Color c = v ? rgba(220,220,220,255) : rgba(30,30,30,255);
            canvas_fill_rect(mini, x * scale, y * scale, scale, scale, c);
        }
    }
    /* player */
    int px = (int)(cam->pos.x * scale);
    int py = (int)(cam->pos.y * scale);
    canvas_fill_rect(mini, px-2, py-2, 5, 5, rgba(255,50,50,255));

    /* facing line */
    int lx = px + (int)(cam->dir.x * 10.0f);
    int ly = py + (int)(cam->dir.y * 10.0f);
    /* simple Bresenham */
    int dx = abs(lx - px), sx = px < lx ? 1 : -1;
    int dy = -abs(ly - py), sy = py < ly ? 1 : -1;
    int err = dx + dy, x0 = px, y0 = py;
    for (;;) {
        canvas_put(mini, x0, y0, rgba(255,100,100,255));
        if (x0 == lx && y0 == ly) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
