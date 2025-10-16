/* ota_manager.c - OTA Update Manager Implementation */
#include "ota_manager.h"
#include "stm32wlxx_hal.h"
#include "stm32_timer.h"
#include <string.h>
#include <stdio.h>

// LPUART1 Debug (ESP32 UART)
extern UART_HandleTypeDef hlpuart1;
static void OTA_DebugPrint(const char* msg) {
    if (msg) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[OTA]", 5, 100);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), 1000);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, 100);
    }
}

// OTA State
static uint8_t ota_active = 0;
static uint8_t ota_version = 0;
static char ota_status[100] = "OTA Idle";

// Dummy OTA timer (simulates OTA process)
static UTIL_TIMER_Object_t otaTimer;
static void OTA_TimerCallback(void *context);

/**
 * @brief Initialize OTA manager
 */
void OTA_Init(void) {
    ota_active = 0;
    ota_version = 0;
    strcpy(ota_status, "OTA Idle");
    
    // Create timer for dummy OTA simulation
    UTIL_TIMER_Create(&otaTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OTA_TimerCallback, NULL);
    
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
    
    // Simulate OTA process: wait 10 seconds then complete
    // In real implementation, this is where you'd download firmware
    UTIL_TIMER_SetPeriod(&otaTimer, 10000); // 10 seconds
    UTIL_TIMER_Start(&otaTimer);
    
    OTA_DebugPrint("SIMULATING_10SEC");
    
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
