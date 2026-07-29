#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ROM_FILES 32
#define MAX_FILENAME_LEN 64

typedef struct {
    char filename[MAX_FILENAME_LEN];
    uint32_t size;
    bool is_valid;
} rom_file_t;

esp_err_t file_manager_init(void);
int file_manager_scan_roms(rom_file_t *roms, int max_count);
esp_err_t file_manager_upload_rom(const char *filename, const uint8_t *data, size_t size);
esp_err_t file_manager_delete_rom(const char *filename);
esp_err_t file_manager_load_rom(const char *filename, void (*progress_cb)(int));
esp_err_t file_manager_get_storage_info(uint32_t *total_kb, uint32_t *used_kb);

#ifdef __cplusplus
}
#endif