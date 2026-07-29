idf_component_register(
    SRCS "touch_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_lcd_touch espressif__esp_lcd_touch_gt911 lvgl driver
)