/**
 ******************************************************************************
 * @file    github_ota_downloader.c
 * @author  IQ Yazılım
 * @brief   GitHub OTA Firmware Downloader for ESP32 (CMake version)
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "github_ota_downloader.h"
#include "lora_ota_protocol.h"
#include "EMPA_MqttAws.h"
#include <string.h>
#include <stdio.h>

/* GitHub Configuration ------------------------------------------------------*/
#define GITHUB_RAW_BASE_URL     "https://raw.githubusercontent.com"
#define GITHUB_USER             "your-username"       // BURAYA GİTHUB USERNAME
#define GITHUB_REPO             "lora-ota-firmware"   // BURAYA REPO NAME
#define GITHUB_BRANCH           "main"
#define FIRMWARE_PATH           "firmware/node"

#define HTTP_RESPONSE_SIZE      4096
#define JSON_BUFFER_SIZE        2048

/* Private types -------------------------------------------------------------*/
typedef struct {
    char version[16];
    char download_url[256];
    uint32_t size;
    uint16_t crc16;
    bool mandatory;
} GitHub_FirmwareInfo_t;

/* Private variables ---------------------------------------------------------*/
static char http_response_buffer[HTTP_RESPONSE_SIZE];
static char json_buffer[JSON_BUFFER_SIZE];
static GitHub_OTA_Config_t github_config = {0};

/* Private function prototypes -----------------------------------------------*/
static OTA_Error_t GitHub_CheckForUpdates(GitHub_FirmwareInfo_t* firmware_info);
static OTA_Error_t GitHub_ParseReleaseJSON(const char* json, GitHub_FirmwareInfo_t* info);
static bool GitHub_ParseJSON_String(const char* json, const char* key, char* value, size_t max_len);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief GitHub OTA'yı başlat
 */
OTA_Error_t GitHub_OTA_Init(const GitHub_OTA_Config_t* config)
{
    if (!config) {
        return OTA_ERR_INVALID;
    }
    
    memcpy(&github_config, config, sizeof(GitHub_OTA_Config_t));
    
    APP_LOG(TS_ON, VLEVEL_L, "GitHub OTA initialized for %s/%s\n\r", 
            github_config.github_user, github_config.github_repo);
    
    return OTA_ERR_NONE;
}

/**
 * @brief GitHub'dan firmware update kontrol et
 */
OTA_Error_t GitHub_OTA_CheckUpdates(void)
{
    GitHub_FirmwareInfo_t firmware_info = {0};
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Checking GitHub for firmware updates...\n\r");
    
    // 1. GitHub releases API'den latest firmware bilgisi al
    if (GitHub_CheckForUpdates(&firmware_info) != OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Failed to check GitHub releases\n\r");
        return OTA_ERR_FLASH;
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Latest firmware version: %s\n\r", firmware_info.version);
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Size: %lu bytes, CRC: 0x%04X\n\r", 
            firmware_info.size, firmware_info.crc16);
    
    // 2. Mevcut versiyonla karşılaştır
    char current_version[] = "2.0.0";
    
    if (strcmp(firmware_info.version, current_version) == 0) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Firmware is up to date\n\r");
        return OTA_ERR_NONE;
    }
    
    // 3. Update gerekli - MQTT ile Gateway'e bildir
    char mqtt_message[512];
    snprintf(mqtt_message, sizeof(mqtt_message),
             "{"
             "\"command\":\"start_ota\","
             "\"firmware_url\":\"%s\","
             "\"version\":\"%s\","
             "\"size\":%lu,"
             "\"crc16\":\"0x%04X\","
             "\"mandatory\":%s"
             "}",
             firmware_info.download_url,
             firmware_info.version, 
             firmware_info.size,
             firmware_info.crc16,
             firmware_info.mandatory ? "true" : "false");
    
    // MQTT publish et
    if (mqttPublish("ota/command/start", mqtt_message) == 0) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Update command sent to Gateway\n\r");
        return OTA_ERR_NONE;
    } else {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Failed to send update command\n\r");
        return OTA_ERR_FLASH;
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief GitHub releases API'den son release bilgisi al
 */
static OTA_Error_t GitHub_CheckForUpdates(GitHub_FirmwareInfo_t* firmware_info)
{
    if (!firmware_info) {
        return OTA_ERR_INVALID;
    }
    
    // GitHub raw URL oluştur
    char releases_url[256];
    snprintf(releases_url, sizeof(releases_url),
             "%s/%s/%s/%s/%s/releases.json",
             GITHUB_RAW_BASE_URL, GITHUB_USER, GITHUB_REPO, GITHUB_BRANCH, FIRMWARE_PATH);
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Fetching releases info from: %s\n\r", releases_url);
    
    // ESP32 HTTP request (Bu kısmı ESP32 HTTP client ile implement et)
    // Simulated response for now
    strcpy(http_response_buffer, 
           "{"
           "\"latest\": {"
           "\"version\": \"2.0.1\","
           "\"url\": \"https://raw.githubusercontent.com/username/lora-ota-firmware/main/firmware/node/latest/node_firmware.bin\""
           "}"
           "}");
    
    // JSON'u parse et
    if (GitHub_ParseReleaseJSON(http_response_buffer, firmware_info) != OTA_ERR_NONE) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Failed to parse releases JSON\n\r");
        return OTA_ERR_INVALID;
    }
    
    return OTA_ERR_NONE;
}

/**
 * @brief releases.json'u parse et
 */
static OTA_Error_t GitHub_ParseReleaseJSON(const char* json, GitHub_FirmwareInfo_t* info)
{
    if (!json || !info) {
        return OTA_ERR_INVALID;
    }
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Parsing releases JSON...\n\r");
    
    // "latest" object'inden bilgileri al
    char* latest_start = strstr(json, "\"latest\"");
    if (!latest_start) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: 'latest' section not found in JSON\n\r");
        return OTA_ERR_INVALID;
    }
    
    // Version
    if (!GitHub_ParseJSON_String(latest_start, "version", info->version, sizeof(info->version))) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Failed to parse version\n\r");
        return OTA_ERR_INVALID;
    }
    
    // Download URL  
    if (!GitHub_ParseJSON_String(latest_start, "url", info->download_url, sizeof(info->download_url))) {
        APP_LOG(TS_ON, VLEVEL_L, "OTA: Failed to parse download URL\n\r");
        return OTA_ERR_INVALID;
    }
    
    // Default values
    info->size = 65536;        // Default size
    info->crc16 = 0x1234;      // Default CRC
    info->mandatory = false;   // Default mandatory
    
    APP_LOG(TS_ON, VLEVEL_L, "OTA: Parsed - Version: %s, URL: %s\n\r", 
            info->version, info->download_url);
    
    return OTA_ERR_NONE;
}

/**
 * @brief JSON'dan string value parse et
 */
static bool GitHub_ParseJSON_String(const char* json, const char* key, char* value, size_t max_len)
{
    if (!json || !key || !value) return false;
    
    // "key": "value" formatını ara
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* key_pos = strstr(json, search_pattern);
    if (!key_pos) return false;
    
    // Value başlangıcını bul
    char* value_start = strchr(key_pos, '"');
    if (!value_start) return false;
    value_start++; // İlk " karakterini atla
    
    value_start = strchr(value_start, '"');
    if (!value_start) return false;
    value_start++; // İkinci " karakterini atla
    
    // Value sonunu bul
    char* value_end = strchr(value_start, '"');
    if (!value_end) return false;
    
    // Value'yu kopyala
    size_t value_len = value_end - value_start;
    if (value_len >= max_len) value_len = max_len - 1;
    
    strncpy(value, value_start, value_len);
    value[value_len] = '\0';
    
    return true;
}

/* Default Configuration */
const GitHub_OTA_Config_t GitHub_OTA_DefaultConfig = {
    .github_user = "your-username",
    .github_repo = "lora-ota-firmware", 
    .github_branch = "main",
    .firmware_path = "firmware/node",
    .check_interval_ms = 24 * 60 * 60 * 1000, // 24 hours
    .auto_download_enabled = true,
    .mandatory_updates_only = false
};