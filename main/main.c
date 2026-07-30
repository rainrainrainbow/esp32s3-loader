#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "wifi_manager.h"
#include "file_manager.h"
#include "touch_ui.h"
#include "http_server.h"
#include "board_config.h"

static const char *TAG = "main";

// Global state
static rom_file_t s_roms[MAX_ROM_FILES];
static int s_rom_count = 0;

// Backlight control
static void backlight_init(void)
{
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << DISPLAY_BACKLIGHT_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 0 : 1);
    }
}

// Progress callback for ROM loading
static void progress_callback(int progress)
{
    touch_ui_update_flash_progress(progress, NULL);
}

// ROM selection callback
static void rom_selected_cb(int index)
{
    if (index < 0 || index >= s_rom_count) {
        ESP_LOGE(TAG, "Invalid ROM index: %d", index);
        return;
    }

    ESP_LOGI(TAG, "Selected ROM: %s", s_roms[index].filename);
    
    // Show flashing screen
    touch_ui_update_flash_progress(0, s_roms[index].filename);
    
    // Load ROM with progress callback
    esp_err_t err = file_manager_load_rom(s_roms[index].filename, progress_callback);
    
    if (err == ESP_OK) {
        touch_ui_show_flash_result(true, "ROM loaded!\nRebooting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        touch_ui_show_flash_result(false, "Failed to load ROM");
    }
}

// Main menu callback
static void main_menu_cb(int menu_index)
{
    switch (menu_index) {
        case 0: { // Load ROM
            s_rom_count = file_manager_scan_roms(s_roms, MAX_ROM_FILES);
            if (s_rom_count > 0) {
                static const char *names[MAX_ROM_FILES];
                for (int i = 0; i < s_rom_count; i++) {
                    names[i] = s_roms[i].filename;
                }
                touch_ui_show_rom_list(names, s_rom_count, rom_selected_cb);
            } else {
                touch_ui_show_flash_result(false, "No ROM files found.\nUpload via Web UI.");
            }
            break;
        }
        case 1: { // WiFi Manager
            char ip[16] = {0};
            bool connected = wifi_manager_is_connected() && wifi_manager_get_ip_str(ip) == ESP_OK;
            touch_ui_show_wifi(connected, connected ? ip : NULL, "ESP32-Loader");
            break;
        }
        case 2: // File Browser
            touch_ui_show_files();
            break;
        case 3: { // Settings
            uint32_t total_kb = 0, used_kb = 0;
            char msg[64];
            if (file_manager_get_storage_info(&total_kb, &used_kb) == ESP_OK) {
                snprintf(msg, sizeof(msg), "Storage: %lu/%lu KB\nFree: %lu KB",
                         (unsigned long)used_kb, (unsigned long)total_kb,
                         (unsigned long)(total_kb - used_kb));
            } else {
                snprintf(msg, sizeof(msg), "Storage: N/A");
            }
            touch_ui_show_flash_result(true, msg);
            break;
        }
    }
}

// Initialize NVS
static void nvs_init_safe(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// Main application entry point
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 ROM Loader starting...");
    ESP_LOGI(TAG, "Free heap: %lu", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // Initialize NVS
    nvs_init_safe();
    
    // Initialize backlight
    backlight_init();
    
    // Initialize touch UI
    ESP_ERROR_CHECK(touch_ui_init());
    
    // Show splash screen
    touch_ui_show_splash();
    
    // Initialize file manager (SPIFFS storage)
    esp_err_t err = file_manager_init();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Storage initialized");
        touch_ui_update_status(LV_SYMBOL_WIFI, LV_SYMBOL_SD_CARD);
        
        // Scan ROMs
        s_rom_count = file_manager_scan_roms(s_roms, MAX_ROM_FILES);
        ESP_LOGI(TAG, "Found %d ROM files", s_rom_count);
    } else {
        ESP_LOGE(TAG, "Storage init failed: %s", esp_err_to_name(err));
    }
    
    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    
    // Start WiFi AP mode for file management
    wifi_manager_start_ap("ESP32-Loader", "12345678");
    ESP_LOGI(TAG, "WiFi AP started: ESP32-Loader (password: 12345678)");
    
    // Start HTTP server
    ESP_ERROR_CHECK(http_server_start());
    ESP_LOGI(TAG, "HTTP server running at http://192.168.4.1");
    
    // Start LVGL task
    xTaskCreate(touch_ui_task, "lvgl_task", 8192, NULL, 10, NULL);
    
    // Wait then show main menu
    vTaskDelay(pdMS_TO_TICKS(2000));
    touch_ui_set_menu_callback(main_menu_cb);
    touch_ui_show_main();
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}