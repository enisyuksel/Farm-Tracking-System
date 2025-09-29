#!/usr/bin/env python3
"""
Test config manager
"""

import json
import os
from datetime import datetime

CONFIG_FILE = "config.json"
HEADER_FILE = "Core/Inc/project_config.h"

def load_config():
    with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
        return json.load(f)

def generate_header(config):
    build_mode = config.get('build', {}).get('mode', 'GATEWAY')
    is_gateway = build_mode == 'GATEWAY'
    
    gateway_config = config.get('gateway', {})
    wifi_config = gateway_config.get('wifi', {})
    mqtt_config = gateway_config.get('mqtt', {})
    
    # Simple test template
    header_content = f'''/**
 * Test header - Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
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
#define WIFI_SSID               "{wifi_config.get('ssid', 'DEFAULT_SSID')}"
#define WIFI_PASSWORD           "{wifi_config.get('password', 'DEFAULT_PASSWORD')}"
#define WIFI_MODE               "{wifi_config.get('mode', 'STATION_MODE')}"
#define WIFI_TIMEOUT            {wifi_config.get('timeout', 30000)}
#define WIFI_TIMEZONE           {wifi_config.get('timezone', 3)}

// MQTT Settings  
#define MQTT_BROKER             "{mqtt_config.get('broker', 'DEFAULT_BROKER')}"
#define MQTT_PORT               {mqtt_config.get('port', 8883)}
#define MQTT_MODE               "{mqtt_config.get('mode', 'MQTT_TLS_1')}"
#define MQTT_USERNAME           "{mqtt_config.get('username', 'DEFAULT_USER')}"
#define MQTT_PASSWORD           "{mqtt_config.get('password', 'DEFAULT_PASS')}"
#define MQTT_CLIENT_ID          "{mqtt_config.get('client_id', 'DEFAULT_CLIENT')}"
#define MQTT_KEEP_ALIVE         {mqtt_config.get('keep_alive', 300)}

// MQTT Topics
#define MQTT_TOPIC_SUBSCRIBE    "{mqtt_config.get('topics', {}).get('subscribe', 'cmd/config')}"
#define MQTT_TOPIC_PUBLISH      "{mqtt_config.get('topics', {}).get('publish', 'tele/data')}"
#define MQTT_TOPIC_STATUS       "{mqtt_config.get('topics', {}).get('status', 'status')}"

// GitHub OTA
#define GITHUB_OWNER            "enisyuksel"
#define GITHUB_REPO             "Farm-Tracking-System"

#endif /* BUILD_MODE_GATEWAY */

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
'''
    
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