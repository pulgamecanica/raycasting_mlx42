#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef struct { float x, y; } Vec2f;
typedef struct { int   x, y; } Point2i;

typedef struct { uint8_t r, g, b, a; } Color;

typedef struct {
    Vec2f pos;   /* world coords in tiles */
    Vec2f dir;   /* normalized view direction */
    Vec2f plane; /* camera plane: controls FOV (≈ 66° if |plane|=tan(fov/2)) */
} Camera;

static inline Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Color c = { r, g, b, a }; return c;
}

static inline uint32_t color_to_u32(Color c) {
    return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8) | (uint32_t)c.a;
}

#endif
