#include "network_manager.h"
#include "wifi_credentials.h"
#include "mqtt_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static network_message_handler_t message_handler = NULL;
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);
static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);
static void mqtt_start(void);

//初始化
void network_manager_init(void)
{
    // 初始化底層網路介面與事件迴圈(ESP-IDF WiFi 的標準起手式)
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_cfg);

    // 註冊事件處理:WiFi 相關事件、拿到 IP 的事件
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    printf("[network_manager] WiFi init done, connecting to %s...\n", WIFI_SSID);
}

bool network_manager_is_ready(void)
{
    return mqtt_connected;
}

void network_manager_set_message_handler(network_message_handler_t handler)
{
    message_handler = handler;
}

bool network_manager_publish(const char *topic, const char *data)
{
    if (!mqtt_connected) {
        printf("[network_manager] publish skipped, MQTT not connected\n");
        return false;
    }
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, 0, 1, false);
    return (msg_id >= 0);
}

// ---- WiFi 事件處理 ----
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("[network_manager] WiFi disconnected, retrying...\n");
        mqtt_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        printf("[network_manager] WiFi connected, got IP, starting MQTT\n");
        mqtt_start();
    }
}

// ---- MQTT 啟動與事件處理 ----
static void mqtt_start(void)
{
    if (mqtt_client != NULL) {
        // 已經初始化過(例如 WiFi 斷線重連的情況),不用重建 client
        return;
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    if (MQTT_USERNAME[0] != '\0') {
        mqtt_cfg.credentials.username = MQTT_USERNAME;
        mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    }

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            printf("[network_manager] MQTT connected\n");
            mqtt_connected = true;
            // 每次連上都重新訂閱,斷線重連後才不會漏掉
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_CMD, 1);
            printf("[network_manager] subscribed: %s\n", MQTT_TOPIC_CMD);
            break;
        case MQTT_EVENT_DATA:
            if (message_handler != NULL) {
                message_handler(event->topic, event->topic_len,
                                event->data, event->data_len);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            printf("[network_manager] MQTT disconnected\n");
            mqtt_connected = false;
            break;
        default:
            break;
    }
}