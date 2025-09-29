/**
 ******************************************************************************
 * @file    github_ota_downloader.h
 * @author  IQ Yazılım
 * @brief   GitHub OTA Firmware Downloader Header
 ******************************************************************************
 */

#ifndef __GITHUB_OTA_DOWNLOADER_H__
#define __GITHUB_OTA_DOWNLOADER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lora_ota_protocol.h"
#include "sys_app.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief GitHub OTA Configuration
 */
typedef struct {
    char github_user[64];          ///< GitHub username
    char github_repo[64];          ///< GitHub repository name  
    char github_branch[32];        ///< Git branch name
    char firmware_path[128];       ///< Firmware path in repo
    uint32_t check_interval_ms;    ///< Auto-check interval
    bool auto_download_enabled;    ///< Otomatik download açık/kapalı
    bool mandatory_updates_only;   ///< Sadece zorunlu update'leri al
} GitHub_OTA_Config_t;

/* Exported constants --------------------------------------------------------*/

// GitHub URL limits
#define GITHUB_MAX_URL_LENGTH           512
#define GITHUB_MAX_VERSION_LENGTH       32

/* Exported functions prototypes ---------------------------------------------*/

// Configuration Functions
OTA_Error_t GitHub_OTA_Init(const GitHub_OTA_Config_t* config);

// Update Check Functions
OTA_Error_t GitHub_OTA_CheckUpdates(void);

/* Default Configuration */
extern const GitHub_OTA_Config_t GitHub_OTA_DefaultConfig;

#ifdef __cplusplus
}
#endif

#endif /* __GITHUB_OTA_DOWNLOADER_H__ */