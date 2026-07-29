idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd esp_lcd_touch esp_wifi esp_http_server nvs_flash spiffs esp_timer esp_partition app_update esp_app_format
    PRIV_REQUIRES wifi_manager file_manager touch_ui http_server
)