// web_server.c
#include "web_server.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include <stdio.h>
#include <string.h>

// 假設 aht20_sensor 模組提供這個函式,回傳最新讀值
// 你之後串接時,把函式名稱換成你實際的 aht20_sensor.h 介面即可
#include "aht_sensor.h"

static httpd_handle_t server = NULL;

// ---- 內部函式宣告 ----
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);
static void start_httpd(void);
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t sensor_get_handler(httpd_req_t *req);

void web_server_init(void)
{
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        printf("[web_server] network ready, starting HTTP server\n");
        start_httpd();
    }
}

static void start_httpd(void)
{
    if (server != NULL) {
        // 已經啟動過(例如斷線重連的情況),不重複啟動
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) != ESP_OK) {
        printf("[web_server] failed to start httpd\n");
        return;
    }

    httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t sensor_uri = {
        .uri      = "/api/sensor",
        .method   = HTTP_GET,
        .handler  = sensor_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sensor_uri);

    printf("[web_server] httpd started\n");
}

// 回傳最簡單的一頁 HTML,之後你可以再擴充成真正的儀表板頁面
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h1>ESP32 Sensor Dashboard</h1>"
        "<p>Temp/Humidity: <a href=\"/api/sensor\">/api/sensor</a></p>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// 回傳 JSON 格式的最新感測值
static esp_err_t sensor_get_handler(httpd_req_t *req)
{
    float temp, humidity;
    aht20_get_latest(&temp, &humidity);   // 呼叫感測器模組的 getter,不是自己重新去讀

    char json_buf[64];
    snprintf(json_buf, sizeof(json_buf),
             "{\"temp\":%.1f,\"humidity\":%.1f}", temp, humidity);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buf, strlen(json_buf));
    return ESP_OK;
}