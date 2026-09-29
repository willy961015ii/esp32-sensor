#ifndef WEB_SERVER_H
#define WEB_SERVER_H

// 初始化並註冊「WiFi連線成功後啟動」的事件監聽
// 呼叫這個函式時,ESP-IDF 的事件系統要已經建立好(network_manager_init() 要先執行過)
void web_server_init(void);

#endif // WEB_SERVER_H