#include <string.h>
#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt_link.h"
#include "imu_config.h"

static const char TAG[] = "mqtt_link";

#define TOPICO_STATUS CONFIG_PNAAT_MQTT_URI_BASE "/status"
#define TOPICO_COMANDO CONFIG_PNAAT_MQTT_URI_BASE "/cmd"

static esp_mqtt_client_handle_t s_client = NULL;
static QueueHandle_t s_fila = NULL;
static volatile bool s_conectado = false;

void mqtt_link_enviar_status(const char *fonte, bool led_on, int brilho_pct) {
    if (!s_fila) {
        return;
    }
    pnaat_ev_t ev;
    strlcpy(ev.fonte, fonte, sizeof(ev.fonte));
    ev.led_on = led_on;
    ev.brilho_pct = brilho_pct;
    xQueueSend(s_fila, &ev, 0);
}

static void publicar_status(const pnaat_ev_t *ev) {
    if (!s_client || !s_conectado) {
        return;
    }
    char hora[9] = "--:--:--";
    time_t agora = time(NULL);
    if (agora > 1600000000) {
        struct tm tm_info;
        localtime_r(&agora, &tm_info);
        strftime(hora, sizeof(hora), "%H:%M:%S", &tm_info);
    }

    char payload[192];
    snprintf(payload, sizeof(payload),
             "{\"fonte\":\"%s\",\"led\":%s,\"brilho\":%d,\"hora\":\"%s\"}",
             ev->fonte, ev->led_on ? "true" : "false", ev->brilho_pct, hora);

    int msg_id = esp_mqtt_client_publish(s_client, TOPICO_STATUS, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado (%d): %s", msg_id, payload);
}

static void aplicar_comando(const char *cmd) {
    const char *fonte = NULL;

    if (strcmp(cmd, "on") == 0) {
        fonte = "remoto:on";
        pnaat_aplicar_comando("direita");
    } else if (strcmp(cmd, "off") == 0) {
        fonte = "remoto:off";
        pnaat_aplicar_comando("esquerda");
    } else if (strcmp(cmd, "up") == 0) {
        fonte = "remoto:up";
        pnaat_aplicar_comando("cima");
    } else if (strcmp(cmd, "down") == 0) {
        fonte = "remoto:down";
        pnaat_aplicar_comando("baixo");
    }

    if (fonte) {
        bool led_on;
        int brilho;
        pnaat_obter_estado(&led_on, &brilho);
        mqtt_link_enviar_status(fonte, led_on, brilho);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado ao broker");
        s_conectado = true;
        esp_mqtt_client_subscribe(s_client, TOPICO_COMANDO, 1);

        bool led_on;
        int brilho;
        pnaat_obter_estado(&led_on, &brilho);
        mqtt_link_enviar_status("online", led_on, brilho);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado do broker");
        s_conectado = false;
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len == (int)strlen(TOPICO_COMANDO) &&
            strncmp(event->topic, TOPICO_COMANDO, event->topic_len) == 0) {
            char cmd[16] = { 0 };
            size_t len = event->data_len < sizeof(cmd) - 1 ? event->data_len : sizeof(cmd) - 1;
            memcpy(cmd, event->data, len);
            ESP_LOGI(TAG, "Comando remoto: %s", cmd);
            aplicar_comando(cmd);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erro MQTT");
        break;

    default:
        break;
    }
}

static void mqtt_publicador_task(void *arg) {
    pnaat_ev_t ev;
    while (1) {
        if (xQueueReceive(s_fila, &ev, portMAX_DELAY) == pdTRUE) {
            publicar_status(&ev);
        }
    }
}

void mqtt_link_start(void) {
    if (s_client) {
        return;
    }
    s_fila = xQueueCreate(8, sizeof(pnaat_ev_t));

    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_PNAAT_MQTT_URI,
        .session.keepalive = 30,
    };
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Falha ao criar cliente MQTT");
        return;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    xTaskCreate(mqtt_publicador_task, "mqtt_pub", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "MQTT iniciado (%s)", CONFIG_PNAAT_MQTT_URI);
}
