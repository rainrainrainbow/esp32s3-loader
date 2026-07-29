#include "touch_ui.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io_spi.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include "board_config.h"

static const char *TAG = "touch_ui";

// Display buffers
static uint16_t *disp_buf1 = NULL;
static uint16_t *disp_buf2 = NULL;

// LCD and touch handles
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_touch_handle_t touch_panel = NULL;
static lv_display_t *display = NULL;
static lv_indev_t *touch_indev = NULL;

// UI state
static ui_screen_id_t current_screen = UI_SCREEN_SPLASH;
static lv_obj_t *screens[UI_SCREEN_COUNT] = {0};
static lv_obj_t *status_bar = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *sd_icon = NULL;

// Animation state
static lv_anim_t splash_anim;

// Callbacks
static ui_rom_select_cb_t rom_select_cb = NULL;

// Color theme
#define COLOR_PRIMARY    lv_color_hex(0x2196F3)  // Blue
#define COLOR_SECONDARY  lv_color_hex(0x03DAC5)  // Teal
#define COLOR_ACCENT     lv_color_hex(0xFF5722)  // Deep Orange
#define COLOR_BG         lv_color_hex(0x121212)  // Dark background
#define COLOR_SURFACE    lv_color_hex(0x1E1E1E)  // Surface
#define COLOR_ON_PRIMARY lv_color_hex(0xFFFFFF)  // White

// Event callback for button press animation
static void btn_press_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_transform_scale(btn, 240, 0);
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_set_style_transform_scale(btn, 256, 0);
    }
}

// Event callback for ROM list item click
static void rom_list_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        int index = lv_obj_get_index(btn);
        if (rom_select_cb) {
            rom_select_cb(index);
        }
    }
}

// Event callback for flash result back button
static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_del(lv_obj_get_parent(lv_event_get_target(e)));
        touch_ui_show_main();
    }
}

// LVGL flush callback
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    esp_lcd_panel_draw_bitmap(lcd_panel, x1, y1, x2 + 1, y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

// LVGL touch read callback
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    esp_lcd_touch_read_data(touch_panel);
    bool touched = esp_lcd_touch_get_coordinates(touch_panel, touch_x, touch_y, touch_strength, &touch_cnt, 1);

    if (touched && touch_cnt > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_x[0];
        data->point.y = touch_y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Create status bar
static void create_status_bar(lv_obj_t *parent)
{
    status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, DISPLAY_WIDTH, 30);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon
    wifi_icon = lv_label_create(status_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x888888), 0);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 10, 0);

    // SD card icon
    sd_icon = lv_label_create(status_bar);
    lv_label_set_text(sd_icon, LV_SYMBOL_SD_CARD);
    lv_obj_set_style_text_color(sd_icon, lv_color_hex(0x888888), 0);
    lv_obj_align(sd_icon, LV_ALIGN_RIGHT_MID, -10, 0);
}

// Update status bar
void touch_ui_update_status(const char *wifi_icon_text, const char *sd_icon_text)
{
    if (wifi_icon) {
        lv_label_set_text(wifi_icon, wifi_icon_text);
        lv_obj_set_style_text_color(wifi_icon, 
            strcmp(wifi_icon_text, LV_SYMBOL_WIFI) == 0 ? COLOR_SECONDARY : lv_color_hex(0x888888), 0);
    }
    if (sd_icon) {
        lv_label_set_text(sd_icon, sd_icon_text);
        lv_obj_set_style_text_color(sd_icon,
            strcmp(sd_icon_text, LV_SYMBOL_SD_CARD) == 0 ? COLOR_SECONDARY : lv_color_hex(0x888888), 0);
    }
}

// Create splash screen
void touch_ui_show_splash(void)
{
    if (screens[UI_SCREEN_SPLASH] == NULL) {
        screens[UI_SCREEN_SPLASH] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_SPLASH], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_SPLASH], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_SPLASH], 0, 0);
        lv_obj_clear_flag(screens[UI_SCREEN_SPLASH], LV_OBJ_FLAG_SCROLLABLE);

        // Logo
        lv_obj_t *logo = lv_label_create(screens[UI_SCREEN_SPLASH]);
        lv_label_set_text(logo, "ESP32-S3\nLoader");
        lv_obj_set_style_text_color(logo, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(logo, &lv_font_montserrat_32, 0);
        lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);

        // Subtitle
        lv_obj_t *subtitle = lv_label_create(screens[UI_SCREEN_SPLASH]);
        lv_label_set_text(subtitle, "ROM Management System");
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x888888), 0);
        lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 20);

        // Loading indicator
        lv_obj_t *loading = lv_spinner_create(screens[UI_SCREEN_SPLASH]);
        lv_obj_set_size(loading, 40, 40);
        lv_obj_align(loading, LV_ALIGN_CENTER, 0, 80);
        lv_obj_set_style_arc_color(loading, COLOR_PRIMARY, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(loading, COLOR_SURFACE, LV_PART_MAIN);

        create_status_bar(screens[UI_SCREEN_SPLASH]);
    }

    lv_screen_load(screens[UI_SCREEN_SPLASH]);
    current_screen = UI_SCREEN_SPLASH;
}

// Create main menu
void touch_ui_show_main(void)
{
    if (screens[UI_SCREEN_MAIN] == NULL) {
        screens[UI_SCREEN_MAIN] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_MAIN], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_MAIN], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_MAIN], 0, 0);
        lv_obj_clear_flag(screens[UI_SCREEN_MAIN], LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t *title = lv_label_create(screens[UI_SCREEN_MAIN]);
        lv_label_set_text(title, "Main Menu");
        lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

        // Menu buttons
        const char *menu_items[] = {
            LV_SYMBOL_PLAY " Load ROM",
            LV_SYMBOL_WIFI " WiFi Manager",
            LV_SYMBOL_FILE " File Browser",
            LV_SYMBOL_SETTINGS " Settings"
        };

        for (int i = 0; i < 4; i++) {
            lv_obj_t *btn = lv_btn_create(screens[UI_SCREEN_MAIN]);
            lv_obj_set_size(btn, 240, 50);
            lv_obj_align(btn, LV_ALIGN_CENTER, 0, -75 + i * 60);
            lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
            lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 10, 0);
            lv_obj_set_style_shadow_width(btn, 10, 0);
            lv_obj_set_style_shadow_color(btn, lv_color_black(), 0);
            lv_obj_set_style_shadow_opa(btn, LV_OPA_50, 0);

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, menu_items[i]);
            lv_obj_set_style_text_color(label, COLOR_ON_PRIMARY, 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_center(label);

            // Add click animation
            lv_obj_add_event_cb(btn, btn_press_event_cb, NULL);
        }

        create_status_bar(screens[UI_SCREEN_MAIN]);
    }

    lv_screen_load(screens[UI_SCREEN_MAIN]);
    current_screen = UI_SCREEN_MAIN;
}

// Show ROM list
void touch_ui_show_rom_list(const char **rom_names, int rom_count, ui_rom_select_cb_t cb)
{
    rom_select_cb = cb;

    if (screens[UI_SCREEN_ROM_LIST] == NULL) {
        screens[UI_SCREEN_ROM_LIST] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_ROM_LIST], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_ROM_LIST], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_ROM_LIST], 0, 0);

        // Title
        lv_obj_t *title = lv_label_create(screens[UI_SCREEN_ROM_LIST]);
        lv_label_set_text(title, "Select ROM");
        lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

        // ROM list
        lv_obj_t *list = lv_list_create(screens[UI_SCREEN_ROM_LIST]);
        lv_obj_set_size(list, 280, DISPLAY_HEIGHT - 120);
        lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(list, COLOR_SURFACE, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_radius(list, 10, 0);

        for (int i = 0; i < rom_count; i++) {
            lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_FILE, rom_names[i]);
            lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
            lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_STATE_PRESSED);
            lv_obj_set_style_text_color(btn, COLOR_ON_PRIMARY, 0);
            lv_obj_set_style_radius(btn, 5, 0);
            
            lv_obj_add_event_cb(btn, rom_list_event_cb, NULL);
        }

        create_status_bar(screens[UI_SCREEN_ROM_LIST]);
    }

    lv_screen_load(screens[UI_SCREEN_ROM_LIST]);
    current_screen = UI_SCREEN_ROM_LIST;
}

// Update flash progress
void touch_ui_update_flash_progress(int pct, const char *filename)
{
    if (screens[UI_SCREEN_FLASHING] == NULL) {
        screens[UI_SCREEN_FLASHING] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_FLASHING], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_FLASHING], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_FLASHING], 0, 0);
        lv_obj_clear_flag(screens[UI_SCREEN_FLASHING], LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t *title = lv_label_create(screens[UI_SCREEN_FLASHING]);
        lv_label_set_text(title, "Flashing ROM");
        lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

        // Filename
        lv_obj_t *file_label = lv_label_create(screens[UI_SCREEN_FLASHING]);
        lv_obj_set_user_data(file_label, (void*)"filename");
        lv_obj_set_width(file_label, 280);
        lv_label_set_long_mode(file_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(file_label, lv_color_hex(0x888888), 0);
        lv_obj_align(file_label, LV_ALIGN_CENTER, 0, -40);

        // Progress bar
        lv_obj_t *bar = lv_bar_create(screens[UI_SCREEN_FLASHING]);
        lv_obj_set_size(bar, 260, 20);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(bar, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(bar, COLOR_PRIMARY, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 10, 0);

        // Percentage label
        lv_obj_t *pct_label = lv_label_create(screens[UI_SCREEN_FLASHING]);
        lv_obj_set_user_data(pct_label, (void*)"percentage");
        lv_obj_set_style_text_color(pct_label, COLOR_ON_PRIMARY, 0);
        lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_18, 0);
        lv_obj_align(pct_label, LV_ALIGN_CENTER, 0, 40);

        create_status_bar(screens[UI_SCREEN_FLASHING]);
    }

    // Update filename
    lv_obj_t *file_label = NULL;
    lv_obj_t *pct_label = NULL;
    
    uint32_t child_count = lv_obj_get_child_count(screens[UI_SCREEN_FLASHING]);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(screens[UI_SCREEN_FLASHING], i);
        const char *tag = (const char *)lv_obj_get_user_data(child);
        if (tag && strcmp(tag, "filename") == 0) {
            file_label = child;
        } else if (tag && strcmp(tag, "percentage") == 0) {
            pct_label = child;
        }
    }

    if (file_label && filename) {
        lv_label_set_text(file_label, filename);
    }

    // Update progress bar
    lv_obj_t *bar = lv_obj_get_child(screens[UI_SCREEN_FLASHING], 2);
    if (bar) {
        lv_bar_set_value(bar, pct, LV_ANIM_ON);
    }

    // Update percentage
    if (pct_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(pct_label, buf);
    }

    lv_screen_load(screens[UI_SCREEN_FLASHING]);
    current_screen = UI_SCREEN_FLASHING;
}

// Show flash result
void touch_ui_show_flash_result(bool ok, const char *msg)
{
    lv_obj_t *screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Icon
    lv_obj_t *icon = lv_label_create(screen);
    lv_label_set_text(icon, ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, ok ? COLOR_SECONDARY : COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -60);

    // Message
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, msg ? msg : (ok ? "Success!" : "Failed"));
    lv_obj_set_style_text_color(label, COLOR_ON_PRIMARY, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Back button
    lv_obj_t *btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn, 10, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Back");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, back_btn_event_cb, NULL);

    lv_screen_load(screen);
}

// Show WiFi status
void touch_ui_show_wifi(bool connected, const char *ip, const char *ssid)
{
    if (screens[UI_SCREEN_WIFI] == NULL) {
        screens[UI_SCREEN_WIFI] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_WIFI], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_WIFI], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_WIFI], 0, 0);
        lv_obj_clear_flag(screens[UI_SCREEN_WIFI], LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t *title = lv_label_create(screens[UI_SCREEN_WIFI]);
        lv_label_set_text(title, "WiFi Manager");
        lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

        create_status_bar(screens[UI_SCREEN_WIFI]);
    }

    // Clear previous content (except title and status bar)
    lv_obj_clean(screens[UI_SCREEN_WIFI]);

    // Recreate title
    lv_obj_t *title = lv_label_create(screens[UI_SCREEN_WIFI]);
    lv_label_set_text(title, "WiFi Manager");
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // Status icon
    lv_obj_t *icon = lv_label_create(screens[UI_SCREEN_WIFI]);
    lv_label_set_text(icon, connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, connected ? COLOR_SECONDARY : COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -40);

    // Status text
    lv_obj_t *status = lv_label_create(screens[UI_SCREEN_WIFI]);
    lv_label_set_text(status, connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(status, COLOR_ON_PRIMARY, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 20);

    // IP address
    if (connected && ip) {
        lv_obj_t *ip_label = lv_label_create(screens[UI_SCREEN_WIFI]);
        char buf[64];
        snprintf(buf, sizeof(buf), "IP: %s", ip);
        lv_label_set_text(ip_label, buf);
        lv_obj_set_style_text_color(ip_label, lv_color_hex(0x888888), 0);
        lv_obj_align(ip_label, LV_ALIGN_CENTER, 0, 50);
    }

    // SSID
    if (ssid) {
        lv_obj_t *ssid_label = lv_label_create(screens[UI_SCREEN_WIFI]);
        char buf[64];
        snprintf(buf, sizeof(buf), "SSID: %s", ssid);
        lv_label_set_text(ssid_label, buf);
        lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x888888), 0);
        lv_obj_align(ssid_label, LV_ALIGN_CENTER, 0, connected ? 80 : 50);
    }

    create_status_bar(screens[UI_SCREEN_WIFI]);
    lv_screen_load(screens[UI_SCREEN_WIFI]);
    current_screen = UI_SCREEN_WIFI;
}

// Show file browser
void touch_ui_show_files(void)
{
    if (screens[UI_SCREEN_FILES] == NULL) {
        screens[UI_SCREEN_FILES] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(screens[UI_SCREEN_FILES], DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(screens[UI_SCREEN_FILES], COLOR_BG, 0);
        lv_obj_set_style_border_width(screens[UI_SCREEN_FILES], 0, 0);

        // Title
        lv_obj_t *title = lv_label_create(screens[UI_SCREEN_FILES]);
        lv_label_set_text(title, "File Browser");
        lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

        // File list placeholder
        lv_obj_t *list = lv_list_create(screens[UI_SCREEN_FILES]);
        lv_obj_set_size(list, 280, DISPLAY_HEIGHT - 120);
        lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(list, COLOR_SURFACE, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_radius(list, 10, 0);

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_FILE, "No files yet");
        lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x888888), 0);

        create_status_bar(screens[UI_SCREEN_FILES]);
    }

    lv_screen_load(screens[UI_SCREEN_FILES]);
    current_screen = UI_SCREEN_FILES;
}

// Navigate to screen
void touch_ui_navigate(ui_screen_id_t screen)
{
    switch (screen) {
        case UI_SCREEN_SPLASH:
            touch_ui_show_splash();
            break;
        case UI_SCREEN_MAIN:
            touch_ui_show_main();
            break;
        case UI_SCREEN_WIFI:
            touch_ui_show_wifi(false, NULL, NULL);
            break;
        case UI_SCREEN_FILES:
            touch_ui_show_files();
            break;
        default:
            break;
    }
}

// Get current screen
ui_screen_id_t touch_ui_get_current_screen(void)
{
    return current_screen;
}

// LVGL task handler
void touch_ui_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Initialize touch UI
esp_err_t touch_ui_init(void)
{
    ESP_LOGI(TAG, "Initializing touch UI");

    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = DISPLAY_CLK_GPIO,
        .mosi_io_num = DISPLAY_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize LCD
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DISPLAY_DC_GPIO,
        .cs_gpio_num = DISPLAY_CS_GPIO,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISPLAY_SPI_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = DISPLAY_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_panel));

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_mirror(lcd_panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    esp_lcd_panel_swap_xy(lcd_panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_set_gap(lcd_panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    // Initialize I2C for touch
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_SDA_GPIO,
        .scl_io_num = TOUCH_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = TOUCH_I2C_CLK_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(TOUCH_I2C_HOST, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(TOUCH_I2C_HOST, I2C_MODE_MASTER, 0, 0, 0));

    // Initialize touch
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_WIDTH,
        .y_max = DISPLAY_HEIGHT,
        .rst_gpio_num = TOUCH_RST_GPIO,
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &touch_panel));

    // Initialize LVGL
    lv_init();

    // Allocate display buffers in PSRAM
    disp_buf1 = heap_caps_malloc(DISPLAY_WIDTH * 40 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    disp_buf2 = heap_caps_malloc(DISPLAY_WIDTH * 40 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    // Create LVGL display
    display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, disp_buf1, disp_buf2, DISPLAY_WIDTH * 40 * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display, io_handle);

    // Create LVGL input device (touch)
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_cb);

    // Set dark theme
    lv_obj_set_style_bg_color(lv_screen_active(), COLOR_BG, 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    ESP_LOGI(TAG, "Touch UI initialized");
    return ESP_OK;
}