// network_manager.h
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdbool.h>

// 初始化並啟動 WiFi 連線(內部會在連上後自動啟動 MQTT)
void network_manager_init(void);

// MQTT 是否已連上 broker、可以發布訊息
bool network_manager_is_ready(void);

// 發布一則 MQTT 訊息,qos 固定用 1,retain 固定用 false
// 回傳 true 代表成功送出(若尚未連線會直接回傳 false,不會卡住)
bool network_manager_publish(const char *topic, const char *data);

// 收到訂閱 topic 訊息時的回呼,topic 與 data 都不是 '\0' 結尾,要搭配長度使用
// 回呼在 MQTT 內部 Task 執行,不要在裡面做耗時的事,應該丟到自己的 queue 再處理
typedef void (*network_message_handler_t)(const char *topic, int topic_len,
                                          const char *data, int data_len);

// 設定訊息回呼,在 network_manager_init() 之前或之後呼叫皆可
void network_manager_set_message_handler(network_message_handler_t handler);


#endif // NETWORK_MANAGER_H