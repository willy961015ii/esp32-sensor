// mqtt_config.h
// MQTT 相關設定集中在這裡,改 broker 或 topic 只需要動這個檔案
#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// ---- Broker 連線 ----
#define MQTT_BROKER_URI         "mqtt://broker.hivemq.com:1883"
// broker 需要驗證時填入,留空字串代表不使用帳號密碼
#define MQTT_USERNAME           ""
#define MQTT_PASSWORD           ""

// ---- 發佈 ----
// 溫濕度資料,payload 格式:{"temp":25.3,"humidity":60.1}
#define MQTT_TOPIC_SENSOR       "esp32/sensor/telemetry"
#define MQTT_PUBLISH_INTERVAL_MS 10000

// ---- 訂閱 ----
// 遠端指令,收到 payload 為 MQTT_CMD_MEASURE 時立即量測並發佈一次
#define MQTT_TOPIC_CMD          "esp32/sensor/cmd"
#define MQTT_CMD_MEASURE        "measure"

#endif // MQTT_CONFIG_H
