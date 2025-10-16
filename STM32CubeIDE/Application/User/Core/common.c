#include "common.h"
#include "myESP32AT.h"

#define MQTT_DATA_BUFF_SIZE                     1
#define MQTT_DATA_PACKET_BUFF_SIZE              400
EMPA_SectionTypeDef EMPA_Section = EMPA_SECTION_SENSOR;

volatile FlagStatus  mqtt_timer_en = RESET;
volatile  uint8_t  mqtt_timer = 0;
extern char mqttPacketBuffer[MQTT_DATA_PACKET_BUFF_SIZE];
char mqttDataBuffer[MQTT_DATA_BUFF_SIZE];

int cnt_callback = 0;
int cnt_uart_callback = 0;



/*TIMER MODULE*/
//void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
//{
//	  if (htim->Instance == TIM16)							// Each 1 second
//	  {
//		if(mqtt_timer_en == SET)
//		{
//		  mqtt_timer++;
//		}
//		else
//		{
//		  mqtt_timer = 0;
//		}
//
//	  }
//}

// === MQTT Line Assembler (IRQ veya Callback'tan çağrılabilir) ===
#include <string.h>
#include <stdio.h>
extern volatile FlagStatus flag_mqtt_rx_done;
extern UART_HandleTypeDef hlpuart1; // ESP32 UART - Debug için
static char mqttLineBuf[256];
static uint16_t mqttLineLen = 0;

// Debug: Gelen tüm byte'ları topla
static char debugBuffer[1024];
static uint16_t debugBufferLen = 0;

// LPUART1 Debug Helper
static void UART_DebugPrint(const char* msg) {
    if (msg) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"[UART]", 6, 100);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), 1000);
        HAL_UART_Transmit(&hlpuart1, (uint8_t*)"\r\n", 2, 100);
    }
}

// Manuel debug çağrısı için fonksiyon (konsol'dan çağırabilirsin)
void MQTT_ClearDebugBuffer(void) {
    debugBufferLen = 0;
}

void MQTT_PrintDebugBuffer(void)
{
    // printf çalışmıyor, bu fonksiyonu boş bırakıyoruz
    // Eğer debug gerekirse LPUART1 kullan
}

void MQTT_LineAssembler_Byte(uint8_t ch)
{
    // Debug buffer'a ekle (overflow kontrolü ile)
    if(debugBufferLen < sizeof(debugBuffer) - 1) {
        debugBuffer[debugBufferLen++] = ch;
    }

    if(mqttLineLen < sizeof(mqttLineBuf)-1) {
        if(ch != '\r' && ch != '\n') {
            mqttLineBuf[mqttLineLen++] = (char)ch;
        } else {
            if(mqttLineLen > 0) {
                mqttLineBuf[mqttLineLen] = 0;

                // MQTTSUBRECV kontrolü - başında + olmasa da kabul et, JSON varsa işle
                if(strstr(mqttLineBuf, "MQTTSUBRECV") && strchr(mqttLineBuf,'{') && strrchr(mqttLineBuf,'}')) {
                    UART_DebugPrint("MQTTSUBRECV_DETECT");
                    memset(mqttPacketBuffer,0,MQTT_DATA_PACKET_BUFF_SIZE);
                    strncpy(mqttPacketBuffer, mqttLineBuf, MQTT_DATA_PACKET_BUFF_SIZE-1);
                    flag_mqtt_rx_done = SET;

                    // JSON'u çıkar ve direkt parse et
                    char *json_start = strchr(mqttLineBuf, '{');
                    char *json_end = strrchr(mqttLineBuf, '}');
                    if(json_start && json_end && json_end > json_start) {
                        size_t len = (size_t)(json_end - json_start + 1);
                        if(len < 200) {
                            char json_clean[200];
                            memset(json_clean, 0, sizeof(json_clean));
                            memcpy(json_clean, json_start, len);
                            json_clean[len] = 0;
                            UART_DebugPrint("JSON_EXTRACT");

                            // Escape karakterleri temizle \" -> "
                            char json_final[200];
                            int j = 0;
                            for(int i = 0; i < len && j < sizeof(json_final)-1; i++) {
                                if(json_clean[i] == '\\' && json_clean[i+1] == '"') {
                                    json_final[j++] = '"';
                                    i++; // skip next char
                                } else {
                                    json_final[j++] = json_clean[i];
                                }
                            }
                            json_final[j] = 0;
                            UART_DebugPrint(json_final); // JSON içeriğini bas

                            // Config parse çağır
                            extern void ParseConfigMessage(const char* message);
                            UART_DebugPrint("CALL_PARSE_FROM_UART");
                            ParseConfigMessage(json_final);
                        }
                    }

                    // Debug buffer'ı yazdır ve temizle
                    MQTT_PrintDebugBuffer();
                    debugBufferLen = 0;
                }
            }
            mqttLineLen = 0;
        }
    } else {
        mqttLineLen = 0; // overflow reset
    }
}// Callback sadece yeniden silahlamak için minimal bırakıldı (şu an IRQ doğrudan RDR okuyacak)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  cnt_callback++;
  if(huart->Instance == LPUART1) {
    // Eğer HAL kendi bufferına aldıysa (kullanılmıyor ama güvenlik)
    if(mqttDataBuffer[0] != 0) {
      MQTT_LineAssembler_Byte((uint8_t)mqttDataBuffer[0]);
    }
    HAL_UART_Receive_IT(huart, (uint8_t*)mqttDataBuffer, 1);
  }
}
