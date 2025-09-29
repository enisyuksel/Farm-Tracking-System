#!/usr/bin/env python3

import json
import os
from datetime import datetime

CONFIG_FILE = "config.json"
HEADER_FILE = "Core/Inc/project_config.h"

def load_config():
    with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
        return json.load(f)

def generate_header(config):
    gateway_config = config.get('gateway', {})
    wifi_config = gateway_config.get('wifi', {})
    mqtt_config = gateway_config.get('mqtt', {})
    
    wifi_ssid = wifi_config.get('ssid', 'DEFAULT_SSID')
    wifi_password = wifi_config.get('password', 'DEFAULT_PASSWORD')
    wifi_mode = wifi_config.get('mode', 'STATION_MODE')
    wifi_timeout = wifi_config.get('timeout', 30000)
    wifi_timezone = wifi_config.get('timezone', 3)
    
    mqtt_broker = mqtt_config.get('broker', 'DEFAULT_BROKER')
    mqtt_port = mqtt_config.get('port', 8883)
    mqtt_mode = mqtt_config.get('mode', 'MQTT_TLS_1')
    mqtt_username = mqtt_config.get('username', 'DEFAULT_USER')
    mqtt_password = mqtt_config.get('password', 'DEFAULT_PASS')
    mqtt_client_id = mqtt_config.get('client_id', 'DEFAULT_CLIENT')
    mqtt_keep_alive = mqtt_config.get('keep_alive', 300)
    
    topics = mqtt_config.get('topics', {})
    topic_subscribe = topics.get('subscribe', 'cmd/config')
    topic_publish = topics.get('publish', 'tele/data')
    topic_status = topics.get('status', 'status')
    
    header_content = f"""/**
 ******************************************************************************
 * @file    project_config.h
 * @author  IQ Yazılım
 * @brief   Project Configuration from config.json
 * @note    This file is auto-generated from config.json
 * @date    Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 ******************************************************************************
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {{
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
#define WIFI_SSID               "{wifi_ssid}"
#define WIFI_PASSWORD           "{wifi_password}"
#define WIFI_MODE               "{wifi_mode}"
#define WIFI_TIMEOUT            {wifi_timeout}
#define WIFI_TIMEZONE           {wifi_timezone}

// MQTT Settings  
#define MQTT_BROKER             "{mqtt_broker}"
#define MQTT_PORT               {mqtt_port}
#define MQTT_MODE               "{mqtt_mode}"
#define MQTT_USERNAME           "{mqtt_username}"
#define MQTT_PASSWORD           "{mqtt_password}"
#define MQTT_CLIENT_ID          "{mqtt_client_id}"
#define MQTT_KEEP_ALIVE         {mqtt_keep_alive}

// MQTT Topics
#define MQTT_TOPIC_SUBSCRIBE    "{topic_subscribe}"
#define MQTT_TOPIC_PUBLISH      "{topic_publish}"
#define MQTT_TOPIC_STATUS       "{topic_status}"

// GitHub OTA
#define GITHUB_OWNER            "enisyuksel"
#define GITHUB_REPO             "Farm-Tracking-System"

#endif /* BUILD_MODE_GATEWAY */

#ifdef __cplusplus
}}
#endif

#endif /* PROJECT_CONFIG_H */

/************************ (C) COPYRIGHT IQ Yazılım *****END OF FILE****/
"""
    
    return header_content

def save_header(content):
    os.makedirs(os.path.dirname(HEADER_FILE), exist_ok=True)
    with open(HEADER_FILE, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"✅ Generated {HEADER_FILE}")

# Main
config = load_config()
header_content = generate_header(config)
save_header(header_content)