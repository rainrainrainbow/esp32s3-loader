#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_SPLASH = 0,
    UI_SCREEN_MAIN,
    UI_SCREEN_ROM_LIST,
    UI_SCREEN_FLASHING,
    UI_SCREEN_WIFI,
    UI_SCREEN_FILES,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef void (*ui_rom_select_cb_t)(int index);
typedef void (*ui_wifi_connect_cb_t)(const char *ssid, const char *password);
typedef void (*ui_navigate_cb_t)(ui_screen_id_t screen);
typedef void (*ui_menu_cb_t)(int menu_index);

esp_err_t touch_ui_init(void);
void touch_ui_show_splash(void);
void touch_ui_show_main(void);
void touch_ui_set_menu_callback(ui_menu_cb_t cb);
void touch_ui_show_rom_list(const char **rom_names, int rom_count, ui_rom_select_cb_t cb);
void touch_ui_update_flash_progress(int pct, const char *filename);
void touch_ui_show_flash_result(bool ok, const char *msg);
void touch_ui_show_wifi(bool connected, const char *ip, const char *ssid);
void touch_ui_show_files(void);
void touch_ui_update_status(const char *wifi_icon, const char *sd_icon);
void touch_ui_task(void *arg);
ui_screen_id_t touch_ui_get_current_screen(void);
void touch_ui_navigate(ui_screen_id_t screen);

#ifdef __cplusplus
}
#endif