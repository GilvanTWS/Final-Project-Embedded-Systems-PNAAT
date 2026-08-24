#include <math.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "bno085.h"
}

#include "esp_lvgl_port.h"
#include "lvgl.h"
#include <time.h>

#include "i2c_config.h"
#include "oled_setup.h"
#include "mqtt_link.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static float inference_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static int buffer_index = 0;

static volatile uint32_t s_reports = 0;
static volatile uint32_t s_windows = 0;
static volatile uint32_t s_int_edges = 0;

static void IRAM_ATTR int_edge_isr(void *arg) {
    s_int_edges = s_int_edges + 1;
}

#define LED_GPIO        GPIO_NUM_35
#define LEDC_FREQ_HZ    5000
#define BRILHO_PASSO    20

static bool s_led_on = false;
static int  s_brilho_pct = 100;

static lv_obj_t *lbl_hora   = NULL;
static lv_obj_t *lbl_gesto  = NULL;
static lv_obj_t *lbl_led    = NULL;
static lv_obj_t *lbl_brilho = NULL;

static void led_aplicar(void) {
    uint32_t duty = s_led_on ? ((uint32_t)s_brilho_pct * 1023) / 100 : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void oled_ui_criar(void) {
    if (!local_disp || !lvgl_port_lock(0)) {
        return;
    }

    lbl_hora = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lbl_hora, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_hora, LV_ALIGN_TOP_LEFT, 4, 2);
    lv_label_set_text(lbl_hora, "--:--:--");

    lbl_gesto = lv_label_create(lv_scr_act());
    lv_obj_align(lbl_gesto, LV_ALIGN_TOP_LEFT, 4, 18);
    lv_label_set_text(lbl_gesto, "Gesto: -");

    lbl_led = lv_label_create(lv_scr_act());
    lv_obj_align(lbl_led, LV_ALIGN_TOP_LEFT, 4, 35);
    lv_label_set_text(lbl_led, "LED: OFF");

    lbl_brilho = lv_label_create(lv_scr_act());
    lv_obj_align(lbl_brilho, LV_ALIGN_TOP_LEFT, 4, 52);
    lv_label_set_text(lbl_brilho, "Brilho: 100%");

    lvgl_port_unlock();
}

static void oled_ui_atualizar(const char *gesto) {
    if (!local_disp || !lvgl_port_lock(0)) {
        return;
    }
    lv_label_set_text_fmt(lbl_gesto, "Gesto: %s", gesto);
    lv_label_set_text(lbl_led, s_led_on ? "LED: ON" : "LED: OFF");
    lv_label_set_text_fmt(lbl_brilho, "Brilho: %d%%", s_brilho_pct);
    lvgl_port_unlock();
}

static bool aplicar_acao_led(const char *gesto) {
    bool mudou = false;

    if (strcmp(gesto, "direita") == 0) {
        mudou = !s_led_on;
        s_led_on = true;
    } else if (strcmp(gesto, "esquerda") == 0) {
        mudou = s_led_on;
        s_led_on = false;
    } else if (strcmp(gesto, "cima") == 0) {
        s_brilho_pct += BRILHO_PASSO;
        if (s_brilho_pct > 100) {
            s_brilho_pct = 100;
        }
        mudou = true;
    } else if (strcmp(gesto, "baixo") == 0) {
        s_brilho_pct -= BRILHO_PASSO;
        if (s_brilho_pct < 0) {
            s_brilho_pct = 0;
        }
        mudou = true;
    }

    led_aplicar();
    return mudou;
}

extern "C" void pnaat_obter_estado(bool *led_on, int *brilho_pct) {
    *led_on = s_led_on;
    *brilho_pct = s_brilho_pct;
}

extern "C" void pnaat_aplicar_comando(const char *gesto) {
    aplicar_acao_led(gesto);
    printf(">> Acao: %s | LED %s | brilho %d%%\n",
           gesto, s_led_on ? "ON" : "OFF", s_brilho_pct);
    oled_ui_atualizar(gesto);
}

static void run_inference(void) {
    s_windows = s_windows + 1;
    signal_t signal;
    ei_impulse_result_t result = { 0 };

    numpy::signal_from_buffer(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        printf("run_classifier falhou (%d)\n", res);
        return;
    }

    const char *melhor = result.classification[0].label;    float conf = result.classification[0].value;
    for (size_t ix = 1; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > conf) {
            melhor = result.classification[ix].label;
            conf = result.classification[ix].value;
        }
    }

    static char ultima_acao[16] = "parado";

    if (strcmp(melhor, "parado") == 0) {
        strcpy(ultima_acao, "parado");
    } else if (conf >= 0.6f && strcmp(melhor, ultima_acao) != 0) {
        strcpy(ultima_acao, melhor);

        aplicar_acao_led(melhor);
        printf(">> Acao: %s | LED %s | brilho %d%%\n",
                melhor, s_led_on ? "ON" : "OFF", s_brilho_pct);

        mqtt_link_enviar_status(melhor, s_led_on, s_brilho_pct);
    }

    printf("--- Gesto Detectado ---\n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        printf("%s: %.2f%%\n", result.classification[ix].label,
               result.classification[ix].value * 100.0f);
    }

    oled_ui_atualizar(melhor);
}

static void sensor_callback(bno085_handle_t handle, const bno085_sensor_value_t *value, void *user_context) {
    if (value->sensor_id != BNO085_SENSOR_ROTATION_VECTOR) {
        return;
    }
    s_reports = s_reports + 1;

    float qr = value->data.rotation_vector.real;
    float qi = value->data.rotation_vector.i;
    float qj = value->data.rotation_vector.j;
    float qk = value->data.rotation_vector.k;

    float roll = atan2f(2.0f * (qr * qi + qj * qk), 1.0f - 2.0f * (qi * qi + qj * qj)) * 57.2958f;
    float pitch = asinf(2.0f * (qr * qj - qk * qi)) * 57.2958f;
    float yaw = atan2f(2.0f * (qr * qk + qi * qj), 1.0f - 2.0f * (qj * qj + qk * qk)) * 57.2958f;

    if (buffer_index + 3 <= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        inference_buffer[buffer_index++] = roll;
        inference_buffer[buffer_index++] = pitch;
        inference_buffer[buffer_index++] = yaw;
    }

    if (buffer_index >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        run_inference();
        buffer_index = 0;
    }
}

static void imu_service_task(void *arg) {
    bno085_handle_t imu = (bno085_handle_t)arg;
    while (1) {
        bno085_service(imu);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void diag_task(void *arg) {
    uint32_t last = 0;
    uint32_t last_edges = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        uint32_t now = s_reports;
        uint32_t edges = s_int_edges;
        printf("[diag] %lu amostras/s | %lu pulsos INT/s | janelas: %lu\n",
               (unsigned long)((now - last) / 5),
               (unsigned long)((edges - last_edges) / 5),
               (unsigned long)s_windows);
        last = now;
        last_edges = edges;
    }
}

static void relogio_task(void *arg) {
    char buf[9];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        time_t agora = time(NULL);
        if (agora < 1600000000) {
            continue;
        }
        struct tm tm_info;
        localtime_r(&agora, &tm_info);
        strftime(buf, sizeof(buf), "%H:%M:%S", &tm_info);

        if (!lbl_hora || !lvgl_port_lock(0)) {
            continue;
        }
        lv_label_set_text(lbl_hora, buf);
        lvgl_port_unlock();
    }
}

extern "C" void imu_config_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_ch = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_ch));

    i2c_master_bus_handle_t bus_handle;
    initialize_i2c_ex(IMU_I2C_PORT, &bus_handle, PIN_NUM_SDA, PIN_NUM_SCL);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x4A,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    bno085_config_t bno_cfg;
    bno085_config_default(&bno_cfg);

    bno085_handle_t imu;
    ESP_ERROR_CHECK(bno085_init(&bno_cfg, dev_handle, GPIO_NUM_5, GPIO_NUM_4, &imu));

    esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_err == ESP_OK || isr_err == ESP_ERR_INVALID_STATE) {
        gpio_set_intr_type(GPIO_NUM_5, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(GPIO_NUM_5, int_edge_isr, NULL);
    } else {
        printf("[diag] nao consegui monitorar INT (%s)\n", esp_err_to_name(isr_err));
    }

    ESP_ERROR_CHECK(bno085_register_sensor_callback(imu, sensor_callback, NULL));

    ESP_ERROR_CHECK(bno085_enable_sensor(imu, BNO085_SENSOR_ROTATION_VECTOR, 20000));

    xTaskCreate(imu_service_task, "imu_service", 8192, imu, 5, NULL);
    xTaskCreate(diag_task, "diag", 4096, NULL, 3, NULL);
    xTaskCreate(relogio_task, "relogio", 4096, NULL, 2, NULL);

    oled_ui_criar();
}
