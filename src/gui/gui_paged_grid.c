#include "gui_paged_grid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char* pg_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

static void get_window_size(mlx_t* mlx, int* out_w, int* out_h) {
    *out_w = (int)mlx->width;
    *out_h = (int)mlx->height;
}

static void grid_recompute_geometry(GuiContext* ctx, GuiPagedGrid* g) {
    int win_w = 0, win_h = 0;
    get_window_size(ctx->mlx, &win_w, &win_h);

    g->x = g->cfg.center_h ? (win_w - g->cfg.w) / 2 : g->cfg.x;
    g->y = g->cfg.center_v ? (win_h - g->cfg.h) / 2 : g->cfg.y;

    int pager_h = (g->cfg.pager_h > 0 ? g->cfg.pager_h : 28);
    if (pager_h >= g->cfg.h) pager_h = g->cfg.h / 5; // fallback
    g->content_h = g->cfg.h - pager_h;
}

static void grid_recompute_pagination(GuiPagedGrid* g) {
    g->per_page   = (size_t)(g->cfg.cols * g->cfg.rows);
    if (g->per_page == 0) g->per_page = 1;
    g->page_count = (g->items_len + g->per_page - 1) / g->per_page;
    if (g->page >= g->page_count) g->page = (g->page_count ? g->page_count - 1 : 0);
}

static void pager_update_label(GuiContext* ctx, GuiPagedGrid* g) {
    if (g->page_label_img) {
        mlx_delete_image(ctx->mlx, g->page_label_img);
        g->page_label_img = NULL;
    }
    char buf[48];
    size_t total = (g->page_count ? g->page_count : 1);
    snprintf(buf, sizeof(buf), "%zu / %zu", (g->page_count ? g->page + 1 : (g->items_len ? 1 : 0)), total);
    g->page_label_img = mlx_put_string(ctx->mlx, buf, -42, -42);
}

static void pager_set_enabled(GuiPagedGrid* g, bool prev_enabled, bool next_enabled) {
    g->btn_prev.state = prev_enabled ? GUI_BTN_NORMAL : GUI_BTN_DISABLED;
    g->btn_next.state = next_enabled ? GUI_BTN_NORMAL : GUI_BTN_DISABLED;
}

static void grid_layout_buttons(GuiContext* ctx, GuiPagedGrid* g) {
    // Compute cell sizes
    int cols = g->cfg.cols, rows = g->cfg.rows, gap = g->cfg.gap;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (gap < 0)  gap  = 0;

    int area_w = g->cfg.w;
    int area_h = g->content_h;

    int cell_w = (area_w - (cols - 1) * gap) / cols;
    int cell_h = (area_h - (rows - 1) * gap) / rows;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    // Ensure we have an array of item buttons sized per_page
    if (!g->item_btns) {
        g->item_btns = (GuiButton*)calloc(g->per_page, sizeof(GuiButton));
    }

    // Build/rebuild visible buttons for the current page
    size_t start = g->page * g->per_page;
    for (size_t i = 0; i < g->per_page; ++i) {
        size_t idx = start + i;
        int r = (int)(i / cols);
        int c = (int)(i % cols);

        int bx = g->x + c * (cell_w + gap);
        int by = g->y + r * (cell_h + gap);

        // Fit button within cell (no larger than cell)
        int bw = cell_w;
        int bh = cell_h;

        // If there's an item for this slot
        if (idx < g->items_len) {
            GuiButton* b = &g->item_btns[i];
            // Free previous instance if it existed (belongs to previous page slot)
            if (b->skin_img || b->label_img) {
                gui_button_free(ctx->mlx, b);
                memset(b, 0, sizeof(*b));
            }
            if (!gui_button_init(ctx, b, bx, by, bw, bh, g->cfg.item_skin_tex, g->cfg.item_skin_cfg, g->items[idx].label))
                continue;
            // Connect callback
            b->on_click = g->items[idx].on_click;
            b->userdata = g->items[idx].userdata;
            gui_button_mount(ctx, b);
        } else {
            // No item: clear if any existing
            GuiButton* b = &g->item_btns[i];
            if (b->skin_img || b->label_img) {
                gui_button_free(ctx->mlx, b);
                memset(b, 0, sizeof(*b));
            }
        }
    }

    // Layout pager controls at bottom
    int pager_gap = 10;
    int pager_y = pager_gap + g->y + g->content_h + (g->cfg.h - g->content_h - (g->cfg.pager_h > 0 ? g->cfg.pager_h : 28)) / 2;
    int pager_btn_w = 28, pager_btn_h = 24;

    // Prev button
    if (g->btn_prev.skin_img || g->btn_prev.label_img) gui_button_free(ctx->mlx, &g->btn_prev);
    gui_button_init(ctx, &g->btn_prev,
                    g->x + (area_w/2) - 60 - pager_btn_w, pager_y,
                    pager_btn_w, pager_btn_h,
                    g->cfg.pager_skin_tex, g->cfg.pager_skin_cfg, "<");
    g->btn_prev.on_click = NULL; // set later (requires access to g)
    gui_button_mount(ctx, &g->btn_prev);

    // Next button
    if (g->btn_next.skin_img || g->btn_next.label_img) gui_button_free(ctx->mlx, &g->btn_next);
    gui_button_init(ctx, &g->btn_next,
                    g->x + (area_w/2) + 60, pager_y,
                    pager_btn_w, pager_btn_h,
                    g->cfg.pager_skin_tex, g->cfg.pager_skin_cfg, ">");
    g->btn_next.on_click = NULL; // set later (requires access to g)
    gui_button_mount(ctx, &g->btn_next);

    // Page label in between
    pager_update_label(ctx, g);
    if (g->page_label_img && g->page_label_img->count > 0) {
        size_t li = g->page_label_img->count - 1;
        int label_w = (int)g->page_label_img->width;
        int label_h = (int)g->page_label_img->height;
        g->page_label_img->instances[li].x = g->x + (area_w - label_w) / 2;
        g->page_label_img->instances[li].y = pager_y + (pager_btn_h - label_h) / 2;
        // Ensure label is above pager skins
        g->page_label_img->instances[li].z = (g->btn_next.skin_img ? g->btn_next.skin_img->instances[g->btn_next.skin_img->count-1].z + 1 : 1);
    }

    // Enable/disable pager based on page
    bool has_prev = (g->page_count > 1);
    bool has_next = (g->page_count > 1);
    pager_set_enabled(g, has_prev, has_next);
}

// Pager click handlers
static void on_prev_click(void* userdata) {
    struct { GuiContext* ctx; GuiPagedGrid* g; } *ud = userdata;
    if (!ud || !ud->g) return;
    if (ud->g->page == 0)
        ud->g->page = ud->g->page_count - 1;
    else
        ud->g->page--;
    gui_paged_grid_mount(ud->ctx, ud->g);
}
static void on_next_click(void* userdata) {
    struct { GuiContext* ctx; GuiPagedGrid* g; } *ud = userdata;
    if (!ud || !ud->g) return;
    ud->g->page++;
    ud->g->page %= ud->g->page_count;
    gui_paged_grid_mount(ud->ctx, ud->g);
}

bool gui_paged_grid_init(GuiContext* ctx, GuiPagedGrid* g, GuiPagedGridConfig cfg) {
    if (!ctx || !ctx->mlx || !g) return false;
    memset(g, 0, sizeof(*g));
    g->cfg = cfg;
    g->page = 0;
    g->item_btns = NULL;
    g->page_label_img = NULL;
    g->mounted = false;

    grid_recompute_geometry(ctx, g);
    grid_recompute_pagination(g);
    // Prepare empty buttons array
    if (g->per_page == 0) g->per_page = (size_t)( (cfg.cols > 0 ? cfg.cols : 1) * (cfg.rows > 0 ? cfg.rows : 1) );
    g->item_btns = (GuiButton*)calloc(g->per_page, sizeof(GuiButton));
    return (g->item_btns != NULL);
}

void gui_paged_grid_set_items(GuiPagedGrid* g, const GuiPagedGridItem* items, size_t count) {
    // Free previous
    for (size_t i = 0; i < g->items_len; ++i) {
        // duplicate labels were owned; free them
        free((void*)g->items[i].label);
    }
    free(g->items);
    g->items = NULL;
    g->items_len = 0;

    if (!items || count == 0) return;

    g->items = (GuiPagedGridItem*)calloc(count, sizeof(GuiPagedGridItem));
    if (!g->items) return;

    for (size_t i = 0; i < count; ++i) {
        g->items[i].label = pg_strdup(items[i].label ? items[i].label : "");
        g->items[i].on_click = items[i].on_click;
        g->items[i].userdata = items[i].userdata;
    }
    g->items_len = count;
    g->page = 0; // reset to first page when items change
    grid_recompute_pagination(g);
}

void gui_paged_grid_mount(GuiContext* ctx, GuiPagedGrid* g) {
    if (!ctx || !g) return;

    grid_recompute_geometry(ctx, g);
    grid_recompute_pagination(g);

    // Initial layout & button creation for current page
    grid_layout_buttons(ctx, g);

    // Hook up pager callbacks (after buttons exist)
    static struct { GuiContext* ctx; GuiPagedGrid* g; } prev_ud, next_ud;
    prev_ud.ctx = ctx; prev_ud.g = g;
    next_ud.ctx = ctx; next_ud.g = g;
    g->btn_prev.on_click = on_prev_click;
    g->btn_prev.userdata = &prev_ud;
    g->btn_next.on_click = on_next_click;
    g->btn_next.userdata = &next_ud;

    g->mounted = true;
}

void gui_paged_grid_update(GuiContext* ctx, GuiPagedGrid* g) {
    if (!ctx || !g || !g->mounted) return;

    // If window size may change, re-center dynamically
    int old_x = g->x, old_y = g->y;
    grid_recompute_geometry(ctx, g);
    if (g->x != old_x || g->y != old_y) {
        // Re-layout if centering moved us
        grid_layout_buttons(ctx, g);
    }

    // Update item buttons
    for (size_t i = 0; i < g->per_page; ++i) {
        if (g->item_btns[i].skin_img || g->item_btns[i].label_img)
            gui_button_update(ctx, &g->item_btns[i]);
    }
    // Update pager buttons (disabled ones early-return)
    gui_button_update(ctx, &g->btn_prev);
    gui_button_update(ctx, &g->btn_next);
}

void gui_paged_grid_free(GuiContext* ctx, GuiPagedGrid* g) {
    if (!g) return;

    // Free item UI
    if (g->item_btns) {
        for (size_t i = 0; i < g->per_page; ++i) {
            gui_button_free(ctx->mlx, &g->item_btns[i]);
        }
        free(g->item_btns);
        g->item_btns = NULL;
    }

    // Pager UI
    gui_button_free(ctx->mlx, &g->btn_prev);
    gui_button_free(ctx->mlx, &g->btn_next);
    if (g->page_label_img) {
        mlx_delete_image(ctx->mlx, g->page_label_img);
        g->page_label_img = NULL;
    }

    // Items
    for (size_t i = 0; i < g->items_len; ++i) {
        free((void*)g->items[i].label);
    }
    free(g->items);
    g->items = NULL;
    g->items_len = 0;

    g->mounted = false;
}
