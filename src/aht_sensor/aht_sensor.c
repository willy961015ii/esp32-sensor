// aht20_sensor.c
#include "aht_sensor.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>

// ---- I2C 硬體腳位與參數(依你實際接線調整)----
#define I2C_MASTER_SDA_IO   GPIO_NUM_21
#define I2C_MASTER_SCL_IO   GPIO_NUM_22
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

#define AHT20_I2C_ADDR      0x38

#define POLL_INTERVAL_MS    5000

// ---- 內部狀態,存最新一次讀到的值 ----
static float latest_temperature = 0.0f;
static float latest_humidity = 0.0f;
static SemaphoreHandle_t sensor_mutex = NULL;   // 保護 I2C 量測流程與最新值

// ---- 內部函式宣告 ----
static void i2c_master_setup(void);
static void aht20_trigger_measurement(void);
static bool aht20_read_raw(uint8_t *data, size_t len);
static void sensor_task(void *pvParameters);

void aht20_sensor_init(void)
{
    sensor_mutex = xSemaphoreCreateMutex();
    i2c_master_setup();

    // AHT20 開機後建議送一次初始化指令(依 datasheet:0xBE, 0x08, 0x00)
    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
    i2c_master_write_to_device(I2C_MASTER_NUM, AHT20_I2C_ADDR,
                                init_cmd, sizeof(init_cmd),
                                pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(10));

    xTaskCreate(sensor_task, "aht20_sensor_task", 4096, NULL, 5, NULL);
}

void aht20_get_latest(float *temperature, float *humidity)
{
    xSemaphoreTake(sensor_mutex, portMAX_DELAY);
    *temperature = latest_temperature;
    *humidity = latest_humidity;
    xSemaphoreGive(sensor_mutex);
}

bool aht20_measure_now(float *temperature, float *humidity)
{
    uint8_t raw[6];
    bool ok;

    xSemaphoreTake(sensor_mutex, portMAX_DELAY);

    aht20_trigger_measurement();
    vTaskDelay(pdMS_TO_TICKS(80));   // datasheet 建議量測需要等待約80ms

    ok = aht20_read_raw(raw, sizeof(raw));
    if (ok) {
        // raw[0] 是狀態位元組,bit7=1 代表still busy,這裡先簡化不特別檢查
        uint32_t raw_humidity = ((uint32_t)raw[1] << 12) |
                                 ((uint32_t)raw[2] << 4) |
                                 (raw[3] >> 4);
        uint32_t raw_temp = (((uint32_t)raw[3] & 0x0F) << 16) |
                             ((uint32_t)raw[4] << 8) |
                             raw[5];

        latest_humidity = ((float)raw_humidity / 1048576.0f) * 100.0f;
        latest_temperature = ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f;
    }
    *temperature = latest_temperature;
    *humidity = latest_humidity;

    xSemaphoreGive(sensor_mutex);
    return ok;
}

// ---- I2C 匯流排設定(標準 legacy driver/i2c.h 流程)----
static void i2c_master_setup(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// 送出「開始量測」指令(依 datasheet:0xAC, 0x33, 0x00)
static void aht20_trigger_measurement(void)
{
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00};
    i2c_master_write_to_device(I2C_MASTER_NUM, AHT20_I2C_ADDR,
                                trigger_cmd, sizeof(trigger_cmd),
                                pdMS_TO_TICKS(100));
}

// 讀取 6 bytes 原始資料(1 byte 狀態 + 5 bytes 溫濕度資料)
static bool aht20_read_raw(uint8_t *data, size_t len)
{
    esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, AHT20_I2C_ADDR,
                                                 data, len,
                                                 pdMS_TO_TICKS(100));
    return (err == ESP_OK);
}

static void sensor_task(void *pvParameters)
{
    float temp, humidity;

    while (1) {
        if (aht20_measure_now(&temp, &humidity)) {
            printf("[aht20_sensor] temp=%.1fC humidity=%.1f%%\n",
                   temp, humidity);
        } else {
            printf("[aht20_sensor] read failed\n");
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}