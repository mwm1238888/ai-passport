#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WC_NET_OFFLINE = 0,
    WC_NET_CONNECTING,
    WC_NET_CONNECTED,
    WC_NET_FAILED,
} wc_net_state_t;

typedef struct {
    int  temp;
    char text[32];
} wc_weather_t;

typedef struct {
    int  temp;
    char text[32];
    bool available;        /* false until the first successful fetch */
} wc_weather_result_t;

void wc_net_init(void);                  /* wifi + SNTP + (SoftAP if no creds) */
int64_t wc_net_time_unix(void);         /* UTC seconds, 0 if unknown */
bool   wc_net_time_synced(void);
wc_net_state_t wc_net_state(void);
void   wc_net_start_provisioning(void); /* open SoftAP config portal */

/* Asynchronous HTTPS fetch on a dedicated task (never block the caller's stack).
 * Posts WC_EV_WEATHER_OK / WC_EV_WEATHER_FAIL when done. */
void wc_weather_request(const char *city, const char *key);

/* Latest fetched result; pointer stays valid for the lifetime of the app. */
const wc_weather_result_t *wc_weather_result(void);

#ifdef __cplusplus
}
#endif
