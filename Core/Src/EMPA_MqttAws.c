#include "EMPA_MqttAws.h"
#include "myESP32AT.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "project_config.h"  // Config ayarları için eklendi
#define FW_VERSION_MAJOR (uint8_t)0
#define FW_VERSION_MINOR (uint8_t)1
#define FW_VERSION_PATCH (uint8_t)0
#define ENABLE_ESP32 SET

APP_mainStateTypdef mainState = STATE_MQTT_INIT;
extern EMPA_SectionTypeDef EMPA_Section;
char mqttPacketBuffer[MQTT_DATA_PACKET_BUFF_SIZE] = {0};
extern volatile FlagStatus flag_mqtt_rx_done;
extern volatile uint8_t capturedPacketDataSize;
extern char mqttDataBuffer[1];
FlagStatus volatile flag_waitMqttData = RESET;
FlagStatus volatile flag_mqtt_connected = RESET;  // MQTT connection status
uint8_t cntTryFunc = 0;
MQTT_MsgDataTypeDef mqttMsgData;
MQTT_MacIdTypeDef headBoard;
MQTT_FwVersionDataTypeDef fwVersion = {FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH};
#define TOPIC_PUB_DATA "EMPA"
#define ENABLE_ESP32 SET
char topicBuffer[256];
extern int32_t temperature;
extern int32_t humidity;
// Device ID string variable tanımı kaldırıldı - config.h'dan #define olarak gelecek
long int counter_mqtt = 0;

MQTT_Config mqttConfig = {
	.mqttPacketBuffer = mqttPacketBuffer,
	.mode_wifi = STATION_MODE,
	.OSC_enable = SC_DISABLE,
	.wifiID = WIFI_SSID,                    // config.json'dan: "EMPA_Arge"
	.wifiPassword = WIFI_PASSWORD,          // config.json'dan: "Emp@Arg2024!"
	.timezone = WIFI_TIMEZONE,              // config.json'dan: 3
	.mode_mqtt = MQTT_TLS_1,
	.clientID = MQTT_CLIENT_ID,             // config.json'dan: "GW-001"
	.username = MQTT_USERNAME,              // config.json'dan: "IQYAZILIM"
	.mqttPassword = MQTT_PASSWORD,          // config.json'dan: "159753456Empa"
	.keepAlive = MQTT_KEEP_ALIVE,           // config.json'dan: 300
	.cleanSession = CLS_1,
	.qos = QOS_0,
	.retain = RTN_0,
	.brokerAddress = MQTT_BROKER,           // config.json'dan: "70a79e332cea4fd2a972c9fccbdedb79.s1.eu.hivemq.cloud"
	.reconnect = 0,
	.subtopic = MQTT_TOPIC_SUBSCRIBE,       // config.json'dan: "devices/GW-001/cmd/config"
	.pubtopic = MQTT_TOPIC_PUBLISH          // config.json'dan: "devices/GW-001/tele/data_batch"
};

// Config message handler function
void HandleConfigMessage(const char* message) {
    // Parse JSON config message from cloud
    // Example: {"type":"set_period","target":"gw","gw":"GW-001","payload":{"period_ms":5000}}
    // Example: {"type":"set_period","target":"snode","gw":"GW-001","id":"S-03","payload":{"period_ms":2000}}
    
    printf("Config received: %s\n", message);
    
    // Simple string parsing for now (could use proper JSON parser later)
    if (strstr(message, "\"type\":\"set_period\"")) {
        if (strstr(message, "\"target\":\"gw\"")) {
            // Gateway batch period change
            // TODO: Extract period_ms and update batch timer
            printf("Gateway period change requested\n");
        }
        else if (strstr(message, "\"target\":\"snode\"")) {
            // Sensor period change - send CONFIG frame via LoRa
            // TODO: Extract node ID and period, send 0xA1 frame
            printf("Sensor period change requested\n");
        }
    }
}

void MY_MqttAwsProcess(void)
{
	EMPA_Section = EMPA_SECTION_MQTT;
	
	// Task yapısına uyarlandı - her çağrıda bir state işle
	switch (mainState)
	{
	case STATE_MQTT_COLLECT:
		if (flag_mqtt_rx_done == 1)
		{
			mainState = STATE_MQTT_SUB_RX_MSG;
			// Only retrigger when we have work to do
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		}
		else
		{
			mainState = STATE_MQTT_PUB_TX_MSG;
			// Only retrigger when we have work to do
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		}
		break;
	case STATE_MQTT_INIT:
		if (MQTT_Init(&mqttConfig) == FUNC_SUCCESSFUL)
		{
			LED_Mqttconnected(SET);
			flag_mqtt_connected = SET;  // MQTT connection established
			if (Wifi_MqttSubInit(mqttPacketBuffer, mqttConfig.subtopic, QOS_0) == FUNC_OK)
			{
				cntTryFunc = 0;
				mainState = STATE_MQTT_COLLECT;
			}
			else
			{
				cntTryFunc++;
				mainState =
					(cntTryFunc == MAX_TRY_FUNC) ? STATE_MQTT_ERROR : STATE_MQTT_INIT;
			}
		}
		else
		{
			mainState = STATE_MQTT_INIT;
		}
		// Retry task for next state
		UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		break;
	case STATE_MQTT_IDLE:
		HAL_Delay(100);  // Shorter delay, allow other tasks to run
		// Continue after delay
		UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		break;
	case STATE_MQTT_SUB_RX_MSG:
		if (flag_waitMqttData)
		{
			Wifi_WaitMqttData();
			flag_waitMqttData = RESET;
		}
		if (flag_mqtt_rx_done == SET)
		{
			LED_MqttRXBlink();
			flag_mqtt_rx_done = RESET;
			memset(mqttMsgData.data, 0, strlen(mqttMsgData.data));
			UART_MqttPacketParser(&mqttMsgData, mqttPacketBuffer, strlen(mqttPacketBuffer));
			printf("\nReceived config message: %s\r\n", mqttMsgData.data);
			
			// Handle config message
			HandleConfigMessage(mqttMsgData.data);
			
			mainState = STATE_MQTT_SUB_RX_MSG;
		}
		counter_mqtt--;
		if (counter_mqtt == 0)
			mainState = STATE_MQTT_COLLECT;
		// Only retrigger if we're still processing messages
		if (flag_mqtt_rx_done == SET || counter_mqtt > 0) {
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		}
		break;
	case STATE_MQTT_PUB_TX_MSG:
		// This will be handled by batch publisher in subghz_phy_app.c
		// Just return to collect state - DON'T auto-retrigger
		mainState = STATE_MQTT_COLLECT;
		break;
	case STATE_MQTT_ERROR:
		cntTryFunc = 0;
		mainState = STATE_MQTT_IDLE;
		// Continue processing
		UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		break;
	}
}
