#include "oled_setup.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define EXAMPLE_PIN_NUM_RST 21
#define EXAMPLE_I2C_HW_ADDR 0x3C

#define EXAMPLE_LCD_H_RES 128
#define EXAMPLE_LCD_V_RES 64

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS   8
#define EXAMPLE_LCD_PARAM_BITS 8

static const char TAG[] = "screen";

lv_disp_t *local_disp = NULL;

esp_err_t configure_oled_screen(i2c_master_bus_handle_t bus_handle) {
  ESP_LOGI(TAG, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = EXAMPLE_I2C_HW_ADDR,
      .scl_speed_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
      .control_phase_bytes = 1,             // According to SSD1306 datasheet
      .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS, // According to SSD1306 datasheet
      .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
      .dc_bit_offset = 6,                   // According to SSD1306 datasheet
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .bits_per_pixel = 1,
      .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
  };
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = EXAMPLE_LCD_V_RES,
  };
  panel_config.vendor_config = &ssd1306_config;
#endif
  ESP_LOGI(TAG, "Install SSD1306 panel driver");
  ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  ESP_LOGI(TAG, "Initialize LVGL");
  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

  const lvgl_port_display_cfg_t disp_cfg = {
      .io_handle = io_handle,
      .panel_handle = panel_handle,
      .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
      .double_buffer = true,
      .hres = EXAMPLE_LCD_H_RES,
      .vres = EXAMPLE_LCD_V_RES,
      .monochrome = true,
#if LVGL_VERSION_MAJOR >= 9
      .color_format = LV_COLOR_FORMAT_RGB565,
#endif
      .rotation =
          {
              .swap_xy = false,
              .mirror_x = false,
              .mirror_y = false,
          },
      .flags = {
#if LVGL_VERSION_MAJOR >= 9
          .swap_bytes = false,
#endif
          .sw_rotate = false,
      }};
  local_disp = lvgl_port_add_disp(&disp_cfg);

  if (lvgl_port_lock(0)) {
    lv_disp_set_rotation(local_disp, LV_DISPLAY_ROTATION_0);
    lvgl_port_unlock();
  }

  return ESP_OK;
}
