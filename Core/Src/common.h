
#ifndef INC_COMMON_H_
#define INC_COMMON_H_

#include "main.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

//Defines
//#define EMPA_DataCollector
#define EMPA_MqttAws
#define EMPA_Sensor


extern UART_HandleTypeDef hlpuart1;
extern TIM_HandleTypeDef htim1;
#define TMR_ONESECOND                           &htim1

#define UART_WIFI                               hlpuart1
#define UART_INST								LPUART1


#define WIFI_SSID                               "EMPA_Arge"
#define WIFI_PASSWORD                           "Emp@Arg2024!"


#define MQTT_CLIENT_ID                          ""
#define MQTT_USERNAME                           ""
#define MQTT_PASSWORD                           ""
#define MQTT_KEEP_ALIVE_TIME                    60                      // in seconds
#define MQTT_BROKER_ADDRESS                     "bed9ff2f1d494c18b5ef64b17f482ce5.s1.eu.hivemq.cloud"



typedef enum
{
	FALSE = 0,
	TRUE  = 1
} bool_t;

typedef enum
{
  EMPA_SECTION_SENSOR          = 0x00U,
  EMPA_SECTION_NANOEDGEAI,
  EMPA_SECTION_MQTT

} EMPA_SectionTypeDef;





#endif /* INC_COMMON_H_ */
