/**
 ******************************************************************************
 * @file    lora_ota_gateway.c
 * @author  IQ Yazılım
 * @brief   LoRa OTA Gateway Coordinator Implementation (CMake version)
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lora_ota_gateway.h"
#include "lora_ota_protocol.h"
#include "subghz_phy_app.h"
#include "stm32_timer.h"
#include "stm32_seq.h"
#include "sys_app.h"
#include "radio.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define OTA_MAX_FIRMWARE_SIZE       (128 * 1024)  // 128KB

/* Private variables ---------------------------------------------------------*/
static OTA_Manager_t g_ota_manager = {0};
static uint8_t g_firmware_buffer[OTA_MAX_FIRMWARE_SIZE];

static UTIL_TIMER_Object_t ota_timer;
static volatile bool ota_session_active = false;

/* Private function prototypes -----------------------------------------------*/
static void OTA_Gateway_StateMachine(void);
static void OTA_Gateway_SendInitPacket(void);
static void OTA_Gateway_SendDataPacket(void);
static void OTA_Timer_Callback(void *context);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief OTA Gateway sistemini başlat
 */
OTA_Error_t OTA_Gateway_Init(void)
{
    // Manager'ı başlat
    memset(&g_ota_manager, 0, sizeof(OTA_Manager_t));
    g_ota_manager.state = OTA_STATE_IDLE;
    g_ota_manager.firmware_buffer = g_firmware_buffer;
    
    // Timer'ı oluştur
    UTIL_TIMER_Create(&ota_timer, 1000, UTIL_TIMER_ONESHOT, OTA_Timer_Callback, NULL);
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA Gateway initialized\n\r");
    return OTA_ERR_NONE;
}

/**
 * @brief OTA oturumunu başlat
 */
OTA_Error_t OTA_Gateway_StartSession(uint8_t* target_nodes, uint8_t node_count, uint32_t firmware_size)
{
    if (ota_session_active) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA session already active\n\r");
        return OTA_ERR_INVALID;
    }
    
    if (firmware_size > OTA_MAX_FIRMWARE_SIZE) {
        return OTA_ERR_SIZE;
    }
    
    // Manager'ı ayarla
    g_ota_manager.firmware_size = firmware_size;
    g_ota_manager.total_packets = (firmware_size + OTA_MAX_DATA_SIZE - 1) / OTA_MAX_DATA_SIZE;
    g_ota_manager.current_packet = 0;
    g_ota_manager.active_node_count = node_count;
    
    // Node'ları kaydet
    for (int i = 0; i < node_count && i < OTA_MAX_NODES; i++) {
        g_ota_manager.nodes[i].node_id = target_nodes[i];
        g_ota_manager.nodes[i].state = OTA_STATE_IDLE;
        g_ota_manager.nodes[i].active = true;
    }
    
    g_ota_manager.state = OTA_STATE_INIT;
    ota_session_active = true;
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA Session started: %lu bytes, %d packets, %d nodes\n\r", 
            firmware_size, g_ota_manager.total_packets, node_count);
    
    // State machine başlat
    OTA_Gateway_StateMachine();
    
    return OTA_ERR_NONE;
}

/**
 * @brief LoRa'dan gelen paketi işle
 */
OTA_Error_t OTA_Gateway_ProcessLoraPacket(const uint8_t* payload, uint16_t size, uint8_t sender_id)
{
    if (!ota_session_active || !payload) {
        return OTA_ERR_INVALID;
    }
    
    const OTA_PacketHeader_t* header = (const OTA_PacketHeader_t*)payload;
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA packet from node %d, type: 0x%02X\n\r", sender_id, header->packet_type);
    
    // ACK paketini işle
    if (header->packet_type == OTA_PKT_ACK) {
        // Node hazır, devam et
        OTA_Gateway_StateMachine();
    }
    
    return OTA_ERR_NONE;
}

/**
 * @brief OTA durumunu al
 */
OTA_Gateway_Status_t OTA_Gateway_GetStatus(void)
{
    OTA_Gateway_Status_t status = {0};
    
    status.session_active = ota_session_active;
    status.total_nodes = g_ota_manager.active_node_count;
    status.current_packet = g_ota_manager.current_packet;
    status.total_packets = g_ota_manager.total_packets;
    
    if (g_ota_manager.total_packets > 0) {
        status.overall_progress = (g_ota_manager.current_packet * 100) / g_ota_manager.total_packets;
    }
    
    return status;
}

/**
 * @brief Oturumun aktif olup olmadığını kontrol et
 */
bool OTA_Gateway_IsSessionActive(void)
{
    return ota_session_active;
}

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief OTA State Machine
 */
static void OTA_Gateway_StateMachine(void)
{
    switch (g_ota_manager.state) {
        case OTA_STATE_INIT:
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Sending INIT packet\n\r");
            OTA_Gateway_SendInitPacket();
            g_ota_manager.state = OTA_STATE_READY;
            UTIL_TIMER_Start(&ota_timer);
            break;
            
        case OTA_STATE_READY:
            // Node'lardan ACK bekle
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Waiting for nodes to be ready\n\r");
            g_ota_manager.state = OTA_STATE_RECEIVING;
            OTA_Gateway_SendDataPacket();
            break;
            
        case OTA_STATE_RECEIVING:
            // Data paketlerini gönder
            if (g_ota_manager.current_packet < g_ota_manager.total_packets) {
                OTA_Gateway_SendDataPacket();
            } else {
                g_ota_manager.state = OTA_STATE_COMPLETE;
                APP_LOG(TS_ON, VLEVEL_L, "OTA: Transfer completed\n\r");
                ota_session_active = false;
            }
            break;
            
        case OTA_STATE_COMPLETE:
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Session completed successfully\n\r");
            ota_session_active = false;
            break;
            
        case OTA_STATE_ERROR:
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Session failed\n\r");
            ota_session_active = false;
            break;
            
        default:
            break;
    }
}

/**
 * @brief INIT paketini gönder
 */
static void OTA_Gateway_SendInitPacket(void)
{
    OTA_InitPacket_t init_packet = {0};
    
    // Header oluştur
    init_packet.header.packet_type = OTA_PKT_INIT;
    init_packet.header.node_id = 0xFF; // Broadcast
    init_packet.header.sequence = 0;
    init_packet.header.data_length = sizeof(OTA_InitPacket_t) - sizeof(OTA_PacketHeader_t);
    
    // Init packet bilgileri
    init_packet.firmware_size = g_ota_manager.firmware_size;
    init_packet.firmware_version = 0x0200; // Version 2.0
    init_packet.firmware_crc16 = g_ota_manager.firmware_crc16;
    init_packet.timestamp = HAL_GetTick();
    
    // Target node'ları kopyala
    for (int i = 0; i < g_ota_manager.active_node_count && i < 10; i++) {
        init_packet.target_nodes[i] = g_ota_manager.nodes[i].node_id;
    }
    init_packet.target_nodes[g_ota_manager.active_node_count] = 0xFF; // Sonlandır
    
    // CRC hesapla
    init_packet.header.checksum = OTA_CalculateCRC8((uint8_t*)&init_packet.header, 
                                                   sizeof(OTA_PacketHeader_t) - 1);
    
    // LoRa ile gönder
    APP_LOG(TS_ON, VLEVEL_L, "Sending OTA INIT packet, firmware size: %lu\n\r", 
            init_packet.firmware_size);
    
    // LoRa üzerinden init paketi gönder
    Radio.Send((uint8_t*)&init_packet, sizeof(OTA_InitPacket_t));
}

/**
 * @brief Data paketini gönder
 */
static void OTA_Gateway_SendDataPacket(void)
{
    if (g_ota_manager.current_packet >= g_ota_manager.total_packets) {
        return;
    }
    
    OTA_DataPacket_t data_packet = {0};
    
    // Header oluştur
    data_packet.header.packet_type = OTA_PKT_DATA;
    data_packet.header.node_id = 0xFF; // Broadcast
    data_packet.header.sequence = g_ota_manager.current_packet + 1;
    data_packet.header.total_packets = g_ota_manager.total_packets;
    
    // Data packet bilgileri
    data_packet.packet_index = g_ota_manager.current_packet;
    
    // Veri kopyala
    uint32_t data_offset = g_ota_manager.current_packet * OTA_MAX_DATA_SIZE;
    uint32_t remaining_size = g_ota_manager.firmware_size - data_offset;
    uint16_t packet_data_size = (remaining_size > OTA_MAX_DATA_SIZE) ? 
                               OTA_MAX_DATA_SIZE : (uint16_t)remaining_size;
    
    memcpy(data_packet.data, &g_firmware_buffer[data_offset], packet_data_size);
    
    data_packet.header.data_length = packet_data_size + 4; // +4 for packet_index and crc16
    data_packet.data_crc16 = OTA_CalculateCRC16(data_packet.data, packet_data_size);
    
    // Header CRC hesapla
    data_packet.header.checksum = OTA_CalculateCRC8((uint8_t*)&data_packet.header, 
                                                   sizeof(OTA_PacketHeader_t) - 1);
    
    // LoRa ile gönder
    APP_LOG(TS_ON, VLEVEL_L, "Sending DATA packet %d/%d (%d bytes)\n\r", 
            g_ota_manager.current_packet + 1, g_ota_manager.total_packets, packet_data_size);
    
    uint16_t total_packet_size = sizeof(OTA_PacketHeader_t) + data_packet.header.data_length;
    Radio.Send((uint8_t*)&data_packet, total_packet_size);
    
    g_ota_manager.current_packet++;
    
    // Bir sonraki paket için timer ayarla
    if (g_ota_manager.current_packet < g_ota_manager.total_packets) {
        UTIL_TIMER_Start(&ota_timer);
    }
}

/**
 * @brief Timer callback
 */
static void OTA_Timer_Callback(void *context)
{
    OTA_Gateway_StateMachine();
}

/* CRC Implementation --------------------------------------------------------*/

/**
 * @brief CRC8 hesapla
 */
uint8_t OTA_CalculateCRC8(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief CRC16 hesapla
 */
uint16_t OTA_CalculateCRC16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Paket doğrula
 */
bool OTA_ValidatePacket(const void *packet, uint16_t packet_size)
{
    if (!packet || packet_size < sizeof(OTA_PacketHeader_t)) {
        return false;
    }
    
    const OTA_PacketHeader_t *header = (const OTA_PacketHeader_t*)packet;
    
    // CRC kontrolü
    uint8_t calculated_crc = OTA_CalculateCRC8((const uint8_t*)header, 
                                               sizeof(OTA_PacketHeader_t) - 1);
    
    return (calculated_crc == header->checksum);
}