#!/usr/bin/env python3
"""
IQ Yazılım Configuration Manager
Manages config.json and generates project_config.h
"""

import json
import os
import sys
from datetime import datetime

CONFIG_FILE = "config.json"
HEADER_FILE = "Core/Inc/project_config.h"

def load_config():
    """Load configuration from config.json"""
    if not os.path.exists(CONFIG_FILE):
        print(f"❌ Error: {CONFIG_FILE} not found!")
        return None
    
    try:
        with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        print(f"❌ Error parsing {CONFIG_FILE}: {e}")
        return None

def generate_header(config):
    """Generate project_config.h from config.json"""
    
    # Determine build mode
    build_mode = config.get('build', {}).get('mode', 'GATEWAY')
    is_gateway = build_mode == 'GATEWAY'
    is_node = build_mode == 'NODE'
    
    # Get configurations
    gateway_config = config.get('gateway', {})
    node_config = config.get('node', {})
    hardware_config = config.get('hardware', {})
    
    # Header template
    header_content = f'''/**
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

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Build Configuration -------------------------------------------------------*/
#define BUILD_MODE_GATEWAY      1
#define BUILD_MODE_NODE         2

// Current build mode: {build_mode}
#define CURRENT_BUILD_MODE      BUILD_MODE_{build_mode}

/* Hardware Configuration ----------------------------------------------------*/
#define LORA_FREQUENCY          {hardware_config.get('frequency', 868000000)}UL
#define LORA_TX_POWER           {hardware_config.get('tx_power', 14)}
#define LORA_SPREADING_FACTOR   {hardware_config.get('spreading_factor', 7)}
#define LORA_BANDWIDTH          0  // {hardware_config.get('bandwidth', '125kHz')}
#define LORA_CODINGRATE         1  // {hardware_config.get('coding_rate', '4/5')}
'''

    if is_gateway:
        wifi_config = gateway_config.get('wifi', {})
        mqtt_config = gateway_config.get('mqtt', {})
        ota_config = gateway_config.get('ota', {})
        github_config = ota_config.get('github', {})
        
        header_content += f'''
/* Gateway Configuration -----------------------------------------------------*/
#if (CURRENT_BUILD_MODE == BUILD_MODE_GATEWAY)

// Device Info
#define DEVICE_ID               "{gateway_config.get('device_id', 'GATEWAY_001')}"
#define DEVICE_TYPE             "GATEWAY"

// WiFi Settings
#define WIFI_SSID               "{wifi_config.get('ssid', 'EMPA_Arge')}"
#define WIFI_PASSWORD           "{wifi_config.get('password', 'Emp@Arg2024!')}"
#define WIFI_MODE               "{wifi_config.get('mode', 'STATION_MODE')}"
#define WIFI_TIMEOUT            {wifi_config.get('timeout', 30000)}
#define WIFI_TIMEZONE           {wifi_config.get('timezone', 3)}
#define WIFI_OSC_ENABLE         {'true' if wifi_config.get('osc_enable', False) else 'false'}

// MQTT Settings
#define MQTT_BROKER             "{mqtt_config.get('broker', '3ac07e9d256141b9a168207c1a3e9dc4.s1.eu.hivemq.cloud')}"
#define MQTT_PORT               {mqtt_config.get('port', 8883)}
#define MQTT_MODE               "{mqtt_config.get('mode', 'MQTT_TLS_1')}"
#define MQTT_USERNAME           "{mqtt_config.get('username', 'atakan1234')}"
#define MQTT_PASSWORD           "{mqtt_config.get('password', 'Atakan1234')}"
#define MQTT_CLIENT_ID          "{mqtt_config.get('client_id', 'GW-001')}"
#define MQTT_KEEP_ALIVE         {mqtt_config.get('keep_alive', 300)}
#define MQTT_CLEAN_SESSION      {'true' if mqtt_config.get('clean_session', True) else 'false'}
#define MQTT_QOS                {mqtt_config.get('qos', 0)}
#define MQTT_RETAIN             {'true' if mqtt_config.get('retain', False) else 'false'}
#define MQTT_RECONNECT          {mqtt_config.get('reconnect', 0)}

// MQTT Topics
#define MQTT_TOPIC_SUBSCRIBE    "{mqtt_config.get('topics', {}).get('subscribe', 'devices/GW-001/cmd/config')}"
#define MQTT_TOPIC_PUBLISH      "{mqtt_config.get('topics', {}).get('publish', 'devices/GW-001/tele/data_batch')}"
#define MQTT_TOPIC_STATUS       "{mqtt_config.get('topics', {}).get('status', 'devices/GW-001/status')}"

// OTA Settings
#define OTA_ENABLED             {'true' if ota_config.get('enabled', True) else 'false'}
#define OTA_CHECK_INTERVAL      {ota_config.get('check_interval', 3600)}
#define GITHUB_OWNER            "{github_config.get('owner', 'enisyuksel')}"
#define GITHUB_REPO             "{github_config.get('repo', 'Farm-Tracking-System')}"
#define GITHUB_TOKEN            "{github_config.get('token', 'YOUR_GITHUB_TOKEN')}"

#endif /* BUILD_MODE_GATEWAY */
'''
    
    if is_node:
        sensor_config = node_config.get('sensor', {})
        power_config = node_config.get('power', {})
        
        header_content += f'''
/* Node Configuration --------------------------------------------------------*/
#if (CURRENT_BUILD_MODE == BUILD_MODE_NODE)

// Device Info  
#define DEVICE_ID               "{node_config.get('device_id', 'NODE_001')}"
#define DEVICE_TYPE             "NODE"

// Sensor Settings
#define SENSOR_TYPE             "{sensor_config.get('type', 'AGRICULTURAL')}"
#define SENSOR_PERIOD           {sensor_config.get('period', 30)}
#define SENSOR_ACCURACY         {sensor_config.get('accuracy', 1)}

// Power Management
#define POWER_MODE              "{power_config.get('mode', 'LOW_POWER')}"
#define SLEEP_DURATION          {power_config.get('sleep_duration', 300)}
#define BATTERY_MONITOR         {'true' if power_config.get('battery_monitor', True) else 'false'}

#endif /* BUILD_MODE_NODE */
'''

    header_content += f'''
#ifdef __cplusplus
}}
#endif

#endif /* PROJECT_CONFIG_H */

/************************ (C) COPYRIGHT IQ Yazılım *****END OF FILE****/
'''

    return header_content

def save_header(content):
    """Save generated header to file"""
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(HEADER_FILE), exist_ok=True)
        
        with open(HEADER_FILE, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"✅ Generated {HEADER_FILE}")
        return True
    except Exception as e:
        print(f"❌ Error writing {HEADER_FILE}: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 config_manager.py [generate|configure]")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "generate":
        config = load_config()
        if config:
            header_content = generate_header(config)
            save_header(header_content)
    
    elif command == "configure":
        print("🔧 Interactive configuration coming soon...")
    
    else:
        print(f"❌ Unknown command: {command}")
        sys.exit(1)

if __name__ == "__main__":
    main()