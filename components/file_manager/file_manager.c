#include "file_manager.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "file_manager";

#define STORAGE_MOUNT_POINT "/storage"
#define ROM_DIR_PATH "/storage/roms"

static bool s_initialized = false;

esp_err_t file_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS storage");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = STORAGE_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS total: %d, used: %d", total, used);
    }
    
    // Create roms directory if not exists
    mkdir(ROM_DIR_PATH, 0755);
    
    s_initialized = true;
    return ESP_OK;
}

int file_manager_scan_roms(rom_file_t *roms, int max_count)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "File manager not initialized");
        return 0;
    }
    
    DIR *dir = opendir(ROM_DIR_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open %s", ROM_DIR_PATH);
        return 0;
    }
    
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        
        // Check if .bin file
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".bin") != 0) {
            continue;
        }
        
        rom_file_t *rom = &roms[count];
        strncpy(rom->filename, entry->d_name, MAX_FILENAME_LEN - 1);
        rom->filename[MAX_FILENAME_LEN - 1] = '\0';
        
        // Get file size
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%s", ROM_DIR_PATH, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            rom->size = st.st_size;
        }
        
        // Check if valid ROM (simplified check)
        FILE *f = fopen(filepath, "rb");
        if (f) {
            uint8_t magic;
            if (fread(&magic, 1, 1, f) == 1) {
                rom->is_valid = (magic == 0xE9);  // ESP32 app image magic
            }
            fclose(f);
        }
        
        ESP_LOGI(TAG, "Found ROM: %s, size: %lu, valid: %d", 
                 rom->filename, rom->size, rom->is_valid);
        count++;
    }
    
    closedir(dir);
    return count;
}

esp_err_t file_manager_upload_rom(const char *filename, const uint8_t *data, size_t size)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }
    
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", ROM_DIR_PATH, filename);
    
    ESP_LOGI(TAG, "Uploading ROM: %s (%lu bytes)", filepath, size);
    
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Write failed: wrote %lu of %lu", written, size);
        remove(filepath);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "ROM uploaded successfully: %s", filename);
    return ESP_OK;
}

esp_err_t file_manager_delete_rom(const char *filename)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }
    
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", ROM_DIR_PATH, filename);
    
    if (remove(filepath) == 0) {
        ESP_LOGI(TAG, "ROM deleted: %s", filename);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to delete ROM: %s", filename);
    return ESP_FAIL;
}

esp_err_t file_manager_load_rom(const char *filename, void (*progress_cb)(int))
{
    if (!s_initialized) {
        return ESP_FAIL;
    }
    
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", ROM_DIR_PATH, filename);
    
    ESP_LOGI(TAG, "Loading ROM: %s", filepath);
    
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Find OTA partition
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        ESP_LOGE(TAG, "OTA partition not found");
        fclose(f);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to OTA partition at 0x%lx, size: %lu", 
             ota_partition->address, ota_partition->size);
    
    if (file_size > ota_partition->size) {
        ESP_LOGE(TAG, "ROM too large: %lu > %lu", file_size, ota_partition->size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Begin OTA
    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(ota_partition, file_size, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        fclose(f);
        return err;
    }
    
    // Write data
    uint8_t buffer[4096];
    size_t total_read = 0;
    size_t read_bytes;
    
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        err = esp_ota_write(update_handle, buffer, read_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            fclose(f);
            return err;
        }
        
        total_read += read_bytes;
        
        if (progress_cb) {
            int progress = (total_read * 100) / file_size;
            progress_cb(progress);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    fclose(f);
    
    // End OTA
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "ROM loaded successfully, rebooting...");
    return ESP_OK;
}

esp_err_t file_manager_get_storage_info(uint32_t *total_kb, uint32_t *used_kb)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }
    
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    
    if (ret == ESP_OK) {
        *total_kb = total / 1024;
        *used_kb = used / 1024;
    }
    
    return ret;
}