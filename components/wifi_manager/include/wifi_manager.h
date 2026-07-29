#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_ap(const char *ssid, const char *password);
esp_err_t wifi_manager_start_sta(const char *ssid, const char *password);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_ip_str(char *ip_str);

#ifdef __cplusplus
}
#endif