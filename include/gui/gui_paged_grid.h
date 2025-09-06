#ifndef GUI_PAGED_GRID_H
#define GUI_PAGED_GRID_H

#include "MLX42/MLX42.h"
#include "gui.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct GuiPagedGridItem {
    const char* label;                 // button text (will be duplicated internally)
    void (*on_click)(void* userdata);  // callback when the item button is clicked
    void* userdata;                    // user data passed to callback
} GuiPagedGridItem;

typedef struct GuiPagedGridConfig {
    // Area & auto-centering
    int x, y, w, h;         // requested area (before centering)
    bool center_h;          // center horizontally in window
    bool center_v;          // center vertically in window

    // Layout
    int cols;               // number of columns (>=1)
    int rows;               // number of rows (>=1)
    int gap;                // pixels between buttons (>=0)
    int pager_h;            // reserved height at bottom for pager (<= h). If 0, defaults to 28.

    // Skins
    const mlx_texture_t* item_skin_tex;   // not owned
    GuiNineSlice         item_skin_cfg;
    const mlx_texture_t* pager_skin_tex;  // not owned
    GuiNineSlice         pager_skin_cfg;
} GuiPagedGridConfig;

typedef struct GuiPagedGrid {
    GuiPagedGridConfig cfg;

    // Computed (after centering)
    int x, y;              // top-left of the grid area in window
    int content_h;         // h - pager_h

    // Items (owned)
    GuiPagedGridItem* items;
    size_t items_len;

    // Pagination
    size_t page;           // 0-based
    size_t per_page;       // rows * cols
    size_t page_count;     // ceil(items_len / per_page)

    // Buttons for current page (size = per_page; owned)
    GuiButton* item_btns;

    // Pager controls (owned)
    GuiButton btn_prev;
    GuiButton btn_next;
    mlx_image_t* page_label_img; // "1 / N"

    bool mounted;
} GuiPagedGrid;

// Lifecycle
bool gui_paged_grid_init(GuiContext* ctx, GuiPagedGrid* g, GuiPagedGridConfig cfg);
void gui_paged_grid_set_items(GuiPagedGrid* g, const GuiPagedGridItem* items, size_t count);
void gui_paged_grid_mount(GuiContext* ctx, GuiPagedGrid* g);
void gui_paged_grid_update(GuiContext* ctx, GuiPagedGrid* g);
void gui_paged_grid_free(GuiContext* ctx, GuiPagedGrid* g);
void gui_paged_grid_set_enabled(GuiPagedGrid* g, bool en);


#endif // GUI_PAGED_GRID_H
