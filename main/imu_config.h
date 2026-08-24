#ifndef __IMU_CONFIG_H__
#define __IMU_CONFIG_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void imu_config_init(void);

void pnaat_obter_estado(bool *led_on, int *brilho_pct);

void pnaat_aplicar_comando(const char *gesto);

#ifdef __cplusplus
}
#endif

#endif
