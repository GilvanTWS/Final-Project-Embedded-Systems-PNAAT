#include "i2c_config.h"

#include <stdbool.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char TAG[] = "i2c_config";

void enable_vext_rail(void)
{
    static bool s_vext_enabled = false;
    if (s_vext_enabled) {
        return;
    }

    gpio_config_t vext_conf = {
        .pin_bit_mask = 1ULL << GPIO_NUM_36,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&vext_conf);
    gpio_set_level(GPIO_NUM_36, 0);
    vTaskDelay(pdMS_TO_TICKS(50));

    s_vext_enabled = true;
}

void initialize_i2c_ex(i2c_port_t port, i2c_master_bus_handle_t *i2c_bus,
                       gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    ESP_LOGI(TAG, "Initialize I2C bus (port %d)", port);

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = port,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, i2c_bus));

    ESP_LOGI(TAG, "Initialize I2C bus done");
}
