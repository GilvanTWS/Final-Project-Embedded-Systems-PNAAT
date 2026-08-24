#ifndef __I2C_CONFIG_H__
#define __I2C_CONFIG_H__

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_NUM_SDA         GPIO_NUM_6
#define PIN_NUM_SCL         GPIO_NUM_7
#define IMU_I2C_PORT        I2C_NUM_0

#define PIN_NUM_SDA_OLED    GPIO_NUM_17
#define PIN_NUM_SCL_OLED    GPIO_NUM_18
#define I2C_OLED_PORT       I2C_NUM_1
#define PIN_NUM_RST_OLED    GPIO_NUM_21

void enable_vext_rail(void);

void initialize_i2c_ex(i2c_port_t port, i2c_master_bus_handle_t *i2c_bus,
                       gpio_num_t sda_gpio, gpio_num_t scl_gpio);

#ifdef __cplusplus
}
#endif

#endif
