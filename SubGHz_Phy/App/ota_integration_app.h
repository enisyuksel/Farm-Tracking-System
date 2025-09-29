/**
 ******************************************************************************
 * @file    ota_integration_app.h
 * @author  IQ Yazılım
 * @brief   OTA Integration Application Header
 ******************************************************************************
 */

#ifndef __OTA_INTEGRATION_APP_H__
#define __OTA_INTEGRATION_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported functions prototypes ---------------------------------------------*/

// Main OTA Application Functions
void OTA_Application_Init(void);
void OTA_Application_Process(void);

// Callback Functions (called from main application)
void OTA_Application_MqttCallback(const char* topic, const char* payload);
void OTA_Application_LoraCallback(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);

// Debug and Utility Functions  
void OTA_App_PrintInfo(void);
void OTA_App_ManualUpdateCheck(void);

/* Configuration */
// Test sequence'ini enable etmek için uncomment et
// #define OTA_ENABLE_TEST_SEQUENCE

#ifdef __cplusplus
}
#endif

#endif /* __OTA_INTEGRATION_APP_H__ */