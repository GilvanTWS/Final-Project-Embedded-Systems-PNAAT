#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_config.h"
#include "oled_setup.h"
#include "imu_config.h"
#include "wifi_ntp.h"
#include "mqtt_link.h"

static void rede_task(void *arg) {
    if (wifi_ntp_aguardar_ip(30000)) {
        mqtt_link_start();
    } else {
        printf("[rede] Wi-Fi nao conectou em 30 s: MQTT desativado\n");
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    wifi_ntp_start();
    xTaskCreate(rede_task, "rede", 4096, NULL, 3, NULL);

    enable_vext_rail();

    i2c_master_bus_handle_t oled_bus;
    initialize_i2c_ex(I2C_OLED_PORT, &oled_bus, PIN_NUM_SDA_OLED, PIN_NUM_SCL_OLED);
    configure_oled_screen(oled_bus);

    imu_config_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
