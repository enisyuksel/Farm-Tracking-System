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
#include "../../Core/Src/myESP32AT.h"
#include "../../Core/Src/EMPA_MqttAws.h"
#include "../../Core/Src/common.h"
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
#define MAX_SNODES 6
#define BATCH_PERIOD_MS 180000 // 60 seconds (1 minute)
#define MAX_PACKETS_IN_BATCH 200 // Maximum packets to store in 60 seconds
#define MAX_PACKETS_PER_BATCH   50  // İstersen dinamik hesaplatabilirsin

typedef struct {
    uint8_t node_id;      		// S-01 = 1, S-02 = 2, etc.
    int16_t td;           		// Digital temp (0.01°C)
    int16_t ta;           		// Analog temp (0.01°C)
    uint8_t h;            		// Humidity (%)
    int16_t ax, ay, az;   		// Accelerometer (mg)
    uint32_t timestamp;   		// When packet was received
    int16_t rssi;         		// Signal strength
    int8_t snr;           		// Signal to noise ratio
} ReceivedPacket_t;


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
#define JSON_BUFFER_SIZE   7000   // İhtiyacına göre 4K veya 8K seç
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
    uint8_t version;        // Config version
    uint32_t period_ms;     // Node transmission period in milliseconds
    uint8_t updated;        // Flag: 1 if config was updated and needs broadcast
    uint8_t broadcast_active; // Flag: 1 if currently broadcasting config
} GatewayConfig_t;

static GatewayConfig_t gatewayConfig = {
    .version = 1,           // Default version
    .period_ms = 3000,      // Default 3 seconds for nodes
    .updated = 0,
    .broadcast_active = 0
};

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

	/* Create phase timer for main gateway cycle (30s listening phase) */
	UTIL_TIMER_Create(&phaseTimer, BATCH_PERIOD_MS, UTIL_TIMER_ONESHOT, GatewayPhaseHandler, NULL);

	/* Create config broadcast timer (2s retry during broadcast phase) */
	UTIL_TIMER_Create(&configBroadcastTimer, 2000, UTIL_TIMER_ONESHOT, ConfigBroadcastHandler, NULL);

	/* Start with listening phase */
	UTIL_TIMER_Start(&phaseTimer);
	printf("*** PHASE TIMER STARTED - Listening phase (30s) ***\n");
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
	if (size >= 14 && payload[0] == 0xD1) { // DATA payload type + CRC
		// Parse sensor data from LoRa payload according to NEW frame structure
		uint8_t type = payload[0];                               // Frame type (0xD1)
		uint8_t node_id = payload[1];                            // Node ID (S-01=1, S-02=2, etc.)
		int16_t td = (int16_t)(payload[2] | (payload[3] << 8));  // Digital temp (0.01°C)
		int16_t ta = (int16_t)(payload[4] | (payload[5] << 8));  // Analog temp (0.01°C)
		uint8_t h = payload[6];                                  // Humidity (%)
		int16_t ax = (int16_t)(payload[7] | (payload[8] << 8));  // Accel X (mg)
		int16_t ay = (int16_t)(payload[9] | (payload[10] << 8)); // Accel Y (mg)
		int16_t az = (int16_t)(payload[11] | (payload[12] << 8)); // Accel Z (mg)
		uint8_t crc = payload[13];                               // CRC-8/MAXIM

		APP_LOG(TS_ON, VLEVEL_L, "DATA from S-%02d: TD=%.2f°C TA=%.2f°C H=%d%% AX=%dmg AY=%dmg AZ=%dmg\n\r",
				node_id, td/100.0f, ta/100.0f, h, ax, ay, az);

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
			packetBuffer[packetCount].timestamp = HAL_GetTick();
			packetBuffer[packetCount].rssi = rssi;
			packetBuffer[packetCount].snr = LoraSnr_FskCfo;

			packetCount++;

			printf("*** DEBUG: Packet stored! Total packets in buffer: %d ***\n\r", packetCount);
			APP_LOG(TS_ON, VLEVEL_L, "Packet %d stored from S-%02d (Total: %d packets)\n\r",
					packetCount, node_id, packetCount);
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

            // Check if config broadcast needed
            if (gatewayConfig.updated) {  //burada gateway config updated değilde config devam ediyor flagı ayarlasak ?
                UTIL_TIMER_Stop(&phaseTimer);
                UTIL_TIMER_Stop(&configBroadcastTimer);
                printf("*** Config updated - Starting PHASE 4: Config broadcast (15s) ***\n");
                currentPhase = PHASE_CONFIG_BROADCAST;
                gatewayConfig.broadcast_active = 1;

                // Start config broadcast timer (2s retry)
                UTIL_TIMER_Start(&configBroadcastTimer);

                // Set phase timer for 15s broadcast period
                UTIL_TIMER_SetPeriod(&phaseTimer, 15000);
                UTIL_TIMER_Start(&phaseTimer);
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
            gatewayConfig.updated = 0;  // Clear update flag
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
                (strstr(config_message, "161") || strstr(config_message, "0xA1"))) {
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
 * Expected format: {"type":161,"version":3,"period":5000} or binary format
 */
void ParseConfigMessage(const char* message) {
    printf("*** ENTERED ParseConfigMessage FUNCTION ***\n");
    if (!message) {
        printf("*** ParseConfigMessage: NULL message received ***\n");
        return;
    }
    printf("*** ParseConfigMessage: Processing: [%s] (len=%zu) ***\n", message, strlen(message));

    uint8_t config_version = 0;
    uint32_t config_period = 0;

    // Try JSON format first: {"type":161,"version":3,"period":5000}
    if (strstr(message, "\"type\"") && strstr(message, "161")) {
        printf("*** Found JSON type and 161 ***\n");
        char* version_ptr = strstr(message, "\"version\":");
        char* period_ptr = strstr(message, "\"period\":");
        printf("*** version_ptr found: %s ***\n", version_ptr ? "YES" : "NO");
        printf("*** period_ptr found: %s ***\n", period_ptr ? "YES" : "NO");

        if (version_ptr && period_ptr) {
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

            printf("*** JSON Config parsed: version=%d, period=%lu ms (extracted from: %s) ***\n",
                   config_version, config_period, period_str);
        }
    }
    // Try binary format: [TYPE=0xA1][VERSION][PERIOD_MS(4B)][CRC8(1B)]
    else if (message[0] == 0xA1 && strlen(message) >= 7) {
        config_version = message[1];
        config_period = (uint32_t)(message[2] | (message[3] << 8) | (message[4] << 16) | (message[5] << 24));

        printf("*** Binary Config parsed: version=%d, period=%lu ms ***\n", config_version, config_period);
    }

    // Update gateway config if valid and version changed
    if (config_version > 0 && config_period > 0 && config_version != gatewayConfig.version) {
        printf("*** Updating config: version=%d -> %d, period=%lu -> %lu ***\n",
               gatewayConfig.version, config_version, gatewayConfig.period_ms, config_period);
        UpdateGatewayConfig(config_version, config_period);
        last_checked_version = config_version;
    } else if (config_version == gatewayConfig.version) {
        printf("*** Config NOT updated: same version=%d (period would be %lu) ***\n",
               config_version, config_period);
    } else {
        printf("*** Config NOT updated: version=%d, period=%lu (invalid values) ***\n",
               config_version, config_period);
    }
}

/**
 * @brief Update gateway config if new version received
 */
static void UpdateGatewayConfig(uint8_t version, uint32_t period_ms) {   //timerlerin hepsi duruk burda
    printf("*** UpdateGatewayConfig: Old version=%d, period=%lu ms ***\n",
        gatewayConfig.version, gatewayConfig.period_ms);

    gatewayConfig.version = version;
    gatewayConfig.period_ms = period_ms;
    gatewayConfig.updated = 1;  // Enable broadcast to nodes

    printf("*** UpdateGatewayConfig: New version=%d, period=%lu ms (broadcast ENABLED) ***\n",
        gatewayConfig.version, gatewayConfig.period_ms);
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
            "  \"config_version\": %d,\n"
            "  \"node_period_ms\": %lu,\n"
            "  \"packets\": []\n"
            "}",
            ++batch_counter,
            HAL_GetTick(),
            gatewayConfig.version,
            gatewayConfig.period_ms);

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
            "  \"config_version\": %d,\n"
            "  \"node_period_ms\": %lu,\n"
            "  \"batch_index\": %d,\n"
            "  \"packets\": [\n",
            ++batch_counter,
            HAL_GetTick(),
            gatewayConfig.version,
            gatewayConfig.period_ms,
            batch_index++);

        int first_in_batch = 1;

        for (int i = packets_sent; i < packetCount; i++) {
            // Bu paketin JSON uzunluğunu TAHMİN ET
            int estimated_len = snprintf(NULL, 0,
                "%s    { \"packet\": %d, \"id\": \"S-%02d\", \"td\": %d, \"ta\": %d, \"h\": %d, "
                "\"ax\": %d, \"ay\": %d, \"az\": %d, \"timestamp\": %lu, \"rssi\": %d, \"snr\": %d }",
                first_in_batch ? "" : ",\n",
                i + 1,
                packetBuffer[i].node_id,
                packetBuffer[i].td,
                packetBuffer[i].ta,
                packetBuffer[i].h,
                packetBuffer[i].ax,
                packetBuffer[i].ay,
                packetBuffer[i].az,
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
                "\"ax\": %d, \"ay\": %d, \"az\": %d, \"timestamp\": %lu, \"rssi\": %d, \"snr\": %d }",
                first_in_batch ? "" : ",\n",
                i + 1,
                packetBuffer[i].node_id,
                packetBuffer[i].td,
                packetBuffer[i].ta,
                packetBuffer[i].h,
                packetBuffer[i].ax,
                packetBuffer[i].ay,
                packetBuffer[i].az,
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
    configPacket[0] = 0xA1;  // TYPE
    configPacket[1] = gatewayConfig.version;  // VERSION
    configPacket[2] = (uint8_t)(gatewayConfig.period_ms & 0xFF);        // PERIOD_MS byte 0
    configPacket[3] = (uint8_t)((gatewayConfig.period_ms >> 8) & 0xFF);  // PERIOD_MS byte 1
    configPacket[4] = (uint8_t)((gatewayConfig.period_ms >> 16) & 0xFF); // PERIOD_MS byte 2
    configPacket[5] = (uint8_t)((gatewayConfig.period_ms >> 24) & 0xFF); // PERIOD_MS byte 3
    configPacket[6] = CalculateCRC8(configPacket, 6);  // CRC8

    printf("*** Sending config to nodes: TYPE=0x%02X, VER=%d, PERIOD=%lu ms, CRC=0x%02X ***\n",
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

/* USER CODE END PrFD */


/* USER CODE END PrFD */
