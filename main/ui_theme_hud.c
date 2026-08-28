/* ui_theme_hud.c — Lopardo-style HUD theme: deep navy surface, cyan corner
 * brackets (no mascot), persistent top status stripe, alert border for
 * reminders. */
#include "ui_theme.h"
#include "fonts/wc_cn_20.h"
#include "lvgl.h"

#define HUD_BG       0x0E1A26
#define HUD_SURFACE  0x152233
#define HUD_MAIN     0xE5EAF0
#define HUD_MUTED    0x8FA3B8
#define HUD_ACCENT   0x2EC5D3
#define HUD_ALERT    0xFF5A5F

typedef struct { lv_obj_t *status; } hud_ctx_t;

static uint32_t hud_color(ui_color_role_t r)
{
    switch (r) {
    case UI_ROLE_SURFACE: return HUD_SURFACE;
    case UI_ROLE_MAIN:    return HUD_MAIN;
    case UI_ROLE_MUTED:   return HUD_MUTED;
    case UI_ROLE_ACCENT:  return HUD_ACCENT;
    case UI_ROLE_ALERT:   return HUD_ALERT;
    default:              return HUD_BG;
    }
}

static void hud_ctx_free(lv_event_t *e)
{
    lv_obj_t *scr = lv_event_get_target(e);
    hud_ctx_t *c = lv_obj_get_user_data(scr);
    if (c) lv_free(c);
}

static void hud_hline(lv_obj_t *s, int x, int y, int w)
{
    lv_obj_t *o = lv_obj_create(s);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(HUD_ACCENT), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_size(o, w, 2);
    lv_obj_set_pos(o, x, y);
}

static void hud_vline(lv_obj_t *s, int x, int y, int h)
{
    lv_obj_t *o = lv_obj_create(s);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(HUD_ACCENT), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_size(o, 2, h);
    lv_obj_set_pos(o, x, y);
}

static lv_obj_t *hud_screen_create(const char *title, const lv_font_t *font)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(HUD_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title ? title : "");
    lv_obj_set_style_text_font(t, font ? font : &wc_cn_20, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(HUD_ACCENT), 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 6);

    hud_ctx_t *c = lv_malloc(sizeof(*c));
    c->status = lv_label_create(scr);
    lv_obj_set_style_text_font(c->status, &wc_cn_20, 0);
    lv_obj_set_style_text_color(c->status, lv_color_hex(HUD_MUTED), 0);
    lv_obj_align(c->status, LV_ALIGN_TOP_RIGHT, -12, 8);

    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, 216, 1);
    lv_obj_set_pos(sep, 12, 30);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2A3B4D), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    lv_obj_set_user_data(scr, c);
    lv_obj_add_event_cb(scr, hud_ctx_free, LV_EVENT_DELETE, NULL);
    return scr;
}

static lv_obj_t *hud_panel_create(lv_obj_t *s, int x, int y, int w, int h)
{
    lv_obj_t *panel = lv_obj_create(s);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(HUD_SURFACE), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    /* corner brackets (drawn above the panel on the screen layer) */
    const int br = 14;
    hud_hline(s, x + 2,     y + 2,    br);     /* top horizontal */
    hud_vline(s, x + 2,     y + 2,    br);     /* left vertical */
    hud_hline(s, x + w - br, y + 2,    br);
    hud_vline(s, x + w - br, y + 2,    br);
    hud_hline(s, x + 2,     y + h - br, br);
    hud_vline(s, x + 2,     y + h - br, br);
    hud_hline(s, x + w - br, y + h - br, br);
    hud_vline(s, x + w - br, y + h - br, br);
    return panel;
}

static lv_obj_t *hud_label(lv_obj_t *p, const char *t, const lv_font_t *f,
                           ui_color_role_t r)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t ? t : "");
    lv_obj_set_style_text_font(l, f ? f : &wc_cn_20, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(hud_color(r)), 0);
    return l;
}

static void hud_status_set(lv_obj_t *s, const char *t)
{
    hud_ctx_t *c = lv_obj_get_user_data(s);
    if (c && c->status) lv_label_set_text(c->status, t ? t : "");
}

static void hud_reminder_open(lv_obj_t *s, lv_obj_t *panel, bool alarm)
{
    (void)s;
    lv_obj_set_style_border_color(panel, lv_color_hex(alarm ? HUD_ALERT : HUD_ACCENT), 0);
    lv_obj_set_style_border_width(panel, 3, 0);
}

static void hud_reminder_close(lv_obj_t *s)
{
    (void)s;
}

const ui_theme_t ui_theme_hud = {
    .name = "HUD",
    .panel_x = 16, .panel_y = 40, .panel_w = 208, .panel_h = 180,
    .hint_x = 14, .hint_y = 246, .hint_w = 210,
    .color = hud_color,
    .screen_create = hud_screen_create,
    .panel_create = hud_panel_create,
    .label = hud_label,
    .status_set = hud_status_set,
    .reminder_open = hud_reminder_open,
    .reminder_close = hud_reminder_close,
};