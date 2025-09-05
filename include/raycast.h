#ifndef RAYCAST_H
#define RAYCAST_H

#include "canvas.h"
#include "map.h"
#include "types.h"

typedef struct {
    Vec2f pos;   /* world coords in tiles */
    Vec2f dir;   /* normalized view direction */
    Vec2f plane; /* camera plane: controls FOV (≈ 66° if |plane|=tan(fov/2)) */
} Camera;

void render_scene(Canvas* scene, const GridMap* map, const Camera* cam);

void draw_minimap(Canvas* mini, const GridMap* map, const Camera* cam, int scale);

#endif
