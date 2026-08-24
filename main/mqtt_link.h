#ifndef __MQTT_LINK_H__
#define __MQTT_LINK_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char fonte[16];
    bool led_on;
    int  brilho_pct;
} pnaat_ev_t;

void mqtt_link_start(void);

void mqtt_link_enviar_status(const char *fonte, bool led_on, int brilho_pct);

#ifdef __cplusplus
}
#endif

#endif
