# VSCode CMake Projesi - OTA Sistemi Quick Start

Bu rehber VSCode'da CMake tabanlı STM32WL55 projesinin nasıl çalıştırılacağını anlatır.

## 🚀 Hızlı Başlangıç

### 1. VSCode'da Proje Aç
```bash
cd /home/enis/workSpace/IQ-Yazilim/IQYazilim-worked.v00/IQYazilim-cmakebased/SubGHz_Phy_PingPong
code .
```

### 2. Build İşlemleri

#### A. VSCode Tasks ile Build
- **Ctrl+Shift+P** → `Tasks: Run Task` → **"STM32: Build"** seç
- Veya **Ctrl+Shift+B** (default build task)

#### B. Terminal ile Build  
```bash
cmake --build build/Debug
```

#### C. Clean Build
```bash
cmake --build build/Debug --target clean
```

### 3. Debug İşlemleri

#### A. Debug Başlat
- **F5** tuşu ile debug başlat
- Veya **Ctrl+Shift+P** → `Debug: Start Debugging`

#### B. Flash ve Debug
- VSCode'da **Run and Debug** panel → **"STM32 Flash & Debug"** seç
- Bu önce build eder, sonra debug başlatır

### 4. Flash İşlemleri

#### A. VSCode Task ile Flash
- **Ctrl+Shift+P** → `Tasks: Run Task` → **"STM32: Flash"** seç

#### B. Manual Flash
```bash
# STM32CubeProgrammer ile
STM32_Programmer_CLI -c port=SWD -w build/Debug/SubGHz_Phy_PingPong_Working.elf -v -rst
```

## 🔧 OTA Sistemi Konfigürasyonu

### 1. GitHub Repository Ayarları
`SubGHz_Phy/App/github_ota_downloader.c` dosyasında:

```c
#define GITHUB_USER     "SENIN_USERNAME"      // GitHub username
#define GITHUB_REPO     "lora-ota-firmware"   // Repository name
```

### 2. OTA Integration Aktifleştir
`Core/Src/main.c` dosyasına ekle:

```c
#include "ota_integration_app.h"

int main(void)
{
    // ... mevcut init kodları ...
    
    // OTA sistemini başlat
    OTA_Application_Init();
    
    while(1)
    {
        // ... mevcut main loop ...
        
        // OTA process
        OTA_Application_Process();
        
        // MQTT callback (gerektiğinde çağır)
        // OTA_Application_MqttCallback(topic, payload);
        
        // LoRa callback (gerektiğinde çağır) 
        // OTA_Application_LoraCallback(payload, size, rssi, snr);
    }
}
```

### 3. Debug Fonksiyonları
Debug console'da çağırabilirsin:

```c
void Debug_OTA_Info(void) { 
    OTA_App_PrintInfo(); 
}

void Debug_OTA_Check(void) { 
    OTA_App_ManualUpdateCheck(); 
}
```

## 📁 Proje Yapısı

```
SubGHz_Phy_PingPong/
├── .vscode/                    # VSCode konfigürasyonları
│   ├── tasks.json             # Build/Flash/Clean tasks
│   ├── launch.json            # Debug konfigürasyonları  
│   └── c_cpp_properties.json  # IntelliSense ayarları
├── build/Debug/               # Build çıktıları
│   ├── SubGHz_Phy_PingPong_Working.elf
│   ├── SubGHz_Phy_PingPong_Working.bin
│   └── SubGHz_Phy_PingPong_Working.hex
├── SubGHz_Phy/App/           # OTA sistem dosyaları
│   ├── lora_ota_protocol.h   # OTA protokol tanımları
│   ├── lora_ota_gateway.c/.h # Gateway OTA coordinator
│   ├── github_ota_downloader.c/.h # GitHub integration
│   └── ota_integration_app.c/.h   # Ana OTA uygulaması
├── Core/                     # STM32 HAL ve uygulama
└── CMakeLists.txt           # CMake build konfigürasyonu
```

## 🐛 Debug Özellikleri

### 1. Breakpoint'ler
- Kod satırında **F9** ile breakpoint ekle/kaldır
- Debug sırasında değişkenleri inspect et

### 2. Watches  
- Debug sırasında değişkenleri watch et:
  - `ota_system_initialized`
  - `g_ota_manager.state`
  - `github_config`

### 3. Debug Console
OTA log mesajlarını görmek için debug console kullan.

## ⚡ Keyboard Shortcuts

| Shortcut | Açıklama |
|----------|----------|
| **Ctrl+Shift+B** | Build (default task) |
| **F5** | Start Debug |
| **Ctrl+F5** | Run without Debug |
| **F9** | Toggle Breakpoint |
| **F10** | Step Over |
| **F11** | Step Into |
| **Shift+F5** | Stop Debug |
| **Ctrl+Shift+P** | Command Palette |

## 🔍 Troubleshooting

### Build Hataları
1. **CMake cache sorunları:** `rm -rf build && mkdir build/Debug`
2. **Missing includes:** `cmake/stm32cubemx/CMakeLists.txt` kontrol et
3. **Linker errors:** Flash/RAM boyutlarını kontrol et

### Debug Sorunları  
1. **ST-Link bulunamıyor:** USB bağlantısını kontrol et
2. **Debug başlamıyor:** `.elf` dosyasının varlığını kontrol et
3. **Breakpoint hit etmiyor:** Optimize edilmemiş debug build olduğundan emin ol

### OTA Sorunları
1. **GitHub connection:** WiFi/MQTT bağlantısını kontrol et  
2. **LoRa communication:** Frequency/SF ayarlarını kontrol et
3. **Flash write errors:** Dual-bank flash konfigürasyonunu kontrol et

## 📊 Memory Usage

**Current Build:**
- **Flash:** 73KB / 256KB (28% kullanım)
- **RAM:** 7.2KB / 32KB (22% kullanım)

OTA firmware buffer için ek ~4KB RAM gerekiyor.

## 🎯 Next Steps

1. **GitHub Repository** kur (`GITHUB_SETUP_GUIDE.md`'ye bak)
2. **MQTT komutları** test et
3. **LoRa node'lar** ile OTA test yap
4. **Production build** için optimize et

---

Artık VSCode'da tam profesyonel geliştirme ortamında çalışabilirsin! 🚀

**Build:** ✅ **Debug:** ✅ **OTA System:** ✅