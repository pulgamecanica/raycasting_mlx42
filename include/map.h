#ifndef MAP_H
#define MAP_H

typedef struct {
    int w, h;
    const int* data; /* row-major: data[y * w + x] */
} GridMap;

int map_at(const GridMap* m, int x, int y);
int map_is_wall(const GridMap* m, int x, int y);
// int map_is_door(const GridMap* m, int x, int y);

extern const int WORLD_W;
extern const int WORLD_H;
extern const int WORLD_DATA[]; /* exposed for init */

#endif
