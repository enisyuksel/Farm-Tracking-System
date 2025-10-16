/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    subghz_phy_app.c
 * @author  MCD Application Team
 * @brief   Application of the SubGHz_Phy Middleware
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "sys_app.h"
#include "subghz_phy_app.h"
#include "radio.h"

/* USER CODE BEGIN Includes */
#include "stm32_timer.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "app_version.h"
#include "subghz_phy_version.h"
#include <stdio.h>
#include "myESP32AT.h"
#include "EMPA_MqttAws.h"
#include "common.h"
#include "stm32wlxx_it.h"  // For MQTT ring buffer functions
#include "ota_manager.h"    // OTA Update Manager

// LPUART1 Debug Helper
extern UART_HandleTypeDef hlpuart1;
static void SUBGHZ_DebugPrint(const char* msg) {
    if (msg) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[SUBGHZ]", 8, 100);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), 1000);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, 100);
    }
}
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	RX, RX_TIMEOUT, RX_ERROR, TX, TX_TIMEOUT,
} States_t;

typedef enum {
	TEMP_AND_HUMIDITY = 0x01,
	ACCELERATION = 0x02,
	CONFIG_MSG = 0x03
} MessageType_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Configurations */
/*Timeout*/
#define RX_TIMEOUT_VALUE              60000
#define TX_TIMEOUT_VALUE              60000

#define MAX_APP_BUFFER_SIZE          255
/* wait for remote to be in Rx, before sending a Tx frame*/
#define RX_TIME_MARGIN                200
/* Afc bandwidth in Hz */
#define FSK_AFC_BANDWIDTH             83333
/* LED blink Period*/
#define LED_PERIOD_MS                 200

/* Packet Type Definitions - matching node implementation */
#define SENSOR_DATA_TYPE              0xD1  // Sensor data packet type
#define CONFIG_MSG_TYPE               0xA1  // Config message type
#define NODE_COUNT_CONFIG_TYPE        163   // Node count configuration message type

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Radio events function pointer */
static RadioEvents_t RadioEvents;

/* USER CODE BEGIN PV */
/*Ping Pong FSM states */
static States_t LoRaState = RX;
/* App Rx Buffer*/
static uint8_t BufferRx[MAX_APP_BUFFER_SIZE];
/* App Tx Buffer*/
static uint8_t BufferTx[MAX_APP_BUFFER_SIZE];
/* Last  Received Buffer Size*/
uint16_t RxBufferSize = 0;
/* Last  Received packer Rssi*/
int8_t RssiValue = 0;
/* Last  Received packer SNR (in Lora modulation)*/
int8_t SnrValue = 0;
/* Led Timers objects*/
static UTIL_TIMER_Object_t timerLed;
/* Gateway operates in pure RX mode - no master/slave concept */
/* random delay removed - not needed for gateway */

// Batch data variables - Modified to collect ALL packets
static uint8_t MAX_SNODES = 1; // Dynamic max sensor nodes count
static uint32_t BATCH_PERIOD_MS = 60000; // 1 minute (60 seconds) for regular intervals
#define INITIAL_SEND_DELAY_MS 5000  // First send after 5 seconds
#define MAX_PACKETS_IN_BATCH 300 // Maximum packets to store in 60 seconds
#define MAX_PACKETS_PER_BATCH   40  // İstersen dinamik hesaplatabilirsin

typedef struct {
    uint8_t node_id;      // S-01 = 1, S-02 = 2, etc.
    int16_t td;           // Digital temp (0.01°C)
    int16_t ta;           // Analog temp (0.01°C)
    uint8_t h;            // Humidity (%)
    int16_t ax, ay, az;   // Accelerometer (mg)
    uint16_t battery_voltage; // Battery voltage (mV)
    uint8_t config_version; // Node's config version from data packet
    uint32_t timestamp;   // When packet was received
    int16_t rssi;         // Signal strength
    int8_t snr;           // Signal to noise ratio
} ReceivedPacket_t;

// Config Version Tracking - Event-driven (no timers)
#define MAX_POSSIBLE_NODES 20  // Maximum possible nodes (for array allocation)
typedef struct {
    uint8_t node_config_status[MAX_POSSIBLE_NODES];  // 0x00=old_config, 0x01=updated_config
    uint8_t expected_version;                        // Expected config version for all nodes
    uint8_t config_update_active;                    // Flag: 1 if config update process is active
} NodeConfigTracker_t;


// Buffer to store ALL received packets during batch period
static ReceivedPacket_t packetBuffer[MAX_PACKETS_IN_BATCH];
static uint16_t packetCount = 0;
static UTIL_TIMER_Object_t phaseTimer;          // Main gateway phase timer
static UTIL_TIMER_Object_t configBroadcastTimer; // Config broadcast retry timer
static uint32_t batch_counter = 0;

uint8_t deger = 1;
int32_t temperature = 0;
int32_t humidity = 0;

// Globalde tanımla (dosyanın en üstüne, fonksiyon dışında)
#define JSON_BUFFER_SIZE   9000   // İhtiyacına göre 4K veya 8K seç
char jsonBuffer[JSON_BUFFER_SIZE];
// Gateway Operation Phases
typedef enum {
    PHASE_LISTENING = 0,    // 30s: Listen for node data
    PHASE_CONFIG_CHECK,     // Check broker for config updates
    PHASE_DATA_SEND,        // Send collected data to broker
    PHASE_CONFIG_BROADCAST  // 15s: Broadcast config to nodes (if updated)
} GatewayPhase_t;

// Gateway Configuration Management
typedef struct {
    uint8_t nodes_version;      // Config version for nodes (type 161)
    uint8_t gateway_version;    // Config version for gateway (type 162)
    uint32_t period_ms;         // Node transmission period in milliseconds
    uint32_t batch_period_ms;   // Gateway batch period in milliseconds
    uint8_t updated;            // Flag: 1 if config was updated and needs broadcast
    uint8_t broadcast_active;   // Flag: 1 if currently broadcasting config
} GatewayConfig_t;

static GatewayConfig_t gatewayConfig = {
    .nodes_version = 1,         // Default version for nodes
    .gateway_version = 1,       // Default version for gateway
    .period_ms = 3000,          // Default 3 seconds for nodes
    .batch_period_ms = 60000,   // 1 minute (60 seconds) for gateway batch
    .updated = 0,
    .broadcast_active = 0
};

static NodeConfigTracker_t configTracker = {0};

static GatewayPhase_t currentPhase = PHASE_LISTENING;
static uint8_t last_checked_version = 0;  // Track last processed config version

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/*!
 * @brief Function to be executed on Radio Tx Done event
 */
static void OnTxDone(void);

/**
  * @brief Function to be executed on Radio Rx Done event
  * @param  payload ptr of buffer received
  * @param  size buffer size
  * @param  rssi
  * @param  LoraSnr_FskCfo
  */
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo);

/**
  * @brief Function executed on Radio Tx Timeout event
  */
static void OnTxTimeout(void);

/**
  * @brief Function executed on Radio Rx Timeout event
  */
static void OnRxTimeout(void);

/**
  * @brief Function executed on Radio Rx Error event
  */
static void OnRxError(void);

/* USER CODE BEGIN PFP */
/**
 * @brief  Function executed on when led timer elapses
 * @param  context ptr of LED context
 */
static void OnledEvent(void *context);

/**
 * @brief PingPong state machine implementation
 */
static void PingPong_Process(void);

/**
 * @brief Gateway: Main phase timer handler - controls 30s listen → config → data → broadcast cycle
 */
static void GatewayPhaseHandler(void *context);

/**
 * @brief Gateway: Config broadcast retry timer handler - sends config every 2s during broadcast phase
 */
static void ConfigBroadcastHandler(void *context);

/**
 * @brief Check broker for config updates
 */
static void CheckBrokerConfig(void);

/**
 * @brief Parse config message and update gateway settings
 */
void ParseConfigMessage(const char* message);

/**
 * @brief Update gateway config if new version received
 */
static void UpdateGatewayConfig(uint8_t version, uint32_t period_ms);

/**
 * @brief Update gateway batch period if type 162 message received
 */
static void UpdateGatewayBatchPeriod(uint8_t version, uint32_t batch_period_ms);

/**
 * @brief Send collected data to broker
 */
static void SendDataToBroker(void);

/**
 * @brief Send config packet to nodes via LoRa
 */
static void SendConfigToNodes(void);

/**
 * @brief Calculate CRC8 for config packet
 */
static uint8_t CalculateCRC8(uint8_t *data, uint8_t length);

/**
 * @brief Initialize config tracking system
 */
static void InitializeNodeConfigTracker(void);

/**
 * @brief Process node config version from data packet and update tracking
 */
static void ProcessNodeConfigVersion(uint8_t node_id, uint8_t received_version);

/**
 * @brief Check if all nodes have updated to new config version
 */
static uint8_t CheckAllNodesUpdated(void);

/**
 * @brief Reset config tracker after config broadcast is complete
 */
static void ResetConfigTracker(void);
/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
void SubghzApp_Init(void)
{
  /* USER CODE BEGIN SubghzApp_Init_1 */

	APP_LOG(TS_OFF, VLEVEL_M, "\n\rLORA GATEWAY\n\r");
	/* Get SubGHY_Phy APP version*/
	APP_LOG(TS_OFF, VLEVEL_M, "APPLICATION_VERSION: V%X.%X.%X\r\n",
			(uint8_t)(APP_VERSION_MAIN), (uint8_t)(APP_VERSION_SUB1),
			(uint8_t)(APP_VERSION_SUB2));

	/* Get MW SubGhz_Phy info */
	APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:    V%X.%X.%X\r\n",
			(uint8_t)(SUBGHZ_PHY_VERSION_MAIN),
			(uint8_t)(SUBGHZ_PHY_VERSION_SUB1),
			(uint8_t)(SUBGHZ_PHY_VERSION_SUB2));

	/* Led Timers*/
	UTIL_TIMER_Create(&timerLed, LED_PERIOD_MS, UTIL_TIMER_ONESHOT, OnledEvent,
			NULL);
	UTIL_TIMER_Start(&timerLed);
  /* USER CODE END SubghzApp_Init_1 */

  /* Radio initialization */
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError = OnRxError;

  Radio.Init(&RadioEvents);

  /* USER CODE BEGIN SubghzApp_Init_2 */
	/* Gateway mode - no random delay needed */

	/* Radio Set frequency */
	Radio.SetChannel(RF_FREQUENCY);

	/* Radio configuration */
#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
	APP_LOG(TS_OFF, VLEVEL_M, "---------------\n\r");
	APP_LOG(TS_OFF, VLEVEL_M, "LORA_MODULATION\n\r");
	APP_LOG(TS_OFF, VLEVEL_M, "LORA_BW=%d kHz\n\r", (1 << LORA_BANDWIDTH) * 125);
	APP_LOG(TS_OFF, VLEVEL_M, "LORA_SF=%d\n\r", LORA_SPREADING_FACTOR);

	Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
	LORA_SPREADING_FACTOR, LORA_CODINGRATE,
	LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
	true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);

	Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
	LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
	LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
			LORA_IQ_INVERSION_ON, true);

	Radio.SetMaxPayloadLength(MODEM_LORA, MAX_APP_BUFFER_SIZE);

#elif ((USE_MODEM_LORA == 0) && (USE_MODEM_FSK == 1))
  APP_LOG(TS_OFF, VLEVEL_M, "---------------\n\r");
  APP_LOG(TS_OFF, VLEVEL_M, "FSK_MODULATION\n\r");
  APP_LOG(TS_OFF, VLEVEL_M, "FSK_BW=%d Hz\n\r", FSK_BANDWIDTH);
  APP_LOG(TS_OFF, VLEVEL_M, "FSK_DR=%d bits/s\n\r", FSK_DATARATE);

  Radio.SetTxConfig(MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV, 0,
                    FSK_DATARATE, 0,
                    FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, 0, TX_TIMEOUT_VALUE);

  Radio.SetRxConfig(MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                    0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                    0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, true,
                    0, 0, false, true);

  Radio.SetMaxPayloadLength(MODEM_FSK, MAX_APP_BUFFER_SIZE);

#else
#error "Please define a modulation in the subghz_phy_app.h file."
#endif /* USE_MODEM_LORA | USE_MODEM_FSK */

	/*fills tx buffer*/
	memset(BufferTx, 0x0, MAX_APP_BUFFER_SIZE);

	APP_LOG(TS_ON, VLEVEL_L, "LoRa Gateway Radio initialized - Waiting for MQTT connection\n\r");

	// Don't start LoRa operations here - wait for MQTT connection
  /* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */
char receiver_buffer[250] = {0};

// Start LoRa Gateway operations after MQTT is connected
void StartLoRaGateway(void) {
	APP_LOG(TS_ON, VLEVEL_L, "Starting LoRa Gateway with 4-phase cycle\n\r");

	/* Initialize packet buffer */
	packetCount = 0;
	memset(packetBuffer, 0, sizeof(packetBuffer));
	printf("*** Packet buffer initialized: packetCount = %d ***\n", packetCount);

	/* Initialize gateway phase system */
	currentPhase = PHASE_LISTENING;
	gatewayConfig.broadcast_active = 0;

	/* Create phase timer - First send after 5 seconds, then 1 minute intervals */
	UTIL_TIMER_Create(&phaseTimer, INITIAL_SEND_DELAY_MS, UTIL_TIMER_ONESHOT, GatewayPhaseHandler, NULL);

	/* Create config broadcast timer (2s retry during broadcast phase) */
	UTIL_TIMER_Create(&configBroadcastTimer, 2000, UTIL_TIMER_ONESHOT, ConfigBroadcastHandler, NULL);

    /* Initialize event-driven config tracking system */
	InitializeNodeConfigTracker();

	/* Start with listening phase */
	UTIL_TIMER_Start(&phaseTimer);
	printf("*** PHASE TIMER STARTED - First send in 5s, then 60s intervals ***\n");
	APP_LOG(TS_ON, VLEVEL_L, "Gateway phase timer started - Phase: LISTENING\n\r");

	/*register task to to be run in while(1) after Radio IT*/
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), UTIL_SEQ_RFU,
			PingPong_Process);

	// Start listening for incoming packets immediately
	APP_LOG(TS_ON, VLEVEL_L, "Starting Gateway RX mode - Phase: LISTENING\n\r");
	LoRaState = RX;
	Radio.Rx(RX_TIMEOUT_VALUE);
}
/* USER CODE END EF */

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void)
{
  /* USER CODE BEGIN OnTxDone */
	APP_LOG(TS_ON, VLEVEL_L, "OnTxDone\n\r");
	/* Gateway: After TX, return to RX mode */
	LoRaState = TX; // Trigger PingPong_Process to restart RX

	/* Run PingPong process in background*/
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process),
			CFG_SEQ_Prio_0);
  /* USER CODE END OnTxDone */
}

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t LoraSnr_FskCfo)
{
  /* USER CODE BEGIN OnRxDone */
	APP_LOG(TS_ON, VLEVEL_L, "OnRxDone\n\r");
	APP_LOG(TS_ON, VLEVEL_L, "Received : %s\n\r", payload);

#if ((USE_MODEM_LORA == 1) && (USE_MODEM_FSK == 0))
	APP_LOG(TS_ON, VLEVEL_L, "RssiValue=%d dBm, SnrValue=%ddB\n\r", rssi,
			LoraSnr_FskCfo);
	/* Record payload Signal to noise ratio in Lora*/
	SnrValue = LoraSnr_FskCfo;
#endif /* USE_MODEM_LORA | USE_MODEM_FSK */
#if ((USE_MODEM_LORA == 0) && (USE_MODEM_FSK == 1))
  APP_LOG(TS_ON, VLEVEL_L, "RssiValue=%d dBm, Cfo=%dkHz\n\r", rssi, LoraSnr_FskCfo);
  SnrValue = 0; /*not applicable in GFSK*/
#endif /* USE_MODEM_LORA | USE_MODEM_FSK */

	// Gateway mode: Parse DATA payload and store ALL packets in buffer
	if (size >= 17 && payload[0] == SENSOR_DATA_TYPE) { // DATA payload type + BATTERY + CONFIG_VER + CRC (17 bytes minimum)
		// Parse sensor data from LoRa payload with BATTERY VOLTAGE and CONFIG VERSION
		uint8_t type = payload[0];                               // Frame type (SENSOR_DATA_TYPE)
		uint8_t node_id = payload[1];                            // Node ID (S-01=1, S-02=2, etc.)
		int16_t td = (int16_t)(payload[2] | (payload[3] << 8));  // Digital temp (0.01°C)
		int16_t ta = (int16_t)(payload[4] | (payload[5] << 8));  // Analog temp (0.01°C)
		uint8_t h = payload[6];                                  // Humidity (%)
		int16_t ax = (int16_t)(payload[7] | (payload[8] << 8));  // Accel X (mg)
		int16_t ay = (int16_t)(payload[9] | (payload[10] << 8)); // Accel Y (mg)
		int16_t az = (int16_t)(payload[11] | (payload[12] << 8)); // Accel Z (mg)
		uint16_t battery_voltage = (uint16_t)(payload[13] | (payload[14] << 8)); // Battery voltage (mV)
		uint8_t config_version = payload[15];                    // Node's config version
		uint8_t crc = payload[16];                               // CRC-8/MAXIM

		APP_LOG(TS_ON, VLEVEL_L, "DATA from S-%02d: TD=%.2f°C TA=%.2f°C H=%d%% AX=%dmg AY=%dmg AZ=%dmg BAT=%dmV ConfigV%d\n\r",
				node_id, td/100.0f, ta/100.0f, h, ax, ay, az, battery_voltage, config_version);

		// CRITICAL: Process config version for event-driven config tracking

		ProcessNodeConfigVersion(node_id, config_version);

		// Store EVERY packet in the buffer (not just latest per node)
		if (node_id >= 1 && node_id <= MAX_SNODES && packetCount < MAX_PACKETS_IN_BATCH) {
			printf("*** DEBUG: Adding packet %d from S-%02d ***\n\r", packetCount + 1, node_id);

			packetBuffer[packetCount].node_id = node_id;
			packetBuffer[packetCount].td = td;
			packetBuffer[packetCount].ta = ta;
			packetBuffer[packetCount].h = h;
			packetBuffer[packetCount].ax = ax;
			packetBuffer[packetCount].ay = ay;
			packetBuffer[packetCount].az = az;
			packetBuffer[packetCount].battery_voltage = battery_voltage;
			packetBuffer[packetCount].config_version = config_version;
			packetBuffer[packetCount].timestamp = HAL_GetTick();
			packetBuffer[packetCount].rssi = rssi;
			packetBuffer[packetCount].snr = LoraSnr_FskCfo;

			packetCount++;

			printf("*** DEBUG: Packet stored! Total packets in buffer: %d ***\n\r", packetCount);
			APP_LOG(TS_ON, VLEVEL_L, "Packet %d stored from S-%02d BAT=%dmV ConfigV%d (Total: %d packets)\n\r",
					packetCount, node_id, battery_voltage, config_version, packetCount);
		} else if (packetCount >= MAX_PACKETS_IN_BATCH) {
			printf("*** WARNING: Packet buffer full! Dropping packet from S-%02d ***\n\r", node_id);
			APP_LOG(TS_ON, VLEVEL_L, "WARNING: Packet buffer full! Dropping packet from S-%02d\n\r", node_id);
		} else {
			printf("*** ERROR: Invalid node_id %d or other error ***\n\r", node_id);
		}
	}

	LoRaState = TX;  // Trigger PingPong_Process to restart RX

	/* Clear BufferRx*/
	memset(BufferRx, 0, MAX_APP_BUFFER_SIZE);
	/* Record payload size*/
	RxBufferSize = size;
	if (RxBufferSize <= MAX_APP_BUFFER_SIZE) {
		memcpy(BufferRx, payload, RxBufferSize);
	}
	/* Record ed Signal Strength*/
	RssiValue = rssi;
	/* Record payload content*/
	APP_LOG(TS_ON, VLEVEL_H, "payload. size=%d \n\r", size);
	for (int32_t i = 0; i < PAYLOAD_LEN; i++) {
		APP_LOG(TS_ON, VLEVEL_H, "%02X", BufferRx[i]);
		if (i % 16 == 15) {
			APP_LOG(TS_ON, VLEVEL_H, "\n\r");
		}
	}
	APP_LOG(TS_OFF, VLEVEL_H, "\n\r");
	/* Run PingPong process in background*/
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process),
			CFG_SEQ_Prio_0);
	memset(payload,0,size);
  /* USER CODE END OnRxDone */
}

static void OnTxTimeout(void)
{
  /* USER CODE BEGIN OnTxTimeout */
	APP_LOG(TS_ON, VLEVEL_L, "OnTxTimeout\n\r");
	/* Update the LoRaState of the FSM*/
	LoRaState = TX_TIMEOUT;
	/* Run PingPong process in background*/
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process),
			CFG_SEQ_Prio_0);
  /* USER CODE END OnTxTimeout */
}
static void OnledEvent(void *context) {
	HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); /* LED_GREEN */
//	HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin); /* LED_RED */
	UTIL_TIMER_Start(&timerLed);
}
static void OnRxTimeout(void)
{
  /* USER CODE BEGIN OnRxTimeout */
	APP_LOG(TS_ON, VLEVEL_L, "OnRxTimeout\n\r");
	/* Update the LoRaState of the FSM*/
	LoRaState = RX_TIMEOUT;  // Gateway mode: trigger restart

	/* Run PingPong process in background*/
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process),
			CFG_SEQ_Prio_0);
  /* USER CODE END OnRxTimeout */
}

static void OnRxError(void)
{
  /* USER CODE BEGIN OnRxError */
	APP_LOG(TS_ON, VLEVEL_L, "OnRxError\n\r");
	/* Update the LoRaState of the FSM*/
	LoRaState = RX_ERROR;
	/* Run PingPong process in background*/
	UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process),
			CFG_SEQ_Prio_0);
  /* USER CODE END OnRxError */
}

/* USER CODE BEGIN PrFD */
static void PingPong_Process(void) {
	// OTA Check: Skip LoRa processing if OTA is active
	if (OTA_IsActive()) {
		SUBGHZ_DebugPrint("LORA_BLOCKED_OTA");
		return;
	}
	
	switch (LoRaState) {
	case RX:
		// Gateway mode: Continue listening (this should not normally happen)
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: Restart listening...\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		break;
	case TX:
		// Gateway received data, restart listening immediately
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: Data processed, restart RX\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		LoRaState = RX;
		break;
	case RX_TIMEOUT:
		// Timeout, restart listening - normal for Gateway
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: RX timeout, restart listening\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		LoRaState = RX;
		break;
	case RX_ERROR:
		// Error, restart listening
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: RX error, restart listening\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		LoRaState = RX;
		break;
	case TX_TIMEOUT:
		// Should not happen in Gateway mode
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: TX timeout (unexpected)\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		LoRaState = RX;
		break;
	default:
		APP_LOG(TS_ON, VLEVEL_L, "Gateway: Unknown state, restart RX\n\r");
		Radio.Rx(RX_TIMEOUT_VALUE);
		LoRaState = RX;
		break;
	}
}

/**
 * @brief Gateway Phase Handler - Main 4-phase cycle controller
 * Phase 1: LISTENING (30s) → Phase 2: CONFIG_CHECK → Phase 3: DATA_SEND → Phase 4: CONFIG_BROADCAST (15s if needed)
 */
static void GatewayPhaseHandler(void *context) {
    printf("\n=== GATEWAY PHASE TRANSITION ===\n");
    printf("Current phase: %d\n", currentPhase);
    printf("Current time: %lu ms\n", HAL_GetTick());
    printf("Packets collected: %d\n", packetCount);

    switch (currentPhase) {
        case PHASE_LISTENING:
            printf("*** PHASE 1 COMPLETE: 30s Listening finished ***\n");
            currentPhase = PHASE_CONFIG_CHECK;
            // Immediately process config check
            GatewayPhaseHandler(NULL);
            break;

        case PHASE_CONFIG_CHECK:
            printf("*** PHASE 2: Config check from broker ***\n");
            CheckBrokerConfig();
            currentPhase = PHASE_DATA_SEND;
            // Immediately process data send
            GatewayPhaseHandler(NULL);
            break;

        case PHASE_DATA_SEND:
            printf("*** PHASE 3: Sending data to broker ***\n");
            SendDataToBroker();
            // Check if config broadcast needed or if config tracking is still active
            if (gatewayConfig.updated) {
            	if (CheckAllNodesUpdated()) {
				   printf("*** All nodes updated! Config tracking complete ***\n");
				   ResetConfigTracker();
				    UTIL_TIMER_SetPeriod(&phaseTimer, BATCH_PERIOD_MS);
				    UTIL_TIMER_Start(&phaseTimer);
				}
			   else{
					UTIL_TIMER_Stop(&phaseTimer);
					UTIL_TIMER_Stop(&configBroadcastTimer);
					printf("*** Config updated - Starting PHASE 4: Config broadcast (15s) ***\n");
					currentPhase = PHASE_CONFIG_BROADCAST;
					gatewayConfig.broadcast_active = 1;
					configTracker.config_update_active = 1;
					gatewayConfig.updated = 1;

					// Start config broadcast timer (2s retry)
					UTIL_TIMER_Start(&configBroadcastTimer);

					// Set phase timer for 15s broadcast period
					UTIL_TIMER_SetPeriod(&phaseTimer, 15000);
					UTIL_TIMER_Start(&phaseTimer);
			   }

            } else {
                printf("*** No config update - Returning to PHASE 1: Listening (30s) ***\n");
                currentPhase = PHASE_LISTENING;

                // Reset packet buffer for next cycle
                packetCount = 0;
                memset(packetBuffer, 0, sizeof(packetBuffer));

                // Restart listening phase
                UTIL_TIMER_SetPeriod(&phaseTimer, BATCH_PERIOD_MS);
                UTIL_TIMER_Start(&phaseTimer);

                // Ensure RX mode
                if (LoRaState != RX) {
                    Radio.Rx(RX_TIMEOUT_VALUE);
                    LoRaState = RX;
                }
            }
            break;

        case PHASE_CONFIG_BROADCAST:
            printf("*** PHASE 4 COMPLETE: 15s Config broadcast finished ***\n");

            // Stop config broadcast
            gatewayConfig.broadcast_active = 0;

            UTIL_TIMER_Stop(&configBroadcastTimer);

            // Return to listening phase
            currentPhase = PHASE_LISTENING;

            // Reset packet buffer for next cycle
            packetCount = 0;
            memset(packetBuffer, 0, sizeof(packetBuffer));

            // Restart listening phase (30s)
            UTIL_TIMER_SetPeriod(&phaseTimer, BATCH_PERIOD_MS);
            UTIL_TIMER_Start(&phaseTimer);

            // Ensure RX mode
            printf("*** Returning to PHASE 1: Listening (30s) ***\n");
            if (LoRaState != RX) {
                Radio.Rx(RX_TIMEOUT_VALUE);
                LoRaState = RX;
            }
            break;
    }

    printf("=== NEW PHASE: %d ===\n\n", currentPhase);
}

/**
 * @brief Config Broadcast Handler - Sends config packet every 2s during broadcast phase
 */
static void ConfigBroadcastHandler(void *context) {
    if (gatewayConfig.broadcast_active && currentPhase == PHASE_CONFIG_BROADCAST) {
        printf("*** Config broadcast retry - sending to nodes ***\n");
        SendConfigToNodes();

        // Restart timer for next broadcast (2s interval)
        UTIL_TIMER_Start(&configBroadcastTimer);
    }
}

/**
 * @brief Check broker for config updates before sending data
 */
static void CheckBrokerConfig(void) {
    extern volatile FlagStatus flag_mqtt_rx_done;
    extern FlagStatus volatile flag_waitMqttData;
    extern FlagStatus volatile flag_mqtt_connected;
    extern APP_mainStateTypdef mainState;
    extern char mqttPacketBuffer[];
    extern MQTT_Config mqttConfig;

    printf("\n=== CheckBrokerConfig: Enhanced MQTT Config Reception ===\n");
    printf("MQTT Status: connected=%d, mainState=%d, flag_rx_done=%d\n",
           flag_mqtt_connected, mainState, flag_mqtt_rx_done);

    if (flag_mqtt_connected != SET) {
        printf("ERROR: MQTT NOT CONNECTED!\n");
        return;
    }

    // Clear packet buffer
    memset(mqttPacketBuffer, 0, MQTT_DATA_PACKET_BUFF_SIZE);

    printf("Starting MQTT config check with ring buffer...\n");
    // Print ring buffer debug info
        extern volatile uint32_t dbg_irq_count, dbg_rxne_hits, dbg_rdr_reads, dbg_flag_sets;
        extern volatile uint32_t dbg_idle_hits, dbg_ore_hits;
        extern volatile uint8_t dbg_last_byte;
        printf("=== RING BUFFER DEBUG INFO ===\n");
        printf("IRQ Count: %lu, RXNE Hits: %lu, RDR Reads: %lu, Flag Sets: %lu\n",
               dbg_irq_count, dbg_rxne_hits, dbg_rdr_reads, dbg_flag_sets);
        printf("IDLE Hits: %lu, ORE Hits: %lu, Last Byte: 0x%02X\n",
               dbg_idle_hits, dbg_ore_hits, dbg_last_byte);

    // Try multiple times to receive complete config message
    for (int attempt = 0; attempt < 3; attempt++) {
        printf("Attempt %d: Triggering MQTT task...\n", attempt + 1);

        // Clear flag and trigger MQTT task
        flag_mqtt_rx_done = RESET;
        flag_waitMqttData = SET;
        UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);

        // Wait for data to arrive

        // Check if we have data in ring buffer
        uint16_t available_bytes = MQTT_GetAvailableBytes();
        printf("Available bytes in ring buffer: %d\n", available_bytes);

        if (available_bytes > 0) {
            // Read complete message from ring buffer
            char config_message[512] = {0};
            uint16_t bytes_read = MQTT_ReadString(config_message, sizeof(config_message));

            printf("Read %d bytes from ring buffer: %s\n", bytes_read, config_message);

            // Check if this looks like a config message
            if (strstr(config_message, "\"type\"") &&
                (strstr(config_message, "161") || strstr(config_message, "162") || strstr(config_message, "163") ||
                 strstr(config_message, "0xA1") || strstr(config_message, "0xA2"))) {
                printf("*** SUCCESS: Config message found! ***\n");
                printf("Config message: %s\n", config_message);

                // Parse the config message
                ParseConfigMessage(config_message);
                return;
            }
        }

        // If no ring buffer data, check legacy flag
        if (flag_mqtt_rx_done == SET) {
            printf("Legacy flag set, but no ring buffer data\n");
            flag_mqtt_rx_done = RESET;
        }
    }

    printf("=== CheckBrokerConfig: No config message found ===\n");
}

/**
 * @brief Parse config message and update gateway settings
 * Expected format: {"type":161,"version":3,"period":5000} for nodes or {"type":162,"version":3,"period":180000} for gateway batch
 */
void ParseConfigMessage(const char* message) {
    SUBGHZ_DebugPrint("PARSE_CFG_ENTER");
    printf("*** ENTERED ParseConfigMessage FUNCTION ***\n");
    if (!message) {
        SUBGHZ_DebugPrint("ERR_NULL_MSG");
        printf("*** ParseConfigMessage: NULL message received ***\n");
        return;
    }
    SUBGHZ_DebugPrint(message); // Mesajı bas
    printf("*** ParseConfigMessage: Processing: [%s] (len=%zu) ***\n", message, strlen(message));

    uint8_t config_version = 0;
    uint32_t config_period = 0;
    uint16_t config_type = 0;

    // Try JSON format first: {"type":161,"version":3,"period":5000} or {"type":162,"version":3,"period":180000}
    if (strstr(message, "\"type\"")) {
        SUBGHZ_DebugPrint("JSON_TYPE_FOUND");
        printf("*** Found JSON type field ***\n");

        // Parse type
        char* type_ptr = strstr(message, "\"type\":");
        if (type_ptr) {
            type_ptr += 7; // Skip "type":
            while (*type_ptr == ' ' || *type_ptr == ':') type_ptr++; // Skip whitespace and :
            config_type = (uint16_t)atoi(type_ptr);
            
            char type_msg[50];
            snprintf(type_msg, sizeof(type_msg), "TYPE=%d", config_type);
            SUBGHZ_DebugPrint(type_msg);
            
            printf("*** Type parsed: %d ***\n", config_type);
        }

        char* version_ptr = strstr(message, "\"version\":");
        char* period_ptr = strstr(message, "\"period\":");
        char* node_numbers_ptr = strstr(message, "\"Node_Numbers\":");
        printf("*** version_ptr found: %s ***\n", version_ptr ? "YES" : "NO");
        printf("*** period_ptr found: %s ***\n", period_ptr ? "YES" : "NO");
        printf("*** node_numbers_ptr found: %s ***\n", node_numbers_ptr ? "YES" : "NO");

        // Handle type 163: Node Numbers update
        if (config_type == 163 && node_numbers_ptr) {
            node_numbers_ptr += 15; // Skip "Node_Numbers":
            while (*node_numbers_ptr == ' ' || *node_numbers_ptr == ':') node_numbers_ptr++; // Skip whitespace and :
            uint8_t new_max_nodes = (uint8_t)atoi(node_numbers_ptr);

            if (new_max_nodes > 0 && new_max_nodes <= 20) { // Reasonable limit
                printf("*** Updating MAX_SNODES: %d -> %d ***\n", MAX_SNODES, new_max_nodes);
                MAX_SNODES = new_max_nodes;

                // Reset config tracker for new node count
                memset(&configTracker, 0, sizeof(configTracker));

                printf("*** MAX_SNODES successfully updated to %d ***\n", MAX_SNODES);
            } else {
                printf("*** ERROR: Invalid Node_Numbers value: %d (must be 1-20) ***\n", new_max_nodes);
            }
            return;
        }

        // Handle type 150: OTA Update Request
        if (config_type == 150 && version_ptr) {
            SUBGHZ_DebugPrint("OTA_REQUEST_DETECTED");
            
            // Parse version
            version_ptr += 10; // Skip "version":
            while (*version_ptr == ' ' || *version_ptr == ':') version_ptr++;
            config_version = (uint8_t)atoi(version_ptr);
            
            char ota_msg[60];
            snprintf(ota_msg, sizeof(ota_msg), "OTA_VER=%d", config_version);
            SUBGHZ_DebugPrint(ota_msg);
            
            // Start OTA process
            if (OTA_Start(config_version)) {
                SUBGHZ_DebugPrint("OTA_STARTED_SUCCESS");
            } else {
                SUBGHZ_DebugPrint("OTA_START_FAILED");
            }
            return;
        }

        if (version_ptr && period_ptr && (config_type == 161 || config_type == 162)) {
            // Parse version number safely
            version_ptr += 10; // Skip "version":
            while (*version_ptr == ' ' || *version_ptr == ':') version_ptr++; // Skip whitespace and :
            config_version = (uint8_t)atoi(version_ptr);
            printf("*** Version parsed: %d from string: [%.20s] ***\n", config_version, version_ptr);

            // Parse period number safely
            period_ptr += 9; // Skip "period":
            while (*period_ptr == ' ' || *period_ptr == ':') period_ptr++; // Skip whitespace and :+
            printf("*** Period pointer at: [%.20s] ***\n", period_ptr);

            // Extract number only (stop at non-digit characters)
            char period_str[16] = {0};
            int i = 0;
            while (period_ptr[i] >= '0' && period_ptr[i] <= '9' && i < 15) {
                period_str[i] = period_ptr[i];
                i++;
            }
            period_str[i] = '\0';
            config_period = (uint32_t)atoi(period_str);
            printf("*** Period extracted: [%s] → parsed as: %lu ***\n", period_str, config_period);

            printf("*** JSON Config parsed: type=%d, version=%d, period=%lu ms ***\n",
                   config_type, config_version, config_period);
        }
    }
    // Try binary format: [TYPE=0xA1/0xA2][VERSION][PERIOD_MS(4B)][CRC8(1B)]
    else if ((message[0] == 0xA1 || message[0] == 0xA2) && strlen(message) >= 7) {
        if (message[0] == 0xA1) {
            config_type = 161; // Node config
        } else if (message[0] == 0xA2) {
            config_type = 162; // Gateway batch config
        }

        config_version = message[1];
        config_period = (uint32_t)(message[2] | (message[3] << 8) | (message[4] << 16) | (message[5] << 24));

        printf("*** Binary Config parsed: type=%d, version=%d, period=%lu ms ***\n", config_type, config_version, config_period);
    }

    // Update configs based on type
    if (config_version > 0 && config_period > 0) {
        if (config_type == 161) {
            // Node configuration update
            if (config_version != gatewayConfig.nodes_version) {
                printf("*** Updating NODE config: version=%d -> %d, period=%lu -> %lu ***\n",
                       gatewayConfig.nodes_version, config_version, gatewayConfig.period_ms, config_period);
                UpdateGatewayConfig(config_version, config_period);
                last_checked_version = config_version;
            } else {
                printf("*** NODE Config NOT updated: same version=%d ***\n", config_version);
            }
        } else if (config_type == 162) {
            // Gateway batch period update
            if (config_version != gatewayConfig.gateway_version) {
                printf("*** Updating GATEWAY config: version=%d -> %d, batch_period=%lu -> %lu ms ***\n",
                       gatewayConfig.gateway_version, config_version, BATCH_PERIOD_MS, config_period);
                UpdateGatewayBatchPeriod(config_version, config_period);
            } else {
                printf("*** GATEWAY Config NOT updated: same version=%d ***\n", config_version);
            }
        }
    } else if (config_type != 163) { // Type 163 is handled above and doesn't need version/period
        printf("*** Config NOT updated: type=%d, version=%d, period=%lu (invalid values) ***\n",
               config_type, config_version, config_period);
    }
}

/**
 * @brief Update gateway config if new version received
 */
static void UpdateGatewayConfig(uint8_t version, uint32_t period_ms) {   //timerlerin hepsi duruk burda
    printf("*** UpdateGatewayConfig: Old nodes_version=%d, period=%lu ms ***\n",
        gatewayConfig.nodes_version, gatewayConfig.period_ms);

    gatewayConfig.nodes_version = version;  // Update node version
    gatewayConfig.period_ms = period_ms;
    gatewayConfig.updated = 1;  // Enable broadcast to nodes

    printf("*** UpdateGatewayConfig: New nodes_version=%d, period=%lu ms (broadcast ENABLED) ***\n",
        gatewayConfig.nodes_version, gatewayConfig.period_ms);

    // Initialize event-driven config tracking - expected_version gets nodes_version
    configTracker.expected_version = gatewayConfig.nodes_version;

    // Reset all node statuses to 0x00 (not updated)
    for (uint8_t i = 0; i < MAX_SNODES; i++) {
        configTracker.node_config_status[i] = 0x00;
    }

    printf("*** Config tracking activated for nodes_version %d - all nodes marked as 0x00 ***\n", gatewayConfig.nodes_version);
}

/**
 * @brief Update gateway batch period if type 162 message received
 */
static void UpdateGatewayBatchPeriod(uint8_t version, uint32_t batch_period_ms) {
    printf("*** UpdateGatewayBatchPeriod: Old gateway_version=%d, batch_period=%lu ms ***\n",
           gatewayConfig.gateway_version, BATCH_PERIOD_MS);

    // Update gateway version and batch period
    gatewayConfig.gateway_version = version;  // Update gateway version
    BATCH_PERIOD_MS = batch_period_ms;
    gatewayConfig.batch_period_ms = batch_period_ms;
    UTIL_TIMER_SetPeriod(&phaseTimer, BATCH_PERIOD_MS);

    printf("*** Gateway config updated: gateway_version=%d, batch_period=%lu ms ***\n",
           gatewayConfig.gateway_version, BATCH_PERIOD_MS);
}


/**
 * @brief Send collected data to broker
 */

static void SendDataToBroker(void) {
    extern char mqttPacketBuffer[];
    extern MQTT_Config mqttConfig;

    printf("*** SendDataToBroker: Sending %d packets to broker ***\n", packetCount);

    if (packetCount == 0) {
        // ---- NO PACKETS: send empty batch ----
        int len = snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{\n"
            "  \"type\": \"data_batch\",\n"
            "  \"gw\": \"GW-001\",\n"
            "  \"batch\": %lu,\n"
            "  \"timestamp\": %lu,\n"
            "  \"nodes_version\": %d,\n"
            "  \"gateway_version\": %d,\n"
            "  \"node_period_ms\": %lu,\n"
            "  \"batch_period_ms\": %lu,\n"
            "  \"packets\": []\n"
            "}",
            ++batch_counter,
            HAL_GetTick(),
            gatewayConfig.nodes_version,
            gatewayConfig.gateway_version,
            gatewayConfig.period_ms,
            gatewayConfig.batch_period_ms);

        memset(mqttPacketBuffer, 0, MQTT_DATA_PACKET_BUFF_SIZE);
        Wifi_MqttPubRaw2(mqttPacketBuffer, mqttConfig.pubtopic, len, jsonBuffer, QOS_0, RTN_0, POLLING_MODE);

        printf("*** Empty batch sent to broker ***\n");
        return;
    }

    // ---- THERE ARE PACKETS ----
    int packets_sent = 0;
    int batch_index = 0;

    while (packets_sent < packetCount) {
        int len = snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{\n"
            "  \"type\": \"data_batch\",\n"
            "  \"gw\": \"GW-001\",\n"
            "  \"batch\": %lu,\n"
            "  \"timestamp\": %lu,\n"
            "  \"nodes_version\": %d,\n"
            "  \"gateway_version\": %d,\n"
            "  \"node_period_ms\": %lu,\n"
            "  \"batch_period_ms\": %lu,\n"
            "  \"batch_index\": %d,\n"
            "  \"packets\": [\n",
            ++batch_counter,
            HAL_GetTick(),
            gatewayConfig.nodes_version,
            gatewayConfig.gateway_version,
            gatewayConfig.period_ms,
            gatewayConfig.batch_period_ms,
            batch_index++);

        int first_in_batch = 1;

        for (int i = packets_sent; i < packetCount; i++) {
            // Bu paketin JSON uzunluğunu TAHMİN ET
            // Packet format from nodes: TYPE(1) + NODE_ID(1) + SensorData(14) + CONFIG_VER(1) + CRC(1) = 17 bytes
            int estimated_len = snprintf(NULL, 0,
                "%s    { \"packet\": %d, \"id\": \"S-%02d\", \"td\": %d, \"ta\": %d, \"h\": %d, "
                "\"ax\": %d, \"ay\": %d, \"az\": %d, \"battery_voltage\": %d, \"config_ver\": %d, \"timestamp\": %lu, \"rssi\": %d, \"snr\": %d }",
                first_in_batch ? "" : ",\n",
                i + 1,
                packetBuffer[i].node_id,
                packetBuffer[i].td,
                packetBuffer[i].ta,
                packetBuffer[i].h,
                packetBuffer[i].ax,
                packetBuffer[i].ay,
                packetBuffer[i].az,
                packetBuffer[i].battery_voltage,
                packetBuffer[i].config_version,
                packetBuffer[i].timestamp,
                packetBuffer[i].rssi,
                packetBuffer[i].snr);

            // Eğer bu paketi eklersek buffer taşacaksa → batch’i kapat ve gönder
            if (len + estimated_len + 10 >= sizeof(jsonBuffer)) { // +10 kapanış için pay
                len += snprintf(jsonBuffer + len, sizeof(jsonBuffer) - len, "\n  ]\n}");

                memset(mqttPacketBuffer, 0, MQTT_DATA_PACKET_BUFF_SIZE);
                Wifi_MqttPubRaw2(mqttPacketBuffer, mqttConfig.pubtopic, len, jsonBuffer, QOS_0, RTN_0, POLLING_MODE);

                printf("*** Batch %d sent: %d packets (%d/%d total) ***\n",
                       batch_index - 1, i - packets_sent, i, packetCount);

                // Sonraki batch’e buradan devam et
                packets_sent = i;
                goto start_new_batch;
            }

            // Paket sığıyor → ekle
            len += snprintf(jsonBuffer + len, sizeof(jsonBuffer) - len,
                "%s    { \"packet\": %d, \"id\": \"S-%02d\", \"td\": %d, \"ta\": %d, \"h\": %d, "
                "\"ax\": %d, \"ay\": %d, \"az\": %d, \"battery_voltage\": %d, \"config_ver\": %d, \"timestamp\": %lu, \"rssi\": %d, \"snr\": %d }",
                first_in_batch ? "" : ",\n",
                i + 1,
                packetBuffer[i].node_id,
                packetBuffer[i].td,
                packetBuffer[i].ta,
                packetBuffer[i].h,
                packetBuffer[i].ax,
                packetBuffer[i].ay,
                packetBuffer[i].az,
                packetBuffer[i].battery_voltage,
                packetBuffer[i].config_version,
                packetBuffer[i].timestamp,
                packetBuffer[i].rssi,
                packetBuffer[i].snr);

            first_in_batch = 0;
        }

        // Batch kapat
        len += snprintf(jsonBuffer + len, sizeof(jsonBuffer) - len, "\n  ]\n}");

        memset(mqttPacketBuffer, 0, MQTT_DATA_PACKET_BUFF_SIZE);
        Wifi_MqttPubRaw2(mqttPacketBuffer, mqttConfig.pubtopic, len, jsonBuffer, QOS_0, RTN_0, POLLING_MODE);

        printf("*** Batch %d sent: %d packets (%d/%d total) ***\n",
               batch_index - 1, packetCount - packets_sent, packetCount, packetCount);

        break; // tüm paketler bitti
start_new_batch:;
    }
}


/**
 * @brief Send config packet to nodes via LoRa
 * Format: [TYPE=0xA1][VERSION][PERIOD_MS(4B)][CRC8(1B)]
 */
static void SendConfigToNodes(void) {
    uint8_t configPacket[7];

    // Build config packet
    configPacket[0] = CONFIG_MSG_TYPE;  // TYPE
    configPacket[1] = gatewayConfig.nodes_version;  // NODE VERSION (not gateway version)
    configPacket[2] = (uint8_t)(gatewayConfig.period_ms & 0xFF);        // PERIOD_MS byte 0
    configPacket[3] = (uint8_t)((gatewayConfig.period_ms >> 8) & 0xFF);  // PERIOD_MS byte 1
    configPacket[4] = (uint8_t)((gatewayConfig.period_ms >> 16) & 0xFF); // PERIOD_MS byte 2
    configPacket[5] = (uint8_t)((gatewayConfig.period_ms >> 24) & 0xFF); // PERIOD_MS byte 3
    configPacket[6] = CalculateCRC8(configPacket, 6);  // CRC8

    printf("*** Sending config to nodes: TYPE=0x%02X, NODES_VER=%d, PERIOD=%lu ms, CRC=0x%02X ***\n",
           configPacket[0], configPacket[1], gatewayConfig.period_ms, configPacket[6]);

    // Send via LoRa
    memcpy(BufferTx, configPacket, 7);
    Radio.Send(BufferTx, 7);

    printf("*** Config packet sent via LoRa ***\n");
}

/**
 * @brief Calculate CRC8 for config packet (CRC-8/MAXIM)
 */
static uint8_t CalculateCRC8(uint8_t *data, uint8_t length) {
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x31; // CRC-8/MAXIM polynomial

    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Initialize config tracking system
 */
static void InitializeNodeConfigTracker(void) {
    printf("*** InitConfigTracker: Event-driven config tracking system ***\n");

    // Initialize all node statuses to 0x01 (assume they have default config)
    for (uint8_t i = 0; i < MAX_SNODES; i++) {
        configTracker.node_config_status[i] = 0x00;  // Start with default version
    }

    configTracker.expected_version = gatewayConfig.nodes_version;  // Use nodes_version for expected
    configTracker.config_update_active = 0;  // No active config update

    printf("*** Config tracker initialized: expected nodes_version %d, all nodes 0x00 ***\n",
           configTracker.expected_version);
}

/**
 * @brief Process node config version from data packets (event-driven)
 */
static void ProcessNodeConfigVersion(uint8_t node_id, uint8_t received_version) {
    // Only process if config tracking is active
    if (!configTracker.config_update_active) {
        return; // No config tracking active
    }

   /* if (node_id < 1 || node_id >= MAX_SNODES) {
        printf("*** ProcessNodeConfigVersion: Invalid node_id %d ***\n", node_id);
        return;
    }*/

    uint8_t node_index = node_id - 1; // Convert to 0-based index

    printf("*** ProcessNodeConfigVersion: S-%02d sent version %d (expected: %d) ***\n",
           node_id, received_version, configTracker.expected_version);

    if (received_version == configTracker.expected_version) {
        // Node has the expected version
        if (configTracker.node_config_status[node_index] != 0x01) {
            configTracker.node_config_status[node_index] = 0x01;
            printf("*** SUCCESS: S-%02d UPDATED to version %d (0x00→0x01) ***\n",
                   node_id, received_version);
        }
    } else {
        // Node has old version
        if (configTracker.node_config_status[node_index] != 0x00) {
            configTracker.node_config_status[node_index] = 0x00;
            printf("*** WARNING: S-%02d has OLD version %d (0x01→0x00) ***\n",
                   node_id, received_version);
        }
    }
}

/**
 * @brief Check if all nodes have updated to new config version
 */
static uint8_t CheckAllNodesUpdated(void) {
    if (!configTracker.config_update_active) {
        return 0; // No tracking active, consider all updated
    }

    printf("\n*** CheckAllNodesUpdated: Checking node config versions ***\n");

    uint8_t all_updated = 1;
    for (uint8_t i = 0; i < MAX_SNODES; i++) {
        printf("Node S-%02d: Status=0x%02X (%s)\n",
               i + 1,
               configTracker.node_config_status[i],
               configTracker.node_config_status[i] == 0x01 ? "UPDATED" : "OUTDATED");

        if (configTracker.node_config_status[i] == 0x00) {
            all_updated = 0; // Found at least one outdated node
        }
    }

    if (all_updated) {
        printf("*** SUCCESS: All nodes have version %d - config update COMPLETE! ***\n",
               configTracker.expected_version);
    } else {
        printf("*** INCOMPLETE: Some nodes still have old config - continue broadcast ***\n");
    }

    return all_updated;
}


/**
 * @brief Reset config tracker after config broadcast is complete
 */
static void ResetConfigTracker(void) {
    printf("*** ResetConfigTracker: Deactivating config tracking ***\n");
    currentPhase = PHASE_LISTENING;
    configTracker.config_update_active = 0;
    gatewayConfig.updated = 0;
    gatewayConfig.broadcast_active = 0;

    // Keep node statuses as they are (0x01 for updated nodes)
    printf("*** Config tracking deactivated - returning to normal operation ***\n");
}

/* USER CODE END PrFD */
