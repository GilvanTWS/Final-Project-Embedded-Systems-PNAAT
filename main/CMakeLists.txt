cmake_minimum_required(VERSION 3.5)
file(GLOB SRCS_C "*.c")
file(GLOB SRCS_CPP "*.cpp")
idf_component_register(SRCS
                    "${SRCS_C}"
                    "${SRCS_CPP}"
                    INCLUDE_DIRS
                    "."
                     REQUIRES i2c_config oled_setup edge_impulse bno085 lvgl esp_lvgl_port driver
                              nvs_flash esp_wifi esp_netif esp_event mqtt)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=maybe-uninitialized
    -Wno-maybe-uninitialized
    -Wno-error=uninitialized)
