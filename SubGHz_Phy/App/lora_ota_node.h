/**
 ******************************************************************************
 * @file    lora_ota_node.h
 * @author  IQ Yazılım
 * @brief   LoRa OTA Node Receiver Header
 ******************************************************************************
 */

#ifndef __LORA_OTA_NODE_H__
#define __LORA_OTA_NODE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lora_ota_protocol.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief Node OTA Status
 */
typedef struct {
    bool session_active;        // OTA oturumu aktif mi?
    OTA_State_t state;          // Mevcut durum
    uint16_t received_packets;  // Alınan paket sayısı
    uint16_t total_packets;     // Toplam paket sayısı
    uint32_t flash_write_address; // Flash yazma adresi
    uint8_t progress;           // İlerleme yüzdesi (0-100)
} OTA_Node_Status_t;

/* Exported functions prototypes ---------------------------------------------*/

// Initialization
OTA_Error_t OTA_Node_Init(void);

// Packet Processing
OTA_Error_t OTA_Node_ProcessLoraPacket(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);

// Transfer Control
OTA_Error_t OTA_Node_CompleteTransfer(void);
void OTA_Node_AbortTransfer(void);

// Status
OTA_Node_Status_t OTA_Node_GetStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_OTA_NODE_H__ */