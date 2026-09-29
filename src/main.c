// main.c
#include "network_manager.h"
#include "mqtt_config.h"
#include "web_server.h"
#include "aht_sensor.h"
#include "gpio_handler.h"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>

#define LED_BLINK_MS        200

// 遠端指令 queue:MQTT 回呼只負責丟請求進來,實際量測在 cmd_task 做,避免卡住 MQTT Task
static QueueHandle_t measure_request_queue = NULL;

// 把溫濕度組成 JSON 並發佈;只要發佈成功就閃一下 LED
// 定期發佈、按鍵、遠端指令都共用這個函式
static void publish_sensor_data(float temp, float humidity)
{
    char json_buf[64];
    snprintf(json_buf, sizeof(json_buf),
             "{\"temp\":%.1f,\"humidity\":%.1f}", temp, humidity);

    if (network_manager_publish(MQTT_TOPIC_SENSOR, json_buf)) {
        printf("[main] published: %s\n", json_buf);
        gpio_handler_led_blink(LED_BLINK_MS);
    }
}

// 發佈目前存好的最新值(不重新量測)
static void publish_latest(void)
{
    float temp, humidity;
    aht20_get_latest(&temp, &humidity);
    publish_sensor_data(temp, humidity);
}

// 立即量測一次再發佈,按鍵與遠端指令共用
static void measure_and_publish(void)
{
    float temp, humidity;
    if (aht20_measure_now(&temp, &humidity)) {
        publish_sensor_data(temp, humidity);
    } else {
        printf("[main] measure failed, nothing published\n");
    }
}

// 定期發佈的背景 Task
static void publish_task(void *pvParameters)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS));
        if (network_manager_is_ready()) {
            publish_latest();
        }
    }
}

// 處理按鍵事件的背景 Task:按下就立即量測並發佈一次
static void button_event_task(void *pvParameters)
{
    button_event_t evt;
    while (1) {
        if (xQueueReceive(button_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt == BUTTON_EVENT_PRESSED) {
                printf("[main] button pressed, measuring\n");
                measure_and_publish();
            }
        }
    }
}

// MQTT 收到訊息的回呼(在 MQTT 內部 Task 執行,只做判斷跟丟 queue)
static void mqtt_message_handler(const char *topic, int topic_len,
                                 const char *data, int data_len)
{
    bool is_cmd_topic = (topic_len == (int)strlen(MQTT_TOPIC_CMD)) &&
                        (memcmp(topic, MQTT_TOPIC_CMD, topic_len) == 0);
    bool is_measure = (data_len == (int)strlen(MQTT_CMD_MEASURE)) &&
                      (memcmp(data, MQTT_CMD_MEASURE, data_len) == 0);

    if (is_cmd_topic && is_measure) {
        uint8_t req = 1;
        if (xQueueSend(measure_request_queue, &req, 0) != pdTRUE) {
            printf("[main] measure request dropped, previous one still pending\n");
        }
    } else {
        printf("[main] unknown message: %.*s -> %.*s\n", topic_len, topic, data_len, data);
    }
}

// 處理遠端量測指令:立即量測一次再發佈
static void cmd_task(void *pvParameters)
{
    uint8_t req;

    while (1) {
        if (xQueueReceive(measure_request_queue, &req, portMAX_DELAY) == pdTRUE) {
            printf("[main] remote measure requested\n");
            measure_and_publish();
        }
    }
}

void app_main(void)
{
    // WiFi 驅動需要 NVS 存放校正資料,必須在 network_manager_init() 之前初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 回呼可能在連上 MQTT 後隨時觸發,queue 要在網路啟動前建好
    measure_request_queue = xQueueCreate(1, sizeof(uint8_t));
    network_manager_set_message_handler(mqtt_message_handler);

    // 感測器(mutex)與 GPIO(LED timer)先初始化,
    // 確保網路一連上、有發佈或 HTTP 請求進來時它們已經可用
    aht20_sensor_init();
    gpio_handler_init();

    // network_manager 負責建立 esp_event 事件迴圈,web_server 的事件註冊依賴它先跑過
    network_manager_init();
    web_server_init();

    xTaskCreate(publish_task, "publish_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_event_task, "button_event_task", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_task, "cmd_task", 4096, NULL, 5, NULL);
}
