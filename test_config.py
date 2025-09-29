#!/usr/bin/env python3
"""
Test config manager
"""

import json
import os
from datetime import datetime

CONFIG_FILE = "config.json"
HEADER_FILE = "Core/Inc/project_config_test.h"

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

#ifndef PROJECT_CONFIG_TEST_H
#define PROJECT_CONFIG_TEST_H

// WiFi Settings
#define WIFI_SSID               "{wifi_config.get('ssid', 'DEFAULT_SSID')}"
#define WIFI_PASSWORD           "{wifi_config.get('password', 'DEFAULT_PASSWORD')}"

// MQTT Settings  
#define MQTT_BROKER             "{mqtt_config.get('broker', 'DEFAULT_BROKER')}"
#define MQTT_CLIENT_ID          "{mqtt_config.get('client_id', 'DEFAULT_CLIENT')}"

#endif
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