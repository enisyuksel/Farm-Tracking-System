#include "EMPA_MqttAws.h"
#include "myESP32AT.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include <string.h>

// LPUART1 Debug Helper (ESP32 UART - Logic Analyzer için)
extern UART_HandleTypeDef hlpuart1;
static void MQTT_DebugPrint(const char* msg) {
    if (msg) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[MQTT]", 6, 100);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), 1000);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, 100);
    }
}

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
extern char DEVICE_ID[];
long int counter_mqtt = 0;

MQTT_Config mqttConfig = {
	.mqttPacketBuffer = mqttPacketBuffer,
	.mode_wifi = STATION_MODE,
	.OSC_enable = SC_DISABLE,
	.wifiID = "EMPA_Arge",
	.wifiPassword = "Emp@Arg2024!",
	.timezone = 3,
	.mode_mqtt = MQTT_TLS_1,
	.clientID = "GW-001",
	.username = "IQYAZILIM",
	.mqttPassword = "159753456Empa",
	.keepAlive = 300,
	.cleanSession = CLS_1,
	.qos = QOS_0,
	.retain = RTN_0,
	.brokerAddress = "70a79e332cea4fd2a972c9fccbdedb79.s1.eu.hivemq.cloud",
	.reconnect = 0,
	.subtopic = "devices/GW-001/cmd/config",      // Config commands from cloud
	.pubtopic = "devices/GW-001/tele/data_batch"  // Batch data to cloud
};

// Config message handler function
void HandleConfigMessage(const char* message) {
	MQTT_DebugPrint("HNDL_CFG");
	if (!message) {
		MQTT_DebugPrint("ERR_NULL");
		return;
	}

	/* OTA Update format: {"type":150,"version":1} */
	if (strstr(message, "\"type\"") && strstr(message, "150")) {
		// Parse version
		uint8_t version = 0;
		char* version_ptr = strstr(message, "\"version\":");
		if (version_ptr) {
			version_ptr += 10; // Skip "version":
			while (*version_ptr == ' ' || *version_ptr == ':') version_ptr++;
			version = (uint8_t)atoi(version_ptr);
		}
		
		// Debug: OTA Request with version number
		char ota_msg[50];
		snprintf(ota_msg, sizeof(ota_msg), "[INFO] OTA Request Version %d", version);
		MQTT_DebugPrint(ota_msg);
		
		// TODO: Call OTA start function here when implemented
		// Example: OTA_StartUpdate(version);
		return;
	}

	/* Legacy numeric config format: {"type":161,"version":X,"period":Y} */
	if (strstr(message, "\"type\"") && (strstr(message, "161")||strstr(message, "162")||strstr(message, "163"))) {
		MQTT_DebugPrint("TYPE161");
		ParseConfigMessage(message); // Bu fonksiyon broadcast'i tetiklemeyecek şekilde ayarlandı
		return;
	}

	MQTT_DebugPrint("UNKNW_TYPE");
}

void MY_MqttAwsProcess(void)
{
	EMPA_Section = EMPA_SECTION_MQTT;

	// Task yapısına uyarlandı - her çağrıda bir state işle
	switch (mainState)
	{
	case STATE_MQTT_COLLECT:
		flag_waitMqttData = SET;

		// KRITIK: Her COLLECT state'inde UART interrupt'i başlat
		Wifi_WaitMqttData();

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
		printf("[DEBUG] *** ENTERED STATE_MQTT_INIT - Starting MQTT initialization ***\n");
		if (MQTT_Init(&mqttConfig) == FUNC_SUCCESSFUL)
		{
			printf("[DEBUG] MQTT_Init SUCCESSFUL!\n");
			flag_mqtt_connected = SET;  // MQTT connection established
			if (Wifi_MqttSubInit(mqttPacketBuffer, mqttConfig.subtopic, QOS_0) == FUNC_OK)
			{
				printf("[DEBUG] Wifi_MqttSubInit SUCCESSFUL!\n");
				cntTryFunc = 0;

				// KRITIK: UART interrupt'i başlat - subscription mesajlarını dinlemeye başla
				printf("[DEBUG] MQTT subscription successful - starting UART interrupt...\n");
				Wifi_WaitMqttData();
				printf("[DEBUG] Wifi_WaitMqttData called - switching to COLLECT state\n");

				mainState = STATE_MQTT_COLLECT; //buraya kadar geliyor
			}
			else
			{
				printf("[DEBUG] Wifi_MqttSubInit FAILED!\n");
				cntTryFunc++;
				mainState =
					(cntTryFunc == MAX_TRY_FUNC) ? STATE_MQTT_ERROR : STATE_MQTT_INIT;
			}
		}
		else
		{
			printf("[DEBUG] MQTT_Init FAILED! Retrying...\n");
			mainState = STATE_MQTT_INIT;
		}
		// Retry task for next state
		UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		break;
	case STATE_MQTT_IDLE:
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
			flag_mqtt_rx_done = RESET;
			/* mqttPacketBuffer içeriğini güvenli şekilde parse etmeden önce null terminate garanti altına al */
			mqttPacketBuffer[MQTT_DATA_PACKET_BUFF_SIZE-1] = '\0';
			/* Eski memset(strlen()) yerine tüm alanı sıfırla */
			memset(mqttMsgData.data, 0, sizeof(mqttMsgData.data));
			uint16_t rawLen = (uint16_t)strlen(mqttPacketBuffer);
			UART_MqttPacketParser(&mqttMsgData, mqttPacketBuffer, rawLen);
			MQTT_DebugPrint("PKT_PARSED");
			
			/* Legacy config tipini doğrudan ParseConfigMessage'e de yönlendirelim */
			if (strstr(mqttMsgData.data, "\"type\"") && strstr(mqttMsgData.data, "161")) {
				MQTT_DebugPrint("CALL_PARSE");
				ParseConfigMessage(mqttMsgData.data);
			} else {
				MQTT_DebugPrint("CALL_HANDLE");
				HandleConfigMessage(mqttMsgData.data);
			}
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
