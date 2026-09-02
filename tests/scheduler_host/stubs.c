#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "nvs.h"
#include "wc_net.h"
#include "wc_audio.h"
#include "sim.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(_WIN32)
/* MinGW does not ship gmtime_r; glibc (Linux) provides it natively.
   Use UTC (gmtime) semantics — the scheduler derives local time by adding the
   configured TZ offset itself. */
struct tm *gmtime_r(const time_t *t, struct tm *r) {
    return gmtime_s(r, t) == 0 ? r : NULL;
}
#endif

static void host_sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ================= simulated clock ================= */
static volatile int64_t s_mono_us = 0;
static volatile int64_t s_unix    = 0;
static volatile int     s_synced  = 0;

void sim_set_synced(int s)        { s_synced = s; }
void sim_set_unix(int64_t u)      { s_unix = u; }
void sim_set_mono_ms(uint32_t ms) { s_mono_us = (int64_t)ms * 1000; }
void sim_advance_ms(uint32_t ms)  { s_mono_us += (int64_t)ms * 1000; }

int64_t esp_timer_get_time(void)  { return s_mono_us; }

/* ================= network stubs ================= */
int64_t wc_net_time_unix(void) { return s_unix; }
bool    wc_net_time_synced(void) { return s_synced != 0; }
void    wc_net_init(void) {}
void    wc_net_start_provisioning(void) {}
wc_net_state_t wc_net_state(void) { return WC_NET_CONNECTED; }
void    wc_weather_request(const char *c, const char *k) { (void)c; (void)k; }
const wc_weather_result_t *wc_weather_result(void) { return NULL; }

/* ================= audio stubs ================= */
static volatile int s_last_sound = -1;
void wc_audio_init(void) {}
void wc_audio_play(wc_sound_t s) { s_last_sound = (int)s; }
void wc_audio_stop(void) {}
void sim_get_sound(wc_sound_t *out) { *out = (wc_sound_t)s_last_sound; }
void sim_clear_sound(void) { s_last_sound = -1; }

/* ================= queue ================= */
typedef struct { void *buf; size_t esize, cap, n, head, tail; pthread_mutex_t mtx; } stubq_t;

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
    stubq_t *q = (stubq_t *)malloc(sizeof *q);
    q->buf = malloc((size_t)uxItemSize * uxQueueLength);
    q->esize = uxItemSize; q->cap = uxQueueLength; q->n = q->head = q->tail = 0;
    pthread_mutex_init(&q->mtx, NULL);
    return q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItem, uint32_t xTicksToWait) {
    stubq_t *q = (stubq_t *)xQueue; (void)xTicksToWait;
    pthread_mutex_lock(&q->mtx);
    int ret;
    if (q->n >= q->cap) { ret = pdFALSE; }
    else {
        memcpy((char *)q->buf + q->tail * q->esize, pvItem, q->esize);
        q->tail = (q->tail + 1) % q->cap; q->n++;
        ret = pdTRUE;
    }
    pthread_mutex_unlock(&q->mtx);
    return ret;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait) {
    stubq_t *q = (stubq_t *)xQueue;
    uint32_t elapsed = 0;
    for (;;) {
        pthread_mutex_lock(&q->mtx);
        if (q->n > 0) {
            memcpy(pvBuffer, (char *)q->buf + q->head * q->esize, q->esize);
            q->head = (q->head + 1) % q->cap; q->n--;
            pthread_mutex_unlock(&q->mtx);
            return pdTRUE;
        }
        pthread_mutex_unlock(&q->mtx);
        if (xTicksToWait == 0) return pdFALSE;
        if (xTicksToWait != portMAX_DELAY) {
            if (elapsed >= xTicksToWait) return pdFALSE;
            uint32_t d = (xTicksToWait - elapsed) < 10 ? (xTicksToWait - elapsed) : 10;
            host_sleep_ms(2); elapsed += d;
        } else {
            host_sleep_ms(2);
        }
    }
}

/* ================= task ================= */
static void *tramp(void *p) { TaskFunction_t f = (TaskFunction_t)p; f(NULL); return NULL; }

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *pcName, uint32_t usStackDepth,
                       void *pvParameters, uint32_t uxPriority, TaskHandle_t *pxCreatedTask) {
    (void)pcName; (void)usStackDepth; (void)uxPriority; (void)pvParameters;
    pthread_t th;
    if (pthread_create(&th, NULL, tramp, (void *)pxTaskCode) != 0) return pdFAIL;
    pthread_detach(th);
    if (pxCreatedTask) *pxCreatedTask = (TaskHandle_t)(intptr_t)th;
    return pdPASS;
}

void vTaskDelay(uint32_t xTicksToDelay) { (void)xTicksToDelay; host_sleep_ms(2); }

/* ================= nvs (in-memory single blob) ================= */
static unsigned char s_nvs_buf[1024];
static size_t        s_nvs_len = 0;

esp_err_t nvs_open(const char *name, uint32_t open_mode, nvs_handle_t *out_handle) {
    (void)name; (void)open_mode; *out_handle = 1; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out_value, size_t *length) {
    (void)h; (void)key;
    if (*length < s_nvs_len) { *length = s_nvs_len; return ESP_ERR_NVS_NOT_FOUND; }
    memcpy(out_value, s_nvs_buf, s_nvs_len);
    *length = s_nvs_len;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t length) {
    (void)h; (void)key;
    if (length > sizeof s_nvs_buf) return -1;
    memcpy(s_nvs_buf, value, length);
    s_nvs_len = length;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }