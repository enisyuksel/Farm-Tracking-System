# 🚀 IQ Yazılım LoRa OTA System

**STM32WL55 CMake/VSCode Projesi - Gateway/Node Seçilebilir Build Sistemi**

---

## 📋 Proje Özeti

Bu proje, STM32WL55 tabanlı LoRa Gateway/Node sistemini **tek CMake projesi** olarak yönetir. JSON tabanlı configuration management ile Gateway veya Node modunu seçebilirsiniz.

### ✨ Özellikler
- 🔧 **JSON Configuration Management** - Tek yerden tüm ayarlar
- 🏭 **Gateway Mode**: WiFi + MQTT + GitHub OTA Distribution  
- 📡 **Node Mode**: Sensor + OTA Receiver
- 🛠️ **VSCode Integration** - Build/Debug/Flash tasks
- 📝 **Auto-Generated Headers** - config.json → project_config.h
- 🎯 **Single Project** - Gateway/Node aynı codebase

---

## 🏗️ Proje Yapısı

```
IQYazilim-cmakebased/SubGHz_Phy_PingPong/
├── config.json                        # 🔧 Ana konfigürasyon dosyası
├── config_manager.py                  # 🐍 Configuration management script
├── CMakeLists.txt                     # 📦 Build configuration
├── Core/
│   ├── Inc/project_config.h           # 📝 Auto-generated config header  
│   └── Src/main.c                     # 🚀 Ana uygulama
├── SubGHz_Phy/App/
│   ├── lora_ota_gateway.c/.h          # 🏭 Gateway OTA kodu
│   ├── lora_ota_node.c/.h             # 📡 Node OTA kodu
│   ├── ota_integration_app.c/.h       # 🔗 Gateway entegrasyon
│   └── ota_node_integration_app.c/.h  # 🔗 Node entegrasyon
└── .vscode/
    ├── tasks.json                     # ⚙️ Build/Config tasks
    └── launch.json                    # 🐛 Debug configuration
```

---

## 🚀 Hızlı Başlangıç

### 1. 🔧 Proje Konfigürasyonu

```bash
# İnteraktif configuration editor
python3 config_manager.py configure

# Veya sadece header generate et
python3 config_manager.py generate
```

**VSCode'da:** `Ctrl+Shift+P` → `Tasks: Run Task` → `🔧 Configure Project`

### 2. 📝 Build Mode Seçimi

`config.json` dosyasında:

```json
{
  "build": {
    "mode": "GATEWAY",        // 🏭 Gateway modu
    "options": ["GATEWAY", "NODE"]
  }
}
```

**Gateway Mode için:**
```json
"mode": "GATEWAY"
```

**Node Mode için:**
```json  
"mode": "NODE"
```

### 3. 🏗️ Build ve Flash

```bash
# Build (auto-generates config headers)
cmake --build build/Debug

# Flash
st-flash write build/Debug/SubGHz_Phy_PingPong_Gateway.bin 0x08000000
```

**VSCode'da:** 
- Build: `Ctrl+Shift+B`
- Flash: `Ctrl+Shift+P` → `Tasks: Run Task` → `STM32: Flash`

---

## ⚙️ Configuration Rehberi

### 🏭 Gateway Configuration

```json
{
  "build": {"mode": "GATEWAY"},
  "gateway": {
    "device_id": "GATEWAY_001",
    
    "wifi": {
      "ssid": "YOUR_WIFI_SSID",
      "password": "YOUR_WIFI_PASSWORD"
    },
    
    "mqtt": {
      "broker": "YOUR_MQTT_BROKER",
      "port": 1883,
      "username": "YOUR_MQTT_USER",
      "password": "YOUR_MQTT_PASS"
    },
    
    "ota": {
      "github": {
        "owner": "YOUR_GITHUB_USER",
        "repo": "YOUR_REPO_NAME",
        "token": "YOUR_GITHUB_TOKEN"
      },
      "firmware": {
        "filename": "node_firmware.bin",
        "chunk_size": 230
      }
    }
  }
}
```

### 📡 Node Configuration  

```json
{
  "build": {"mode": "NODE"},
  "node": {
    "device_id": "NODE_001",
    
    "sensor": {
      "type": "ENVIRONMENTAL",
      "sampling_period": 30,
      "battery_monitor": true
    },
    
    "ota": {
      "flash": {
        "bank2_start": "0x08020000",
        "page_size": 2048,
        "bootloader_flag": "0x0803F800"
      },
      "retry": {
        "max_attempts": 3,
        "timeout": 5000
      }
    },
    
    "power": {
      "low_power_mode": true,
      "wake_interval": 60
    }
  }
}
```

### 📻 Hardware Configuration

```json
{
  "hardware": {
    "frequency": 868000000,      // LoRa frekansı (Hz)
    "tx_power": 14,             // TX gücü (dBm)
    "spreading_factor": 7,       // LoRa SF
    "bandwidth": "125kHz",       // LoRa BW
    "coding_rate": "4/5"        // LoRa CR
  }
}
```

---

## 🛠️ VSCode Tasks

| Task | Açıklama | Kısayol |
|------|----------|---------|
| `🔧 Configure Project` | İnteraktif configuration editor | - |
| `📝 Generate Config Headers` | config.json → project_config.h | - |
| `STM32: Build` | CMake build (auto-generates configs) | `Ctrl+Shift+B` |
| `STM32: Clean` | Clean build files | - |
| `STM32: Flash` | Program STM32 | - |

---

## 🔄 Mode Değiştirme Workflow

### Gateway → Node

1. **Config değiştir:**
   ```bash
   python3 config_manager.py configure
   # Build mode: 2 (NODE) seç
   ```

2. **Build:**
   ```bash
   cmake --build build/Debug
   # Output: SubGHz_Phy_PingPong_Node.elf
   ```

3. **Flash:**
   ```bash
   st-flash write build/Debug/SubGHz_Phy_PingPong_Node.bin 0x08000000
   ```

### Node → Gateway

Aynı workflow, sadece build mode'u 1 (GATEWAY) seç.

---

## 📊 Generated Files

### `project_config.h`
`config.json`'dan otomatik generate edilen header:

```c
#define CURRENT_BUILD_MODE      BUILD_MODE_GATEWAY
#define DEVICE_ID               "GATEWAY_001"
#define WIFI_SSID               "YOUR_WIFI_SSID"
#define MQTT_BROKER             "YOUR_MQTT_BROKER"
#define OTA_ENABLED             true
// ... vs
```

### Build Output
- **Gateway:** `SubGHz_Phy_PingPong_Gateway.elf/bin/hex`
- **Node:** `SubGHz_Phy_PingPong_Node.elf/bin/hex`

---

## 🚨 Troubleshooting

### Build Hatası: "Unknown build mode"
```bash
# config.json kontrol et
python3 config_manager.py generate
cmake --build build/Debug
```

### VSCode IntelliSense Sorunu  
```bash
# Config headers'ı yeniden generate et
python3 config_manager.py generate
# VSCode restart: Ctrl+Shift+P → "Developer: Reload Window"
```

### Flash Sorunu
```bash
# ST-Link driver kontrol et
st-info --probe
# Bağlantıları kontrol et: VDD, GND, SWDIO, SWCLK
```

---

## 📚 Code Structure

### Gateway Mode
- `main.c` → ESP32 + MQTT initialization
- `ota_integration_app.c` → Gateway OTA coordinator  
- `lora_ota_gateway.c` → LoRa OTA distributor
- `github_ota_downloader.c` → GitHub API client

### Node Mode
- `main.c` → Sensor + low power mode
- `ota_node_integration_app.c` → Node OTA coordinator
- `lora_ota_node.c` → LoRa OTA receiver + flash management

---

## 🎯 Next Steps

- [ ] Add encryption support
- [ ] Web-based configuration interface
- [ ] Multiple sensor types support
- [ ] Battery monitoring dashboard
- [ ] Over-the-air configuration updates

---

**🏢 IQ Yazılım - Professional IoT Solutions**
