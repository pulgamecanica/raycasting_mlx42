#ifndef SCENE_H
#define SCENE_H

#include <stdbool.h>

struct App;

typedef struct Scene {
    // vtable
    void (*on_init)(struct Scene*, struct App*);
    void (*on_show)(struct Scene*);                 // make visible (enable GUI, etc.)
    void (*on_hide)(struct Scene*);                 // make invisible
    void (*on_update)(struct Scene*, double now, float dt);
    void (*on_render)(struct Scene*);               // draw into App->screen
    void (*on_resize)(struct Scene*, int w, int h); // keep buffers sized
    void (*on_destroy)(struct Scene*);

    struct App* app;
} Scene;

static inline void scene_init(Scene* s, struct App* a)        { if (s && s->on_init)   s->on_init(s, a); }
static inline void scene_show(Scene* s)                        { if (s && s->on_show)   s->on_show(s); }
static inline void scene_hide(Scene* s)                        { if (s && s->on_hide)   s->on_hide(s); }
static inline void scene_update(Scene* s, double n, float dt)  { if (s && s->on_update) s->on_update(s, n, dt); }
static inline void scene_render(Scene* s)                      { if (s && s->on_render) s->on_render(s); }
static inline void scene_resize(Scene* s, int w, int h)        { if (s && s->on_resize) s->on_resize(s, w, h); }
static inline void scene_destroy(Scene* s)                     { if (s && s->on_destroy) s->on_destroy(s); }
#endif