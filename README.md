# ESP32-S3 ROM Loader

基于 xiaomiao-loader 改进的 ESP32-S3 ROM 加载器，支持 WiFi 文件管理和绚丽的触摸 UI。

## ✨ 特性

- 🎮 **绚丽的触摸 UI** - 基于 LVGL 9.x 的现代深色主题界面
- 📶 **WiFi 文件管理** - 通过 Web 界面上传/管理 ROM 文件
- 💾 **SPIFFS 存储** - 16MB Flash 中分配 11MB 用于 ROM 存储
- 🚀 **OTA 更新** - 支持 ROM 固件 OTA 写入
- 📊 **实时状态显示** - WiFi 连接状态、存储空间、加载进度等

## 🏗️ 硬件要求

- ESP32-S3-N16R8（16MB Flash + 8MB PSRAM）
- 320x240 ST7789 SPI TFT 显示屏
- GT911 I2C 触摸控制器
- ES8311 音频编解码器（可选）

## 📦 构建

### GitHub Actions 自动构建

本项目配置了 GitHub Actions，每次推送到 main 分支时会自动构建并生成固件文件。

构建产物包括：
- 分离的固件文件（bootloader.bin, partition-table.bin, esp32s3-loader.bin）
- 合并的固件文件（esp32s3-loader-merged.bin）

### 本地构建

需要 ESP-IDF v5.3.1 或更高版本：

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 构建项目
idf.py build

# 烧录固件
idf.py -p /dev/ttyUSB0 flash

# 监控串口
idf.py -p /dev/ttyUSB0 monitor
```

## 🚀 使用方法

### 1. 首次启动

1. 烧录固件到 ESP32-S3
2. 设备会自动创建 WiFi 热点：
   - SSID: `ESP32-Loader`
   - 密码: `12345678`

### 2. WiFi 文件管理

1. 连接设备的 WiFi 热点
2. 打开浏览器访问 `http://192.168.4.1`
3. 上传 `.bin` 格式的 ROM 文件
4. 文件会自动保存到 SPIFFS 存储

### 3. 本地文件管理

1. 在触摸屏上操作 UI
2. 点击 "Load ROM" 浏览存储中的 ROM 文件
3. 选择 ROM 文件并加载
4. 设备会自动重启并运行选中的 ROM

## 📊 分区表

| 名称 | 类型 | 子类型 | 偏移 | 大小 | 说明 |
|------|------|--------|------|------|------|
| nvs | data | nvs | 0x9000 | 24KB | 非易失性存储 |
| phy_init | data | phy | 0xf000 | 4KB | PHY 初始化 |
| factory | app | factory | 0x10000 | 4MB | Loader 固件 |
| storage | data | spiffs | | 11MB | ROM 文件存储 |

## 📁 项目结构

```
esp32s3-loader/
├── main/
│   ├── main.c              # 主程序入口
│   ├── board_config.h      # 硬件配置
│   └── CMakeLists.txt
├── components/
│   ├── wifi_manager/       # WiFi 管理组件
│   ├── file_manager/       # 文件管理组件（SPIFFS）
│   ├── touch_ui/           # 触摸 UI 组件（LVGL）
│   └── http_server/        # HTTP 服务器组件
├── partitions.csv          # 分区表
├── sdkconfig.defaults      # SDK 配置
└── .github/workflows/      # GitHub Actions 配置
```

## 🐛 故障排除

### WiFi 连接失败
- 检查 WiFi 密码是否正确（默认：12345678）
- 确保设备在 WiFi 范围内
- 重启设备重试

### 存储空间不足
- 通过 Web 界面删除不需要的 ROM 文件
- 单个 ROM 文件大小限制：4MB

### 触摸屏无响应
- 检查 GT911 I2C 连接
- 检查触摸校准
- 重启设备

## 📄 许可证

MIT License

## 🙏 致谢

- [xiaomiao-loader](https://github.com/jsfaint/xiaomiao-loader) - 原始项目
- [LVGL](https://lvgl.io/) - 图形库
- [ESP-IDF](https://github.com/espressif/esp-idf) - 开发框架