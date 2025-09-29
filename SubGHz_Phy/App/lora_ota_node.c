/**
 ******************************************************************************
 * @file    lora_ota_node.c
 * @author  IQ Yazılım
 * @brief   LoRa OTA Node Receiver Implementation (CMake version)
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lora_ota_node.h"
#include "lora_ota_protocol.h"
#include "subghz_phy_app.h"
#include "stm32_timer.h"
#include "sys_app.h"
#include "radio.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define OTA_RECEIVE_BUFFER_SIZE     (4 * 1024)    // 4KB receive buffer
#define OTA_FLASH_WRITE_TIMEOUT     1000          // 1 second

// STM32WL55 Flash parameters
#define OTA_FLASH_BANK2_START       0x08020000    // Bank 2 start address
#define OTA_FLASH_BANK2_END         0x0803FFFF    // Bank 2 end address
#define OTA_FLASH_PAGE_SIZE         2048          // 2KB per page
#define OTA_PAGES_PER_BANK          64            // 64 pages in bank 2

// Bootloader communication
#define OTA_BOOTLOADER_FLAG_ADDR    0x0803F800    // Last page of bank 2
#define OTA_BOOTLOADER_MAGIC        0xDEADBEEF    // Magic number for OTA flag

/* Private types -------------------------------------------------------------*/
typedef struct {
    uint32_t magic;                 // Magic number
    uint32_t firmware_size;         // New firmware size
    uint32_t firmware_crc;          // New firmware CRC
    uint32_t source_addr;           // Source address (bank 2)
    uint32_t target_addr;           // Target address (bank 1)
    uint8_t  update_required;       // Update flag
    uint8_t  reserved[11];          // Padding to 32 bytes
} OTA_BootloaderFlag_t;

/* Private variables ---------------------------------------------------------*/
static OTA_NodeHandler_t g_node_handler = {0};
static uint8_t g_receive_buffer[OTA_RECEIVE_BUFFER_SIZE];
static uint8_t g_packet_bitmap[256];  // 256 * 8 = 2048 max packets support

static UTIL_TIMER_Object_t ota_response_timer;
static volatile bool ota_active = false;
static volatile bool flash_operation_busy = false;

/* Private function prototypes -----------------------------------------------*/
static OTA_Error_t OTA_Node_ProcessInitPacket(const OTA_InitPacket_t* packet);
static OTA_Error_t OTA_Node_ProcessDataPacket(const OTA_DataPacket_t* packet);
static OTA_Error_t OTA_Node_SendAck(uint16_t sequence, bool is_ack, uint8_t error_code);
static OTA_Error_t OTA_Node_WriteFlashPage(uint32_t address, const uint8_t* data, uint32_t size);
static void OTA_Response_Timer_Callback(void *context);

/* Private function prototypes -----------------------------------------------*/
static OTA_Error_t OTA_Node_WriteFlashPage(uint32_t address, const uint8_t* data, uint32_t size);
static void OTA_Node_WriteBootloaderFlag(const OTA_BootloaderFlag_t* flag);
static void OTA_Node_SetBitmap(uint16_t packet_index);
static bool OTA_Node_CheckBitmap(uint16_t packet_index);
static void OTA_Response_Timer_Callback(void *context);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief OTA Node sistemini başlat
 */
OTA_Error_t OTA_Node_Init(void)
{
    // Handler'ı başlat
    memset(&g_node_handler, 0, sizeof(OTA_NodeHandler_t));
    g_node_handler.state = OTA_STATE_IDLE;
    g_node_handler.receive_buffer = g_receive_buffer;
    g_node_handler.buffer_size = OTA_RECEIVE_BUFFER_SIZE;
    
    // Timer'ı oluştur
    UTIL_TIMER_Create(&ota_response_timer, 100, UTIL_TIMER_ONESHOT, 
                      OTA_Response_Timer_Callback, NULL);
    
    // Packet bitmap'i temizle
    memset(g_packet_bitmap, 0, sizeof(g_packet_bitmap));
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA Node initialized\n\r");
    return OTA_ERR_NONE;
}

/**
 * @brief LoRa'dan gelen OTA paketini işle
 */
OTA_Error_t OTA_Node_ProcessLoraPacket(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (!payload || size < sizeof(OTA_PacketHeader_t)) {
        return OTA_ERR_INVALID;
    }
    
    const OTA_PacketHeader_t* header = (const OTA_PacketHeader_t*)payload;
    
    // Paket doğrulaması
    if (!OTA_ValidatePacket(payload, size)) {
        APP_LOG(TS_ON, VLEVEL_L, "Invalid OTA packet, CRC failed\n\r");
        return OTA_ERR_CHECKSUM;
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA packet received: type=0x%02X, seq=%d, RSSI=%d\n\r",
            header->packet_type, header->sequence, rssi);
    
    // Packet type'a göre işle
    switch (header->packet_type) {
        case OTA_PKT_INIT:
            return OTA_Node_ProcessInitPacket((const OTA_InitPacket_t*)payload);
            
        case OTA_PKT_DATA:
            if (ota_active) {
                return OTA_Node_ProcessDataPacket((const OTA_DataPacket_t*)payload);
            }
            break;
            
        case OTA_PKT_END:
            if (ota_active) {
                APP_LOG(TS_ON, VLEVEL_L, "OTA END packet received\n\r");
                return OTA_Node_CompleteTransfer();
            }
            break;
            
        case OTA_PKT_ABORT:
            APP_LOG(TS_ON, VLEVEL_L, "OTA ABORT received\n\r");
            OTA_Node_AbortTransfer();
            break;
            
        default:
            return OTA_ERR_INVALID;
    }
    
    return OTA_ERR_NONE;
}

/**
 * @brief OTA durumunu al
 */
OTA_Node_Status_t OTA_Node_GetStatus(void)
{
    OTA_Node_Status_t status = {0};
    
    status.session_active = ota_active;
    status.state = g_node_handler.state;
    status.received_packets = g_node_handler.received_packets;
    status.total_packets = g_node_handler.total_packets;
    status.flash_write_address = g_node_handler.flash_write_addr;
    
    if (g_node_handler.total_packets > 0) {
        status.progress = (g_node_handler.received_packets * 100) / g_node_handler.total_packets;
    }
    
    return status;
}

/**
 * @brief OTA transfer'ını tamamla
 */
OTA_Error_t OTA_Node_CompleteTransfer(void)
{
    if (!ota_active) {
        return OTA_ERR_INVALID;
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Completing transfer...\n\r");
    
    // Flash verification
    uint16_t calculated_crc = OTA_CalculateCRC16(g_receive_buffer, g_node_handler.received_size);
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Received %lu bytes, CRC=0x%04X\n\r",
            g_node_handler.received_size, calculated_crc);
    
    // Bootloader flag'ini yazalım
    OTA_BootloaderFlag_t bootloader_flag = {0};
    bootloader_flag.magic = OTA_BOOTLOADER_MAGIC;
    bootloader_flag.firmware_size = g_node_handler.received_size;
    bootloader_flag.firmware_crc = calculated_crc;
    bootloader_flag.source_addr = OTA_FLASH_BANK2_START;
    bootloader_flag.target_addr = 0x08000000; // Bank 1 start
    bootloader_flag.update_required = 1;
    
    OTA_Node_WriteBootloaderFlag(&bootloader_flag);
    
    // ACK gönder
    OTA_Node_SendAck(0, true, OTA_ERR_NONE);
    
    // Session'ı sonlandır
    ota_active = false;
    g_node_handler.state = OTA_STATE_COMPLETE;
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Transfer completed successfully!\n\r");
    APP_LOG(TS_ON, VLEVEL_L, "OTA: System will reset in 5 seconds...\n\r");
    
    // 5 saniye sonra reset
    UTIL_TIMER_SetPeriod(&ota_response_timer, 5000);
    UTIL_TIMER_Start(&ota_response_timer);
    
    return OTA_ERR_NONE;
}

/**
 * @brief OTA transfer'ını abort et
 */
void OTA_Node_AbortTransfer(void)
{
    if (ota_active) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Transfer aborted\n\r");
        
        ota_active = false;
        g_node_handler.state = OTA_STATE_ERROR;
        
        // Timer'ı durdur
        UTIL_TIMER_Stop(&ota_response_timer);
        
        // Buffer'ı temizle
        memset(g_receive_buffer, 0, OTA_RECEIVE_BUFFER_SIZE);
        memset(g_packet_bitmap, 0, sizeof(g_packet_bitmap));
        
        g_node_handler.received_packets = 0;
        g_node_handler.received_size = 0;
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief INIT paketini işle
 */
static OTA_Error_t OTA_Node_ProcessInitPacket(const OTA_InitPacket_t* packet)
{
    APP_LOG(TS_ON, VLEVEL_L, "OTA INIT: size=%lu, version=0x%04X\n\r",
            packet->firmware_size, packet->firmware_version);
    
    // Bu node'a gönderilmiş mi kontrol et
    bool target_found = false;
    for (int i = 0; i < 10 && packet->target_nodes[i] != 0xFF; i++) {
        if (packet->target_nodes[i] == 0x01) { // Bu node'un ID'si (örnek)
            target_found = true;
            break;
        }
    }
    
    if (!target_found) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Not targeted to this node\n\r");
        return OTA_ERR_NONE;
    }
    
    // Firmware size kontrolü
    if (packet->firmware_size > (OTA_FLASH_BANK2_END - OTA_FLASH_BANK2_START)) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Firmware too large\n\r");
        return OTA_ERR_SIZE;
    }
    
    // Session başlat
    ota_active = true;
    g_node_handler.state = OTA_STATE_READY;
    g_node_handler.session_active = true;
    g_node_handler.flash_write_addr = OTA_FLASH_BANK2_START;
    g_node_handler.total_packets = (packet->firmware_size + OTA_MAX_DATA_SIZE - 1) / OTA_MAX_DATA_SIZE;
    g_node_handler.received_packets = 0;
    g_node_handler.received_size = 0;
    
    // Bitmap'i temizle
    memset(g_packet_bitmap, 0, sizeof(g_packet_bitmap));
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Ready to receive %d packets\n\r", g_node_handler.total_packets);
    
    // ACK gönder
    return OTA_Node_SendAck(packet->header.sequence, true, OTA_ERR_NONE);
}

/**
 * @brief DATA paketini işle
 */
static OTA_Error_t OTA_Node_ProcessDataPacket(const OTA_DataPacket_t* packet)
{
    // Sequence kontrolü
    if (packet->packet_index >= g_node_handler.total_packets) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Invalid packet index %d\n\r", packet->packet_index);
        return OTA_ERR_SEQUENCE;
    }
    
    // Bu paketi daha önce aldık mı?
    if (OTA_Node_CheckBitmap(packet->packet_index)) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Duplicate packet %d\n\r", packet->packet_index);
        return OTA_Node_SendAck(packet->header.sequence, true, OTA_ERR_NONE); // Duplicate ACK
    }
    
    // Data CRC kontrolü
    uint16_t calculated_crc = OTA_CalculateCRC16(packet->data, packet->header.data_length - 4);
    if (calculated_crc != packet->data_crc16) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Data CRC mismatch for packet %d\n\r", packet->packet_index);
        return OTA_ERR_CHECKSUM;
    }
    
    // Buffer'a kopyala
    uint32_t buffer_offset = packet->packet_index * OTA_MAX_DATA_SIZE;
    uint16_t data_size = packet->header.data_length - 4; // -4 for packet_index and crc
    
    if (buffer_offset + data_size <= OTA_RECEIVE_BUFFER_SIZE) {
        memcpy(&g_receive_buffer[buffer_offset], packet->data, data_size);
        g_node_handler.received_size += data_size;
    }
    
    // Bitmap'e işaretle
    OTA_Node_SetBitmap(packet->packet_index);
    g_node_handler.received_packets++;
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Packet %d received (%d/%d)\n\r",
            packet->packet_index, g_node_handler.received_packets, g_node_handler.total_packets);
    
    // Flash'a yaz (her 4 paket bir kez)
    if (g_node_handler.received_packets % 4 == 0) {
        uint32_t flash_addr = OTA_FLASH_BANK2_START + (buffer_offset & ~(OTA_FLASH_PAGE_SIZE - 1));
        uint32_t page_offset = buffer_offset & (OTA_FLASH_PAGE_SIZE - 1);
        
        if (page_offset == 0) {
            OTA_Node_WriteFlashPage(flash_addr, &g_receive_buffer[buffer_offset], OTA_FLASH_PAGE_SIZE);
        }
    }
    
    // ACK gönder
    return OTA_Node_SendAck(packet->header.sequence, true, OTA_ERR_NONE);
}

/**
 * @brief ACK paketi gönder
 */
static OTA_Error_t OTA_Node_SendAck(uint16_t sequence, bool is_ack, uint8_t error_code)
{
    OTA_AckPacket_t ack_packet = {0};
    
    // Header oluştur
    ack_packet.header.packet_type = is_ack ? OTA_PKT_ACK : OTA_PKT_NACK;
    ack_packet.header.node_id = 0x01; // Bu node'un ID'si
    ack_packet.header.sequence = sequence;
    ack_packet.header.data_length = sizeof(OTA_AckPacket_t) - sizeof(OTA_PacketHeader_t);
    
    // ACK bilgileri
    ack_packet.ack_type = is_ack ? 1 : 0;
    ack_packet.ack_sequence = sequence;
    ack_packet.error_code = error_code;
    ack_packet.next_expected = g_node_handler.received_packets;
    
    // Header CRC hesapla
    ack_packet.header.checksum = OTA_CalculateCRC8((uint8_t*)&ack_packet.header, 
                                                   sizeof(OTA_PacketHeader_t) - 1);
    
    // LoRa ile gönder
    APP_LOG(TS_ON, VLEVEL_L, "Sending %s for seq %d\n\r", is_ack ? "ACK" : "NACK", sequence);
    
    // LoRa üzerinden ACK paketi gönder  
    Radio.Send((uint8_t*)&ack_packet, sizeof(OTA_AckPacket_t));
    
    return OTA_ERR_NONE;
}

/**
 * @brief Flash page yaz
 */
static OTA_Error_t OTA_Node_WriteFlashPage(uint32_t address, const uint8_t* data, uint32_t size)
{
    if (flash_operation_busy) {
        return OTA_ERR_FLASH;
    }
    
    flash_operation_busy = true;
    
    APP_LOG(TS_ON, VLEVEL_L, "Writing flash page at 0x%08lX (%lu bytes)\n\r", address, size);
    
    // Flash unlock
    HAL_FLASH_Unlock();
    
    // Page erase (STM32WL55 uses single flash with page addressing)
    FLASH_EraseInitTypeDef erase_init = {0};
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = ((address - OTA_FLASH_BANK2_START) / OTA_FLASH_PAGE_SIZE) + 64; // Bank2 offset
    erase_init.NbPages = 1;
    
    uint32_t page_error = 0;
    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        flash_operation_busy = false;
        return OTA_ERR_FLASH;
    }
    
    // Write data (64-bit aligned)
    for (uint32_t i = 0; i < size; i += 8) {
        uint64_t data64 = 0;
        memcpy(&data64, &data[i], (size - i >= 8) ? 8 : (size - i));
        
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, data64) != HAL_OK) {
            HAL_FLASH_Lock();
            flash_operation_busy = false;
            return OTA_ERR_FLASH;
        }
    }
    
    HAL_FLASH_Lock();
    flash_operation_busy = false;
    
    return OTA_ERR_NONE;
}

/**
 * @brief Bootloader flag'ini yaz
 */
static void OTA_Node_WriteBootloaderFlag(const OTA_BootloaderFlag_t* flag)
{
    HAL_FLASH_Unlock();
    
    // Erase flag page (STM32WL55)
    FLASH_EraseInitTypeDef erase_init = {0};
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = ((OTA_BOOTLOADER_FLAG_ADDR - OTA_FLASH_BANK2_START) / OTA_FLASH_PAGE_SIZE) + 64;
    erase_init.NbPages = 1;
    
    uint32_t page_error = 0;
    HAL_FLASHEx_Erase(&erase_init, &page_error);
    
    // Write flag
    for (uint32_t i = 0; i < sizeof(OTA_BootloaderFlag_t); i += 8) {
        uint64_t data64 = 0;
        memcpy(&data64, ((uint8_t*)flag) + i, 8);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, OTA_BOOTLOADER_FLAG_ADDR + i, data64);
    }
    
    HAL_FLASH_Lock();
    
    APP_LOG(TS_ON, VLEVEL_L, "Bootloader flag written\n\r");
}

/**
 * @brief Packet bitmap set
 */
static void OTA_Node_SetBitmap(uint16_t packet_index)
{
    if (packet_index < 2048) {
        uint16_t byte_index = packet_index / 8;
        uint8_t bit_index = packet_index % 8;
        g_packet_bitmap[byte_index] |= (1 << bit_index);
    }
}

/**
 * @brief Packet bitmap check
 */
static bool OTA_Node_CheckBitmap(uint16_t packet_index)
{
    if (packet_index < 2048) {
        uint16_t byte_index = packet_index / 8;
        uint8_t bit_index = packet_index % 8;
        return (g_packet_bitmap[byte_index] & (1 << bit_index)) != 0;
    }
    return false;
}

/**
 * @brief Timer callback
 */
static void OTA_Response_Timer_Callback(void *context)
{
    // 5 saniye sonra sistem reset
    if (g_node_handler.state == OTA_STATE_COMPLETE) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Performing system reset...\n\r");
        HAL_NVIC_SystemReset();
    }
}