#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wc_audio.h"

/* drive the simulated clock */
void sim_set_synced(int synced);
void sim_set_unix(int64_t unix_sec);
void sim_set_mono_ms(uint32_t ms);
void sim_advance_ms(uint32_t ms);

/* audio call capture */
void sim_get_sound(wc_sound_t *out);
void sim_clear_sound(void);