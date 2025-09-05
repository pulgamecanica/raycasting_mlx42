#include "map.h"
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

const int WORLD_W = 24;
const int WORLD_H = 24;

/* 0 = empty, >0 = wall type */
const int WORLD_DATA[] = {
/* A tiny Wolf3D-like test map */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,3,3,3,0,0,0,1,
1,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,3,0,3,0,0,0,1,
1,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,3,3,3,0,0,0,1,
1,0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,4,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,4,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

int map_at(const GridMap* m, int x, int y) {
    if ((unsigned)x >= (unsigned)m->w || (unsigned)y >= (unsigned)m->h) return 1; /* treat out-of-bounds as wall */
    return m->data[y * m->w + x];
}

int map_is_wall(const GridMap* m, int x, int y) {
    return map_at(m, x, y) > 0;
}

static int has_ext_cub3d(const char* name) {
    if (!name) return 0;
    const char* dot = strrchr(name, '.');
    if (!dot) return 0;
    const char* ext = dot + 1; // after '.'
    // accept "cub3d" (case-insensitive)
    const char* want = "cub3d";
    while (*ext && *want) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*want)) return 0;
        ++ext; ++want;
    }
    return *ext == '\0' && *want == '\0';
}

static int is_regular_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

// Join dir + "/" + name (handles trailing '/')
static char* join_path(const char* dir, const char* name) {
    size_t ld = strlen(dir), ln = strlen(name);
    int need_slash = (ld > 0 && dir[ld-1] != '/');
    size_t total = ld + (need_slash ? 1 : 0) + ln + 1;
    char* out = (char*)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, ld);
    size_t pos = ld;
    if (need_slash) out[pos++] = '/';
    memcpy(out + pos, name, ln);
    out[pos + ln] = '\0';
    return out;
}

int map_list_levels(const char* dir, char*** out_paths, size_t* out_count) {
    if (!out_paths || !out_count || !dir) return -1;
    *out_paths = NULL; *out_count = 0;

    DIR* d = opendir(dir);
    if (!d) return -1;

    size_t cap = 16, n = 0;
    char** arr = (char**)malloc(cap * sizeof(char*));
    if (!arr) { closedir(d); return -1; }

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue; // skip hidden/.,..
        if (!has_ext_cub3d(ent->d_name)) continue;

        char* full = join_path(dir, ent->d_name);
        if (!full) continue;
        if (!is_regular_file(full)) { free(full); continue; }

        if (n == cap) {
            cap *= 2;
            char** tmp = (char**)realloc(arr, cap * sizeof(char*));
            if (!tmp) { free(full); break; }
            arr = tmp;
        }
        arr[n++] = full;
    }
    closedir(d);

    if (n == 0) { free(arr); return 0; }
    *out_paths = arr;
    *out_count = n;
    return 0;
}

void map_free_paths(char** paths, size_t count) {
    if (!paths) return;
    for (size_t i = 0; i < count; ++i) free(paths[i]);
    free(paths);
}

// Very lenient integer parser for a line (digits + optional spaces).
// Appends parsed ints to a growing buffer (realloc).
static int parse_line_ints(const char* line, int** buf, size_t* len, size_t* cap) {
    const char* p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == '\0' || *p == '\n') break;
        if (*p == '#') break; // rest of line is comment
        // read an integer (single digit 0-9 or multi-digit)
        int neg = 0;
        if (*p == '+') { ++p; }
        else if (*p == '-') { neg = 1; ++p; }
        if (!isdigit((unsigned char)*p)) {
            // invalid token: bail out
            return -1;
        }
        long v = 0;
        while (isdigit((unsigned char)*p)) { v = v*10 + (*p - '0'); ++p; }
        if (neg) v = -v;

        if (*len == *cap) {
            size_t nc = (*cap ? *cap * 2 : 64);
            int* nb = (int*)realloc(*buf, nc * sizeof(int));
            if (!nb) return -1;
            *buf = nb; *cap = nc;
        }
        (*buf)[(*len)++] = (int)v;

        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (*p == ',' ) ++p; // optional comma separator
    }
    return 0;
}

int map_parse_cub3d_file(const char* path, int** out_data, int* out_w, int* out_h, char** out_err) {
    if (out_err) *out_err = NULL;
    if (!out_data || !out_w || !out_h || !path) return -1;

    FILE* f = fopen(path, "rb");
    if (!f) {
        if (out_err) {
            size_t n = strlen(strerror(errno)) + 64 + strlen(path);
            *out_err = (char*)malloc(n);
            if (*out_err) snprintf(*out_err, n, "cannot open '%s': %s", path, strerror(errno));
        }
        return -1;
    }

    int* buf = NULL; size_t len = 0, cap = 0;
    int width = -1, height = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f)) {
        const char* p = line;
        // skip comments/empty
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        // Remember length before parsing this row
        size_t len_before = len;
        if (parse_line_ints(line, &buf, &len, &cap) != 0) {
            if (out_err) {
                const char* msg = "invalid token in map line";
                *out_err = (char*)malloc(strlen(msg)+1);
                if (*out_err) strcpy(*out_err, msg);
            }
            fclose(f);
            free(buf);
            return -1;
        }
        size_t row_w = len - len_before;
        if (row_w == 0) continue; // empty row after filters
        if (width < 0) width = (int)row_w;
        else if ((int)row_w != width) {
            if (out_err) {
                const char* msg = "non-rectangular map (rows have different widths)";
                *out_err = (char*)malloc(strlen(msg)+1);
                if (*out_err) strcpy(*out_err, msg);
            }
            fclose(f);
            free(buf);
            return -1;
        }
        height++;
    }
    fclose(f);

    if (width <= 0 || height <= 0) {
        if (out_err) {
            const char* msg = "empty or invalid map";
            *out_err = (char*)malloc(strlen(msg)+1);
            if (*out_err) strcpy(*out_err, msg);
        }
        free(buf);
        return -1;
    }
    if ((size_t)(width * height) != len) {
        // Shouldn't happen if we enforced rectangularity
        if (out_err) {
            const char* msg = "internal size mismatch";
            *out_err = (char*)malloc(strlen(msg)+1);
            if (*out_err) strcpy(*out_err, msg);
        }
        free(buf);
        return -1;
    }

    *out_data = buf;
    *out_w = width;
    *out_h = height;
    return 0;
}
