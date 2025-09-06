#ifndef RAYCAST_H
#define RAYCAST_H

#include "canvas.h"
#include "map.h"
#include "types.h"

void render_scene(Canvas* scene, const GridMap* map, const Camera* cam);

void draw_minimap(Canvas* mini, const GridMap* map, const Camera* cam, int scale);

#endif
