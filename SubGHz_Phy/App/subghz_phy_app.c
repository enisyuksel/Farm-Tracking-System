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
#define RX_TIMEOUT_VALUE              3000
#define TX_TIMEOUT_VALUE              3000

#define MAX_APP_BUFFER_SIZE          255
/* wait for remote to be in Rx, before sending a Tx frame*/
#define RX_TIME_MARGIN                200
/* Afc bandwidth in Hz */
#define FSK_AFC_BANDWIDTH             83333
/* LED blink Period*/
#define LED_PERIOD_MS                 200

//#define TRANSMITTER
#define RECEIVER

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
/* device state. Master: true, Slave: false*/
bool isMaster = true;
/* random delay to make sure 2 devices will sync*/
/* the closest the random delays are, the longer it will
 take for the devices to sync when started simultaneously*/
static int32_t random_delay;

// Batch data variables
#define MAX_SNODES 6
#define BATCH_PERIOD_MS 10000 // 90 seconds

typedef struct {
    uint8_t node_id;
    int16_t td;
    int16_t ta;
    uint8_t h;
    int16_t ax;
    int16_t ay;
    int16_t az;
    uint8_t crc;
    uint32_t timestamp;
} SensorData_t;

static SensorData_t dataBuffer[MAX_SNODES];
static uint8_t dataCount = 0;
static uint8_t missingNodes[MAX_SNODES];
static uint8_t missingCount = 0;
static UTIL_TIMER_Object_t batchTimer;

uint8_t deger = 1;



typedef struct {
    uint8_t node_id;      // S-01 = 1, S-02 = 2, etc.
    int16_t td;           // Digital temp (0.01°C)
    int16_t ta;           // Analog temp (0.01°C)
    uint8_t h;            // Humidity (%)
    int16_t ax, ay, az;   // Accelerometer (mg)
    bool valid;           // Data received in current batch window
    uint32_t last_seen;   // Timestamp of last data
} SensorNode_t;

static SensorNode_t snodes[MAX_SNODES];
static UTIL_TIMER_Object_t batchTimer;
static uint32_t batch_counter = 0;
int32_t temperature = 0;
int32_t humidity = 0;

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
 * @brief Gateway: Publish batch data to MQTT
 */
static void PublishBatchToMQTT(void *context);

/**
 * @brief Calculate CRC-8/MAXIM for data validation
 */
static uint8_t calculate_crc8(uint8_t *data, uint8_t len);

/**
 * @brief Check for missing nodes and update missing array
 */
static void CheckMissingNodes(void);

/**
 * @brief Send CONFIG message to sensors via LoRa
 */
static void SendConfigMessage(uint32_t period);

/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
void SubghzApp_Init(void)
{
  /* USER CODE BEGIN SubghzApp_Init_1 */

	APP_LOG(TS_OFF, VLEVEL_M, "\n\rPING PONG\n\r");
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
	/*calculate random delay for synchronization*/
	random_delay = (Radio.Random()) >> 22; /*10bits random e.g. from 0 to 1023 ms*/

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

	APP_LOG(TS_ON, VLEVEL_L, "rand=%d\n\r", random_delay);
	APP_LOG(TS_ON, VLEVEL_L, "SubGHz Radio initialized - Waiting for MQTT connection\n\r");
	
	// Don't start LoRa operations here - wait for MQTT connection
  /* USER CODE END SubghzApp_Init_2 */
}

/* USER CODE BEGIN EF */
char receiver_buffer[250] = {0};

// Start LoRa Gateway operations after MQTT is connected
void StartLoRaGateway(void) {
	/*starts reception - Gateway mode*/
	APP_LOG(TS_ON, VLEVEL_L, "Gateway Mode - Waiting for sensor data\n\r");
	LoRaState = RX;  // Start in receiving mode for Gateway
	
	/* Initialize sensor nodes array */
	for (int i = 0; i < MAX_SNODES; i++) {
		snodes[i].node_id = i + 1;  // S-01, S-02, etc.
		snodes[i].valid = false;
	}
	
	/* Create and start batch timer */
	UTIL_TIMER_Create(&batchTimer, BATCH_PERIOD_MS, UTIL_TIMER_PERIODIC, PublishBatchToMQTT, NULL);
	UTIL_TIMER_Start(&batchTimer);
	APP_LOG(TS_ON, VLEVEL_L, "Batch timer started - %d ms period\n\r", BATCH_PERIOD_MS);
	
	/*register task to to be run in while(1) after Radio IT*/
	UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_SubGHz_Phy_App_Process), UTIL_SEQ_RFU,
			PingPong_Process);
	
	// Start listening for incoming packets immediately
	APP_LOG(TS_ON, VLEVEL_L, "Starting Gateway RX mode...\n\r");
	Radio.Rx(RX_TIMEOUT_VALUE);
}
/* USER CODE END EF */

/* Private functions ---------------------------------------------------------*/
static void OnTxDone(void)
{
  /* USER CODE BEGIN OnTxDone */
	APP_LOG(TS_ON, VLEVEL_L, "OnTxDone\n\r");
	/* Update the LoRaState of the FSM*/
#ifdef TRANSMITTER

  LoRaState = RX;
#else
  LoRaState = TX;
#endif

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

	// Gateway mode: Parse DATA payload and store in batch
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
		
		// Store in batch array
		if (node_id >= 1 && node_id <= MAX_SNODES) {
			snodes[node_id-1].node_id = node_id;
			snodes[node_id-1].td = td;
			snodes[node_id-1].ta = ta;
			snodes[node_id-1].h = h;
			snodes[node_id-1].ax = ax;
			snodes[node_id-1].ay = ay;
			snodes[node_id-1].az = az;
			snodes[node_id-1].valid = true;
			snodes[node_id-1].last_seen = HAL_GetTick();
			
			APP_LOG(TS_ON, VLEVEL_L, "Stored in batch for S-%02d\n\r", node_id);
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

static void OnledEvent(void *context) {
	HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); /* LED_GREEN */
//	HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin); /* LED_RED */
	UTIL_TIMER_Start(&timerLed);
}

/**
 * @brief Gateway: Publish batch data to MQTT in JSON format
 */
static void PublishBatchToMQTT(void *context) {
    extern char mqttPacketBuffer[];
    extern MQTT_Config mqttConfig;
    
    // Build JSON batch message according to scenario
    char json[800];
    int len = snprintf(json, sizeof(json), 
        "{\n"
        "  \"type\": \"data_batch\",\n"
        "  \"gw\": \"GW-001\",\n"
        "  \"batch\": %lu,\n"
        "  \"timestamp\": %lu,\n"
        "  \"nodes\": [\n", 
        batch_counter++, HAL_GetTick());
    
    int node_count = 0;
    
    // Add valid nodes that sent data in this batch window
    for (int i = 0; i < MAX_SNODES; i++) {
        if (snodes[i].valid) {
            if (node_count > 0) {
                len += snprintf(json + len, sizeof(json) - len, ",\n");
            }
            len += snprintf(json + len, sizeof(json) - len,
                "    { \"id\": \"S-%02d\", \"td\": %d, \"ta\": %d, \"h\": %d, \"ax\": %d, \"ay\": %d, \"az\": %d }",
                snodes[i].node_id,
                snodes[i].td,  // Keep as 0.01°C units (integer)
                snodes[i].ta,  // Keep as 0.01°C units (integer)
                snodes[i].h,
                snodes[i].ax, snodes[i].ay, snodes[i].az);
            node_count++;
        }
    }
    
    len += snprintf(json + len, sizeof(json) - len, "\n  ],\n");
    
    // Add missing nodes (expected but not received)
    len += snprintf(json + len, sizeof(json) - len, "  \"missing\": [");
    int missing_count = 0;
    for (int i = 1; i <= MAX_SNODES; i++) {  // Node IDs are 1-6
        if (!snodes[i-1].valid) {
            if (missing_count > 0) {
                len += snprintf(json + len, sizeof(json) - len, ", ");
            }
            len += snprintf(json + len, sizeof(json) - len, "\"S-%02d\"", i);
            missing_count++;
        }
    }
    len += snprintf(json + len, sizeof(json) - len, "]\n}");
    
    // Send to MQTT broker via ESP32
    printf("Publishing batch %lu: %d nodes, %d missing\n", batch_counter-1, node_count, missing_count);
    
    // Clear MQTT buffer and publish
    memset(mqttPacketBuffer, 0, MQTT_DATA_PACKET_BUFF_SIZE);
    Wifi_MqttPubRaw2(mqttPacketBuffer, mqttConfig.pubtopic, strlen(json), json, QOS_0, RTN_0, POLLING_MODE);
    
    // Reset all nodes for next batch period
    for (int i = 0; i < MAX_SNODES; i++) {
        snodes[i].valid = false;
    }
    
    // Restart batch timer for next 30-second window
    UTIL_TIMER_Start(&batchTimer);
}

/**
 * @brief Calculate CRC-8/MAXIM for data validation
 */
static uint8_t calculate_crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        uint8_t inbyte = *data++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

/**
 * @brief Check for missing nodes and update missing array
 */
static void CheckMissingNodes(void) {
    missingCount = 0;
    for (int i = 0; i < MAX_SNODES; i++) {
        if (!snodes[i].valid) {
            missingNodes[missingCount++] = i + 1; // Node IDs are 1-based
        }
    }
}

/**
 * @brief Send CONFIG message to sensors via LoRa
 */
static void SendConfigMessage(uint32_t period) {
    uint8_t configFrame[8];
    configFrame[0] = CONFIG_MSG;           // Message type
    configFrame[1] = 0xFF;                 // Broadcast to all nodes
    configFrame[2] = (period >> 24) & 0xFF; // Period in seconds (big-endian)
    configFrame[3] = (period >> 16) & 0xFF;
    configFrame[4] = (period >> 8) & 0xFF;
    configFrame[5] = period & 0xFF;
    configFrame[6] = 0; // Reserved
    configFrame[7] = calculate_crc8(configFrame, 7); // CRC
    
    APP_LOG(TS_ON, VLEVEL_L, "Sending CONFIG message: period=%lu\n\r", period);
    Radio.Send(configFrame, 8);
    LoRaState = TX;
}

/* USER CODE END PrFD */
