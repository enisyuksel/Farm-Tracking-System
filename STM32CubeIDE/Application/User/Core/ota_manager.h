/* ota_manager.h - OTA Update Manager */
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>

/**
 * @brief Initialize OTA manager
 */
void OTA_Init(void);

/**
 * @brief Start OTA update process
 * @param version Firmware version to download
 * @return 1 if OTA started successfully, 0 otherwise
 */
uint8_t OTA_Start(uint8_t version);

/**
 * @brief Check if OTA is currently active
 * @return 1 if OTA is active, 0 otherwise
 */
uint8_t OTA_IsActive(void);

/**
 * @brief Complete OTA update (called after update finishes)
 */
void OTA_Complete(void);

/**
 * @brief Get current OTA status message
 * @return Status string for debug
 */
const char* OTA_GetStatus(void);

#endif /* OTA_MANAGER_H */
