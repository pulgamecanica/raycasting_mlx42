#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <stdbool.h>
#include "scene.h"

typedef enum {
    SCN_MENU = 0,
    SCN_GAME = 1,
    SCN_COUNT
} SceneId;

typedef struct SceneManager {
    struct App* app;
    Scene*      scenes[SCN_COUNT];
    SceneId     current;
    bool        has_current;

    // deferred switching
    bool        change_pending;
    SceneId     next;
} SceneManager;

void sm_init(SceneManager* sm, struct App* app);
void sm_register(SceneManager* sm, SceneId id, Scene* s);
void sm_request_change(SceneManager* sm, SceneId id);   // safe: just sets a flag
void sm_process_switch(SceneManager* sm);               // call at end of frame

// helpers
static inline Scene* sm_active(SceneManager* sm) {
  if (sm->has_current)
    return sm->scenes[sm->current];
  return 0;
}
#endif