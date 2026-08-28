/* ui_theme.h — runtime-switchable display theme abstraction.
 * Business code (wc_ui.c) only talks to these ops + semantic color roles,
 * so adding themes never changes page/reminder logic. */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_ROLE_BG = 0,     /* screen background */
    UI_ROLE_SURFACE,    /* panel surface */
    UI_ROLE_MAIN,       /* primary text */
    UI_ROLE_MUTED,      /* hint / secondary text */
    UI_ROLE_ACCENT,     /* accent color */
    UI_ROLE_ALERT,      /* alarm / record emphasis */
    UI_ROLE_COUNT
} ui_color_role_t;

typedef struct ui_theme ui_theme_t;
struct ui_theme {
    const char *name;
    int panel_x, panel_y, panel_w, panel_h;   /* content panel geometry */
    int hint_x, hint_y, hint_w;      /* where business places the bottom hint line */

    uint32_t (*color)(ui_color_role_t r);
    lv_obj_t *(*screen_create)(const char *title, const lv_font_t *title_font);
    lv_obj_t *(*panel_create)(lv_obj_t *screen, int x, int y, int w, int h);
    lv_obj_t *(*label)(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, ui_color_role_t r);
    void (*status_set)(lv_obj_t *screen, const char *text);   /* persistent HUD stripe */
    void (*reminder_open)(lv_obj_t *screen, lv_obj_t *panel, bool alarm);
    void (*reminder_close)(lv_obj_t *screen);
};

void ui_theme_set_id(uint8_t id);
uint8_t ui_theme_get_id(void);
uint8_t ui_theme_count(void);
const char *ui_theme_name(uint8_t id);
const ui_theme_t *ui_theme_get(void);

extern const ui_theme_t ui_theme_pixel;
extern const ui_theme_t ui_theme_hud;

#ifdef __cplusplus
}
#endif