#include "app.h"
#include "menu_scene.h"
#include "game_scene.h"

int main(void) {
    App* app = app_create(800, 600, "MLX42 Raycaster");
    if (!app) return 1;

    // instantiate scenes (static storage or heap)
    static MenuScene menu;
    static GameScene game;
    menu_scene_init_instance(&menu);
    game_scene_init_instance(&game);

    sm_register(&app->sm, SCN_MENU, (Scene*)&menu);
    sm_register(&app->sm, SCN_GAME, (Scene*)&game);

    // start on menu
    sm_request_change(&app->sm, SCN_MENU);
    sm_process_switch(&app->sm); 
    
    app_run(app);
    app_destroy(app);
    return 0;
}
