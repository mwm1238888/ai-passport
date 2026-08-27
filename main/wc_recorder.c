#include "wc_recorder.h"
#include "wc_state.h"
#include "wc_config.h"
#include "bsp_audio.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "wc_rec";
#define CHUNK_SAMPLES (WC_REC_HZ * WC_REC_CHUNK_MS / 1000)   /* 200ms @16k = 3200 */
#define CHUNK_BYTES   (CHUNK_SAMPLES * (WC_REC_BITS / 8) * WC_REC_CHANNELS)
#define TASK_STACK     6144

typedef enum { RC_START, RC_STOP, RC_PLAY, RC_STOPPLAY } rec_cmd_t;
typedef struct { rec_cmd_t cmd; int idx; } rec_req_t;

static QueueHandle_t s_req;
static volatile wc_rec_state_t s_state = WC_REC_IDLE;
static bool s_mounted = false;

static bool mount_fs(void) {
    if (s_mounted) return true;
    esp_vfs_littlefs_conf_t conf = {
        .partition_label = WC_REC_PARTITION,
        .mount_point = WC_REC_MOUNT,
        .max_files = 4,
    };
    if (esp_vfs_littlefs_mount(&conf) != ESP_OK) {
        ESP_LOGE(TAG, "mount failed");
        return false;
    }
    s_mounted = true;
    return true;
}

/* count files + fill sorted name list into caller array (max cnt) */
static int list_files(char names[][32], int cnt) {
    if (!mount_fs()) return 0;
    DIR *d = opendir(WC_REC_MOUNT);
    if (!d) return 0;
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < cnt) {
        if (strstr(de->d_name, ".pcm")) {
            strncpy(names[n], de->d_name, 31);
            names[n][31] = 0;
            n++;
        }
    }
    closedir(d);
    /* simple bubble sort (names are zero-padded) */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[32]; memcpy(t, names[i], 32);
                memcpy(names[i], names[j], 32); memcpy(names[j], t, 32);
            }
    return n;
}

static void prune_old(void) {
    char names[WC_REC_MAX_FILES + 4][32];
    int n = list_files(names, WC_REC_MAX_FILES + 4);
    while (n > WC_REC_MAX_FILES) {
        char path[48];
        snprintf(path, sizeof(path), "%s/%s", WC_REC_MOUNT, names[0]);
        remove(path);
        for (int i = 1; i < n; i++) memcpy(names[i - 1], names[i], 32);
        n--;
    }
}

static int next_index(void) {
    char names[WC_REC_MAX_FILES + 4][32];
    int n = list_files(names, WC_REC_MAX_FILES + 4);
    return n;   /* index = current count */
}

static void do_record(void) {
    if (!mount_fs()) return;
    prune_old();
    char path[48];
    snprintf(path, sizeof(path), "%s/REC_%04d.pcm", WC_REC_MOUNT, next_index());
    FILE *f = fopen(path, "wb");
    if (!f) { ESP_LOGE(TAG, "open %s failed", path); return; }

    static int16_t buf[CHUNK_SAMPLES];
    bsp_audio_set_format(WC_REC_HZ, WC_REC_BITS, WC_REC_CHANNELS);
    s_state = WC_REC_RECORDING;

    while (s_state == WC_REC_RECORDING) {
        if (bsp_audio_read(buf, CHUNK_BYTES) == ESP_OK) {
            fwrite(buf, 1, CHUNK_BYTES, f);
        }
        /* check stop flag handled via state transition from cmd queue */
        rec_req_t r;
        if (xQueueReceive(s_req, &r, 0) == pdTRUE && r.cmd == RC_STOP) {
            s_state = WC_REC_IDLE;
            break;
        }
    }
    fclose(f);
    s_state = WC_REC_IDLE;
    wc_event_t ev = { .id = WC_EV_RECORD_DONE };
    wc_state_post(ev);
}

static void do_play(int idx) {
    char names[WC_REC_MAX_FILES + 4][32];
    int n = list_files(names, WC_REC_MAX_FILES + 4);
    if (idx < 0 || idx >= n) return;
    char path[48];
    snprintf(path, sizeof(path), "%s/%s", WC_REC_MOUNT, names[idx]);
    FILE *f = fopen(path, "rb");
    if (!f) return;

    static int16_t buf[CHUNK_SAMPLES];
    bsp_audio_set_format(WC_REC_HZ, WC_REC_BITS, WC_REC_CHANNELS);
    s_state = WC_REC_PLAYING;
    while (s_state == WC_REC_PLAYING) {
        size_t got = fread(buf, 1, CHUNK_BYTES, f);
        if (got == 0) break;
        bsp_audio_write(buf, got);
        rec_req_t r;
        if (xQueueReceive(s_req, &r, 0) == pdTRUE && r.cmd == RC_STOPPLAY) {
            s_state = WC_REC_IDLE;
            break;
        }
    }
    fclose(f);
    s_state = WC_REC_IDLE;
}

static void rec_task(void *arg) {
    rec_req_t r;
    for (;;) {
        if (xQueueReceive(s_req, &r, portMAX_DELAY) != pdTRUE) continue;
        if (r.cmd == RC_START)      do_record();
        else if (r.cmd == RC_PLAY)  do_play(r.idx);
        else if (r.cmd == RC_STOP)  s_state = WC_REC_IDLE;
        else if (r.cmd == RC_STOPPLAY) s_state = WC_REC_IDLE;
    }
}

void wc_recorder_init(void) {
    s_req = xQueueCreate(4, sizeof(rec_req_t));
    mount_fs();
    xTaskCreate(rec_task, "wc_rec", TASK_STACK, NULL, 6, NULL);
}

bool wc_recorder_start(void) {
    if (s_state != WC_REC_IDLE) return false;
    rec_req_t r = { .cmd = RC_START };
    xQueueSend(s_req, &r, 0);
    return true;
}

void wc_recorder_stop(void) {
    rec_req_t r = { .cmd = RC_STOP };
    xQueueSend(s_req, &r, 0);
}

wc_rec_state_t wc_recorder_state(void) { return s_state; }

int wc_recorder_count(void) {
    char names[WC_REC_MAX_FILES + 4][32];
    return list_files(names, WC_REC_MAX_FILES + 4);
}

const char *wc_recorder_name(int idx) {
    static char name[32];
    char names[WC_REC_MAX_FILES + 4][32];
    int n = list_files(names, WC_REC_MAX_FILES + 4);
    if (idx < 0 || idx >= n) return NULL;
    strncpy(name, names[idx], 31);
    return name;
}

bool wc_recorder_play(int idx) {
    if (s_state != WC_REC_IDLE) return false;
    rec_req_t r = { .cmd = RC_PLAY, .idx = idx };
    xQueueSend(s_req, &r, 0);
    return true;
}

void wc_recorder_stop_play(void) {
    rec_req_t r = { .cmd = RC_STOPPLAY };
    xQueueSend(s_req, &r, 0);
}
