// aht20_sensor.h
#ifndef AHT20_SENSOR_H
#define AHT20_SENSOR_H

#include <stdbool.h>

// 初始化 I2C 匯流排,並啟動背景讀值 Task(週期性讀取,更新最新值)
void aht20_sensor_init(void);

// 取得最新一次讀到的溫濕度(不會主動觸發新的量測,只是拿目前存好的值)
void aht20_get_latest(float *temperature, float *humidity);

// 立即量測一次(會阻塞約 80ms),成功時更新最新值並回傳 true
// 與背景 Task 共用 I2C,內部有 mutex 保護,可從任何 Task 呼叫
bool aht20_measure_now(float *temperature, float *humidity);

#endif // AHT20_SENSOR_H