/* ui_theme_pixel.c — wraps the pixel-art primitives (ui_pixel.c) as a theme.
 * Preserves the exact look: sky/grass, paper panel, ink text, TV-robot mascot. */
#include "ui_theme.h"
#include "ui_pixel.h"
#include "lvgl.h"

typedef struct { lv_obj_t *mascot; lv_timer_t *jump_timer; } pixel_ctx_t;

static uint32_t pixel_color(ui_color_role_t r)
{
    switch (r) {
    case UI_ROLE_MAIN:   return UI_INK;
    case UI_ROLE_MUTED:  return 0x52525B;
    case UI_ROLE_ACCENT: return UI_INK;
    case UI_ROLE_ALERT:  return UI_RED;
    default:             return UI_PAPER;
    }
}

static void pixel_ctx_free(lv_event_t *e)
{
    lv_obj_t *scr = lv_event_get_target(e);
    pixel_ctx_t *c = lv_obj_get_user_data(scr);
    if (!c) return;
    if (c->jump_timer) lv_timer_delete(c->jump_timer);
    lv_free(c);
}

static lv_obj_t *pixel_screen_create(const char *title, const lv_font_t *font)
{
    ui_pixel_scr_opts_t o = { .title = title,
                              .title_font = font ? font : &lv_font_montserrat_20 };
    lv_obj_t *scr = ui_pixel_screen_create_opts(&o);
    pixel_ctx_t *c = lv_malloc(sizeof(*c));
    c->jump_timer = NULL;
    c->mascot = ui_pixel_mascot_create(scr, 16, 230);
    lv_obj_set_user_data(scr, c);
    lv_obj_add_event_cb(scr, pixel_ctx_free, LV_EVENT_DELETE, NULL);
    return scr;
}

static lv_obj_t *pixel_panel_create(lv_obj_t *s, int x, int y, int w, int h)
{
    return ui_pixel_panel_create(s, x, y, w, h, UI_PAPER);
}

static lv_obj_t *pixel_label(lv_obj_t *p, const char *t, const lv_font_t *f,
                             ui_color_role_t r)
{
    return ui_pixel_label(p, t, f, pixel_color(r));
}

static void pixel_status_set(lv_obj_t *s, const char *t) { (void)s; (void)t; }

static void pixel_jump_cb(lv_timer_t *tm)
{
    pixel_ctx_t *c = lv_timer_get_user_data(tm);
    c->jump_timer = NULL;
    if (c->mascot && lv_obj_is_valid(c->mascot)) ui_pixel_mascot_jump(c->mascot);
}

static void pixel_reminder_open(lv_obj_t *s, lv_obj_t *panel, bool alarm)
{
    (void)panel; (void)alarm;
    pixel_ctx_t *c = lv_obj_get_user_data(s);
    if (!c || !c->mascot) return;
    ui_pixel_mascot_jump(c->mascot);
    if (!c->jump_timer) {
        c->jump_timer = lv_timer_create(pixel_jump_cb, 260, NULL);
        lv_timer_set_user_data(c->jump_timer, c);
        lv_timer_set_repeat_count(c->jump_timer, 1);
    }
}

static void pixel_reminder_close(lv_obj_t *s)
{
    pixel_ctx_t *c = lv_obj_get_user_data(s);
    if (c && c->jump_timer) { lv_timer_delete(c->jump_timer); c->jump_timer = NULL; }
}

const ui_theme_t ui_theme_pixel = {
    .name = "Pixel",
    .panel_x = 16, .panel_y = 56, .panel_w = 208, .panel_h = 168,
    .hint_x = 66, .hint_y = 244, .hint_w = 156,
    .color = pixel_color,
    .screen_create = pixel_screen_create,
    .panel_create = pixel_panel_create,
    .label = pixel_label,
    .status_set = pixel_status_set,
    .reminder_open = pixel_reminder_open,
    .reminder_close = pixel_reminder_close,
};