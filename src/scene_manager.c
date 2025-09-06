#include "scene_manager.h"
#include "app.h"

void sm_init(SceneManager* sm, struct App* app) {
    sm->app = app;
    for (int i = 0; i < SCN_COUNT; ++i) sm->scenes[i] = NULL;
    sm->has_current = false;
    sm->change_pending = false;
    sm->next = SCN_MENU;
}

void sm_register(SceneManager* sm, SceneId id, Scene* s) {
    sm->scenes[id] = s;
}

void sm_request_change(SceneManager* sm, SceneId id) {
    sm->change_pending = true;
    sm->next = id;
}
#include <stdio.h>

void sm_process_switch(SceneManager* sm) {
    if (!sm->change_pending) return;
    sm->change_pending = false;

    Scene* next = sm->scenes[sm->next];
    if (!next) return;

    if (sm->has_current) {
        Scene* cur = sm->scenes[sm->current];
        scene_hide(cur);
    }

    if (!sm->has_current || next != sm->scenes[sm->current]) {
        // first time or switching; ensure initialized
        if (!next->app) scene_init(next, sm->app);
    }

    scene_show(next);
    sm->current = sm->next;
    sm->has_current = true;
}
