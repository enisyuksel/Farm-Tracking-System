/**
 ******************************************************************************
 * @file    ota_node_integration_app.c
 * @author  IQ Yazılım
 * @brief   OTA Node Integration Application
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lora_ota_node.h"
#include "lora_ota_protocol.h"
#include "subghz_phy_app.h"
#include "sys_app.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define NODE_ID                     0x01    // Bu node'un ID'si (S-01)

/* Private variables ---------------------------------------------------------*/
static bool node_ota_system_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static void OTA_Node_App_Init(void);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief OTA Node Application'ını başlat
 */
void OTA_Node_Application_Init(void)
{
    APP_LOG(TS_ON, VLEVEL_L, "\n\r=== OTA Node System Initialization ===\n\r");
    
    OTA_Node_App_Init();
    
    APP_LOG(TS_ON, VLEVEL_L, "=== OTA Node System Ready (ID: S-%02d) ===\n\r\n\r", NODE_ID);
}

/**
 * @brief OTA Node Application process
 */
void OTA_Node_Application_Process(void)
{
    // Node OTA sistem işlemleri
    if (node_ota_system_initialized) {
        // Status monitoring, timeouts, vs. burada handle edilir
    }
}

/**
 * @brief LoRa packet callback (Node version)
 */
void OTA_Node_Application_LoraCallback(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (!node_ota_system_initialized) return;
    
    // OTA paketlerini kontrol et
    if (size >= sizeof(OTA_PacketHeader_t)) {
        const OTA_PacketHeader_t* header = (const OTA_PacketHeader_t*)payload;
        
        // OTA paketi mi?
        if (header->packet_type >= OTA_PKT_INIT && header->packet_type <= OTA_PKT_RESET) {
            
            // Bu node'a mı gönderilmiş? (Broadcast veya bu node ID)
            if (header->node_id == 0xFF || header->node_id == NODE_ID) {
                APP_LOG(TS_ON, VLEVEL_L, "OTA packet for node: type=0x%02X, seq=%d, RSSI=%d\n\r",
                        header->packet_type, header->sequence, rssi);
                
                // Node'a işle
                OTA_Node_ProcessLoraPacket(payload, size, rssi, snr);
            }
        }
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief OTA Node sistemini başlat
 */
static void OTA_Node_App_Init(void)
{
    // 1. OTA Node'u başlat
    if (OTA_Node_Init() == OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "✓ OTA Node initialized (ID: %d)\n\r", NODE_ID);
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "✗ OTA Node initialization failed\n\r");
        return;
    }
    
    node_ota_system_initialized = true;
    
    APP_LOG(TS_ON, VLEVEL_L, "✓ OTA Node ready to receive firmware updates\n\r");
}

/* Debug and Utility Functions -----------------------------------------------*/

/**
 * @brief OTA Node sistem bilgilerini yazdır
 */
void OTA_Node_App_PrintInfo(void)
{
    APP_LOG(TS_ON, VLEVEL_L, "\n\r=== OTA Node Information ===\n\r");
    APP_LOG(TS_ON, VLEVEL_L, "Node ID: S-%02d\n\r", NODE_ID);
    APP_LOG(TS_ON, VLEVEL_L, "Initialized: %s\n\r", node_ota_system_initialized ? "Yes" : "No");
    
    if (node_ota_system_initialized) {
        OTA_Node_Status_t status = OTA_Node_GetStatus();
        
        APP_LOG(TS_ON, VLEVEL_L, "Node Status:\n\r");
        APP_LOG(TS_ON, VLEVEL_L, "  - Session Active: %s\n\r", status.session_active ? "Yes" : "No");
        APP_LOG(TS_ON, VLEVEL_L, "  - State: %d\n\r", status.state);
        APP_LOG(TS_ON, VLEVEL_L, "  - Progress: %d/%d (%d%%)\n\r", 
                status.received_packets, status.total_packets, status.progress);
        APP_LOG(TS_ON, VLEVEL_L, "  - Flash Address: 0x%08lX\n\r", status.flash_write_address);
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "=== End of OTA Node Information ===\n\r\n\r");
}

/**
 * @brief Manuel OTA abort
 */
void OTA_Node_App_ManualAbort(void)
{
    if (node_ota_system_initialized) {
        APP_LOG(TS_ON, VLEVEL_L, "Manual OTA abort initiated\n\r");
        OTA_Node_AbortTransfer();
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "OTA Node system not initialized\n\r");
    }
}

/* Integration with existing main.c ------------------------------------------*/

/*
Node main.c dosyasına şunları ekle:

// Gateway için:
#include "ota_integration_app.h"

// Node için:
#include "ota_node_integration_app.h"

int main(void)
{
    // ... existing init code ...
    
    // Gateway version:
    // OTA_Application_Init();
    
    // Node version:
    OTA_Node_Application_Init();
    
    while(1)
    {
        // ... existing main loop ...
        
        // Gateway version:
        // OTA_Application_Process();
        
        // Node version:
        OTA_Node_Application_Process();
        
        // LoRa paketleri geldiğinde:
        // Gateway version: 
        // OTA_Application_LoraCallback(payload, size, rssi, snr);
        
        // Node version:
        // OTA_Node_Application_LoraCallback(payload, size, rssi, snr);
    }
}

// Debug komutları için:
// Gateway version:
void Debug_OTA_Info(void) { OTA_App_PrintInfo(); }

// Node version:
void Debug_OTA_Info(void) { OTA_Node_App_PrintInfo(); }
void Debug_OTA_Abort(void) { OTA_Node_App_ManualAbort(); }

*/