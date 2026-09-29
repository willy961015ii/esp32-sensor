#include "gpio_handler.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>

//接腳位更改
#define BUTTON_GPIO GPIO_NUM_4
#define LED_GPIO    GPIO_NUM_2

#define DELAY_TIME   20 //按鍵回彈
#define POLL_TIME    10
#define BUTTON_QUEUE_LENGTH 1  

QueueHandle_t button_event_queue = NULL;
bool last_pressed = false;
static TimerHandle_t led_off_timer = NULL;

static inline bool button_is_pressed(void) {
    return gpio_get_level(BUTTON_GPIO) == 0;
}

//按鍵按下邏輯 推送一個queue
static void button_task(void *pvParameters)
{
    while (1) {
        bool current = button_is_pressed();

        if (!last_pressed && current) {
            vTaskDelay(pdMS_TO_TICKS(DELAY_TIME));

            if (button_is_pressed()) {
                button_event_t evt = BUTTON_EVENT_PRESSED;
                
                if (xQueueSend(button_event_queue, &evt, 0) != pdTRUE) {
                    printf("[gpio_handler] queue full, event dropped\n");
                }
            }
        }
        last_pressed = current;
        vTaskDelay(pdMS_TO_TICKS(POLL_TIME));
    }
}
// LED 計時到期,熄滅
static void led_off_callback(TimerHandle_t timer)
{
    gpio_set_level(LED_GPIO, 0);
}

//gpio初始設定
void gpio_handler_init(void)
{
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_conf);

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_GPIO, 0);   // 開機預設
    led_off_timer = xTimerCreate("led_off", pdMS_TO_TICKS(100), pdFALSE, NULL, led_off_callback);

    button_event_queue = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(button_event_t));

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
}

void gpio_handler_led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void gpio_handler_led_blink(uint32_t duration_ms)
{
    gpio_set_level(LED_GPIO, 1);
    // 改週期會一併(重新)啟動 timer,連續觸發時亮燈時間從最後一次起算
    xTimerChangePeriod(led_off_timer, pdMS_TO_TICKS(duration_ms), 0);
}
