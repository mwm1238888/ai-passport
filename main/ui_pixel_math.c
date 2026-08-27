#include "ui_pixel_math.h"

int ui_pixel_blink_frame(int ms) {
    int phase = ms % 2000;
    return (phase >= 1700 && phase < 1850) ? 1 : 0;
}

int ui_pixel_jump_offset(int frame) {
    static const int offsets[] = {0, -3, -5, -3, 0};
    return offsets[frame % 5];
}
