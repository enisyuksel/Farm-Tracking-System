/**
 ******************************************************************************
 * @file    lora_ota_protocol.h
 * @author  IQ Yazılım
 * @brief   LoRa Over-The-Air Update Protocol Definitions
 ******************************************************************************
 * @attention
 * 
 * Bu dosya LoRa üzerinden firmware güncelleme protokolünü tanımlar.
 * Gateway ve Node arasında güvenilir firmware transferi sağlar.
 * 
 ******************************************************************************
 */

#ifndef __LORA_OTA_PROTOCOL_H__
#define __LORA_OTA_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* OTA Protocol Definitions --------------------------------------------------*/

// OTA Packet Types
typedef enum {
    OTA_PKT_INIT        = 0xF0,  // OTA başlatma komutu
    OTA_PKT_START       = 0xF1,  // Firmware transferi başlangıcı
    OTA_PKT_DATA        = 0xF2,  // Firmware veri paketi
    OTA_PKT_END         = 0xF3,  // Firmware transferi sonu
    OTA_PKT_ACK         = 0xF4,  // Onay paketi
    OTA_PKT_NACK        = 0xF5,  // Olumsuz onay (retry gerekli)
    OTA_PKT_ABORT       = 0xF6,  // İptal komutu
    OTA_PKT_STATUS      = 0xF7,  // Durum sorgulama
    OTA_PKT_RESET       = 0xF8   // Node reset komutu
} OTA_PacketType_t;

// OTA States
typedef enum {
    OTA_STATE_IDLE      = 0x00,  // Beklemede
    OTA_STATE_INIT      = 0x01,  // Başlatılıyor
    OTA_STATE_READY     = 0x02,  // Hazır
    OTA_STATE_RECEIVING = 0x03,  // Alıyor
    OTA_STATE_COMPLETE  = 0x04,  // Tamamlandı
    OTA_STATE_ERROR     = 0x05,  // Hata
    OTA_STATE_TIMEOUT   = 0x06   // Zaman aşımı
} OTA_State_t;

// Error Codes
typedef enum {
    OTA_ERR_NONE        = 0x00,  // Hata yok
    OTA_ERR_CHECKSUM    = 0x01,  // Checksum hatası
    OTA_ERR_SEQUENCE    = 0x02,  // Sıra hatası
    OTA_ERR_SIZE        = 0x03,  // Boyut hatası
    OTA_ERR_TIMEOUT     = 0x04,  // Zaman aşımı
    OTA_ERR_FLASH       = 0x05,  // Flash yazma hatası
    OTA_ERR_MEMORY      = 0x06,  // Bellek hatası
    OTA_ERR_INVALID     = 0x07   // Geçersiz paket
} OTA_Error_t;

/* Protocol Constants --------------------------------------------------------*/
#define OTA_MAX_DATA_SIZE           230     // LoRa paketi maksimum veri boyutu
#define OTA_MAX_RETRIES             3       // Maksimum yeniden deneme
#define OTA_TIMEOUT_MS              5000    // Paket timeout (5 saniye)
#define OTA_BROADCAST_TIMEOUT_MS    10000   // Broadcast timeout (10 saniye)
#define OTA_MAX_NODES               10      // Maksimum node sayısı
#define OTA_FIRMWARE_MAX_SIZE       (128 * 1024)  // 128KB maksimum firmware

// Flash Memory Layout (STM32WL55 için)
#define OTA_FLASH_START_ADDR        0x08020000  // Bank 2 başlangıcı
#define OTA_FLASH_END_ADDR          0x0803FFFF  // Bank 2 sonu
#define OTA_FLASH_PAGE_SIZE         2048        // Flash page boyutu
#define OTA_BOOTLOADER_FLAG_ADDR    0x0803F800  // Bootloader flag adresi

/* Packet Structures ---------------------------------------------------------*/

// Common packet header (tüm paketlerde)
typedef struct __attribute__((packed)) {
    uint8_t packet_type;        // Paket tipi (OTA_PacketType_t)
    uint8_t node_id;            // Hedef/kaynak node ID (0xFF = broadcast)
    uint16_t sequence;          // Sıra numarası
    uint16_t total_packets;     // Toplam paket sayısı (sadece DATA paketlerinde)
    uint8_t data_length;        // Veri uzunluğu
    uint8_t checksum;           // Header checksum (CRC8)
} OTA_PacketHeader_t;

// OTA Init Packet - Gateway'den node'lara
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint32_t firmware_size;     // Firmware toplam boyutu
    uint16_t firmware_version;  // Yeni firmware versiyonu
    uint16_t firmware_crc16;    // Firmware CRC16
    uint8_t target_nodes[10]; // Hedef node ID'leri (0xFF ile sonlandırılır)
    uint32_t timestamp;         // OTA başlatma zamanı
} OTA_InitPacket_t;

// OTA Start Packet - Her node'a spesifik
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint32_t firmware_size;     // Firmware boyutu
    uint16_t total_packets;     // Toplam paket sayısı
    uint16_t packet_size;       // Her paket boyutu
    uint32_t flash_addr;        // Flash yazılacak adres
} OTA_StartPacket_t;

// OTA Data Packet - Firmware veri paketi
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint16_t packet_index;      // Paket indeksi (0'dan başlar)
    uint8_t data[OTA_MAX_DATA_SIZE]; // Firmware verisi
    uint16_t data_crc16;        // Veri CRC16
} OTA_DataPacket_t;

// OTA End Packet - Transfer sonu
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint32_t total_size;        // Toplam transfer boyutu
    uint16_t firmware_crc16;    // Tüm firmware CRC16
    uint8_t verification_result; // Doğrulama sonucu
} OTA_EndPacket_t;

// OTA ACK/NACK Packet - Onay paketi
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint8_t ack_type;           // ACK = 1, NACK = 0
    uint16_t ack_sequence;      // Onaylanacak paket sequence
    uint8_t error_code;         // Hata kodu (NACK durumunda)
    uint16_t next_expected;     // Bir sonraki beklenen paket (NACK için)
} OTA_AckPacket_t;

// OTA Status Packet - Durum paketi
typedef struct __attribute__((packed)) {
    OTA_PacketHeader_t header;
    uint8_t current_state;      // Mevcut durum (OTA_State_t)
    uint16_t received_packets;  // Alınan paket sayısı
    uint16_t missing_packets;   // Eksik paket sayısı
    uint8_t battery_level;      // Batarya seviyesi (%)
    uint32_t free_memory;       // Boş bellek (bytes)
} OTA_StatusPacket_t;

/* OTA Management Structures -------------------------------------------------*/

// Node OTA Info - Gateway'de her node için
typedef struct {
    uint8_t node_id;            // Node ID
    OTA_State_t state;          // Mevcut durum
    uint16_t received_packets;  // Alınan paket sayısı
    uint16_t total_packets;     // Toplam paket sayısı
    uint8_t retry_count;        // Yeniden deneme sayısı
    uint32_t last_activity;     // Son aktivite zamanı
    bool active;                // Node aktif mi?
} OTA_NodeInfo_t;

// Gateway OTA Manager
typedef struct {
    OTA_State_t state;          // Genel durum
    uint8_t *firmware_buffer;   // Firmware buffer
    uint32_t firmware_size;     // Firmware boyutu
    uint16_t firmware_crc16;    // Firmware CRC
    uint16_t total_packets;     // Toplam paket sayısı
    uint16_t current_packet;    // Mevcut paket indeksi
    OTA_NodeInfo_t nodes[10]; // Node bilgileri
    uint8_t active_node_count;  // Aktif node sayısı
    uint32_t session_start;     // OTA oturumu başlangıcı
    bool broadcast_mode;        // Broadcast modunda mı?
} OTA_Manager_t;

// Node OTA Handler
typedef struct {
    OTA_State_t state;          // Mevcut durum
    uint8_t *receive_buffer;    // Alma buffer'ı
    uint32_t buffer_size;       // Buffer boyutu
    uint32_t received_size;     // Alınan boyut
    uint16_t expected_sequence; // Beklenen sequence
    uint16_t total_packets;     // Toplam paket sayısı
    uint16_t received_packets;  // Alınan paket sayısı
    uint32_t session_start;     // Oturum başlangıcı
    uint32_t flash_write_addr;  // Flash yazma adresi
    bool session_active;        // Oturum aktif mi?
} OTA_NodeHandler_t;

/* Function Prototypes -------------------------------------------------------*/

// CRC Calculation
uint8_t OTA_CalculateCRC8(const uint8_t *data, uint16_t length);
uint16_t OTA_CalculateCRC16(const uint8_t *data, uint32_t length);

// Packet Creation and Validation
bool OTA_CreateInitPacket(OTA_InitPacket_t *packet, uint8_t *target_nodes, 
                         uint32_t fw_size, uint16_t fw_version, uint16_t fw_crc);
bool OTA_CreateDataPacket(OTA_DataPacket_t *packet, uint8_t node_id, 
                         uint16_t sequence, uint16_t packet_index, 
                         uint8_t *data, uint16_t data_len);
bool OTA_CreateAckPacket(OTA_AckPacket_t *packet, uint8_t node_id, 
                        uint16_t sequence, bool is_ack, uint8_t error_code);

bool OTA_ValidatePacket(const void *packet, uint16_t packet_size);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_OTA_PROTOCOL_H__ */