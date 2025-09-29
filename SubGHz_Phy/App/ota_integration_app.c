/**
 ******************************************************************************
 * @file    ota_integration_app.c
 * @author  IQ Yazılım
 * @brief   OTA Integration Application for CMake-based Project
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lora_ota_gateway.h"
#include "github_ota_downloader.h" 
#include "lora_ota_protocol.h"
#include "subghz_phy_app.h"
#include "EMPA_MqttAws.h"
#include "sys_app.h"
#include "stm32_timer.h"
#include <string.h>
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/
#define OTA_CHECK_INTERVAL_MS       (24 * 60 * 60 * 1000)  // 24 saat

/* Private variables ---------------------------------------------------------*/
static UTIL_TIMER_Object_t ota_check_timer;
static bool ota_system_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static void OTA_App_Init(void);
static void OTA_App_TimerCallback(void *context);
static void OTA_App_HandleMqttMessage(const char* topic, const char* payload);
static void OTA_App_TestSequence(void);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief OTA Application'ını başlat
 */
void OTA_Application_Init(void)
{
    APP_LOG(TS_ON, VLEVEL_L, "\n\r=== OTA System Initialization ===\n\r");
    
    OTA_App_Init();
    
    APP_LOG(TS_ON, VLEVEL_L, "=== OTA System Ready ===\n\r\n\r");
}

/**
 * @brief OTA Application main loop
 */
void OTA_Application_Process(void)
{
    // OTA sistem durumunu kontrol et
    if (ota_system_initialized) {
        // GitHub check timer'ını sürekli çalıştır
        // Timer callback'i otomatik çalışacak
    }
}

/**
 * @brief MQTT message callback
 */
void OTA_Application_MqttCallback(const char* topic, const char* payload)
{
    if (!ota_system_initialized) return;
    
    OTA_App_HandleMqttMessage(topic, payload);
}

/**
 * @brief LoRa packet callback
 */
void OTA_Application_LoraCallback(const uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (!ota_system_initialized) return;
    
    // OTA paketlerini kontrol et
    if (size >= sizeof(OTA_PacketHeader_t)) {
        const OTA_PacketHeader_t* header = (const OTA_PacketHeader_t*)payload;
        
        // OTA paketi mi?
        if (header->packet_type >= OTA_PKT_INIT && header->packet_type <= OTA_PKT_RESET) {
            APP_LOG(TS_ON, VLEVEL_L, "OTA packet received: type=0x%02X, from node=%d, RSSI=%d\n\r",
                    header->packet_type, header->node_id, rssi);
            
            // Gateway'e ilet
            OTA_Gateway_ProcessLoraPacket(payload, size, header->node_id);
        }
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief OTA sistemini başlat
 */
static void OTA_App_Init(void)
{
    // 1. OTA Gateway'i başlat
    if (OTA_Gateway_Init() == OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "✓ OTA Gateway initialized\n\r");
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "✗ OTA Gateway initialization failed\n\r");
        return;
    }
    
    // 2. GitHub OTA downloader'ı başlat
    GitHub_OTA_Config_t github_config = {
        .github_user = "IQ-Yazilim",            // BURAYA GITHUB USERNAME YAZ
        .github_repo = "lora-ota-firmware",     // BURAYA REPO NAME YAZ
        .github_branch = "main",
        .firmware_path = "firmware/node",
        .check_interval_ms = OTA_CHECK_INTERVAL_MS,
        .auto_download_enabled = true,
        .mandatory_updates_only = false
    };
    
    if (GitHub_OTA_Init(&github_config) == OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "✓ GitHub OTA initialized for %s/%s\n\r", 
                github_config.github_user, github_config.github_repo);
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "✗ GitHub OTA initialization failed\n\r");
    }
    
    // 3. Periyodik GitHub check timer'ını başlat
    UTIL_TIMER_Create(&ota_check_timer, OTA_CHECK_INTERVAL_MS, UTIL_TIMER_PERIODIC, 
                      OTA_App_TimerCallback, NULL);
    UTIL_TIMER_Start(&ota_check_timer);
    
    ota_system_initialized = true;
    
    // 4. Test sequence'ini başlat (isteğe bağlı)
    #ifdef OTA_ENABLE_TEST_SEQUENCE
    OTA_App_TestSequence();
    #endif
    
    APP_LOG(TS_ON, VLEVEL_L, "✓ OTA periodic check started (every 24h)\n\r");
}

/**
 * @brief GitHub check timer callback
 */
static void OTA_App_TimerCallback(void *context)
{
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Scheduled GitHub firmware check...\n\r");
    
    // GitHub'dan firmware update kontrol et
    GitHub_OTA_CheckUpdates();
}

/**
 * @brief MQTT message handler
 */
static void OTA_App_HandleMqttMessage(const char* topic, const char* payload)
{
    if (!topic || !payload) return;
    
    APP_LOG(TS_ON, VLEVEL_L, "MQTT OTA message: %s = %s\n\r", topic, payload);
    
    // OTA komutları
    if (strstr(topic, "ota/command")) {
        if (strstr(payload, "check_updates")) {
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Manual update check requested\n\r");
            GitHub_OTA_CheckUpdates();
        }
        else if (strstr(payload, "start_ota")) {
            APP_LOG(TS_ON, VLEVEL_L, "OTA: Manual OTA start requested\n\r");
            
            // payload'dan firmware bilgilerini parse et
            // Bu kısmı GitHub downloader handle eder
            // Burada sadece log
        }
        else if (strstr(payload, "status")) {
            OTA_Gateway_Status_t status = OTA_Gateway_GetStatus();
            
            char status_msg[256];
            snprintf(status_msg, sizeof(status_msg),
                     "{"
                     "\"session_active\":%s,"
                     "\"total_nodes\":%d,"
                     "\"current_packet\":%d,"
                     "\"total_packets\":%d,"
                     "\"progress\":%d"
                     "}",
                     status.session_active ? "true" : "false",
                     status.total_nodes,
                     status.current_packet,
                     status.total_packets,
                     status.overall_progress);
            
            // MQTT ile status'u geri gönder
            mqttPublish("ota/status", status_msg);
        }
    }
    
    // Config komutları
    else if (strstr(topic, "ota/config")) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Config update received\n\r");
        // Config update'i burada handle et
    }
}

/**
 * @brief Test sequence (opsiyonel)
 */
static void OTA_App_TestSequence(void)
{
    APP_LOG(TS_ON, VLEVEL_L, "\n\r=== OTA Test Sequence ===\n\r");
    
    // 1. GitHub connection test
    APP_LOG(TS_ON, VLEVEL_L, "1. Testing GitHub connection...\n\r");
    GitHub_OTA_CheckUpdates();
    
    // 2. MQTT test  
    APP_LOG(TS_ON, VLEVEL_L, "2. Testing MQTT...\n\r");
    char test_msg[] = "{\"test\":\"ota_system_ready\"}";
    mqttPublish("ota/test", test_msg);
    
    // 3. LoRa test (fake OTA INIT packet)
    APP_LOG(TS_ON, VLEVEL_L, "3. Testing LoRa OTA packet creation...\n\r");
    
    uint8_t test_nodes[] = {0x01, 0x02, 0x03};
    OTA_Error_t result = OTA_Gateway_StartSession(test_nodes, 3, 65536);
    
    if (result == OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "✓ Test OTA session created\n\r");
        
        // Hemen abort et (test amaçlı)
        // OTA_Gateway_AbortSession("Test completed");
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "✗ Test OTA session failed: %d\n\r", result);
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "=== OTA Test Sequence Completed ===\n\r\n\r");
}

/* Debug and Utility Functions -----------------------------------------------*/

/**
 * @brief OTA sistem bilgilerini yazdır
 */
void OTA_App_PrintInfo(void)
{
    APP_LOG(TS_ON, VLEVEL_L, "\n\r=== OTA System Information ===\n\r");
    APP_LOG(TS_ON, VLEVEL_L, "Initialized: %s\n\r", ota_system_initialized ? "Yes" : "No");
    
    if (ota_system_initialized) {
        OTA_Gateway_Status_t status = OTA_Gateway_GetStatus();
        
        APP_LOG(TS_ON, VLEVEL_L, "Gateway Status:\n\r");
        APP_LOG(TS_ON, VLEVEL_L, "  - Session Active: %s\n\r", status.session_active ? "Yes" : "No");
        APP_LOG(TS_ON, VLEVEL_L, "  - Total Nodes: %d\n\r", status.total_nodes);
        APP_LOG(TS_ON, VLEVEL_L, "  - Progress: %d/%d (%d%%)\n\r", 
                status.current_packet, status.total_packets, status.overall_progress);
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "=== End of OTA Information ===\n\r\n\r");
}

/**
 * @brief Manuel GitHub update check
 */
void OTA_App_ManualUpdateCheck(void)
{
    if (ota_system_initialized) {
        APP_LOG(TS_ON, VLEVEL_L, "Manual GitHub update check initiated\n\r");
        GitHub_OTA_CheckUpdates();
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "OTA system not initialized\n\r");
    }
}

/* Integration with existing main.c ------------------------------------------*/

/*
Ana main.c dosyasına şunları ekle:

#include "ota_integration_app.h"

int main(void)
{
    // ... existing init code ...
    
    // OTA sistemi başlat
    OTA_Application_Init();
    
    while(1)
    {
        // ... existing main loop ...
        
        // OTA process
        OTA_Application_Process();
        
        // MQTT mesajları geldiğinde:
        // OTA_Application_MqttCallback(topic, payload);
        
        // LoRa paketleri geldiğinde: 
        // OTA_Application_LoraCallback(payload, size, rssi, snr);
    }
}

// Debug komutları için:
void Debug_OTA_Info(void) { OTA_App_PrintInfo(); }
void Debug_OTA_Check(void) { OTA_App_ManualUpdateCheck(); }

*/