#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include <stdlib.h>

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


// Scan 'dir' for files ending with ".cub3d" (case-insensitive).
// Returns 0 on success and fills out_paths with a heap-allocated array of
// heap-allocated strings (full paths or names as returned by the OS).
// Caller must free with map_free_paths().
int map_list_levels(const char* dir, char*** out_paths, size_t* out_count);

// Free the result of map_list_levels().
void map_free_paths(char** paths, size_t count);

// Parse a simple .cub3d ASCII grid file (digits 0-9, spaces allowed).
// Lines starting with '#' are ignored. All map rows must have the same width.
// On success, returns 0 and sets *out_data (heap int[w*h]), *out_w, *out_h.
// On failure, returns -1 and sets *out_err to a heap string (print+free).
int map_parse_cub3d_file(const char* path, int** out_data, int* out_w, int* out_h, char** out_err);


#endif
