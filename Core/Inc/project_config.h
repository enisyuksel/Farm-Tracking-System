/**
 ******************************************************************************
 * @file    project_config.h
 * @author  IQ Yazılım
 * @brief   Project Configuration from config.json
 * @note    This file is auto-generated from config.json
 * @date    Generated: 2025-09-30 10:23:11
 ******************************************************************************
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Build Configuration -------------------------------------------------------*/
#define BUILD_MODE_GATEWAY      1
#define BUILD_MODE_NODE         2

// Current build mode: GATEWAY
#define CURRENT_BUILD_MODE      BUILD_MODE_GATEWAY

/* Gateway Configuration -----------------------------------------------------*/
#if (CURRENT_BUILD_MODE == BUILD_MODE_GATEWAY)

// Device Info
#define DEVICE_ID               "GATEWAY_001"
#define DEVICE_TYPE             "GATEWAY"

// WiFi Settings
#define WIFI_SSID               "EMPA_Arge"
#define WIFI_PASSWORD           "Emp@Arg2024!"
#define WIFI_MODE               "STATION_MODE"
#define WIFI_TIMEOUT            30000
#define WIFI_TIMEZONE           3

// MQTT Settings  
#define MQTT_BROKER             "70a79e332cea4fd2a972c9fccbdedb79.s1.eu.hivemq.cloud"
#define MQTT_PORT               8883
#define MQTT_MODE               "MQTT_TLS_1"
#define MQTT_USERNAME           "IQYAZILIM"
#define MQTT_PASSWORD           "159753456Empa"
#define MQTT_CLIENT_ID          "GW-001"
#define MQTT_KEEP_ALIVE         300

// MQTT Topics
#define MQTT_TOPIC_SUBSCRIBE    "devices/GW-001/cmd/config"
#define MQTT_TOPIC_PUBLISH      "devices/GW-001/tele/data_batch"
#define MQTT_TOPIC_STATUS       "devices/GW-001/status"

// GitHub OTA
#define GITHUB_OWNER            "enisyuksel"
#define GITHUB_REPO             "Farm-Tracking-System"

#endif /* BUILD_MODE_GATEWAY */

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */

/************************ (C) COPYRIGHT IQ Yazılım *****END OF FILE****/
