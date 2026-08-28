/* ui_pixel.h — pixel-art theme for FoloToy AI Passport (upstream baseline).
 * Adapted for Work Companion: title font parameter added for CJK titles. */
#pragma once
#include "lvgl.h"

#define UI_SKY       0xA9DDF7
#define UI_GRASS     0x94C560
#define UI_GRASS_DARK 0x6E8B3D
#define UI_PAPER     0xFDF6E3
#define UI_INK       0x2B2B2B
#define UI_ORANGE    0xF59E0B
#define UI_RED       0xE23D2E
#define UI_YELLOW    0xFDE047

typedef struct {
    const char *title;
    const lv_font_t *title_font;
    uint32_t bg;            /* background color, UI_SKY by default */
} ui_pixel_scr_opts_t;

lv_obj_t *ui_pixel_screen_create(const char *title);
lv_obj_t *ui_pixel_screen_create_opts(const ui_pixel_scr_opts_t *opts);
lv_obj_t *ui_pixel_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t color);
lv_obj_t *ui_pixel_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color);
lv_obj_t *ui_pixel_mascot_create(lv_obj_t *parent, int x, int y);
void ui_pixel_mascot_jump(lv_obj_t *mascot);
void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled);
