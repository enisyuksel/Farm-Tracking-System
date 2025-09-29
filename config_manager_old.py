#!/usr/bin/env python3
"""
IQ Yazılım LoRa Project Configuration Manager
Manages config.json and regenerates project_config.h
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

def save_config(config):
    """Save configuration to config.json"""
    try:
        with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        print(f"✅ Configuration saved to {CONFIG_FILE}")
        return True
    except Exception as e:
        print(f"❌ Error saving {CONFIG_FILE}: {e}")
        return False

def generate_header(config):
    """Generate project_config.h from config.json"""
    
    # Determine build mode
    build_mode = config.get('build', {}).get('mode', 'GATEWAY')
    is_gateway = build_mode == 'GATEWAY'
    is_node = build_mode == 'NODE'
    
    # Header template
    header_content = f'''/**
 ******************************************************************************
 * @file    project_config.h
 * @author  IQ Yazılım
 * @brief   Project Configuration (Auto-generated from config.json)
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
#define LORA_FREQUENCY          {config.get('hardware', {}).get('frequency', 868000000)}UL
#define LORA_TX_POWER           {config.get('hardware', {}).get('tx_power', 14)}
#define LORA_SPREADING_FACTOR   {config.get('hardware', {}).get('spreading_factor', 7)}
#define LORA_BANDWIDTH          0  // {config.get('hardware', {}).get('bandwidth', '125kHz')}
#define LORA_CODINGRATE         1  // {config.get('hardware', {}).get('coding_rate', '4/5')}
'''

    if is_gateway:
        gateway_config = config.get('gateway', {})
        wifi_config = gateway_config.get('wifi', {})
        mqtt_config = gateway_config.get('mqtt', {})
        ota_config = gateway_config.get('ota', {})
        github_config = ota_config.get('github', {})
        
        header_content += f'''
/* Gateway Configuration -----------------------------------------------------*/
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
#define OTA_FIRMWARE_FILENAME   "{ota_config.get('firmware', {}).get('filename', 'node_firmware.bin')}"
#define OTA_MAX_FIRMWARE_SIZE   {ota_config.get('firmware', {}).get('max_size', 131072)}
#define OTA_CHUNK_SIZE          {ota_config.get('firmware', {}).get('chunk_size', 230)}

// Sensor Collection
#define SENSOR_COLLECTION_PERIOD {gateway_config.get('sensors', {}).get('collection_period', 60)}
#define MAX_SENSORS             {gateway_config.get('sensors', {}).get('max_sensors', 10)}
#define SENSOR_TIMEOUT          {gateway_config.get('sensors', {}).get('timeout', 5000)}
'''
    
    if is_node:
        node_config = config.get('node', {})
        sensor_config = node_config.get('sensor', {})
        ota_config = node_config.get('ota', {})
        power_config = node_config.get('power', {})
        
        header_content += f'''
/* Node Configuration --------------------------------------------------------*/
// Device Info
#define DEVICE_ID               "{node_config.get('device_id', 'NODE_001')}"
#define DEVICE_TYPE             "NODE"

// Sensor Settings
#define SENSOR_TYPE             "{sensor_config.get('type', 'ENVIRONMENTAL')}"
#define SENSOR_SAMPLING_PERIOD  {sensor_config.get('sampling_period', 30)}
#define BATTERY_MONITOR_ENABLED {'true' if sensor_config.get('battery_monitor', True) else 'false'}
#define SLEEP_MODE_ENABLED      {'true' if sensor_config.get('sleep_mode', True) else 'false'}

// OTA Settings
#define OTA_ENABLED             {'true' if ota_config.get('enabled', True) else 'false'}
#define FLASH_BANK1_START       {ota_config.get('flash', {}).get('bank1_start', '0x08000000')}
#define FLASH_BANK2_START       {ota_config.get('flash', {}).get('bank2_start', '0x08020000')}
#define FLASH_PAGE_SIZE         {ota_config.get('flash', {}).get('page_size', 2048)}
#define BOOTLOADER_FLAG_ADDR    {ota_config.get('flash', {}).get('bootloader_flag', '0x0803F800')}
#define OTA_MAX_RETRY_ATTEMPTS  {ota_config.get('retry', {}).get('max_attempts', 3)}
#define OTA_RETRY_TIMEOUT       {ota_config.get('retry', {}).get('timeout', 5000)}
#define OTA_RETRY_BACKOFF       {ota_config.get('retry', {}).get('backoff', 2)}

// Power Management
#define LOW_POWER_MODE_ENABLED  {'true' if power_config.get('low_power_mode', True) else 'false'}
#define WAKE_INTERVAL           {power_config.get('wake_interval', 60)}
#define BATTERY_THRESHOLD       {power_config.get('battery_threshold', 20)}
'''

    # Common configurations
    logging_config = config.get('logging', {})
    security_config = config.get('security', {})
    
    header_content += f'''
/* Logging Configuration -----------------------------------------------------*/
#define LOG_LEVEL_DEBUG         0
#define LOG_LEVEL_INFO          1
#define LOG_LEVEL_WARN          2
#define LOG_LEVEL_ERROR         3

#define CURRENT_LOG_LEVEL       LOG_LEVEL_{logging_config.get('level', 'INFO')}
#define UART_LOG_ENABLED        {'true' if logging_config.get('uart', {}).get('enabled', True) else 'false'}
#define UART_BAUDRATE           {logging_config.get('uart', {}).get('baudrate', 115200)}
#define LOG_TIMESTAMP_ENABLED   {'true' if logging_config.get('timestamp', True) else 'false'}

/* Security Configuration ---------------------------------------------------*/
#define SECURITY_ENCRYPTION_ENABLED {'true' if security_config.get('encryption', False) else 'false'}
#define AES_KEY                 "{security_config.get('aes_key', '0123456789ABCDEF0123456789ABCDEF')}"
#define DEVICE_UNIQUE_KEY       "{security_config.get('device_key', 'DEVICE_UNIQUE_KEY_HERE')}"
'''

    if is_gateway:
        header_content += f'''
/* API URLs (Generated from config.json) ------------------------------------*/
#define GITHUB_RELEASES_API     "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest"
#define GITHUB_DOWNLOAD_URL     "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download"
'''

    header_content += f'''
/* Build Mode Helpers -------------------------------------------------------*/
#define IS_GATEWAY_BUILD()      (CURRENT_BUILD_MODE == BUILD_MODE_GATEWAY)
#define IS_NODE_BUILD()         (CURRENT_BUILD_MODE == BUILD_MODE_NODE)

#ifdef __cplusplus
}}
#endif

#endif /* PROJECT_CONFIG_H */
'''

    # Write header file
    try:
        os.makedirs(os.path.dirname(HEADER_FILE), exist_ok=True)
        with open(HEADER_FILE, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"✅ Generated {HEADER_FILE}")
        return True
    except Exception as e:
        print(f"❌ Error generating {HEADER_FILE}: {e}")
        return False

def interactive_config():
    """Interactive configuration editor"""
    config = load_config()
    if not config:
        return False
    
    print("🔧 IQ Yazılım LoRa Project Configuration")
    print("=" * 50)
    
    # Build mode selection
    current_mode = config.get('build', {}).get('mode', 'GATEWAY')
    print(f"\\n📋 Current build mode: {current_mode}")
    print("1. GATEWAY (ESP32 WiFi + MQTT + OTA distributor)")
    print("2. NODE (Sensor + OTA receiver)")
    
    choice = input("\\nSelect build mode (1/2) or press Enter to keep current: ").strip()
    if choice == '1':
        config['build']['mode'] = 'GATEWAY'
        config['gateway']['enabled'] = True
        config['node']['enabled'] = False
    elif choice == '2':
        config['build']['mode'] = 'NODE'
        config['gateway']['enabled'] = False
        config['node']['enabled'] = True
    
    # Gateway specific configuration
    if config['build']['mode'] == 'GATEWAY':
        print("\\n🏭 Gateway Configuration:")
        
        # WiFi settings
        print("\\n📶 WiFi Settings:")
        config['gateway']['wifi']['ssid'] = input(f"WiFi SSID [{config['gateway']['wifi']['ssid']}]: ").strip() or config['gateway']['wifi']['ssid']
        config['gateway']['wifi']['password'] = input(f"WiFi Password [{config['gateway']['wifi']['password']}]: ").strip() or config['gateway']['wifi']['password']
        
        # MQTT settings  
        print("\\n📡 MQTT Settings:")
        config['gateway']['mqtt']['broker'] = input(f"MQTT Broker [{config['gateway']['mqtt']['broker']}]: ").strip() or config['gateway']['mqtt']['broker']
        config['gateway']['mqtt']['username'] = input(f"MQTT Username [{config['gateway']['mqtt']['username']}]: ").strip() or config['gateway']['mqtt']['username']
        config['gateway']['mqtt']['password'] = input(f"MQTT Password [{config['gateway']['mqtt']['password']}]: ").strip() or config['gateway']['mqtt']['password']
        
        # GitHub OTA settings
        print("\\n🚀 GitHub OTA Settings:")
        config['gateway']['ota']['github']['owner'] = input(f"GitHub Owner [{config['gateway']['ota']['github']['owner']}]: ").strip() or config['gateway']['ota']['github']['owner']
        config['gateway']['ota']['github']['repo'] = input(f"GitHub Repo [{config['gateway']['ota']['github']['repo']}]: ").strip() or config['gateway']['ota']['github']['repo']
        config['gateway']['ota']['github']['token'] = input(f"GitHub Token [{config['gateway']['ota']['github']['token']}]: ").strip() or config['gateway']['ota']['github']['token']
    
    # Node specific configuration
    elif config['build']['mode'] == 'NODE':
        print("\\n📡 Node Configuration:")
        
        # Sensor settings
        print("\\n🌡️ Sensor Settings:")
        period = input(f"Sampling period (seconds) [{config['node']['sensor']['sampling_period']}]: ").strip()
        if period.isdigit():
            config['node']['sensor']['sampling_period'] = int(period)
        
        # Power settings
        print("\\n🔋 Power Management:")
        wake_interval = input(f"Wake interval (seconds) [{config['node']['power']['wake_interval']}]: ").strip()
        if wake_interval.isdigit():
            config['node']['power']['wake_interval'] = int(wake_interval)
    
    # Common LoRa settings
    print("\\n📻 LoRa Settings:")
    freq = input(f"Frequency (Hz) [{config['hardware']['frequency']}]: ").strip()
    if freq.isdigit():
        config['hardware']['frequency'] = int(freq)
    
    power = input(f"TX Power (dBm) [{config['hardware']['tx_power']}]: ").strip()
    if power.isdigit():
        config['hardware']['tx_power'] = int(power)
    
    return save_config(config) and generate_header(config)

def main():
    if len(sys.argv) > 1:
        if sys.argv[1] == 'generate':
            # Just generate header from existing config
            config = load_config()
            if config:
                generate_header(config)
        elif sys.argv[1] == 'configure':
            # Interactive configuration
            interactive_config()
        else:
            print("Usage: python config_manager.py [generate|configure]")
    else:
        # Default: interactive configuration
        interactive_config()

if __name__ == "__main__":
    main()