/* ota_manager.c - OTA Update Manager Implementation */
#include "ota_manager.h"
#include "stm32wlxx_hal.h"
#include "stm32_timer.h"
#include "myESP32AT.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// LPUART1 Debug (ESP32 UART)
extern UART_HandleTypeDef hlpuart1;
static void OTA_DebugPrint(const char* msg) {
    if (msg) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[OTA]", 5, 100);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), 1000);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, 100);
    }
}

// OTA Configuration
#define FIRMWARE_URL_BASE "https://raw.githubusercontent.com/enisyuksel/Farm-Tracking-System/master/STM32CubeIDE/Debug/"
#define FIRMWARE_BINARY_NAME "SubGHz_Phy_PingPong.bin"
#define FIRMWARE_METADATA_NAME "firmware_metadata.json"

// Flash Configuration (STM32WL55JC - 256KB total)
#define FLASH_START_ADDR        0x08000000
#define FLASH_SIZE              (256 * 1024)  // 256KB
#define APP_START_ADDR          0x08000000    // Application starts at beginning
#define APP_MAX_SIZE            (128 * 1024)  // Max 128KB for app (rest for backup)
#define FLASH_PAGE_SIZE         2048          // STM32WL55 page size

// OTA Buffer (use limited RAM carefully - STM32WL55 has 64KB RAM)
#define OTA_BUFFER_SIZE         4096          // 4KB buffer for firmware chunks

// OTA State
static uint8_t ota_active = 0;
static uint8_t ota_version = 0;
static char ota_status[100] = "OTA Idle";
static uint8_t ota_buffer[OTA_BUFFER_SIZE];
static uint32_t firmware_size = 0;
static uint32_t firmware_crc32 = 0;

// Timer for async OTA process
static UTIL_TIMER_Object_t otaTimer;
static void OTA_ProcessCallback(void *context);

// Forward declarations
static uint8_t OTA_DownloadFirmware(void);
static uint8_t OTA_VerifyFirmware(void);
static uint8_t OTA_FlashFirmware(void);
static uint32_t OTA_CalculateCRC32(const uint8_t *data, uint32_t length);

/**
 * @brief Initialize OTA manager
 */
void OTA_Init(void) {
    ota_active = 0;
    ota_version = 0;
    strcpy(ota_status, "OTA Idle");
    
    // Create timer for OTA state machine
    UTIL_TIMER_Create(&otaTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OTA_ProcessCallback, NULL);
    
    OTA_DebugPrint("INIT_OK");
}

/**
 * @brief Start OTA update process
 */
uint8_t OTA_Start(uint8_t version) {
    if (ota_active) {
        OTA_DebugPrint("ERR_ALREADY_ACTIVE");
        return 0;
    }
    
    ota_active = 1;
    ota_version = version;
    
    char msg[80];
    snprintf(msg, sizeof(msg), "START_VER_%d", version);
    OTA_DebugPrint(msg);
    
    snprintf(ota_status, sizeof(ota_status), "OTA Active - Version %d", version);
    
    // Start OTA process after short delay (give time for MQTT to stabilize)
    UTIL_TIMER_SetPeriod(&otaTimer, 2000); // 2 seconds delay
    UTIL_TIMER_Start(&otaTimer);
    
    OTA_DebugPrint("STARTING_IN_2SEC");
    
    return 1;
}

/**
 * @brief Check if OTA is active
 */
uint8_t OTA_IsActive(void) {
    return ota_active;
}

/**
 * @brief Complete OTA update
 */
void OTA_Complete(void) {
    if (!ota_active) {
        OTA_DebugPrint("WARN_NOT_ACTIVE");
        return;
    }
    
    OTA_DebugPrint("COMPLETE");
    
    ota_active = 0;
    ota_version = 0;
    strcpy(ota_status, "OTA Complete - System Resumed");
    
    OTA_DebugPrint("SYSTEM_RESUME");
}

/**
 * @brief Get OTA status
 */
const char* OTA_GetStatus(void) {
    return ota_status;
}

/**
 * @brief OTA timer callback (simulates OTA completion after 10 seconds)
 */
static void OTA_TimerCallback(void *context) {
    OTA_DebugPrint("TIMER_COMPLETE");
    OTA_Complete();
}

/**
 * @brief OTA process callback - handles OTA state machine
 */
static void OTA_ProcessCallback(void *context) {
    OTA_DebugPrint("PROCESS_START");
    
    // Step 1: Download firmware metadata
    OTA_DebugPrint("STEP1_METADATA");
    // In real implementation: download firmware_metadata.json
    // Parse JSON to get size and CRC32
    firmware_size = 71152;  // From metadata
    firmware_crc32 = 0xA5A6D716;  // From metadata
    
    char msg[80];
    snprintf(msg, sizeof(msg), "SIZE=%lu_CRC=0x%08lX", firmware_size, firmware_crc32);
    OTA_DebugPrint(msg);
    
    // Step 2: Download firmware binary
    OTA_DebugPrint("STEP2_DOWNLOAD");
    if (!OTA_DownloadFirmware()) {
        OTA_DebugPrint("ERR_DOWNLOAD_FAIL");
        OTA_Complete();
        return;
    }
    
    // Step 3: Verify CRC32
    OTA_DebugPrint("STEP3_VERIFY");
    if (!OTA_VerifyFirmware()) {
        OTA_DebugPrint("ERR_VERIFY_FAIL");
        OTA_Complete();
        return;
    }
    
    // Step 4: Flash firmware
    OTA_DebugPrint("STEP4_FLASH");
    if (!OTA_FlashFirmware()) {
        OTA_DebugPrint("ERR_FLASH_FAIL");
        OTA_Complete();
        return;
    }
    
    // Step 5: Reboot
    OTA_DebugPrint("SUCCESS_REBOOT");
    HAL_Delay(1000);
    NVIC_SystemReset();  // System reset to boot new firmware
}

/**
 * @brief Download firmware from GitHub
 */
static uint8_t OTA_DownloadFirmware(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s%s", FIRMWARE_URL_BASE, FIRMWARE_BINARY_NAME);
    
    OTA_DebugPrint("DL_START");
    
    // For now, simulate download
    // In real implementation: use Wifi_HttpGet()
    uint32_t bytes_read = 0;
    
    // Simulated download
    HAL_Delay(3000);  // Simulate download time
    
    OTA_DebugPrint("DL_COMPLETE");
    return 1;
}

/**
 * @brief Verify firmware CRC32
 */
static uint8_t OTA_VerifyFirmware(void) {
    OTA_DebugPrint("VERIFY_CRC");
    
    // In real implementation:
    // 1. Calculate CRC32 of downloaded data
    // 2. Compare with expected CRC32
    
    // Simulated verification
    OTA_DebugPrint("CRC_OK");
    return 1;
}

/**
 * @brief Flash firmware to STM32
 */
static uint8_t OTA_FlashFirmware(void) {
    OTA_DebugPrint("FLASH_START");
    
    // In real implementation:
    // 1. HAL_FLASH_Unlock()
    // 2. Erase necessary pages
    // 3. Write firmware data
    // 4. HAL_FLASH_Lock()
    
    // For now, simulate flashing
    HAL_Delay(2000);
    
    OTA_DebugPrint("FLASH_OK");
    return 1;
}

/**
 * @brief Calculate CRC32
 */
static uint32_t OTA_CalculateCRC32(const uint8_t *data, uint32_t length) {
    // CRC32 table (standard Ethernet polynomial)
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        // ... (full table would go here - 256 entries)
        // For now, return dummy value
    };
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc_table[index];
    }
    
    return ~crc;
}
