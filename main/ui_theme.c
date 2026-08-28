#include "ui_theme.h"

static uint8_t s_id = 0;

void ui_theme_set_id(uint8_t id) { if (id < ui_theme_count()) s_id = id; }
uint8_t ui_theme_get_id(void) { return s_id; }
uint8_t ui_theme_count(void) { return 2; }

const char *ui_theme_name(uint8_t id)
{
    switch (id) {
    case 1:  return "HUD";
    default: return "Pixel";
    }
}

const ui_theme_t *ui_theme_get(void)
{
    static const ui_theme_t *s_table[2] = { &ui_theme_pixel, &ui_theme_hud };
    if (s_id >= ui_theme_count()) s_id = 0;
    return s_table[s_id];
}