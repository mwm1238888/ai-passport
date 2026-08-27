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

void wc_net_init(void);                  /* wifi + SNTP + (SoftAP if no creds) */
int64_t wc_net_time_unix(void);         /* UTC seconds, 0 if unknown */
bool   wc_net_time_synced(void);
wc_net_state_t wc_net_state(void);
void   wc_net_start_provisioning(void); /* open SoftAP config portal */

bool wc_weather_fetch(const char *city, const char *key, wc_weather_t *out);

#ifdef __cplusplus
}
#endif
