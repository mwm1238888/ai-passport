#include "wc_net.h"
#include "wc_state.h"
#include "wc_config.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#define WC_WIFI_SSID_1 ""
#define WC_WIFI_PASS_1 ""
#define WC_WIFI_SSID_2 ""
#define WC_WIFI_PASS_2 ""
#endif

static const char *TAG = "wc_net";
#define WIFI_NS  "wcomp"
#define WIFI_KEY "wifi"

static volatile bool      s_synced = false;
static volatile wc_net_state_t s_state = WC_NET_OFFLINE;
static bool               s_sntp_started = false;
static esp_netif_t       *s_sta_netif = NULL;
static bool               s_wifi_ready = false;
static char               s_ap_ssid[16];

/* ---- IP / wifi event handler ---- */
static void sntp_start(void);  /* forward */
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_state = WC_NET_CONNECTED;
        if (!s_sntp_started) { sntp_start(); s_sntp_started = true; }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_state = WC_NET_FAILED;
    }
}

/* ---- credentials: NVS user > build-time ---- */
static bool load_creds(char *ssid, size_t sc, char *pass, size_t pc) {
    nvs_handle_t h;
    bool got = false;
    if (nvs_open(WIFI_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t need = sc;
        if (nvs_get_str(h, "ssid", ssid, &need) == ESP_OK &&
            nvs_get_str(h, "pass", pass, &pc) == ESP_OK && ssid[0]) got = true;
        nvs_close(h);
    }
    if (!got && strlen(WC_WIFI_SSID_1)) {
        strncpy(ssid, WC_WIFI_SSID_1, sc - 1); ssid[sc - 1] = 0;
        strncpy(pass, WC_WIFI_PASS_1, pc - 1); pass[pc - 1] = 0;
        got = true;
    }
    return got;
}

bool wc_net_save_creds(const char *ssid, const char *pass) {
    nvs_handle_t h;
    if (nvs_open(WIFI_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "pass", pass);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

/* ---- SNTP ---- */
static void sntp_cb(struct timeval *tv) {
    s_synced = true;
    s_state = WC_NET_CONNECTED;
    wc_event_t ev = { .id = WC_EV_TIME_SYNCED };
    wc_state_post(ev);
}

static void sntp_start(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, WC_NTP_SERVER_1);
    esp_sntp_setservername(1, WC_NTP_SERVER_2);
    esp_sntp_set_sync_interval(60 * 1000);
    esp_sntp_init();
    setenv("TZ", "CST-8", 1);
    tzset();
}

int64_t wc_net_time_unix(void) {
    if (!s_synced) return 0;
    return (int64_t)time(NULL);   /* UTC seconds */
}
bool wc_net_time_synced(void) { return s_synced; }
wc_net_state_t wc_net_state(void) { return s_state; }

/* ---- SoftAP provisioning (minimal) ---- */
static esp_err_t portal_get(httpd_req_t *req) {
    const char *html =
      "<html><body><h2>Work Companion WiFi</h2>"
      "<form method=post action=/save>"
      "SSID:<input name=ssid><br>PASS:<input name=pass type=password><br>"
      "<button>SAVE</button></form></body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
static esp_err_t portal_save(httpd_req_t *req) {
    char body[128] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len < 0) len = 0; body[len] = 0;
    char ssid[40] = {0}, pass[64] = {0};
    /* tiny form decoder */
    char *p = strstr(body, "ssid="); if (p) sscanf(p, "ssid=%39[^&]", ssid);
    p = strstr(body, "pass="); if (p) sscanf(p, "pass=%63[^&]", pass);
    if (ssid[0]) {
        wc_net_save_creds(ssid, pass);
        httpd_resp_sendstr(req, "OK. Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    httpd_resp_sendstr(req, "missing ssid");
    return ESP_OK;
}

static void start_ap_provision(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s-%02X%02X",
             WC_AP_SSID_PREFIX, mac[4], mac[5]);
    wifi_config_t ap = {0};
    strcpy((char *)ap.ap.ssid, s_ap_ssid);
    ap.ap.ssid_len = strlen(s_ap_ssid);
    ap.ap.channel = 6;
    ap.ap.max_connection = 1;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();

    httpd_handle_t s = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 4;
    if (httpd_start(&s, &cfg) == ESP_OK) {
        httpd_uri_t g = { .uri="/", .method=HTTP_GET, .handler=portal_get };
        httpd_uri_t sv = { .uri="/save", .method=HTTP_POST, .handler=portal_save };
        httpd_register_uri_handler(s, &g);
        httpd_register_uri_handler(s, &sv);
    }
    ESP_LOGI(TAG, "provisioning AP: %s  open http://192.168.4.1", s_ap_ssid);
}

/* ---- connect + sync task ---- */
static void net_task(void *arg) {
    char ssid[40], pass[64];
    if (!load_creds(ssid, sizeof(ssid), pass, sizeof(pass))) {
        start_ap_provision();
        for (;;) vTaskDelay(portMAX_DELAY);   /* wait for reboot after save */
    }

    wifi_config_t cfg = {0};
    strcpy((char *)cfg.sta.ssid, ssid);
    strcpy((char *)cfg.sta.password, pass);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    s_state = WC_NET_CONNECTING;
    esp_wifi_connect();
    int waited = 0;
    while (s_state == WC_NET_CONNECTING && waited < WC_WIFI_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(200));
        waited += 200;
    }
    /* periodic re-connect / re-sync */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WC_RESYNC_MS));
        if (s_state != WC_NET_CONNECTED) {
            s_state = WC_NET_CONNECTING;
            esp_wifi_connect();
        }
    }
}

void wc_net_init(void) {
    s_sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_ps(WIFI_PS_MIN_MODE);
    s_wifi_ready = true;
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    esp_sntp_set_time_sync_notification_cb(sntp_cb);
    xTaskCreate(net_task, "wc_net", 6144, NULL, 4, NULL);
}

void wc_net_start_provisioning(void) {
    if (!s_wifi_ready) return;
    start_ap_provision();
}

/* ---- weather (QWeather) ---- */
bool wc_weather_fetch(const char *city, const char *key, wc_weather_t *out) {
    if (!city || !city[0] || !key || !key[0] || !out) return false;
    char url[160];
    snprintf(url, sizeof(url), "https://%s%s?location=%s&key=%s",
             WC_WEATHER_HOST, WC_WEATHER_PATH, city, key);
    esp_http_client_config_t cfg = {
        .url = url, .timeout_ms = 8000, .buffer_size = 2048,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_err_t e = esp_http_client_perform(c);
    bool ok = false;
    int code = esp_http_client_get_status_code(c);
    if (e == ESP_OK && code == 200) {
        char buf[2048];
        int n = esp_http_client_read(c, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            cJSON *root = cJSON_Parse(buf);
            if (root) {
                cJSON *daily = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "daily"), 0);
                if (daily) {
                    cJSON *tmax = cJSON_GetObjectItem(daily, "tempMax");
                    cJSON *text = cJSON_GetObjectItem(daily, "textDay");
                    if (tmax) out->temp = tmax->valueint;
                    if (text && text->valuestring) {
                        strncpy(out->text, text->valuestring, sizeof(out->text) - 1);
                        out->text[sizeof(out->text) - 1] = 0;
                    }
                    ok = true;
                }
                cJSON_Delete(root);
            }
        }
    }
    esp_http_client_cleanup(c);
    return ok;
}
