#ifndef __WIFI_NTP_H__
#define __WIFI_NTP_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_ntp_start(void);

bool wifi_ntp_aguardar_ip(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
