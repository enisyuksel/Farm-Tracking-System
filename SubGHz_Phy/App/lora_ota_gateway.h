/**
 ******************************************************************************
 * @file    lora_ota_gateway.h
 * @author  IQ Yazılım
 * @brief   LoRa OTA Gateway Coordinator Header
 ******************************************************************************
 */

#ifndef __LORA_OTA_GATEWAY_H__
#define __LORA_OTA_GATEWAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lora_ota_protocol.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief Gateway OTA Status
 */
typedef struct {
    bool session_active;        // Oturum aktif mi?
    uint8_t total_nodes;        // Toplam node sayısı
    uint8_t completed_nodes;    // Tamamlanan node sayısı
    uint8_t failed_nodes;       // Başarısız node sayısı
    uint16_t current_packet;    // Mevcut paket indeksi
    uint16_t total_packets;     // Toplam paket sayısı
    uint8_t overall_progress;   // Genel ilerleme (%)
} OTA_Gateway_Status_t;

/* Exported functions prototypes ---------------------------------------------*/

// Initialization
OTA_Error_t OTA_Gateway_Init(void);

// Session Management
OTA_Error_t OTA_Gateway_StartSession(uint8_t* target_nodes, uint8_t node_count, uint32_t firmware_size);
OTA_Error_t OTA_Gateway_AbortSession(const char* reason);

// Communication
OTA_Error_t OTA_Gateway_ProcessLoraPacket(const uint8_t* payload, uint16_t size, uint8_t sender_id);

// Status
OTA_Gateway_Status_t OTA_Gateway_GetStatus(void);
bool OTA_Gateway_IsSessionActive(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_OTA_GATEWAY_H__ */