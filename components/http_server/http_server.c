#include "http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "file_manager.h"
#include <string.h>

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// HTML page for file management
static const char INDEX_HTML[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"  <meta charset='UTF-8'>"
"  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"  <title>ESP32-S3 ROM Loader</title>"
"  <style>"
"    * { margin: 0; padding: 0; box-sizing: border-box; }"
"    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }"
"    .container { max-width: 800px; margin: 0 auto; background: white; border-radius: 20px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); overflow: hidden; }"
"    .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center; }"
"    .header h1 { font-size: 2em; margin-bottom: 10px; }"
"    .header p { opacity: 0.9; }"
"    .content { padding: 30px; }"
"    .section { margin-bottom: 30px; }"
"    .section h2 { color: #667eea; margin-bottom: 15px; font-size: 1.5em; }"
"    .upload-area { border: 3px dashed #667eea; border-radius: 15px; padding: 40px; text-align: center; cursor: pointer; transition: all 0.3s; background: #f8f9ff; }"
"    .upload-area:hover { border-color: #764ba2; background: #f0f2ff; }"
"    .upload-area.dragover { border-color: #764ba2; background: #e8ebff; }"
"    .upload-icon { font-size: 3em; margin-bottom: 15px; }"
"    .file-input { display: none; }"
"    .btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; padding: 12px 30px; border-radius: 25px; cursor: pointer; font-size: 1em; transition: transform 0.2s; }"
"    .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4); }"
"    .file-list { list-style: none; }"
"    .file-item { background: #f8f9ff; padding: 15px; margin-bottom: 10px; border-radius: 10px; display: flex; justify-content: space-between; align-items: center; transition: all 0.3s; }"
"    .file-item:hover { background: #f0f2ff; transform: translateX(5px); }"
"    .file-name { font-weight: 500; color: #333; }"
"    .file-size { color: #666; font-size: 0.9em; }"
"    .file-actions { display: flex; gap: 10px; }"
"    .btn-small { padding: 6px 15px; font-size: 0.9em; border-radius: 15px; }"
"    .btn-danger { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }"
"    .progress { width: 100%; height: 8px; background: #e0e0e0; border-radius: 4px; overflow: hidden; margin-top: 15px; display: none; }"
"    .progress-bar { height: 100%; background: linear-gradient(90deg, #667eea 0%, #764ba2 100%); width: 0%; transition: width 0.3s; }"
"    .status { margin-top: 15px; padding: 15px; border-radius: 10px; display: none; }"
"    .status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }"
"    .status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }"
"    .wifi-info { background: #f8f9ff; padding: 20px; border-radius: 15px; }"
"    .info-item { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #e0e0e0; }"
"    .info-item:last-child { border-bottom: none; }"
"    .info-label { font-weight: 500; color: #666; }"
"    .info-value { color: #333; font-weight: 600; }"
"  </style>"
"</head>"
"<body>"
"  <div class='container'>"
"    <div class='header'>"
"      <h1>🎮 ESP32-S3 ROM Loader</h1>"
"      <p>WiFi File Management System</p>"
"    </div>"
"    <div class='content'>"
"      <div class='section'>"
"        <h2>📤 Upload ROM Files</h2>"
"        <div class='upload-area' id='uploadArea'>"
"          <div class='upload-icon'>📁</div>"
"          <p>Click to select or drag & drop .bin files here</p>"
"          <input type='file' id='fileInput' class='file-input' accept='.bin' multiple>"
"        </div>"
"        <div class='progress' id='progress'>"
"          <div class='progress-bar' id='progressBar'></div>"
"        </div>"
"        <div class='status' id='status'></div>"
"      </div>"
"      <div class='section'>"
"        <h2>📂 ROM Files on Device</h2>"
"        <ul class='file-list' id='fileList'>"
"          <li class='file-item'><span>Loading...</span></li>"
"        </ul>"
"      </div>"
"      <div class='section'>"
"        <h2>📊 Storage Information</h2>"
"        <div class='wifi-info'>"
"          <div class='info-item'>"
"            <span class='info-label'>Total Storage:</span>"
"            <span class='info-value' id='totalStorage'>Loading...</span>"
"          </div>"
"          <div class='info-item'>"
"            <span class='info-label'>Used Storage:</span>"
"            <span class='info-value' id='usedStorage'>Loading...</span>"
"          </div>"
"          <div class='info-item'>"
"            <span class='info-label'>Free Space:</span>"
"            <span class='info-value' id='freeStorage'>Loading...</span>"
"          </div>"
"        </div>"
"      </div>"
"    </div>"
"  </div>"
"  <script>"
"    const uploadArea = document.getElementById('uploadArea');"
"    const fileInput = document.getElementById('fileInput');"
"    const fileList = document.getElementById('fileList');"
"    const progress = document.getElementById('progress');"
"    const progressBar = document.getElementById('progressBar');"
"    const status = document.getElementById('status');"
"    uploadArea.addEventListener('click', () => fileInput.click());"
"    uploadArea.addEventListener('dragover', (e) => { e.preventDefault(); uploadArea.classList.add('dragover'); });"
"    uploadArea.addEventListener('dragleave', () => uploadArea.classList.remove('dragover'));"
"    uploadArea.addEventListener('drop', (e) => { e.preventDefault(); uploadArea.classList.remove('dragover'); handleFiles(e.dataTransfer.files); });"
"    fileInput.addEventListener('change', (e) => handleFiles(e.target.files));"
"    async function handleFiles(files) {"
"      for (let file of files) {"
"        if (!file.name.endsWith('.bin')) { showStatus('Only .bin files are allowed', 'error'); return; }"
"        await uploadFile(file);"
"      }"
"    }"
"    async function uploadFile(file) {"
"      const formData = new FormData();"
"      formData.append('file', file);"
"      progress.style.display = 'block';"
"      try {"
"        const xhr = new XMLHttpRequest();"
"        xhr.open('POST', '/upload');"
"        xhr.upload.onprogress = (e) => { if (e.lengthComputable) { progressBar.style.width = (e.loaded / e.total * 100) + '%'; } };"
"        xhr.onload = () => { if (xhr.status === 200) { showStatus('File uploaded successfully!', 'success'); loadFileList(); } else { showStatus('Upload failed', 'error'); } progress.style.display = 'none'; };"
"        xhr.onerror = () => { showStatus('Upload failed', 'error'); progress.style.display = 'none'; };"
"        xhr.send(formData);"
"      } catch (e) { showStatus('Upload failed: ' + e.message, 'error'); progress.style.display = 'none'; }"
"    }"
"    async function loadFileList() {"
"      try { const res = await fetch('/api/files'); const data = await res.json(); fileList.innerHTML = ''; if (data.files.length === 0) { fileList.innerHTML = '<li class=\\'file-item\\'><span>No ROM files found</span></li>'; return; } data.files.forEach(file => { const li = document.createElement('li'); li.className = 'file-item'; li.innerHTML = `<div><div class='file-name'>${file.name}</div><div class='file-size'>${formatSize(file.size)}</div></div><div class='file-actions'><button class='btn btn-small btn-danger' onclick='deleteFile(\"${file.name}\")'>Delete</button></div>`; fileList.appendChild(li); }); } catch (e) { fileList.innerHTML = '<li class=\\'file-item\\'><span>Error loading files</span></li>'; }"
"    }"
"    async function deleteFile(filename) { if (!confirm('Delete ' + filename + '?')) return; try { const res = await fetch('/api/delete?file=' + encodeURIComponent(filename)); if (res.ok) { showStatus('File deleted', 'success'); loadFileList(); } else { showStatus('Delete failed', 'error'); } } catch (e) { showStatus('Delete failed', 'error'); } }"
"    function formatSize(bytes) { if (bytes < 1024) return bytes + ' B'; if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'; return (bytes / (1024 * 1024)).toFixed(1) + ' MB'; }"
"    function showStatus(msg, type) { status.textContent = msg; status.className = 'status ' + type; status.style.display = 'block'; setTimeout(() => status.style.display = 'none', 3000); }"
"    async function loadStorageInfo() { try { const res = await fetch('/api/storage'); const data = await res.json(); document.getElementById('totalStorage').textContent = data.total_kb + ' KB'; document.getElementById('usedStorage').textContent = data.used_kb + ' KB'; document.getElementById('freeStorage').textContent = (data.total_kb - data.used_kb) + ' KB'; } catch (e) {} }"
"    loadFileList(); loadStorageInfo(); setInterval(loadFileList, 5000);"
"  </script>"
"</body>"
"</html>";

// HTTP request handlers
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
    return ESP_OK;
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "File upload request, content length: %d", req->content_len);
    
    // Get filename from content-Disposition header
    char content_disp[256] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Disposition", content_disp, sizeof(content_disp)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_FAIL;
    }
    
    char *filename_start = strstr(content_disp, "filename=\"");
    if (!filename_start) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    filename_start += 10;
    char *filename_end = strchr(filename_start, '"');
    if (!filename_end) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    
    char filename[64] = {0};
    int filename_len = filename_end - filename_start;
    if (filename_len >= sizeof(filename)) filename_len = sizeof(filename) - 1;
    strncpy(filename, filename_start, filename_len);
    
    ESP_LOGI(TAG, "Uploading file: %s", filename);
    
    // Allocate buffer for file data
    uint8_t *buf = malloc(req->content_len);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    
    // Read file data
    int received = 0;
    int remaining = req->content_len;
    
    // Skip multipart form data headers
    bool headers_skipped = false;
    int data_start = 0;
    
    while (remaining > 0) {
        int to_read = remaining < 4096 ? remaining : 4096;
        int received_bytes = httpd_req_recv(req, (char *)(buf + received), to_read);
        
        if (received_bytes <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        
        if (!headers_skipped) {
            // Find the end of headers (double newline)
            for (int i = 0; i < received_bytes - 3; i++) {
                if (buf[received + i] == '\r' && buf[received + i + 1] == '\n' && 
                    buf[received + i + 2] == '\r' && buf[received + i + 3] == '\n') {
                    data_start = received + i + 4;
                    headers_skipped = true;
                    break;
                }
            }
        }
        
        received += received_bytes;
        remaining -= received_bytes;
    }
    
    // Calculate actual data size (skip headers and boundary)
    int data_size = received - data_start;
    if (data_size > 0 && data_start > 0) {
        // Remove trailing boundary
        if (data_size > 2 && buf[received - 2] == '-' && buf[received - 1] == '-') {
            data_size -= 2;
        }
        if (data_size > 2 && buf[received - 2] == '\r' && buf[received - 1] == '\n') {
            data_size -= 2;
        }
        
        // Save file
        esp_err_t err = file_manager_upload_rom(filename, buf + data_start, data_size);
        free(buf);
        
        if (err == ESP_OK) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
            return ESP_OK;
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }
    }
    
    free(buf);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file data");
    return ESP_FAIL;
}

static esp_err_t files_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_AddArrayToObject(root, "files");
    
    // Scan ROM files
    rom_file_t roms[MAX_ROM_FILES];
    int count = file_manager_scan_roms(roms, MAX_ROM_FILES);
    
    for (int i = 0; i < count; i++) {
        cJSON *file = cJSON_CreateObject();
        cJSON_AddStringToObject(file, "name", roms[i].filename);
        cJSON_AddNumberToObject(file, "size", roms[i].size);
        cJSON_AddBoolToObject(file, "valid", roms[i].is_valid);
        cJSON_AddItemToArray(files, file);
    }
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    
    free(json);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t delete_get_handler(httpd_req_t *req)
{
    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
        return ESP_FAIL;
    }
    
    char filename[64] = {0};
    if (httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing file parameter");
        return ESP_FAIL;
    }
    
    if (file_manager_delete_rom(filename) == ESP_OK) {
        ESP_LOGI(TAG, "File deleted: %s", filename);
        httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    } else {
        ESP_LOGE(TAG, "Failed to delete file: %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
    }
    
    return ESP_OK;
}

static esp_err_t storage_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    uint32_t total_kb = 0, used_kb = 0;
    if (file_manager_get_storage_info(&total_kb, &used_kb) == ESP_OK) {
        cJSON_AddNumberToObject(root, "total_kb", total_kb);
        cJSON_AddNumberToObject(root, "used_kb", used_kb);
    } else {
        cJSON_AddNumberToObject(root, "total_kb", 0);
        cJSON_AddNumberToObject(root, "used_kb", 0);
    }
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    
    free(json);
    cJSON_Delete(root);
    
    return ESP_OK;
}

esp_err_t http_server_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.stack_size = 8192;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ret;
    }
    
    // Register URI handlers
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
    };
    httpd_register_uri_handler(server, &index_uri);
    
    httpd_uri_t upload_uri = {
        .uri = "/upload",
        .method = HTTP_POST,
        .handler = upload_post_handler,
    };
    httpd_register_uri_handler(server, &upload_uri);
    
    httpd_uri_t files_uri = {
        .uri = "/api/files",
        .method = HTTP_GET,
        .handler = files_get_handler,
    };
    httpd_register_uri_handler(server, &files_uri);
    
    httpd_uri_t delete_uri = {
        .uri = "/api/delete",
        .method = HTTP_GET,
        .handler = delete_get_handler,
    };
    httpd_register_uri_handler(server, &delete_uri);
    
    httpd_uri_t storage_uri = {
        .uri = "/api/storage",
        .method = HTTP_GET,
        .handler = storage_get_handler,
    };
    httpd_register_uri_handler(server, &storage_uri);
    
    ESP_LOGI(TAG, "HTTP server initialized");
    return ESP_OK;
}

esp_err_t http_server_start(void)
{
    if (server == NULL) {
        return http_server_init();
    }
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    return ESP_OK;
}