#ifndef GPIO_HANDLER_H
#define GPIO_HANDLER_H

/*GPIO 功能
    按鍵定義
    LED定義
    按鍵邏輯
    LED邏輯
*/
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 按鍵事件類型
typedef enum {
    BUTTON_EVENT_PRESSED = 0, //按下
} button_event_t;

//Queue把手
extern QueueHandle_t button_event_queue;
void gpio_handler_init(void);
void gpio_handler_led_set(bool on);
// LED 亮 duration_ms 後自動熄滅,不會阻塞呼叫端;連續呼叫會重新計時
void gpio_handler_led_blink(uint32_t duration_ms);


#endif