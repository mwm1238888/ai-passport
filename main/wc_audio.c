#include "wc_audio.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define WC_AUD_HZ   16000
#define WC_AUD_BITS 16
#define WC_AUD_CH   1
#define FRAMES      (WC_AUD_HZ / 10)   /* 100 ms frame */

typedef struct {
    wc_sound_t sound;
} wc_req_t;

static QueueHandle_t s_req;
static volatile bool s_stop = false;

static void fill_sine(int16_t *buf, int n, int freq, int gain) {
    for (int i = 0; i < n; i++) {
        float ph = 2.0f * 3.14159265f * freq * i / WC_AUD_HZ;
        int16_t v = (int16_t)(sinf(ph) * gain);
        buf[i] = v;
    }
}

static void play_frames(int freq, int gain, int ms, int gap_ms) {
    static int16_t buf[FRAMES];
    int frames = (ms * WC_AUD_HZ) / 1000 / FRAMES;
    for (int k = 0; k < frames && !s_stop; k++) {
        fill_sine(buf, FRAMES, freq, gain);
        bsp_audio_write(buf, sizeof(buf));
    }
    if (gap_ms > 0) {
        memset(buf, 0, sizeof(buf));
        int gf = (gap_ms * WC_AUD_HZ) / 1000 / FRAMES;
        for (int k = 0; k < gf && !s_stop; k++)
            bsp_audio_write(buf, sizeof(buf));
    }
}

static void play_sound(wc_sound_t s) {
    s_stop = false;
    bsp_audio_set_format(WC_AUD_HZ, WC_AUD_BITS, WC_AUD_CH);
    bsp_audio_set_volume(80);
    switch (s) {
    case WC_SOUND_ALARM:
        for (int i = 0; i < 20 && !s_stop; i++)
            play_frames(1000, 28000, 80, 80);
        break;
    case WC_SOUND_CHIME:
        play_frames(880, 22000, 120, 40);
        play_frames(1320, 22000, 160, 0);
        break;
    case WC_SOUND_REMINDER:
        play_frames(660, 18000, 150, 0);
        break;
    }
}

static void audio_task(void *arg) {
    wc_req_t r;
    for (;;) {
        if (xQueueReceive(s_req, &r, portMAX_DELAY) != pdTRUE) continue;
        play_sound(r.sound);
    }
}

void wc_audio_init(void) {
    s_req = xQueueCreate(4, sizeof(wc_req_t));
    xTaskCreate(audio_task, "wc_audio", 4096, NULL, 5, NULL);
}

void wc_audio_play(wc_sound_t s) {
    if (s_req) {
        wc_req_t r = { .sound = s };
        xQueueSend(s_req, &r, 0);
    }
}

void wc_audio_stop(void) { s_stop = true; }
