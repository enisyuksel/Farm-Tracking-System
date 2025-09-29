/**
 ******************************************************************************
 * @file    ota_node_integration_app.h
 * @author  IQ Yazılım
 * @brief   OTA Node Integration Application Header
 ******************************************************************************
 */

#ifndef __OTA_NODE_INTEGRATION_APP_H__
#define __OTA_NODE_INTEGRATION_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported functions prototypes ---------------------------------------------*/

// Main OTA Node Application Functions
void OTA_Node_Application_Init(void);
void OTA_Node_Application_Process(void);

// Callback Functions (called from main application)  
void OTA_Node_Application_LoraCallback(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);

// Debug and Utility Functions
void OTA_Node_App_PrintInfo(void);
void OTA_Node_App_ManualAbort(void);

/* Node Configuration */
// Node ID'sini değiştirmek için ota_node_integration_app.c'deki NODE_ID'yi düzenle

#ifdef __cplusplus
}
#endif

#endif /* __OTA_NODE_INTEGRATION_APP_H__ */